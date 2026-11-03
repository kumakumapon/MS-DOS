#ifndef K32_HOST_H
#define K32_HOST_H
#include "elf.h"
#include "../k386/abi.h"
/* A real-mode, bounded-slice service. The legacy K386 mailbox is unchanged. */
enum {K32_HOST_CREATE=1,K32_HOST_STEP,K32_HOST_KEY,K32_HOST_STOP,K32_HOST_RESUME,K32_HOST_END,K32_HOST_POLL};
typedef struct {K386View view;u32 generation;char command[128];u8 record[1024];} K32HostView;
typedef struct {
 char signature[8];u16 version,size,ready,error,command,session,key,busy,owner,reserved;
 u32 cookie;char path[128];K32HostView task[2];u8 failure[128];
} K32HostMailbox;
#endif
