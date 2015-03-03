/*
  Copyright (c) 2015, Francois Kritzinger
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

#define _POSIX_C_SOURCE 200809L

#include <fcntl.h>
#include <grp.h>
#include <pwd.h>
#include <sys/stat.h>
#include <unistd.h>

#include <limits.h>
#include <signal.h>
#include <stdlib.h>

#include <sfd_config.h>

#include "sendfiled.h"

#include "impl/errors.h"
#include "impl/log.h"
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

/* Whether or not to sync with the parent process. This would be the case if
   being spawned by sfd_spawn(), for example. */
static bool do_sync = false;

#define LOG_(msg)                                         \
    {                                                     \
        if (do_sync) {                                    \
            sfd_log(LOG_INFO, "%s: %s\n", __func__, msg); \
        } else {                                          \
            const int tmp__ = errno;                      \
            printf("%s: %s\n", __func__, msg);            \
            errno = tmp__;                                \
        }                                                 \
    }                                                     \

#define LOGERRNO_(msg)                          \
    {                                           \
        if (do_sync) {                          \
            sfd_log(LOG_ERR,                    \
                    "%s [errno: %d %m] %s\n",   \
                    __func__, errno, msg);      \
        } else {                                \
            LOGERRNO(msg);                      \
        }                                       \
    }                                           \

#define LOGERRNOV_(fmt, ...)                        \
    {                                               \
        if (do_sync) {                              \
            sfd_log(LOG_ERR,                        \
                    "%s [errno %d %m] "fmt"\n",     \
                    __func__, errno, __VA_ARGS__);  \
        } else {                                    \
            LOGERRNOV(fmt"\n", __VA_ARGS__);        \
        }                                           \
    }                                               \

extern char** environ;

int main(const int argc, char** argv)
{
    environ = NULL;

    const char* srvname = NULL;
    const char* root_dir = NULL;
    const char* sockdir = SFD_SRV_SOCKDIR;
    const char* uname = NULL;
    const char* gname = NULL;
    long maxfiles = 0;
    long fd_timeout_ms = 30000;
    bool daemonise = false;

    int opt;
    while ((opt = getopt(argc, argv, "+s:S:n:t:r:u:g:pd")) != -1) {
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

        case 'S':
            sockdir = optarg;
            break;

        case 'n':
            maxfiles = opt_strtol(optarg);
            break;

        case 't':
            fd_timeout_ms = opt_strtol(optarg);
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

    if (!root_dir || !srvname || maxfiles == 0) {
        if (!do_sync)
            print_usage(fd_timeout_ms);
        LOG_("Missing command-line argument");
        errno = EINVAL;
        goto fail1;
    }

    if (maxfiles == -1) {
        LOGERRNO_("Invalid value for max files");
        goto fail1;
    }

    if (fd_timeout_ms == -1) {
        LOGERRNO_("Invalid value for file descriptor timeout");
        goto fail1;
    }

    if (fd_timeout_ms > OPEN_FD_TIMEOUT_MS_MAX) {
        LOG_("Invalid value for file timeout");
        goto fail1;
    }

    uid_t new_uid = getuid();
    gid_t new_gid = getgid();

    if (uname) {
        struct passwd* pwd = getpwnam(uname);
        if (!pwd) {
            LOGERRNOV_("Couldn't find user %s", uname);
            goto fail1;
        }
        new_uid = pwd->pw_uid;
    }

    if (gname) {
        struct group* grp = getgrnam(gname);
        if (!grp) {
            LOGERRNOV_("Couldn't find group %s", gname);
            goto fail1;
        }
        new_gid = grp->gr_gid;
    }

    /* Ignore SIGPIPE */
    sigset_t sigmask;
    if (sigemptyset(&sigmask) == -1 ||
        sigaddset(&sigmask, SIGPIPE) == -1 ||
        sigprocmask(SIG_BLOCK, &sigmask, NULL) == -1) {
        LOGERRNO_("Couldn't ignore SIGPIPE");
        goto fail1;
    }

    if (daemonise && !proc_daemonise(NULL, 0)) {
        LOGERRNO_("Couldn't enter daemon mode");
        goto fail1;
    }

    sfd_log_open(SFD_PROGNAME, LOG_NDELAY | LOG_CONS | LOG_PID, LOG_DAEMON);

    if (!chroot_and_drop_privs(root_dir, new_uid, new_gid))
        goto fail1;

    const int requestfd = us_serve(sockdir, srvname, new_uid, new_gid);
    if (requestfd == -1) {
        LOGERRNO_("Failed to bind and listen");
        goto fail1;
    }

    if (do_sync) {
        if (!sync_parent(0)) {
            LOGERRNO_("Failed to sync with parent");
            goto fail2;
        }
        close(PROC_SYNCFD);
    }

    sfd_log(LOG_INFO,
            "Starting; name: %s; root_dir: \"%s\";"
            " uid: %d %s; gid: %d %s;"
            " maxfiles: %ld; fd_timeout_ms: %ld\n",
            srvname, root_dir,
            getuid(), uname, getgid(), gname, maxfiles, fd_timeout_ms);

    const bool success = srv_run(requestfd, (int)maxfiles, fd_timeout_ms);

    if (!success) {
        sfd_log(LOG_EMERG, "srv_run() failed [%m]; server shutting down\n");
    } else {
        sfd_log(LOG_INFO, "Shutting down\n");
    }

    us_stop_serving(sockdir, srvname, requestfd);

    return (success ? EXIT_SUCCESS : EXIT_FAILURE);

 fail2:
    us_stop_serving(sockdir, srvname, requestfd);
 fail1:
    if (do_sync && !sync_parent(errno)) {
        sfd_log(LOG_ERR, "Couldn't sync with parent process; errno: %m\n");
    }

    return EXIT_FAILURE;
}

static bool chroot_and_drop_privs(const char* root_dir,
                                  const uid_t new_uid,
                                  const gid_t new_gid)
{
    const uid_t euid = geteuid();
    const gid_t gid = getgid();

    if (new_uid == 0) {
        errno = EPERM;
        LOG_("Refusing to run as root user");
        return false;
    }

    if (new_gid == 0) {
        errno = EPERM;
        LOG_("Refusing to switch to 'root' group");
        return false;
    }

    if (strncmp(root_dir, "/", 2) != 0) {
        if (euid != 0) {
            errno = EACCES;
            LOG_("Executable doesn't appear to be setuid");
            return false;
        }

        if (proc_chroot(root_dir) == -1) {
            LOGERRNOV_("Couldn't chroot to %s", root_dir);
            return false;
        }

        if (chdir("/") == -1) {
            LOGERRNO_("Couldn't chdir to '/'");
            return false;
        }
    } else {
        sfd_log(LOG_WARNING,
                "Not chrooting because user-specified root dir is \"/\"\n");
    }

    if (new_gid != gid && setgid(new_gid) == -1) {
        LOGERRNOV_("Couldn't setgid to GID %d", new_gid);
        return false;
    }

    if (setuid(new_uid) == -1) {
        LOGERRNOV_("Couldn't setuid to UID %d", new_uid);
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
            LOGERRNO_("strtol");
        return -1;
    } else {
        return l;
    }
}

static void print_usage(const long fd_timeout_ms)
{
    printf("Usage: "
           SFD_PROGNAME" OPTION\n"
           "\nOptions:\n"
           "-r <root_dir> (chroot to this directory)\n"
           "-s <server_name> (user-friendly name to identify server instance)\n"
           "-n <maxfiles> (maximum number of concurrent file transfers)\n"
           "[-d] (run as a daemon)\n"
           "[-S <server_unix_socket_dir>] (default: \""SFD_SRV_SOCKDIR"\")\n"
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
