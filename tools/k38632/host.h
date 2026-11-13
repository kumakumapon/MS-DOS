#ifndef K32_HOST_H
#define K32_HOST_H
#include "elf.h"
#include "../k386/abi.h"
/* A real-mode, bounded-slice service. The legacy K386 mailbox is unchanged. */
enum {K32_HOST_CREATE=1,K32_HOST_STEP,K32_HOST_KEY,K32_HOST_STOP,K32_HOST_RESUME,K32_HOST_END,K32_HOST_POLL};
typedef struct {K386View view;u32 generation;char command[128];u8 record[1024];} K32HostView;
typedef struct {
 char signature[8];u16 version,size,ready,error,command,session,key,busy,owner,reserved;
 u32 cookie;char path[128];K32HostView task[2];u8 failure[1024];
} K32HostMailbox;
typedef struct {u16 state,tables;u32 generation;} K32HostSlot;
extern unsigned host_slot_bias;
int host_operate(unsigned,unsigned);
int host_bank_capacity(unsigned);
u32 host_task_address(unsigned);
void host_task_read(unsigned,unsigned,void *,unsigned);
#endif
