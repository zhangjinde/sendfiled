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

#ifndef SFD_RESPONSES_H
#define SFD_RESPONSES_H

/**
   @file
   @ingroup mod_client
*/

#include <sys/types.h>

#include <stdbool.h>
#include <stdint.h>

#include "attr.h"

/**
   @addtogroup mod_client
   @{
*/

/**
   Response Command IDs.
*/
enum {
    /** File information */
    SFD_FILE_INFO = 0x81,
    /** Open file information */
    SFD_OPEN_FILE_INFO = 0x82,
    /** File transfer request/operation status */
    SFD_XFER_STAT = 0x83
};

enum {
    /** No error */
    SFD_STAT_OK = 0
};

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpadded"

/**
   Information pertaining to a file being transferred.
*/
struct sfd_file_info {
    uint8_t cmd;                /**< Command ID */
    uint8_t stat;               /**< Status Code */

    size_t size;                /**< File size on disk */
    time_t atime;               /**< Time of last access */
    time_t mtime;               /**< Time of last modification */
    time_t ctime;               /**< Time of last status change */
};

/**
   The size, in bytes, of a PDU header.

   The header includes the command ID and status code.
*/
#define SFD_HDR_SIZE (offsetof(struct sfd_file_info, stat) +        \
                      sizeof(((struct sfd_file_info*)NULL)->stat))

/**
   Information pertaining to an opened file.

   These files have been opened and read-locked, but their transfers are only
   started when the client issues the <em>Send Open File</em> command.

   @see sfd_open() and sfd_send_open().
*/
struct sfd_open_file_info {
    uint8_t cmd;                /**< Command ID */
    uint8_t stat;               /**< Status Code */

    size_t size;                /**< File size on disk */
    time_t atime;               /**< Time of last access */
    time_t mtime;               /**< Time of last modification */
    time_t ctime;               /**< Time of last status change */

    /** The open file's unique transaction identifier */
    size_t txnid;
};

/**
   A file transfer status PDU.
*/
struct sfd_xfer_stat {
    uint8_t cmd;                /**< Command ID */
    uint8_t stat;               /**< Status Code */

    /** The size of the most recent write or group of writes. */
    size_t size;
};

#pragma GCC diagnostic pop

#ifdef __cplusplus
extern "C" {
#endif

    /**
       @name Server responses
       @{
    */

    /**
       Returns the command ID field from a buffer.
    */
    int sfd_get_cmd(const void*) SFD_API;

    /**
       Returns the status code from a buffer.
    */
    int sfd_get_stat(const void*) SFD_API;

    /**
       Unmarshals a File Information PDU.

       @param[out] pdu The PDU

       @param[in] buf The source buffer
    */
    bool sfd_unmarshal_file_info(struct sfd_file_info* pdu,
                                 const void* buf) SFD_API;

    /**
       Unmarshals an Open File Information PDU.

       @param[out] pdu The PDU

       @param[in] buf The source buffer
    */
    bool sfd_unmarshal_open_file_info(struct sfd_open_file_info* pdu,
                                      const void* buf) SFD_API;

    /**
       Unmarshals a Transfer Status PDU.

       @param[out] pdu The PDU

       @param[in] buf The source buffer
    */
    bool sfd_unmarshal_xfer_stat(struct sfd_xfer_stat* pdu,
                                 const void* buf) SFD_API;

    /**
       Checks whether a Transfer Status PDU indicates transfer completion.
    */
    bool sfd_xfer_complete(const struct sfd_xfer_stat*) SFD_API;

    /**@}*/

#ifdef __cplusplus
}
#endif

/**@}*/

#endif
