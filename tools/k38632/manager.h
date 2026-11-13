#ifndef K32_MANAGER_H
#define K32_MANAGER_H
#include "host.h"
#include "../k386/resources.h"
enum {K32_V86,K32_PM32};
enum {K32_RESTART=13};
/* The first 152 bytes retain command/key/path offsets, not the legacy signature.
 * Versioned pointers select the real producer of each view and diagnostic. */
typedef struct {
 char signature[8];u16 version,size,ready,error,command,session,key,busy;
 char path[128];u16 kind[2],abi[2];u32 generation[2],view[2],record[2];
 u32 resources,legacy,diagnostics;u16 owner,reserved;
} K32Manager;
/* D392 v2 is additive; the original D391/200-byte producer stays intact. */
typedef struct {
 char signature[8];u16 version,size,ready,error,command,session,key,busy;
 char path[128];u16 kind[4],abi[4];u32 generation[4],view[4],record[4],usage[4],frame[4];
 u32 resources,legacy;u16 owner,capacity,profile,reserved;
} K32Manager4;
extern K32Manager k32_manager;
extern K32Manager4 k32_extended;
u32 manager_generation(unsigned);

void manager_init(unsigned segment);
void manager_dispatch(void);
void manager_publish(void);
int manager_capacity(unsigned);
int manager_v86_active(unsigned id);
int manager_staging(unsigned id);
unsigned host_occupied(void);
#endif
