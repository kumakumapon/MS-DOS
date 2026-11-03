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
extern K32Manager k32_manager;
void manager_init(unsigned segment);
void manager_dispatch(void);
int manager_v86_active(unsigned id);
int manager_staging(unsigned id);
unsigned host_occupied(void);
#endif
