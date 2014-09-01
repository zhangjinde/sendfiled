/**
   @defgroup mod_client Client

   Interface for clients.
 */

/**
   @file

   @ingroup mod_client
 */

#ifndef FIOD_FIOD_H
#define FIOD_FIOD_H

#include <stdbool.h>

#include "attributes.h"

#ifdef __cplusplus
extern "C" {
#endif

    /**
       @addtogroup mod_client
       @{
     */

    /**
       Spawns the fiod process.

       @param root The root directory

       @retval >0 The process id
       @retval -1 An error occurred (see @a errno)
     */
    int fiod_spawn(const char* root) DSO_EXPORT;

    /**
       Signals the fiod process to shut down.
     */
    int fiod_shutdown(int pid) DSO_EXPORT;

    /**@} */

#ifdef __cplusplus
}
#endif

#endif
