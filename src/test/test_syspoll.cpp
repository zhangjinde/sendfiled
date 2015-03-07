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

#include <sys/stat.h>

#include <fcntl.h>
#include <signal.h>

#include <future>

#include <gtest/gtest.h>

#include "../impl/sendfiled.h"
#include "../impl/syspoll.h"
#include "../impl/test_utils.hpp"
#include "../impl/util.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wglobal-constructors"
#pragma GCC diagnostic ignored "-Wexit-time-destructors"
#pragma GCC diagnostic ignored "-Wpadded"

TEST(Syspoll, read_write)
{
    auto p = syspoll_new(100);
    if (!p)
        FAIL() << "Couldn't create poller";

    test::tmpfifo fd;
    syspoll_resrc resrc {fd};

    EXPECT_TRUE(syspoll_register(p, &resrc, SYSPOLL_READ | SYSPOLL_WRITE));

    // Make it readable
    const int byte {123};
    if (write(fd, &byte, 1) != 1)
        FAIL() << "Couldn't write to FIFO";

    const int nevents {syspoll_wait(p)};
    ASSERT_NE(-1, nevents);
    EXPECT_GE(nevents, 1);

    bool got_read {false};
    bool got_write {false};

    for (int i = 0; i < nevents; i++) {
        syspoll_events event = syspoll_get(p, i);
        if (event.events & SYSPOLL_READ)
            got_read = true;
        if (event.events & SYSPOLL_WRITE)
            got_write = true;
    }

    EXPECT_TRUE(got_read);
    EXPECT_TRUE(got_write);

    EXPECT_TRUE(syspoll_deregister(p, fd));

    syspoll_delete(p);
}

/**
 * On Linux, pipe writers are woken whenever the pipe's I/O space is drained to
 * 0. E.g.:
 *
 * pipe(); EPOLLOUT; (first poll right after creation)
 * write(1); read(1); EPOLLOUT
 * write(2); read(1); read(1); EPOLLOUT
 *
 * On FreeBSD the writer is woken a few more times that the reader drained the
 * pipe (e.g., 10,015 wakes for 9,999 draining reads.
 */
TEST(Syspoll, pipe_writability)
{
    constexpr int expected_nwrites {9999};

    test::unique_fd pipe_read;
    test::unique_fd pipe_write;
    {
        int fds[2];
        if (pipe(fds) == -1)
            FAIL() << "Couldn't create pipe";

        pipe_read = fds[0];
        pipe_write = fds[1];
    }

    pthread_t tid;

    auto poll_thr = [&tid, fd = std::move(pipe_write)] {
        tid = pthread_self();

        size_t nwrites {};

        auto p = syspoll_new(100);
        if (!p)
            return nwrites;

        syspoll_resrc resrc {fd};

        if (!syspoll_register(p, &resrc, SYSPOLL_WRITE))
            goto done;

        for (;;) {
            const int nevents {syspoll_wait(p)};

            for (int i = 0; i < nevents; i++) {
                auto ev = syspoll_get(p, i);

                if (ev.events & SYSPOLL_WRITE) {
                    nwrites++;
                    if (write(fd, &nwrites, 1) == -1)
                        goto done;
                }

                if (ev.events & SYSPOLL_TERM)
                    goto done;
            }
        }

    done:
        syspoll_delete(p);

        return nwrites;
    };

    auto poll_res = std::async(std::launch::async, std::move(poll_thr));

    int byte {};

    for (int i = 0; i < expected_nwrites - 1; i++) {
        EXPECT_EQ(1, read(pipe_read, &byte, 1));
    }

    pthread_kill(tid, SIGTERM);

    EXPECT_GE(poll_res.get(), expected_nwrites);
}

/**
 * On Linux EPOLLOUT is reported on a socket thus:
 *
 * socket() EPOLLOUT (first poll after creation)
 *
 * W(IO_SPACE - 1); R(2); W(3); EPOLLOUT
 *
 * This differs from pipes (on Linux, at least; see test pipe_writability).
 */
TEST(Syspoll, socket_writability)
{
    auto sockets = test::make_connection(59999);

    pthread_t tid;

    auto poll_thr = [&tid, fd = std::move(sockets.first)] {
        tid = pthread_self();

        size_t nwrites {};

        auto p = syspoll_new(100);
        if (!p)
            return nwrites;

        syspoll_resrc resrc {fd};

        if (!syspoll_register(p, &resrc, SYSPOLL_WRITE))
            goto done;

        for (;;) {
            const int nevents {syspoll_wait(p)};

            for (int i = 0; i < nevents; i++) {
                auto ev = syspoll_get(p, i);

                if (ev.events & SYSPOLL_WRITE) {
                    nwrites++;
                    if (write(fd, &nwrites, 1) == -1)
                        goto done;
                }

                if (ev.events & SYSPOLL_TERM)
                    goto done;
            }
        }

    done:
        syspoll_delete(p);

        return nwrites;
    };

    auto poll_res = std::async(std::launch::async, std::move(poll_thr));

    ASSERT_TRUE(set_nonblock(sockets.second, true));

    int byte {};

    std::size_t nreads {};
    std::size_t nblocks {};

    while (nblocks < 100000) {
        const ssize_t nread {read(sockets.second, &byte, 1)};

        if (nread == -1) {
            ASSERT_TRUE(errno == EWOULDBLOCK || errno == EAGAIN);
            nblocks++;
        } else if (nread == 0) {
            break;
        } else {
            ASSERT_EQ(1, nread);
            nreads++;
        }
    }

    pthread_kill(tid, SIGTERM);

    EXPECT_EQ(1, poll_res.get());
    EXPECT_EQ(1, nreads);
}

#pragma GCC diagnostic pop
