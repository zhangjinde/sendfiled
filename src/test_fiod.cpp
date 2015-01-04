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

#include <unistd.h>

#include <cerrno>
#include <cstdint>
#include <numeric>
#include <vector>

#include <gtest/gtest.h>

#include <fiod_config.h>

#include "impl/test_utils.hpp"

#include "fiod.h"
#include "impl/protocol_client.h"
#include "impl/server.h"
#include "impl/test_interpose.h"
#include "impl/unix_socket_server.h"
#include "impl/util.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wglobal-constructors"
#pragma GCC diagnostic ignored "-Wexit-time-destructors"
#pragma GCC diagnostic ignored "-Wpadded"

namespace {
bool wouldblock(const ssize_t err) noexcept
{
    return (err == -1 && (errno == EWOULDBLOCK || errno == EAGAIN));
}

/// Base fixture which runs the server in a separate process
struct FiodProcFix : public ::testing::Test {
    static const std::string srvname;
    static pid_t srv_pid;

    static void SetUpTestCase() {
        srv_pid = fiod_spawn(srvname.c_str(),
                             "/",
                             FIOD_SRV_SOCKDIR,
                             1000, 1000);
        if (srv_pid == -1)
            throw std::runtime_error("Couldn't start daemon");
    }

    static void TearDownTestCase() {
        if (srv_pid > 0) {
            const int status {fiod_shutdown(srv_pid)};
            if (!WIFEXITED(status))
                std::fprintf(stderr, "Daemon has not exited\n");
            if (WEXITSTATUS(status) != EXIT_SUCCESS)
                std::fprintf(stderr, "Daemon did not shut down cleanly\n");
        }
    }

    FiodProcFix() : srv_fd(fiod_connect(FIOD_SRV_SOCKDIR, srvname.c_str())) {
        if (!srv_fd)
            throw std::runtime_error("Couldn't connect to daemon");
    }

    test::unique_fd srv_fd;
};

const std::string FiodProcFix::srvname {"testing123"};
pid_t FiodProcFix::srv_pid;

/**
 * Base fixture which runs the server in a thread instead of a process.
 *
 * Currently exists solely to make it easier to control the mocked versions of
 * system calls such as sendfile(2) and splice(2).
 */
template<long OpenFileTimeoutMs>
struct FiodThreadFix : public ::testing::Test {
    static constexpr long open_file_timeout_ms {OpenFileTimeoutMs};
    static constexpr int maxfiles {1000};
    static const std::string srvname;

    FiodThreadFix() : srv_barr(2),
                thr([this] { run_server(); }) {

        srv_barr.wait();

        srv_fd = fiod_connect(FIOD_SRV_SOCKDIR, srvname.c_str());
        if (!srv_fd) {
            perror("fiod_connect()");
            stop_thread();
            throw std::runtime_error("Couldn't connect to daemon");
        }
    }

    ~FiodThreadFix() {
        stop_thread();
        mock_write_reset();
        mock_sendfile_reset();
        mock_splice_reset();
    }

    void run_server() {
        const int listenfd {us_serve(FIOD_SRV_SOCKDIR,
                                     srvname.c_str(),
                                     getuid(), getgid())};
        if (listenfd == -1) {
            perror("us_serve");
            throw std::runtime_error("Couldn't start server");
        }

        srv_barr.wait();

        srv_run(listenfd, maxfiles, OpenFileTimeoutMs);

        us_stop_serving(FIOD_SRV_SOCKDIR, srvname.c_str(), listenfd);
    }

    void stop_thread() {
        test::kill_thread(thr, SIGTERM);
        thr.join();
    }

    test::unique_fd srv_fd;
    test::thread_barrier srv_barr;
    std::thread thr;
};

template<long OpenFileTimeoutMs>
constexpr long FiodThreadFix<OpenFileTimeoutMs>::open_file_timeout_ms;

template<long OpenFileTimeoutMs>
constexpr int FiodThreadFix<OpenFileTimeoutMs>::maxfiles;

template<long OpenFileTimeoutMs>
const std::string FiodThreadFix<OpenFileTimeoutMs>::srvname {"testing123_thread"};

struct SmallFile {
    static const std::string file_contents;

    SmallFile() : file(file_contents) {}

    test::TmpFile file;
};

const std::string SmallFile::file_contents {"1234567890"};

struct LargeFile {
    static constexpr int NCHUNKS {1024};
    static constexpr int CHUNK_SIZE {1024};

    static void SetUpTestCase() {
        std::iota(file_chunk.begin(), file_chunk.end(), 0);
    }

    LargeFile() {
        for (int i = 0; i < NCHUNKS; i++)
            std::fwrite(file_chunk.data(), 1, file_chunk.size(), file);

        file.close();
    }

    test::TmpFile file;

private:
    static std::vector<std::uint8_t> file_chunk;
};

constexpr int LargeFile::NCHUNKS;
constexpr int LargeFile::CHUNK_SIZE;
std::vector<std::uint8_t> LargeFile::file_chunk(LargeFile::CHUNK_SIZE);

struct FiodProcSmallFileFix : public FiodProcFix, public SmallFile {};

struct FiodThreadSmallFileFix : public FiodThreadFix<1000>, public SmallFile {};

struct FiodThreadSmallFileShortOpenFileTimeoutFix :
        public FiodThreadFix<100>, public SmallFile {};

struct FiodProcLargeFileFix : public FiodProcFix, public LargeFile {
    static void SetUpTestCase() {
        LargeFile::SetUpTestCase();
        FiodProcFix::SetUpTestCase();
    }
};

struct FiodThreadLargeFileFix : public FiodThreadFix<1000>, public LargeFile {
    static void SetUpTestCase() {
        LargeFile::SetUpTestCase();
    }
};

} // namespace

// Non-existent file should respond to request with status message containing
// ENOENT.
TEST_F(FiodProcSmallFileFix, file_not_found)
{
    int data_pipe[2];

    ASSERT_NE(-1, pipe(data_pipe));

    const test::unique_fd data_fd {data_pipe[0]};

    const test::unique_fd stat_fd {fiod_send(srv_fd,
                                             (file.name() + "XXXXXXXXX").c_str(),
                                             data_pipe[1],
                                             0, 0, false)};
    close(data_pipe[1]);

    ASSERT_TRUE(stat_fd);

    uint8_t buf [PROT_REQ_MAXSIZE];
    struct fiod_xfer_stat ack;

    // Request ACK
    const ssize_t nread {read(stat_fd, buf, sizeof(ack))};
    ASSERT_EQ(sizeof(struct prot_hdr), nread);
    EXPECT_EQ(FIOD_FILE_INFO, fiod_get_cmd(buf));
    EXPECT_EQ(ENOENT, fiod_get_stat(buf));
    EXPECT_FALSE(fiod_unmarshal_xfer_stat(&ack, buf));
}

// This test should fail for any file which is not a regular file.
TEST_F(FiodProcFix, open_directory_fails)
{
    int data_pipe[2];

    if (pipe(data_pipe) == -1)
        FAIL() << "Couldn't create a pipe; errno: " << strerror(errno);

    const test::unique_fd data_fd {data_pipe[0]};

    const test::unique_fd stat_fd {fiod_send(srv_fd,
                                             "/",
                                             data_pipe[1],
                                             0, 0, false)};
    close(data_pipe[1]);

    ASSERT_TRUE(stat_fd);

    uint8_t buf [PROT_REQ_MAXSIZE];
    struct fiod_xfer_stat ack;

    // Request ACK
    const ssize_t nread {read(stat_fd, buf, sizeof(ack))};
    ASSERT_EQ(sizeof(struct prot_hdr), nread);

    EXPECT_EQ(FIOD_FILE_INFO, fiod_get_cmd(buf));
    EXPECT_EQ(EINVAL, fiod_get_stat(buf));
}

TEST_F(FiodProcSmallFileFix, send)
{
    // Pipe to which file will be written
    int data_pipe[2];

    ASSERT_NE(-1, pipe(data_pipe));

    const test::unique_fd stat_fd {fiod_send(srv_fd,
                                             file.name().c_str(),
                                             data_pipe[1],
                                             0, 0, false)};
    close(data_pipe[1]);

    ASSERT_TRUE(stat_fd);

    const test::unique_fd data_fd {data_pipe[0]};

    uint8_t buf [PROT_REQ_MAXSIZE];
    ssize_t nread;
    struct fiod_file_info ack;
    struct fiod_xfer_stat xfer_stat;

    // Request ACK
    nread = read(stat_fd, buf, sizeof(ack));
    ASSERT_EQ(sizeof(ack), nread);
    ASSERT_TRUE(fiod_unmarshal_file_info(&ack, buf));
    EXPECT_EQ(FIOD_FILE_INFO, ack.cmd);
    EXPECT_EQ(FIOD_STAT_OK, ack.stat);
    EXPECT_EQ(file_contents.size(), ack.size);

    // Transfer status update
    nread = read(stat_fd, buf, sizeof(xfer_stat));
    ASSERT_EQ(sizeof(xfer_stat), nread);
    ASSERT_TRUE(fiod_unmarshal_xfer_stat(&xfer_stat, buf));
    EXPECT_EQ(FIOD_XFER_STAT, xfer_stat.cmd);
    EXPECT_EQ(FIOD_STAT_OK, xfer_stat.stat);
    EXPECT_EQ(PROT_XFER_COMPLETE, xfer_stat.size);

    // File content
    nread = read(data_fd, buf, sizeof(buf));
    ASSERT_EQ(file_contents.size(), nread);
    const std::string recvd_file(reinterpret_cast<const char*>(buf),
                                 file_contents.size());
    EXPECT_EQ(file_contents, recvd_file);
}

/// Causes the final (and only, in this case) transfer status send to fail
/// temporarily. The server should keep trying until it is able to send the
/// final status.
TEST_F(FiodThreadSmallFileFix, send_final_xfer_status_fails)
{
    // Pipe to which file will be written
    int data_pipe[2];

    ASSERT_NE(-1, pipe(data_pipe));

    // Due to edge-trigged mode, more than one EWOULDBLOCK will cause
    // deadlock. The lone one only works because when the final xfer status send
    // fails with EWOULDBLOCK, it de-registers the dest fd and registers the
    // stat fd, causing its events to be re-read.
    const std::vector<ssize_t> retvals {MOCK_REALRV, -EWOULDBLOCK};
    mock_write_set_retval_n(retvals.data(), (int)retvals.size());

    const test::unique_fd stat_fd {fiod_send(srv_fd,
                                             file.name().c_str(),
                                             data_pipe[1],
                                             0, 0, false)};
    close(data_pipe[1]);

    ASSERT_TRUE(stat_fd);

    const test::unique_fd data_fd {data_pipe[0]};

    uint8_t buf [PROT_REQ_MAXSIZE];
    ssize_t nread;
    struct fiod_file_info ack;
    struct fiod_xfer_stat xfer_stat;

    // Request ACK
    nread = read(stat_fd, buf, sizeof(ack));
    ASSERT_EQ(sizeof(ack), nread);
    ASSERT_TRUE(fiod_unmarshal_file_info(&ack, buf));
    EXPECT_EQ(FIOD_FILE_INFO, ack.cmd);
    EXPECT_EQ(FIOD_STAT_OK, ack.stat);
    EXPECT_EQ(file_contents.size(), ack.size);

    // Transfer status update
    nread = read(stat_fd, buf, sizeof(xfer_stat));
    ASSERT_EQ(sizeof(xfer_stat), nread);
    ASSERT_TRUE(fiod_unmarshal_xfer_stat(&xfer_stat, buf));
    EXPECT_EQ(FIOD_XFER_STAT, xfer_stat.cmd);
    EXPECT_EQ(FIOD_STAT_OK, xfer_stat.stat);
    EXPECT_EQ(PROT_XFER_COMPLETE, xfer_stat.size);

    // File content
    nread = read(data_fd, buf, sizeof(buf));
    ASSERT_EQ(file_contents.size(), nread);
    const std::string recvd_file(reinterpret_cast<const char*>(buf),
                                 file_contents.size());
    EXPECT_EQ(file_contents, recvd_file);
}

TEST_F(FiodThreadSmallFileFix, send_error_send_fails)
{
    // Pipe to which file will be written
    int data_pipe[2];

    ASSERT_NE(-1, pipe(data_pipe));

    // Due to edge-trigged mode, more than one EWOULDBLOCK will cause
    // deadlock. The lone one only works because when the final xfer status send
    // fails with EWOULDBLOCK, it de-registers the dest fd and registers the
    // stat fd, causing its events to be re-read.
    const std::vector<ssize_t> write_retvals {MOCK_REALRV, -EWOULDBLOCK};
    mock_write_set_retval_n(write_retvals.data(), (int)write_retvals.size());

    const test::unique_fd stat_fd {fiod_send(srv_fd,
                                             file.name().c_str(),
                                             data_pipe[1],
                                             0, 0, false)};
    close(data_pipe[1]);

    mock_sendfile_set_retval(-EIO);

    ASSERT_TRUE(stat_fd);

    const test::unique_fd data_fd {data_pipe[0]};

    uint8_t buf [PROT_REQ_MAXSIZE];
    ssize_t nread;
    struct fiod_file_info ack;
    struct prot_hdr err;

    // Request ACK
    nread = read(stat_fd, buf, sizeof(ack));
    ASSERT_EQ(sizeof(ack), nread);
    ASSERT_TRUE(fiod_unmarshal_file_info(&ack, buf));
    EXPECT_EQ(FIOD_FILE_INFO, ack.cmd);
    EXPECT_EQ(FIOD_STAT_OK, ack.stat);
    EXPECT_EQ(file_contents.size(), ack.size);

    // Error code
    nread = read(stat_fd, buf, sizeof(err));
    ASSERT_EQ(sizeof(err), nread);
    memcpy(&err, buf, sizeof(err));
    EXPECT_EQ(FIOD_XFER_STAT, err.cmd);
    EXPECT_EQ(EIO, err.stat);

    // Not checking data channel because server is under the impression that
    // fatal error has occured and therefore will not have bothered to close the
    // data fd.
}

TEST_F(FiodProcSmallFileFix, read)
{
    const test::unique_fd data_fd {fiod_read(srv_fd,
                                             file.name().c_str(),
                                             0, 0, false)};
    ASSERT_TRUE(data_fd);

    uint8_t buf [PROT_REQ_MAXSIZE];
    ssize_t nread;
    struct fiod_file_info ack;

    // Request ACK
    nread = read(data_fd, buf, sizeof(ack));
    ASSERT_EQ(sizeof(ack), nread);
    ASSERT_TRUE(fiod_unmarshal_file_info(&ack, buf));
    EXPECT_EQ(FIOD_FILE_INFO, ack.cmd);
    EXPECT_EQ(FIOD_STAT_OK, ack.stat);
    EXPECT_EQ(file_contents.size(), ack.size);

    // File content
    nread = read(data_fd, buf, sizeof(buf));
    ASSERT_EQ(file_contents.size(), nread);
    const std::string recvd_file(reinterpret_cast<const char*>(buf),
                                 file_contents.size());
    EXPECT_EQ(file_contents, recvd_file);
}

TEST_F(FiodProcSmallFileFix, send_open_file)
{
    // Create pipe to which file will ultimately be written
    int pfd [2];
    ASSERT_NE(-1, pipe(pfd));
    const test::unique_fd pipe_read {pfd[0]};
    test::unique_fd pipe_write {pfd[1]};

    // Open the file
    const test::unique_fd stat_fd {fiod_open(srv_fd,
                                             file.name().c_str(),
                                             0, 0, false)};
    ASSERT_TRUE(stat_fd);

    ssize_t nread;
    struct fiod_open_file_info ack;
    uint8_t buf [sizeof(ack)];

    // Receive 'ACK' with file stats
    nread = read(stat_fd, buf, sizeof(ack));
    ASSERT_EQ(sizeof(ack), nread);
    ASSERT_TRUE(fiod_unmarshal_open_file_info(&ack, buf));
    EXPECT_EQ(FIOD_OPEN_FILE_INFO, ack.cmd);
    EXPECT_EQ(FIOD_STAT_OK, ack.stat);
    EXPECT_EQ(file_contents.size(), ack.size);
    EXPECT_GT(ack.txnid, 0);

    // Send 'open file'
    ASSERT_TRUE(fiod_send_open(srv_fd, ack.txnid, pipe_write));
    ::close(pipe_write);

    // Read file data
    nread = read(pipe_read, buf, sizeof(buf));
    ASSERT_EQ(file_contents.size(), nread);
    const std::string recvd_file(reinterpret_cast<const char*>(buf),
                                 static_cast<std::size_t>(nread));
    EXPECT_EQ(file_contents, recvd_file);

    // Read transfer status
    struct fiod_xfer_stat xstat;
    nread = read(stat_fd, buf, sizeof(buf));
    ASSERT_EQ(sizeof(xstat), nread);
    ASSERT_TRUE(fiod_unmarshal_xfer_stat(&xstat, buf));
    EXPECT_EQ(PROT_XFER_COMPLETE, xstat.size);
}

/**
 * Opens a file, waits too long, then requests server to send the opened
 * file. By this time the timer should've expired and the open file closed,
 * resulting in an error message from the server.
 */
TEST_F(FiodThreadSmallFileShortOpenFileTimeoutFix, open_file_timeout)
{
    // Create pipe to which file will ultimately be written
    int pfd [2];
    ASSERT_NE(-1, pipe(pfd));
    const test::unique_fd pipe_read {pfd[0]};
    test::unique_fd pipe_write {pfd[1]};

    // Open the file
    const test::unique_fd stat_fd {fiod_open(srv_fd,
                                             file.name().c_str(),
                                             0, 0, false)};
    ASSERT_TRUE(stat_fd);

    struct fiod_open_file_info ack;
    uint8_t buf [sizeof(ack)];
    ssize_t nread;

    // Confirm that the file was opened
    nread = read(stat_fd, buf, sizeof(ack));
    ASSERT_EQ(sizeof(ack), nread);
    ASSERT_EQ(FIOD_OPEN_FILE_INFO, fiod_get_cmd(buf));
    ASSERT_EQ(FIOD_STAT_OK, fiod_get_stat(buf));
    ASSERT_TRUE(fiod_unmarshal_open_file_info(&ack, buf));
    ASSERT_EQ(file_contents.size(), ack.size);
    ASSERT_GT(ack.txnid, 0);

    // Sleep for longer than the server's open file timeout
    std::this_thread::sleep_for(std::chrono::milliseconds{open_file_timeout_ms});

    // Send 'open file'
    ASSERT_TRUE(fiod_send_open(srv_fd, ack.txnid, pipe_write));
    ::close(pipe_write);

    // Server should've written the timeout error message to the status
    // channel...
    nread = read(stat_fd, buf, sizeof(buf));
    EXPECT_EQ(sizeof(struct prot_hdr), nread);
    EXPECT_EQ(FIOD_XFER_STAT, fiod_get_cmd(buf));
    EXPECT_EQ(ETIME, fiod_get_stat(buf));
    // ... and then should've closed the status channel.
    EXPECT_EQ(0, read(stat_fd, buf, sizeof(buf)));

    // Data channel should have been closed at timeout, before any data had been
    // written to it.
    nread = read(pipe_read, buf, sizeof(buf));
    EXPECT_EQ(0, nread);
}

TEST_F(FiodProcSmallFileFix, read_range)
{
    constexpr loff_t offset {3};
    constexpr size_t len {5};

    const test::unique_fd data_fd {fiod_read(srv_fd,
                                             file.name().c_str(),
                                             offset, len, false)};
    ASSERT_TRUE(data_fd);

    uint8_t buf [PROT_REQ_MAXSIZE];
    ssize_t nread;
    struct fiod_file_info ack;

    // Request ACK
    nread = read(data_fd, buf, sizeof(ack));
    ASSERT_EQ(sizeof(ack), nread);
    ASSERT_TRUE(fiod_unmarshal_file_info(&ack, buf));
    EXPECT_EQ(FIOD_FILE_INFO, ack.cmd);
    EXPECT_EQ(FIOD_STAT_OK, ack.stat);
    EXPECT_EQ(len, ack.size);

    // File content
    nread = read(data_fd, buf, sizeof(buf));
    ASSERT_EQ(len, nread);
    const std::string recvd_file(reinterpret_cast<const char*>(buf), len);
    std::printf("recvd_file: %s\n", recvd_file.c_str());
    EXPECT_EQ(0, memcmp(buf, file_contents.c_str() + offset, len));
}

TEST_F(FiodProcLargeFileFix, multiple_reading_clients)
{
    constexpr int nclients {10};

    struct client {
        test::unique_fd data_fd {};
        std::size_t nchunks {};
        std::size_t nread_chunk {};
    };

    client clients [nclients];

    for (int i = 0; i < nclients; i++) {
        client& cli {clients[i]};
        cli.data_fd = fiod_read(srv_fd, file.name().c_str(), 0, 0, false);
        ASSERT_TRUE(cli.data_fd);

        uint8_t buf [PROT_REQ_MAXSIZE];
        ssize_t nread;
        struct fiod_file_info ack;

        // Request ACK
        nread = read(cli.data_fd, buf, sizeof(ack));
        ASSERT_EQ(sizeof(ack), nread);
        ASSERT_TRUE(fiod_unmarshal_file_info(&ack, buf));
        EXPECT_EQ(FIOD_FILE_INFO, ack.cmd);
        EXPECT_EQ(FIOD_STAT_OK, ack.stat);
        EXPECT_EQ(CHUNK_SIZE * NCHUNKS, ack.size);
    }

    // File content
    std::vector<std::uint8_t> recvbuf(CHUNK_SIZE);
    int ndone {};

    while (ndone < nclients) {
        for (int i = 0; i < nclients; i++) {
            client& cli {clients[i]};

            if (cli.nchunks == NCHUNKS)
                continue;

            ssize_t n = read(cli.data_fd,
                             recvbuf.data() + cli.nread_chunk,
                             recvbuf.size() - cli.nread_chunk);

            ASSERT_NE(-1, n);
            ASSERT_NE(0, n);

            cli.nread_chunk += (size_t)n;

            if (cli.nread_chunk == recvbuf.size()) {
                for (size_t j = 0; j < recvbuf.size(); j++)
                    ASSERT_EQ((uint8_t)j, (uint8_t)recvbuf[j]);
                cli.nread_chunk = 0;
                cli.nchunks++;
            }

            if (cli.nchunks == NCHUNKS)
                ndone++;
        }
    }

    for (int i = 0; i < nclients; i++)
        EXPECT_EQ(NCHUNKS, clients[i].nchunks);
}

TEST_F(FiodProcFix, multiple_clients_reading_different_large_files)
{
    constexpr int NCLIENTS {10};
    constexpr int NCHUNKS {1024};
    constexpr int CHUNK_SIZE {1024};

    struct client {
        test::TmpFile file {};
        test::unique_fd data_fd {};
        std::size_t nchunks {};
        std::size_t nread_chunk {};
    };

    std::vector<std::uint8_t> file_chunk(CHUNK_SIZE);
    std::iota(file_chunk.begin(), file_chunk.end(), 0);

    client clients [NCLIENTS];

    // Write file contents
    for (int i = 0; i < NCLIENTS; i++) {
        client& cli {clients[i]};
        for (int j = 0; j < NCHUNKS; j++)
            std::fwrite(file_chunk.data(), 1, file_chunk.size(), cli.file);
        cli.file.close();
    }

    // Send requests
    for (int i = 0; i < NCLIENTS; i++) {
        client& cli {clients[i]};

        cli.data_fd = fiod_read(srv_fd, cli.file.name().c_str(), 0, 0, false);
        ASSERT_TRUE(cli.data_fd);

        uint8_t buf [PROT_REQ_MAXSIZE];
        ssize_t nread;
        struct fiod_file_info ack;

        // Request ACK
        nread = read(cli.data_fd, buf, sizeof(ack));
        ASSERT_EQ(sizeof(ack), nread);
        ASSERT_TRUE(fiod_unmarshal_file_info(&ack, buf));
        EXPECT_EQ(FIOD_FILE_INFO, ack.cmd);
        EXPECT_EQ(FIOD_STAT_OK, ack.stat);
        EXPECT_EQ(CHUNK_SIZE * NCHUNKS, ack.size);
    }

    // Read file content
    std::vector<std::uint8_t> recvbuf(CHUNK_SIZE);
    int ndone {};

    while (ndone < NCLIENTS) {
        for (int i = 0; i < NCLIENTS; i++) {
            client& cli {clients[i]};

            if (cli.nchunks == NCHUNKS)
                continue;

            ssize_t n = read(cli.data_fd,
                             recvbuf.data() + cli.nread_chunk,
                             recvbuf.size() - cli.nread_chunk);

            ASSERT_NE(-1, n);
            ASSERT_NE(0, n);

            cli.nread_chunk += (size_t)n;

            if (cli.nread_chunk == CHUNK_SIZE) {
                for (size_t j = 0; j < CHUNK_SIZE; j++)
                    ASSERT_EQ((uint8_t)j, (uint8_t)recvbuf[j]);
                cli.nread_chunk = 0;
                cli.nchunks++;
            }

            if (cli.nchunks == NCHUNKS)
                ndone++;
        }
    }

    for (int i = 0; i < NCLIENTS; i++)
        EXPECT_EQ(NCHUNKS, clients[i].nchunks);
}

TEST_F(FiodThreadLargeFileFix, read_io_error)
{
    const test::unique_fd data_fd {
        fiod_read(srv_fd, file.name().c_str(), 0, 0, false)};
    ASSERT_TRUE(data_fd);

    uint8_t buf [PROT_REQ_MAXSIZE];
    ssize_t nread;
    struct fiod_file_info ack;

    // Request ACK
    nread = read(data_fd, buf, sizeof(ack));
    ASSERT_EQ(sizeof(ack), nread);
    ASSERT_TRUE(fiod_unmarshal_file_info(&ack, buf));
    EXPECT_EQ(FIOD_FILE_INFO, ack.cmd);
    EXPECT_EQ(FIOD_STAT_OK, ack.stat);
    EXPECT_EQ(CHUNK_SIZE * NCHUNKS, ack.size);

    // File content
    std::vector<std::uint8_t> recvbuf(CHUNK_SIZE);
    size_t nchunks {};
    size_t nread_chunk {};
    bool got_eof {false};

    for (;;) {
        if (nchunks > NCHUNKS / 2)
            mock_splice_set_retval(-EIO);

        ssize_t n = read(data_fd,
                         recvbuf.data() + nread_chunk,
                         CHUNK_SIZE - nread_chunk);

        if (n == 0) {
            got_eof = true;
            break;
        }

        nread_chunk += (size_t)n;

        if (nread_chunk == CHUNK_SIZE) {
            nread_chunk = 0;
            nchunks++;
        }

        if (nchunks == NCHUNKS)
            break;
    }

    EXPECT_TRUE(got_eof);
    EXPECT_LT(nchunks, NCHUNKS);
}

TEST_F(FiodThreadLargeFileFix, send_io_error)
{
    int data_pipe [2];
    ASSERT_NE(-1, pipe(data_pipe));

    const test::unique_fd data_fd {data_pipe[0]};

    const test::unique_fd stat_fd {fiod_send(srv_fd,
                                             file.name().c_str(),
                                             data_pipe[1],
                                             0, 0, true)};
    close(data_pipe[1]);

    ASSERT_TRUE(stat_fd);

    std::vector<uint8_t> data_buf(CHUNK_SIZE);
    std::vector<uint8_t> stat_buf(sizeof(struct fiod_file_info));
    ssize_t nread;
    struct fiod_file_info ack;
    struct fiod_xfer_stat xfer_stat;

    // Request ACK
    while (wouldblock(nread = read(stat_fd, stat_buf.data(), sizeof(ack)))) {}

    ASSERT_EQ(sizeof(ack), nread);
    ASSERT_TRUE(fiod_unmarshal_file_info(&ack, stat_buf.data()));
    EXPECT_EQ(FIOD_FILE_INFO, ack.cmd);
    EXPECT_EQ(FIOD_STAT_OK, ack.stat);
    EXPECT_EQ(CHUNK_SIZE * NCHUNKS, ack.size);

    size_t nchunks {};
    ssize_t nread_chunk {};
    bool got_data_eof {false};
    bool got_stat_eof {false};
    bool got_errno {false};

    for (;;) {
        if (nchunks > NCHUNKS / 2)
            mock_sendfile_set_retval(-EIO);

        nread = read(data_fd,
                     data_buf.data() + nread_chunk,
                     CHUNK_SIZE - (size_t)nread_chunk);

        if (nread == 0) {
            got_data_eof = true;
            break;
        }

        ASSERT_GT(nread, 0);

        nread_chunk += nread;

        if (nread_chunk >= CHUNK_SIZE) {
            if (++nchunks == NCHUNKS)
                break;
            nread_chunk = 0;
        }

        // Read transfer status update (don't let pipe fill up)
        nread = read(stat_fd, stat_buf.data(), sizeof(xfer_stat));
        if (wouldblock(nread))
            continue;
        ASSERT_GE(sizeof(struct prot_hdr), nread);
        ASSERT_EQ(FIOD_XFER_STAT, fiod_get_cmd(stat_buf.data()));
        got_errno = (fiod_get_stat(stat_buf.data()) == EIO);
        if (fiod_get_stat(stat_buf.data()) != FIOD_STAT_OK)
            break;
    }

    ASSERT_TRUE(set_nonblock(stat_fd, false));

    while (!got_stat_eof) {
        nread = read(stat_fd, stat_buf.data(), sizeof(xfer_stat));

        if (nread == 0) {
            got_stat_eof = true;

        } else if (nread == -1) {
            if (wouldblock(nread))
                continue;
            else
                break;
        } else {
            ASSERT_GE(nread, sizeof(struct prot_hdr));

            ASSERT_EQ(FIOD_XFER_STAT, fiod_get_cmd(stat_buf.data()));
            got_errno = (fiod_get_stat(stat_buf.data()) == EIO);
        }
    }

    EXPECT_TRUE(got_data_eof);
    EXPECT_TRUE(got_stat_eof);
    EXPECT_TRUE(got_errno);
    EXPECT_LT(nchunks, NCHUNKS);
}

#pragma GCC diagnostic pop
