#ifndef TEST_UTILS_HPP
#define TEST_UTILS_HPP

#include <cstdio>
#include <stdexcept>
#include <string>
#include <thread>

#include "../attributes.h"

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
class DSO_EXPORT TmpFile {
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
class DSO_EXPORT thread_barrier final {
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
