#include <assert.h>
#include <string.h>
#include "settings-model.h"

static unsigned build(KgBootSettings *old,KgBootSettings *next,unsigned autoexec,
 const char *input,char *output,unsigned size){
 int n=kg_settings_build(old,next,autoexec,input,strlen(input),output,size-1);
 assert(n>=0);output[n]=0;return n;
}
int main(void){
 const char original[]="FILES=20\r\nBUFFERS=8\n\r\nDEVICE=KXHIGH.SYS\r\n"
  "device = fdxms286.sys\r\nDEVICE=KXMS32.SYS\r\nDEVICE=EMM386.EXE EMM=9160\r\n"
  "REM keep manually edited directives  \nDEVICE=KHD98.SYS\x1a";
 KgBootSettings old={0};
 kg_settings_read(&old,0,original,sizeof original-1);
 assert(old.xms&&old.ems&&old.files==20&&old.buffers==8);
 char output[512];
 KgBootSettings next=old;next.files=21;next.buffers=16;
 build(&old,&next,0,original,output,sizeof output);
 assert(strstr(output,"FILES=21\r\nBUFFERS=16\r\n\r\n"));
 assert(strstr(output,"DEVICE=EMM386.EXE EMM=9160\r\nREM keep manually edited directives  \nDEVICE=KHD98.SYS\x1a"));
 next.xms=next.ems=0;
 build(&old,&next,0,original,output,sizeof output);
 assert(!strstr(output,"fdxms286")&&!strstr(output,"EMM386")&&strstr(output,"KXMS32.SYS"));
 char disabled[512];strcpy(disabled,output);kg_settings_read(&old,0,disabled,strlen(disabled));
 next.xms=next.ems=1;build(&old,&next,0,disabled,output,sizeof output);
 assert(strstr(output,"DEVICE=KXHIGH.SYS\r\nDEVICE=FDXMS286.SYS\r\nDEVICE=KXMS32.SYS"));
 assert(strstr(output,"DEVICE=EMM386.EXE EMM=8192\r\n\x1a"));

 const char autoexec[]="@ECHO OFF\r\nIF EXIST NOTE.TXT FD98\nKGUILDR.EXE\r\nECHO done";
 old.fd98=0;next.fd98=1;build(&old,&next,1,autoexec,output,sizeof output);
 assert(strstr(output,"REM CPANEL AUTO FD98\r\nFD98\r\nKGUILDR.EXE"));
 char enabled[512];strcpy(enabled,output);kg_settings_read(&old,1,enabled,strlen(enabled));assert(old.fd98);
 next.fd98=0;build(&old,&next,1,enabled,output,sizeof output);assert(!strcmp(output,autoexec));
 assert(kg_settings_build(&old,&next,1,autoexec,strlen(autoexec),output,4)==-27);
 return 0;
}
