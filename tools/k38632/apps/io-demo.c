#include "io.h"
#include "libc.h"
extern volatile unsigned practical_stats[16];
static unsigned char data[70001],readback[4098];
static unsigned hash(const unsigned char *p,unsigned n){unsigned h=2166136261u;for(unsigned i=0;i<n;i++)h=(h^p[i])*16777619u;return h;}
#define BOUND(expression) do{practical_stats[14]++;if(!(expression)){practical_stats[15]=practical_stats[14];return 50;}}while(0)
int practical_io(const char *mode){
 if(mode[0]=='b'){
  K32Io r=k32_io_name(K32_IO_STAT,"BOUND.BIN");
  if(!(k32_query()&K32_FEATURE_IO)){BOUND(k32_io(&r)==-38);practical_stats[1]=4;return 0;}
  static const K32Io ro={.version=1,.size=sizeof(K32Io),.operation=K32_IO_STAT,.name="BOUND.BIN"};
  BOUND(k32_call(18,0x10000,sizeof r)==-14);BOUND(k32_call(18,(unsigned)&ro,sizeof ro)==-14);BOUND(k32_call(18,K32_BOOT_ADDRESS-16,sizeof r)==-14);BOUND(k32_call(18,~0u-15,sizeof r)==-14);
  r.operation=K32_IO_READ;r.token=123;r.data=(unsigned)&ro;r.bytes=1;BOUND(k32_io(&r)==-14);
  r.operation=K32_IO_WRITE;r.data=0x10000;BOUND(k32_io(&r)==-14);r.data=(unsigned)data;r.bytes=4097;BOUND(k32_io(&r)==-22);
  r.bytes=0;BOUND(k32_io(&r)==-9);r.operation=K32_IO_STAT;k32_memcpy(r.name,"../BOUND",9);BOUND(k32_io(&r)==-22);
  r=k32_io_name(K32_IO_STAT,"BOUND.BIN");r.reserved=1;BOUND(k32_io(&r)==-22);r.reserved=0;r.version=2;BOUND(k32_io(&r)==-22);r.version=1;r.size--;BOUND(k32_io(&r)==-22);
  BOUND(k32_call(19,0,0)==-38);practical_stats[1]=4;k32_printf("PRACT32 IO BOUNDARIES PASS: %u\r\n",practical_stats[14]);return 0;
 }
 if(mode[0]=='c'){
  K32Revision missing={0};K32Io tx;int e=k32_io_begin("CANCEL.BIN",&missing,&tx);if(e)return 31;
  if(k32_io_write(&tx,"recoverable bytes",17)!=17)return 32;
  practical_stats[1]=3;for(;;)k32_wait(K32_WAIT_KEY|K32_WAIT_TIME,1000);
 }
 if(mode[0]=='r'){
  K32Revision old;if(k32_io_revision("CANCEL.BIN",1,&old)||!old.exists)return 33;
  if(k32_io_recover("CANCEL.BIN",1,"RESCUE.BIN",&old))return 34;
  practical_stats[12]=old.length;practical_stats[13]=old.hash;practical_stats[1]=4;k32_printf("PRACT32 RECOVERY PASS\r\n");return 0;
 }
 for(unsigned i=0;i<sizeof data;i++)data[i]=(unsigned char)(i*37+11);
 K32Revision old;int e=k32_io_revision("STREAM.BIN",0,&old);if(e)return 35;
 if(mode[0]=='v'){if(!old.exists||old.length!=sizeof data||old.hash!=hash(data,sizeof data))return 44;practical_stats[9]=old.length;practical_stats[10]=old.hash;practical_stats[11]=2;practical_stats[1]=4;k32_printf("PRACT32 COLD READ PASS\r\n");return 0;}
 if(k32_io_save("STREAM.BIN",data,sizeof data,&old))return 36;
 K32Revision got;if(k32_io_revision("STREAM.BIN",0,&got)||!got.exists||got.length!=sizeof data||got.hash!=hash(data,sizeof data))return 37;
 unsigned bytes;int h=k32_io_open("STREAM.BIN",0,&bytes);if(h<0||bytes!=sizeof data)return 38;
 if(k32_io_seek((unsigned)h,65535))return 39;
 k32_memset(readback,0xa5,sizeof readback);if(k32_io_read((unsigned)h,readback+1,4095)!=4095||readback[0]!=0xa5||readback[4096]!=0xa5||k32_memcmp(readback+1,data+65535,4095)||k32_io_close((unsigned)h))return 40;
 K32Io tx;if(k32_io_begin("STREAM.BIN",&got,&tx))return 41;
 if(k32_io_write(&tx,"new",3)!=3)return 42;
 tx.hash^=1; /* Request fields cannot replace the broker's captured revision. */
 if(k32_io_abort(&tx,0))return 43;
 practical_stats[9]=sizeof data;practical_stats[10]=got.hash;practical_stats[11]=1;practical_stats[1]=4;k32_printf("PRACT32 IO PASS: %u bytes hash=%x\r\n",got.length,got.hash);return 0;
}
