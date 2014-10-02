#include <cstring>

#include <gtest/gtest.h>

#include "impl/protocol_client.h"
#include "impl/protocol_server.h"

// ----------------- Client --------------------

TEST(Protocol, marshal_file_open)
{
    const std::string fname {"abc"};

    struct prot_request_m pdu;
    EXPECT_TRUE(prot_marshal_file_open(&pdu, fname.c_str(), 0xDEAD, 0xBEEF));

    EXPECT_EQ(PROT_CMD_FILE_OPEN, pdu.hdr[0]);
    EXPECT_EQ(PROT_STAT_OK, pdu.hdr[1]);

    size_t body_len;
    loff_t offset;
    size_t len;

    uint8_t* p = &pdu.hdr[2];

    memcpy(&body_len, p, 8);
    p += 8;
    memcpy(&offset, p, 8);
    p += 8;
    memcpy(&len, p, 8);
    p += 8;

    EXPECT_EQ(PROT_REQ_BODY_LEN + fname.size() + 1, body_len);
    EXPECT_EQ(0xDEAD, offset);
    EXPECT_EQ(0xBEEF, len);
    EXPECT_EQ(fname.c_str(), pdu.filename);
}

TEST(Protocol, unmarshal_open_file_info)
{
    struct prot_open_file_info_m buf;
    prot_marshal_open_file_info(&buf, 111, 222, 333, 444, 777);

    struct prot_open_file_info pdu;
    ASSERT_EQ(0, prot_unmarshal_open_file_info(&pdu, buf.data));

    EXPECT_EQ(111, pdu.size);
    EXPECT_EQ(222, pdu.atime);
    EXPECT_EQ(333, pdu.mtime);
    EXPECT_EQ(444, pdu.ctime);
    EXPECT_EQ(777, pdu.xfer_id);
}

TEST(Protocol, unmarshal_file_info)
{
    struct prot_file_info_m buf;
    prot_marshal_file_info(&buf, 111, 222, 333, 444);

    struct prot_file_info pdu;
    ASSERT_EQ(0, prot_unmarshal_file_info(&pdu, buf.data));

    EXPECT_EQ(111, pdu.size);
    EXPECT_EQ(222, pdu.atime);
    EXPECT_EQ(333, pdu.mtime);
    EXPECT_EQ(444, pdu.ctime);
}

TEST(Protocol, marshal_send)
{
    const std::string fname {"abc"};

    struct prot_request_m pdu;
    EXPECT_TRUE(prot_marshal_send(&pdu, fname.c_str(), 0xDEAD, 0xBEEF));

    EXPECT_EQ(PROT_CMD_SEND, pdu.hdr[0]);
    EXPECT_EQ(PROT_STAT_OK, pdu.hdr[1]);

    size_t body_len;
    loff_t offset;
    size_t len;

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

TEST(Protocol, marshal_send_open)
{
    struct prot_send_open_m pdu;
    prot_marshal_send_open(&pdu, 0xDEADBEEF);

    EXPECT_EQ(PROT_CMD_SEND_OPEN, pdu.data[0]);
    EXPECT_EQ(PROT_STAT_OK, pdu.data[1]);

    size_t body_len;
    std::uint32_t txnid;

    uint8_t* p = &pdu.data[2];

    memcpy(&body_len, p, 8);
    p += 8;
    memcpy(&txnid, p, PROT_TXNID_SIZE);

    EXPECT_EQ(PROT_TXNID_SIZE, body_len);
    EXPECT_EQ(0xDEADBEEF, txnid);
}

TEST(Protocol, unmarshal_error_status)
{
    struct prot_xfer_stat stat;

    std::uint8_t buf[PROT_XFER_STAT_SIZE] {}; // Zeroes body_len field
    buf[0] = PROT_CMD_XFER_STAT;
    buf[1] = ENOENT;

    EXPECT_EQ(ENOENT, prot_unmarshal_xfer_stat(&stat, buf));
    EXPECT_EQ(PROT_CMD_XFER_STAT, stat.cmd);
    EXPECT_EQ(ENOENT, stat.stat);
    EXPECT_EQ(0, stat.body_len);
}

TEST(Protocol, unmarshal_invalid_cmd)
{
    struct prot_xfer_stat stat;

    std::uint8_t buf[PROT_XFER_STAT_SIZE] {};
    buf[0] = 0xFF;
    buf[1] = PROT_STAT_OK;
    buf[2] = 111;

    EXPECT_EQ(-1, prot_unmarshal_xfer_stat(&stat, buf));
    EXPECT_EQ(0xFF, stat.cmd);
    EXPECT_EQ(PROT_STAT_OK, stat.stat);
    EXPECT_EQ(111, stat.body_len);
}

// ---------------- Server ------------------

TEST(Protocol, unmarshal_open_file)
{
    const std::string fname {"abc"};

    struct prot_request_m tmp;
    ASSERT_TRUE(prot_marshal_file_open(&tmp, fname.c_str(), 0xDEAD, 0xBEEF));

    uint8_t buf [PROT_REQ_MAXSIZE];
    memcpy(buf, tmp.iovs[0].iov_base, tmp.iovs[0].iov_len);
    memcpy(buf + tmp.iovs[0].iov_len, tmp.iovs[1].iov_base, tmp.iovs[1].iov_len);

    struct prot_request pdu;
    ASSERT_EQ(0, prot_unmarshal_request(&pdu, buf));
    EXPECT_EQ(PROT_CMD_FILE_OPEN, pdu.cmd);
    EXPECT_EQ(PROT_STAT_OK, pdu.stat);
    EXPECT_EQ(PROT_REQ_BODY_LEN + fname.size() + 1, pdu.body_len);
    EXPECT_EQ(0xDEAD, pdu.offset);
    EXPECT_EQ(0xBEEF, pdu.len);
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

TEST(Protocol, unmarshal_send_open_file)
{
    struct prot_send_open_m tmp;
    prot_marshal_send_open(&tmp, 0xDEADBEEF);

    struct prot_send_open pdu;
    ASSERT_EQ(0, prot_unmarshal_send_open(&pdu, tmp.data));
    EXPECT_EQ(PROT_CMD_SEND_OPEN, pdu.cmd);
    EXPECT_EQ(PROT_STAT_OK, pdu.stat);
    EXPECT_EQ(PROT_TXNID_SIZE, pdu.body_len);
    EXPECT_EQ(0xDEADBEEF, pdu.txnid);
}

TEST(Protocol, marshal_file_info)
{
    struct prot_file_info_m pdu;
    prot_marshal_file_info(&pdu, 111, 222, 333, 444);

    EXPECT_EQ(PROT_CMD_FILE_INFO, pdu.data[0]);
    EXPECT_EQ(PROT_STAT_OK, pdu.data[1]);

    uint8_t* p = pdu.data + 2;

    size_t body_len;
    memcpy(&body_len, p, sizeof(body_len));
    EXPECT_EQ(PROT_FILE_INFO_BODY_LEN, body_len);
    p += sizeof(body_len);

    size_t file_size;
    memcpy(&file_size, p, sizeof(file_size));
    EXPECT_EQ(111, file_size);
    p += sizeof(file_size);

    time_t mtime, atime, ctime;

    memcpy(&atime, p, sizeof(atime));
    p += sizeof(atime);
    memcpy(&mtime, p, sizeof(mtime));
    p += sizeof(mtime);
    memcpy(&ctime, p, sizeof(ctime));
    p += sizeof(ctime);

    EXPECT_EQ(222, atime);
    EXPECT_EQ(333, mtime);
    EXPECT_EQ(444, ctime);
}

TEST(Protocol, marshal_open_file_info)
{
    struct prot_open_file_info_m pdu;

    prot_marshal_open_file_info(&pdu,
                                111,  // size
                                222,  // atime
                                333,  // mtime
                                444,  // ctime
                                777); // open file's descriptor

    EXPECT_EQ(PROT_CMD_OPEN_FILE_INFO, pdu.data[0]);
    EXPECT_EQ(PROT_STAT_OK, pdu.data[1]);

    uint8_t* p = pdu.data + 2;

    size_t body_len;
    memcpy(&body_len, p, sizeof(body_len));
    EXPECT_EQ(PROT_OPEN_FILE_INFO_BODY_LEN, body_len);
    p += sizeof(body_len);

    size_t file_size;
    memcpy(&file_size, p, sizeof(file_size));
    EXPECT_EQ(111, file_size);
    p += sizeof(file_size);

    time_t mtime, atime, ctime;

    memcpy(&atime, p, sizeof(atime));
    p += sizeof(atime);
    memcpy(&mtime, p, sizeof(mtime));
    p += sizeof(mtime);
    memcpy(&ctime, p, sizeof(ctime));
    p += sizeof(ctime);

    EXPECT_EQ(222, atime);
    EXPECT_EQ(333, mtime);
    EXPECT_EQ(444, ctime);

    int open_file_fd;
    memcpy(&open_file_fd, p, sizeof(open_file_fd));

    EXPECT_EQ(777, open_file_fd);
}
