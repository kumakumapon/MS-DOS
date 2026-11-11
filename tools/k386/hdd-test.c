/* Real V86 DOS probe. The MZ stub refuses direct host-DOS execution. */
#include "sdk.h"
#include "../filer/filer.h"
typedef struct {char magic[8];unsigned version,size,checks,error,stage,drive,bytes,hash,first,actual,environment,reserved[3];} HddStats;
extern volatile HddStats v86_hdd_stats;
static u8 buffer[4096],dta[43],env[1536] __attribute__((aligned(16)));
static void check(int condition,int actual){v86_hdd_stats.checks++;if(!condition){if(!v86_hdd_stats.error){v86_hdd_stats.first=v86_hdd_stats.checks;v86_hdd_stats.actual=actual;}v86_hdd_stats.error++;}}
static unsigned drive(void){unsigned a=0x1900;__asm__ volatile("int $0x21":"+a"(a)::"cc","memory");return a&255;}
static int select_drive(unsigned d){unsigned a=0x0e00;__asm__ volatile("int $0x21; movzwl %%ax,%%eax; jnc 1f; negl %%eax; 1:":"+a"(a):"d"(d):"cc","memory");return (int)a;}
static int cwd(unsigned d,char *out){unsigned a=0x4700;__asm__ volatile("int $0x21; movzwl %%ax,%%eax; jnc 1f; negl %%eax; 1:":"+a"(a):"d"(d),"S"((unsigned)out):"cc","memory");return (int)a;}
static int path_call(unsigned fn,const char *path,unsigned value){unsigned a=fn,c=value;__asm__ volatile("int $0x21; movzwl %%ax,%%eax; jnc 1f; negl %%eax; 1:":"+a"(a),"+c"(c):"d"((unsigned)path):"cc","memory");return (int)a;}
static int duplicate(unsigned source,unsigned target){unsigned a=0x4600;__asm__ volatile("int $0x21; movzwl %%ax,%%eax; jnc 1f; negl %%eax; 1:":"+a"(a):"b"(source),"c"(target):"cc","memory");return (int)a;}
static void set_dta(void){unsigned a=0x1a00;__asm__ volatile("int $0x21":"+a"(a):"d"((unsigned)dta):"cc","memory");}
static int find(const char *pattern,int next){return path_call(next?0x4f00:0x4e00,pattern,0);}
static u8 peek(unsigned segment,unsigned at){u8 c;__asm__ volatile("pushw %%es; movw %w1,%%es; movb %%es:(%2),%0; popw %%es":"=q"(c):"r"(segment),"r"(at):"memory");return c;}
static int execute(const char *name,unsigned environment){
 u16 ds;__asm__ volatile("movw %%ds,%0":"=r"(ds));
 static const u8 tail[]={6,' ','C','H','I','L','D',13};
 struct __attribute__((packed)){u16 env,tail,segment,fcb1,seg1,fcb2,seg2;} params={environment,(unsigned)tail,ds,0,0,0,0};
 unsigned a=0x4b00;__asm__ volatile("pushw %%es; pushw %%ds; popw %%es; int $0x21; popw %%es; movzwl %%ax,%%eax; jnc 1f; negl %%eax; 1:":"+a"(a):"b"((unsigned)&params),"d"((unsigned)name):"cc","memory");return (int)a;
}
static int report(const char *label){v86_hdd_stats.stage=9;p_output(v86_hdd_stats.error?"V86 HDD FAIL\r\n":label);return v86_hdd_stats.error?1:0;}
static int child(void){
 unsigned segment;__asm__ volatile("movzwl %%fs:0x2c,%0":"=r"(segment));
 check(peek(segment,0)=='P'&&peek(segment,3)=='=',peek(segment,0));
 check(peek(segment,1498)=='Q'&&!peek(segment,1499)&&!peek(segment,1500),peek(segment,1498));
 check(peek(segment,1501)==1&&!peek(segment,1502)&&peek(segment,1503)=='A'+drive(),peek(segment,1503));v86_hdd_stats.environment=1501;
 int n=p_read(10,buffer,1);check(n==1&&buffer[0]==70000%251,n);check(!p_close(10),10);
 set_dta();check(!find("VDATA.BIN",0),0);
 v86_hdd_stats.stage=3;p_output("V86 HDD CHILD WAIT\r\n");p_key();
 return report("V86 HDD CHILD PASS\r\n");
}
int main(void){
 char args[128],directory[68],path[32]="A:\\APPS\\VDATA.BIN";p_arguments(args,sizeof args);
 char *mode=args;while(*mode==' ')mode++;
 v86_hdd_stats.drive=drive();v86_hdd_stats.stage=1;
 if(mode[0]=='C')return child();
 if(mode[0]=='F'){
  check(!drive(),drive());check(!cwd(0,directory)&&!directory[0],directory[0]);
  int h=p_open("A:\\KDATA.TXT");check(h>=0,h);if(h>=0){check(p_read(h,buffer,16)==16,0);check(!p_close(h),h);}
  if(mode[2]=='N'){int e=select_drive(1);check(e==-15,e);check(!drive(),drive());}
  return report("V86 FD READ PASS\r\n");
 }
 unsigned own=drive();check(own>0&&own<26,own);if(!own||own>25)return report("unused");path[0]='A'+own;
 check(!cwd(0,directory)&&!fd_cmp(directory,"APPS"),directory[0]);
 int h=p_open("VDATA.BIN");check(h>=0,h);if(h<0)return report("unused");
 if(mode[0]=='H'||mode[0]=='X'){
  v86_hdd_stats.stage=5;p_output("V86 HDD HANDLE HELD\r\n");p_key();
  if(mode[0]=='X')__asm__ volatile("pushw %%es; movw $0xf000,%%ax; movw %%ax,%%es; movb $1,%%es:0; popw %%es":::"eax","memory");
  check(!p_close(h),h);return report("V86 HDD HOLD PASS\r\n");
 }
 unsigned offset=0,hash=2166136261u;int n;
 while((n=p_read(h,buffer,sizeof buffer))>0){int valid=1;for(int i=0;i<n;i++){if(buffer[i]!=(offset+(unsigned)i)%251)valid=0;hash=(hash^buffer[i])*16777619u;}check(valid,n);offset+=n;}
 check(n==0,n);check(offset==70001,offset);v86_hdd_stats.bytes=offset;v86_hdd_stats.hash=hash;
 check(p_seek(h,65535)>=0,0);n=p_read(h,buffer,5);check(n==5&&buffer[0]==65535%251&&buffer[4]==65539%251,n);
 check(p_seek(h,70000)>=0,0);check(duplicate(h,10)>=0,10);check(!p_close(h),h);
 check(path_call(0x3d01,path,0)==-5,1);check(path_call(0x3d02,path,0)==-5,2);
 const unsigned denied[]={0x3900,0x3a00,0x3c00,0x4100,0x4301,0x5b00};
 for(unsigned i=0;i<sizeof denied/sizeof *denied;i++){int e=path_call(denied[i],path,0);check(e==-5,e);}
 unsigned a=0x4000;__asm__ volatile("int $0x21; movzwl %%ax,%%eax; jnc 1f; negl %%eax; 1:":"+a"(a):"b"(10),"c"(1),"d"((unsigned)buffer):"cc","memory");check((int)a==-5,a);
 a=0x5600;__asm__ volatile("pushw %%es; pushw %%ds; popw %%es; int $0x21; popw %%es; movzwl %%ax,%%eax; jnc 1f; negl %%eax; 1:":"+a"(a):"d"((unsigned)path),"D"((unsigned)path):"cc","memory");check((int)a==-5,a);
 const char *bad[]={"NUL.TXT","COM1.EXE","COM0.TXT","LPT0.TXT","Z:\\X.TXT","*.BIN","A:BAD:NAME"};
 for(unsigned i=0;i<sizeof bad/sizeof *bad;i++){int e=p_open(bad[i]);check(e<0,e);if(e>=0)p_close(e);}
 check(select_drive(own==25?24:25)==-15,25);check(drive()==own,drive());
 check(select_drive(0)>=0,0);check(!drive(),drive());
 check(!cwd(own+1,directory)&&!fd_cmp(directory,"APPS"),directory[0]);
 path[1]=':';path[2]=0;check(path_call(0x3b00,path,0)==0,0);check(!drive(),drive());
 check(select_drive(own)>=0,own);check(drive()==own,drive());
 check(!path_call(0x3b00,"\\",0),0);check(!cwd(0,directory)&&!directory[0],directory[0]);
 check(!path_call(0x3b00,"APPS",0),0);
 set_dta();check(!find("*.EXE",0),0);u8 saved=dta[0];dta[0]^=1;check(find("",1)==-18,18);dta[0]=saved;
 for(unsigned i=0;i<1499;i++)env[i]='Q';
 env[0]='P';env[1]='A';env[2]='D';env[3]='=';env[1499]=env[1500]=0;
 u16 ds;__asm__ volatile("movw %%ds,%0":"=r"(ds));
 v86_hdd_stats.stage=2;int e=execute("HDDCHLD.EXE",ds+(unsigned)env/16);check(!e,e);
 a=0x4d00;__asm__ volatile("int $0x21":"+a"(a)::"cc","memory");check(!(a&65535),a);
 check(p_read(10,buffer,1)==0,10);check(!p_close(10),10);check(!find("",1),0);
 check(drive()==own,drive());check(!cwd(0,directory)&&!fd_cmp(directory,"APPS"),directory[0]);
 v86_hdd_stats.stage=4;return report("V86 HDD READ PASS\r\n");
}
_Static_assert(sizeof(HddStats)==64,"V86 snapshot probe layout");
