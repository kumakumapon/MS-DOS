/* Execute the actual transaction engine against an independent faultable disk. */
#define K32IO_TEST
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "driver.c"
typedef struct {char name[DOS_PATH];unsigned char *data;unsigned bytes,position,attr;int opened;} MockFile;
static MockFile disk[24];static unsigned fail_rename,renames,short_write,fail_close,rename_mask,fail_remove;
static int find(const char *p){for(unsigned i=0;i<24;i++)if(!strcmp(disk[i].name,p))return (int)i;return -1;}
static int attributes(const char *p){int i=find(p);return i<0?-2:(int)disk[i].attr;}
static int change_dir(const char *p,unsigned remove){
 int i=find(p);
 if(remove){
  if(i<0||!(disk[i].attr&16))return -5;
  unsigned n=strlen(p);
  for(unsigned j=0;j<24;j++)if(!strncmp(p,disk[j].name,n)&&disk[j].name[n]=='\\')return -5;
  disk[i]=(MockFile){0};return 0;
 }
 if(i>=0)return -17;
 for(i=0;i<24&&disk[i].name[0];i++);
 assert(i<24);strcpy(disk[i].name,p);disk[i].attr=16;return 0;
}
static int set_dir_attributes(const char *p,unsigned attrs){int i=find(p);if(i<0)return -5;disk[i].attr=16|attrs;return 0;}
static int hide_dir(const char *p){int i=find(p);if(i<0)return -5;disk[i].attr|=2;return 0;}
static int find_entry(const char *p,unsigned char *dta,int next){(void)p;(void)dta;(void)next;return -18;}
static int close_handle(int h){if(fail_close){fail_close--;return -5;}assert(h>=0&&h<24&&disk[h].opened);disk[h].opened=0;return 0;}
static int rename_path(const char *a,const char *b){renames++;if(renames==fail_rename||(rename_mask&(1u<<renames)))return -5;int i=find(a);if(i<0||find(b)>=0)return -5;unsigned n=strlen(a);
 for(unsigned j=0;j<24;j++)if(!strncmp(a,disk[j].name,n)&&disk[j].name[n]=='\\'){
  char path[DOS_PATH];strcpy(path,b);strcat(path,disk[j].name+n);strcpy(disk[j].name,path);
 }
 strcpy(disk[i].name,b);return 0;}
static int remove_path(const char *p){if(fail_remove)return -5;int i=find(p);if(i<0||disk[i].opened)return -5;free(disk[i].data);disk[i]=(MockFile){0};return 0;}
static int open_path(const char *p,int create){int i=find(p);if(create&&i>=0)return -17;if(!create&&i<0)return -2;if(i<0){for(i=0;i<24&&disk[i].name[0];i++);assert(i<24);strcpy(disk[i].name,p);disk[i].data=calloc(1,K32_IO_LIMIT);}assert(!disk[i].opened);disk[i].opened=1;disk[i].position=0;return i;}
static int seek_handle(int h,unsigned at){assert(at<=disk[h].bytes);disk[h].position=at;return 0;}
static int size_handle(int h,unsigned *n){*n=disk[h].bytes;return 0;}
static int data_io(int h,unsigned n,int write){assert(n<=4096);MockFile *f=&disk[h];assert(f->opened);if(write){if(short_write){n/=2;short_write=0;}assert(n<=K32_IO_LIMIT-f->position);memcpy(f->data+f->position,buffer,n);if(f->bytes<f->position+n)f->bytes=f->position+n;}else{if(n>f->bytes-f->position)n=f->bytes-f->position;memcpy(buffer,f->data+f->position,n);}f->position+=n;return (int)n;}
static unsigned digest(const void *p,unsigned n){const unsigned char *s=p;unsigned h=2166136261u;for(unsigned i=0;i<n;i++)h=(h^s[i])*16777619u;return h;}
static K32Io request(unsigned op,const char *name){K32Io r={.version=1,.size=sizeof r,.operation=op};if(name)strcpy(r.name,name);return r;}
static int commit(Owner *o,K32Io *r){int e;r->operation=K32_IO_COMMIT;do{e=dispatch(o,r);}while(e==-11);return e;}
static void abort_request(Owner *o,K32Io *r,unsigned keep){r->operation=K32_IO_ABORT;r->flags=keep;assert(!dispatch(o,r));}
static void clear(void){for(unsigned i=0;i<24;i++){free(disk[i].data);disk[i]=(MockFile){0};}memset(owners,0,sizeof owners);memset(locks,0,sizeof locks);tree=(FolderCopy){0};serial=0;fail_rename=renames=short_write=fail_close=rename_mask=fail_remove=0;}
static void save_original(void){int h=open_path("A:\\WORK\\TEST.BIN",1);memcpy(buffer,"original",8);assert(data_io(h,8,1)==8);assert(!close_handle(h));}
static K32Fs fs_request(unsigned action,const char *path,const char *destination){
 K32Fs r={.version=1,.size=sizeof r,.action=action};
 strcpy(r.path,path);strcpy(r.destination,destination);
 return r;
}
static void test_management(Owner *o){
 K32Fs f=fs_request(K32_FS_MKDIR,"box","");
 assert(!filesystem(o,&f));
 f=fs_request(K32_FS_OPEN,"test.bin","");
 assert(!filesystem(o,&f)&&f.length==8);
 K32Fs move=fs_request(K32_FS_MOVE,"test.bin","box\\copy.bin");
 assert(filesystem(&owners[1],&move)==-16);
 assert(!file_close(o,f.token));
 assert(!filesystem(o,&move));
 assert(find("A:\\WORK\\TEST.BIN")<0&&find("A:\\WORK\\BOX\\COPY.BIN")>=0);
 f=fs_request(K32_FS_TRASH,"box","");
 assert(!filesystem(o,&f)&&attributes("A:\\WORK\\BOX")==22&&find("A:\\WORK\\BOX\\COPY.BIN")>=0);
 f=fs_request(K32_FS_RESTORE,"box","");
 assert(!filesystem(o,&f)&&attributes("A:\\WORK\\BOX")==16);
 f=fs_request(K32_FS_TRASH,"box\\copy.bin","");
 assert(!filesystem(o,&f));
 assert(attributes("A:\\WORK\\KTRASH")==18);
 f=fs_request(K32_FS_CREATE,"box\\copy.bin","");
 assert(!filesystem(o,&f));
 K32Io tx=request(K32_IO_COMMIT,0);tx.token=f.token;assert(!commit(o,&tx));
 f=fs_request(K32_FS_RESTORE,"box\\copy.bin","");
 assert(filesystem(o,&f)==-17);
 assert(remove_path("A:\\WORK\\BOX\\COPY.BIN")==0);
 assert(!filesystem(o,&f)&&find("A:\\WORK\\KTRASH\\BOX\\COPY.BIN")<0);
 int i=find("A:\\WORK\\BOX\\COPY.BIN");
 assert(i>=0&&!memcmp(disk[i].data,"original",8));
 const char *bad[]={"..\\BOOT.SYS","A:BOOT.SYS","BOX\\CON.TXT","BOX\\OLD.BAK","KTRASH\\COPY.BIN","A\\B\\C\\D"};
 for(unsigned n=0;n<sizeof bad/sizeof *bad;n++){
  f=fs_request(K32_FS_CREATE,bad[n],"");
  assert(filesystem(o,&f)==-22);
 }
 f=fs_request(K32_FS_BOOT_OPEN,"IO.SYS","");
 assert(filesystem(o,&f)==-22);
 const char original[]="FILES=20\r\nBUFFERS=8\r\nREM keep\r\n";
 const char updated[]="FILES=21\r\nBUFFERS=8\r\nREM keep\r\n";
 int h=open_path("A:\\CONFIG.SYS",1);memcpy(buffer,original,sizeof original-1);
 assert(data_io(h,sizeof original-1,1)==sizeof original-1&&!close_handle(h));
 f=fs_request(K32_FS_BOOT_WRITE,"CONFIG.SYS","");
 f.length=sizeof original-1;f.hash=digest(original,f.length)^1;
 f.flags=kg_settings_config_flags(&(KgBootSettings){.files=21,.buffers=8});
 assert(filesystem(o,&f)==-116&&find("A:\\CONFIG.TMP")<0);
 f.hash^=1;f.flags|=K32_BOOT_EMS;assert(filesystem(o,&f)==-22);
 f.flags&=~K32_BOOT_EMS;
 assert(!filesystem(o,&f)&&!f.token&&!f.flags);
 assert(!memcmp(disk[find("A:\\CONFIG.SYS")].data,updated,sizeof updated-1));
 assert(find("A:\\CONFIG.TMP")<0&&find("A:\\CONFIG.BAK")<0);
 f.hash=digest(updated,sizeof updated-1);f.flags=kg_settings_config_flags(&(KgBootSettings){.files=22,.buffers=8});
 fail_rename=renames+2;assert(filesystem(o,&f)==-5);
 assert(find("A:\\CONFIG.TMP")>=0&&!memcmp(disk[find("A:\\CONFIG.SYS")].data,updated,sizeof updated-1));
}
static void test_folders(void){
 clear();Owner *o=&owners[0];o->owner=1;owners[1].owner=2;
 K32Fs r=fs_request(K32_FS_MKDIR,"BOX","");
 assert(!filesystem(o,&r));
 r=fs_request(K32_FS_TREE_BEGIN,"BOX","BOX\\CHILD");
 assert(filesystem(o,&r)==-22);
 r=fs_request(K32_FS_TREE_BEGIN,"BOX","COPY");
 assert(!filesystem(o,&r)&&attributes("A:\\WORK\\COPY.TMP")==16);
 r=fs_request(K32_FS_MOVE,"BOX","OTHER");
 assert(filesystem(&owners[1],&r)==-16);
 r=fs_request(K32_FS_TREE_CREATE,"FILE.TXT","");
 assert(!filesystem(o,&r));
 K32Io write=request(K32_IO_WRITE,0);write.token=r.token;write.bytes=3;
 memcpy(buffer,"new",3);assert(dispatch(o,&write)==3&&!commit(o,&write));
 r=fs_request(K32_FS_TREE_PUBLISH,"","");
 assert(!filesystem(o,&r)&&find("A:\\WORK\\COPY\\FILE.TXT")>=0);
 assert(find("A:\\WORK\\COPY.TMP")<0&&attributes("A:\\WORK\\BOX")==16);
 r=fs_request(K32_FS_TREE_BEGIN,"COPY","MOVED");r.flags=K32_FS_TREE_MOVE;
 assert(!filesystem(o,&r));
 r=fs_request(K32_FS_TREE_PUBLISH,"","");fail_rename=renames+1;
 assert(filesystem(o,&r)<0&&attributes("A:\\WORK\\COPY")==16);
 assert(find("A:\\WORK\\COPY\\FILE.TXT")>=0&&attributes("A:\\WORK\\MOVED.TMP")==16);
 r=fs_request(K32_FS_TREE_CANCEL,"","");
 assert(!filesystem(o,&r)&&!tree.owner);
}
int main(void){
 test_folders();
 clear();
 char p[24];assert(!k32_io_path(1,"abc.bin",2,p)&&!strcmp(p,"B:\\WORK\\ABC.BAK"));
 const char *bad[]={"CON.BIN","../X.BIN","A:TEST.BIN","A.TMP","A.BAK","TOOLONGNAME.BIN","A"};for(unsigned i=0;i<sizeof bad/sizeof *bad;i++)assert(k32_io_path(0,bad[i],0,p)==-22);
 Owner *o=&owners[0];K32Io r=request(K32_IO_BEGIN,"TEST.BIN");assert(!dispatch(o,&r));unsigned token=r.token;
 K32Io other=request(K32_IO_BEGIN,"TEST.DIC");assert(dispatch(&owners[1],&other)==-16);other=request(K32_IO_WRITE,0);other.token=token;assert(dispatch(&owners[1],&other)==-9);
 for(unsigned at=0;at<70001;){unsigned n=70001-at;if(n>4096)n=4096;for(unsigned i=0;i<n;i++)buffer[i]=(unsigned char)((at+i)*37+11);r.operation=K32_IO_WRITE;r.bytes=n;assert(dispatch(o,&r)==(int)n);at+=n;}
 assert(!commit(o,&r));int i=find("A:\\WORK\\TEST.BIN");assert(i>=0&&disk[i].bytes==70001);for(unsigned j=0;j<70001;j++)assert(disk[i].data[j]==(unsigned char)(j*37+11));assert(!handles(o));
 clear();save_original();o=&owners[0];r=request(K32_IO_BEGIN,"TEST.BIN");r.exists=1;r.length=8;r.hash=digest("original",8)^1;assert(!dispatch(o,&r));r.operation=K32_IO_WRITE;r.bytes=3;memcpy(buffer,"new",3);assert(dispatch(o,&r)==3);assert(commit(o,&r)==-116);assert(!memcmp(disk[find("A:\\WORK\\TEST.BIN")].data,"original",8));abort_request(o,&r,1);assert(find("A:\\WORK\\TEST.TMP")>=0);
 K32Io recovery=request(K32_IO_RECOVER,"TEST.BIN");recovery.flags=1;recovery.exists=1;recovery.length=3;recovery.hash=digest("new",3);recovery.bytes=9;memcpy(buffer,"SAVED.BIN",9);assert(!dispatch(o,&recovery));assert(find("A:\\WORK\\TEST.TMP")==-1&&find("A:\\WORK\\SAVED.BIN")>=0);
 clear();save_original();o=&owners[0];r=request(K32_IO_BEGIN,"TEST.BIN");r.exists=1;r.length=8;r.hash=digest("original",8);assert(!dispatch(o,&r));r.operation=K32_IO_WRITE;r.bytes=10;short_write=1;assert(dispatch(o,&r)==-28);assert(commit(o,&r)==-117);assert(!cleanup(o));assert(find("A:\\WORK\\TEST.TMP")>=0&&!disk[find("A:\\WORK\\TEST.TMP")].opened);assert(!memcmp(disk[find("A:\\WORK\\TEST.BIN")].data,"original",8));
 for(unsigned failure=1;failure<=3;failure++){
  clear();save_original();o=&owners[0];r=request(K32_IO_BEGIN,"TEST.BIN");r.exists=1;r.length=8;r.hash=digest("original",8);assert(!dispatch(o,&r));r.operation=K32_IO_WRITE;r.bytes=3;memcpy(buffer,"new",3);assert(dispatch(o,&r)==3);fail_rename=failure;int e=commit(o,&r);
  if(failure<=2){assert(e==-5);assert(!memcmp(disk[find("A:\\WORK\\TEST.BIN")].data,"original",8));abort_request(o,&r,1);}else assert(!e);
  assert(!handles(o));
 }
 clear();o=&owners[0];r=request(K32_IO_BEGIN,"TEST.BIN");assert(!dispatch(o,&r));fail_close=1;r.operation=K32_IO_ABORT;assert(dispatch(o,&r)==-117&&handles(o)==1);assert(!cleanup(o)&&!handles(o));
 clear();save_original();o=&owners[0];r=request(K32_IO_BEGIN,"TEST.BIN");r.exists=1;r.length=8;r.hash=digest("original",8);assert(!dispatch(o,&r));r.operation=K32_IO_WRITE;r.bytes=3;memcpy(buffer,"new",3);assert(dispatch(o,&r)==3);rename_mask=(1u<<2)|(1u<<3);assert(commit(o,&r)==-117);assert(find("A:\\WORK\\TEST.BIN")==-1&&!memcmp(disk[find("A:\\WORK\\TEST.BAK")].data,"original",8));assert(!cleanup(o)&&find("A:\\WORK\\TEST.TMP")>=0);
 clear();save_original();o=&owners[0];r=request(K32_IO_BEGIN,"TEST.BIN");r.exists=1;r.length=8;r.hash=digest("original",8);assert(!dispatch(o,&r));r.operation=K32_IO_WRITE;r.bytes=3;memcpy(buffer,"new",3);assert(dispatch(o,&r)==3);fail_remove=1;assert(commit(o,&r)==1);assert(!memcmp(disk[find("A:\\WORK\\TEST.BIN")].data,"new",3)&&!memcmp(disk[find("A:\\WORK\\TEST.BAK")].data,"original",8));
 clear();o=&owners[0];serial=0x07ffffff;r=request(K32_IO_BEGIN,"LAST.BIN");assert(dispatch(o,&r)==-75&&!handles(o));
 clear();save_original();test_management(&owners[0]);
 clear();puts("common I/O: >64 KiB, locks, generations, stale revisions, short writes, recovery, rename failures and close retry PASS");return 0;
}
