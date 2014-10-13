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

#ifndef TEST_UTILS_HPP
#define TEST_UTILS_HPP

#include <cstdio>
#include <stdexcept>
#include <string>
#include <thread>

namespace test {

/// An RAII file descriptor
class unique_fd final {
public:
    unique_fd() : fd(-1) {}

    unique_fd(const int fd_) : fd(fd_) {}

    ~unique_fd() {
        close();
    }

    unique_fd(const unique_fd&) = delete;
    unique_fd& operator=(const unique_fd&) = delete;

    unique_fd(unique_fd&& that) noexcept : fd(that.fd) {
        that.fd = -1;
    }

    unique_fd& operator=(unique_fd&& that) noexcept {
        reset(that.release());
        return *this;
    }

    int release() noexcept {
        const int tmp = fd;
        fd = -1;
        return tmp;
    }

    void reset(const int fd_) noexcept {
        close();
        fd = fd_;
    }

    operator int() const noexcept {
        return fd;
    }

    operator bool() const noexcept {
        return (fd != -1);
    }

    void close() noexcept {
        if (*this) {
            ::close(fd);
            fd = -1;
        }
    }

private:
    int fd;
};

/**
 * A RAII temporary file.
 */
class TmpFile {
    TmpFile(const TmpFile&) = delete;
    TmpFile(TmpFile&&) = delete;
    TmpFile& operator=(const TmpFile&) = delete;
    TmpFile& operator=(TmpFile&&) = delete;
public:
    TmpFile();

    explicit TmpFile(const std::string& contents);

    ~TmpFile();

    void close();

    const std::string& name() const noexcept;

    operator int();

    operator FILE*();
private:
    std::string name_;
    FILE* fp_;
};

/**
 * A thread rendezvous point in the spirit of @a boost::barrier.
 *
 * Doesn't seem to be anything comparable in C++11.
 */
class thread_barrier final {
public:
    thread_barrier(const thread_barrier&) = delete;
    thread_barrier(thread_barrier&&) = delete;
    thread_barrier& operator=(const thread_barrier&) = delete;
    thread_barrier& operator=(thread_barrier&&) = delete;

    /**
     * Constructor.
     *
     * @param nthreads The number of threads that will be meeting at the
     * thread_barrier.
     */
    explicit thread_barrier(int nthreads) noexcept;

    /**
     * Blocks until all of the other threads have arrived at the thread_barrier
     * point.
     */
    void wait();

private:
    /**
     * The total number of threads that will be meeting at the thread_barrier
     * point.
     */
    const int nthreads;

    /**
     * The number of threads that have arrived. This is the CV's condition and
     * is therefore protected by @a mtx.
     */
    int nwaiting;

    /**
     * CV used to wake all waiting threads once they've all arrived.
     */
    std::condition_variable cv;

    /**
     * Mutex used with the condition variable; protects @a nwaiting.
     */
    std::mutex mtx;
};

/// Sends a signal to a thread.
void kill_thread(std::thread& t, int signum);

} // namespace test

#endif
