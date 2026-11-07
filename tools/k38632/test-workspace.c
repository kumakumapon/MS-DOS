#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "workspace.h"
static struct {char name[24];unsigned char data[65536];unsigned n;int live;} fs[4];
static unsigned char document[65536],buffer[4096];
static int calls,fail,fail2,opened,short_write;
static int failed(void){calls++;return calls==fail||calls==fail2;}
static unsigned hash(const unsigned char *p,unsigned n){unsigned h=2166136261u;for(unsigned i=0;i<n;i++)h=(h^p[i])*16777619u;return h;}
static int locate(const char *s){for(int i=0;i<4;i++)if(fs[i].live&&!strcmp(fs[i].name,s))return i;return -1;}
static int stat_file(const char *s,unsigned *n,unsigned *h){if(failed())return -5;int i=locate(s);if(i<0)return 0;*n=fs[i].n;*h=hash(fs[i].data,*n);return 1;}
static int create(const char *s){if(failed())return -5;if(locate(s)>=0)return -17;for(int i=0;i<4;i++)if(!fs[i].live){fs[i].live=1;strcpy(fs[i].name,s);fs[i].n=0;opened=i+1;return opened;}return -28;}
static int write_file(int h,const void *p,unsigned n){if(failed())return -5;assert(opened==h);if(short_write)n--;memcpy(fs[h-1].data+fs[h-1].n,p,n);fs[h-1].n+=n;return (int)n;}
static int close_file(int h){assert(opened==h);opened=0;return failed()?-5:0;}
static int rename_file(const char *a,const char *b){if(failed())return -5;int i=locate(a);if(i<0||locate(b)>=0)return -5;strcpy(fs[i].name,b);return 0;}
static int remove_file(const char *s){if(failed())return -5;int i=locate(s);if(i<0)return -5;fs[i].live=0;return 0;}
static int copy(unsigned at,void *p,unsigned n){if(failed())return -5;assert(at<=65536&&n<=65536-at);memcpy(p,document+at,n);return 0;}
static const K32WorkIO io={stat_file,create,write_file,close_file,rename_file,remove_file,copy,0};
static unsigned free_bytes,cluster_bytes;
static int space(unsigned *free,unsigned *cluster){if(failed())return -5;*free=free_bytes;*cluster=cluster_bytes;return 0;}
static K32Work reset(int old){memset(fs,0,sizeof fs);memset(document,'x',sizeof document);calls=fail=fail2=opened=short_write=0;K32Work r={.version=1,.size=sizeof r,.operation=K32_WORK_SAVE,.bytes=65536};strcpy(r.name,"NOTE.TXT");if(old){fs[0].live=1;strcpy(fs[0].name,"A:\\WORK\\NOTE.TXT");memcpy(fs[0].data,"old",3);fs[0].n=3;r.exists=1;r.length=3;r.hash=hash(fs[0].data,3);}return r;}
static void old_survives(void){int i=locate("A:\\WORK\\NOTE.TXT");if(i<0||fs[i].n!=3)i=locate("A:\\WORK\\NOTE.BAK");assert(i>=0&&fs[i].n==3&&!memcmp(fs[i].data,"old",3));}
int main(void){
 char path[24];assert(!k32_work_path("note.txt",path)&&!strcmp(path,"A:\\WORK\\NOTE.TXT"));
 const char *bad[]={"","IO.SYS","AUTOEXEC.BAT","CON.TXT","COM1.TXT","../X.TXT","A:ONE.TXT","A\\B.TXT","a..txt","tool.exe","a.txt ","123456789.TXT",".TXT","ONE.TX"};
 for(unsigned i=0;i<sizeof bad/sizeof *bad;i++)assert(k32_work_path(bad[i],path)==-22);
 for(int old=0;old<2;old++){
  K32Work r=reset(old);assert(k32_work_save(&r,&io,buffer)==0);int steps=calls,i=locate("A:\\WORK\\NOTE.TXT");assert(i>=0&&fs[i].n==65536&&hash(fs[i].data,fs[i].n)==hash(document,sizeof document)&&!opened);
  /* Fail every boundary independently: copy, stat, create, write, close,
   * both renames and cleanup. Never lose the previous complete document. */
  for(int at=1;at<=steps;at++){r=reset(old);fail=at;int e=k32_work_save(&r,&io,buffer);assert(e<0||e==1);assert(!opened);if(old&&e<0)old_survives();}
 }
 K32Work r=reset(1);r.hash++;assert(k32_work_save(&r,&io,buffer)==-116);old_survives();assert(locate("A:\\WORK\\NOTE.TMP")==-1);
 r=reset(1);r.exists=0;assert(k32_work_save(&r,&io,buffer)==-116);old_survives();
 r=reset(1);short_write=1;assert(k32_work_save(&r,&io,buffer)==-28);old_survives();assert(locate("A:\\WORK\\NOTE.TMP")==-1);
 r=reset(1);document[5000]=0;assert(k32_work_save(&r,&io,buffer)==-22);old_survives();assert(locate("A:\\WORK\\NOTE.TMP")==-1);
 r=reset(1);fs[1].live=1;strcpy(fs[1].name,"A:\\WORK\\NOTE.TMP");assert(k32_work_save(&r,&io,buffer)==-17);old_survives();
 r=reset(1);fs[1].live=1;strcpy(fs[1].name,"A:\\WORK\\NOTE.BAK");assert(k32_work_save(&r,&io,buffer)==-117);old_survives();
 /* Failed publication and failed rollback preserve the original in BAK. */
 r=reset(1);assert(!k32_work_save(&r,&io,buffer));int steps=calls;r=reset(1);fail=steps-1;fail2=steps;assert(k32_work_save(&r,&io,buffer)==-117);old_survives();
 r=reset(0);r.bytes=0;assert(!k32_work_save(&r,&io,buffer));assert(fs[locate("A:\\WORK\\NOTE.TXT")].n==0);
 r=reset(1);r.bytes=65537;assert(k32_work_save(&r,&io,buffer)==-22&&calls==0);
 r=reset(1);r.reserved[0]=1;assert(k32_work_save(&r,&io,buffer)==-22&&calls==0);
 assert(!k32_work_path_at(1,"note.txt",path)&&!strcmp(path,"B:\\WORK\\NOTE.TXT"));
 assert(!k32_work_path_at(25,"note.txt",path)&&path[0]=='Z');
 assert(k32_work_path_at(26,"note.txt",path)==-22);
 K32WorkInfo info={0};assert(!k32_work_space(&info,4,10115,1024,10116));
 assert(info.free_bytes==41431040&&info.total_bytes==41435136&&info.cluster_sectors==4);
 assert(!k32_work_space(&info,1,30,1024,1221)&&info.free_bytes==30720);
 assert(k32_work_space(&info,0,1,1024,1)==-5);assert(k32_work_space(&info,4,2,1024,1)==-5);
 assert(k32_work_space(&info,1,1,1000,10)==-5);assert(k32_work_space(&info,128,1,4096,10)==-75);
 assert(k32_work_space(&info,4,1,1024,~0u)==-75);
 r=(K32Work){.version=1,.size=sizeof r,.operation=K32_WORK_INFO,.bytes=sizeof info};assert(!k32_work_request(&r));
 r.name[15]='A';assert(k32_work_request(&r)==-22);r.name[15]=0;r.bytes--;assert(k32_work_request(&r)==-22);
 K32WorkIO hdd=io;hdd.space=space;cluster_bytes=4096;free_bytes=65536;
 for(int old=0;old<2;old++){
  r=reset(old);if(old)fs[0].name[0]='B';assert(!k32_work_save_at(1,&r,&hdd,buffer));int steps=calls;
  assert(locate("A:\\WORK\\NOTE.TXT")==-1&&locate("B:\\WORK\\NOTE.TXT")>=0);
  for(int at=1;at<=steps;at++){
   r=reset(old);if(old)fs[0].name[0]='B';fail=at;int e=k32_work_save_at(1,&r,&hdd,buffer);assert(e<0||e==1);assert(!opened);
   if(old&&e<0){int i=locate("B:\\WORK\\NOTE.TXT");if(i<0)i=locate("B:\\WORK\\NOTE.BAK");assert(i>=0&&fs[i].n==3&&!memcmp(fs[i].data,"old",3));}
  }
 }
 r=reset(1);fs[0].name[0]='B';free_bytes=65535;assert(k32_work_save_at(1,&r,&hdd,buffer)==-28);
 assert(fs[0].n==3&&!memcmp(fs[0].data,"old",3)&&locate("B:\\WORK\\NOTE.TMP")==-1&&!opened);
 r=reset(0);r.bytes=1;free_bytes=4095;assert(k32_work_save_at(1,&r,&hdd,buffer)==-28);assert(locate("B:\\WORK\\NOTE.TMP")==-1);
 r=reset(0);r.bytes=0;free_bytes=0;assert(!k32_work_save_at(1,&r,&hdd,buffer));
 r=reset(0);cluster_bytes=0;assert(k32_work_save_at(1,&r,&hdd,buffer)==-5);
 puts("workspace: names, full 64 KiB, empty files, conflict, short writes and every transaction failure passed");return 0;
}
