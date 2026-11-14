/* Reuse the existing fixed-decimal calculator without its 16-bit UI/driver. */
#include "stats.h"
#include "../../window/utilities.h"
#include "ui-calculator.h"
volatile KgStats calc_stats __attribute__((section(".data.gui_stats")))={.magic=0x3243474b,.version=1};
static Calculator calc;
static int draw(void){
 unsigned w=kg_width(),h=kg_height();kg_begin();
 kg_rect(0,0,w,h,0xeaf0f5);kg_rect(8,6,w-16,30,0xffffff);kg_text(16,13,calc_text(&calc),0x182c46);
 static const char *labels[]={"7","8","9","/","4","5","6","*","1","2","3","-","0",".","=","+","C","CE","+/-","%"};
 static const unsigned keys[]={'7','8','9','/','4','5','6','*','1','2','3','-','0','.','=','+','c',CALC_CE,CALC_SIGN,'%'};
 int bw=(w-24)/4,bh=(h-44)/5;
 for(unsigned i=0;i<20;i++)kg_button(keys[i],8+(i%4)*bw,40+(i/4)*bh,bw-4,bh-3,labels[i]);
 int e=kg_present();calc_stats.error=e<0?-e:0;if(!e)calc_stats.frames++;
 calc_stats.width=w;calc_stats.height=h;calc_stats.mode=calc.error;
 const char *s=calc_text(&calc);unsigned negative=*s=='-',value=0;if(negative)s++;for(;*s>='0'&&*s<='9';s++)value=value*10+*s-'0';calc_stats.value=negative?0u-value:value;return e;
}
int main(int argc,char **argv){
 int token=kg_open("Calculator",328,200);if(token<0){calc_stats.error=-token;return 1;}calc_stats.token=token;calc_init(&calc);
 if(draw()<0)return 2;
 if(argc>1&&same(argv[1],"fault"))*(volatile unsigned *)0x10000=1;
 for(;;){
  KgEvent event;int got=kg_poll(&event);if(got<0){calc_stats.error=-got;return 3;}calc_stats.heartbeat++;
  int dirty=0;
  if(got){calc_stats.events++;
   if(event.type==KG_KEY){calc_stats.keys++;if(event.key=='q'||event.key=='Q')break;dirty=calc_key(&calc,event.key);}
   if(event.type==KG_MOUSE&&(event.key&1)&&event.control){calc_stats.clicks++;dirty=calc_key(&calc,event.control);}
   if(event.type==KG_RESIZE)dirty=1;
  }
  if(dirty||calc_stats.error==22)draw();
  k32_wait(K32_WAIT_TIME,30);
 }
 return kg_close()?4:0;
}
