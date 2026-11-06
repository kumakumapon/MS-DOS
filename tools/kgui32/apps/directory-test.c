/* Real CPL3 syscall boundary checks, not a mocked DOS enumeration. */
#include "stats.h"
volatile KgStats directory_stats __attribute__((section(".data.gui_stats")))={.magic=0x3244474b,.version=1};
static K32DirEntry entry;
static const K32DirEntry readonly_entry={.version=1};
#define CHECK(x) do{if(!(x)){directory_stats.error=++directory_stats.value;return 1;}directory_stats.value++;}while(0)
int main(int argc,char **argv){
 (void)argc;(void)argv;
 if(!(k32_query()&64)){CHECK(k32_dir_first(&entry)==-38);CHECK(k32_dir_next(1,&entry)==-38);CHECK(k32_dir_close(1)==-38);return 0;}
 CHECK((k32_query()&64)!=0);
 CHECK(k32_call(14,0x10000,64)==-14);
 CHECK(k32_call(14,(u32)&readonly_entry,64)==-14);
 CHECK(k32_call(14,0xfffffff0u,64)==-14);
 CHECK(k32_call(14,(u32)&entry,63)==-22);
 CHECK(k32_dir_next(0,&entry)==-9);
 CHECK(k32_dir_close(0)==-9);
 CHECK(k32_dir_first(&entry)==1);
 CHECK(entry.version==1&&entry.size==64&&entry.cursor&&entry.name[0]);
 unsigned first=entry.cursor;
 CHECK(k32_call4(15,first,0x10000,64)==-14);
 CHECK(k32_call4(15,first,(u32)&readonly_entry,64)==-14);
 CHECK(k32_call4(15,first,(u32)&entry,65)==-22);
 CHECK(k32_dir_next(first^1,&entry)==-9);
 CHECK(k32_dir_close(first)==0);
 CHECK(k32_dir_next(first,&entry)==-9);
 CHECK(k32_dir_close(first)==-9);
 CHECK(k32_dir_first(&entry)==1);
 CHECK(entry.cursor!=first);
 CHECK(k32_dir_next(first,&entry)==-9);
 unsigned cursor=entry.cursor,count=1;int e;
 do{e=k32_dir_next(cursor,&entry);if(e==1)count++;}while(e==1&&count<=192);
 CHECK(e==0&&count>20&&count<=192);
 CHECK(k32_dir_next(cursor,&entry)==0);
 CHECK(k32_dir_close(cursor)==0);
 CHECK(k32_open("../IO.SYS")==-22);
 directory_stats.frames=1;
 return 0;
}
