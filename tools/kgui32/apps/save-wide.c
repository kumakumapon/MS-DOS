/* Real CPL3 64 KiB save/read/replace diagnostic, for disposable test disks. */
#include "../../k38632/sdk/k32x.h"
volatile struct {unsigned magic,version,checks,error,reserved[12];} wide_stats __attribute__((section(".data.gui_stats")))={.magic=0x3457474b,.version=4};
static char text[65536],buffer[4096];
static void check(int yes){wide_stats.checks++;if(!yes)wide_stats.error++;}
static unsigned hash(void){unsigned h=2166136261u;for(unsigned i=0;i<sizeof text;i++)h=(h^(unsigned char)text[i])*16777619u;return h;}
static int save(K32Work *r){r->operation=K32_WORK_SAVE;r->data=(unsigned)text;r->bytes=sizeof text;return k32_work(r);}
int main(int argc,char **argv){
 int hdd=argc>1&&argv[1][0]=='h',info_only=argc>1&&argv[1][0]=='i';
 if(hdd||info_only){
  wide_stats.version=5;K32WorkInfo info;int e=k32_work_info(&info);check(e==0);if(e)return 5;
  check(info.version==1&&info.size==sizeof info&&info.max_bytes==65536);
  check(info.drive<26&&info.root[0]==(char)('A'+info.drive)&&info.root[7]==0);
  check(info.error==0&&(info.flags&K32_WORK_READY));
  check(info.free_bytes==info.free_clusters*info.sector_bytes*info.cluster_sectors&&info.free_bytes<=info.total_bytes);
  if(hdd)check((info.flags&K32_WORK_HDD)&&info.drive>0&&info.total_bytes==41435136);
  wide_stats.reserved[0]=info.flags;wide_stats.reserved[1]=info.drive;wide_stats.reserved[2]=info.sector_bytes;
  wide_stats.reserved[3]=info.cluster_sectors;wide_stats.reserved[4]=info.free_bytes;wide_stats.reserved[5]=info.total_bytes;wide_stats.reserved[6]=info.max_bytes;
  K32Work bad={.version=1,.size=sizeof bad,.operation=K32_WORK_INFO,.data=(unsigned)&info,.bytes=sizeof info};
  bad.bytes--;check(k32_work(&bad)==-22);bad.bytes++;
  bad.data=0x10000;check(k32_work(&bad)==-14);bad.data=(unsigned)&info;
  bad.name[15]='X';check(k32_work(&bad)==-22);bad.name[15]=0;
  bad.flags=1;check(k32_work(&bad)==-22);bad.flags=0;
  if(info_only)return wide_stats.error?5:0;
 }
 for(unsigned i=0;i<sizeof text;i++)text[i]=i%79==78?'\n':'A'+i%26;
 K32Work r=k32_work_name(K32_WORK_SAVE,"WIDE.TXT");check(save(&r)==0);if(wide_stats.error)return 1;
 r=k32_work_name(K32_WORK_STAT,"WIDE.TXT");check(k32_work(&r)==0);check(r.exists==1&&r.length==sizeof text&&r.hash==hash());
 text[0]='!';text[65535]='?';check(save(&r)==0);if(wide_stats.error)return 2;
 if(hdd)for(unsigned pass=0;pass<2;pass++){
  r=k32_work_name(K32_WORK_STAT,"WIDE.TXT");check(k32_work(&r)==0);check(r.exists==1&&r.length==sizeof text&&r.hash==hash());
  text[1+pass]='0'+pass;text[65534-pass]='a'+pass;check(save(&r)==0);if(wide_stats.error)return 6;
 }
 if(hdd)wide_stats.reserved[11]=4;
 r=k32_work_name(K32_WORK_OPEN,"WIDE.TXT");int h=k32_work(&r);check(h>0);if(h<0)return 3;
 for(unsigned at=0;at<sizeof text;at+=sizeof buffer){int n=k32_read(h,buffer,sizeof buffer);check(n==sizeof buffer);if(n!=(int)sizeof buffer)break;for(unsigned i=0;i<sizeof buffer;i++)if(buffer[i]!=text[at+i]){wide_stats.error++;break;}}
 check(k32_read(h,buffer,1)==0);check(k32_close(h)==0);
 return wide_stats.error?4:0;
}
