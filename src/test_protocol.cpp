#include <cstring>

#include <gtest/gtest.h>

#include "impl/protocol.h"

TEST(Protocol, marshal_send)
{
    const std::string fname {"abc"};

    struct prot_request_m pdu;
    EXPECT_TRUE(prot_marshal_send(&pdu, fname.c_str(), 0xDEAD, 0xBEEF));

    EXPECT_EQ(PROT_CMD_SEND, pdu.hdr[0]);
    EXPECT_EQ(PROT_STAT_OK, pdu.hdr[1]);

    uint64_t body_len;
    uint64_t offset;
    uint64_t len;

    uint8_t* p = &pdu.hdr[2];

    memcpy(&body_len, p, 8);
    p += 8;
    memcpy(&offset, p, 8);
    p += 8;
    memcpy(&len, p, 8);
    p += 8;

    EXPECT_EQ(8 + 8 + fname.size() + 1, body_len);
    EXPECT_EQ(0xDEAD, offset);
    EXPECT_EQ(0xBEEF, len);
    EXPECT_EQ(fname.c_str(), pdu.filename);
}

TEST(Protocol, unmarshal_send)
{
    const std::string fname {"abc"};

    struct prot_request_m tmp;
    ASSERT_TRUE(prot_marshal_send(&tmp, fname.c_str(), 0xDEAD, 0xBEEF));

    uint8_t buf [PROT_REQ_MAXSIZE];
    memcpy(buf, tmp.iovs[0].iov_base, tmp.iovs[0].iov_len);
    memcpy(buf + tmp.iovs[0].iov_len, tmp.iovs[1].iov_base, tmp.iovs[1].iov_len);

    struct prot_request pdu;
    ASSERT_EQ(0, prot_unmarshal_request(&pdu, buf));
    EXPECT_EQ(PROT_CMD_SEND, pdu.cmd);
    EXPECT_EQ(PROT_STAT_OK, pdu.stat);
    EXPECT_EQ(8 + 8 + fname.size() + 1, pdu.body_len);
    EXPECT_EQ(0xDEAD, pdu.offset);
    EXPECT_EQ(0xBEEF, pdu.len);
}

TEST(Protocol, unmarshal_error_status)
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

TEST(Protocol, unmarshal_invalid_cmd)
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
