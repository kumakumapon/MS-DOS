#include "utilities.h"
#include <assert.h>
#include <string.h>
static void keys(Calculator *c,const char *s){for(;*s;s++)calc_key(c,*s);}
static void expect(Calculator *c,const char *s){assert(!strcmp(calc_text(c),s));}
int main(void){
 Calculator c;calc_init(&c);keys(&c,"0.1+0.2=");expect(&c,"0.3");
 calc_init(&c);keys(&c,"2+3*4==");expect(&c,"80");keys(&c,"7=");expect(&c,"7");
 calc_init(&c);keys(&c,"1/6=");expect(&c,"0.1667");
 calc_init(&c);keys(&c,"1s/32=");expect(&c,"-0.0313");
 calc_init(&c);keys(&c,"23.4567s*1.5=");expect(&c,"-35.1851");
 calc_init(&c);keys(&c,"99999999*1=");expect(&c,"99999999");
 calc_init(&c);keys(&c,"9999.9999*9999.9999=");expect(&c,"99999998");
 calc_init(&c);keys(&c,"99999999.9999/99999999.9999=");expect(&c,"1");
 calc_init(&c);keys(&c,"99999999.9999+0.0001=");expect(&c,"Overflow");keys(&c,"3+4=");expect(&c,"7");
 calc_init(&c);keys(&c,"99999999*99999999=");expect(&c,"Overflow");
 calc_init(&c);keys(&c,"9/0=");expect(&c,"Divide by zero");keys(&c,"c8/2");calc_key(&c,CALC_CE);keys(&c,"3=");expect(&c,"2.6667");
 calc_init(&c);keys(&c,"20%");expect(&c,"0.2");
 calc_init(&c);keys(&c,"5");calc_key(&c,CALC_PLUS);keys(&c,"c2");calc_key(&c,CALC_MINUS);calc_key(&c,CALC_MR);expect(&c,"3");
 keys(&c,"c");assert(c.memory==30000);calc_key(&c,CALC_MC);calc_key(&c,CALC_MR);expect(&c,"0");
 assert(calc_paste(&c," \r\n-12.34 ",10));expect(&c,"-12.34");
 assert(!calc_paste(&c,"1\0.2",4));expect(&c,"-12.34");
 assert(!calc_paste(&c,"1e3",3));expect(&c,"-12.34");assert(!calc_paste(&c,"1.12345",7));expect(&c,"-12.34");
 calc_init(&c);keys(&c,"99999999.9999");assert(calc_key(&c,'1')==2);expect(&c,"99999999.9999");
 calc_init(&c);keys(&c,"1.25\b");expect(&c,"1.2");
 return 0;
}
