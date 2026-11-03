#include "k32x.h"
int main(int argc,char **argv){
 const K32Info *i=k32_info();
 if(i->magic!=0x3149324b||i->abi!=257||i->size!=64||i->argc!=(unsigned)argc||i->features!=15||k32_query()!=15||i->page_size!=4096||i->tick_rate!=307200)return 1;
 if(argc!=3||k32_len(argv[1])!=9||k32_len(argv[2])!=5)return 2;
 const char *a="two words",*b="three";for(unsigned j=0;a[j];j++)if(argv[1][j]!=a[j])return 3;
 for(unsigned j=0;b[j];j++)if(argv[2][j]!=b[j])return 4;
 k32_puts("ARGS32: startup and quoted arguments OK\r\n");return 0;
}
