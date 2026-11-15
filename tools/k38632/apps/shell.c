#include "process.h"
#include "io.h"
#include "libc.h"
#include "../../kproc/shell.h"
volatile unsigned shell_stats[16] __attribute__((section(".data.stats")))={0x3248534b,1};
static unsigned char output[1048576],input[4096],received[256];
static KpShell plan;
static void finish(unsigned *children,unsigned n){for(unsigned i=0;i<n;i++)if(children[i])k32_process_cancel(children[i]);for(unsigned i=0;i<n;i++)if(children[i])for(unsigned j=0;j<4096;j++){int e=k32_process_wait(children[i],0,0);if(e!=-11)break;k32_wait(K32_WAIT_TIME,16);}}
static int execute(const char *line){
 shell_stats[4]=shell_stats[5]=shell_stats[6]=0;
 int error=kp_shell_parse(line,&plan);if(error)return error;
 unsigned children[2]={0},ends[3][2]={{0}},pipes=0,source=0,source_bytes=0,source_at=0,chunk=0,chunk_at=0,destination=0,bytes=0,eof=0,done=0,keyboard_head=0,keyboard_count=0,stdin_eof=0,console_input=0;
 unsigned char keyboard[64];
 K32Revision revision={0};
 if(plan.output[0]){error=k32_io_revision(plan.output,0,&revision);if(error)return error;}
 if(plan.input[0]){int h=k32_io_open(plan.input,0,&source_bytes);if(h<0)return h;source=(unsigned)h;}
 unsigned in_pipe=3,middle=3,out_pipe=3;
 in_pipe=pipes++;error=k32_pipe(ends[in_pipe]);if(error)goto cleanup;
 if(plan.count==2){middle=pipes++;error=k32_pipe(ends[middle]);if(error)goto cleanup;}
 out_pipe=pipes++;error=k32_pipe(ends[out_pipe]);if(error)goto cleanup;
 for(unsigned i=0;i<plan.count;i++){
  unsigned in=i?ends[middle][0]:in_pipe<3?ends[in_pipe][0]:0;
  unsigned out=i+1<plan.count?ends[middle][1]:out_pipe<3?ends[out_pipe][1]:0;
  int h=k32_spawn(plan.command[i],in,out);if(h<0){error=h;goto cleanup;}children[i]=(unsigned)h;
 }
 /* Queued children already own their endpoint copies. Parent retains only pumps. */
 for(unsigned i=0;i<pipes;i++)for(unsigned j=0;j<2;j++)if(!(i==in_pipe&&j==1)&&!(i==out_pipe&&!j)){error=k32_pipe_close(ends[i][j]);if(error)goto cleanup;ends[i][j]=0;}
 if(out_pipe<3)destination=ends[out_pipe][0];else eof=1;
 shell_stats[1]=2;shell_stats[2]=children[0];shell_stats[3]=children[1];
 while(done<plan.count||!eof){
  int key=k32_poll_key();if(key>=0){shell_stats[9]++;shell_stats[11]=(unsigned)key;}if(key>=0&&(key&255)==3){error=-130;goto cleanup;}
  if(!plan.input[0]&&ends[in_pipe][1]){
   if(!console_input&&!stdin_eof&&keyboard_count<sizeof keyboard){unsigned char byte;int n=k32_pipe_read(0,&byte,1);if(n==-38)console_input=1;else if(!n)stdin_eof=1;else if(n==1){keyboard[(keyboard_head+keyboard_count++)%sizeof keyboard]=byte;}else if(n!=-11){error=n;goto cleanup;}}
   if(console_input&&key>=0&&(key&255)){unsigned c=key&255;if(c==4)stdin_eof=1;else if(keyboard_count+2<=sizeof keyboard){keyboard[(keyboard_head+keyboard_count++)%sizeof keyboard]=(unsigned char)c;if(c==13)keyboard[(keyboard_head+keyboard_count++)%sizeof keyboard]=10;}}
   if(keyboard_count){int n=k32_pipe_write(ends[in_pipe][1],keyboard+keyboard_head,1);if(n>0){keyboard_head=(keyboard_head+1)%sizeof keyboard;keyboard_count--;}else if(n==-32){error=k32_pipe_close(ends[in_pipe][1]);if(error)goto cleanup;ends[in_pipe][1]=0;keyboard_count=0;}else if(n!=-11){error=n;goto cleanup;}}
   if(stdin_eof&&!keyboard_count&&ends[in_pipe][1]){error=k32_pipe_close(ends[in_pipe][1]);if(error)goto cleanup;ends[in_pipe][1]=0;}
  }

  if(source&&in_pipe<3&&ends[in_pipe][1]){
   if(chunk_at==chunk&&source_at<source_bytes){unsigned remaining=source_bytes-source_at;if(remaining>sizeof input)remaining=sizeof input;int n=k32_io_read(source,input,remaining);if(n<=0){error=n<0?n:-5;goto cleanup;}chunk=(unsigned)n;chunk_at=0;source_at+=(unsigned)n;}
   if(chunk_at<chunk){unsigned n=chunk-chunk_at;if(n>256)n=256;int e=k32_pipe_write(ends[in_pipe][1],input+chunk_at,n);if(e==-32){k32_pipe_close(ends[in_pipe][1]);ends[in_pipe][1]=0;}else if(e>0)chunk_at+=(unsigned)e;else if(e!=-11){error=e;goto cleanup;}}
   if(source_at==source_bytes&&chunk_at==chunk&&ends[in_pipe][1]){error=k32_pipe_close(ends[in_pipe][1]);if(error)goto cleanup;ends[in_pipe][1]=0;}
  }
  if(destination&&!eof){int n=k32_pipe_read(destination,received,sizeof received);if(n>0){if(plan.output[0]){if((unsigned)n>sizeof output-bytes){error=-27;goto cleanup;}k32_memcpy(output+bytes,received,(unsigned)n);}else{int e=k32_stream_write(received,(unsigned)n);if(e!=n){error=e<0?e:-5;goto cleanup;}}bytes+=(unsigned)n;}else if(!n)eof=1;else if(n!=-11){error=n;goto cleanup;}}
  for(unsigned i=0;i<plan.count;i++)if(children[i]){unsigned code,why;int e=k32_process_wait(children[i],&code,&why);if(!e){children[i]=0;done++;if(code||why){error=code?-(int)code:-5;goto cleanup;}}else if(e!=-11){error=e;goto cleanup;}}
  shell_stats[4]=bytes;shell_stats[5]=source_at;shell_stats[6]=done;if(done<plan.count||!eof)k32_wait(K32_WAIT_TIME,1);
 }
 if(plan.output[0])error=k32_io_save(plan.output,output,bytes,&revision);
 cleanup:
 if(error)finish(children,plan.count);
 if(source){int e=k32_io_close(source);if(!error)error=e;}
 for(unsigned i=0;i<pipes;i++)for(unsigned j=0;j<2;j++)if(ends[i][j]){int e=k32_pipe_close(ends[i][j]);if(!error)error=e;}
 shell_stats[1]=1;shell_stats[4]=bytes;return error;
}
static int run_line(const char *line){int e=execute(line);shell_stats[1]=1;shell_stats[7]=(unsigned)e;shell_stats[8]++;return e;}
int main(int argc,char **argv){
 if(!(k32_query()&K32_FEATURE_PROCESS))return 38;
 k32_printf("KSH32: protected processes, one pipe, Work <input >output\r\n");
 char line[128];unsigned used=0;
 if(argc>1&&!k32_strcmp(argv[1],"/C")){for(int i=2;i<argc;i++){unsigned n=k32_strlen(argv[i]);if(used+n+(used?1:0)>=sizeof line)return 22;if(used)line[used++]=' ';k32_memcpy(line+used,argv[i],n);used+=n;}line[used]=0;int e=run_line(line);k32_printf("KSH32 status %d\r\n",e);return e<0?-e:e;}
 for(;;){k32_printf("K32> ");used=0;shell_stats[10]=0;for(;;){int c=k32_getchar();if(c<0)return 1;shell_stats[9]++;shell_stats[11]=(unsigned)c;if(c==3){used=0;line[0]=0;k32_write("^C\r\n",4);break;}if(c=='\r'||c=='\n'){line[used]=0;k32_write("\r\n",2);break;}if(c==8){if(used){used--;k32_write("\b \b",3);}continue;}if(c>=32&&c<127&&used<126){line[used++]=(char)c;shell_stats[10]=used;k32_putchar((unsigned)c);}}
  if(!used)continue;
  if(!k32_strcmp(line,"exit"))return 0;
  if(!k32_strcmp(line,"help")){k32_printf("KTOOL32.EXE echo text | KTOOL32.EXE upper >OUT.TXT\r\nKTOOL32.EXE cat <IN.TXT >COPY.TXT\r\nexit; Ctrl+C cancels foreground; End closes shell and children\r\n");continue;}
  int e=run_line(line);if(e)k32_printf("KSH32 error %d; output original retained\r\n",e);
 }
}
