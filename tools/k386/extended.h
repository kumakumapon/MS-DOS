#ifndef K386_EXTENDED_H
#define K386_EXTENDED_H
#include "abi.h"
#define K386_CAPACITY 15u
/* D38B discovers the stable four-slot producer. V1 records retain both offsets
 * and sizes; the second pair is stored in separately owned conventional RAM. */
typedef struct {
 char signature[8];k16 version,size,capacity,busy;
 k32 view[4],usage[4],frame[4];
} K386Extended;
extern K386Extended k386_extended;
int k386_bank_capacity(unsigned);
void api_dispatch_extended(void);
#endif
