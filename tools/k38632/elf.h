#ifndef K32_ELF_H
#define K32_ELF_H
#ifdef K32_HOST
#define K32_STATE __attribute__((section(".task.data")))
#else
#define K32_STATE
#endif
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned u32;
#define K32_BASE 0x800000u
#define K32_LIMIT 0xbfd000u
#define K32_STACK 0xbff000u
typedef struct {u32 offset,address,filesz,memsz,flags;} K32Segment;
typedef struct {u32 entry,elf_offset,elf_size,count,rwbase,rwbytes;K32Segment segment[8];u32 abi,features;} K32Plan;
typedef int (*K32Read)(void *,u32,void *,u32);
int k32_plan(K32Read read,void *opaque,u32 bytes,K32Plan *out);
#endif
