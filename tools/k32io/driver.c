/* Serialized DOS broker. User pointers/authority are checked by K32RUN/HOST.
 * Each call performs at most one 4 KiB data read/write, never a whole-file scan. */
#include "../k38632/io.h"
#include "../k38632/boot-settings.h"
#include "../k38632/services.h"
#include "../filer/verified-disk.h"
#include "../window/hardware.h"
extern unsigned short io_sender_segment,io_sender_offset;
extern unsigned char __io_stack_guard[];
extern unsigned io_old_vector;extern void io_api(void);
#define DOS_PATH 64
#define WORK_PATH_BYTES (8+K32_FS_PATH)
/* DOS handles are below the broker PSP's 96-handle ceiling. */
typedef struct __attribute__((packed)) {unsigned token,bytes;unsigned char h,lock;} File;
typedef struct {
 unsigned token,length,limit,old_length,old_hash,hash,scanned,reader;
 short writer;unsigned char phase,exists,kind,lock[2];
 char target[WORK_PATH_BYTES],destination[21];
} Transaction;
typedef struct {unsigned owner,epoch,cursor;File file[8];Transaction tx;unsigned char drive,list_kind,dta[43];} Owner;
#define LOCK_PATH WORK_PATH_BYTES
static Owner owners[8];static char locks[16][LOCK_PATH];
static unsigned self,psp,serial,indos_segment,indos_offset;
static unsigned char buffer[4096];
typedef struct {Owner *owner;char source[WORK_PATH_BYTES],target[WORK_PATH_BYTES];unsigned move;} FolderCopy;
static FolderCopy tree;
volatile struct {char signature[8];unsigned short version,size,ready,busy;unsigned owner,calls,rejected,stack_ok;struct {unsigned owner,epoch,handles,phase,bytes;} task[8];} io_record={.signature={'K','3','2','I','O','V','0','1'},.version=1,.size=sizeof io_record};
static void copy(void *d,const void *s,unsigned n){unsigned char *a=d;const unsigned char *b=s;for(unsigned i=0;i<n;i++)a[i]=b[i];}
static int same(const char *a,const char *b){for(unsigned i=0;;i++){if(a[i]!=b[i])return 0;if(!a[i])return 1;}}
#ifndef K32IO_TEST
static int attributes(const char *p){unsigned a=0x4300,c=0;__asm__ volatile("int $0x21; movzwl %%ax,%%eax; jnc 1f; negl %%eax; jmp 2f; 1: movl %%ecx,%%eax; 2:":"+a"(a),"+c"(c):"d"((unsigned)p):"cc","memory");return (int)a;}
static int close_handle(int h){unsigned a=0x3e00;__asm__ volatile("int $0x21; jnc 1f; movl $-1,%%eax; 1:":"+a"(a):"b"(h):"cc","memory");return a==~0u?-5:0;}
static int rename_path(const char *a,const char *b){unsigned ax=0x5600;__asm__ volatile("pushw %%es; pushw %%ds; popw %%es; int $0x21; popw %%es; jnc 1f; movl $-1,%%eax; 1:":"+a"(ax):"d"((unsigned)a),"D"((unsigned)b):"cc","memory");return ax==~0u?-5:0;}
static int remove_path(const char *p){unsigned a=0x4100;__asm__ volatile("int $0x21; jnc 1f; movl $-1,%%eax; 1:":"+a"(a):"d"((unsigned)p):"cc","memory");return a==~0u?-5:0;}
static int open_path(const char *p,int create){
 unsigned a=create?0x5b00:0x3d00;
 __asm__ volatile("int $0x21; movzwl %%ax,%%eax; jnc 1f; negl %%eax; 1:":"+a"(a):"d"((unsigned)p),"c"(0):"cc","memory");
 return (int)a<0?((int)a==-80?-17:(int)a==-2?-2:-5):(int)a;
}
static int seek_handle(int h,unsigned at){unsigned a=0x4200,d=at&65535;__asm__ volatile("int $0x21; jnc 1f; movl $-1,%%eax; 1:":"+a"(a),"+d"(d):"b"(h),"c"(at>>16):"cc","memory");return a==~0u||((d&65535)<<16|(a&65535))!=at?-5:0;}
static int size_handle(int h,unsigned *size){unsigned a=0x4202,d=0;__asm__ volatile("int $0x21; jnc 1f; movl $-1,%%eax; 1:":"+a"(a),"+d"(d):"b"(h),"c"(0):"cc","memory");if(a==~0u)return -5;*size=(d&65535)<<16|(a&65535);return seek_handle(h,0);}
static int data_io(int h,unsigned n,int write){unsigned a=write?0x4000:0x3f00;__asm__ volatile("int $0x21; jnc 1f; movl $-1,%%eax; 1:":"+a"(a):"b"(h),"c"(n),"d"((unsigned)buffer):"cc","memory");return a==~0u?-5:(int)(a&65535);}
#else
static int attributes(const char *);
static int close_handle(int);
static int rename_path(const char *,const char *);
static int remove_path(const char *);
static int open_path(const char *,int);
static int seek_handle(int,unsigned);
static int size_handle(int,unsigned *);
static int data_io(int,unsigned,int);
#endif
static unsigned handles(const Owner *o){unsigned n=o->tx.phase&&o->tx.writer>=0;for(unsigned i=0;i<8;i++)n+=o->file[i].token!=0;return n;}
static unsigned token(void){if(serial>=0x07ffffff)return 0;return ++serial<<4|1;}
static int file_index(Owner *o,unsigned value){for(unsigned i=0;i<8;i++)if(value&&o->file[i].token==value)return (int)i;return -9;}
static int file_close(Owner *o,unsigned value){
 int i=file_index(o,value);if(i<0)return i;
 File *f=&o->file[i];int e=close_handle(f->h);
 if(!e){if(f->lock)locks[f->lock-1][0]=0;*f=(File){0};}
 return e;
}
static int file_open(Owner *o,const char *path){
 unsigned i=0;while(i<8&&o->file[i].token)i++;if(i==8)return -24;
 int a=attributes(path);if(a==-2)return -2;if(a<0)return -5;if(a&0x5e)return -13;
 unsigned value=token();if(!value)return -75;
 int h=open_path(path,0);if(h<0)return h;
 /* Publish ownership before size/seek so failed close cannot leak authority. */
 File *f=&o->file[i];*f=(File){.h=h,.token=value};unsigned bytes=0;int e=size_handle(h,&bytes);if(!e&&bytes>K32_IO_LIMIT)e=-27;f->bytes=bytes;
 if(e){if(file_close(o,f->token))return -117;return e;}return (int)f->token;
}
static int lock_path(const char *path){
 unsigned n=0;while(n<LOCK_PATH&&path[n])n++;if(n==LOCK_PATH)return -22;
 for(unsigned i=0;i<16;i++)if(locks[i][0]&&same(locks[i],path))return -16;
 for(unsigned i=0;i<16;i++)if(!locks[i][0]){for(unsigned n=0;;n++){locks[i][n]=path[n];if(!path[n])break;}return (int)i+1;}
 return -24;
}
static void unlock(Transaction *t){for(unsigned i=0;i<2;i++)if(t->lock[i])locks[t->lock[i]-1][0]=0;}
static void sibling(const char *,const char *,char [DOS_PATH]);
static int abort_tx(Owner *o,int keep){
 Transaction *t=&o->tx;if(!t->phase)return 0;int e=0;
 if(t->writer>=0){e=close_handle(t->writer);if(!e)t->writer=-1;}
 if(t->reader){int r=file_close(o,t->reader);if(!r)t->reader=0;if(!e)e=r;}
 if(e)return -117;
 if(!keep&&!t->kind&&remove_path(locks[t->lock[0]-1]))return -117;
 unlock(t);*t=(Transaction){0};return 0;
}
static int cleanup(Owner *o){
 int e=abort_tx(o,1);for(unsigned i=0;i<8;i++)if(o->file[i].token&&file_close(o,o->file[i].token))e=-117;
 if(!e){if(tree.owner==o)tree.owner=0;*o=(Owner){0};}
 return e;
}
static int ready(Owner *o){
#ifdef K32IO_TEST
 (void)o;return 0;
#else
 char root[]="A:\\WORK";root[0]='A'+o->drive;int a=attributes(root);
 if(a==-2&&fd_hdd_verified(o->drive)){unsigned ax=0x3900;__asm__ volatile("int $0x21; jnc 1f; movl $-1,%%eax; 1:":"+a"(ax):"d"((unsigned)root):"cc","memory");if(ax==~0u)return -5;a=attributes(root);}
 return a<0?-5:!(a&16)?-20:a&0x4f?-13:0;
#endif
}
static int begin_paths(Owner *o,Transaction *next,int creation){
 if(o->tx.phase)return -16;
 next->token=token();if(!next->token)return -75;
 next->phase=1;next->writer=-1;
 char part[DOS_PATH];sibling(next->target,"TMP",part);
 int lock=lock_path(part);if(lock<0)return lock;next->lock[0]=lock;
 int e;
 if(next->kind){
  int a=attributes(next->target);if(a<0||(a&0x5f)){e=a<0?-2:-13;goto failed;}
  char destlock[24];e=k32_io_path(o->drive,next->destination,1,destlock);if(e)goto failed;
  char dest[24];e=k32_io_path(o->drive,next->destination,0,dest);if(e)goto failed;
  copy(next->destination,dest,sizeof next->destination);
  lock=lock_path(destlock);if(lock<0){e=lock;goto failed;}next->lock[1]=lock;
  a=attributes(next->destination);if(a!=-2){e=a<0?-5:-17;goto failed;}
  int h=file_open(o,next->target);if(h<0){e=h;goto failed;}
  next->reader=(unsigned)h;next->phase=2;next->hash=2166136261u;
 }else{
  int a=attributes(next->target);
  if(a!=-2&&(a<0||(a&0x5f)||creation)){e=a<0?-5:creation?-17:-13;goto failed;}
  if(a==-2&&next->exists){e=-116;goto failed;}
  sibling(next->target,"BAK",part);a=attributes(part);if(a!=-2){e=a<0?-5:-117;goto failed;}
  int h=open_path(locks[next->lock[0]-1],1);if(h<0){e=h;goto failed;}next->writer=h;
 }
 o->tx=*next;return 0;
 failed:unlock(next);return e;
}
static int begin(Owner *o,K32Io *r,int recovery){
 Transaction next={.limit=K32_IO_LIMIT,.exists=r->exists,.old_length=r->length,.old_hash=r->hash,.kind=recovery};
 int e=k32_io_path(o->drive,r->name,recovery?r->flags:0,next.target);if(e)return e;
 if(recovery)copy(next.destination,buffer,r->bytes);
 e=begin_paths(o,&next,0);if(!e)r->token=o->tx.token;return e;
}
static int advance(Owner *o){
 Transaction *t=&o->tx;
 if(t->phase>3)return -117;
 if(t->phase==1){
  int e=close_handle(t->writer);if(e)return -117;t->writer=-1;t->phase=2;t->hash=2166136261u;
  int h=file_open(o,t->target);
  if(h==-2){if(t->exists)return -116;t->phase=3;}
  else if(h<0)return h;
  else {t->reader=(unsigned)h;if(!t->exists)return -116;}
 }
 if(t->phase==2){
  int i=file_index(o,t->reader);if(i<0)return i;int n=data_io(o->file[i].h,4096,0);if(n<0)return n;
  if(n>4096||(unsigned)n>K32_IO_LIMIT-t->scanned)return -27;
  t->scanned+=(unsigned)n;for(int j=0;j<n;j++)t->hash=(t->hash^buffer[j])*16777619u;
  if(n==4096)return -11;
  int e=file_close(o,t->reader);if(e)return -117;t->reader=0;
  if(t->scanned!=t->old_length||t->hash!=t->old_hash)return -116;
  t->phase=3;
 }
 int e;
 if(t->kind){int a=attributes(t->destination);if(a!=-2)return a<0?-5:-17;e=rename_path(t->target,t->destination);}
 else {char backup[DOS_PATH];sibling(t->target,"BAK",backup);const K32IoPublish io={rename_path,remove_path};e=k32_io_publish(t->target,locks[t->lock[0]-1],backup,t->exists,&io);}
 if(e>=0){unlock(t);*t=(Transaction){0};}return e;
}
#include "filesystem.inc"
static int dispatch(Owner *o,K32Io *r){
 if(r->operation==K32_IO_FS){
  if(r->version!=1||r->size!=sizeof *r||r->flags||r->reserved||r->bytes!=sizeof(K32Fs))return -22;
  K32Fs fs;copy(&fs,buffer,sizeof fs);
  int e=filesystem(o,&fs);if(e<0)return e;
  copy(buffer,&fs,sizeof fs);return sizeof fs;
 }
 int e=k32_io_validate(r);if(e)return e;
 if(r->operation<=K32_IO_OPEN||r->operation==K32_IO_BEGIN||r->operation==K32_IO_RECOVER){e=ready(o);if(e)return e;}
 Transaction *t=&o->tx;char path[24];
 if(r->operation==K32_IO_STAT||r->operation==K32_IO_OPEN){
  k32_io_path(o->drive,r->name,r->flags,path);int h=file_open(o,path);
  if(r->operation==K32_IO_STAT){r->exists=0;r->length=r->hash=0;if(h==-2)return 0;if(h<0)return h;r->exists=1;r->length=o->file[file_index(o,(unsigned)h)].bytes;return file_close(o,(unsigned)h);}
  if(h<0)return h;
  r->token=(unsigned)h;r->length=o->file[file_index(o,r->token)].bytes;return 0;
 }
 if(r->operation==K32_IO_BEGIN)return begin(o,r,0);
 if(r->operation==K32_IO_RECOVER){if(!r->token){e=begin(o,r,1);if(e)return e;}else if(r->token!=t->token||!t->kind)return -9;return advance(o);}
 if(r->operation>=K32_IO_WRITE&&r->operation<=K32_IO_ABORT){
  if(!t->phase||!r->token||r->token!=t->token||(t->kind&&r->operation!=K32_IO_ABORT))return -9;
  if(r->operation==K32_IO_ABORT)return abort_tx(o,r->flags);
  if(r->operation==K32_IO_COMMIT)return advance(o);
  if(t->phase!=1)return -16;
  if(r->bytes>t->limit-t->length)return -27;
  if(!r->bytes)return 0;
  int n=data_io(t->writer,r->bytes,1);if(n<0)return n;if((unsigned)n!=r->bytes){t->phase=4;return -28;}t->length+=r->bytes;return n;
 }
 int i=file_index(o,r->token);if(i<0)return i;File *f=&o->file[i];
 if(r->operation==K32_IO_CLOSE)return file_close(o,r->token);
 if(r->operation==K32_IO_SEEK){if(r->offset>f->bytes)return -22;return seek_handle(f->h,r->offset);}
 if(r->operation==K32_IO_READ){if(!r->bytes)return 0;int n=data_io(f->h,r->bytes,0);if(n>4096||(n>=0&&(unsigned)n>r->bytes))return -5;return n;}
 return -38;
}
#ifndef K32IO_TEST
void io_dispatch(void){
 if(io_sender_offset>65536-sizeof(K32IoPacket))return;
 K32IoPacket p;mem_copy(self,(unsigned)&p,io_sender_segment,io_sender_offset,sizeof p);p.result=-22;
 unsigned short msw;__asm__ volatile("smsw %0":"=r"(msw));
 if(p.version!=1||p.size!=sizeof p||(msw&1)||mem_get(indos_segment,indos_offset)||p.owner<0x100000||p.owner>=0x2000000||(p.owner&4095)||!p.epoch||p.operation<1||p.operation>3)goto done;
 for(unsigned i=0;i<16;i++)if(mem_get(self,(unsigned)__io_stack_guard+i)!=0xa5)io_record.stack_ok=0;
 if(!io_record.stack_ok){p.result=-75;goto done;}
 io_record.calls++;io_record.busy=1;
 unsigned previous,a=0x6200;__asm__ volatile("int $0x21":"+a"(a),"=b"(previous)::"cc","memory");a=0x5000;__asm__ volatile("int $0x21":"+a"(a):"b"(psp):"cc","memory");
 unsigned old=fd_disk_critical(1,0);Owner *o=0;unsigned slot=0;
 for(unsigned i=0;i<8;i++)if(owners[i].owner==p.owner&&owners[i].epoch==p.epoch){o=&owners[i];slot=i;break;}
 if(p.operation==2){p.result=o?cleanup(o):0;goto restore;}
 if(p.request>65536-sizeof(K32Io)||p.buffer>65536-sizeof buffer)goto restore;
 if(p.operation==3){K32Io query;char path[24];mem_copy(self,(unsigned)&query,io_sender_segment,p.request,sizeof query);p.result=k32_io_path(query.offset,query.name,1,path);if(!p.result)for(unsigned i=0;i<16;i++)if(locks[i][0]&&same(locks[i],path)){p.result=-16;break;}goto restore;}
 if(!o){for(unsigned i=0;i<8;i++)if(!owners[i].owner){o=&owners[i];slot=i;*o=(Owner){.owner=p.owner,.epoch=p.epoch};for(unsigned drive=0;drive<26;drive++)if(fd_hdd_verified(drive)){o->drive=drive;break;}break;}}
 if(!o){p.result=-24;goto restore;}
 K32Io r;mem_copy(self,(unsigned)&r,io_sender_segment,p.request,sizeof r);
 if((r.operation==K32_IO_WRITE||r.operation==K32_IO_RECOVER||r.operation==K32_IO_FS)&&r.bytes<=4096)mem_copy(self,(unsigned)buffer,io_sender_segment,p.buffer,r.bytes);
 if((r.operation==K32_IO_OPEN||r.operation==K32_IO_BEGIN)&&same(r.name,KG_LAYOUT_FILE)&&!fd_hdd_verified(o->drive))p.result=-13;
 else p.result=dispatch(o,&r);
 if((r.operation==K32_IO_READ||r.operation==K32_IO_FS)&&p.result>0)mem_copy(io_sender_segment,p.buffer,self,(unsigned)buffer,(unsigned)p.result);
 mem_copy(io_sender_segment,p.request,self,(unsigned)&r,sizeof r);
 restore:
 p.handles=o?handles(o):0;if(o){io_record.task[slot].owner=o->owner;io_record.task[slot].epoch=o->epoch;io_record.task[slot].handles=p.handles;io_record.task[slot].phase=o->tx.phase;io_record.task[slot].bytes=o->tx.length;}
 fd_disk_critical(0,old);a=0x5000;__asm__ volatile("int $0x21":"+a"(a):"b"(previous):"cc","memory");io_record.busy=0;
 done:if(p.result<0&&p.result!=-11)io_record.rejected++;mem_copy(io_sender_segment,io_sender_offset,self,(unsigned)&p,sizeof p);
}
int io_install(void){
 unsigned a=0xd39f,b=0;__asm__ volatile("pushw %%es; int $0x2f; popw %%es":"+a"(a):"b"(b):"ecx","edx","esi","edi","cc","memory");if((a&65535)==0x39fd)return 2;
 __asm__ volatile("movw %%ds,%w0":"=r"(self));a=0x6200;__asm__ volatile("int $0x21":"+a"(a),"=b"(psp)::"cc","memory");
 a=0x6700;__asm__ volatile("int $0x21; jnc 1f; movl $-1,%%eax; 1:":"+a"(a):"b"(96):"cc","memory");if(a==~0u)return 0;
 unsigned short seg;unsigned off;a=0x3400;__asm__ volatile("pushw %%es; int $0x21; movw %%es,%1; popw %%es":"+a"(a),"=c"(seg),"=b"(off)::"cc","memory");indos_segment=seg;indos_offset=off&65535;
 a=0x352f;__asm__ volatile("pushw %%es; int $0x21; movw %%es,%1; popw %%es":"+a"(a),"=c"(seg),"=b"(off)::"cc","memory");io_old_vector=(unsigned)seg<<16|(off&65535);
 a=0x252f;__asm__ volatile("int $0x21":"+a"(a):"d"((unsigned)io_api):"ebx","ecx","esi","edi","cc","memory");
 for(unsigned i=0;i<16;i++)mem_put(self,(unsigned)__io_stack_guard+i,0xa5);
 io_record.owner=self;io_record.ready=io_record.stack_ok=1;return 1;
}
#endif
