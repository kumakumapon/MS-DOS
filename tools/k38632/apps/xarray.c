/* The XARRAY workloads on ordinary protected 32-bit pointers. No XMS cache or
 * DOS calls in user code. Explicit widths match the V86 demo's byte format. */
#include "k32x.h"
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned u32;
_Static_assert(sizeof(u16)==2&&sizeof(u32)==4,"workload widths");
enum {FILL=2*1024*1024,PAGE=4096,VALUES=8192};
enum {IDLE,RUNNING,DONE,FAILED,HELD};
typedef struct {u32 magic,version,sequence,scenario,state,error,bytes,sum,detail,work,allocations,frees,first,last,reserved,identity;} Result;
volatile Result xarray_result __attribute__((used,section(".data.xarray_record")))={.magic=0x58413332,.version=1};
typedef struct {volatile u8 *data;u32 bytes;} Array;
static void begin(u32 scenario,u32 bytes){
 u32 sequence=xarray_result.sequence+1,id=xarray_result.identity;
 xarray_result=(Result){.magic=0x58413332,.version=1,.sequence=sequence,.scenario=scenario,.state=RUNNING,.bytes=bytes,.identity=id};
}
static int open_array(Array *a,u32 bytes){a->data=k32_alloc(bytes);a->bytes=bytes;if(!a->data)return 1;xarray_result.allocations++;return 0;}
static int close_array(Array *a){if(k32_free((void *)a->data))return 1;a->data=0;xarray_result.frees++;return 0;}
/* Aligned four-byte groups have the same high-byte XOR. Native 32-bit loads
 * cover all four bytes; the dataset and reported work unit remain bytes. */
static inline u32 pattern_word(u32 at,u32 mask){return ((at^(at>>8)^(at>>16)^mask)&255)*0x01010101u^0x03020100u;}
static void append(char *text,unsigned *n,const char *s){while(*s)text[(*n)++]=*s++;}
static void number(char *text,unsigned *n,u32 v){char reverse[10];unsigned k=0;do{reverse[k++]=(char)('0'+v%10);v/=10;}while(v);while(k)text[(*n)++]=reverse[--k];}
static void report(const char *name){
 char text[128];unsigned n=0;append(text,&n,"XARRAY32 RESULT ");append(text,&n,name);append(text,&n," bytes=");number(text,&n,xarray_result.bytes);
 append(text,&n," sum=");number(text,&n,xarray_result.sum);append(text,&n," detail=");number(text,&n,xarray_result.detail);
 append(text,&n," work=");number(text,&n,xarray_result.work);append(text,&n,"\r\n");k32_write(text,n);
}
static int fill_array(Array *a){
 volatile u32 *words=(volatile u32 *)a->data;u32 mask=xarray_result.identity==2?0x5a:0;
 for(u32 at=0;at<a->bytes;at+=PAGE){for(u32 j=0;j<PAGE;j+=4)if(words[(at+j)/4])return 1;xarray_result.work=at+PAGE;}
 for(u32 at=0;at<a->bytes;at+=PAGE){for(u32 j=0;j<PAGE;j+=4)words[(at+j)/4]=pattern_word(at+j,mask);xarray_result.work=a->bytes+at+PAGE;}
 u32 sum=0;
 for(u32 at=0;at<a->bytes;at+=PAGE){for(u32 j=0;j<PAGE;j+=4){u32 value=words[(at+j)/4];if(value!=pattern_word(at+j,mask))return 2;sum+=(value&255)+((value>>8)&255)+((value>>16)&255)+(value>>24);}xarray_result.work=2*a->bytes+at+PAGE;}
 xarray_result.sum=sum;return 0;
}
static int fill(void){Array a;begin(1,FILL);if(open_array(&a,FILL)||fill_array(&a)||close_array(&a))return 1;xarray_result.state=DONE;report("fill");return 0;}
static int sequential(void){
 Array a;begin(2,256*PAGE);if(open_array(&a,256*PAGE))return 1;u32 sum=0;
 for(u32 page=0;page<256;page++){sum+=a.data[page*PAGE];sum+=a.data[page*PAGE+PAGE-1];}
 if(sum||close_array(&a))return 1;
 xarray_result.sum=sum;xarray_result.work=512;xarray_result.state=DONE;report("seq");return 0;
}
static int sort(void){
 Array a,b;begin(3,VALUES*2);if(open_array(&a,VALUES*2)||open_array(&b,VALUES*2))return 1;
 volatile u16 *src=(volatile u16 *)a.data,*dst=(volatile u16 *)b.data,*swap;
 u32 seed=12345^(xarray_result.identity==2?0xa5a5a5a5:0),sum=0,check=0,work=0;
 for(u32 i=0;i<VALUES;i++){seed=seed*1103515245u+12345;u16 v=(u16)(seed>>8);src[i]=v;sum+=v;work++;}
 for(u32 width=1;width<VALUES;width*=2){
  for(u32 start=0;start<VALUES;start+=2*width){
   u32 left=start,right=start+width,out=start,left_end=right<VALUES?right:VALUES,right_end=start+2*width<VALUES?start+2*width:VALUES;
   u16 lv=0,rv=0;int have_l=0,have_r=0;
   if(left<left_end){lv=src[left];work++;have_l=1;}if(right<right_end){rv=src[right];work++;have_r=1;}
   while(have_l||have_r){u16 chosen;
    if(have_l&&(!have_r||lv<=rv)){chosen=lv;left++;have_l=0;if(left<left_end){lv=src[left];work++;have_l=1;}}
    else{chosen=rv;right++;have_r=0;if(right<right_end){rv=src[right];work++;have_r=1;}}
    dst[out++]=chosen;work++;
   }
  }swap=src;src=dst;dst=swap;xarray_result.work=work;
 }
 u16 previous=0;
 for(u32 i=0;i<VALUES;i++){u16 v=src[i];if(i&&v<previous)return 2;previous=v;check+=v;work++;}
 if(check!=sum||close_array(&a)||close_array(&b))return 3;
 xarray_result.sum=check;xarray_result.detail=previous;xarray_result.work=work;xarray_result.state=DONE;report("sort");return 0;
}
static int range_ok(const Array *a,u32 at,u32 n){return at<=a->bytes&&n<=a->bytes-at;}
static int read_array(const Array *a,u32 at,u8 *out,u32 n){if(!range_ok(a,at,n))return 87;for(u32 i=0;i<n;i++)out[i]=a->data[at+i];return 0;}
static int write_array(Array *a,u32 at,const u8 *in,u32 n){if(!range_ok(a,at,n))return 87;for(u32 i=0;i<n;i++)a->data[at+i]=in[i];return 0;}
static int page_array(Array *a,u32 at){return at<a->bytes?0:87;}
static int search(void){
 static const u8 needle[8]={'K','3','8','6','x','m','s','!'};Array a;begin(4,65536);if(open_array(&a,65536))return 1;
 u32 planted=3*PAGE-3,found=~0u,matches=0,work=0;
 if(write_array(&a,planted,needle,sizeof needle))return 2;
 for(u32 start=0;start<a.bytes;start+=1024){
  u32 n=1024+sizeof needle-1;if(start+n>a.bytes)n=a.bytes-start;
  for(u32 i=0;i+sizeof needle<=n&&i<1024;i++){u32 j=0;while(j<sizeof needle&&a.data[start+i+j]==needle[j])j++;
   if(j==sizeof needle){matches++;found=start+i;}work++;
  }
 }if(matches!=1||found!=planted||close_array(&a))return 3;
 xarray_result.sum=matches;xarray_result.detail=found;xarray_result.work=work;xarray_result.state=DONE;report("search");return 0;
}
static int range(void){
 Array a;u8 byte=0;begin(5,10000);if(open_array(&a,10000))return 1;
 if(read_array(&a,10000,&byte,1)!=87||write_array(&a,9999,(const u8 *)"ab",2)!=87||read_array(&a,~0u,&byte,2)!=87||page_array(&a,10000)!=87)return 2;
 if(a.data[9999]||a.data[10000]||read_array(&a,10000,&byte,0)||read_array(&a,9999,&byte,1)||byte||close_array(&a))return 3;
 xarray_result.detail=4;xarray_result.work=6;xarray_result.state=DONE;report("range");return 0;
}
static int hold(void){
 Array a;begin(6,FILL);if(open_array(&a,FILL)||fill_array(&a))return 1;
 xarray_result.first=*(volatile u32 *)a.data;xarray_result.last=*(volatile u32 *)(a.data+FILL-4);xarray_result.state=HELD;
 k32_puts("XARRAY32 HOLD: C verifies, Q exits, F faults\r\n");
 int key;do{k32_wait(K32_WAIT_KEY,0);key=k32_poll_key();}while(key!='c'&&key!='C'&&key!='q'&&key!='Q'&&key!='f'&&key!='F');
 if(key=='f'||key=='F')*(volatile u32 *)0x10000=1;
 u32 mask=xarray_result.identity==2?0x5a:0;
 for(u32 i=0;i<FILL;i+=4)if(*(volatile u32 *)(a.data+i)!=pattern_word(i,mask))return 2;
 if(close_array(&a))return 3;
 xarray_result.state=DONE;report("hold");return key=='q'||key=='Q'?-1:0;
}
int main(int argc,char **argv){
 if(argc>2||(argc==2&&((argv[1][0]!='1'&&argv[1][0]!='2')||argv[1][1])))return 2;
 xarray_result.identity=argc==2&&argv[1][0]=='2'?2:1;
 if((k32_query()&7)!=7)return 3;
 k32_puts("XARRAY32 READY\r\n1 fill, 2 sequential, 3 sort, 4 search, 5 range, A all, H hold, F fault, Q quit\r\n");
 for(;;){int key;do{k32_wait(K32_WAIT_KEY,0);key=k32_poll_key();}while(key<0);
  if(key=='q'||key=='Q')return 0;
  int error=0;
  if(key=='1')error=fill();
  if(key=='2')error=sequential();
  if(key=='3')error=sort();
  if(key=='4')error=search();
  if(key=='5')error=range();
  if(key=='a'||key=='A')error=fill()||sequential()||sort()||search()||range();
  if(key=='h'||key=='H'){error=hold();if(error==-1)return 0;}
  if(key=='f'||key=='F'){Array a;begin(7,FILL);if(open_array(&a,FILL))error=1;else *(volatile u32 *)0x10000=1;}
  if(error){xarray_result.error=error;xarray_result.state=FAILED;k32_puts("XARRAY32 FAIL\r\n");return 1;}
 }
}
