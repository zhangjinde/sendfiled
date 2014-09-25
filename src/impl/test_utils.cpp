#include <unistd.h>

#include <cstdlib>
#include <signal.h>

#include "test_utils.hpp"

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
