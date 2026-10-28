#include "../filer/filer.h"
#include "sdk.h"
int main(void){
    unsigned token=0;unsigned char value;
    /* Parent token belongs to another PSP, even within this session. */
    for(unsigned t=1;t<=2;t++)if(kapp_xms_read(t,0,&value,1)!=6){p_output("XMS CHILD OWNERSHIP FAIL\r\n");return 1;}
    if(kapp_xms_alloc(128,&token)){p_output("XMS CHILD ALLOC FAIL\r\n");return 1;}
    p_output("XMS CHILD OK\r\n");return 0; /* reap must free only the child's block */
}
