#ifndef FIOD_RESPONSES_H
#define FIOD_RESPONSES_H

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
    FIOD_FILE_INFO = 0x81,
    /** Open file information */
    FIOD_OPEN_FILE_INFO = 0x82,
    /** File transfer request/operation status */
    FIOD_XFER_STAT = 0x83
};

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpadded"

/**
   Information pertaining to a file being transferred.
 */
struct fiod_file_info {
    uint8_t cmd;                /**< Command ID */
    uint8_t stat;               /**< Status Code */

    size_t size;                /**< File size on disk */
    time_t atime;               /**< Time of last access */
    time_t mtime;               /**< Time of last modification */
    time_t ctime;               /**< Time of last status change */
};

/**
   Information pertaining to an opened file.

   These files have been opened and read-locked, but their transfers are only
   started when the client issues the <em>Send Open File</em> command.

   @see fiod_open() and fiod_send_open().
 */
struct fiod_open_file_info {
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
struct fiod_xfer_stat {
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
       @name Server Responses
       @{
    */

    /**
       Returns the command ID field from a buffer.
    */
    int fiod_get_cmd(const void*) FIOD_API;

    /**
       Returns the status code from a buffer.
    */
    int fiod_get_stat(const void*) FIOD_API;

    /**
       Unmarshals a File Information PDU.

       @param[out] pdu The PDU

       @param[in] buf The source buffer
     */
    bool fiod_unmarshal_file_info(struct fiod_file_info* pdu,
                                  const void* buf) FIOD_API;

    /**
       Unmarshals an Open File Information PDU.

       @param[out] pdu The PDU

       @param[in] buf The source buffer
     */
    bool fiod_unmarshal_open_file_info(struct fiod_open_file_info* pdu,
                                       const void* buf) FIOD_API;

    /**
       Unmarshals a Transfer Status PDU.

       @param[out] pdu The PDU

       @param[in] buf The source buffer
     */
    bool fiod_unmarshal_xfer_stat(struct fiod_xfer_stat* pdu,
                                  const void* buf) FIOD_API;

    /**
       Checks whether a Transfer Status PDU indicates transfer completion.
     */
    bool fiod_xfer_complete(const struct fiod_xfer_stat*) FIOD_API;

    /**@}*/

#ifdef __cplusplus
}
#endif

/**@}*/

#endif
