#include <unistd.h>

#include <numeric>
#include <vector>

#include <gtest/gtest.h>

#include "impl/test_utils.hpp"

#include "fiod.h"
#include "impl/protocol.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wexit-time-destructors"
#pragma GCC diagnostic ignored "-Wpadded"

namespace {
struct FiodFix : public ::testing::Test {
    static const std::string srvname;
    static pid_t srv_pid;
    static const std::string file_contents;

    static void SetUpTestCase() {
        srv_pid = fiod_spawn(srvname.c_str(), "/tmp", 10);
        if (srv_pid == -1)
            throw std::runtime_error("Couldn't start daemon");
    }

    static void TearDownTestCase() {
        if (srv_pid > 0) {
            const int status = fiod_shutdown(srv_pid);
            if (!WIFEXITED(status))
                std::fprintf(stderr, "Daemon has not exited\n");
            if (WEXITSTATUS(status) != EXIT_SUCCESS)
                std::fprintf(stderr, "Daemon did not shut down cleanly\n");
        }
    }

    FiodFix() : file(file_contents) {
        srv_fd = fiod_connect(srvname.c_str());
        if (srv_fd == -1)
            throw std::runtime_error("Couldn't connect to daemon");
    }

    ~FiodFix() {
        close(srv_fd);
    }

    int srv_fd;
    test::TmpFile file;
};

const std::string FiodFix::srvname {"testing123"};
pid_t FiodFix::srv_pid;
const std::string FiodFix::file_contents {"1234567890"};

#pragma GCC diagnostic pop

} // namespace

// Non-existent file should respond to request with status message containing
// ENOENT.
TEST_F(FiodFix, file_not_found)
{
    int data_pipe[2];

    ASSERT_NE(-1, pipe(data_pipe));

    const int stat_fd = fiod_send(srv_fd,
                                  (file.name() + "XXXXXXXXX").c_str(),
                                  data_pipe[1],
                                  0, 0);
    ASSERT_NE(-1, stat_fd);

    close(data_pipe[1]);

    uint8_t buf [PROT_PDU_MAXSIZE];
    struct prot_file_stat ack;

    // Request ACK
    const ssize_t nread {read(stat_fd, buf, PROT_STAT_SIZE)};
    ASSERT_EQ(nread, PROT_HDR_SIZE);
    ASSERT_EQ(ENOENT, prot_unmarshal_stat(&ack, buf));
    EXPECT_EQ(PROT_CMD_STAT, ack.cmd);
    EXPECT_EQ(ENOENT, ack.stat);
    EXPECT_EQ(0, ack.body_len);

    close(stat_fd);
    close(data_pipe[0]);
}

TEST_F(FiodFix, send)
{
    // Pipe to which file will be written
    int data_pipe[2];

    ASSERT_NE(-1, pipe(data_pipe));

    const int stat_fd = fiod_send(srv_fd,
                                  file.name().c_str(),
                                  data_pipe[1],
                                  0, 0);
    ASSERT_NE(-1, stat_fd);

    close(data_pipe[1]);

    uint8_t buf [PROT_PDU_MAXSIZE];
    ssize_t nread;
    struct prot_file_stat ack;
    struct prot_file_stat xfer_stat;

    // Request ACK
    nread = read(stat_fd, buf, PROT_STAT_SIZE);
    ASSERT_EQ(PROT_STAT_SIZE, nread);
    ASSERT_EQ(0, prot_unmarshal_stat(&ack, buf));
    EXPECT_EQ(PROT_CMD_STAT, ack.cmd);
    EXPECT_EQ(PROT_STAT_OK, ack.stat);
    EXPECT_EQ(8, ack.body_len);
    EXPECT_EQ(file_contents.size(), ack.size);

    // Transfer status update
    nread = read(stat_fd, buf, PROT_STAT_SIZE);
    ASSERT_EQ(PROT_STAT_SIZE, nread);
    ASSERT_EQ(0, prot_unmarshal_stat(&xfer_stat, buf));
    EXPECT_EQ(PROT_CMD_STAT, xfer_stat.cmd);
    EXPECT_EQ(PROT_STAT_OK, xfer_stat.stat);
    EXPECT_GT(xfer_stat.size, 0);
    EXPECT_LE(xfer_stat.size, file_contents.size());

    // File content
    nread = read(data_pipe[0], buf, sizeof(buf));
    ASSERT_EQ(file_contents.size(), nread);
    const std::string recvd_file(reinterpret_cast<const char*>(buf),
                                 file_contents.size());
    EXPECT_EQ(file_contents, recvd_file);

    close(data_pipe[0]);
    close(stat_fd);
}

TEST_F(FiodFix, read)
{
    const int data_fd = fiod_read(srv_fd, file.name().c_str(), 0, 0);
    ASSERT_NE(-1, data_fd);

    uint8_t buf [PROT_PDU_MAXSIZE];
    ssize_t nread;
    struct prot_file_stat ack;

    // Request ACK
    nread = read(data_fd, buf, PROT_STAT_SIZE);
    ASSERT_EQ(PROT_STAT_SIZE, nread);
    ASSERT_EQ(0, prot_unmarshal_stat(&ack, buf));
    EXPECT_EQ(PROT_CMD_STAT, ack.cmd);
    EXPECT_EQ(PROT_STAT_OK, ack.stat);
    EXPECT_EQ(8, ack.body_len);
    EXPECT_EQ(file_contents.size(), ack.size);

    // File content
    nread = read(data_fd, buf, sizeof(buf));
    ASSERT_EQ(file_contents.size(), nread);
    const std::string recvd_file(reinterpret_cast<const char*>(buf),
                                 file_contents.size());
    EXPECT_EQ(file_contents, recvd_file);

    close(data_fd);
}

TEST_F(FiodFix, read_range)
{
    constexpr loff_t offset {3};
    constexpr size_t len {5};

    const int data_fd = fiod_read(srv_fd, file.name().c_str(), offset, len);
    ASSERT_NE(-1, data_fd);

    uint8_t buf [PROT_PDU_MAXSIZE];
    ssize_t nread;
    struct prot_file_stat ack;

    // Request ACK
    nread = read(data_fd, buf, PROT_STAT_SIZE);
    ASSERT_EQ(PROT_STAT_SIZE, nread);
    ASSERT_EQ(0, prot_unmarshal_stat(&ack, buf));
    EXPECT_EQ(PROT_CMD_STAT, ack.cmd);
    EXPECT_EQ(PROT_STAT_OK, ack.stat);
    EXPECT_EQ(8, ack.body_len);
    EXPECT_EQ(len, ack.size);

    // File content
    nread = read(data_fd, buf, sizeof(buf));
    ASSERT_EQ(len, nread);
    const std::string recvd_file(reinterpret_cast<const char*>(buf), len);
    std::printf("recvd_file: %s\n", recvd_file.c_str());
    EXPECT_EQ(0, memcmp(buf, file_contents.c_str() + offset, len));

    close(data_fd);
}

TEST_F(FiodFix, read_large_file)
{
    test::TmpFile file2;

    std::vector<std::uint8_t> file_chunk(1024);
    std::iota(file_chunk.begin(), file_chunk.end(), 0);

    for (int i = 0; i < 1024; i++) {
        std::fwrite(file_chunk.data(), 1, file_chunk.size(), file2);
    }

    file2.close();

    const int data_fd = fiod_read(srv_fd, file2.name().c_str(), 0, 0);
    ASSERT_NE(-1, data_fd);

    uint8_t buf [PROT_PDU_MAXSIZE];
    ssize_t nread;
    struct prot_file_stat ack;

    // Request ACK
    nread = read(data_fd, buf, PROT_STAT_SIZE);
    ASSERT_EQ(PROT_STAT_SIZE, nread);
    ASSERT_EQ(0, prot_unmarshal_stat(&ack, buf));
    EXPECT_EQ(PROT_CMD_STAT, ack.cmd);
    EXPECT_EQ(PROT_STAT_OK, ack.stat);
    EXPECT_EQ(8, ack.body_len);
    EXPECT_EQ(1024 * 1024, ack.size);

    // File content
    size_t nchunks {};
    size_t nread_chunk {};

    for (;;) {
        ssize_t n = read(data_fd,
                         file_chunk.data() + nread_chunk,
                         file_chunk.size() - nread_chunk);

        ASSERT_NE(-1, n);
        ASSERT_NE(0, n);

        nread_chunk += (size_t)n;

        if (nread_chunk == file_chunk.size()) {
            for (size_t i = 0; i < file_chunk.size(); i++)
                ASSERT_EQ((uint8_t)i, (uint8_t)file_chunk[i]);
            nread_chunk = 0;
            nchunks++;
        }

        if (nchunks == 1024) {
            break;
        }
    }

    EXPECT_EQ(1024, nchunks);

    close(data_fd);
}
