/* Actual CPL3 negative ABI probe. Never creates or modifies a file. */
#include "../../k38632/sdk/k32x.h"
volatile struct {unsigned magic,version,checks,error,reserved[12];} save_stats __attribute__((section(".data.gui_stats")))={.magic=0x3453474b,.version=4};
static void check(int actual,int expected){save_stats.checks++;if(actual!=expected)save_stats.error++;}
static unsigned char invalid_text[4097];
int main(int argc,char **argv){
 K32Work r=k32_work_name(K32_WORK_STAT,"MISSING.TXT");
 if(!(k32_query()&K32_FEATURE_SAVE)){check(k32_work(&r),-38);return save_stats.error?1:0;}
 if(argc==2&&k32_len(argv[1])==4&&argv[1][0]=='s'&&argv[1][1]=='j'&&argv[1][2]=='i'&&argv[1][3]=='s'){
  const unsigned char bad[][2]={{0x81,0},{0x81,0x7f},{0x81,'0'},{0x80,'x'},{0xa0,'x'},{0xf0,0x40}};
  for(unsigned i=0;i<sizeof bad/sizeof *bad;i++){
   for(unsigned n=0;n<sizeof invalid_text;n++)invalid_text[n]='x';
   invalid_text[4095]=bad[i][0];invalid_text[4096]=bad[i][1];
   r=k32_work_name(K32_WORK_SAVE,"JPREJECT.TXT");r.data=(unsigned)invalid_text;r.bytes=bad[i][1]?4097:4096;
   check(k32_work(&r),-22);
  }
  return save_stats.error?1:0;
 }
 check(k32_call(17,(unsigned)&r,sizeof r-1),-22);
 check(k32_call(17,0x10000,sizeof r),-14);
 check(k32_call(17,0x800000,sizeof r),-14);
 check(k32_call(17,0xbffff0,sizeof r),-14);
 r.version=2;check(k32_work(&r),-22);r.version=1;
 r.reserved[1]=1;check(k32_work(&r),-22);r.reserved[1]=0;
 r.flags=2;check(k32_work(&r),-22);r.flags=0;
 r.operation=99;check(k32_work(&r),-38);
 const char *bad[]={"../IO.SYS","IO.SYS","CON.TXT","A:BAD.TXT","A\\B.TXT","AUTOEXEC.BAT","ONE.TXT ",".TXT"};
 for(unsigned i=0;i<sizeof bad/sizeof *bad;i++){r=k32_work_name(K32_WORK_SAVE,bad[i]);check(k32_work(&r),-22);}
 r=k32_work_name(K32_WORK_SAVE,"PROBE.TXT");r.bytes=65537;check(k32_work(&r),-22);
 r.bytes=1;r.data=0x10000;check(k32_work(&r),-14);
 r.data=0xbfffff;r.bytes=2;check(k32_work(&r),-14);
 r.bytes=0;r.exists=2;check(k32_work(&r),-22);
 r=k32_work_name(K32_WORK_STAT,"MISSING.TXT");check(k32_work(&r),0);check(r.exists,0);
 K32DirEntry e;int got=k32_work_first(&e);check(got>=0,1);if(got>=0)check(k32_dir_close(e.cursor),0);
 return save_stats.error?1:0;
}
