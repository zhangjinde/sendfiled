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

#include <sys/types.h>
#include <sys/wait.h>

#include <unistd.h>

#include <cerrno>
#include <cstdint>
#include <csignal>
#include <future>
#include <numeric>
#include <vector>

#include <gtest/gtest.h>

#include <sfd_config.h>

#include "../impl/test_utils.hpp"

#include "../sendfiled.h"
#include "../impl/protocol_client.h"
#include "../impl/server.h"
#include "../impl/syspoll.h"
#include "../impl/test_interpose.h"
#include "../impl/unix_socket_server.h"
#include "../impl/util.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wglobal-constructors"
#pragma GCC diagnostic ignored "-Wexit-time-destructors"
#pragma GCC diagnostic ignored "-Wpadded"
#pragma GCC diagnostic ignored "-Wold-style-cast"

namespace {

constexpr int test_port {59999};

template<std::size_t MaxFiles>
struct SfdProcFixTemplate : public ::testing::Test {
    static constexpr std::size_t maxfiles {MaxFiles};
    static const std::string srvname;
    static pid_t srv_pid;

    static void SetUpTestCase() {
        srv_pid = sfd_spawn(srvname.c_str(),
                             "/",
                             SFD_SRV_SOCKDIR,
                             MaxFiles, 1000);
        if (srv_pid == -1)
            throw std::runtime_error("Couldn't start daemon");
    }

    static void TearDownTestCase() {
        if (srv_pid > 0) {
            const int status {sfd_shutdown(srv_pid)};
            if (!WIFEXITED(status))
                std::fprintf(stderr, "Daemon has not exited\n");
            if (WEXITSTATUS(status) != EXIT_SUCCESS)
                std::fprintf(stderr, "Daemon did not shut down cleanly\n");
        }
    }

    SfdProcFixTemplate() : srv_fd(sfd_connect(SFD_SRV_SOCKDIR, srvname.c_str())) {
        if (!srv_fd)
            throw std::runtime_error("Couldn't connect to daemon");
    }

    test::unique_fd srv_fd;
};

template<std::size_t MaxFiles>
const std::string SfdProcFixTemplate<MaxFiles>::srvname {"testing123"};

template<std::size_t MaxFiles>
pid_t SfdProcFixTemplate<MaxFiles>::srv_pid;

using SfdProcFix = SfdProcFixTemplate<1000>;

/**
 * Base fixture which runs the server in a thread instead of a process.
 *
 * Currently exists solely to make it easier to control the mocked versions of
 * system calls such as sendfile(2) and splice(2).
 */
template<long OpenFileTimeoutMs>
struct SfdThreadFix : public ::testing::Test {
    static constexpr long open_file_timeout_ms {OpenFileTimeoutMs};
    static constexpr int maxfiles {1000};
    static const std::string srvname;

    SfdThreadFix() : srv_barr(2),
                thr([this] { run_server(); }) {

        srv_barr.wait();

        srv_fd = sfd_connect(SFD_SRV_SOCKDIR, srvname.c_str());
        if (!srv_fd) {
            perror("sfd_connect()");
            stop_thread();
            throw std::runtime_error("Couldn't connect to daemon");
        }
    }

    ~SfdThreadFix() {
        stop_thread();
        mock_read_reset();
        mock_write_reset();
        mock_sendfile_reset();
        mock_splice_reset();
    }

    void run_server() {
        const int listenfd {us_serve(SFD_SRV_SOCKDIR,
                                     srvname.c_str(),
                                     getuid(), getgid())};
        if (listenfd == -1) {
            perror("us_serve");
            throw std::runtime_error("Couldn't start server");
        }

        srv_barr.wait();

        srv_run(listenfd, maxfiles, OpenFileTimeoutMs);

        us_stop_serving(SFD_SRV_SOCKDIR, srvname.c_str(), listenfd);
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
constexpr long SfdThreadFix<OpenFileTimeoutMs>::open_file_timeout_ms;

template<long OpenFileTimeoutMs>
constexpr int SfdThreadFix<OpenFileTimeoutMs>::maxfiles;

template<long OpenFileTimeoutMs>
const std::string SfdThreadFix<OpenFileTimeoutMs>::srvname {"testing123_thread"};

struct SmallFile {
    static const std::string file_contents;

    SmallFile() : file(file_contents) {}

    test::TmpFile file;
};

const std::string SmallFile::file_contents {"1234567890"};

struct LargeFile {
    static constexpr int NCHUNKS {1024};
    static constexpr int CHUNK_SIZE {1024};
    static constexpr int FILE_SIZE {CHUNK_SIZE * NCHUNKS};

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
constexpr int LargeFile::FILE_SIZE;
std::vector<std::uint8_t> LargeFile::file_chunk(LargeFile::CHUNK_SIZE);

struct SfdProcSmallFileFix : public SfdProcFix, public SmallFile {};

struct SfdThreadSmallFileFix : public SfdThreadFix<1000>, public SmallFile {};

struct SfdThreadSmallFileShortOpenFileTimeoutFix :
        public SfdThreadFix<100>, public SmallFile {};

struct SfdProcLargeFileFix : public SfdProcFix, public LargeFile {
    static void SetUpTestCase() {
        LargeFile::SetUpTestCase();
        SfdProcFix::SetUpTestCase();
    }
};

struct SfdThreadLargeFileFix : public SfdThreadFix<1000>, public LargeFile {
    static void SetUpTestCase() {
        LargeFile::SetUpTestCase();
    }
};

} // namespace

// Non-existent file should respond to request with status message containing
// ENOENT.
TEST_F(SfdProcSmallFileFix, file_not_found)
{
    int data_pipe[2];

    ASSERT_NE(-1, pipe(data_pipe));

    const test::unique_fd data_fd {data_pipe[0]};

    const test::unique_fd stat_fd {sfd_send(srv_fd,
                                             (file.name() + "XXXXXXXXX").c_str(),
                                             data_pipe[1],
                                             0, 0, false)};
    close(data_pipe[1]);

    ASSERT_TRUE(stat_fd);

    uint8_t buf [PROT_REQ_MAXSIZE];
    struct sfd_xfer_stat ack;

    // Request ACK
    const ssize_t nread {read(stat_fd, buf, sizeof(ack))};
    ASSERT_EQ(sizeof(struct prot_hdr), nread);
    EXPECT_EQ(SFD_FILE_INFO, sfd_get_cmd(buf));
    EXPECT_EQ(ENOENT, sfd_get_stat(buf));
    EXPECT_FALSE(sfd_unmarshal_xfer_stat(&ack, buf));
}

// This test should fail for any file which is not a regular file.
TEST_F(SfdProcFix, open_directory_fails)
{
    int data_pipe[2];

    if (pipe(data_pipe) == -1)
        FAIL() << "Couldn't create a pipe; errno: " << strerror(errno);

    const test::unique_fd data_fd {data_pipe[0]};

    const test::unique_fd stat_fd {sfd_send(srv_fd,
                                             "/",
                                             data_pipe[1],
                                             0, 0, false)};
    close(data_pipe[1]);

    ASSERT_TRUE(stat_fd);

    uint8_t buf [PROT_REQ_MAXSIZE];
    struct sfd_xfer_stat ack;

    // Request ACK
    const ssize_t nread {read(stat_fd, buf, sizeof(ack))};
    ASSERT_EQ(sizeof(struct prot_hdr), nread);

    EXPECT_EQ(SFD_FILE_INFO, sfd_get_cmd(buf));
    EXPECT_EQ(EINVAL, sfd_get_stat(buf));
}

using SfdProcFix2Xfers = SfdProcFixTemplate<2>;
/**
 * Checks that the client receives EMFILE if there are too many concurrent
 * transfers on the server (-n command-line parameter sets the limit).
 */
TEST_F(SfdProcFix2Xfers, transfer_table_full)
{
    test::unique_fd stat_fds [maxfiles];
    test::TmpFile file {"1234567890"};

    for (std::size_t i = 0; i < maxfiles; i++) {
        stat_fds[i] = sfd_open(srv_fd,
                               file.name().c_str(),
                               0, 0, false);

        ASSERT_TRUE(stat_fds[i]);

        uint8_t buf [PROT_REQ_MAXSIZE];
        struct sfd_file_info finfo;

        const ssize_t nread {read(stat_fds[i], buf, sizeof(finfo))};
        EXPECT_EQ(sizeof(struct sfd_file_info), nread);

        EXPECT_EQ(SFD_FILE_INFO, sfd_get_cmd(buf));
        EXPECT_EQ(SFD_STAT_OK, sfd_get_stat(buf));
    }

    // One over the 'open file limit'

    auto sockets = test::make_connection(test_port + maxfiles);

    const test::unique_fd stat_fd {sfd_open(srv_fd,
                                            file.name().c_str(),
                                            0, 0, false)};

    ASSERT_TRUE(stat_fd);

    uint8_t buf [PROT_REQ_MAXSIZE] {};
    struct prot_hdr hdr {};

    const ssize_t nread {read(stat_fd, buf, sizeof(hdr))};
    EXPECT_EQ(sizeof(struct prot_hdr), nread);

    EXPECT_EQ(SFD_FILE_INFO, sfd_get_cmd(buf));
    EXPECT_EQ(EMFILE, sfd_get_stat(buf));
}

TEST_F(SfdProcSmallFileFix, send)
{
    auto sockets = test::make_connection(test_port);

    const test::unique_fd stat_fd {sfd_send(srv_fd,
                                            file.name().c_str(),
                                            sockets.first,
                                            0, 0, false)};

    ASSERT_TRUE(stat_fd);

    sockets.first.reset();

    uint8_t buf [PROT_REQ_MAXSIZE];
    ssize_t nread;
    struct sfd_file_info ack;
    struct sfd_xfer_stat xfer_stat;

    // Request ACK
    nread = read(stat_fd, buf, sizeof(ack));
    ASSERT_EQ(sizeof(ack), nread);
    ASSERT_TRUE(sfd_unmarshal_file_info(&ack, buf));
    EXPECT_EQ(SFD_FILE_INFO, ack.cmd);
    EXPECT_EQ(SFD_STAT_OK, ack.stat);
    EXPECT_EQ(file_contents.size(), ack.size);

    // Transfer status update
    nread = read(stat_fd, buf, sizeof(xfer_stat));
    ASSERT_EQ(sizeof(xfer_stat), nread);
    ASSERT_TRUE(sfd_unmarshal_xfer_stat(&xfer_stat, buf));
    EXPECT_EQ(SFD_XFER_STAT, xfer_stat.cmd);
    EXPECT_EQ(SFD_STAT_OK, xfer_stat.stat);
    EXPECT_EQ(PROT_XFER_COMPLETE, xfer_stat.size);

    // File content
    nread = read(sockets.second, buf, sizeof(buf));
    ASSERT_EQ(file_contents.size(), nread);
    const std::string recvd_file(reinterpret_cast<const char*>(buf),
                                 file_contents.size());
    EXPECT_EQ(file_contents, recvd_file);
}

TEST_F(SfdThreadSmallFileFix, read)
{
    const test::unique_fd data_fd {sfd_read(srv_fd,
                                            file.name().c_str(),
                                            0, 0, false)};
    ASSERT_TRUE(data_fd);

    uint8_t buf [PROT_REQ_MAXSIZE];
    ssize_t nread;
    struct sfd_file_info ack;

    // Request ACK
    nread = read(data_fd, buf, sizeof(ack));
    ASSERT_EQ(sizeof(ack), nread);
    ASSERT_TRUE(sfd_unmarshal_file_info(&ack, buf));
    EXPECT_EQ(SFD_FILE_INFO, ack.cmd);
    EXPECT_EQ(SFD_STAT_OK, ack.stat);
    EXPECT_EQ(file_contents.size(), ack.size);

    // File content
    nread = read(data_fd, buf, sizeof(buf));
    ASSERT_EQ(file_contents.size(), nread);
    const std::string recvd_file(reinterpret_cast<const char*>(buf),
                                 file_contents.size());
    EXPECT_EQ(file_contents, recvd_file);
}

TEST_F(SfdThreadSmallFileFix, send)
{
    auto sockets = test::make_connection(test_port);

    const test::unique_fd stat_fd {sfd_send(srv_fd,
                                            file.name().c_str(),
                                            sockets.first,
                                            0, 0, false)};

    ASSERT_TRUE(stat_fd);

    sockets.first.reset();

    uint8_t buf [PROT_REQ_MAXSIZE];
    ssize_t nread;
    struct sfd_file_info ack;
    struct sfd_xfer_stat xfer_stat;

    // Request ACK
    nread = read(stat_fd, buf, sizeof(ack));
    ASSERT_EQ(sizeof(ack), nread);
    ASSERT_TRUE(sfd_unmarshal_file_info(&ack, buf));
    EXPECT_EQ(SFD_FILE_INFO, ack.cmd);
    EXPECT_EQ(SFD_STAT_OK, ack.stat);
    EXPECT_EQ(file_contents.size(), ack.size);

    // Transfer status update
    nread = read(stat_fd, buf, sizeof(xfer_stat));
    ASSERT_EQ(sizeof(xfer_stat), nread);
    ASSERT_TRUE(sfd_unmarshal_xfer_stat(&xfer_stat, buf));
    EXPECT_EQ(SFD_XFER_STAT, xfer_stat.cmd);
    EXPECT_EQ(SFD_STAT_OK, xfer_stat.stat);
    EXPECT_EQ(PROT_XFER_COMPLETE, xfer_stat.size);

    // File content
    nread = read(sockets.second, buf, sizeof(buf));
    ASSERT_EQ(file_contents.size(), nread);
    const std::string recvd_file(reinterpret_cast<const char*>(buf),
                                 file_contents.size());
    EXPECT_EQ(file_contents, recvd_file);
}

/// Causes the final (and only, in this case) transfer status send to fail
/// temporarily. The server should keep trying until it is able to send the
/// final status.
TEST_F(SfdThreadSmallFileFix, send_final_xfer_status_fails)
{
    auto sockets = test::make_connection(test_port);

    // Due to edge-trigged mode, more than one EWOULDBLOCK will cause
    // deadlock. The lone one only works because when the final xfer status send
    // fails with EWOULDBLOCK, it de-registers the dest fd and registers the
    // stat fd, causing its events to be re-read.
    // const std::vector<ssize_t> retvals {MOCK_REALRV, -EWOULDBLOCK};
    // mock_write_set_retval_n(retvals.data(), (int)retvals.size());

    const test::unique_fd stat_fd {sfd_send(srv_fd,
                                            file.name().c_str(),
                                            sockets.first,
                                            0, 0, false)};

    ASSERT_TRUE(stat_fd);

    sockets.first.reset();

    uint8_t buf [PROT_REQ_MAXSIZE];
    ssize_t nread;
    struct sfd_file_info ack;
    struct sfd_xfer_stat xfer_stat;

    // Request ACK
    nread = read(stat_fd, buf, sizeof(ack));
    ASSERT_EQ(sizeof(ack), nread);
    ASSERT_TRUE(sfd_unmarshal_file_info(&ack, buf));
    EXPECT_EQ(SFD_FILE_INFO, ack.cmd);
    EXPECT_EQ(SFD_STAT_OK, ack.stat);
    EXPECT_EQ(file_contents.size(), ack.size);

    // Transfer status update
    nread = read(stat_fd, buf, sizeof(xfer_stat));
    ASSERT_EQ(sizeof(xfer_stat), nread);
    ASSERT_TRUE(sfd_unmarshal_xfer_stat(&xfer_stat, buf));
    EXPECT_EQ(SFD_XFER_STAT, xfer_stat.cmd);
    EXPECT_EQ(SFD_STAT_OK, xfer_stat.stat);
    EXPECT_EQ(PROT_XFER_COMPLETE, xfer_stat.size);

    // File content
    nread = read(sockets.second, buf, sizeof(buf));
    ASSERT_EQ(file_contents.size(), nread);
    const std::string recvd_file(reinterpret_cast<const char*>(buf),
                                 file_contents.size());
    EXPECT_EQ(file_contents, recvd_file);
}

TEST_F(SfdThreadSmallFileFix, send_error_send_fails)
{
    auto sockets = test::make_connection(test_port);

    // Due to edge-trigged mode, more than one EWOULDBLOCK will cause
    // deadlock. The lone one only works because when the final xfer status send
    // fails with EWOULDBLOCK, it de-registers the dest fd and registers the
    // stat fd, causing its events to be re-read.
    const std::vector<ssize_t> write_retvals {MOCK_REALRV, -EWOULDBLOCK};
    mock_write_set_retval_n(write_retvals.data(), (int)write_retvals.size());

    mock_sendfile_set_retval(-EIO);

    const test::unique_fd stat_fd {sfd_send(srv_fd,
                                            file.name().c_str(),
                                            sockets.first,
                                            0, 0, false)};

    ASSERT_TRUE(stat_fd);

    sockets.first.reset();

    uint8_t buf [PROT_REQ_MAXSIZE];
    ssize_t nread;
    struct sfd_file_info ack;
    struct prot_hdr err;

    // Request ACK
    nread = read(stat_fd, buf, sizeof(ack));
    ASSERT_EQ(sizeof(ack), nread);
    ASSERT_TRUE(sfd_unmarshal_file_info(&ack, buf));
    EXPECT_EQ(SFD_FILE_INFO, ack.cmd);
    EXPECT_EQ(SFD_STAT_OK, ack.stat);
    EXPECT_EQ(file_contents.size(), ack.size);

    // Error code
    nread = read(stat_fd, buf, sizeof(err));
    ASSERT_EQ(sizeof(err), nread);
    memcpy(&err, buf, sizeof(err));
    EXPECT_EQ(SFD_XFER_STAT, err.cmd);
    EXPECT_EQ(EIO, err.stat);

    // Not checking data channel because server is under the impression that
    // fatal error has occured and therefore will not have bothered to close the
    // data fd.
}

TEST_F(SfdProcSmallFileFix, read)
{
    const test::unique_fd data_fd {sfd_read(srv_fd,
                                             file.name().c_str(),
                                             0, 0, false)};
    ASSERT_TRUE(data_fd);

    uint8_t buf [PROT_REQ_MAXSIZE];
    ssize_t nread;
    struct sfd_file_info ack;

    // Request ACK
    nread = read(data_fd, buf, sizeof(ack));
    ASSERT_EQ(sizeof(ack), nread);
    ASSERT_TRUE(sfd_unmarshal_file_info(&ack, buf));
    EXPECT_EQ(SFD_FILE_INFO, ack.cmd);
    EXPECT_EQ(SFD_STAT_OK, ack.stat);
    EXPECT_EQ(file_contents.size(), ack.size);

    // File content
    nread = read(data_fd, buf, sizeof(buf));
    ASSERT_EQ(file_contents.size(), nread);
    const std::string recvd_file(reinterpret_cast<const char*>(buf),
                                 file_contents.size());
    EXPECT_EQ(file_contents, recvd_file);
}

TEST_F(SfdProcSmallFileFix, send_open_file)
{
    auto sockets = test::make_connection(test_port);

    // Open the file
    const test::unique_fd stat_fd {sfd_open(srv_fd,
                                             file.name().c_str(),
                                             0, 0, false)};
    ASSERT_TRUE(stat_fd);

    ssize_t nread;
    struct sfd_file_info ack;
    uint8_t buf [sizeof(ack)];

    // Receive 'ACK' with file stats
    nread = read(stat_fd, buf, sizeof(ack));
    ASSERT_EQ(sizeof(ack), nread);
    ASSERT_TRUE(sfd_unmarshal_file_info(&ack, buf));
    EXPECT_EQ(SFD_FILE_INFO, ack.cmd);
    EXPECT_EQ(SFD_STAT_OK, ack.stat);
    EXPECT_EQ(file_contents.size(), ack.size);
    EXPECT_GT(ack.txnid, 0);

    // Send 'open file'
    ASSERT_TRUE(sfd_send_open(srv_fd, ack.txnid, sockets.first));
    sockets.first.reset();

    // Read file data
    nread = read(sockets.second, buf, sizeof(buf));
    ASSERT_EQ(file_contents.size(), nread);
    const std::string recvd_file(reinterpret_cast<const char*>(buf),
                                 static_cast<std::size_t>(nread));
    EXPECT_EQ(file_contents, recvd_file);

    // Read transfer status
    struct sfd_xfer_stat xstat;
    nread = read(stat_fd, buf, sizeof(buf));
    ASSERT_EQ(sizeof(xstat), nread);
    ASSERT_TRUE(sfd_unmarshal_xfer_stat(&xstat, buf));
    EXPECT_EQ(PROT_XFER_COMPLETE, xstat.size);
}

TEST_F(SfdThreadSmallFileFix, cancel_open_file)
{
    // Open the file
    const test::unique_fd stat_fd {sfd_open(srv_fd,
                                             file.name().c_str(),
                                             0, 0, false)};
    ASSERT_TRUE(stat_fd);

    ssize_t nread;
    struct sfd_file_info ack;
    uint8_t buf [sizeof(ack)];

    // Receive 'ACK' with file stats
    nread = read(stat_fd, buf, sizeof(ack));
    ASSERT_EQ(sizeof(ack), nread);
    ASSERT_TRUE(sfd_unmarshal_file_info(&ack, buf));
    EXPECT_EQ(SFD_FILE_INFO, ack.cmd);
    EXPECT_EQ(SFD_STAT_OK, ack.stat);
    EXPECT_EQ(file_contents.size(), ack.size);
    EXPECT_GT(ack.txnid, 0);

    // Close the file
    EXPECT_TRUE(sfd_cancel(srv_fd, ack.txnid));

    EXPECT_EQ(0, read(stat_fd, buf, sizeof(buf)));
}

TEST_F(SfdThreadLargeFileFix, cancel_read)
{
    // Initiate the read
    const test::unique_fd stat_fd {sfd_read(srv_fd,
                                            file.name().c_str(),
                                            0, 0, false)};
    ASSERT_TRUE(stat_fd);

    ssize_t nread;
    struct sfd_file_info ack;
    std::vector<char> buf(1024);

    // Receive 'ACK' with file stats
    nread = read(stat_fd, buf.data(), sizeof(ack));
    ASSERT_EQ(sizeof(ack), nread);
    ASSERT_TRUE(sfd_unmarshal_file_info(&ack, buf.data()));
    EXPECT_EQ(SFD_FILE_INFO, ack.cmd);
    EXPECT_EQ(SFD_STAT_OK, ack.stat);
    EXPECT_EQ(FILE_SIZE, ack.size);
    EXPECT_GT(ack.txnid, 0);

    // Read a bit of the file in order to confirm that the server has started
    // sending it.
    int b;
    ASSERT_EQ(1, read(stat_fd, &b, 1));

    // Cancel the transfer
    EXPECT_TRUE(sfd_cancel(srv_fd, ack.txnid));

    // Server should only have been able to write the part of the file which
    // could fit into a pipe.

    nread = 0;
    bool got_eof {false};
    for (;;) {
        const ssize_t n = read(stat_fd, buf.data(), buf.size());

        if (n == 0) {
            got_eof = true;
            break;
        }

        ASSERT_GT(n, 0);
        nread += n;
    }

    EXPECT_TRUE(got_eof);
    EXPECT_LT(nread, FILE_SIZE);
}

TEST_F(SfdThreadLargeFileFix, cancel_send)
{
    auto sockets = test::make_connection(test_port);

    // Initiate the read
    const test::unique_fd stat_fd {sfd_send(srv_fd,
                                            file.name().c_str(),
                                            sockets.first,
                                            0, 0, false)};

    ASSERT_TRUE(stat_fd);

    sockets.first.reset();

    ssize_t nread;
    struct sfd_file_info ack;
    std::vector<char> buf(1024);

    // Receive 'ACK' with file stats
    nread = read(stat_fd, buf.data(), sizeof(ack));
    ASSERT_EQ(sizeof(ack), nread);
    ASSERT_TRUE(sfd_unmarshal_file_info(&ack, buf.data()));
    EXPECT_EQ(SFD_FILE_INFO, ack.cmd);
    EXPECT_EQ(SFD_STAT_OK, ack.stat);
    EXPECT_EQ(FILE_SIZE, ack.size);
    EXPECT_GT(ack.txnid, 0);

    // Read a bit of the file in order to confirm that the server has started
    // sending it.
    int b;
    ASSERT_EQ(1, read(sockets.second, &b, 1));

    // Cancel the transfer
    EXPECT_TRUE(sfd_cancel(srv_fd, ack.txnid));

    // Server should only have been able to write the part of the file which
    // could fit into a pipe.

    nread = 0;
    bool got_eof {false};
    for (;;) {
        const ssize_t n = read(sockets.second, buf.data(), buf.size());

        if (n == 0) {
            got_eof = true;
            break;
        }

        ASSERT_GT(n, 0);
        nread += n;
    }

    EXPECT_TRUE(got_eof);
    EXPECT_LT(nread, FILE_SIZE);
}

/**
 * Opens a file, waits too long, then requests server to send the opened
 * file. By this time the timer should've expired and the open file closed,
 * resulting in an error message from the server.
 */
TEST_F(SfdThreadSmallFileShortOpenFileTimeoutFix, open_file_timeout)
{
    // Open the file
    const test::unique_fd stat_fd {sfd_open(srv_fd,
                                             file.name().c_str(),
                                             0, 0, false)};
    ASSERT_TRUE(stat_fd);

    struct sfd_file_info ack;
    uint8_t buf [sizeof(ack)];
    ssize_t nread;

    // Confirm that the file was opened
    nread = read(stat_fd, buf, sizeof(ack));
    ASSERT_EQ(sizeof(ack), nread);
    ASSERT_EQ(SFD_FILE_INFO, sfd_get_cmd(buf));
    ASSERT_EQ(SFD_STAT_OK, sfd_get_stat(buf));
    ASSERT_TRUE(sfd_unmarshal_file_info(&ack, buf));
    ASSERT_EQ(file_contents.size(), ack.size);
    ASSERT_GT(ack.txnid, 0);

    // Sleep for longer than the server's open file timeout
    std::this_thread::sleep_for(std::chrono::milliseconds{open_file_timeout_ms});

    auto sockets = test::make_connection(test_port);

    // Send 'open file'
    ASSERT_TRUE(sfd_send_open(srv_fd, ack.txnid, sockets.first));
    sockets.first.reset();

    // Server should've written the timeout error message to the status
    // channel...
    nread = read(stat_fd, buf, sizeof(buf));

    if (nread == sizeof(struct prot_hdr)) {
        EXPECT_EQ(SFD_XFER_STAT, sfd_get_cmd(buf));
        EXPECT_EQ(ETIMEDOUT, sfd_get_stat(buf));
        // ... and then should've closed the status channel.
        EXPECT_EQ(0, read(stat_fd, buf, sizeof(buf)));

        // Data channel should have been closed at timeout, before any data had
        // been written to it.
        nread = read(sockets.second, buf, sizeof(buf));
        EXPECT_EQ(0, nread);

    } else {
        // Timeout occurred after commencement of the transfer
        EXPECT_EQ(sizeof(struct sfd_xfer_stat), nread);
        EXPECT_EQ(SFD_XFER_STAT, sfd_get_cmd(buf));
        EXPECT_EQ(SFD_STAT_OK, sfd_get_stat(buf));
        struct sfd_xfer_stat pdu;
        ASSERT_TRUE(sfd_unmarshal_xfer_stat(&pdu, buf));
        EXPECT_GT(pdu.size, 0);
        // No point in carrying on at this point
    }
}

TEST_F(SfdProcSmallFileFix, read_range)
{
    constexpr off_t offset {3};
    constexpr size_t len {5};

    const test::unique_fd data_fd {sfd_read(srv_fd,
                                             file.name().c_str(),
                                             offset, len, false)};
    ASSERT_TRUE(data_fd);

    uint8_t buf [PROT_REQ_MAXSIZE];
    ssize_t nread;
    struct sfd_file_info ack;

    // Request ACK
    nread = read(data_fd, buf, sizeof(ack));
    ASSERT_EQ(sizeof(ack), nread);
    ASSERT_TRUE(sfd_unmarshal_file_info(&ack, buf));
    EXPECT_EQ(SFD_FILE_INFO, ack.cmd);
    EXPECT_EQ(SFD_STAT_OK, ack.stat);
    EXPECT_EQ(len, ack.size);

    // File content
    nread = read(data_fd, buf, sizeof(buf));
    ASSERT_EQ(len, nread);
    const std::string recvd_file(reinterpret_cast<const char*>(buf), len);
    EXPECT_EQ(0, memcmp(buf, file_contents.c_str() + offset, len));
}

TEST_F(SfdProcLargeFileFix, multiple_reading_clients)
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
        cli.data_fd = sfd_read(srv_fd, file.name().c_str(), 0, 0, false);
        ASSERT_TRUE(cli.data_fd);

        uint8_t buf [PROT_REQ_MAXSIZE];
        ssize_t nread;
        struct sfd_file_info ack;

        // Request ACK
        nread = read(cli.data_fd, buf, sizeof(ack));
        ASSERT_EQ(sizeof(ack), nread);
        ASSERT_TRUE(sfd_unmarshal_file_info(&ack, buf));
        EXPECT_EQ(SFD_FILE_INFO, ack.cmd);
        EXPECT_EQ(SFD_STAT_OK, ack.stat);
        EXPECT_EQ(CHUNK_SIZE * NCHUNKS, ack.size);

        if (!set_nonblock(cli.data_fd, true))
            FAIL() << "Couldn't make data fd non-blocking";
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

            if (n == -1) {
                if (errno != EWOULDBLOCK && errno != EAGAIN)
                    FAIL() << "Fatal read error on data fd";

            } else {
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
    }

    for (int i = 0; i < nclients; i++)
        EXPECT_EQ(NCHUNKS, clients[i].nchunks);
}

TEST_F(SfdProcFix, multiple_clients_reading_different_large_files)
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

        cli.data_fd = sfd_read(srv_fd, cli.file.name().c_str(), 0, 0, false);
        ASSERT_TRUE(cli.data_fd);

        uint8_t buf [PROT_REQ_MAXSIZE];
        ssize_t nread;
        struct sfd_file_info ack;

        // Request ACK
        nread = read(cli.data_fd, buf, sizeof(ack));
        ASSERT_EQ(sizeof(ack), nread);
        ASSERT_TRUE(sfd_unmarshal_file_info(&ack, buf));
        EXPECT_EQ(SFD_FILE_INFO, ack.cmd);
        EXPECT_EQ(SFD_STAT_OK, ack.stat);
        EXPECT_EQ(CHUNK_SIZE * NCHUNKS, ack.size);

        if (!set_nonblock(cli.data_fd, true))
            FAIL() << "Couldn't make data fd non-blocking";
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

            if (n == -1) {
                if (errno != EWOULDBLOCK && errno != EAGAIN) {
                    FAIL() << "Fatal read error on data fd";
                } else {
                    continue;
                }
            }

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

TEST_F(SfdThreadLargeFileFix, read_io_error)
{
    const test::unique_fd data_fd {
        sfd_read(srv_fd, file.name().c_str(), 0, 0, false)};
    ASSERT_TRUE(data_fd);

    uint8_t buf [PROT_REQ_MAXSIZE];
    ssize_t nread;
    struct sfd_file_info ack;

    // Request ACK
    nread = read(data_fd, buf, sizeof(ack));
    ASSERT_EQ(sizeof(ack), nread);
    ASSERT_TRUE(sfd_unmarshal_file_info(&ack, buf));
    EXPECT_EQ(SFD_FILE_INFO, ack.cmd);
    EXPECT_EQ(SFD_STAT_OK, ack.stat);
    EXPECT_EQ(CHUNK_SIZE * NCHUNKS, ack.size);

    // File content
    std::vector<std::uint8_t> recvbuf(CHUNK_SIZE);
    size_t nchunks {};
    size_t nread_chunk {};
    bool got_eof {false};

    for (;;) {
        if (nchunks > NCHUNKS / 2) {
#ifdef __linux__
            mock_splice_set_retval(-EIO); // Linux
#else
            mock_read_set_retval_except_fd(-EIO, data_fd);   // Other
#endif
        }

        ssize_t n = read(data_fd,
                         recvbuf.data() + nread_chunk,
                         CHUNK_SIZE - nread_chunk);

        if (n == 0) {
            got_eof = true;
            break;

        } else if (n > 0) {
            nread_chunk += (size_t)n;

            if (nread_chunk == CHUNK_SIZE) {
                nread_chunk = 0;
                nchunks++;
            }

            if (nchunks == NCHUNKS)
                break;
        }
    }

    EXPECT_TRUE(got_eof);
    EXPECT_LT(nchunks, NCHUNKS);
}

TEST_F(SfdThreadLargeFileFix, send_io_error)
{
    auto sockets = test::make_connection(test_port);

    const test::unique_fd stat_fd {sfd_send(srv_fd,
                                            file.name().c_str(),
                                            sockets.first,
                                            0, 0, false)};

    ASSERT_TRUE(stat_fd);

    sockets.first.reset();

    // File info (first response to send request)
    {
        std::vector<uint8_t> buf(SFD_MAX_RESP_SIZE);
        struct sfd_file_info file_info;

        const ssize_t nread {read(stat_fd, buf.data(), sizeof(file_info))};

        ASSERT_EQ(sizeof(file_info), nread);
        ASSERT_TRUE(sfd_unmarshal_file_info(&file_info, buf.data()));
        EXPECT_EQ(SFD_FILE_INFO, file_info.cmd);
        EXPECT_EQ(SFD_STAT_OK, file_info.stat);
        EXPECT_EQ(CHUNK_SIZE * NCHUNKS, file_info.size);
    }

    bool got_stat_eof {false};
    bool got_errno {false};

    auto read_stat_channel = [&stat_fd, &got_stat_eof, &got_errno] {
        std::vector<uint8_t> buf(SFD_MAX_RESP_SIZE);
        struct sfd_xfer_stat stat;

        for (;;) {
            const auto nread = read(stat_fd, buf.data(), sizeof(stat));

            if (nread == -1) {
                if (errno != EWOULDBLOCK && errno != EAGAIN)
                    return;
            } else if (nread > 0) {
                if (static_cast<std::size_t>(nread) < sizeof(struct prot_hdr) ||
                    sfd_get_cmd(buf.data()) != SFD_XFER_STAT) {
                    return;
                }
                got_errno = (sfd_get_stat(buf.data()) == EIO);
            } else {
                got_stat_eof = (nread == 0);
                return;
            }
        }
    };

    // Read from the status channel in another thread
    auto stat_future = std::async(std::launch::async, read_stat_channel);

    std::vector<uint8_t> data_buf(CHUNK_SIZE);
    size_t nchunks {};
    ssize_t nread_chunk {};
    bool got_data_eof {false};

    // Read one byte of file data to ensure that the server has sent some data,
    // then make the next call to sendfile fail.  Needs to be done as early as
    // possible otherwise the server may have finished sending by the time
    // sendfile is affected.
    {
        int byte {};
        ASSERT_EQ(1, read(sockets.second, &byte, 1));
        mock_sendfile_set_retval(-EIO);
    }

    // Read from the data channel in this thread
    for (;;) {
        const ssize_t nread {read(sockets.second,
                                  data_buf.data() + nread_chunk,
                                  CHUNK_SIZE - (size_t)nread_chunk)};

        ASSERT_GE(nread, 0);

        if (nread == 0) {
            got_data_eof = true;
            break;
        } else {
            nread_chunk += nread;

            if (nread_chunk >= CHUNK_SIZE) {
                if (++nchunks == NCHUNKS)
                    break;
                nread_chunk = 0;
            }
        }
    }

    stat_future.get();

    EXPECT_TRUE(got_data_eof);
    EXPECT_TRUE(got_stat_eof);
    EXPECT_TRUE(got_errno);
    EXPECT_LT(nchunks, NCHUNKS);
}

#pragma GCC diagnostic pop
