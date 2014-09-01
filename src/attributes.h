#ifndef FIOD_ATTRIBUTES_H
#define FIOD_ATTRIBUTES_H

#define UNUSED __attribute__((unused))

#define ALWAYS_INLINE __attribute__((always_inline))

#define DSO_EXPORT  __attribute__((visibility("default")))
#define DSO_IMPORT  __attribute__((visibility("default")))
#define DSO_LOCAL   __attribute__((visibility("hidden")))

#endif
