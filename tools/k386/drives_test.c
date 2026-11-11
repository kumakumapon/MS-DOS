#include "drives.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static void path(const K386Drives *d,const char *input,int wild,const char *expected){char out[128];assert(!k386_drive_path(d,input,out,wild));assert(!strcmp(out,expected));}
int main(void){
 K386Drives a,b;k386_drives_init(&a,0);k386_drives_init(&b,2);char out[128];
 path(&a,"file.txt",0,"A:\\FILE.TXT");assert(k386_drive_path(&a,"B:\\FILE.TXT",out,0)==15);
 assert(k386_drive_path(&b,"B:\\FILE.TXT",out,0)==15);path(&b,"c:",0,"C:\\");
 assert(!k386_drive_cwd(&b,"C:\\APPS"));assert(b.current==0);path(&b,"c:leaf.txt",0,"C:\\APPS\\LEAF.TXT");
 path(&b,"leaf.txt",0,"A:\\LEAF.TXT");b.current=2;path(&b,"leaf.txt",0,"C:\\APPS\\LEAF.TXT");
 path(&b,"A:leaf.txt",0,"A:\\LEAF.TXT");path(&b,"/leaf.txt",0,"C:\\LEAF.TXT");
 path(&b,"..\\..\\leaf.txt",0,"C:\\LEAF.TXT");path(&b,"C:/APPS/./DATA/../*.TXT",1,"C:\\APPS\\*.TXT");
 assert(!strcmp(a.cwd[0],"A:\\")&&!a.current&&!a.hdd);
 const char *bad[]={"C:\\NUL.TXT","C:\\COM1.EXE","C:\\LPT0.TXT","C:\\AUX\\X.TXT","C:\\CLOCK$","C:\\A*.DIR\\X.TXT","C:\\NAME:BAD","\\\\SERVER\\SHARE\\FILE.TXT","C:\\TOOLONG99.TXT"};
 /* UNC is not a network route; overlong/malformed/device components are refused. */
 for(unsigned i=0;i<sizeof bad/sizeof *bad;i++)assert(k386_drive_path(&b,bad[i],out,0));
 assert(k386_drive_path(&b,"*.TXT",out,0));assert(k386_drive_path(&b,"",out,0));
 char longname[129];memset(longname,'A',128);longname[128]=0;assert(k386_drive_path(&b,longname,out,0));
 K386Drives before=b;char directory[80]="C:\\";for(unsigned i=0;i<7;i++)strcat(directory,i?"\\ABCDEFGH":"ABCDEFGH");
 assert(strlen(directory)==65);assert(!k386_drive_cwd(&b,directory));strcat(directory,"\\X");assert(strlen(directory)==67);
 K386Drives limit=b;assert(k386_drive_cwd(&b,directory)==3&&!memcmp(&b,&limit,sizeof b));
 b=before;b.current=25;assert(k386_drive_path(&b,"FILE.TXT",out,0)==15);
 puts("V86 drives: private CWDs, dynamic HDD letter, canonical paths, devices and bounds passed");
}
