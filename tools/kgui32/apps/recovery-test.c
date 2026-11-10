/* Real CPL3 negative requests: no filesystem mutation or forged DOS handle. */
#include "../../k38632/sdk/k32x.h"
volatile struct {unsigned magic,version,checks,error,reserved[12];} recovery_stats __attribute__((section(".data.gui_stats")))={.magic=0x3552474b,.version=1};
static void check(int actual,int expected){recovery_stats.checks++;if(actual!=expected)recovery_stats.error++;}
int main(void){
 K32Work r=k32_work_name(K32_WORK_REC_INFO,"MISSING.TXT");K32RecoveryInfo info;
 r.data=(unsigned)&info;r.bytes=sizeof info;
 if(!(k32_query()&K32_FEATURE_SAVE)){check(k32_work(&r),-38);return recovery_stats.error?1:0;}
 check(k32_call(17,(unsigned)&r,sizeof r-1),-22);
 check(k32_call(17,0x10000,sizeof r),-14);
 check(k32_call(17,0x800000,sizeof r),-14);
 r.bytes--;check(k32_work(&r),-22);r.bytes++;
 r.data=0x10000;check(k32_work(&r),-14);
 r.data=0x800000;check(k32_work(&r),-14);
 r.data=0xbffff0;check(k32_work(&r),-14);
 r.data=(unsigned)&info;check(k32_work(&r),0);check(info.version,1);check(info.size,64);
 check(info.file[0].exists|info.file[1].exists|info.file[2].exists,0);
 check(k32_recovery_open("MISSING.TXT",1),-2);check(k32_recovery_open("MISSING.TXT",2),-2);check(k32_recovery_open("MISSING.TXT",3),-22);
 K32RecoveryRestore restore={.expected=info,.destination="UNUSED.TXT"};
 r=k32_work_name(K32_WORK_REC_RESTORE_BAK,"MISSING.TXT");r.bytes=sizeof restore;r.data=0x800000;check(k32_work(&r),-14);
 r.data=0xbfffc0;check(k32_work(&r),-14);
 r.data=(unsigned)&restore;check(k32_work(&r),-2);
 restore.expected.version=2;check(k32_work(&r),-22);restore.expected.version=1;
 restore.expected.reserved[1]=1;check(k32_work(&r),-22);restore.expected.reserved[1]=0;
 restore.expected.file[2].exists=1;restore.expected.file[2].hash=123;check(k32_work(&r),-116);restore.expected=info;
 const char *bad[]={"../X.TXT","NUL.TXT","A:ONE.TXT","ONE.EXE","*.TXT"};
 for(unsigned i=0;i<sizeof bad/sizeof *bad;i++){
  for(unsigned j=0;j<16;j++)restore.destination[j]=0;
  for(unsigned j=0;bad[i][j];j++)restore.destination[j]=bad[i][j];
  check(k32_work(&r),-22);
 }
 for(unsigned kind=1;kind<=2;kind++){
  K32DirEntry entry;int got=k32_recovery_first(kind,&entry);check(got>=0,1);if(got>=0)check(k32_dir_close(entry.cursor),0);
 }
 return recovery_stats.error?1:0;
}
