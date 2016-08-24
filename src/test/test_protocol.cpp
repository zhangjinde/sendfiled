/*
  Copyright (c) 2016, Francois Kritzinger
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

#include <cstring>

#include <gtest/gtest.h>

#include "../sendfiled.h"
#include "../impl/protocol_client.h"
#include "../impl/protocol_server.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wglobal-constructors"
#pragma GCC diagnostic ignored "-Wexit-time-destructors"
#pragma GCC diagnostic ignored "-Wpadded"
#pragma GCC diagnostic ignored "-Wold-style-cast"

// ----------------- Client --------------------

TEST(Protocol, marshal_file_open)
{
    const std::string fname {"abc"};

    struct prot_request pdu;
    EXPECT_TRUE(prot_marshal_file_open(&pdu, fname.c_str(), 0xDEAD, 0xBEEF));

    EXPECT_EQ(PROT_CMD_FILE_OPEN, pdu.cmd);
    EXPECT_EQ(SFD_STAT_OK, pdu.stat);

    EXPECT_EQ(0xDEAD, pdu.offset);
    EXPECT_EQ(0xBEEF, pdu.len);
    EXPECT_EQ(fname.c_str(), pdu.filename);
}

TEST(Protocol, unmarshal_file_info)
{
    struct sfd_file_info pdu1;
    prot_marshal_file_info(&pdu1, 111, 222, 333, 444, 777);

    struct sfd_file_info pdu2;
    ASSERT_TRUE(sfd_unmarshal_file_info(&pdu2, &pdu1));

    EXPECT_EQ(111, pdu2.size);
    EXPECT_EQ(222, pdu2.atime);
    EXPECT_EQ(333, pdu2.mtime);
    EXPECT_EQ(444, pdu2.ctime);
    EXPECT_EQ(777, pdu2.txnid);
}

TEST(Protocol, marshal_send)
{
    const std::string fname {"abc"};

    struct prot_request pdu;
    EXPECT_TRUE(prot_marshal_send(&pdu, fname.c_str(), 0xDEAD, 0xBEEF));

    EXPECT_EQ(PROT_CMD_SEND, pdu.cmd);
    EXPECT_EQ(SFD_STAT_OK, pdu.stat);

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
    EXPECT_EQ(SFD_STAT_OK, pdu.stat);
    EXPECT_EQ(0xDEADBEEF, pdu.txnid);
}

TEST(Protocol, marshal_cancel)
{
    struct prot_cancel pdu;
    prot_marshal_cancel(&pdu, 0xDEADBEEF);

    EXPECT_EQ(PROT_CMD_CANCEL, pdu.cmd);
    EXPECT_EQ(SFD_STAT_OK, pdu.stat);
    EXPECT_EQ(0xDEADBEEF, pdu.txnid);
}

TEST(Protocol, unmarshal_malformed_xfer_stat)
{
    struct sfd_xfer_stat pdu;
    std::uint8_t buf [sizeof(pdu)];

    prot_marshal_xfer_stat(&pdu, 0xDEAD);
    memcpy(buf, &pdu, sizeof(pdu));

    const auto cmd = pdu.cmd;
    const auto stat = pdu.stat;

    // Check that we have a valid base PDU
    ASSERT_TRUE(sfd_unmarshal_xfer_stat(&pdu, buf));

    pdu.size = 0xBEEF;

    // Invalid command ID
    buf[0] = ~buf[0];
    EXPECT_FALSE(sfd_unmarshal_xfer_stat(&pdu, buf));
    // Body unmodified
    EXPECT_EQ(0xBEEF, pdu.size);
    buf[0] = cmd;

    // Nonzero status code (error)
    buf[1] = SFD_STAT_OK + 1;
    EXPECT_FALSE(sfd_unmarshal_xfer_stat(&pdu, buf));
    // Body unmodified
    EXPECT_EQ(0xBEEF, pdu.size);
    buf[0] = stat;
}

TEST(Protocol, unmarshal_malformed_file_info)
{
    const std::size_t txnid {777};

    struct sfd_file_info pdu;
    std::uint8_t buf [sizeof(pdu)];

    prot_marshal_file_info(&pdu, 0xDEAD, 111, 222, 333, txnid);
    memcpy(buf, &pdu, sizeof(pdu));

    const auto cmd = pdu.cmd;
    const auto stat = pdu.stat;

    // Check that we have a valid base PDU
    ASSERT_TRUE(sfd_unmarshal_file_info(&pdu, buf));

    pdu.size = 0xBEEF;
    pdu.atime = 777;
    pdu.mtime = 888;
    pdu.ctime = 999;

    // Invalid command ID
    buf[0] = ~buf[0];
    EXPECT_FALSE(sfd_unmarshal_file_info(&pdu, buf));
    // Body unmodified
    EXPECT_EQ(0xBEEF, pdu.size);
    EXPECT_EQ(777, pdu.atime);
    EXPECT_EQ(888, pdu.mtime);
    EXPECT_EQ(999, pdu.ctime);
    buf[0] = cmd;

    // Nonzero status code (error)
    buf[1] = SFD_STAT_OK + 1;
    EXPECT_FALSE(sfd_unmarshal_file_info(&pdu, buf));
    // Body unmodified
    EXPECT_EQ(0xBEEF, pdu.size);
    EXPECT_EQ(777, pdu.atime);
    EXPECT_EQ(888, pdu.mtime);
    EXPECT_EQ(999, pdu.ctime);
    buf[0] = stat;
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
    EXPECT_EQ(SFD_STAT_OK, pdu.stat);

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
    EXPECT_EQ(SFD_STAT_OK, pdu.stat);
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
    EXPECT_EQ(SFD_STAT_OK, pdu.stat);
    EXPECT_EQ(0xDEADBEEF, pdu.txnid);
}

TEST(Protocol, unmarshal_cancel_file)
{
    struct prot_cancel tmp;
    prot_marshal_cancel(&tmp, 0xDEADBEEF);

    struct prot_cancel pdu;
    ASSERT_TRUE(prot_unmarshal_cancel(&pdu, &tmp));
    EXPECT_EQ(PROT_CMD_CANCEL, pdu.cmd);
    EXPECT_EQ(SFD_STAT_OK, pdu.stat);
    EXPECT_EQ(0xDEADBEEF, pdu.txnid);
}

TEST(Protocol, marshal_file_info)
{
    const std::size_t txnid {777};

    struct sfd_file_info pdu;

    prot_marshal_file_info(&pdu,
                           111,  // size
                           222,  // atime
                           333,  // mtime
                           444,  // ctime
                           txnid);

    EXPECT_EQ(SFD_FILE_INFO, sfd_get_cmd(&pdu));
    EXPECT_EQ(SFD_STAT_OK, sfd_get_stat(&pdu));

    EXPECT_EQ(111, pdu.size);

    EXPECT_EQ(222, pdu.atime);
    EXPECT_EQ(333, pdu.mtime);
    EXPECT_EQ(444, pdu.ctime);
    EXPECT_EQ(777, pdu.txnid);
}

TEST(Protocol, unmarshal_malformed_request)
{
    const std::string fname {"abc"};

    struct prot_request req;
    ASSERT_TRUE(prot_marshal_file_open(&req, fname.c_str(), 0xDEAD, 0xBEEF));

    const auto cmd = req.cmd;
    const auto stat = req.stat;

    std::vector<std::uint8_t> buf(PROT_REQ_BASE_SIZE + req.filename_len + 1);
    memcpy(buf.data(), &req, PROT_REQ_BASE_SIZE);
    memcpy(buf.data() + PROT_REQ_BASE_SIZE, req.filename, req.filename_len);

    // First confirm that the buffer we have just manually constructed contains
    // a valid request PDU
    ASSERT_TRUE(prot_unmarshal_request(&req, buf.data(), buf.size()));

    std::vector<std::uint8_t> b {buf};

    // Too short
    for (std::size_t i = 0; i < PROT_REQ_MINSIZE; i++)
        ASSERT_FALSE(prot_unmarshal_request(&req, b.data(), i));

    // Invalid command ID
    b[0] = ~buf[0];  // Assuming the complement of a valid cmd ID is invalid
    EXPECT_FALSE(prot_unmarshal_request(&req, b.data(), b.size()));
    b[0] = cmd;

    // Nonzero status code
    b[1] = SFD_STAT_OK + 1;
    EXPECT_FALSE(prot_unmarshal_request(&req, b.data(), b.size()));
    b[1] = stat;

    // Filename not NUL-terminated
    b.back() = 'a';
    EXPECT_FALSE(prot_unmarshal_request(&req, b.data(), b.size()));
    b.back() = '\0';
}

TEST(Protocol, unmarshal_request_with_oversized_filename)
{
    std::string fname(PROT_FILENAME_MAX + 1, 'a');

    struct prot_request req = {
        PROT_CMD_SEND,
        SFD_STAT_OK,
        0xDEAD,
        0xBEEF,
        nullptr, 0
    };

    std::vector<std::uint8_t> buf(PROT_REQ_BASE_SIZE + fname.size() + 1, '\0');
    std::memcpy(buf.data(), &req, PROT_REQ_BASE_SIZE);
    std::memcpy(buf.data() + PROT_REQ_BASE_SIZE, fname.data(), fname.size());

    EXPECT_FALSE(prot_unmarshal_request(&req, buf.data(), buf.size()));
    EXPECT_EQ(ENAMETOOLONG, errno);
}

TEST(Protocol, unmarshal_malformed_send_open_file)
{
    struct prot_send_open pdu;
    prot_marshal_send_open(&pdu, 0xDEADBEEF);

    const auto cmd = pdu.cmd;
    const auto stat = pdu.stat;

    std::uint8_t buf [sizeof(pdu)];
    memcpy(buf, &pdu, sizeof(pdu));

    // First confirm that the buffer we have just manually constructed contains
    // a valid request PDU
    ASSERT_TRUE(prot_unmarshal_send_open(&pdu, buf));

    // Caller is responsible for ensuring that there's enough data in the buffer

    pdu.txnid = 123;

    // Invalid command ID
    buf[0] = ~pdu.cmd;
    EXPECT_FALSE(prot_unmarshal_send_open(&pdu, buf));
    // PDU was not actually unmarshaled
    EXPECT_EQ(123, pdu.txnid);
    buf[0] = cmd;

    // Nonzero status code
    buf[1] = SFD_STAT_OK + 1;
    EXPECT_FALSE(prot_unmarshal_send_open(&pdu, buf));
    // Body unmodified
    EXPECT_EQ(123, pdu.txnid);
    buf[1] = stat;
}

#pragma GCC diagnostic pop
