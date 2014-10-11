#include <cstring>

#include <gtest/gtest.h>

#include "impl/protocol_client.h"
#include "impl/protocol_server.h"

// ----------------- Client --------------------

TEST(Protocol, marshal_file_open)
{
    const std::string fname {"abc"};

    struct prot_request pdu;
    EXPECT_TRUE(prot_marshal_file_open(&pdu, fname.c_str(), 0xDEAD, 0xBEEF));

    EXPECT_EQ(PROT_CMD_FILE_OPEN, pdu.cmd);
    EXPECT_EQ(PROT_STAT_OK, pdu.stat);

    EXPECT_EQ(0xDEAD, pdu.offset);
    EXPECT_EQ(0xBEEF, pdu.len);
    EXPECT_EQ(fname.c_str(), pdu.filename);
}

TEST(Protocol, unmarshal_open_file_info)
{
    struct prot_open_file_info pdu1;
    prot_marshal_open_file_info(&pdu1, 111, 222, 333, 444, 777);

    struct prot_open_file_info pdu2;
    ASSERT_EQ(0, prot_unmarshal_open_file_info(&pdu2, &pdu1));

    EXPECT_EQ(111, pdu2.size);
    EXPECT_EQ(222, pdu2.atime);
    EXPECT_EQ(333, pdu2.mtime);
    EXPECT_EQ(444, pdu2.ctime);
    EXPECT_EQ(777, pdu2.txnid);
}

TEST(Protocol, unmarshal_file_info)
{
    struct prot_file_info pdu1;
    prot_marshal_file_info(&pdu1, 111, 222, 333, 444);

    struct prot_file_info pdu2;
    ASSERT_EQ(0, prot_unmarshal_file_info(&pdu2, &pdu1));

    EXPECT_EQ(111, pdu2.size);
    EXPECT_EQ(222, pdu2.atime);
    EXPECT_EQ(333, pdu2.mtime);
    EXPECT_EQ(444, pdu2.ctime);
}

TEST(Protocol, marshal_send)
{
    const std::string fname {"abc"};

    struct prot_request pdu;
    EXPECT_TRUE(prot_marshal_send(&pdu, fname.c_str(), 0xDEAD, 0xBEEF));

    EXPECT_EQ(PROT_CMD_SEND, pdu.cmd);
    EXPECT_EQ(PROT_STAT_OK, pdu.stat);

    EXPECT_EQ(0xDEAD, pdu.offset);
    EXPECT_EQ(0xBEEF, pdu.len);
    EXPECT_EQ(fname.c_str(), pdu.filename);
    EXPECT_EQ(fname.size(), pdu.filename_len);
}

TEST(Protocol, marshal_send_open)
{
    struct prot_send_open pdu;
    prot_marshal_send_open(&pdu, 0xDEADBEEF);

    EXPECT_EQ(PROT_CMD_SEND_OPEN, pdu.cmd);
    EXPECT_EQ(PROT_STAT_OK, pdu.stat);
    EXPECT_EQ(0xDEADBEEF, pdu.txnid);
}

TEST(Protocol, unmarshal_error_status)
{
    struct prot_xfer_stat stat;

    std::uint8_t buf[sizeof(stat)] {}; // Zeroes body_len field
    buf[0] = PROT_CMD_XFER_STAT;
    buf[1] = ENOENT;

    EXPECT_EQ(ENOENT, prot_unmarshal_xfer_stat(&stat, buf));
    EXPECT_EQ(PROT_CMD_XFER_STAT, stat.cmd);
    EXPECT_EQ(ENOENT, stat.stat);
}

TEST(Protocol, unmarshal_invalid_cmd)
{
    struct prot_xfer_stat stat;

    std::uint8_t buf[sizeof(stat)] {};
    buf[0] = 0xFF;
    buf[1] = PROT_STAT_OK;

    EXPECT_EQ(-1, prot_unmarshal_xfer_stat(&stat, buf));
    EXPECT_EQ(0xFF, stat.cmd);
    EXPECT_EQ(PROT_STAT_OK, stat.stat);
}

// ---------------- Server ------------------

TEST(Protocol, unmarshal_open_file)
{
    const std::string fname {"abc"};

    struct prot_request tmp;
    ASSERT_TRUE(prot_marshal_file_open(&tmp, fname.c_str(), 0xDEAD, 0xBEEF));

    std::vector<uint8_t> buf(PROT_REQ_BASE_SIZE + tmp.filename_len + 1);
    memcpy(buf.data(), &tmp, PROT_REQ_BASE_SIZE);
    memcpy(buf.data() + PROT_REQ_BASE_SIZE, tmp.filename, tmp.filename_len);

    struct prot_request pdu;
    ASSERT_TRUE(prot_unmarshal_request(&pdu, buf.data(), buf.size()));
    EXPECT_EQ(PROT_CMD_FILE_OPEN, pdu.cmd);
    EXPECT_EQ(PROT_STAT_OK, pdu.stat);

    ASSERT_EQ(fname.size(), pdu.filename_len);
    const std::string recvd_fname(pdu.filename);
    EXPECT_EQ(recvd_fname, fname);

    EXPECT_EQ(0xDEAD, pdu.offset);
    EXPECT_EQ(0xBEEF, pdu.len);
}

TEST(Protocol, unmarshal_send)
{
    const std::string fname {"abc"};

    struct prot_request tmp;
    ASSERT_TRUE(prot_marshal_send(&tmp, fname.c_str(), 0xDEAD, 0xBEEF));

    std::vector<uint8_t> buf(PROT_REQ_BASE_SIZE + tmp.filename_len + 1);
    memcpy(buf.data(), &tmp, PROT_REQ_BASE_SIZE);
    memcpy(buf.data() + PROT_REQ_BASE_SIZE, tmp.filename, tmp.filename_len);

    struct prot_request pdu;
    ASSERT_TRUE(prot_unmarshal_request(&pdu, buf.data(), buf.size()));
    EXPECT_EQ(PROT_CMD_SEND, pdu.cmd);
    EXPECT_EQ(PROT_STAT_OK, pdu.stat);
    EXPECT_EQ(0xDEAD, pdu.offset);
    EXPECT_EQ(0xBEEF, pdu.len);

    ASSERT_EQ(fname.size(), pdu.filename_len);
    const std::string recvd_fname(pdu.filename);
    EXPECT_EQ(fname, recvd_fname);
}

TEST(Protocol, unmarshal_send_open_file)
{
    struct prot_send_open tmp;
    prot_marshal_send_open(&tmp, 0xDEADBEEF);

    struct prot_send_open pdu;
    ASSERT_TRUE(prot_unmarshal_send_open(&pdu, &tmp));
    EXPECT_EQ(PROT_CMD_SEND_OPEN, pdu.cmd);
    EXPECT_EQ(PROT_STAT_OK, pdu.stat);
    EXPECT_EQ(0xDEADBEEF, pdu.txnid);
}

TEST(Protocol, marshal_file_info)
{
    struct prot_file_info pdu;
    prot_marshal_file_info(&pdu, 111, 222, 333, 444);

    EXPECT_EQ(PROT_CMD_FILE_INFO, pdu.cmd);
    EXPECT_EQ(PROT_STAT_OK, pdu.stat);

    EXPECT_EQ(111, pdu.size);
    EXPECT_EQ(222, pdu.atime);
    EXPECT_EQ(333, pdu.mtime);
    EXPECT_EQ(444, pdu.ctime);
}

TEST(Protocol, marshal_open_file_info)
{
    struct prot_open_file_info pdu;

    prot_marshal_open_file_info(&pdu,
                                111,  // size
                                222,  // atime
                                333,  // mtime
                                444,  // ctime
                                777); // open file's descriptor

    EXPECT_EQ(PROT_CMD_OPEN_FILE_INFO, prot_get_cmd(&pdu));
    EXPECT_EQ(PROT_STAT_OK, prot_get_stat(&pdu));

    EXPECT_EQ(111, pdu.size);

    EXPECT_EQ(222, pdu.atime);
    EXPECT_EQ(333, pdu.mtime);
    EXPECT_EQ(444, pdu.ctime);
    EXPECT_EQ(777, pdu.txnid);
}
