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

#ifndef SFD_PROTOCOL_H
#define SFD_PROTOCOL_H

#include <sys/types.h>

#include <stddef.h>
#include <stdint.h>

/**
   Request Command IDs.

   Responses have bit 7 set, requests do not.
*/
enum prot_cmd_req {
    /* Open and send file information */
    PROT_CMD_FILE_OPEN = 0x01,
    /* Read file contents */
    PROT_CMD_READ = 0x02,
    /* Send file contents to destination file descriptor */
    PROT_CMD_SEND = 0x03,
    /* Send a previously-opened file */
    PROT_CMD_SEND_OPEN = 0x04
};

#define PROT_IS_REQUEST(cmd) (((cmd) & 0x80) == 0)

/* Maximum number of file descriptors transferred in a single message */
#define PROT_MAXFDS 2

#define PROT_FILENAME_MAX 512   /* Excludes the terminating '\0' */

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpadded"

/* ------------------ PDU Headers ------------- */

#define PROT_HDR_FIELDS                         \
    uint8_t cmd;                                \
    uint8_t stat

struct prot_hdr {
    PROT_HDR_FIELDS;
};

/* ------------- File Operation Request PDU ------------ */

/**
   A request PDU.

   This is currently the only PDU type which is not sent over the 'wire' as-is
   (bit-by-bit). The 'wire format' looks something like this:
   CSOOOOOOOOLLLLLLLLFFFFF0, where C = cmd; D = stat; O = offset bytes; L =
   transfer length bytes; F = filename characters; 0 = filename-terminating
   NUL. NOTE that the filename_len field is not transmitted.
*/
struct prot_request {
    PROT_HDR_FIELDS;
    /* Offset from the beginning of the file to start reading from */
    loff_t offset;
    /* Number of bytes to transfer */
    size_t len;

    /* The name of the file to be read */
    const char* filename;
    /* The filename length (not sent--for convenience only) */
    size_t filename_len;
};

#define PROT_REQ_BASE_SIZE (offsetof(struct prot_request, len) +        \
                            sizeof(((struct prot_request*)NULL)->len))

/* 1 for a non-empty filename; 1 for the terminating NUL */
#define PROT_REQ_MINSIZE PROT_REQ_BASE_SIZE + 1 + 1

/* Maximum size of a file operation request PDU */
#define PROT_REQ_MAXSIZE (sizeof(struct prot_request) + \
                          PROT_FILENAME_MAX + 1)

/* -------------- 'Send Open File' PDU */

struct prot_send_open {
    PROT_HDR_FIELDS;
    size_t txnid;
};

/** The value struct sfd_xfer_stat.size is set to in a terminal transfer status
    notification to indicate a complete transfer */
#define PROT_XFER_COMPLETE (size_t)-1

#pragma GCC diagnostic pop

#endif
