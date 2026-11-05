/* Parent-side ownership/reclamation proof: two existing PM32 workers must
 * survive child GUI execution, including its fault and forced-stop paths. */
#include "../k38632/host.h"
#include "../window/hardware.h"
#include "../window/k386_client.h"
#include "../filer/filer.h"
static unsigned seg,off,owned[2];
volatile struct {
 char signature[8];u32 version,size,tag,error,loops;
 u32 generation[2],pd[2],physical[2],before[2],after[2],cs[2],flags[2],heap[2];
 u32 free_before,free_after,largest_before,largest_after,dos_before,dos_after;
} proof={.signature={'K','G','P','R','O','O','F','1'},.version=1,.size=sizeof proof};
static unsigned word(unsigned at){return mem_get(seg,off+at)|(unsigned)mem_get(seg,off+at+1)<<8;}
static u32 dword(unsigned at){return word(at)|(u32)word(at+2)<<16;}
static unsigned largest(void){unsigned a=0x4800,b=65535;__asm__ volatile("int $0x21":"+a"(a),"+b"(b)::"cc","memory");return b&65535;}
static void xfree(u32 *total,u32 *block){
 unsigned a=0x4310,b;u16 es;__asm__ volatile("pushw %%es; int $0x2f; movw %%es,%1; popw %%es":"+a"(a),"=c"(es),"=b"(b)::"edx","esi","edi","cc","memory");
 u32 entry=(u32)es<<16|(b&65535),d=0;a=0x800;
 __asm__ volatile("pushw %%ds; pushw %%es; lcallw *%2; popw %%es; popw %%ds":"+a"(a),"+d"(d):"m"(entry):"ebx","ecx","esi","edi","cc","memory");
 *total=d&65535;*block=a&65535;
}
int main(void){
 p_init();proof.tag=input_ticks()|1;proof.dos_before=largest();
 if(!k386_probe()){proof.error=1;goto done;}
 /* No active desktop: a GUI client must be refused before any CPU/owner change. */
 for(unsigned i=0;i<2;i++)if(k386_word(i,0)){proof.error=2;goto done;}
 if(k386_command(K386_CREATE,0,0,"KGCALC.EXE")!=50||k386_word(0,0)){proof.error=15;goto done;}
 for(unsigned id=0;id<2;id++){unsigned state=k386_word(id,0);if(state==1||state==2||state==3){proof.error=2;goto done;}}
 unsigned a=0xd390,b=0,di;u16 es;__asm__ volatile("pushw %%es; int $0x2f; movw %%es,%1; popw %%es":"+a"(a),"=c"(es),"=D"(di):"b"(b):"edx","esi","cc","memory");
 if((a&65535)!=0x390d){proof.error=3;goto done;}seg=es;off=di&65535;
 for(unsigned id=0;id<2;id++){
  if(k386_command(K386_CREATE,id,0,id?"COUNT32.EXE 2":"COUNT32.EXE 1")){proof.error=4;goto cleanup;}owned[id]=1;
 }
 for(unsigned n=0;n<16;n++)k386_tick();
 for(unsigned id=0;id<2;id++){
  K386Usage u;if(!k386_usage(id,&u)){proof.error=5;goto cleanup;}
  unsigned at=__builtin_offsetof(K32HostMailbox,task)+id*sizeof(K32HostView)+__builtin_offsetof(K32HostView,record);
  proof.generation[id]=u.generation;proof.pd[id]=dword(at+24);proof.physical[id]=dword(at+28);proof.before[id]=k386_counter(id);
  proof.cs[id]=dword(at+380);proof.flags[id]=dword(at+384);proof.heap[id]=dword(at+788);
  if(!proof.before[id]||proof.cs[id]!=59||(proof.flags[id]&0x20000)||proof.heap[id]!=2){proof.error=6;goto cleanup;}
 }
 u32 total,block;xfree(&total,&block);proof.free_before=total;proof.largest_before=block;
 /* Each child returns to this still-live DOS parent. The GUI must refuse T
  * ownership while these slots are already occupied and preserve both. */
 const char *args[]={"KGUI32.EXE proof","KGUI32.EXE badapi","KGUI32.EXE fault","KGUI32.EXE sessionapi"};
 for(unsigned n=0;n<4;n++){
  int code=p_exec("KGUILDR.EXE",args[n]);
  if(code!=(n==2?1:0)){proof.error=7+n;goto cleanup;}proof.loops++;
  for(unsigned id=0;id<2;id++){
   K386Usage u;unsigned at=__builtin_offsetof(K32HostMailbox,task)+id*sizeof(K32HostView)+__builtin_offsetof(K32HostView,record);
   if(!k386_usage(id,&u)||u.generation!=proof.generation[id]||dword(at+24)!=proof.pd[id]||dword(at+28)!=proof.physical[id]||dword(at+788)!=2||k386_counter(id)<=proof.before[id]){proof.error=10+n;goto cleanup;}
   proof.after[id]=k386_counter(id);
  }
 }
 xfree(&total,&block);proof.free_after=total;proof.largest_after=block;
 if(proof.free_before!=total||proof.largest_before!=block)proof.error=13;
cleanup:
 for(unsigned id=0;id<2;id++)if(owned[id])k386_command(K386_END,id,0,0);
done:
 proof.dos_after=largest();if(proof.dos_after!=proof.dos_before&&!proof.error)proof.error=14;
 p_output(proof.error?"KGPROOF FAIL\r\n":"KGPROOF PASS: two existing owners survive GUI exit/fault; DOS and XMS reclaimed\r\n");
 return proof.error;
}
