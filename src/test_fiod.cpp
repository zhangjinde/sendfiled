#include <unistd.h>

#include <cerrno>
#include <cstdint>
#include <numeric>
#include <vector>

#include <gtest/gtest.h>

#include "impl/test_utils.hpp"

#include "fiod.h"
#include "impl/protocol_client.h"
#include "impl/server.h"
#include "impl/test_interpose.h"
#include "impl/unix_sockets.h"
#include "impl/util.h"

#pragma GCC diagnostic push
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
        srv_pid = fiod_spawn(srvname.c_str(), "/tmp", 1000, 1000);
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

    FiodProcFix() : srv_fd(fiod_connect(srvname.c_str())) {
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
struct FiodThreadFix : public ::testing::Test {
    static constexpr int maxfiles {1000};
    static const std::string srvname;

    FiodThreadFix() : srv_barr(2),
                thr([this] { run_server(); }) {

        srv_barr.wait();

        srv_fd = fiod_connect(srvname.c_str());
        if (!srv_fd) {
            stop_thread();
            throw std::runtime_error("Couldn't connect to daemon");
        }
    }

    ~FiodThreadFix() {
        stop_thread();
        mock_sendfile_reset();
        mock_splice_reset();
    }

    void run_server() {
        const std::string path {"/tmp/" + srvname};

        const int listenfd {us_serve(path.c_str())};
        if (listenfd == -1) {
            perror("us_serve");
            throw std::runtime_error("Couldn't start server");
        }

        srv_barr.wait();

        srv_run(listenfd, maxfiles, 3000);

        us_stop_serving(path.c_str(), listenfd);
    }

    void stop_thread() {
        test::kill_thread(thr, SIGTERM);
        thr.join();
    }

    test::unique_fd srv_fd;
    test::thread_barrier srv_barr;
    std::thread thr;
};

const int FiodThreadFix::maxfiles;
const std::string FiodThreadFix::srvname {"testing123_thread"};

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

struct FiodThreadSmallFileFix : public FiodThreadFix, public SmallFile {};

struct FiodProcLargeFileFix : public FiodProcFix, public LargeFile {
    static void SetUpTestCase() {
        LargeFile::SetUpTestCase();
        FiodProcFix::SetUpTestCase();
    }
};

struct FiodThreadLargeFileFix : public FiodThreadFix, public LargeFile {
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

    uint8_t buf [PROT_PDU_MAXSIZE];
    struct prot_xfer_stat ack;

    // Request ACK
    const ssize_t nread {read(stat_fd, buf, PROT_XFER_STAT_SIZE)};
    ASSERT_EQ(nread, PROT_HDR_SIZE);
    ASSERT_EQ(ENOENT, prot_unmarshal_xfer_stat(&ack, buf));
    EXPECT_EQ(PROT_CMD_XFER_STAT, ack.cmd);
    EXPECT_EQ(ENOENT, ack.stat);
    EXPECT_EQ(0, ack.body_len);
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

    uint8_t buf [PROT_PDU_MAXSIZE];
    ssize_t nread;
    struct prot_file_info ack;
    struct prot_xfer_stat xfer_stat;

    // Request ACK
    nread = read(stat_fd, buf, PROT_FILE_INFO_SIZE);
    ASSERT_EQ(PROT_FILE_INFO_SIZE, nread);
    ASSERT_EQ(0, prot_unmarshal_file_info(&ack, buf));
    EXPECT_EQ(PROT_CMD_FILE_INFO, ack.cmd);
    EXPECT_EQ(PROT_STAT_OK, ack.stat);
    EXPECT_EQ(PROT_FILE_INFO_BODY_LEN, ack.body_len);
    EXPECT_EQ(file_contents.size(), ack.size);

    // Transfer status update
    nread = read(stat_fd, buf, PROT_XFER_STAT_SIZE);
    ASSERT_EQ(PROT_XFER_STAT_SIZE, nread);
    ASSERT_EQ(0, prot_unmarshal_xfer_stat(&xfer_stat, buf));
    EXPECT_EQ(PROT_CMD_XFER_STAT, xfer_stat.cmd);
    EXPECT_EQ(PROT_STAT_OK, xfer_stat.stat);
    EXPECT_GT(xfer_stat.size, 0);
    EXPECT_LE(xfer_stat.size, file_contents.size());

    // File content
    nread = read(data_fd, buf, sizeof(buf));
    ASSERT_EQ(file_contents.size(), nread);
    const std::string recvd_file(reinterpret_cast<const char*>(buf),
                                 file_contents.size());
    EXPECT_EQ(file_contents, recvd_file);
}

TEST_F(FiodProcSmallFileFix, read)
{
    const test::unique_fd data_fd {fiod_read(srv_fd,
                                             file.name().c_str(),
                                             0, 0, false)};
    ASSERT_TRUE(data_fd);

    uint8_t buf [PROT_PDU_MAXSIZE];
    ssize_t nread;
    struct prot_file_info ack;

    // Request ACK
    nread = read(data_fd, buf, PROT_FILE_INFO_SIZE);
    ASSERT_EQ(PROT_FILE_INFO_SIZE, nread);
    ASSERT_EQ(0, prot_unmarshal_file_info(&ack, buf));
    EXPECT_EQ(PROT_CMD_FILE_INFO, ack.cmd);
    EXPECT_EQ(PROT_STAT_OK, ack.stat);
    EXPECT_EQ(PROT_FILE_INFO_BODY_LEN, ack.body_len);
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
    const test::unique_fd data_fd {fiod_open(srv_fd,
                                             file.name().c_str(),
                                             0, 0, false)};
    ASSERT_TRUE(data_fd);

    uint8_t buf [64];
    ssize_t nread;
    struct prot_open_file_info ack;

    // Request ACK
    nread = read(data_fd, buf, PROT_OPEN_FILE_INFO_SIZE);
    ASSERT_EQ(PROT_OPEN_FILE_INFO_SIZE, nread);
    ASSERT_EQ(0, prot_unmarshal_open_file_info(&ack, buf));
    EXPECT_EQ(PROT_CMD_OPEN_FILE_INFO, ack.cmd);
    EXPECT_EQ(PROT_STAT_OK, ack.stat);
    EXPECT_EQ(PROT_OPEN_FILE_INFO_BODY_LEN, ack.body_len);
    EXPECT_EQ(file_contents.size(), ack.size);
    EXPECT_GT(ack.xfer_id, 0);

    // Send 'open file'

    int pfd[2];
    ASSERT_NE(-1, pipe(pfd));
    const test::unique_fd pipe_read {pfd[0]};
    test::unique_fd pipe_write {pfd[1]};

    ASSERT_TRUE(fiod_send_open(srv_fd, ack.xfer_id, pipe_write));
    ::close(pipe_write);

    nread = read(pipe_read, buf, sizeof(buf));
    ASSERT_EQ(file_contents.size(), nread);
    const std::string recvd_file(reinterpret_cast<const char*>(buf),
                                 static_cast<std::size_t>(nread));
    EXPECT_EQ(file_contents, recvd_file);
}

TEST_F(FiodProcSmallFileFix, read_range)
{
    constexpr loff_t offset {3};
    constexpr size_t len {5};

    const test::unique_fd data_fd {fiod_read(srv_fd,
                                             file.name().c_str(),
                                             offset, len, false)};
    ASSERT_TRUE(data_fd);

    uint8_t buf [PROT_PDU_MAXSIZE];
    ssize_t nread;
    struct prot_file_info ack;

    // Request ACK
    nread = read(data_fd, buf, PROT_FILE_INFO_SIZE);
    ASSERT_EQ(PROT_FILE_INFO_SIZE, nread);
    ASSERT_EQ(0, prot_unmarshal_file_info(&ack, buf));
    EXPECT_EQ(PROT_CMD_FILE_INFO, ack.cmd);
    EXPECT_EQ(PROT_STAT_OK, ack.stat);
    EXPECT_EQ(PROT_FILE_INFO_BODY_LEN, ack.body_len);
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

        uint8_t buf [PROT_PDU_MAXSIZE];
        ssize_t nread;
        struct prot_file_info ack;

        // Request ACK
        nread = read(cli.data_fd, buf, PROT_FILE_INFO_SIZE);
        ASSERT_EQ(PROT_FILE_INFO_SIZE, nread);
        ASSERT_EQ(0, prot_unmarshal_file_info(&ack, buf));
        EXPECT_EQ(PROT_CMD_FILE_INFO, ack.cmd);
        EXPECT_EQ(PROT_STAT_OK, ack.stat);
        EXPECT_EQ(PROT_FILE_INFO_BODY_LEN, ack.body_len);
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

        uint8_t buf [PROT_PDU_MAXSIZE];
        ssize_t nread;
        struct prot_file_info ack;

        // Request ACK
        nread = read(cli.data_fd, buf, PROT_FILE_INFO_SIZE);
        ASSERT_EQ(PROT_FILE_INFO_SIZE, nread);
        ASSERT_EQ(0, prot_unmarshal_file_info(&ack, buf));
        EXPECT_EQ(PROT_CMD_FILE_INFO, ack.cmd);
        EXPECT_EQ(PROT_STAT_OK, ack.stat);
        EXPECT_EQ(PROT_FILE_INFO_BODY_LEN, ack.body_len);
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

    uint8_t buf [PROT_PDU_MAXSIZE];
    ssize_t nread;
    struct prot_file_info ack;

    // Request ACK
    nread = read(data_fd, buf, PROT_FILE_INFO_SIZE);
    ASSERT_EQ(PROT_FILE_INFO_SIZE, nread);
    ASSERT_EQ(0, prot_unmarshal_file_info(&ack, buf));
    EXPECT_EQ(PROT_CMD_FILE_INFO, ack.cmd);
    EXPECT_EQ(PROT_STAT_OK, ack.stat);
    EXPECT_EQ(PROT_FILE_INFO_BODY_LEN, ack.body_len);
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
    std::vector<uint8_t> stat_buf(PROT_FILE_INFO_SIZE);
    ssize_t nread;
    struct prot_file_info ack;
    struct prot_xfer_stat xfer_stat;

    // Request ACK
    while (wouldblock(nread = read(stat_fd, stat_buf.data(), PROT_FILE_INFO_SIZE))) {}

    ASSERT_EQ(PROT_FILE_INFO_SIZE, nread);
    ASSERT_EQ(0, prot_unmarshal_file_info(&ack, stat_buf.data()));
    EXPECT_EQ(PROT_CMD_FILE_INFO, ack.cmd);
    EXPECT_EQ(PROT_STAT_OK, ack.stat);
    EXPECT_EQ(PROT_FILE_INFO_BODY_LEN, ack.body_len);
    EXPECT_EQ(CHUNK_SIZE * NCHUNKS, ack.size);

    size_t nchunks {};
    ssize_t nread_chunk {};
    bool got_eof {false};
    bool got_errno {false};

    for (;;) {
        if (nchunks > NCHUNKS / 2)
            mock_splice_set_retval(-EIO);

        nread = read(data_fd,
                     data_buf.data() + nread_chunk,
                     CHUNK_SIZE - (size_t)nread_chunk);

        if (nread == 0) {
            got_eof = true;
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
        nread = read(stat_fd, stat_buf.data(), PROT_XFER_STAT_SIZE);
        if (wouldblock(nread))
            continue;
        ASSERT_GE(PROT_HDR_SIZE, nread);
        ASSERT_NE(-1, prot_unmarshal_xfer_stat(&xfer_stat, stat_buf.data()));
        ASSERT_EQ(PROT_CMD_XFER_STAT, xfer_stat.cmd);
        if (xfer_stat.stat != PROT_STAT_OK) {
            got_errno = (xfer_stat.stat == EIO);
            break;
        }
    }

    ASSERT_TRUE(set_nonblock(stat_fd, false));

    while (!got_eof) {
        nread = read(stat_fd, stat_buf.data(), PROT_XFER_STAT_SIZE);

        if (nread == 0) {
            got_eof = true;

        } else {
            ASSERT_GE(nread, PROT_HDR_SIZE);
            ASSERT_NE(-1, prot_unmarshal_xfer_stat(&xfer_stat, stat_buf.data()));
            ASSERT_EQ(PROT_CMD_XFER_STAT, xfer_stat.cmd);

            if (xfer_stat.stat != PROT_STAT_OK)
                got_errno = (xfer_stat.stat == EIO);
        }
    }

    EXPECT_TRUE(got_eof);
    EXPECT_TRUE(got_errno);
    EXPECT_LT(nchunks, NCHUNKS);
}

#pragma GCC diagnostic pop
