/* Original flat-pointer GUI: composition, cursor, hit testing and window state
 * execute only in CPL3 protected mode. No physical VRAM or I/O mapping. */
#include "scene.h"
#include "shell-ui.h"
#include "layout.h"
#include "sdk/preferences.h"
#include "sdk/i18n.h"
unsigned ku_language;
#include "sdk/protocol.h"
#include "sdk/text.h"
#include "surface.h"
#include "../k386/text.h"
#include "terminal-video.h"
#include "../k38632/sdk/k32x.h"
#include "../desktop-style.h"
#include "gallery.h"
#include "ui-desktop.h"
typedef struct {u32 magic,version,frames,events,mx,my,x0,y0,x1,y1,active,drag,presents,last_key,heartbeat,error;
 u32 terminal_magic,terminal0,terminal1,terminal_phase,terminal_valid;
 u32 terminal_geometry[2][6];
 u32 capacity,terminal2,terminal3,extra_geometry[2][6];
 u32 bitmap_magic,bitmap_meta,bitmap_pixels,bitmap_stride,bitmap_frames;
 u32 video_magic,video_cache,video_stride,video_generation,video_revision,video_valid;
 u32 surfaces_version,extra_meta,extra_pixels,extra_stride,surface_budget;
} GuiState;
volatile GuiState gui_state __attribute__((used,section(".data.gui_state")))={.magic=0x3147554b,.version=1,.terminal_magic=0x3254474b};
static GuiInfo info;static Scene scene;static u8 font[95*16];
static WindowLayout layout;
static K32Revision layout_revision;
static unsigned layout_available,layout_dirty,layout_changed;
static unsigned preference_revision;
static K32Revision pins_revision,history_revision;
static unsigned history_available,history_serial;
static unsigned pins_available;
static unsigned desktop_minute,desktop_slots;
static u32 desktop_clock_poll,drive_last[2];
static unsigned drive_counts[2],drive_lamps;
static KgPreferences preferences;
static void layout_save(void){
 if(!layout_dirty)return;
 int e=k32_io_save(KG_LAYOUT_FILE,&layout,sizeof layout,&layout_revision);
 if(e>=0){
  unsigned hash=2166136261u;const unsigned char *p=(const unsigned char *)&layout;
  for(unsigned i=0;i<sizeof layout;i++)hash=(hash^p[i])*16777619u;
  layout_revision=(K32Revision){1,sizeof layout,hash};
 }else scene_notice_set(&scene,GN_LAYOUT_SAVE_FAILED);
 layout_dirty=0;
}
static int same_rect(Rect a,Rect b){
 return a.x==b.x&&a.y==b.y&&a.w==b.w&&a.h==b.h;
}
static struct {u32 code;u8 bitmap[32];} glyph_cache[128];
static unsigned glyph_slot;
/* 64-row transfers reclaim 857600 bytes without growing the 4 MiB desktop. */
static u8 framebuffer[800*600*2],packed[800*64*2];
static u8 bitmap_cube5[32],bitmap_cube6[64];
static Rect clip;
static GuiSessionView views[4];static int errors[4],editing[4],lengths[4];
static GuiTerminalView terminals[4],terminal_stage;
static KvCanvas video_cache[4],video_stage;
static u32 video_generation[4],video_revision[4],video_valid[4];
static u8 video_pixels[640*400];
static u32 video_composed_slot,video_composed_revision,video_composed_generation,video_composed_phase;
static int video_read(unsigned id,const GuiSessionView *v,unsigned region,void *data,unsigned n){
 for(unsigned at=0;at<n;){unsigned count=n-at;if(count>KV_READ_BYTES)count=KV_READ_BYTES;
  GuiVideoRead r={.version=1,.size=sizeof r,.slot=id,.generation=v->generation,.region=region,.offset=at,.data=(u32)data+at,.bytes=count,.revision=v->text_revision};
  int e=k32_call(GUI_VIDEO_READ,(u32)&r,sizeof r);if(e)return e;at+=count;
 }return 0;
}
static void refresh_video(unsigned id,const GuiSessionView *v){
 if(!(info.features&GUI_VIDEO_MODES)||!v->owned||v->kind||!v->generation){video_valid[id]=0;return;}
 if(video_valid[id]&&video_generation[id]==v->generation&&video_revision[id]==v->text_revision)return;
 video_stage=video_cache[id];
 int e=video_read(id,v,0,&video_stage.meta,sizeof(KvMeta));
 if(!e)video_stage.meta.text_cursor=(video_stage.meta.text_start+v->row*video_stage.meta.text_pitch+v->column)&4095;
 if(!e&&(video_stage.meta.version!=1||video_stage.meta.size!=sizeof(KvMeta)))e=-22;
 if(!e&&kt_video_extra(&video_stage.meta)){
  if(!video_valid[id]||video_generation[id]!=v->generation||video_stage.meta.font_writes!=video_cache[id].meta.font_writes)e=video_read(id,v,1,video_stage.font,sizeof video_stage.font);
  if(!e&&(video_stage.meta.flags&KV_GRAPH))for(unsigned p=0;p<4&&!e;p++)e=video_read(id,v,2+p,video_stage.planes[p],32768);
  if(!e)e=video_read(id,v,6,video_stage.codes,sizeof video_stage.codes);
  if(!e)e=video_read(id,v,7,video_stage.attributes,sizeof video_stage.attributes);
 }
 if(e)return;
 int old_extra=kt_video_extra(&video_cache[id].meta);
 video_cache[id]=video_stage;video_valid[id]=1;video_generation[id]=v->generation;video_revision[id]=v->text_revision;
 if(kt_video_extra(&video_stage.meta)||old_extra)scene_window_damage(&scene,id+2,scene.window[id+2].r);
}
static unsigned terminal_valid[4],terminal_phase;static u32 terminal_tick;
static char paths[4][128];static unsigned following[4];
static KgRequest clients[4],client_io;
static KgImage images[4];static u16 image_pixels[4][KG_SURFACE_PIXELS*2];
static KgImage extra_images[4];static u16 extra_pixels[4][KG_SURFACE_EXTRA_PIXELS*2];
static unsigned exit_pending,closing_slot;
static KgDesktopShared shared;
static unsigned scale_xmap[800];
static u32 close_generation[4],close_token[4],close_cancel[4];
static unsigned capture,capture_generation,capture_token;
static int equal(const char *,const char *);
static void restore_app_layout(unsigned id,const char *program){
 if(!layout_available)return;
 for(unsigned peer=0;peer<(unsigned)scene.slots;peer++)
  if(peer!=id&&views[peer].owned&&views[peer].state&&equal(views[peer].program,program))return;
 layout_restore(&layout,&scene,id+2,program);
}
static void refresh_graphical(unsigned);
static int client_event(unsigned,const KgEvent *);
static void follow_cursor(unsigned);
static void terminal_report(void){
 gui_state.video_magic=0x3156474b;gui_state.video_cache=(u32)video_cache;gui_state.video_stride=sizeof(KvCanvas);gui_state.video_generation=(u32)video_generation;gui_state.video_revision=(u32)video_revision;gui_state.video_valid=(u32)video_valid;
 gui_state.terminal0=(u32)&terminals[0];gui_state.terminal1=(u32)&terminals[1];gui_state.terminal_valid=terminal_valid[0]|terminal_valid[1]<<1|terminal_valid[2]<<2|terminal_valid[3]<<3;gui_state.capacity=scene.slots;gui_state.terminal2=(u32)&terminals[2];gui_state.terminal3=(u32)&terminals[3];gui_state.terminal_phase=terminal_phase;
 for(unsigned id=0;id<(unsigned)scene.slots;id++){Rect r=scene.window[id+2].r;u32 *g=(u32 *)(id<2?gui_state.terminal_geometry[id]:gui_state.extra_geometry[id-2]);g[0]=r.x;g[1]=r.y;g[2]=r.w;g[3]=r.h;g[4]=scene.scroll_x[id];g[5]=scene.scroll_y[id];}
}
static int session_command(unsigned slot,unsigned operation,unsigned key){
 GuiSessionCommand c={.version=1,.size=sizeof c,.slot=slot,.generation=operation==GS_CREATE?0:views[slot].generation,.operation=operation,.key=key};
 for(unsigned i=0;i<128;i++){c.path[i]=paths[slot][i];if(!c.path[i])break;}
 return k32_call(GUI_SESSION,(u32)&c,sizeof c);
}
static void refresh(void){
 for(unsigned id=0;id<(unsigned)scene.slots;id++){
  GuiSessionView v={.version=1,.size=sizeof v,.slot=id};
  if(k32_call(GUI_SNAPSHOT,(u32)&v,sizeof v))continue;
  if(v.owned&&v.state==4&&(views[id].state!=4||views[id].generation!=v.generation))scene_notice_set(&scene,GN_APP_FAULTED);
  GuiWindow *w=&scene.window[id+2];
  if(v.owned&&!v.kind&&v.generation!=views[id].generation)restore_app_layout(id,v.program);
  if(v.kind||!v.owned){terminal_valid[id]=0;}
  else if(!terminal_valid[id]||terminals[id].generation!=v.generation||terminals[id].revision!=v.text_revision||terminals[id].state!=v.state){
   terminal_stage=(GuiTerminalView){.version=2,.size=sizeof terminal_stage,.slot=id,.generation=v.generation};
   if(!k32_call(GUI_SNAPSHOT,(u32)&terminal_stage,sizeof terminal_stage)){
    if(w->open&&!w->hidden)for(unsigned r=0;r<25;r++){int changed=!terminal_valid[id]||terminals[id].generation!=v.generation;
     for(unsigned c=0;c<80&&!changed;c++){unsigned at=r*80+c;if(terminal_stage.codes[at]!=terminals[id].codes[at]||terminal_stage.attributes[at]!=terminals[id].attributes[at])changed=1;}
     if(changed)scene_window_damage(&scene,id+2,(Rect){w->r.x+8,w->r.y+70+((int)r-scene.scroll_y[id])*16,(w->r.w-16)/8*8,16});
    }
    terminals[id]=terminal_stage;terminal_valid[id]=1;
   }
  }
  if(w->open&&!w->hidden){
   int changed=v.generation!=views[id].generation||v.state!=views[id].state||v.owned!=views[id].owned;
   for(int i=0;i<16;i++)if(v.program[i]!=views[id].program[i])changed=1;
   if(changed)scene_window_damage(&scene,id+2,w->r);
   int cols=(w->r.w-16)/8,rows=(w->r.h-104)/16;
   for(int row=0;row<rows;row++){
    int r=row+scene.scroll_y[id];if(r>=25)break;
    int dirty=(v.row==(u32)r||views[id].row==(u32)r)&&(v.row!=views[id].row||v.column!=views[id].column);
    for(int col=0;col<cols&&col+scene.scroll_x[id]<80;col++){
     int at=r*80+col+scene.scroll_x[id];if(v.screen[at]!=views[id].screen[at])dirty=1;
    }
    if(dirty)scene_window_damage(&scene,id+2,(Rect){w->r.x+8,w->r.y+70+row*16,cols*8,16});
   }
  }
  refresh_video(id,&v);views[id]=v;refresh_graphical(id);if(following[id]){follow_cursor(id);following[id]--;}
 }
 terminal_report();
}
static int client_event(unsigned id,const KgEvent *e){
 client_io=(KgRequest){.version=1,.size=sizeof client_io,.slot=id,.generation=views[id].generation,.token=clients[id].token,.event=*e};
 return k32_call(GUI_CLIENT_EVENT,(u32)&client_io,sizeof client_io);
}
static void refresh_graphical(unsigned id){
 client_io=(KgRequest){.version=1,.size=sizeof client_io,.slot=id};
 if(k32_call(GUI_CLIENT_VIEW,(u32)&client_io,sizeof client_io))return;
 if(kg_surface_operation(client_io.operation)&&client_io.token&&views[id].owned&&client_io.generation==views[id].generation){
  unsigned secondary=kg_surface_word(&client_io,16);if(secondary>1){errors[id]=-22;return;}
  KgImage *image=secondary?&extra_images[id]:&images[id];
  int e=kg_surface_receive_n(image,secondary?extra_pixels[id]:image_pixels[id],secondary?KG_SURFACE_EXTRA_PIXELS:KG_SURFACE_PIXELS,&client_io,views[id].generation,client_io.token);
  if(!e){GuiSurfaceAck ack={1,sizeof ack,id,client_io.generation,client_io.token,client_io.revision};e=k32_call(GUI_SURFACE_ACK,(u32)&ack,sizeof ack);}
  if(e)errors[id]=e;
  else if(client_io.operation==KG_SURFACE_DESTROY||(image->valid&&image->uploaded==image->width*image->height*2))scene_window_damage(&scene,id+2,scene.window[id+2].r);
  return;
 }
 if(!client_io.token||client_io.generation!=views[id].generation||!views[id].owned)images[id]=extra_images[id]=(KgImage){0};
 if(close_token[id]){
  if(close_generation[id]!=views[id].generation||close_generation[id]!=client_io.generation||close_cancel[id]!=client_io.event.key||(client_io.token&&client_io.token!=close_token[id]))close_token[id]=0;
  else if(!client_io.token){scene_close(&scene,id+2);close_token[id]=0;capture=0;}
 }
 unsigned yes=views[id].owned&&views[id].kind==1&&client_io.generation==views[id].generation&&client_io.revision;
 scene.graphical[id]=yes;scene.menu[id]=yes&&client_io.token&&(client_io.event.control&KG_INPUT_MENU);
 if(yes&&client_io.token&&(clients[id].token!=client_io.token||clients[id].generation!=client_io.generation)){
  GuiWindow *w=&scene.window[id+2];scene_damage(&scene,(Rect){w->r.x,w->r.y,w->r.w+6,w->r.h+6});
  scene.restored[id]=(Rect){0};scene.placement[id]=0;
  w->r.w=client_io.width+16;w->r.h=client_io.height+104;
  w->r.x=id?scene.width-w->r.w-16:16;w->r.y=id?scene.height-w->r.h-40:40;
  if(w->r.y<40)w->r.y=40;
  restore_app_layout(id,views[id].program);
  scene_focus(&scene,id+2);scene_damage(&scene,w->r);
 }
 if(exit_pending&&clients[id].event.key!=client_io.event.key)exit_pending=0;
 if(!client_io.operation&&clients[id].token==client_io.token&&clients[id].generation==client_io.generation&&client_io.token){clients[id].event=client_io.event;return;}
 if(clients[id].revision!=client_io.revision||clients[id].generation!=client_io.generation||clients[id].token!=client_io.token){
  if(yes&&scene.window[id+2].open&&!scene.window[id+2].hidden)scene_window_damage(&scene,id+2,scene.window[id+2].r);
  clients[id]=client_io;
 }
}
static void launch_app(int act,const char *document){
 /* An app can finish between GUI timer events. Decide from the monitor's
  * current state rather than rejecting an available slot from the cache. */
 refresh();
 unsigned id=0;
 while(id<(unsigned)scene.slots&&(views[id].state==1||views[id].state==2||views[id].state==3||(views[id].state==4&&!views[id].owned)))id++;
 if(id==(unsigned)scene.slots){
  scene.action_result=-16;scene_damage(&scene,scene.window[0].r);scene_notice_set(&scene,GN_SLOTS_FULL);return;
 }
 const ShellApp *app=shell_app(act);if(!app)return;
 const char *path=document?document:app->command;
 for(unsigned i=0;i<128;i++){paths[id][i]=path[i];if(!path[i])break;}
 errors[id]=session_command(id,GS_CREATE,0);scene.action_result=errors[id];
 if(!errors[id]){shell_record_launch(&scene,act);scene_focus(&scene,id+2);}
 else {
  scene_damage(&scene,scene.window[0].r);
  scene_notice_set(&scene,errors[id]==-2?GN_APP_MISSING:GN_START_FAILED);
 }
 refresh();
}
static void launch_recent(void){
 const ShellDocument *d=shell_document(&scene,scene.document_index);
 if(!d||d->volume>26||d->bitmap>1)return;
 char command[64];const char *program=d->bitmap?"KGIMAGE.EXE":"KGEDIT.EXE";unsigned n=0;
 while(*program)command[n++]=*program++;
 command[n++]=' ';command[n++]='@';command[n++]=d->volume?'A'+d->volume-1:'-';command[n++]=' ';
 for(unsigned i=0;i<KG_RECENT_PATH;i++){
  char ch=d->path[i];command[n++]=ch;
  if(!ch){launch_app(d->bitmap?ACT_IMAGE:ACT_EDITOR,command);return;}
 }
}
enum { PINS_MAGIC=0x3150474b };
static void load_pins(void){
 u32 data[2]={0};
 pins_available=layout_available&&!kg_state_read(KG_PINS_FILE,data,sizeof data,&pins_revision);
 if(pins_available&&data[0]==PINS_MAGIC)scene.app_pins=data[1]&0xfff;
}
static void save_pins(void){
 if(!pins_available)return;
 u32 data[]={PINS_MAGIC,scene.app_pins},hash=2166136261u;
 int e=k32_io_save(KG_PINS_FILE,data,sizeof data,&pins_revision);
 if(e>=0){
  for(unsigned i=0;i<sizeof data;i++)hash=(hash^((const u8 *)data)[i])*16777619u;
  pins_revision=(K32Revision){1,sizeof data,hash};
 }else scene_notice_set(&scene,GN_LAYOUT_SAVE_FAILED);
}
static void load_history(void){
 ShellHistory saved={0};
 history_available=!kg_state_read(KG_HISTORY_FILE,&saved,sizeof saved,&history_revision);
 if(!history_available||!history_revision.exists)return;
 if(shell_history_valid(&saved))scene.history=saved;
 else history_available=0;
}
static void save_history(void){
 if(!scene.history_dirty||!history_available)return;
 ShellHistory saved={.version=1,.recent_apps=scene.history.recent_apps};
 for(unsigned i=0;i<saved.recent_apps;i++)saved.app_recent[i]=scene.history.app_recent[i];
 for(unsigned i=0;i<scene.history.document_count;i++){
  ShellDocument *doc=&scene.history.documents[i];
  if(!doc->volume)saved.documents[saved.document_count++]=*doc;
 }
 int e=k32_io_save(KG_HISTORY_FILE,&saved,sizeof saved,&history_revision);
 if(e>=0){
  u32 hash=2166136261u;const u8 *p=(const u8 *)&saved;
  for(unsigned i=0;i<sizeof saved;i++)hash=(hash^p[i])*16777619u;
  history_revision=(K32Revision){1,sizeof saved,hash};
  scene.history_dirty=0;
 }else scene_notice_set(&scene,GN_HISTORY_SAVE_FAILED);
}
static void collect_history(void){
 const volatile KgRecentDocument *docs=shared.documents;
 for(;;){
  unsigned serial=~0u,index=KG_RECENT_COUNT;
  for(unsigned i=0;i<KG_RECENT_COUNT;i++)if(docs[i].serial>history_serial&&docs[i].serial<serial){
   serial=docs[i].serial;index=i;
  }
  if(index==KG_RECENT_COUNT)return;
  ShellDocument doc={.volume=docs[index].volume,.bitmap=docs[index].bitmap};
  char destination[KG_RECENT_PATH];
  for(unsigned i=0;i<KG_RECENT_PATH;i++){doc.path[i]=docs[index].path[i];destination[i]=docs[index].destination[i];}
  if(docs[index].serial!=serial)return;
  history_serial=serial;
  if(destination[0]){if(shell_follow_move(&scene,doc.path,destination))save_history();}
  else shell_accept_document(&scene,&doc);
 }
}
static void follow_cursor(unsigned id){
 GuiWindow *w=&scene.window[id+2];int cols=(w->r.w-16)/8,rows=(w->r.h-104)/16;
 int x=scene.scroll_x[id],y=scene.scroll_y[id];
 if((int)views[id].column<x)x=views[id].column;
 if((int)views[id].column>=x+cols)x=(int)views[id].column-cols+1;
 if((int)views[id].row<y)y=views[id].row;
 if((int)views[id].row>=y+rows)y=(int)views[id].row-rows+1;
 if(x!=scene.scroll_x[id]||y!=scene.scroll_y[id]){scene.scroll_x[id]=x;scene.scroll_y[id]=y;scene_damage(&scene,w->r);}
}
static void begin_run(unsigned id){
 editing[id]=1;lengths[id]=0;paths[id][0]=0;
 GuiWindow *w=&scene.window[id+2];scene_damage(&scene,(Rect){w->r.x+8,w->r.y+70,w->r.w-16,72});
}
static unsigned key_modifiers;
static void action(int act){
 unsigned id=scene.slot;
 if(act==ACT_NOTICE_INPUT)return;
 if(act==ACT_PIN_CHANGED){save_pins();return;}
 if(act==ACT_HISTORY_CHANGED){save_history();return;}
 if(act==ACT_OPEN_RECENT){launch_recent();return;}
 if(act>=ACT_CALCULATOR){launch_app(act,0);return;}
 if(act==ACT_START||act==ACT_STOP){
  scene.action_result=k32_call(GUI_PROOF,act==ACT_START?1:2,0);scene_damage(&scene,scene.window[1].r);
  scene_notice_set(&scene,scene.action_result<0?GN_WORKER_FAILED:act==ACT_START?GN_WORKERS_STARTED:GN_WORKERS_STOPPED);
  refresh();return;
 }
 if(act<ACT_NEW)return;
 if(act==ACT_SCROLL){following[id]=0;return;}
 if(act==ACT_RUN){begin_run(id);return;}
 int was_editing=editing[id],old_error=errors[id];
 if(act==ACT_KEY&&editing[id]){
  unsigned k=scene.key;int n=lengths[id];
  if(k==27)editing[id]=0;
  else if(k==8&&n)paths[id][--n]=0;
  else if(k>=32&&k<=126&&n<127){paths[id][n++]=k;paths[id][n]=0;}
  else if(k==13&&n){
   errors[id]=session_command(id,GS_CREATE,0);
   if(!errors[id])editing[id]=0;
   else scene_notice_set(&scene,errors[id]==-2?GN_APP_MISSING:GN_SESSION_FAILED);
  }
  lengths[id]=n;
  scene_damage(&scene,scene.window[id+2].r);
 }else if(act==ACT_NEW){
  if(!views[id].owned&&!views[id].state){
   const char *p="KSH.EXE";for(int i=0;i<8;i++)paths[id][i]=p[i];
   errors[id]=session_command(id,GS_CREATE,0);
   if(errors[id])scene_notice_set(&scene,errors[id]==-2?GN_APP_MISSING:GN_SESSION_FAILED);
  }
 }else {
  unsigned op=act==ACT_KEY?GS_KEY:act==ACT_END?GS_END:act==ACT_PAUSE?GS_PAUSE:act==ACT_RESUME?GS_RESUME:GS_RESTART;
  if(act==ACT_KEY&&scene.graphical[id]){
   if(clients[id].token){KgEvent event={.type=KG_KEY,.key=scene.key,.control=scene.key==KG_MENU_KEY?0:key_modifiers};errors[id]=client_event(id,&event);}
  }else errors[id]=session_command(id,op,scene.key);
  if(act==ACT_KEY&&!scene.graphical[id])following[id]=3;
  if(act==ACT_RESTART&&!errors[id])scene.scroll_x[id]=scene.scroll_y[id]=0;
 }
 refresh();if(act==ACT_KEY&&!editing[id]&&!scene.graphical[id])follow_cursor(id);
 GuiWindow *w=&scene.window[id+2];
 if(act==ACT_KEY&&!was_editing&&!editing[id]){
  if(old_error!=errors[id])scene_damage(&scene,(Rect){w->r.x,w->r.y+w->r.h-32,w->r.w,32});
 }else if(act==ACT_KEY&&editing[id])scene_damage(&scene,(Rect){w->r.x+8,w->r.y+70,w->r.w-16,72});
 else scene_damage(&scene,w->r);
}
static void input_focus(void){
 unsigned id=scene.active>=2?(unsigned)scene.active-2:0;
 unsigned on=0;
 if(scene.shell_page)on=2;
 else if(scene.active>=2&&scene.window[scene.active].open&&!scene.window[scene.active].hidden)on=editing[id]?2:views[id].owned?1:0;
 session_command(id,GS_FOCUS,on);
}

static u32 color(unsigned n){if(preferences.theme==KG_THEME_CONTRAST){if(n==2)n=7;else if(n==4||n==6)n=14;}u32 c=info.colors[n];return info.bpp==1?n:((c>>19)<<11)|(((c>>10)&63)<<5)|((c>>3)&31);}
static u32 rgb(unsigned r,unsigned g,unsigned b){return info.bpp==1?16+(r+25)/51*36+(g+25)/51*6+(b+25)/51:(r>>3)<<11|(g>>2)<<5|(b>>3);}
static void box(int x,int y,int w,int h,u32 c){
 int right=x+w,bottom=y+h;
 if(x<clip.x)x=clip.x;
 if(y<clip.y)y=clip.y;
 if(right>clip.x+clip.w)right=clip.x+clip.w;
 if(bottom>clip.y+clip.h)bottom=clip.y+clip.h;
 if(right<=x||bottom<=y)return;
 for(int row=y;row<bottom;row++){
  unsigned n=right-x;
  if(info.bpp==1){u8 *p=framebuffer+row*info.pitch+x;
   if(n==1)*p=(u8)c;else __asm__ volatile("cld; rep stosb":"+D"(p),"+c"(n):"a"(c):"memory","cc");
  }else {u16 *p=(u16 *)(framebuffer+row*info.pitch)+x;
   if(n==1)*p=(u16)c;else __asm__ volatile("cld; rep stosw":"+D"(p),"+c"(n):"a"(c):"memory","cc");
  }
 }
}
static const u8 *glyph(unsigned code){
 for(unsigned i=0;i<128;i++)if(glyph_cache[i].code==code)return glyph_cache[i].bitmap;
 unsigned i=glyph_slot++%128;GuiGlyph r={.version=1,.size=sizeof r,.code=code};
 if(k32_call(GUI_FONT,(u32)&r,sizeof r)||r.columns!=kt_glyph(code)){
  for(unsigned j=0;j<32;j++)r.bitmap[j]=font[('?' -32)*16+j%16];
 }
 glyph_cache[i].code=code;for(unsigned j=0;j<32;j++)glyph_cache[i].bitmap[j]=r.bitmap[j];return glyph_cache[i].bitmap;
}
static void text(int x,int y,const char *s,u32 c){
 for(;*s;){unsigned cells=kt_unit(s,s[1]?2:1),ch=(u8)*s;
  if(!cells){cells=1;ch='?';}
  unsigned code=cells==2?(ch<<8)|(u8)s[1]:ch;const u8 *bits_data;
  s+=cells;int origin=x;x+=cells*8;
  if(ch==' '||x<=clip.x||origin>=clip.x+clip.w||y+16<=clip.y||y>=clip.y+clip.h)continue;
  bits_data=code>=32&&code<=126?font+(code-32)*16:glyph(code);
  for(int row=0;row<16;row++){
   int yy=y+row;if(yy<clip.y||yy>=clip.y+clip.h)continue;
   u8 *line=framebuffer+yy*info.pitch;
   for(unsigned col=0;col<cells*8;col++)if((bits_data[col/8*16+row]&(128>>(col%8)))&&origin+(int)col>=clip.x&&origin+(int)col<clip.x+clip.w){
    if(info.bpp==1)line[origin+col]=(u8)c;else ((u16 *)line)[origin+col]=(u16)c;
   }
  }
 }
}
static void text_double(int x,int y,const char *s,u32 c){
 for(;*s;){
  unsigned cells=kt_unit(s,s[1]?2:1),ch=(u8)*s;
  if(!cells){cells=1;ch='?';}
  unsigned code=cells==2?(ch<<8)|(u8)s[1]:ch;s+=cells;
  if(ch!=' '&&x<clip.x+clip.w&&x+(int)cells*16>clip.x&&y<clip.y+clip.h&&y+32>clip.y){
   const u8 *bits=code>=32&&code<=126?font+(code-32)*16:glyph(code);
   for(unsigned row=0;row<16;row++)for(unsigned col=0;col<cells*8;col++)
    if(bits[col/8*16+row]&(128>>(col%8)))box(x+col*2,y+row*2,2,2,c);
  }
  x+=cells*16;
 }
}
static void ui_label(int x,int y,const char *s,u32 c){text(x,y,KU(s),c);}
static void number(int x,int y,u32 n,u32 c){char rev[10],out[11];unsigned k=0,i=0;do{rev[k++]=(char)('0'+n%10);n/=10;}while(n);while(k)out[i++]=rev[--k];out[i]=0;text(x,y,out,c);}
static void button(int x,int y,int w,const char *s){
 s=KU(s);
 box(x,y,w,28,color(11));box(x+1,y+1,w-2,26,color(3));box(x+2,y+2,w-4,1,color(7));text(x+(w-8*(int)k32_len(s))/2,y+6,s,color(0));
}
static u32 style_color(u32 value){return rgb(value>>16,(value>>8)&255,value&255);}
static void style_box(int x,int y,int w,int h,u32 value){box(x,y,w,h,style_color(value));}
static void style_label(int x,int y,const char *s,u32 value){text(x,y,s,style_color(value));}
static void style_outline(int x,int y,int w,int h,int thickness,u32 value){
 style_box(x,y,w,thickness,value);style_box(x,y+h-thickness,w,thickness,value);
 style_box(x,y,thickness,h,value);style_box(x+w-thickness,y,thickness,h,value);
}
static void styled_button(int x,int y,int w,int h,const char *s,unsigned role,unsigned state){
 style_box(x,y,w,h,desktop_button_fill(role,state));
 style_outline(x,y,w,h,1,DESKTOP_COLOR_BORDER);
 if(state&DESKTOP_CONTROL_FOCUSED)
  style_outline(x,y,w,h,DESKTOP_FOCUS_WIDTH,DESKTOP_COLOR_FOCUS);
 style_label(x+(w-8*(int)k32_len(s))/2,y+(h-16)/2,s,
  state&DESKTOP_CONTROL_DISABLED?DESKTOP_COLOR_MUTED:DESKTOP_COLOR_INK);
}
static const KGalleryPainter gallery_painter={.label=style_label,.button=styled_button,.rectangle=style_box};
static void draw_notice(void){
 static const char *const messages[]={
  [GN_SLOTS_FULL]="No free app slot. End a session.",
  [GN_START_FAILED]="App could not start. Try again.",
  [GN_APP_MISSING]="App missing. Check the boot disk.",
  [GN_WORKERS_STARTED]="CPU workers started.",
  [GN_WORKERS_STOPPED]="CPU workers stopped.",
  [GN_WORKER_FAILED]="Worker action failed. Check the slots.",
  [GN_SESSION_FAILED]="Session failed. Check the command.",
  [GN_APP_FAULTED]="App stopped unexpectedly. Reopen it.",
  [GN_LAYOUT_SAVE_FAILED]="Window layout could not be saved.",
  [GN_HISTORY_SAVE_FAILED]="Recent items could not be saved.",
  [GN_DOCUMENT_PIN_LIMIT]="Unpin a document before adding another."
 };
 if(!scene.notice)return;
 Rect r=scene_notice_rect(&scene);
 int success=scene.notice==GN_WORKERS_STARTED||scene.notice==GN_WORKERS_STOPPED;
 style_box(r.x,r.y,r.w,r.h,DESKTOP_COLOR_PAPER);
 style_outline(r.x,r.y,r.w,r.h,1,DESKTOP_COLOR_BORDER);
 style_box(r.x,r.y,DESKTOP_SPACE_SMALL,r.h,success?DESKTOP_COLOR_SUCCESS:DESKTOP_COLOR_DANGER);
 style_label(r.x+DESKTOP_SPACE_LARGE,r.y+(r.h-16)/2,KU(messages[scene.notice]),DESKTOP_COLOR_INK);
 style_label(r.x+r.w-DESKTOP_SPACE_LARGE,r.y+(r.h-16)/2,"x",DESKTOP_COLOR_INK);
}
static const u8 *video_glyph(void *unused,unsigned code){(void)unused;return code>=32&&code<=126?font+(code-32)*16:glyph(code);}
static void terminal_video(unsigned id,Rect area){
 KvCanvas *v=&video_cache[id];KvMeta *m=&v->meta;
 unsigned w=m->flags&KV_GRAPH?m->width:640,h=m->flags&KV_GRAPH?m->height:400;
 if(!w||w>640||!h||h>400)return;
 if(video_composed_slot!=id||video_composed_revision!=video_revision[id]||video_composed_generation!=video_generation[id]||video_composed_phase!=terminal_phase){
  kt_video_compose(v,video_pixels,terminal_phase,video_glyph,0);video_composed_slot=id;video_composed_revision=video_revision[id];video_composed_generation=video_generation[id];video_composed_phase=terminal_phase;
 }
 u32 colors[25];for(unsigned i=0;i<16;i++)colors[i]=rgb(m->palette[i][0]*17,m->palette[i][1]*17,m->palette[i][2]*17);
 for(unsigned i=0;i<8;i++)colors[16+i]=rgb(i&2?255:0,i&4?255:0,i&1?255:0);
 colors[24]=rgb(0,0,0);
 static u16 source_x[800];
 int dw=area.w,dh=area.h;if(dw*(int)h>dh*(int)w)dw=dh*w/h;else dh=dw*h/w;
 for(int i=0;i<dw;i++)source_x[i]=i*w/dw;
 for(int yy=clip.y;yy<clip.y+clip.h&&yy<area.y+dh;yy++){
  unsigned sy=(yy-area.y)*h/dh;u8 *line=framebuffer+yy*info.pitch;
  for(int xx=clip.x;xx<clip.x+clip.w&&xx<area.x+dw;xx++){unsigned sx=source_x[xx-area.x];u32 c=colors[video_pixels[sy*w+sx]];if(info.bpp==1)line[xx]=(u8)c;else ((u16 *)line)[xx]=(u16)c;}
 }
}
static void terminal(unsigned id,int x,int y,int width,int height){
 GuiSessionView *v=&views[id];
 const char *names[]={"Run","Stop","Resume","End","Again"};
 for(int i=0;i<5;i++)button(x+8+i*64,y+36,60,names[i]);
 int cols=(width-16)/8,rows=(height-104)/16;
 box(x+8,y+70,cols*8,rows*16,terminal_valid[id]&&!v->kind?rgb(0,0,0):color(0));
 Rect before=clip;Rect area={x+8,y+70,cols*8,rows*16};
 int right=clip.x+clip.w,bottom=clip.y+clip.h;
 if(clip.x<area.x)clip.x=area.x;
 if(clip.y<area.y)clip.y=area.y;
 if(right>area.x+area.w)right=area.x+area.w;
 if(bottom>area.y+area.h)bottom=area.y+area.h;
 clip.w=right>clip.x?right-clip.x:0;clip.h=bottom>clip.y?bottom-clip.y:0;
 int extra=video_valid[id]&&video_generation[id]==v->generation&&kt_video_extra(&video_cache[id].meta);
 if(extra)terminal_video(id,area);
 for(int row=0;!extra&&row<rows;row++){
  int rr=row+scene.scroll_y[id];if(rr>=25)break;
  if(terminal_valid[id]&&terminals[id].generation==v->generation&&!v->kind){
   const u16 *codes=terminals[id].codes+rr*80;
   u16 row_glyphs[80];u8 row_halves[80];k98_row(codes,row_glyphs,row_halves);
   for(int col=0;col<cols&&col+scene.scroll_x[id]<80;col++){
    unsigned at=col+scene.scroll_x[id],half=row_halves[at],code=row_glyphs[at],attr=terminals[id].attributes[rr*80+at];
    if(code==' '&&!(attr&28))continue;
    const u8 *bitmap=kt_video_gaiji(video_valid[id]&&video_generation[id]==v->generation?&video_cache[id]:0,codes[at-half]);
    if(!bitmap)bitmap=code>=32&&code<=126?font+(code-32)*16:glyph(code);
    u32 ink=rgb((k98_rgb(attr)>>16)&255,(k98_rgb(attr)>>8)&255,k98_rgb(attr)&255);
    int xx=x+8+col*8,yy=y+70+row*16;
    for(unsigned scan=0;scan<16;scan++){
     unsigned bits=k98_ink(bitmap[half*16+scan],attr,scan,terminal_phase,0);
     int py=yy+scan;if(py<clip.y||py>=clip.y+clip.h)continue;
     u8 *line=framebuffer+py*info.pitch;
     for(unsigned bit=0;bit<8;bit++)if(bits&(128>>bit)){int px=xx+bit;if(px<clip.x||px>=clip.x+clip.w)continue;if(info.bpp==1)line[px]=(u8)ink;else ((u16 *)line)[px]=(u16)ink;}
    }
   }
   continue;
  }
  char line[81];int col=0;
  for(;col<cols&&col+scene.scroll_x[id]<80;col++)line[col]=v->screen[rr*80+col+scene.scroll_x[id]];
  line[col]=0;text(x+8,y+70+row*16,line,color(7));
 }
 if(!extra&&v->owned&&(v->state==1||v->state==2)&&scene.active==(int)id+2){
  unsigned half=0,code=32;if(terminal_valid[id]&&v->row<25&&v->column<80)code=k98_glyph(terminals[id].codes+v->row*80,v->column,&half);
  box(x+8+((int)v->column-(int)half-scene.scroll_x[id])*8,y+84+((int)v->row-scene.scroll_y[id])*16,code>255?16:8,2,color(6));
 }
 clip=before;
 int yy=y+height-30;const char *arrows[]={"Left","Right","Up","Down"};
 for(int i=0;i<4;i++)button(x+8+i*48,yy,44,arrows[i]);
 if(scene.command[id]||scene.shell[id]){
  button(x+206,yy,60,scene.command[id]?"Enter":"Cancel");
  button(x+270,yy,60,scene.command[id]?"Esc":"EOF");
 }else ui_label(x+208,yy+6,errors[id]?"Error:":!v->owned&&v->state?"Other owner":v->state==1?"Running":v->state==2?"Waiting":v->state==3?"Stopped":v->state==4?"Faulted":"Ended",color(10));
 if(errors[id]&&!scene.command[id]&&!scene.shell[id])number(x+264,yy+6,-errors[id],color(10));
 text(x+width-18,y+height-18,"/",color(10));
 if(editing[id]){
  box(x+8,y+70,width-16,72,color(3));ui_label(x+16,y+78,"Run (End current first):",color(0));
  int n=lengths[id],visible=(width-40)/8,start=n>visible?n-visible:0;
  text(x+16,y+102,paths[id]+start,color(0));box(x+16+(n-start)*8,y+116,8,2,color(4));
  ui_label(x+16,y+126,"Enter: start   Esc: cancel",color(10));
 }
}
static void graphical(unsigned id,int x,int y,int width,int height){
 const char *names[]={"Run","Stop","Resume","End","Again"};
 for(int i=0;i<5;i++)button(x+8+i*64,y+36,60,names[i]);
 Rect saved=clip;int left=x+8,top=y+70,right=x+width-8,bottom=y+height-34;
 if(clip.x>left)left=clip.x;
 if(clip.y>top)top=clip.y;
 if(clip.x+clip.w<right)right=clip.x+clip.w;
 if(clip.y+clip.h<bottom)bottom=clip.y+clip.h;
 clip=(Rect){left,top,right>left?right-left:0,bottom>top?bottom-top:0};
 box(x+8,y+70,width-16,height-104,rgb(234,240,245));
 KgRequest *r=&clients[id];
 for(unsigned i=0;r->token&&i<r->count;i++){
  KgDraw *d=&r->draw[i];int xx=x+8+d->x,yy=y+70+d->y;u32 c=rgb(d->color>>16,(d->color>>8)&255,d->color&255);
  if(d->op==KG_RECT)box(xx,yy,d->w,d->h,c);
  if(d->op==KG_TEXT)text(xx,yy,d->text,c);
  if(d->op==KG_TEXT_DOUBLE)text_double(xx,yy,d->text,c);
  if(d->op==KG_BITMAP||d->op==KG_BITMAP_SCALE){
   unsigned secondary=d->id==extra_images[id].handle;const KgImage *im=secondary?&extra_images[id]:&images[id];
   if(im->valid&&im->handle==d->id&&im->generation==r->generation&&im->token==r->token){
    unsigned sx=d->color&65535,sy=d->color>>16;
    const u8 *dimensions=(const u8 *)d->text;unsigned sw=d->op==KG_BITMAP?(unsigned)d->w:dimensions[0]|(unsigned)dimensions[1]<<8,sh=d->op==KG_BITMAP?(unsigned)d->h:dimensions[2]|(unsigned)dimensions[3]<<8;
    if(sx<im->width&&sy<im->height&&sw&&sh&&sw<=im->width-sx&&sh<=im->height-sy){
     const u16 *p=secondary?extra_pixels[id]+im->front*KG_SURFACE_EXTRA_PIXELS:image_pixels[id]+im->front*KG_SURFACE_PIXELS;
     int l=xx>clip.x?xx:clip.x,t=yy>clip.y?yy:clip.y,right=xx+d->w<clip.x+clip.w?xx+d->w:clip.x+clip.w,bottom=yy+d->h<clip.y+clip.h?yy+d->h:clip.y+clip.h;
     if(d->op==KG_BITMAP_SCALE){unsigned cw=dimensions[8]|(unsigned)dimensions[9]<<8,ch=dimensions[10]|(unsigned)dimensions[11]<<8;if(cw&&ch){int cx=x+8+(dimensions[4]|(unsigned)dimensions[5]<<8),cy=y+70+(dimensions[6]|(unsigned)dimensions[7]<<8);if(l<cx)l=cx;if(t<cy)t=cy;if(right>cx+(int)cw)right=cx+cw;if(bottom>cy+(int)ch)bottom=cy+ch;}}
     if(l>=right||t>=bottom)continue;
     for(int col=l;col<right;col++)scale_xmap[col-l]=sx+(unsigned)(col-xx)*sw/(unsigned)d->w;
     /* The viewport and damage intersection already bounds this entire row.
      * Store it directly and use the exact existing cube quantization. */
     for(int row=t;row<bottom;row++){
      unsigned source=(sy+(unsigned)(row-yy)*sh/(unsigned)d->h)*im->width;u8 *out=framebuffer+row*info.pitch+l*info.bpp;
      if(info.bpp==2)for(int col=l;col<right;col++){u16 c=p[source+scale_xmap[col-l]];*out++=c;*out++=c>>8;}
      else for(int col=l;col<right;col++){u16 c=p[source+scale_xmap[col-l]];*out++=16+bitmap_cube5[c>>11]*36+bitmap_cube6[(c>>5)&63]*6+bitmap_cube5[c&31];}
     }
    }
   }
  }
  if(d->op==KG_BUTTON){
   box(xx,yy,d->w,d->h,color(11));box(xx+1,yy+1,d->w-2,d->h-2,c);box(xx+2,yy+2,d->w-4,1,color(7));
   int label=(int)k32_len(d->text);
   if(preferences.large&&d->h>=34&&d->w>=16*label+4)text_double(xx+(d->w-16*label)/2,yy+(d->h-32)/2,d->text,color(0));
   else text(xx+(d->w-8*label)/2,yy+(d->h-16)/2,d->text,color(0));
  }
 }
 if(!r->token)ui_label(x+24,y+86,views[id].state==4?"Application faulted":"Application ended",color(10));
 clip=saved;
 ui_label(x+16,y+height-22,views[id].state==3?"Stopped":views[id].state==4?"Faulted":views[id].state==0?"Ended":"PM32 GUI client",color(10));
 text(x+width-18,y+height-18,"/",color(10));
 if(editing[id]){
  box(x+8,y+70,width-16,72,color(3));ui_label(x+16,y+78,"Run (End current first):",color(0));
  int n=lengths[id],visible=(width-40)/8,start=n>visible?n-visible:0;
  text(x+16,y+102,paths[id]+start,color(0));ui_label(x+16,y+126,"Enter: start   Esc: cancel",color(10));
 }
}
static void client_mouse(const GuiEvent *e){
 if(e->type!=GUI_BUTTON&&e->type!=GUI_MOTION)return;
 if(capture){unsigned id=capture-1;GuiWindow *w=&scene.window[id+2];
  if(scene.active!=(int)id+2||!w->open||w->hidden||editing[id]||scene.drag||scene.resize||clients[id].token!=capture_token||views[id].generation!=capture_generation)capture=0;
 }
 if(scene.active<2||scene.drag||scene.resize)return;
 unsigned id=capture?capture-1:(unsigned)scene.active-2;
 if(!scene.graphical[id]||!clients[id].token||editing[id])return;
 GuiWindow *w=&scene.window[id+2];int x=(int)e->x-w->r.x-8,y=(int)e->y-w->r.y-70;
 if(!w->open||w->hidden||(!capture&&(x<0||y<0||x>=w->r.w-16||y>=w->r.h-104)))return;
 unsigned flags=clients[id].event.control;
 if(e->type==GUI_MOTION&&(!(flags&KG_INPUT_MOTION)||!capture))return;
 if(e->type==GUI_BUTTON&&(e->buttons&1)&&(flags&KG_INPUT_MOTION)){
  capture=id+1;capture_generation=views[id].generation;capture_token=clients[id].token;
 }
 KgEvent event={.type=e->type==GUI_MOTION?KG_POINTER:KG_MOUSE,.x=x,.y=y,.key=e->buttons};
 KgRequest *r=&clients[id];
 for(unsigned i=0;i<r->count;i++){KgDraw *d=&r->draw[i];if(d->op==KG_BUTTON&&x>=d->x&&y>=d->y&&x-d->x<d->w&&y-d->y<d->h)event.control=d->id;}
 errors[id]=client_event(id,&event);
 if(!(e->buttons&1))capture=0;
}
static void window(int id){
 GuiWindow *w=&scene.window[id];if(!w->open||w->hidden)return;
 int x=w->r.x,y=w->r.y,width=w->r.w,height=w->r.h;
 if(x+width+6<=clip.x||x>=clip.x+clip.w||y+height+6<=clip.y||y>=clip.y+clip.h)return;
 box(x+5,y+5,width,height,rgb(8,16,28));box(x,y,width,height,color(11));box(x+1,y+1,width-2,height-2,color(2));
 box(x+2,y+2,width-4,DESKTOP_TITLE_HEIGHT,color(scene.active==id?4:9));box(x+3,y+3,width-6,1,color(scene.active==id?13:3));
 char label[6]="DOS 1";if(id>=2){if(scene.graphical[id-2]){label[0]='A';label[1]='p';label[2]='p';}label[4]='1'+id-2;}
 if(id>=2&&scene.menu[id-2]){box(x+8,y+3,56,26,color(3));ui_label(x+12,y+9,"Menu",color(0));}
 else ui_label(x+12,y+9,id>=2?label:id?"System":"Welcome",color(scene.active==id?7:0));
 box(x+width-47,y+6,18,18,color(3));box(x+width-25,y+6,18,18,color(3));
 text(x+width-43,y+7,"_",color(0));text(x+width-21,y+7,"x",color(0));
 if(id>=2){
  Rect saved=clip;
  int title_right=x+width-126;
  if(clip.x+clip.w>title_right)clip.w=title_right>clip.x?title_right-clip.x:0;
  ui_label(x+72,y+9,scene.graphical[id-2]?clients[id-2].title:views[id-2].program,color(7));
  clip=saved;
  box(x+width-120,y+3,32,26,color(3));
  text(x+width-108,y+8,scene.placement[id-2]==2?">":scene.placement[id-2]==3?"=":"<",color(0));
  box(x+width-84,y+3,32,26,color(3));
  text(x+width-72,y+8,scene.placement[id-2]==1?"=":"+",color(0));
  if(scene.graphical[id-2])graphical(id-2,x,y,width,height);else terminal(id-2,x,y,width,height);
 }else if(!id){
  static const char *const apps[LAUNCHER_BUTTONS]={"Shortcuts","User dictionary",
   "Calculator","Clock","DOS Shell","Files","Editor","Images","32-bit shell","Calendar"};
  if(scene.help==LAUNCHER_GALLERY){kgallery_draw(x,y,&gallery_painter);return;}
  ui_label(x+18,y+46,scene.help==LAUNCHER_GUIDE?"Keyboard shortcuts":"32-bit protected mode",color(0));
  for(unsigned i=0;i<(scene.help==LAUNCHER_GUIDE?2u:LAUNCHER_BUTTONS);i++){
   Rect b=scene_launcher_button(i);
   button(x+b.x,y+b.y,b.w,scene.help==LAUNCHER_GUIDE?(i?"Components":"Back"):apps[i]);
  }
  if(!scene.help)styled_button(x+216,y+38,98,36,KU("Memo  F4"),DESKTOP_BUTTON_SECONDARY,0);
  if(scene.help){
   ui_label(x+18,y+124,"F1: Shortcuts   F2: Apps",color(0));
   ui_label(x+18,y+150,"Tab: Switch window",color(0));
   ui_label(x+18,y+176,"F8: Language   F10: Exit",color(0));
   ui_label(x+18,y+202,"G: Components",color(0));
  }
  ui_label(x+18,y+LAUNCHER_HINT_Y,scene.action_result<0?"Slots full / error":"F1: Shortcuts",color(10));
 }else{
  ui_label(x+16,y+46,"Desktop: private context",color(0));
  ui_label(x+16,y+70,scene.slots==4?"Application slots: 4":"Application slots: 2",color(0));
  for(int i=0;i<scene.slots;i++){
   int yy=y+98+i*24;number(x+16,yy,i+1,color(0));ui_label(x+32,yy,scene.states[i]==1?"Running":scene.states[i]==2?"Waiting":scene.states[i]==3?"Stopped":scene.states[i]==4?"Faulted":"Free",color(10));if(views[i].owned&&views[i].program[0])text(x+108,yy,views[i].program,color(0));
   else number(x+108,yy,scene.counters[i],color(0));
  }
  ui_label(x+16,y+146+(scene.slots-2)*24,scene.action_result<0?"Slots busy / unavailable":"CPU workers: T=start S=stop",color(10));
  styled_button(x+16,y+168+(scene.slots-2)*24,94,28,KU("Test CPU"),DESKTOP_BUTTON_PRIMARY,0);
  styled_button(x+124,y+168+(scene.slots-2)*24,94,28,KU("Stop"),DESKTOP_BUTTON_DANGER,0);
  styled_button(x+16,y+198+(scene.slots-2)*24,202,24,KU("Settings  F9"),DESKTOP_BUTTON_SECONDARY,0);
 }
}
static void cursor(void){
 static const u16 edge[16]={0x8000,0xc000,0xe000,0xf000,0xf800,0xfc00,0xfe00,0xff00,0xff80,0xffc0,0xfc00,0xdc00,0x8e00,0x0600,0x0700,0x0300};
 static const u16 body[16]={0,0,0x4000,0x6000,0x7000,0x7800,0x7c00,0x7e00,0x7f00,0x7c00,0x5800,0x0800,0x0400,0x0200,0x0200,0};
 for(int y=0;y<16;y++)for(int x=0;x<16;x++)if(edge[y]&(0x8000>>x))box(scene.mx+x,scene.my+y,1,1,color(body[y]&(0x8000>>x)?7:0));
}
static void draw_shell(void){
 if(!scene.shell_page)return;
 Rect r=shell_panel();box(r.x,r.y,r.w,r.h,color(2));box(r.x,r.y,r.w,24,color(0));
 const char *title=scene.shell_page==SHELL_KUMA?"Kuma":scene.shell_page==SHELL_SEARCH?"Find apps":scene.shell_page==SHELL_SWITCH?"Switch windows":scene.shell_page==SHELL_RECENT?"Recent items":scene.shell_page==SHELL_REBOOT?"Restart computer":"About KGUI32";
 ui_label(r.x+8,r.y+5,title,color(7));
 if(scene.shell_page==SHELL_ABOUT){
  ui_label(20,76,"KGUI32 32-bit protected desktop",color(0));
  ui_label(20,102,info.bpp==2?"Cirrus 800x600 RGB565":"PEGC 640x400 indexed",color(0));
  ui_label(20,130,"Alt+arrows: place window",color(0));
  ui_label(20,154,"Alt+F4: close / Esc: back",color(0));return;
 }
 unsigned count=shell_count(&scene);
 if(scene.shell_page==SHELL_KUMA){button(16,62,92,"Recent");button(112,62,120,"Restart");}
 if(scene.shell_page==SHELL_SEARCH){
  box(16,62,204,26,color(7));text(20,68,scene.shell_query,color(0));
  if(!count)ui_label(20,103,"No matching apps",color(0));
 }else if(scene.shell_page==SHELL_SWITCH)ui_label(20,66,"Alt+Tab / Enter",color(0));
 else if(scene.shell_page==SHELL_RECENT)ui_label(20,count?66:102,count?"Del: forget selected":"Nothing opened yet",color(0));
 else if(scene.shell_page==SHELL_REBOOT)ui_label(20,66,"Review unsaved changes first",color(0));
 if((scene.shell_page==SHELL_SEARCH||scene.shell_page==SHELL_RECENT)&&count>SHELL_ROWS)button(224,62,88,"Next page");
 for(unsigned i=0;i<SHELL_ROWS&&i+scene.shell_top<count;i++){
  unsigned at=i+scene.shell_top;Rect row=shell_row(i);
  int item=shell_item(&scene,at);
  const char *label=shell_label(&scene,at);
  if(scene.shell_page==SHELL_SWITCH){
   label=item==0?"Welcome":item==1?"System":scene.graphical[item-2]?clients[item-2].title:views[item-2].program;
   if(!*label)label="DOS Shell";
  }else if(scene.shell_page==SHELL_SEARCH||(scene.shell_page==SHELL_RECENT&&shell_document(&scene,item))){
   button(246,row.y+2,64,shell_pinned(&scene,item)?"Unpin":"Pin");row.w=222;
  }
  if(scene.shell_page==SHELL_RECENT){button(174,row.y+2,64,"Forget");row.w=150;}
  label=KU(label);
  unsigned role=at==scene.shell_selected?DESKTOP_BUTTON_PRIMARY:DESKTOP_BUTTON_SECONDARY;
  styled_button(row.x,row.y,row.w,row.h,preferences.large?"":label,role,0);
  if(preferences.large){
   char clipped[48];unsigned n=kt_prefix(label,k32_len(label),(row.w-12)/16);
   for(unsigned j=0;j<n;j++)clipped[j]=label[j];
   clipped[n]=0;
   text_double(row.x+6,row.y,clipped,role==DESKTOP_BUTTON_PRIMARY?color(7):color(0));
  }
 }
}
static void draw_desktop_status(void){
 unsigned hour=(desktop_minute/60)%24,min=desktop_minute%60;
 char clock[]={"00:00"};
 clock[0]='0'+hour/10;clock[1]='0'+hour%10;clock[3]='0'+min/10;clock[4]='0'+min%10;
 char slots[]="0/0";slots[0]+=desktop_slots;slots[2]+=scene.slots;
 text(100,9,clock,color(7));text(148,9,preferences.ime==1?"ROM":preferences.ime==2?"JIS":"IME -",color(9));
 text(188,9,slots,color(7));
 for(unsigned i=0;i<2;i++){
  int x=216+i*26,fill=color((drive_lamps&(1u<<i))?9:4);
  box(x+1,12,4,6,fill);box(x,13,6,4,fill);
  text(x+7,9,i?"HDD":"FD",color(i==layout_available?7:4));
 }
}
static void update_status(const GuiEvent *event){
 unsigned slots=0;
 for(int i=0;i<scene.slots;i++)slots+=views[i].owned&&views[i].state>0&&views[i].state<4;
 if(slots!=desktop_slots){desktop_slots=slots;scene_damage(&scene,(Rect){100,3,174,28});}
 if(event->type!=GUI_TIMER)return;
 int counters=k32_call(GUI_DISK_ACTIVITY,0,0);
 if(counters>=0){
  unsigned lights=drive_lamps;
  for(unsigned i=0;i<2;i++){
   unsigned count=(unsigned)counters>>(16*i)&0x7fff;
   if(count!=drive_counts[i]){drive_counts[i]=count;drive_last[i]=event->ticks;lights|=1u<<i;}
   else if(((event->ticks-drive_last[i])&0xffffff)>=153600)lights&=~(1u<<i);
  }
  if(lights!=drive_lamps){drive_lamps=lights;scene_damage(&scene,(Rect){216,3,58,28});}
 }
 if(((event->ticks-desktop_clock_poll)&0xffffff)<307200)return;
 desktop_clock_poll=event->ticks;
 int seconds=k32_call(GUI_DESKTOP_CLOCK,0,0);
 if(seconds<0)return;
 unsigned minute=(unsigned)seconds/60;
 if(minute!=desktop_minute){desktop_minute=minute;scene_damage(&scene,(Rect){100,3,174,28});}
}
static int paint(void){
 if(!scene.damage.w)return 0;
 clip=scene.damage;
 for(int y=clip.y;y<clip.y+clip.h;y++)box(clip.x,y,clip.w,1,preferences.theme==KG_THEME_CONTRAST?color(0):rgb(18+y*14/info.height,30+y*34/info.height,58+y*40/info.height));
 box(0,0,info.width,32,color(0));box(0,31,info.width,1,color(4));
 Rect kuma=shell_button();button(kuma.x,kuma.y,kuma.w,"Kuma");draw_desktop_status();
 for(int i=0;i<scene.slots;i++){char label[]="DOS 1";label[4]+=i;if(scene.slots==2)button(info.width-312+i*92,3,84,i?"New DOS 2":"New DOS 1");else button(info.width-360+i*56,3,52,label);}
 button(info.width-128,3,120,"Exit to DOS");
 for(int i=0;i<scene.slots+2;i++)window(scene.order[i]);
 box(0,info.height-32,info.width,32,color(0));box(0,info.height-32,info.width,1,color(4));
 button(6,info.height-29,136,"Welcome");button(152,info.height-29,136,"System");
 if(scene.active<=1)box(10+146*scene.active,info.height-5,128,DESKTOP_ACTIVE_MARKER_HEIGHT,color(6));
 for(int i=0;i<scene.slots;i++)if(scene.window[i+2].open){char label[]="DOS 1";label[4]+=i;int step=scene.slots==2?146:(info.width-300)/4;button(300+i*step,info.height-29,step-10,scene.graphical[i]?clients[i].title:label);if(scene.active==i+2)box(304+i*step,info.height-5,step-18,DESKTOP_ACTIVE_MARKER_HEIGHT,color(6));}
 draw_notice();draw_shell();
 cursor();
 unsigned row_bytes=clip.w*info.bpp;
 for(int offset=0;offset<clip.h;offset+=64){
  int rows=clip.h-offset;if(rows>64)rows=64;
  for(int row=0;row<rows;row++){
   const u8 *source=framebuffer+(clip.y+offset+row)*info.pitch+clip.x*info.bpp;u8 *to=packed+row*row_bytes;unsigned n=row_bytes;
   __asm__ volatile("cld; rep movsb":"+S"(source),"+D"(to),"+c"(n)::"memory","cc");
  }
  GuiPresent p={(u32)packed,clip.x,clip.y+offset,clip.w,rows,row_bytes};
  int e=k32_call(GUI_PRESENT,(u32)&p,sizeof p);if(e)return e;
 }
 scene.damage=(Rect){0};gui_state.presents++;gui_state.frames++;return 0;
}
static int equal(const char *a,const char *b){while(*a&&*a==*b){a++;b++;}return *a==*b;}
static int session_negative(void){
 static const u32 client_header[4]={1,sizeof(KgRequest),0,0};
 if(k32_call(GUI_CLIENT_VIEW,0x10000,sizeof(KgRequest))!=-14)return 12;
 if(k32_call(GUI_CLIENT_VIEW,(u32)client_header,sizeof(KgRequest))!=-14)return 13;
 client_io=(KgRequest){.version=1,.size=sizeof client_io,.slot=scene.slots};
 if(k32_call(GUI_CLIENT_VIEW,(u32)&client_io,sizeof client_io)!=-22)return 14;
 client_io.slot=0;client_io.generation=~0u;
 if(k32_call(GUI_CLIENT_EVENT,(u32)&client_io,sizeof client_io)!=-1)return 15;
 static const u32 readonly_header[4]={1,sizeof(GuiSessionView),0,0};
 GuiSessionCommand c={.version=1,.size=sizeof c,.operation=GS_END};
 GuiSessionView v={.version=1,.size=sizeof v};
 if(k32_call(GUI_SESSION,0x10000,sizeof c)!=-14)return 1;
 if(k32_call(GUI_SNAPSHOT,0x10000,sizeof v)!=-14)return 2;
 if(k32_call(GUI_SNAPSHOT,(u32)readonly_header,sizeof v)!=-14)return 11;
 if(k32_call(GUI_SNAPSHOT,0x800000,sizeof v)!=-22)return 3; /* RO ELF header is not a valid versioned request. */
 if(k32_call(GUI_SESSION,(u32)&c,sizeof c-1)!=-22)return 4;
 c.slot=scene.slots;if(k32_call(GUI_SESSION,(u32)&c,sizeof c)!=-22)return 5;c.slot=0;
 c.operation=7;if(k32_call(GUI_SESSION,(u32)&c,sizeof c)!=-22)return 6;
 for(unsigned id=0;id<(unsigned)scene.slots;id++){
  c.slot=id;c.operation=GS_END;c.generation=views[id].generation+1;
  if(k32_call(GUI_SESSION,(u32)&c,sizeof c)!=-116)return 7;
  c.generation=views[id].generation;c.key=1;
  /* A live DOS parent owns both workers in the KGPROOF scenario. */
  if(views[id].state&&!views[id].owned){
   for(unsigned op=GS_END;op<=GS_RESTART;op++){c.operation=op;if(k32_call(GUI_SESSION,(u32)&c,sizeof c)!=-1)return 8;}
   c.operation=GS_FOCUS;if(k32_call(GUI_SESSION,(u32)&c,sizeof c)!=-1)return 9;
   c.operation=GS_CREATE;c.generation=0;c.path[0]='X';c.path[1]=0;
   if(k32_call(GUI_SESSION,(u32)&c,sizeof c)!=-16)return 10;
  }
 }
 return 0;
}
static int negative(void){
 GuiPresent p={(u32)packed,0,0,~0u,1,640};
 if(k32_call(GUI_INFO,0x10000,sizeof info)!=-14)return 1;
 if(k32_call(GUI_FONT,0x800000,sizeof font)!=-14)return 2;
 if(k32_call(GUI_PRESENT,(u32)&p,sizeof p)!=-22)return 3;
 p=(GuiPresent){0x10000,0,0,16,16,16*info.bpp};
 if(k32_call(GUI_PRESENT,(u32)&p,sizeof p)!=-14)return 4;
 if(k32_call(GUI_EVENT,0x10000,sizeof(GuiEvent))!=-14)return 5;
 GuiGlyph g={.version=1,.size=sizeof g,.code=0x93fa};
 if(k32_call(GUI_FONT,0x10000,sizeof g)!=-14)return 6;
 if(k32_call(GUI_FONT,(u32)&g,sizeof g)||g.columns!=2)return 7;
 g.columns=0;g.code=0x80;if(k32_call(GUI_FONT,(u32)&g,sizeof g)!=-22)return 8;
 g.code=0x817f;if(k32_call(GUI_FONT,(u32)&g,sizeof g)!=-22)return 9;
 g.code=0x10093fa;if(k32_call(GUI_FONT,(u32)&g,sizeof g)!=-22)return 10;
 g.code=0x93fa;g.version=2;if(k32_call(GUI_FONT,(u32)&g,sizeof g)!=-22)return 11;
 static const GuiGlyph ro={.version=1,.size=sizeof(GuiGlyph),.code=0x93fa};
 if(k32_call(GUI_FONT,(u32)&ro,sizeof ro)!=-14)return 12;
 if(k32_call(GUI_SNAPSHOT,0x10000,sizeof terminal_stage)!=-14)return 13;
 static const u32 text_ro[4]={2,sizeof(GuiTerminalView),0,0};
 if(k32_call(GUI_SNAPSHOT,(u32)text_ro,sizeof terminal_stage)!=-14)return 14;
 terminal_stage=(GuiTerminalView){.version=3,.size=sizeof terminal_stage};
 if(k32_call(GUI_SNAPSHOT,(u32)&terminal_stage,sizeof terminal_stage)!=-22)return 15;
 if(k32_call(GUI_SNAPSHOT,(u32)&terminal_stage,sizeof terminal_stage-1)!=-22)return 16;
 GuiSurfaceAck ack={.version=1,.size=sizeof ack};
 if(k32_call(GUI_SURFACE_ACK,0x10000,sizeof ack)!=-14)return 17;
 ack.slot=scene.slots;if(k32_call(GUI_SURFACE_ACK,(u32)&ack,sizeof ack)!=-22)return 18;
 ack.slot=0;if(k32_call(GUI_SURFACE_ACK,(u32)&ack,sizeof ack)!=-1)return 19;
 return 0;
}
static int startup_japanese(int argc,char **argv){
 if(argc>1)return equal(argv[1],"ja");
 char value[3];
 int handle=k32_open(DESKTOP_LANGUAGE_FILE);if(handle<0)return 0;
 int length=k32_read(handle,value,sizeof value);k32_close(handle);
 return desktop_japanese(value,length);
}
int main(int argc,char **argv){
 terminal_phase=1;
 if((k32_query()&23)!=23)return 1;
 int e=k32_call(GUI_INFO,(u32)&info,sizeof info);if(e)return 2;
 if(info.magic!=0x3149474b||info.version!=1||info.width>800||info.height>600||(info.bpp!=1&&info.bpp!=2)||info.pitch!=info.width*info.bpp)return 3;
 for(unsigned i=0;i<32;i++)bitmap_cube5[i]=(i*255/31+25)/51;
 for(unsigned i=0;i<64;i++)bitmap_cube6[i]=(i*255/63+25)/51;
 if(k32_call(GUI_FONT,(u32)font,sizeof font))return 4;
 k32_call(GUI_IMAGE_CLIP,(u32)&shared,sizeof shared);
 gui_state.bitmap_magic=0x3153474b;gui_state.bitmap_meta=(u32)images;gui_state.bitmap_pixels=(u32)image_pixels;gui_state.bitmap_stride=sizeof image_pixels[0];gui_state.bitmap_frames=(u32)clients;
 gui_state.surfaces_version=2;gui_state.extra_meta=(u32)extra_images;gui_state.extra_pixels=(u32)extra_pixels;gui_state.extra_stride=sizeof extra_pixels[0];gui_state.surface_budget=sizeof image_pixels+sizeof extra_pixels;
 scene_init(&scene,info.width,info.height);scene_slots(&scene,info.features&GUI_FOUR_SLOTS?4:2);gui_state.x0=scene.window[0].r.x;gui_state.y0=scene.window[0].r.y;gui_state.x1=scene.window[1].r.x;gui_state.y1=scene.window[1].r.y;
 preferences.language=startup_japanese(argc,argv);
 K32Revision prefs_file;kg_preferences_load(&preferences,&prefs_file);
 if(argc>1&&equal(argv[1],"ja"))preferences.language=1;
 ku_language=preferences.language;k32_call(GUI_LANGUAGE,ku_language|(preferences.large<<1),0);
 layout_available=!kg_state_read(KG_LAYOUT_FILE,&layout,sizeof layout,&layout_revision);
 if(!layout_validate(&layout)&&layout_revision.exists)layout_available=0;
 load_pins();load_history();
 for(int id=0;id<2;id++)layout_restore(&layout,&scene,id,0);
 gui_state.x0=scene.window[0].r.x;gui_state.y0=scene.window[0].r.y;
 gui_state.x1=scene.window[1].r.x;gui_state.y1=scene.window[1].r.y;
 refresh();
 int once=argc>1&&equal(argv[1],"once"),proof=argc>1&&equal(argv[1],"proof");
 if(argc>1&&equal(argv[1],"sessionapi")){int bad=session_negative();gui_state.error=bad?200+bad:0;return bad?5:0;}
 if(proof)scene.action_result=k32_call(GUI_PROOF,1,0);
 if(argc>1&&equal(argv[1],"badapi")){int bad=negative();if(bad){gui_state.error=100+bad;return 5;}once=1;}
 if(paint()){gui_state.error=6;return 6;}
 if(argc>1&&equal(argv[1],"fault"))*(volatile u32 *)0x10000=1;
 if(argc>1&&equal(argv[1],"spin"))for(;;)gui_state.heartbeat++;
 for(;;){
  GuiEvent4 event4={0};int got=k32_call(GUI_EVENT,(u32)&event4,sizeof event4);GuiEvent event=event4.base;
  if(got<0){gui_state.error=7;return 7;}
  if(!got){k32_wait(K32_WAIT_TIME,16);continue;}
  scene_notice_tick(&scene,event.ticks);
  collect_history();
  gui_state.events++;gui_state.heartbeat++;key_modifiers=event.type==GUI_KEY?event.buttons:0;if(event.type==GUI_KEY)gui_state.last_key=event.key;
  if(event.type==GUI_KEY&&event.key==287){ku_language^=1;k32_call(GUI_LANGUAGE,ku_language|(preferences.large<<1),0);scene_damage(&scene,(Rect){0,0,info.width,info.height});if(paint())return 7;continue;}
  if(event.type==GUI_TIMER){
   int revision=k32_call(GUI_PREFERENCES_REV,0,0);
   if(revision>=0&&(unsigned)revision!=preference_revision){
    preference_revision=(unsigned)revision;K32Revision changed;
    if(!kg_preferences_load(&preferences,&changed)&&changed.exists){
     ku_language=preferences.language;k32_call(GUI_LANGUAGE,ku_language|(preferences.large<<1),0);
     scene_damage(&scene,(Rect){0,0,info.width,info.height});
    }
   }
   if(layout_dirty&&!scene.drag&&!scene.resize&&((event.ticks-layout_changed)&0xffffff)>=230400)layout_save();
  }
  if(!terminal_tick)terminal_tick=event.ticks;
  if(((event.ticks-terminal_tick)&0xffffff)>=153600){terminal_tick=event.ticks;terminal_phase^=1;
   for(unsigned id=0;id<(unsigned)scene.slots;id++)if(terminal_valid[id]&&scene.window[id+2].open&&!scene.window[id+2].hidden){
    if(video_valid[id]&&kt_video_extra(&video_cache[id].meta)){
     KvCanvas *v=&video_cache[id];KvMeta *m=&v->meta;if(!(m->flags&KV_TEXT))continue;
     unsigned blink=0;for(unsigned i=0;i<4096;i++)if(v->attributes[i]&2){blink=1;break;}
     Rect r=scene.window[id+2].r;
     if(blink){scene_window_damage(&scene,id+2,r);continue;}
     if((m->flags&KV_CURSOR)&&(views[id].state==1||views[id].state==2)){
      int dw=(r.w-16)/8*8,dh=(r.h-104)/16*16,gw=m->flags&KV_GRAPH?m->width:640,gh=m->flags&KV_GRAPH?m->height:400;if(dw*gh>dh*gw)dw=dh*gw/gh;else dh=dw*gh/gw;
      unsigned cursor=(m->text_cursor-m->text_start)&4095,row=cursor/m->text_pitch,col=cursor%m->text_pitch;
      if(row<m->rows&&col<m->columns)scene_window_damage(&scene,id+2,(Rect){r.x+8+(int)(col*dw/m->columns),r.y+70+(int)(row*dh/m->rows),dw/(int)m->columns+2,dh/(int)m->rows+2});
     }
    }else {unsigned blink=0;for(unsigned i=0;i<2000;i++)if(terminals[id].attributes[i]&2){blink=1;break;}
     if(blink)scene_window_damage(&scene,id+2,scene.window[id+2].r);
    }
   }
  }
  int previous_focus=scene.active;Rect previous_rect[4];for(unsigned id=0;id<(unsigned)scene.slots;id++)previous_rect[id]=scene.window[id+2].r;
  /* A child may OPEN/SUBSCRIBE between timer events. Route input against
   * its current generation/token, never the ended root's cached canvas. */
  if(event.type==GUI_KEY||event.type==GUI_BUTTON)refresh();
  Rect prior[6];unsigned placement[4];
  for(int id=0;id<scene.slots+2;id++)prior[id]=scene.window[id].r;
  for(int id=0;id<scene.slots;id++)placement[id]=scene.placement[id];
  int act;
  for(unsigned id=0;id<(unsigned)scene.slots;id++){
   scene.shell[id]=views[id].owned&&views[id].kind==1&&equal(views[id].program,"KSH32.EXE");
   scene.command[id]=editing[id];
  }
  if(event.type==GUI_KEY&&!scene.shell_page&&!((event.buttons&KG_MOD_ALT)&&(event.key==9||event.key==283||(event.key>=256&&event.key<=259)))&&scene.active>=2&&event.key!=281&&event.key!=11&&event.key!=289&&
     ((editing[scene.active-2]&&event.key!=9)||(!editing[scene.active-2]&&clients[scene.active-2].token&&event.key!=289&&((event.key==9&&(clients[scene.active-2].event.control&KG_INPUT_TAB))||((clients[scene.active-2].event.control&KG_INPUT_KEYS)&&event.key!=9))))){
   scene.slot=scene.active-2;scene.key=event.key;act=ACT_KEY;
  }else act=scene_event4(&scene,&event4);
  if(layout_available)for(int id=0;id<scene.slots+2;id++)
   if(!same_rect(prior[id],scene.window[id].r)||(id>=2&&placement[id-2]!=(unsigned)scene.placement[id-2])){
    if(layout_remember(&layout,&scene,id,id<2?0:views[id-2].program)){layout_dirty=1;layout_changed=event.ticks;}
   }
  if(event.type==GUI_BUTTON&&!(event.buttons&1)&&!scene.drag&&!scene.resize)layout_save();
  if(act==ACT_EXIT||act==ACT_REBOOT){
   if(!exit_pending){exit_pending=act==ACT_REBOOT?2:1;closing_slot=0;}
   act=ACT_NONE;
  }
  if(act==ACT_END&&scene.slot<scene.slots&&clients[scene.slot].token&&(clients[scene.slot].event.control&KG_INPUT_CLOSE)&&
     ((event.type==GUI_KEY&&(event.buttons&KG_MOD_ALT)&&event.key==283)||
      (event.type==GUI_BUTTON&&event.x>=(u32)(scene.window[scene.slot+2].r.x+scene.window[scene.slot+2].r.w-25)&&event.y<(u32)(scene.window[scene.slot+2].r.y+30)))){
   unsigned id=scene.slot;scene.window[id+2].open=1;scene_focus(&scene,id+2);
   KgEvent q={.type=KG_QUIT,.key=1};if(!client_event(id,&q)){close_generation[id]=views[id].generation;close_token[id]=clients[id].token;close_cancel[id]=clients[id].event.key;}act=ACT_NONE;
  }
  action(act);if(event.type==GUI_TIMER)refresh();
  update_status(&event);
  if(exit_pending){
   if(closing_slot&&(!clients[closing_slot-1].token||clients[closing_slot-1].token!=close_token[closing_slot-1]))closing_slot=0;
   if(!closing_slot){
    int id=-1;
    for(int at=scene.slots+1;at>=0;at--){
     int w=scene.order[at]-2;
     if(w>=0&&clients[w].token&&(clients[w].event.control&KG_INPUT_CLOSE)){id=w;break;}
    }
    if(id<0){
     layout_save();save_history();
     if(exit_pending==2)k32_call(GUI_REBOOT,0,0);
     return 0;
    }
    scene.window[id+2].open=1;scene_focus(&scene,id+2);
    KgEvent q={.type=KG_QUIT,.key=2};
    if(!client_event(id,&q)){
     closing_slot=id+1;close_generation[id]=views[id].generation;
     close_token[id]=clients[id].token;close_cancel[id]=clients[id].event.key;
    }else exit_pending=0;
   }
  }
  for(unsigned id=0;id<(unsigned)scene.slots;id++)if(scene.graphical[id]&&clients[id].token){
   Rect r=scene.window[id+2].r;
   if(r.w!=previous_rect[id].w||r.h!=previous_rect[id].h){capture=0;KgEvent change={.type=KG_RESIZE,.width=r.w-16,.height=r.h-104};client_event(id,&change);}
   if(previous_focus!=scene.active&&(previous_focus==(int)id+2||scene.active==(int)id+2)){KgEvent focus={.type=KG_FOCUS,.key=scene.active==(int)id+2};client_event(id,&focus);capture=0;}
  }
  if(act==ACT_NONE)client_mouse(&event);
  input_focus();
  gui_state.mx=scene.mx;gui_state.my=scene.my;gui_state.x0=scene.window[0].r.x;gui_state.y0=scene.window[0].r.y;gui_state.x1=scene.window[1].r.x;gui_state.y1=scene.window[1].r.y;gui_state.active=scene.active;gui_state.drag=scene.drag;
  terminal_report();
  e=paint();if(e){gui_state.error=8;return 8;}
  if((once&&gui_state.events>=20)||(proof&&gui_state.events>=16))return 0;
 }
}
