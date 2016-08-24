/*
  Copyright (c) 2016, Francois Kritzinger
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

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <netinet/in.h>

#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>

#include "test_utils.hpp"

[[noreturn]]
void test::throw_errno()
{
    throw std::system_error{
        std::make_error_code(static_cast<std::errc>(errno))};
}

// --------------- test::TmpFile ---------------------

test::TmpFile::TmpFile() :
    name_{},
    fp_{}
{
    char name[] {"/tmp/unittesttmpXXXXXX"};

    const int fd {::mkstemp(name)};
    if (fd == -1)
        throw std::runtime_error{"Couldn't create temporary file (mkstemp)"};

    name_ = name;

    fp_ = ::fdopen(fd, "w");
    if (!fp_) {
        this->close();
        throw std::runtime_error{"Couldn't create temporary file (fdopen)"};
    }
}

test::TmpFile::TmpFile(const std::string& contents) :
    TmpFile()
{
    if (std::fputs(contents.c_str(), fp_) == EOF)
        throw std::runtime_error("Couldn't write to temp file");
    this->close();
    fp_ = nullptr;
}

test::TmpFile::~TmpFile()
{
    this->close();
    if (::unlink(name().c_str()) == -1)
        fprintf(stderr, "Couldn't unlink file %s\n", name().c_str());
}

void test::TmpFile::close()
{
    if (fp_ != nullptr) {
        std::fclose(fp_);
        fp_ = nullptr;
    }
}

const std::string& test::TmpFile::name() const noexcept
{
    return name_;
}

test::TmpFile::operator int()
{
    return fileno(fp_);
}

test::TmpFile::operator FILE*()
{
    return fp_;
}

// ------------ test::tmpfifo ------------

test::tmpfifo::tmpfifo()
{
    {
        TmpFile file;
        fname = file.name();
    }

    if (mkfifo(fname.c_str(), S_IRUSR | S_IWUSR) != 0) {
        throw std::runtime_error{"Couldn't create FIFO; errno: " +
                std::to_string(errno) + " " + strerror(errno)};
    }

    fd = open(fname.c_str(), O_RDWR);
    if (!fd) {
        unlink(fname.c_str());
        throw std::runtime_error{"Couldn't open FIFO; errno: " +
                std::to_string(errno) + " " + strerror(errno)};
    }
}

test::tmpfifo::~tmpfifo()
{
    close(fd);
    unlink(fname.c_str());
}

test::tmpfifo::operator int() const noexcept
{
    return fd;
}

// -------------------- test::make_connection() ---------------------

std::pair<test::unique_fd, test::unique_fd> test::make_connection(int port)
{
    test::unique_fd listenfd {::socket(AF_INET, SOCK_STREAM, 0)};
    if (!listenfd)
        throw_errno();

    const int on {1};
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) == -1)
        throw_errno();

    struct sockaddr_in addr {};

    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<std::uint16_t>(port));
    addr.sin_addr = {INADDR_ANY};

    if (bind(listenfd,
             reinterpret_cast<const struct sockaddr*>(&addr),
             sizeof(addr)) == -1) {
        throw_errno();
    }

    if (listen(listenfd, 100) == -1)
        throw_errno();

    test::unique_fd cli {::socket(AF_INET, SOCK_STREAM, 0)};
    if (!cli)
        throw_errno();

    if (connect(cli,
                reinterpret_cast<const struct sockaddr*>(&addr),
                sizeof(addr)) == -1) {
        throw_errno();
    }

    test::unique_fd srv {accept(listenfd, NULL, NULL)};
    if (!srv)
        throw_errno();

    return {std::move(cli), std::move(srv)};
}

// ------------ test::thread_barrier ----------------

test::thread_barrier::thread_barrier(const int nthreads_in) noexcept :
    nthreads {nthreads_in},
    nwaiting {0}
{}

void test::thread_barrier::wait()
{
    std::unique_lock<std::mutex> l {mtx};

    nwaiting++;

    if (nwaiting >= nthreads)
        cv.notify_all();
    else
        cv.wait(l, [this] {return (nwaiting >= nthreads);});
}

// ----------------- test namespace --------------------

void test::kill_thread(std::thread& t, int signum)
{
    pthread_kill(t.native_handle(), signum);
}
