/* Opt-in DOS block driver, no formatting, IRQ hooks or changes to the kernel. */
#include "policy.h"
extern unsigned resident_end,khd_sense(void),khd_bios(unsigned,unsigned,unsigned);
extern void khd_copy(unsigned,unsigned,unsigned);
volatile KhdRecord khd_record __attribute__((section(".record"),aligned(1)))={.signature={'K','H','D','D','I','O','P','1'},.version=1,.size=64};
u32 khd_request;u16 khd_bounce;
static u8 dma_raw[4096],partition[512],boot[2048];
static u32 local(const void *p){u16 ds;__asm__ volatile("movw %%ds,%0":"=r"(ds));return (u32)ds*16+(u32)p;}
static u8 byte(u32 p){u8 c;__asm__ volatile("pushw %%es; movw %w1,%%es; movb %%es:(%2),%0; popw %%es":"=q"(c):"r"(p>>4),"r"(p&15):"memory");return c;}
static void set(u32 p,u8 c){__asm__ volatile("pushw %%es; movw %w0,%%es; movb %b2,%%es:(%1); popw %%es"::"r"(p>>4),"r"(p&15),"q"(c):"memory");}
static u16 word(unsigned n){return byte(khd_request+n)|(u16)byte(khd_request+n+1)<<8;}
static void w(unsigned n,u16 x){set(khd_request+n,x);set(khd_request+n+1,x>>8);}
static void farptr(unsigned at,u32 p){w(at,p&15);w(at+2,p>>4);}
static int io(unsigned op,unsigned sector,unsigned bytes){unsigned e=khd_bios(op,sector,bytes);khd_record.last_bios=e;if(e>=0x20){khd_record.bios_errors++;return 1;}return 0;}
static int initialize(void){
 khd_bounce=(local(dma_raw)+2047)/2048*128;
 if(khd_sense())return 1;
 if(io(0x0600,0,256))return 2;
 khd_copy(local(boot),(u32)khd_bounce*16,256);
 if(boot[4]!='I'||boot[5]!='P'||boot[6]!='L'||boot[7]!='1'||boot[254]!=0x55||boot[255]!=0xaa)return 3;
 if(io(0x0600,1,512))return 4;
 khd_copy(local(partition),(u32)khd_bounce*16,512);
 if(io(0x0600,KHD_START,KHD_BYTES))return 5;
 khd_copy(local(boot),(u32)khd_bounce*16,KHD_BYTES);
 KhdPlan p;if(khd_validate(partition,boot,&p))return 6;
 if(io(0x0600,KHD_PHYSICAL-1,256))return 7;
 khd_record.sectors=p.sectors;khd_record.start=p.start;khd_record.physical=KHD_PHYSICAL;
 khd_record.ready=1;return 0;
}
void khd_dispatch(void){
 u16 status=0x100;unsigned cmd=byte(khd_request+2);khd_record.requests++;
 if(byte(khd_request)<13){w(3,0x8105);return;}
 if(cmd==0){
  if(byte(khd_request)<23){w(3,0x8105);return;}
  int e=initialize();khd_record.error=e;
  set(khd_request+13,e?0:1);farptr(14,e?local((void *)0):local(&resident_end));
  if(!e){static u16 list;list=(u32)boot+11;w(18,(u32)&list);w(20,local((void *)0)>>4);khd_record.drive=byte(khd_request+22);}
  w(3,status);return;
 }
 if(byte(khd_request+1)){if(byte(khd_request)>=22&&(cmd==4||cmd==8||cmd==9))w(18,0);w(3,0x8101);return;}
 if(!khd_record.ready){w(3,0x8102);return;}
 if(cmd==1){if(byte(khd_request)<15)status=0x8105;else set(khd_request+14,1);} /* fixed, unchanged media */
 else if(cmd==2){if(byte(khd_request)<22)status=0x8105;else farptr(18,local(boot+11));}
 else if(cmd==4||cmd==8||cmd==9){
  if(byte(khd_request)<22){w(3,0x8105);return;}
  u32 start=word(20),count=word(18),buffer=(u32)word(16)*16+word(14);w(18,0);
  if(khd_range(start,count,buffer)){khd_record.rejected++;w(3,0x810b);return;}
  unsigned done=0;
  for(;done<count;done++){
   if(cmd!=4)khd_copy((u32)khd_bounce*16,buffer+done*KHD_BYTES,KHD_BYTES);
   if(io(cmd==4?0x0600:0x0500,KHD_START+(start+done)*4,KHD_BYTES)){status=0x810b;khd_record.partial++;break;}
   if(cmd==9){
    int mismatch=io(0x0600,KHD_START+(start+done)*4,KHD_BYTES);
    for(unsigned i=0;!mismatch&&i<KHD_BYTES;i++)if(byte((u32)khd_bounce*16+i)!=byte(buffer+done*KHD_BYTES+i))mismatch=1;
    if(mismatch){status=0x8104;khd_record.partial++;break;}
   }
   if(cmd==4){khd_copy(buffer+done*KHD_BYTES,(u32)khd_bounce*16,KHD_BYTES);khd_record.reads++;}else khd_record.writes++;
   khd_record.completed++;
  }w(18,done);
 }else if(cmd!=10&&cmd!=11)status=0x8103;
 w(3,status);
}
