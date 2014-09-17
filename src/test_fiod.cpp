#include <gtest/gtest.h>

#include "impl/test_utils.hpp"

#include "fiod.h"
#include "impl/protocol.h"

TEST(Fiod, send)
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

    uint8_t buf [PROT_PDU_MAXSIZE];

    // Request ACK (with file size)
    ssize_t nread = read(pipefd, buf, PROT_ACK_SIZE);
    ASSERT_EQ(PROT_ACK_SIZE, nread);
    struct prot_ack ack;
    ASSERT_EQ(0, prot_unmarshal_ack(&ack, buf));
    EXPECT_EQ(PROT_CMD_ACK, ack.cmd);
    EXPECT_EQ(PROT_STAT_OK, ack.stat);
    EXPECT_EQ(8, ack.body_len);
    EXPECT_EQ(file_contents.size(), ack.file_size);

    // Transfer status update
    nread = read(pipefd, buf, PROT_DATA_FRAME_SIZE);
    ASSERT_EQ(PROT_DATA_FRAME_SIZE, nread);

    struct prot_chunk_hdr chunk_hdr;
    ASSERT_EQ(0, prot_unmarshal_chunk_hdr(&chunk_hdr, buf));
    EXPECT_EQ(PROT_CMD_DATA, chunk_hdr.cmd);
    EXPECT_EQ(PROT_STAT_OK, chunk_hdr.stat);
    // Check that file data chunk is in range (0, file_contents.size()]
    EXPECT_GT(chunk_hdr.chunk_size, 0);
    EXPECT_LE(chunk_hdr.chunk_size, file_contents.size());

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
