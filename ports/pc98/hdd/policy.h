#ifndef KHD_POLICY_H
#define KHD_POLICY_H
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned u32;
enum {KHD_PHYSICAL=162360,KHD_START=264,KHD_SECTORS=40524,KHD_BYTES=1024};
typedef struct {u32 start,sectors,bytes_per_sector,sectors_per_cluster,clusters;} KhdPlan;
typedef struct {
 char signature[8];u16 version,size,ready,error;
 u32 sectors,start,physical,requests,reads,writes,rejected,bios_errors;
 u16 last_bios,drive;u32 completed,partial;u8 reserved[4];
} KhdRecord;
_Static_assert(sizeof(KhdRecord)==64,"HDD native record");
int khd_validate(const u8 *,const u8 *,KhdPlan *);
int khd_range(u32 start,u32 count,u32 buffer);
#endif
