#include "k32x.h"
#include "libc.h"
int main(int argc,char **argv){
 char line[96];int n=k32_snprintf(line,sizeof line,"Hello from %s; argc=%d\r\n",argv[0],argc);
 if(n<0||(unsigned)n>=sizeof line)return 1;
 return k32_write(line,(unsigned)n)==n?0:2;
}
