#include <gtest/gtest.h>

#include "impl/test_utils.hpp"

#include "fiod.h"
#include "impl/protocol.h"

namespace {
struct FiodFix : public ::testing::Test {
    static const std::string file_contents;

    FiodFix() : file(file_contents) {
        const std::string srvname {"testing123"};

        srv_pid = fiod_spawn(srvname.c_str(), "/tmp", 10);
        if (srv_pid <= 0)
            throw std::runtime_error("Couldn't start daemon");

        srv_fd = fiod_connect(srvname.c_str());
        if (srv_fd == -1)
            throw std::runtime_error("Couldn't connect to daemon");
    }

    ~FiodFix() {
        const int status = fiod_shutdown(srv_pid);
        if (!WIFEXITED(status))
            std::fprintf(stderr, "Daemon has not exited\n");
        if (WEXITSTATUS(status) != EXIT_SUCCESS)
            std::fprintf(stderr, "Daemon did not shut down cleanly\n");
    }

    int srv_fd;
    pid_t srv_pid;
    test::TmpFile file;
};

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wexit-time-destructors"

const std::string FiodFix::file_contents {"1234567890"};

#pragma GCC diagnostic pop

} // namespace

TEST(Fiod, unmarshal_error_status)
{
    struct prot_file_stat stat;

    std::uint8_t buf[PROT_STAT_SIZE] {}; // Zeroes body_len field
    buf[0] = PROT_CMD_STAT;
    buf[1] = ENOENT;

    EXPECT_EQ(ENOENT, prot_unmarshal_stat(&stat, buf));
    EXPECT_EQ(PROT_CMD_STAT, stat.cmd);
    EXPECT_EQ(ENOENT, stat.stat);
    EXPECT_EQ(0, stat.body_len);
}

TEST(Fiod, unmarshal_invalid_cmd)
{
    struct prot_file_stat stat;

    std::uint8_t buf[PROT_STAT_SIZE] {};
    buf[0] = 0xFF;
    buf[1] = PROT_STAT_OK;
    buf[2] = 111;

    EXPECT_EQ(-1, prot_unmarshal_stat(&stat, buf));
    EXPECT_EQ(0xFF, stat.cmd);
    EXPECT_EQ(PROT_STAT_OK, stat.stat);
    EXPECT_EQ(111, stat.body_len);
}

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
    EXPECT_EQ(0, ack.size);
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
