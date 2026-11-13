#include "bank.h"
#include "../window/hardware.h"
void kbank_copy(k16 segment,const KBankSpan *s,unsigned count,int save){
 u16 ds;__asm__ volatile("movw %%ds,%0":"=r"(ds));
 for(unsigned i=0;i<count;i++)if(save)mem_copy(segment,s[i].offset,ds,(unsigned)s[i].local,s[i].bytes);
 else mem_copy(ds,(unsigned)s[i].local,segment,s[i].offset,s[i].bytes);
}
