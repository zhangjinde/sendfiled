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
   Response command IDs.
*/
enum sfd_cmd_id {
    /** File information */
    SFD_FILE_INFO = 0x81,
    /** File transfer request/operation status */
    SFD_XFER_STAT = 0x82
};

/**
   Sendfiled-specific response status codes.
 */
enum sfd_op_stat {
    /** No error */
    SFD_STAT_OK = 0
};

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpadded"

/**
   A response message containing file metadata.

   Sent in response to sfd_read() and sfd_send().
*/
struct sfd_file_info {
    /* header */
    uint8_t cmd;                /**< Command ID */
    uint8_t stat;               /**< Status Code */

    /* body */
    size_t size;                /**< File size on disk */
    time_t atime;               /**< Time of last access */
    time_t mtime;               /**< Time of last modification */
    time_t ctime;               /**< Time of last status change */

    /** The file's unique transaction identifier */
    size_t txnid;
};

/**
   A response message containing file transfer status.

   Sent in response to sfd_send() and sfd_send_open().

   @sa sfd_xfer_complete()
*/
struct sfd_xfer_stat {
    /* header */
    uint8_t cmd;                /**< Command ID */
    uint8_t stat;               /**< Status Code */

    /* body */
    size_t size;                /**< Size of the most recent group of writes */
};

#pragma GCC diagnostic pop

/**
   The size of a PDU header, in bytes.

   The PDU header consists of the command ID and status code.
*/
#define SFD_HDR_SIZE (offsetof(struct sfd_file_info, stat) +        \
                      sizeof(((struct sfd_file_info*)NULL)->stat))

/**
   Size of the biggest response message that can be received from the server.
 */
#define SFD_MAX_RESP_SIZE sizeof(struct sfd_file_info)

#ifdef __cplusplus
extern "C" {
#endif

    /**
       @name Server responses
       @{
    */

    /**
       Returns the command ID from a buffer.
    */
    int sfd_get_cmd(const void*) SFD_API;

    /**
       Returns the response status code from a buffer.
    */
    int sfd_get_stat(const void*) SFD_API;

    /**
       Unmarshals a File Information PDU.

       @param[out] pdu The PDU

       @param[in] buf The source buffer

       @retval true Success

       @retval false The buffer contained an unexpected command ID or error
       response code.
    */
    bool sfd_unmarshal_file_info(struct sfd_file_info* pdu,
                                 const void* buf) SFD_API;

    /**
       Unmarshals a Transfer Status PDU.

       @param[out] pdu The PDU

       @param[in] buf The source buffer

       @retval true Success

       @retval false The buffer contained an unexpected command ID or error
       response code.
    */
    bool sfd_unmarshal_xfer_stat(struct sfd_xfer_stat* pdu,
                                 const void* buf) SFD_API;

    /**
       Checks whether a Transfer Status PDU signifies transfer completion.
    */
    bool sfd_xfer_complete(const struct sfd_xfer_stat*) SFD_API;

    /**@}*/

#ifdef __cplusplus
}
#endif

/**@}*/

#endif
