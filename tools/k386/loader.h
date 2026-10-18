#ifndef K386_LOADER_H
#define K386_LOADER_H
#include "abi.h"
typedef struct {
    k32 header,bytes;
    k16 cs,ip,ss,sp,relocations,table;
} K386Image;
/* Returns a DOS error code; every accepted image fits the private 64 KiB VM. */
int k386_image(const k8 *header,k32 length,K386Image *image);
int k386_relocation(const K386Image *image,k16 offset,k16 segment);
#endif
