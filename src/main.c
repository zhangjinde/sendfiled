/*
  Copyright (c) 2014, Francois Kritzinger
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

  1. Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

  2. Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#define _XOPEN_SOURCE 500       /* For chroot, primarily */

#include <fcntl.h>
#include <grp.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <syslog.h>
#include <unistd.h>

#include <limits.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <fiod_config.h>

#include "fiod.h"

#include "impl/errors.h"
#include "impl/process.h"
#include "impl/server.h"
#include "impl/unix_socket_server.h"

static const long OPEN_FD_TIMEOUT_MS_MAX = 60 * 60 * 1000;

static void print_usage(long fd_timeout_ms);
static bool sync_parent(int status_code);
static long opt_strtol(const char*);
static bool chroot_and_drop_privs(const char* root_dir,
                                  uid_t new_uid,
                                  gid_t new_gid);

int main(const int argc, char** argv)
{
    const char* srvname = NULL;
    const char* root_dir = NULL;
    const char* uname = NULL;
    const char* gname = NULL;
    long maxfiles = 0;
    bool do_sync = false;
    long fd_timeout_ms = 30000;
    bool daemonise = false;

    int opt;
    while ((opt = getopt(argc, argv, "+s:n:t:r:u:g:pd")) != -1) {
        switch (opt) {
        case 'r':
            root_dir = optarg;
            break;
        case 'u':
            uname = optarg;
            break;
        case 'g':
            gname = optarg;
            break;
        case 's':
            srvname = optarg;
            break;

        case 'n': {
            maxfiles = opt_strtol(optarg);

            if (maxfiles == -1) {
                LOGERRNOV("Invalid value '%s' for max files\n", optarg);
                return EXIT_FAILURE;
            }
        } break;

        case 't':
            fd_timeout_ms = opt_strtol(optarg);

            if (fd_timeout_ms == -1 || fd_timeout_ms > OPEN_FD_TIMEOUT_MS_MAX) {
                LOGERRNOV("Invalid value '%s' for open"
                          " file descriptor timeout\n",
                          optarg);
                return EXIT_FAILURE;
            }
            break;

        case 'p':
            do_sync = true;
            break;

        case 'd':
            daemonise = true;
            break;

        case '?':
            return EXIT_FAILURE;

        default:
            print_usage(fd_timeout_ms);
            return EXIT_FAILURE;
        }
    }

    if (!root_dir || !srvname || (maxfiles == 0)) {
        print_usage(fd_timeout_ms);
        return EXIT_FAILURE;
    }

    uid_t new_uid = getuid();
    gid_t new_gid = getgid();

    if (uname) {
        struct passwd* pwd = getpwnam(uname);
        if (!pwd) {
            syslog(LOG_ERR, "Couldn't find user %s\n", uname);
            return EXIT_FAILURE;
        }
        new_uid = pwd->pw_uid;
    }

    if (gname) {
        struct group* grp = getgrnam(gname);
        if (!grp) {
            syslog(LOG_ERR, "Couldn't find group %s\n", gname);
            return EXIT_FAILURE;
        }
        new_gid = grp->gr_gid;
    }

    /* Ignore SIGPIPE */
    sigset_t sigmask;
    if (sigemptyset(&sigmask) == -1 ||
        sigaddset(&sigmask, SIGPIPE) == -1 ||
        sigprocmask(SIG_BLOCK, &sigmask, NULL) == -1) {
        LOGERRNO("Couldn't ignore SIGPIPE");
        return EXIT_FAILURE;;
    }

    const int requestfd = us_serve(srvname, new_uid, new_gid);
    if (requestfd == -1) {
        if (do_sync && !sync_parent(errno))
            LOGERRNO("Failed to write errno to sync fd");
        return EXIT_FAILURE;
    }

    if (do_sync) {
        if (!sync_parent(0)) {
            LOGERRNO("Failed to sync with parent");
            goto fail1;
        }
        close(PROC_SYNCFD);
    }

    if (daemonise && !proc_daemonise(&requestfd, 1))
        goto fail1;

    openlog(FIOD_PROGNAME, LOG_NDELAY | LOG_CONS | LOG_PID, LOG_DAEMON);

    if (!chroot_and_drop_privs(root_dir, new_uid, new_gid)) {
        us_stop_serving(srvname, requestfd);
        goto fail1;
    }

    syslog(LOG_INFO,
           "Starting; name: %s; root_dir: \"%s\";"
           " uid: %d %s; gid: %d %s;"
           " maxfiles: %ld; fd_timeout_ms: %ld\n",
           srvname, root_dir,
           getuid(), uname, getgid(), gname, maxfiles, fd_timeout_ms);

    const bool success = srv_run(requestfd, (int)maxfiles, fd_timeout_ms);

    if (!success) {
        syslog(LOG_EMERG,
               "srv_run() failed [%s]; server shutting down\n",
               strerror(errno));
    } else {
        syslog(LOG_INFO, "Shutting down\n");
    }

    us_stop_serving(srvname, requestfd);

    return (success ? EXIT_SUCCESS : EXIT_FAILURE);

 fail1:
    us_stop_serving(srvname, requestfd);

    return EXIT_FAILURE;
}

static bool chroot_and_drop_privs(const char* root_dir,
                                  const uid_t new_uid,
                                  const gid_t new_gid)
{
    const uid_t euid = geteuid();
    const gid_t gid = getgid();

    if (new_uid == 0) {
        syslog(LOG_ERR, "Refusing to run as root user\n");
        return false;
    }

    if (new_gid == 0) {
        syslog(LOG_ERR, "Refusing to switch to 'root' group\n");
        return false;
    }

    if (strncmp(root_dir, "/", 2) != 0) {
        if (euid != 0) {
            syslog(LOG_ERR, "Executable doesn't appear to be setuid\n");
            return false;
        }

        if (chroot(root_dir) == -1) {
            syslog(LOG_ERR, "Couldn't chroot to %s\n", root_dir);
            return false;
        }

        if (chdir("/") == -1) {
            syslog(LOG_ERR, "Couldn't chdir to '/'\n");
            return false;
        }
    } else {
        syslog(LOG_WARNING,
               "Not chrooting because user-specified root dir is \"/\"\n");
    }

    if (new_gid != gid && setgid(new_gid) == -1) {
        syslog(LOG_ERR, "Couldn't setgid to GID %d: %s\n",
               new_gid, strerror(errno));
        return false;
    }

    if (setuid(new_uid) == -1) {
        syslog(LOG_ERR, "Couldn't setuid to UID %d: %s\n",
               new_uid, strerror(errno));
        return false;
    }

    return true;
}

static long opt_strtol(const char* s)
{
    errno = 0;
    const long l = strtol(s, NULL, 10);

    if (errno != 0 || l == 0 || l == LONG_MIN || l == LONG_MAX) {
        if (errno != 0)
            LOGERRNO("strtol");
        return -1;
    } else {
        return l;
    }
}

static void print_usage(const long fd_timeout_ms)
{
    printf("Usage: "
           FIOD_PROGNAME" OPTION\n"
           "\nOptions:\n"
           "-r <root_dir> (chroot to this directory)\n"
           "-s <server_name> (user-friendly name to identify server instance)\n"
           "-n <maxfiles> (maximum number of concurrent file transfers)\n"
           "[-u <user_name>] (run as different user)\n"
           "[-g <group_name>] (run as different group)\n"
           "[-p (sync with parent process (via a pipe))]\n"
           "[-t <open_fd_timeout_ms> (default: %ld)]\n",
           fd_timeout_ms);
}

static bool sync_parent(const int status)
{
    return (write(PROC_SYNCFD, &status, sizeof(status)) == sizeof(status));
}
