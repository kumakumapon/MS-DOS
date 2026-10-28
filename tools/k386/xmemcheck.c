/* Actual V86/XMS diagnostic. Each instance keeps a different 1 MiB pattern. */
#include "../filer/filer.h"
#include "sdk.h"
static unsigned char buffer[4096];
static unsigned token;
static int fail(const char *message){p_output("XMS FAIL: ");p_output(message);p_output("\r\n");return 1;}
static unsigned char pattern(unsigned at,unsigned session){return (at^(at>>8)^(at>>16)^(session*73))&255;}
static int check(unsigned size,unsigned session){
    for(unsigned at=0;at<size;at+=sizeof buffer){
        if(kapp_xms_read(token,at,buffer,sizeof buffer))return fail("zero read");
        for(unsigned i=0;i<sizeof buffer;i++)if(buffer[i])return fail("not initialized");
        for(unsigned i=0;i<sizeof buffer;i++)buffer[i]=pattern(at+i,session);
        if(kapp_xms_write(token,at,buffer,sizeof buffer))return fail("pattern write");
    }
    for(unsigned at=0;at<size;at+=sizeof buffer){
        if(kapp_xms_read(token,at,buffer,sizeof buffer))return fail("pattern read");
        for(unsigned i=0;i<sizeof buffer;i++)if(buffer[i]!=pattern(at+i,session))return fail("pattern mismatch");
    }
    return 0;
}
int main(void){
    unsigned session=kapp_session();char number[12];
    if(kapp_xms_version()!=0x100)return fail("API missing");
    p_output("XMS allocating 1024 KiB; GUI remains active\r\n");
    if(kapp_xms_alloc(1024,&token))return fail("allocate");
    if(check(1024*1024,session))return 1;
    unsigned extra;KAppXmsUsage usage;
    if(kapp_xms_usage(&usage)||usage.process_kib!=1024||usage.process_blocks!=1)return fail("usage");
    if(kapp_xms_alloc(0,&extra)!=87||kapp_xms_alloc(4096,&extra)!=8)return fail("quota");
    /* Odd offsets/counts, end byte and overflow must all be safe. */
    buffer[0]=0xa5;
    if(kapp_xms_write(token,1024*1024-1,buffer,1)||kapp_xms_read(token,1024*1024-1,buffer,1)||buffer[0]!=0xa5)return fail("last odd byte");
    if(kapp_xms_write(token,65535,buffer,3)||kapp_xms_read(token,65535,buffer,3))return fail("64 KiB crossing");
    if(kapp_xms_read(token,1024*1024,buffer,1)!=87||kapp_xms_read(token,0xffffffffu,buffer,2)!=87||kapp_xms_read(token,0,buffer,4097)!=87)return fail("range accepted");
    fd_number(number,token);p_output("XMS READY token=");p_output(number);
    fd_number(number,session);p_output(" session=");p_output(number);
    p_output("\r\n1 MiB verified incl. 64 KiB boundary\r\n");
    p_output("Q exit, F fault, P child, T try handle, R recheck\r\n");
    for(;;){
        unsigned a=0x0800;__asm__ volatile("int $0x21":"+a"(a)::"cc","memory");unsigned key=a&255;
        if(key=='q'||key=='Q')return 0; /* intentionally leave allocation for automatic reap */
        if(key=='f'||key=='F'){__asm__ volatile("pushw %%es; movw $0xe000,%%ax; movw %%ax,%%es; movb $1,%%es:0; popw %%es":::"eax","memory");return fail("fault survived");}
        if(key=='t'||key=='T'){
            unsigned foreign=token==1?2:1;
            if(kapp_xms_read(foreign,0,buffer,1)!=6)return fail("foreign handle accepted");
            p_output("XMS FOREIGN REJECTED\r\n");
        }
        if(key=='r'||key=='R'){
            if(kapp_xms_read(token,0x90001,buffer,4095))return fail("retained read");
            for(unsigned i=0;i<4095;i++)if(buffer[i]!=pattern(0x90001+i,session))return fail("retained data");
            p_output("XMS RETAINED DATA OK\r\n");
        }
        if(key=='p'||key=='P'){
            if(p_exec("XCHILD.EXE",""))return fail("child EXEC");
            if(kapp_xms_read(token,0x90001,buffer,1)||buffer[0]!=pattern(0x90001,session))return fail("parent allocation lost");
            p_output("XMS PARENT RETAINED\r\n");
        }
        kapp_yield();
    }
}
