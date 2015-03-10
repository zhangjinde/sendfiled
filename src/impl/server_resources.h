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

#ifndef SFD_SERVER_RESOURCES_H_INCLUDED
#define SFD_SERVER_RESOURCES_H_INCLUDED

#include "protocol_server.h"
#include "../responses.h"

/*
  Resources are event sources such as the socket on which client requests are
  received (the server's 'listen' socket), pipes to which transfer status and/or
  file data is written, timers for open files, and which are registered with the
  poller for event notification.

  The poller requires all resources to have their registered file descriptors as
  the first field (they need to be type-punnable to struct syspoll_resrc).
*/

/*
  Resource type tags.

  All of these resource types will all be returned by the poller, so there needs
  to be a way to tell them apart from each other.
*/
enum {
    /* Identifies a resource as a file transfer */
    XFER_RESRC_TAG,
    /** Tag which identifies a resource as a timer. */
    TIMER_RESRC_TAG,
    /** Identifies a response pending delivery */
    PENDING_RESP_TAG
};

/**
   Types of transfer deferrals.

   Deferred transfers require processing during a secondary loop executed
   immediately after the regular (primary) event-processing loop.

   E.g., a timer may delete a transfer before events for that transfer have been
   processed and therefore, in order to avoid use-after-free errors, the
   transfer is marked as cancelled and its object only actually deleted during
   the deferred or secondary processing loop.
*/
enum deferral {
    NONE,

    /* The transfer is to be cancelled */
    CANCEL,

    /** The transfer's destination descriptor's I/O space could not be filled
        during primary processing without starving other transfers, so transfer
        its data during secondary processing until its I/O space has been
        filled, after which it can return to being processed during the primary
        loop. */
    READY
};

/**
   Information about a file being transferred.

   These values do not change through the course of a transfer.
*/
struct resrc_xfer_file {
    /** Number of bytes to be transferred from the file. Does not necessarily
        equal its size on disk (ranged transfers). */
    size_t size;
    /** File descriptor */
    int fd;
    /** Optimal block size for I/O */
    unsigned blksize;
};

/**
   A file transfer resource.

   @note The first few fields must be identical to the other resource structures
   due to the use of type punning.
*/
struct resrc_xfer {
    /** The data channel file descriptor (registered with poller) */
    int dest_fd;
    /* The type tag */
    int tag;
    /** The status channel file descriptor. Hijacked by other resource
        structures for use as a type tag (shameful!). */
    int stat_fd;
    /** The command ID */
    enum prot_cmd_req cmd;
    /** The unique identifier for this transfer */
    size_t txnid;
    /** Static information about the file being transferred, as it is on disk */
    struct resrc_xfer_file file;
    /** Context used by data-transfer functions on some platforms; NULL on
        others */
    struct fio_ctx* fio_ctx;
    /** Number of bytes left to transfer */
    size_t nbytes_left;
    /** The client process ID */
    pid_t client_pid;
    /** The deferral type */
    enum deferral defer;
};

struct resrc_xfer* xfer_new(enum prot_cmd_req cmd,
                            const struct resrc_xfer_file* file,
                            size_t nbytes,
                            pid_t client_pid,
                            int stat_fd,
                            int dest_fd,
                            size_t txnid);

bool is_xfer(const void*);

size_t resrc_xfer_txnid(void*);

void xfer_delete(void*);

/**
   A response waiting to be delivered.

   Instances are created when the first attempt to send a transfer error or
   completion notification fails temporarily.
*/
struct resrc_resp {
    /* Destination file descriptor (registered with poller) */
    int stat_fd;
    /* The type tag */
    int tag;
    /** The size of the PDU. Error notifications are headers only, but transfer
        completion notifications have a size_t field in the body. */
    size_t pdu_size;
    /** The PDU to be sent */
    struct sfd_xfer_stat pdu;
};

bool is_response(const void* p);

/**
   A timer set on an open file associated with a nascent file transfer.
*/
struct resrc_timer {
    /** Identifies the timer (registered with poller) */
    int ident;
    /** The type tag */
    int tag;
    /** The associated transfer ID */
    size_t txnid;
    /** Address of the associated transfer, for detecting txnid collisions */
    void* xfer_addr;
};

bool is_timer(const void* p);

size_t resrc_timer_txnid(void*);

void timer_delete(void* p);

#endif
