/* Transient graphical service. Called only in real mode; the INT 2F callback
 * never invokes DOS, BIOS, scheduling or rendering. Its stack is separate. */
#include "gui.h"
#include "client-policy.h"
#include "../window/hardware.h"
#include "../window/k386_client.h"
extern u8 loader_buffer[4096];
extern unsigned owned[4];extern u32 owned_generation[4];
extern int gui_copy(u32,void *,unsigned,int);
extern void kg_gate(void);
extern u32 kg_old_vector;extern u16 kg_sender_segment,kg_sender_offset;
unsigned kg_language,kg_preferences_revision;
int kg_clients_command(unsigned,char *,unsigned);
int kg_image_clip_request(KgRequest *,unsigned,u32);
void kg_image_clip_drop(unsigned,u32);
/* Pump process work only after returning from K32HOST, never inside INT 2F. */
void kg_process_tick(void){
 unsigned a=0xd3a0,b=0,di;u16 es;
 __asm__ volatile("pushw %%es; int $0x2f; movw %%es,%1; popw %%es":"+a"(a),"=c"(es),"=D"(di):"b"(b):"edx","esi","cc","memory");
 if((a&65535)!=0x3a0d)return;
 unsigned segment=es,offset=di&65535;a=0xd3a0;b=2;
 __asm__ volatile("pushw %%es; int $0x2f; popw %%es":"+a"(a):"b"(b):"ecx","edx","esi","edi","cc","memory");
 unsigned mask=mem_get(segment,offset+40);
 for(unsigned id=0;id<k386_capacity();id++)if(mask&(1u<<id)){K386Usage u;if(k386_usage(id,&u)){owned[id]=1;owned_generation[id]=u.generation;kg_clients_command(id,"",1);}}
}

static KgClient client;static unsigned arena,self,host_segment,installed;
#define META KG_CLIENT_META
_Static_assert(sizeof(KgRequest)<=META&&sizeof(KgClient)<=4096-META,"private per-slot far metadata");
static void load(unsigned id){if(arena)mem_copy(self,(unsigned)&client,arena,id*4096+META,sizeof client);else client=(KgClient){0};}
static void save(unsigned id){if(arena)mem_copy(arena,id*4096+META,self,(unsigned)&client,sizeof client);}
/* Restart commands share the existing private far page; no extra DOS block. */
int kg_clients_command(unsigned id,char *path,unsigned write){
 if(!arena||id>=k386_capacity())return -38;
 unsigned at=id*4096+KG_CLIENT_COMMAND;
 if(write){unsigned n=0;while(n<128&&path[n])n++;if(n==128)return -22;mem_fill(arena,at,128,0);mem_copy(arena,at,self,(unsigned)path,n+1);}
 else{mem_copy(self,(unsigned)path,arena,at,128);path[127]=0;if(!path[0])return -22;}
 return 0;
}
static unsigned clipboard(void){return k386_capacity()*4096;}
static u32 serial,seconds,last_clock,clip_bytes,dos_date;
unsigned kg_clients_time(void){return seconds;}
volatile struct {
 char signature[8];u32 version,size,owner,arena,old_vector,vector,ready,requests,rejected;
 u32 generation[2],token[2],revision[2],events[2],dropped[2];
} kg_producer={.signature={'K','G','C','L','N','T','0','1'},.version=1,.size=sizeof kg_producer};
/* Keep the original 84-byte discovery record intact; metrics are additive. */
volatile struct {char signature[8];u32 version,size,owner,request_bytes,reply_bytes,status_requests,status_bytes,uploads,upload_bytes;} kg_transfer={.signature={'K','G','X','F','E','R','0','1'},.version=1,.size=sizeof kg_transfer};
static void publish(unsigned i){
 extern volatile GuiDriverRecord gui_record;
 if(i<2){kg_producer.generation[i]=client.generation;kg_producer.token[i]=client.token;kg_producer.revision[i]=client.revision;kg_producer.events[i]=client.received;kg_producer.dropped[i]=client.dropped;}
 gui_record.reserved[8+i]=client.surface.handle;gui_record.reserved[12+i]=client.surface.uploaded;
 gui_record.reserved[16+i]=client.surface.pending;gui_record.reserved[20+i]=client.generation;
}

extern int gui_open_document(unsigned,u32,unsigned,const char *);
void kg_clients_tick(void){
 /* Metadata lives in far per-slot pages. Poll deferred work on the same
  * bounded 100 ms clock as DOS time, not after every preempted GUI slice. */
 u32 now=input_ticks();if(last_clock&&((now-last_clock)&0xffffff)<30720)return;last_clock=now;
 kg_process_tick();
 /* This tick is outside INT 2F: DOS/EXEC are allowed only at this boundary. */
 for(unsigned id=0;id<k386_capacity();id++){load(id);if(!client.edit_pending)continue;
  client.edit_pending=0;u32 generation=client.generation,token=client.token;save(id);
  int result=!token||!owned[id]||owned_generation[id]!=generation?-116:gui_open_document(id,generation,client.edit_namespace,client.edit_name);
  /* DOS/EXEC can reenter the client bridge while replacing another slot. */
  load(id);if(client.generation==generation&&client.token==token){client.edit_result=result;save(id);}
 }
 unsigned a=0x2c00,c=0,d=0;__asm__ volatile("int $0x21":"+a"(a),"=c"(c),"=d"(d)::"ebx","esi","edi","cc","memory");
 unsigned next=((c>>8)&255)*3600+(c&255)*60+((d>>8)&255);
 if(!dos_date||next!=seconds){
  a=0x2a00;__asm__ volatile("int $0x21":"+a"(a),"=c"(c),"=d"(d)::"ebx","esi","edi","cc","memory");
  dos_date=(c&65535u)<<16|(d&65535u);
 }
 seconds=next;
}
int kg_clients_start(void){
 kg_language=kg_preferences_revision=0;
 unsigned a=0xd394,b=0;__asm__ volatile("int $0x2f":"+a"(a),"+b"(b)::"ecx","edx","esi","edi","cc","memory");if((a&65535)==0x394d)return -16;
 a=0xd390;b=0;unsigned di;u16 es;
 __asm__ volatile("pushw %%es; int $0x2f; movw %%es,%1; popw %%es":"+a"(a),"=c"(es),"=D"(di):"b"(b):"edx","esi","cc","memory");
 if((a&65535)!=0x390d)return 0;
 host_segment=es;__asm__ volatile("movw %%ds,%w0":"=r"(self));
 unsigned bytes=(k386_capacity()+1)*4096;arena=mem_alloc(bytes);if(!arena)return -12;mem_fill(arena,0,bytes,0);
 a=0x352f;__asm__ volatile("pushw %%es; int $0x21; movw %%es,%1; popw %%es":"+a"(a),"=c"(es),"=b"(b)::"edx","esi","edi","cc","memory");kg_old_vector=(u32)es<<16|(b&65535);
 a=0x252f;__asm__ volatile("int $0x21":"+a"(a):"d"((u32)kg_gate):"ebx","ecx","esi","edi","cc","memory");installed=1;
 kg_transfer.owner=self;kg_producer.owner=self;kg_producer.arena=arena;kg_producer.old_vector=kg_old_vector;kg_producer.vector=(u32)self<<16|(unsigned)kg_gate;kg_producer.ready=1;
 kg_clients_tick();return 0;
}
void kg_clients_close(void){
 kg_process_tick();
 if(installed){unsigned a=0x252f,d=kg_old_vector&65535,seg=kg_old_vector>>16;
  __asm__ volatile("pushw %%ds; movw %w1,%%ds; int $0x21; popw %%ds":"+a"(a),"+c"(seg):"d"(d):"ebx","esi","edi","cc","memory");installed=0;
 }
 mem_free(arena);arena=0;kg_producer.ready=kg_producer.arena=0;
 client=(KgClient){0};
 for(unsigned i=0;i<k386_capacity();i++)publish(i);
}
void kg_dispatch(void){
 KgPacket p;unsigned id=4;kg_producer.requests++;
 if(kg_sender_segment!=host_segment||kg_sender_offset>65536-sizeof p)return;
 mem_copy(self,(unsigned)&p,kg_sender_segment,kg_sender_offset,sizeof p);
 p.result=-22;
 if(p.version!=1||p.size!=sizeof p||p.slot>=k386_capacity())goto done;
 id=p.slot;load(id);
 if(!p.operation){kg_image_clip_drop(id+1,p.generation);if(client.generation==p.generation){client.surface=client.extra_surface=(KgSurface){0};client.edit_pending=0;client.token=0;client.input_flags=0;client.revision++;client.head=client.count=0;}p.result=0;goto done;}
 if(p.operation!=1||p.bytes!=sizeof(KgRequest)||p.request>65536-sizeof(KgRequest))goto done;
 if(!owned[id]||owned_generation[id]!=p.generation){p.result=-1;goto done;}
 KgRequest *r=(KgRequest *)loader_buffer;
 mem_copy(self,(unsigned)r,host_segment,p.request,KG_REQUEST_HEADER);
 unsigned input=kg_request_bytes(r->operation);if(input>KG_REQUEST_HEADER)mem_copy(self,(unsigned)r+KG_REQUEST_HEADER,host_segment,p.request+KG_REQUEST_HEADER,input-KG_REQUEST_HEADER);
 kg_transfer.request_bytes+=input;
 extern volatile GuiDriverRecord gui_record;
 p.result=kg_policy(&client,r,p.generation,&serial,gui_record.width,gui_record.height);
 if(p.result>=0){
  r->slot=id;r->generation=p.generation;r->revision=client.revision;r->event.seconds=seconds;
  if(r->operation==KG_POLL&&(client.input_flags&KG_INPUT_LANGUAGE)){
   if(kg_language&1)r->event.control|=KG_LANGUAGE_JA;
   if(kg_language&2)r->event.control|=KG_LARGE_TEXT;
  }
  if(r->operation==KG_DATE)p.result=(int)dos_date;
  if(r->operation==KG_CLOSE)kg_image_clip_drop(id+1,p.generation);
  if(r->operation>=KG_IMAGE_COPY_BEGIN)p.result=kg_image_clip_request(r,id+1,p.generation);
  if(r->operation==KG_PREFERENCES_CHANGED)kg_preferences_revision=(kg_preferences_revision+1)&0x7fffffffu;
  if(r->operation==KG_CLIP_SET){clip_bytes=r->count;mem_copy(arena,clipboard(),self,(unsigned)r->draw,clip_bytes);}
  if(r->operation==KG_CLIP_GET){unsigned n=r->count<clip_bytes?r->count:clip_bytes;mem_copy(self,(unsigned)r->draw,arena,clipboard(),n);p.result=n;}
  if(r->operation==KG_FRAME){r->width=client.width;r->height=client.height;for(unsigned i=0;i<32;i++)r->title[i]=client.title[i];}
  if(kg_surface_operation(r->operation)){
   KgSurface *s=kg_client_surface(&client,r);
   kg_surface_set(r,0,s->handle);kg_surface_set(r,4,r->operation==KG_SURFACE_UPLOAD?r->height:0);kg_surface_set(r,8,s->width);kg_surface_set(r,12,s->height);kg_surface_set(r,16,s==&client.extra_surface);
  }
  if(r->operation==KG_FRAME||kg_surface_operation(r->operation))mem_copy(arena,id*4096,self,(unsigned)r,sizeof *r);
  if(r->operation==KG_OPEN)mem_fill(arena,id*4096,sizeof *r,0);
  if(p.result>=0){unsigned output=kg_reply_bytes(r->operation);mem_copy(host_segment,p.request,self,(unsigned)r,output);kg_transfer.reply_bytes+=output;}
 }
 if(r->operation==KG_SURFACE_STATUS){kg_transfer.status_requests++;kg_transfer.status_bytes+=input+(p.result>=0?kg_reply_bytes(r->operation):0);}
 if(r->operation==KG_SURFACE_UPLOAD){kg_transfer.uploads++;kg_transfer.upload_bytes+=input+(p.result>=0?kg_reply_bytes(r->operation):0);}
done:
 if(id<k386_capacity()){save(id);publish(id);}
 if(p.result<0)kg_producer.rejected++;
 mem_copy(kg_sender_segment,kg_sender_offset,self,(unsigned)&p,sizeof p);
}
int kg_clients_snapshot(u32 pointer,unsigned bytes){
 if(bytes!=sizeof(KgRequest))return -22;
 u32 h[4];int e=gui_copy(pointer,h,sizeof h,0);if(e)return e;
 if(h[0]!=1||h[1]!=bytes||h[3]>=k386_capacity())return -22;
 unsigned id=h[3];load(id);KgClient *c=&client;KgRequest *r=(KgRequest *)loader_buffer;
 if(arena&&c->token&&owned[id]&&owned_generation[id]==c->generation)mem_copy(self,(unsigned)r,arena,id*4096,sizeof *r);
 else {*r=(KgRequest){0};}
 r->version=1;r->size=sizeof *r;r->slot=id;r->generation=c->generation;r->token=owned[id]&&owned_generation[id]==c->generation?c->token:0;
 r->revision=c->revision;r->width=c->width;r->height=c->height;r->event.control=c->input_flags;r->event.key=c->cancelled;
 if(kg_surface_operation(r->operation)){
  KgSurface *s=kg_surface_word(r,16)?&c->extra_surface:&c->surface;
  if(s->pending)r->revision=s->pending;
  else {r->operation=0;r->count=0;}
 }else for(unsigned i=0;i<32;i++)r->title[i]=c->title[i];
 return gui_copy(pointer,r,sizeof *r,1);
}
int kg_clients_event(u32 pointer,unsigned bytes){
 if(bytes!=sizeof(KgRequest))return -22;
 KgRequest *r=(KgRequest *)loader_buffer;int e=gui_copy(pointer,r,sizeof *r,0);if(e)return e;
 if(r->version!=1||r->size!=sizeof *r||r->slot>=k386_capacity())return -22;
 unsigned id=r->slot;load(id);KgClient *c=&client;
 if(!owned[id]||owned_generation[id]!=r->generation)return -1;
 if(c->generation!=r->generation||!c->token||c->token!=r->token)return -116;
 KgEvent v=r->event;
 if(v.type<KG_KEY||v.type>KG_QUIT)return -22;
 if(v.type==KG_KEY&&(v.control&~7u))return -22;
 if(v.type==KG_RESIZE&&(v.width<1||v.width>784||v.height<1||v.height>416))return -22;
 if((v.type==KG_POINTER||v.type==KG_MOUSE)&&(v.x < -800||v.x>800||v.y < -600||v.y>600||v.key>7))return -22;
 v.seconds=seconds;kg_enqueue(c,&v);save(id);publish(id);return 0;
}

int kg_clients_close_guard(void){for(unsigned i=0;i<k386_capacity();i++){load(i);if(client.token&&(client.input_flags&KG_INPUT_CLOSE))return 1;}return 0;}

int kg_clients_surface_ack(u32 pointer,unsigned bytes){
 GuiSurfaceAck r;if(bytes!=sizeof r)return -22;
 int e=gui_copy(pointer,&r,sizeof r,0);if(e)return e;
 if(r.version!=1||r.size!=sizeof r||r.slot>=k386_capacity())return -22;
 unsigned id=r.slot;load(id);KgClient *c=&client;
 if(!owned[id]||owned_generation[id]!=r.generation)return -1;
 if(!c->token||c->token!=r.token||c->generation!=r.generation)return -116;
 /* The trusted desktop acknowledges only an accepted complete transport. */
 KgRequest *stored=(KgRequest *)loader_buffer;mem_copy(self,(unsigned)stored,arena,id*4096,sizeof *stored);
 KgSurface *s=kg_surface_word(stored,16)?&c->extra_surface:&c->surface;
 if(!s->pending||s->pending!=r.revision)return -116;
 e=kg_surface_ack(s,r.revision,stored->operation);if(e)return e;
 save(id);publish(id);return 0;
}
