/* Real DOS client: assertions use live CPU results and monitor-owned records. */
#include "../window/k386_client.h"
#include "../window/hardware.h"
static unsigned stage;
static int call(unsigned command,unsigned id,unsigned key){return k386_command(command,id,key,0);}
static int contains(unsigned id,const char *needle){
    for(unsigned i=0;i<K386_SCREEN;i++){
        unsigned n=0;while(needle[n]&&i+n<K386_SCREEN&&k386_character(id,i+n)==needle[n])n++;
        if(!needle[n])return 1;
    }return 0;
}
int main(void){
    K386Usage a,b;K386Event e;
    stage=1;if(!k386_probe()||!k386_resources_available())goto fail;
    call(K386_END,0,0);call(K386_END,1,0);
    call(K386_BUDGET,0,0);call(K386_BUDGET,1,0);
    call(K386_PRIORITY,0,K386_HIGH);call(K386_PRIORITY,1,K386_LOW);
    if(k386_command(K386_CREATE,0,0,"COUNTER.EXE")||k386_command(K386_CREATE,1,0,"COUNTER.EXE"))goto fail;
    stage=2;for(unsigned n=0;n<100;n++)if(call(K386_SCHEDULE,0,0))goto fail;
    k386_usage(0,&a);k386_usage(1,&b);
    if(a.runs!=80||b.runs!=20||!a.cpu_ticks||!b.cpu_ticks||!k386_counter(0)||!k386_counter(1)||a.memory_kb!=664||a.depth!=1)goto fail;
    stage=3;call(K386_STOP,0,0);unsigned runs=a.runs,counter=k386_counter(0);
    for(unsigned n=0;n<16;n++)call(K386_SCHEDULE,0,0);
    k386_usage(0,&a);k386_usage(1,&b);
    if(a.runs!=runs||k386_counter(0)!=counter||b.runs!=36)goto fail;
    stage=4;if(k386_command(K386_CREATE,1,0,"COUNTER.EXE")||call(K386_BUDGET,1,25))goto fail;
    k386_usage(1,&b);unsigned generation=b.generation,physical=k386_word(1,16)|(k386_word(1,18)<<16);
    for(unsigned n=0;n<128&&k386_word(1,0)!=K386_STOPPED;n++)call(K386_SCHEDULE,0,0);
    k386_usage(1,&b);
    if(k386_word(1,0)!=K386_STOPPED||b.reason!=K386_REASON_LIMIT||b.cpu_ticks<b.limit_ticks||b.cpu_ticks>b.limit_ticks+K386_ARTIC_SECOND/10)goto fail;
    counter=k386_counter(1);runs=b.runs;
    for(unsigned n=0;n<8;n++)call(K386_SCHEDULE,0,0);
    k386_usage(1,&b);if(b.runs!=runs||counter!=k386_counter(1)||call(K386_RESUME,1,0)!=5)goto fail;
    stage=5;if(call(K386_BUDGET,1,0)||call(K386_RESUME,1,0)||call(K386_SCHEDULE,0,0))goto fail;
    k386_usage(1,&b);
    if(b.generation!=generation||k386_counter(1)<=counter||(k386_word(1,16)|(k386_word(1,18)<<16))!=physical)goto fail;
    stage=6;if(k386_command(K386_CREATE,1,0,"INTWAIT.EXE"))goto fail;
    k386_usage(1,&b);generation=b.generation;unsigned parent=b.psp;
    for(unsigned n=0;n<64;n++){call(K386_SCHEDULE,0,0);k386_usage(1,&b);if(b.depth==2)break;}
    if(b.depth!=2||b.psp==parent||call(K386_INTERRUPT,1,0))goto fail;
    k386_usage(1,&b);if(b.depth!=1||b.psp!=parent||b.generation!=generation||b.reason!=K386_REASON_INTERRUPT)goto fail;
    for(unsigned n=0;n<64&&k386_word(1,0)!=K386_ENDED;n++)call(K386_SCHEDULE,0,0);
    if(!contains(1,"INTERRUPT PARENT PASS")||k386_word(1,2)!=0)goto fail;
    stage=7;if(k386_command(K386_CREATE,1,0,"COUNTER.EXE")||k386_command(K386_CREATE,0,0,"FAULT.EXE"))goto fail;
    call(K386_STEP,1,0);counter=k386_counter(1);call(K386_STEP,0,0);
    if(k386_word(0,0)!=K386_FAULTED||!k386_history(0,&e)||e.type!=K386_EVENT_FAULT||e.fault!=14||e.cr2!=0xe0000||e.cs!=0x8010)goto fail;
    call(K386_STEP,1,0);if(k386_counter(1)<=counter)goto fail;
    /* Legacy callers above retain exact single-slice accounting. Exercise
       the opt-in bounded desktop batch and invalid-count refusal too. */
    stage=9;k386_usage(1,&b);runs=b.runs;
    if(call(K386_SCHEDULE,1,9)!=87)goto fail;
    k386_usage(1,&b);if(b.runs!=runs)goto fail;
    if(call(K386_SCHEDULE,1,8))goto fail;
    k386_usage(1,&b);if(b.runs<=runs||b.runs>runs+8)goto fail;
    stage=8;if(call(K386_PRIORITY,0,3)!=87||call(K386_BUDGET,0,60001)!=87)goto fail;
    for(unsigned n=0;n<20;n++)call(K386_PRIORITY,0,n%3);
    if(!k386_history(15,&e)||k386_history(16,&e))goto fail;
    k386_usage(0,&a);generation=a.generation;
    call(K386_END,0,0);k386_usage(0,&a);if(a.memory_kb||a.handles||a.depth)goto fail;
    if(k386_command(K386_CREATE,0,0,"ECHO.EXE"))goto fail;
    k386_usage(0,&a);if(a.generation!=generation+1||a.cpu_ticks||a.runs)goto fail;
    call(K386_END,0,0);call(K386_END,1,0);
    call(K386_PRIORITY,0,K386_NORMAL);call(K386_PRIORITY,1,K386_NORMAL);
    p_output("K386 RESOURCE CHECK PASS: weighted CPU, live time budget, retained RAM, child interrupt, fault journal, cleanup\r\n");return 0;
fail:
    p_output("K386 RESOURCE CHECK FAIL stage ");char number[3]={'0'+stage/10,'0'+stage%10,0};p_output(number);p_output("\r\n");return 1;
}
