/* DOS-native test parent; writes only a recognized disposable proof volume. */
#include "policy.h"
extern void fault_begin(void),fault_end(void);
static u8 buffer[4096],packet[32];static u32 driver,strategy,handler;
static char path[20]="B:\\HDD.ID";static unsigned drive;
static u8 get(u32 p){u8 c;__asm__ volatile("pushw %%es; movw %w1,%%es; movb %%es:(%2),%0; popw %%es":"=q"(c):"r"(p>>4),"r"(p&15):"memory");return c;}
static void set(u32 p,u8 c){__asm__ volatile("pushw %%es; movw %w0,%%es; movb %b2,%%es:(%1); popw %%es"::"r"(p>>4),"r"(p&15),"q"(c):"memory");}
static u16 word(u32 p){return get(p)|(u16)get(p+1)<<8;}
static u32 dword(u32 p){return word(p)|(u32)word(p+2)<<16;}
static u32 local(const void *p){u16 ds;__asm__ volatile("movw %%ds,%0":"=r"(ds));return (u32)ds*16+(u32)p;}
static void print(const char *s){unsigned n=0;while(s[n])n++;u32 a=0x4000;__asm__ volatile("int $0x21":"+a"(a):"b"(1),"c"(n),"d"((u32)s):"cc","memory");}
static void number(unsigned n){char s[12];unsigned i=10;s[11]=0;do{s[i--]='0'+n%10;n/=10;}while(n);print(s+i+1);}
static void release(u16 block){u32 a=0x4900;__asm__ volatile("pushw %%es; movw %w1,%%es; int $0x21; popw %%es":"+a"(a):"r"((u32)block):"cc","memory");}
static int open(const char *s,unsigned mode){u32 a=mode?0x3c00:0x3d00;__asm__ volatile("int $0x21; jnc 1f; movl $-1,%%eax; 1:":"+a"(a):"c"(0),"d"((u32)s):"cc","memory");return a==~0u?-1:(int)(a&65535);}
static int close(int h){u32 a=0x3e00;__asm__ volatile("int $0x21; jnc 1f; movl $-1,%%eax; 1:":"+a"(a):"b"(h):"cc","memory");return a==~0u?-1:0;}
static int transfer(int h,unsigned n,int writing){u32 a=writing?0x4000:0x3f00;__asm__ volatile("int $0x21; jnc 1f; movl $-1,%%eax; 1:":"+a"(a):"b"(h),"c"(n),"d"((u32)buffer):"cc","memory");return a==~0u?-1:(int)(a&65535);}
static int locate(void){
 for(unsigned d=2;d<=5;d++){
  u32 a=0x3600,b,c,e=d;__asm__ volatile("int $0x21":"+a"(a),"=b"(b),"=c"(c),"+d"(e)::"cc","memory");
  if((a&65535)==65535||(a&65535)!=4||(c&65535)!=1024||(e&65535)!=10116)continue;
  path[0]='A'+d-1;int h=open(path,0);if(h<0)continue;int n=transfer(h,8,0),closed=close(h);if(n!=8||closed||buffer[0]!='K'||buffer[1]!='H'||buffer[2]!='D'||buffer[3]!='P'||buffer[4]!='R'||buffer[5]!='O'||buffer[6]!='O'||buffer[7]!='F')continue;
  drive=d;print("HDD DRIVE=");char letter[2]={path[0],0};print(letter);print(" BPS=1024 SPC=4 CLUSTERS=");number(e&65535);print(" FREE=");number(b&65535);print(" BYTES=");number(4096*(e&65535));print("\r\n");
  u16 seg;u32 ptr;a=0x3200;__asm__ volatile("pushw %%ds; int $0x21; movw %%ds,%1; popw %%ds":"+a"(a),"=c"(seg),"=b"(ptr):"d"(d):"cc","memory");if(a&255)return 0;
  u32 dpb=(u32)seg*16+(ptr&65535);u32 far=dword(dpb+19);driver=(far>>16)*16+(far&65535);
  const char *magic="KHDDIOP1";for(unsigned i=0;i<8;i++)if(get(driver+18+i)!=(u8)magic[i])return 0;
  strategy=((driver>>4)<<16)|word(driver+6);handler=((driver>>4)<<16)|word(driver+8);return 1;
 }return 0;
}
static unsigned mode(void){u32 a=0x6200,b;__asm__ volatile("int $0x21":"+a"(a),"=b"(b)::"cc","memory");unsigned n=get(b*16+0x80);for(unsigned i=0;i<n;i++){unsigned c=get(b*16+0x81+i);if(c!=' ')return c>='a'&&c<='z'?c-32:c;}return 'R';}
static void raw(unsigned unit,unsigned op,unsigned start,unsigned count,u32 address){
 for(unsigned i=0;i<sizeof packet;i++)packet[i]=0;
 packet[0]=sizeof packet;packet[1]=unit;packet[2]=op;
 *(u16 *)(packet+14)=address&15;*(u16 *)(packet+16)=address>>4;*(u16 *)(packet+18)=count;*(u16 *)(packet+20)=start;
 /* Preserve the deliberately unnormalized incoming offset to test wrap. */
 if(address&&((address&65535)==0xff80)){*(u16 *)(packet+14)=0xff80;*(u16 *)(packet+16)=(address-0xff80)>>4;}
 u32 p=local(packet),a=p>>4,b=p&15;
 __asm__ volatile("pushw %%es; movw %w0,%%es; lcallw *%2; lcallw *%3; popw %%es":"+a"(a),"+b"(b):"m"(strategy),"m"(handler):"ecx","edx","esi","edi","cc","memory");
}
static int bounds(void){
 unsigned tests=0;u32 addr=local(buffer);
 unsigned cases[][3]={{KHD_SECTORS,1,0},{KHD_SECTORS-1,2,0},{65535,2,0},{0,1,1}};
 for(unsigned i=0;i<4;i++){raw(cases[i][2],4,cases[i][0],cases[i][1],addr);if(!(*(u16 *)(packet+3)&0x8000)||*(u16 *)(packet+18))return 1;tests++;}
 raw(0,4,0,1,0xa0000);if(!(*(u16 *)(packet+3)&0x8000))return 2;tests++;
 raw(0,4,0,0,0xffffffff);if(*(u16 *)(packet+3)!=0x100||*(u16 *)(packet+18))return 3;tests++;
 u32 a=0x4800,b=0x1200;__asm__ volatile("int $0x21; jnc 1f; movl $-1,%%eax; 1:":"+a"(a):"b"(b):"cc","memory");if(a==~0u)return 4;
 u16 block=a;u32 cross=((u32)block*16+65535)&~65535u;cross+=0xff80;
 /* Pick the preceding 64K edge if the first would exceed the allocated block. */
 if(cross+2048>(u32)block*16+0x12000)cross-=65536;
 if(cross<(u32)block*16||cross+2048>(u32)block*16+0x12000){release(block);return 5;}
 for(unsigned i=0;i<2048;i++)set(cross+i,0xac);
 raw(0,4,KHD_SECTORS-1,1,cross);int error=0;
 const char *last="HDD LAST SECTOR";if(*(u16 *)(packet+3)!=0x100||*(u16 *)(packet+18)!=1)error=6;
 for(unsigned i=0;last[i];i++)if(get(cross+i)!=(u8)last[i])error=7;
 if(!error)tests++;
 if(!error){raw(0,9,KHD_SECTORS-1,1,cross);if(*(u16 *)(packet+3)!=0x100||*(u16 *)(packet+18)!=1)error=10;else tests++;}
 for(unsigned op=8;op<=9;op++){raw(0,op,KHD_SECTORS-1,2,cross);if(!(*(u16 *)(packet+3)&0x8000)||*(u16 *)(packet+18))error=11;else tests++;}
 for(unsigned i=0;i<2048;i++)set(cross+i,0xac);
 fault_begin();raw(0,4,0,2,cross);fault_end();
 if(*(u16 *)(packet+3)!=0x810b||*(u16 *)(packet+18)!=1)error=8;
 for(unsigned i=1024;i<2048;i++)if(get(cross+i)!=0xac)error=9;
 if(!error)tests++;
 release(block);
 if(error)return error;
 print("BOUNDS PASS ");number(tests);print("\r\n");return 0;
}
int main(void){
 unsigned m=mode();if(!locate()){print("NO HDD\r\n");return m=='N'?0:1;}if(m=='N'){print("UNEXPECTED HDD\r\n");return 2;}
 if(m=='B'){int e=bounds();if(e){print("BOUNDS FAIL ");number(e);print("\r\n");}return e;}
 if(m=='R'){print("READ PASS\r\n");return 0;}
 path[3]='I';path[4]='O';path[5]='P';path[6]='R';path[7]='O';path[8]='O';path[9]='F';path[10]='.';path[11]='D';path[12]='A';path[13]='T';path[14]=0;
 int writing=m=='W'||m=='O',alternate=m=='O'||m=='T';if(!writing&&m!='V'&&m!='T')return 3;
 /* This path is private to a marked, disposable proof volume. */
 int h=open(path,writing);if(h<0){print("OPEN FAIL\r\n");return 4;}
 unsigned hash=2166136261u,at=0;int e=0;
 while(at<131073){unsigned n=131073-at;if(n>sizeof buffer)n=sizeof buffer;
  if(writing)for(unsigned i=0;i<n;i++)buffer[i]=((at+i)*(alternate?3:1)+(alternate?7:0))%251;
  int got=transfer(h,n,writing);if(got!=(int)n){e=5;break;}
  for(unsigned i=0;i<n;i++){if(buffer[i]!=((at+i)*(alternate?3:1)+(alternate?7:0))%251){e=6;break;}hash=(hash^buffer[i])*16777619u;}if(e)break;at+=n;
 }
 if(!writing&&!e&&transfer(h,1,0)!=0)e=7;
 if(close(h))e=8;
 if(e){print("IO FAIL ");number(e);print("\r\n");return e;}
 print(writing?"WRITE PASS BYTES=":"VERIFY PASS BYTES=");number(at);print(" HASH=");number(hash);print("\r\n");return 0;
}
