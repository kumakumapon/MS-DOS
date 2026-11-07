/* Real CPL3 64 KiB save/read/replace diagnostic, for disposable test disks. */
#include "../../k38632/sdk/k32x.h"
volatile struct {unsigned magic,version,checks,error,reserved[12];} wide_stats __attribute__((section(".data.gui_stats")))={.magic=0x3457474b,.version=4};
static char text[65536],buffer[4096];
static void check(int yes){wide_stats.checks++;if(!yes)wide_stats.error++;}
static unsigned hash(void){unsigned h=2166136261u;for(unsigned i=0;i<sizeof text;i++)h=(h^(unsigned char)text[i])*16777619u;return h;}
static int save(K32Work *r){r->operation=K32_WORK_SAVE;r->data=(unsigned)text;r->bytes=sizeof text;return k32_work(r);}
int main(void){
 for(unsigned i=0;i<sizeof text;i++)text[i]=i%79==78?'\n':'A'+i%26;
 K32Work r=k32_work_name(K32_WORK_SAVE,"WIDE.TXT");check(save(&r)==0);if(wide_stats.error)return 1;
 r=k32_work_name(K32_WORK_STAT,"WIDE.TXT");check(k32_work(&r)==0);check(r.exists==1&&r.length==sizeof text&&r.hash==hash());
 text[0]='!';text[65535]='?';check(save(&r)==0);if(wide_stats.error)return 2;
 r=k32_work_name(K32_WORK_OPEN,"WIDE.TXT");int h=k32_work(&r);check(h>0);if(h<0)return 3;
 for(unsigned at=0;at<sizeof text;at+=sizeof buffer){int n=k32_read(h,buffer,sizeof buffer);check(n==sizeof buffer);if(n!=(int)sizeof buffer)break;for(unsigned i=0;i<sizeof buffer;i++)if(buffer[i]!=text[at+i]){wide_stats.error++;break;}}
 check(k32_read(h,buffer,1)==0);check(k32_close(h)==0);
 return wide_stats.error?4:0;
}
