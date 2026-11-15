/* DOS client proves the process service also runs without the graphical loader. */
#include "../window/hardware.h"
#include "../window/k386_client.h"
#include "../filer/filer.h"
volatile struct {char signature[8];unsigned version,size,phase,error,capacity,steps,exit_code,largest_before,largest_after;} process_run={.signature={'K','P','R','U','N','0','0','1'},.version=1,.size=sizeof process_run};
static unsigned largest(void){unsigned a=0x4800,b=65535;__asm__ volatile("int $0x21":"+a"(a),"+b"(b)::"cc","memory");return b&65535;}
static void pump(void){unsigned a=0xd3a0,b=2;__asm__ volatile("pushw %%es; int $0x2f; popw %%es":"+a"(a):"b"(b):"ecx","edx","esi","edi","cc","memory");}
int main(void){
 p_init();process_run.phase=1;process_run.largest_before=largest();if(!k386_probe()){process_run.error=1;goto done;}
 for(unsigned i=0;i<k386_capacity();i++)if(k386_word(i,0)!=K386_ENDED){process_run.error=2;goto done;}
 unsigned expanded=!k386_capacity_request(4);process_run.capacity=k386_capacity();
 const char *command=expanded?"KSH32.EXE /C \"KTOOL32.EXE echo headless | KTOOL32.EXE upper\"":"KSH32.EXE /C \"KTOOL32.EXE echo headless\"";
 if(k386_command(K386_CREATE,0,0,command)){process_run.error=3;goto cleanup;}
 process_run.phase=2;
 for(unsigned i=0;i<20000;i++){
  if(k386_command(K386_SCHEDULE,0,4,0)){process_run.error=4;break;}pump();process_run.steps++;
  unsigned state=k386_word(0,0);if(state==K386_FAULTED){process_run.error=5;break;}if(state==K386_ENDED){process_run.exit_code=k386_word(0,2);process_run.error=process_run.exit_code?6:0;break;}if(i==19999)process_run.error=7;
 }
 cleanup:
 k386_command(K386_END,0,0,0);pump();for(unsigned i=1;i<k386_capacity();i++)if(k386_word(i,0)==K386_RUNNING||k386_word(i,0)==K386_WAITING)process_run.error=8;
 if(expanded&&k386_capacity_request(2))process_run.error=9;
 done:process_run.largest_after=largest();if(process_run.largest_before!=process_run.largest_after)process_run.error=10;process_run.phase=3;p_output(process_run.error?"KPRUN FAIL\r\n":"KPRUN PASS: headless protected processes and streams\r\n");return process_run.error;
}
