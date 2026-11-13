/* Dedicated desktop context. Shared CPU engine, independent DOS allocation,
 * stacks, page tables and XMS owner; neither application slot is a GUI slot. */
#include "gui.h"
#include "../k38632/services.h"
#include "../window/hardware.h"
#include "../window/k386_client.h"
extern u8 results[1024];extern u32 context[19],loader_request,xms_entry,initialized;
extern K32Plan loader_plan;extern K32Args loader_args;
extern int loader_open(void),services_dispatch(void),services_wait(void),host_acquire(void);
extern void loader_close(void),services_close(void),host_enter(void),host_drop_xms(void);
extern int gui_copy(u32,void *,unsigned,int),gui_copy_far(u32,unsigned,unsigned),gfx_font_copy(unsigned);
extern int gfx_glyph_copy(unsigned,u8 [32]);
extern void gfx_driver_release(void);
char host_command[128];
volatile GuiDriverRecord gui_record={.signature={'K','G','3','2','D','R','V','1'},.version=1,.size=sizeof(GuiDriverRecord)};
static GuiInfo info;static GuiEvent queue[32];
static unsigned head,count,segment,scratch,tables,started,quit;
static u32 last_schedule,last_timer;
static unsigned expanded_here;
unsigned manager,owned[4],session_focus;u32 owned_generation[4];
extern int gui_session(u32,unsigned),gui_snapshot(u32,unsigned),kg_clients_start(void),kg_clients_snapshot(u32,unsigned),kg_clients_event(u32,unsigned);
extern int kg_clients_surface_ack(u32,unsigned);
extern void kg_clients_close(void),kg_clients_tick(void);
static Mouse previous;
static u32 get(unsigned at){return *(u32 *)(results+at);}
static void put(unsigned at,u32 n){*(u32 *)(results+at)=n;}
static unsigned largest(void){unsigned a=0x4800,b=65535;__asm__ volatile("int $0x21":"+a"(a),"+b"(b)::"cc","memory");return b&65535;}
int gui_output_range(u32 at,unsigned n){extern u8 loader_map[1024];return k32_user_access(loader_map,at,n,1);}
void host_physical32(u32 at,u32 value){for(unsigned i=0;i<4;i++)mem_put(at>>4,(at&15)+i,value>>(i*8));}
int host_write(const void *p,unsigned n){(void)p;return (int)n;}
static void workers(void){
 gui_record.owned_workers=gui_record.appmask=0;
 for(unsigned id=0;id<k386_capacity();id++){
  K386Usage u;unsigned generation=0;
  if(manager&&k386_usage(id,&u)){if(owned[id]&&owned_generation[id]!=u.generation)owned[id]=0;generation=u.generation;}
  unsigned counter=manager?k386_counter(id):0,state=manager?k386_word(id,0):0;
  if(id<2){gui_record.worker_generation[id]=generation;gui_record.worker_counter[id]=counter;gui_record.worker_state[id]=state;}
  else{gui_record.reserved[2+id-2]=generation;gui_record.reserved[4+id-2]=counter;gui_record.reserved[6+id-2]=state;}
  gui_record.owned_workers|=owned[id]<<id;gui_record.appmask|=(state!=0)<<id;
 }
 gui_record.reserved[1]=k386_capacity();
 if(session_focus&&session_focus<=k386_capacity()&&!owned[session_focus-1])session_focus=0;
}
static void stop_workers(void){
 for(unsigned id=0;id<k386_capacity();id++)if(owned[id]){
  K386Usage u;if(k386_usage(id,&u)&&u.generation==owned_generation[id])k386_command(K386_END,id,0,0);
  owned[id]=0;
 }workers();
}
static int start_workers(void){
 if(!manager)return -38;
 for(unsigned id=0;id<k386_capacity();id++)if(owned[id])return 0;
 for(unsigned id=0;id<k386_capacity();id++){unsigned s=k386_word(id,0);if(s==K386_RUNNING||s==K386_WAITING||s==K386_STOPPED)return -16;}
 for(unsigned id=0;id<k386_capacity();id++){
  if(k386_command(K386_CREATE,id,0,id?"COUNT32.EXE 2":"COUNT32.EXE 1")){stop_workers();return -12;}
  K386Usage u;if(!k386_usage(id,&u)){stop_workers();return -5;}
  owned[id]=1;owned_generation[id]=u.generation;
 }workers();return 0;
}
static void enqueue(GuiEvent e){
 if(count&&e.type==GUI_MOTION&&queue[(head+count-1)%32].type==GUI_MOTION){queue[(head+count-1)%32]=e;return;}
 if(count==32){head=(head+1)%32;count--;gui_record.input_lost++;}
 queue[(head+count++)%32]=e;
}
static void pump(void){
 for(unsigned n=0;n<16;n++){
  Mouse m=mouse_read();
  if(m.x!=previous.x||m.y!=previous.y||m.buttons!=previous.buttons)
   enqueue((GuiEvent){.type=m.buttons!=previous.buttons?GUI_BUTTON:GUI_MOTION,.x=m.x,.y=m.y,.buttons=m.buttons,.ticks=m.ticks});
  previous=m;if(!mouse_queued())break;
 }
 for(unsigned n=0;n<16&&key_ready();n++){
  unsigned k=key_read();gui_record.last_key=k;
  extern int kg_clients_close_guard(void);
  if(k==7||(k==KEY_F1+9&&!kg_clients_close_guard())||(k==27&&!session_focus))quit=1;
  else enqueue((GuiEvent){.type=GUI_KEY,.key=k,.x=previous.x,.y=previous.y,.buttons=key_modifiers(),.ticks=input_ticks()});
 }
}
static int event(u32 pointer,unsigned bytes){
 if(bytes!=sizeof(GuiEvent)&&bytes!=sizeof(GuiEvent4))return -22;
 GuiEvent4 extended={.base={.type=GUI_TIMER,.x=previous.x,.y=previous.y,.buttons=previous.buttons,.ticks=input_ticks()}};
 GuiEvent *e=&extended.base;
 if(count)*e=queue[head];else if(((e->ticks-last_timer)&0xffffff)<30720)return 0;
 e->sequence=gui_record.events+1;workers();
 for(unsigned id=0;id<2;id++){e->states[id]=gui_record.worker_state[id];e->counters[id]=gui_record.worker_counter[id];extended.extra_states[id]=gui_record.reserved[6+id];extended.extra_counters[id]=gui_record.reserved[4+id];}
 e->lost=gui_record.input_lost;
 int error=gui_copy(pointer,&extended,bytes,1);if(error)return error;
 if(count){head=(head+1)%32;count--;}else last_timer=e->ticks;
 gui_record.events++;return 1;
}
static int present(u32 pointer,unsigned bytes){
 GuiPresent r;if(bytes!=sizeof r)return -22;
 int error=gui_copy(pointer,&r,sizeof r,0);if(error)return error;
 if(!r.width||!r.height||r.x>=info.width||r.y>=info.height||r.width>info.width-r.x||r.height>info.height-r.y||r.pitch!=r.width*info.bpp)return -22;
 u32 length=r.pitch*r.height;
 extern u8 loader_map[1024];if(!k32_user_access(loader_map,r.pixels,length,0))return -14;
 gui_record.framebuffer=r.pixels;gui_record.rect[0]=r.x;gui_record.rect[1]=r.y;gui_record.rect[2]=r.width;gui_record.rect[3]=r.height;
 gfx_target(1);unsigned rows=32768/r.pitch;
 for(unsigned y=0;y<r.height;){
  unsigned n=r.height-y;if(n>rows)n=rows;
  /* Read full packed rows in even-sized bands. Odd indexed rows use a
   * one-row near copy so no out-of-range padding is requested from user RAM. */
  if((n*r.pitch)&1){
   u8 line[800];error=gui_copy(r.pixels+y*r.pitch,line,r.pitch,0);if(error)return error;
   gfx_write_span(r.x,r.y+y,segment,(unsigned)line,r.width);y++;
  }else{
   error=gui_copy_far(r.pixels+y*r.pitch,scratch,n*r.pitch);if(error)return error;
   for(unsigned i=0;i<n;i++)gfx_write_span(r.x,r.y+y+i,scratch,i*r.pitch,r.width);
   y+=n;
  }input_poll();
 }
 gfx_present(r.x,r.y,r.width,r.height);gui_record.presents++;return 0;
}
static int dispatch(void){
 unsigned nr=loader_request;u32 b=context[8],c=context[10];
 if(nr<64)return services_dispatch();
 loader_request=0;int error;
 switch(nr){
 case GUI_INFO:error=c!=sizeof info?-22:gui_copy(b,&info,sizeof info,1);break;
 case GUI_EVENT:error=event(b,c);break;
 case GUI_PRESENT:error=present(b,c);break;
 case GUI_FONT:
  error=-22;if(c==95*16&&gfx_font_copy(scratch)){
   u8 font[95*16];mem_copy(segment,(unsigned)font,scratch,0,sizeof font);error=gui_copy(b,font,sizeof font,1);
  }else if(c==sizeof(GuiGlyph)){
   GuiGlyph r;error=gui_copy(b,&r,sizeof r,0);if(error)break;
   if(r.version!=1||r.size!=sizeof r||r.columns){error=-22;break;}
   extern u8 loader_map[1024];if(!k32_user_access(loader_map,b,sizeof r,1)){error=-14;break;}
   r.columns=gfx_glyph_copy(r.code,r.bitmap);error=r.columns?gui_copy(b,&r,sizeof r,1):-22;
  }break;
 case GUI_PROOF:error=b==1?start_workers():b==2?(stop_workers(),0):-22;break;
 case GUI_SESSION:error=gui_session(b,c);break;
 case GUI_SNAPSHOT:error=gui_snapshot(b,c);break;
 case GUI_CLIENT_VIEW:error=kg_clients_snapshot(b,c);break;
 case GUI_CLIENT_EVENT:error=kg_clients_event(b,c);break;
 case GUI_SURFACE_ACK:error=kg_clients_surface_ack(b,c);break;
 default:gui_record.unknown++;return -38;
 }if(error<0)gui_record.rejected++;return error;
}
int gui_main(void){
 for(unsigned i=0;i<16;i++)*(volatile u8 *)(0xd400+i)=0xa5;
 p_init();__asm__ volatile("movw %%ds,%w0":"=r"(segment));
 gui_record.owner=segment;gui_record.cookie=input_ticks()|1;gui_record.runtime=(u32)segment*16+(unsigned)results;
 gui_record.largest_before=largest();gui_record.state=1;
 u16 msw;u8 mask;__asm__ volatile("smsw %0; inb $2,%1":"=r"(msw),"=a"(mask));
 if((msw&1)||!(mask&1)){gui_record.error=1;goto done;}
 unsigned a=0x4300;__asm__ volatile("int $0x2f":"+a"(a)::"ebx","ecx","edx","esi","edi","cc","memory");if((a&255)!=128){gui_record.error=2;goto done;}
 unsigned bx;u16 es;a=0x4310;__asm__ volatile("pushw %%es; int $0x2f; movw %%es,%1; popw %%es":"+a"(a),"=c"(es),"=b"(bx)::"edx","esi","edi","cc","memory");xms_entry=(u32)es<<16|(bx&65535);
 a=0x6200;__asm__ volatile("int $0x21":"+a"(a),"=b"(bx)::"cc","memory");put(44,bx);put(20,(u32)segment*16);put(56,gui_record.cookie);put(492,~0u);put(504,(unsigned)results);
 unsigned n=mem_get(bx,0x80);if(n>126){gui_record.error=3;goto done;}
 for(unsigned i=0;i<n;i++)host_command[i]=mem_get(bx,0x81+i);
 host_command[n]=0;
 if(!n){const char *s="KGUI32.EXE";for(unsigned i=0;;i++){host_command[i]=s[i];if(!s[i])break;}}
 if(loader_open()||!(loader_plan.features&16)){gui_record.error=4;goto cleanup;}
 tables=mem_alloc(16384);scratch=mem_alloc(32768);gui_record.tables=tables;gui_record.scratch=scratch;
 if(!tables||!scratch){gui_record.error=8;goto cleanup;}
 put(24,((u32)tables*16+4095)&~4095u);
 if(host_acquire()){gui_record.error=get(16);goto cleanup;}
 manager=k386_probe();if(manager&&k386_capacity()==2&&!k386_capacity_request(4))expanded_here=1;gfx_highcolor(1);gfx_start();started=1;
 if(gfx_colors()<256){gui_record.error=11;goto cleanup;}
 info=(GuiInfo){.magic=0x3149474b,.version=1,.size=sizeof info,.width=gfx_width(),.height=gfx_height(),.bpp=gfx_colors()>256?2:1,.features=1|(k386_capacity()==4?GUI_FOUR_SLOTS:0)};info.pitch=info.width*info.bpp;
 for(unsigned i=0;i<16;i++)info.colors[i]=gfx_rgb(i);
 gui_record.width=info.width;gui_record.height=info.height;gui_record.bpp=info.bpp;gui_record.graphics=1;gui_record.ready=1;
 if(kg_clients_start()){gui_record.error=12;goto cleanup;}
 previous=mouse_read();enqueue((GuiEvent){.type=GUI_MOTION,.x=previous.x,.y=previous.y,.buttons=previous.buttons,.ticks=previous.ticks});
 while(!gui_record.error&&!quit){
  gui_record.busy=1;pump();kg_clients_tick();
  /* Charge the scheduling interval after the bounded peer batch finishes.
   * Peer execution must not immediately trigger another batch and starve
   * the GUI while it is composing a window between IRQ preemptions. */
  u32 now=input_ticks();if(manager&&((now-last_schedule)&0xffffff)>=3072){k386_tick();last_schedule=input_ticks();workers();}
  if(get(12)==7)services_wait();
  else {host_enter();put(636,get(636)+1);gui_record.quanta++;if(!get(16)&&get(12)==2&&loader_request)context[11]=dispatch();}
  gui_record.busy=0;
  if(get(16)||get(12)==3||get(12)==4||get(12)==5)break;
 }
 if(quit){put(12,5);put(68,65);}
cleanup:
 gui_record.busy=1;stop_workers();kg_clients_close();if(expanded_here){if(k386_capacity_request(2))gui_record.error=14;expanded_here=0;}services_close();loader_close();
 if(initialized&&tables){mem_fill(get(24)>>4,0,12288,0);put(464,1);put(948,1);}
 host_drop_xms();
 if(started){gfx_stop();gfx_driver_release();p_finish();}
 mem_free(scratch);mem_free(tables);scratch=tables=0;
 gui_record.scratch=gui_record.tables=0;gui_record.graphics=0;
 if(get(16)&&!gui_record.error)gui_record.error=get(16);
done:
 gui_record.reserved[0]=1;
 for(unsigned i=0;i<16;i++)if(*(volatile u8 *)(0xd400+i)!=0xa5){gui_record.reserved[0]=0;gui_record.error=9;}
 /* Terminal status is a publication boundary for external observers. In
  * particular, the final DOS allocation query must finish before ready is
  * cleared; otherwise a successful exit can expose largest_after == 0. */
 gui_record.largest_after=largest();
 gui_record.state=get(12)==4?4:gui_record.error?6:3;
 gui_record.completed=1;
 gui_record.ready=0;gui_record.busy=0;
 p_output(gui_record.error?"KGUI32 REFUSED/FAILED\r\n":gui_record.state==4?"KGUI32 FAULT: resources reclaimed\r\n":"KGUI32 DONE: resources reclaimed\r\n");
 return gui_record.error||gui_record.state==4?1:0;
}
