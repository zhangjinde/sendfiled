#include <gtest/gtest.h>

#include "impl/test_utils.hpp"

#include "fiod.h"
#include "impl/protocol.h"

TEST(Fiod, xxx)
{
    const std::string srvname {"testing123"};

    const pid_t pid = fiod_spawn(srvname.c_str(), "/tmp", 10);

    ASSERT_GT(pid, 0);

    const int fd = fiod_connect(srvname.c_str());
    if (fd == -1)
        std::printf("XXX errno: %s\n", strerror(errno));

    std::printf("Connected; fd: %d\n", fd);

    const std::string file_contents {"1234567890"};
    test::TmpFile file(file_contents);

    int pfd[2];
    ASSERT_NE(-1, pipe(pfd));

    const int pipefd = fiod_send(fd, file.name().c_str(), pfd[1], 0, 0);
    close(pfd[1]);

    if (pipefd == -1)
        std::printf("XXX errno: %s\n", strerror(errno));

    uint8_t buf [PROT_XFER_STAT_SIZE];
    ssize_t nstat = read(pipefd, buf, PROT_XFER_STAT_SIZE);
    ASSERT_EQ(PROT_XFER_STAT_SIZE, nstat);

    struct prot_xfer_stat stat;
    ASSERT_TRUE(prot_unmarshal_xfer_stat(&stat, buf));
    EXPECT_EQ(PROT_CMD_STAT, stat.cmd);
    EXPECT_EQ(PROT_STAT_XFER, stat.stat);
    EXPECT_EQ(file_contents.size(), stat.file_size);
    EXPECT_EQ(file_contents.size(), stat.new_file_offset);

    ssize_t nfile = read(pfd[0], buf, sizeof(buf));
    if (nfile == -1) {
        printf("LLLLerrno: %s\n", strerror(errno));
    }
    ASSERT_EQ(file_contents.size(), nfile);
    const std::string recvd_file((const char*)buf, file_contents.size());
    EXPECT_EQ(file_contents, recvd_file);

    const int status = fiod_shutdown(pid);

    EXPECT_TRUE(WIFEXITED(status));
    EXPECT_EQ(EXIT_SUCCESS, WEXITSTATUS(status));
}
