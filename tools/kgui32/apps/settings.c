#include "../sdk/kgui.h"
#include "../sdk/widgets.h"
#include "../sdk/preferences.h"
#include "../sdk/ime.h"
#include "../../k38632/boot-settings.h"
#include "../../k38632/sdk/fs.h"
#include "../../k38632/sdk/libc.h"
#include "ui-settings.h"

enum {S_XMS=510,S_EMS,S_LESS,S_MORE,S_BUFFERS,S_FD98,S_CONFIG,S_AUTO,S_RELOAD,S_CLOSE,S_OK,S_CANCEL,S_PREFS,S_LANG,S_THEME,S_IME,S_APPLY,S_BACK,S_SIZE};
typedef struct {u32 magic,version,frames,saved,error,xms,ems,files,buffers,fd98,dirty,ready,control;} SettingsStats;
volatile SettingsStats stats __attribute__((section(".data.gui_stats")))={.magic=0x3253474b,.version=1};
typedef struct {char data[K32_FS_BOOT_LIMIT];unsigned n,hash;int error;} BootFile;
static BootFile boot[2];
static const char *const boot_path[]={"CONFIG.SYS","AUTOEXEC.BAT"};
static KgBootSettings old,next;
static KwFocus focus;
static unsigned dialog,page,closing;
static KgPreferences prefs,next_prefs;
static K32Revision prefs_revision;
static int prefs_error;
static const char *status;

static unsigned digest(const char *data,unsigned n){
 unsigned h=2166136261u;for(unsigned i=0;i<n;i++)h=(h^(unsigned char)data[i])*16777619u;return h;
}
static int preferences_dirty(void){
 return next_prefs.language!=prefs.language||next_prefs.theme!=prefs.theme||next_prefs.ime!=prefs.ime||next_prefs.large!=prefs.large;
}
static unsigned modified(void){
 return ((next.xms!=old.xms||next.ems!=old.ems||next.files!=old.files||next.buffers!=old.buffers)?1:0)|
  ((next.fd98!=old.fd98)?2:0);
}
static void load(unsigned startup){
 BootFile *f=&boot[startup];
 K32Fs r=k32_fs_request(K32_FS_BOOT_OPEN,boot_path[startup],0);
 f->error=k32_fs(&r);
 if(f->error)return;
 int n=k32_io_read(r.token,f->data,r.length);
 int closed=k32_io_close(r.token);
 if(n<0||(unsigned)n!=r.length||closed<0){f->error=n<0?n:closed<0?closed:-5;return;}
 f->n=(unsigned)n;f->hash=digest(f->data,f->n);
 kg_settings_read(&old,startup,f->data,f->n);
}
static void reload(void){
 load(0);load(1);next=old;
 prefs_error=kg_preferences_load(&prefs,&prefs_revision);next_prefs=prefs;
 stats.error=boot[0].error<0?-boot[0].error:boot[1].error<0?-boot[1].error:0;
 status=stats.error?"Unable to read boot settings":"Changes take effect after reboot";
}
static void save(unsigned startup){
 BootFile *f=&boot[startup];
 if(f->error||!(modified()&(1u<<startup)))return;
 K32Fs r=k32_fs_request(K32_FS_BOOT_WRITE,boot_path[startup],0);
 r.length=f->n;r.hash=f->hash;
 if(startup)r.flags=next.fd98?K32_BOOT_FD98:0;
 else r.flags=kg_settings_config_flags(&next);
 int e=k32_fs(&r);
 if(!e){
  stats.saved++;
  load(startup);
  if(!f->error){
   if(startup)next.fd98=old.fd98;
   else {unsigned fd98=next.fd98;next=old;next.fd98=fd98;}
  }
  if(r.flags)e=1;
 }
 stats.error=e<0?-e:0;
 status=e==1?"Saved / BAK kept":e==0?"Saved / reboot to apply":e==-116?"Boot file changed / reload":
  e==-13?"Boot file is read-only":e==-17||e==-117?"TMP or BAK needs recovery":e==-28?"Disk full / TMP kept":"Boot file save failed";
}
static void button(int x,int y,int w,unsigned id,const char *label,unsigned disabled,unsigned primary){
 unsigned state=(disabled?KW_DISABLED:0)|(focus.current==id-S_XMS?KW_FOCUSED:0);
 kw_draw_button((KwRect){x,y,w,36},id,KU(label),primary?KW_PRIMARY:KW_SECONDARY,state);
}
static void preference_button(int x,int y,int w,unsigned id,const char *label,unsigned disabled){
 kw_draw_button((KwRect){x,y,w,36},id,KU(label),id==S_APPLY?KW_PRIMARY:KW_SECONDARY,
   (disabled?KW_DISABLED:0)|(focus.current==id-S_LANG?KW_FOCUSED:0));
}
static void draw_preferences(unsigned w){
 kw_readable(8,kg_large_text()?0:8,KU("Desktop preferences"),w-16,0x20344a);
 preference_button(8,34,w-16,S_LANG,next_prefs.language?"Language: Japanese":"Language: English",prefs_error);
 int half=(w-24)/2;
 preference_button(8,74,half,S_THEME,next_prefs.theme?"Display: High contrast":"Display: Classic",prefs_error);
 preference_button(8,114,w-16,S_IME,next_prefs.ime==KI_ROMAJI?"Input: Romaji":next_prefs.ime==KI_KANA?"Input: JIS kana":"Input: IME off",prefs_error);
 preference_button(16+half,74,half,S_SIZE,next_prefs.large?"Text: Large":"Text: Standard",prefs_error);
 preference_button(8,154,half,S_APPLY,"Apply preferences",prefs_error||!preferences_dirty());
 preference_button(16+half,154,half,S_BACK,"Boot settings",0);
 kw_label(8,198,prefs_error?"Desktop preferences unavailable":status,(w-16)/8,0x20344a);
}
static void preference_command(unsigned cmd){
 if(cmd==S_LANG&&!prefs_error)next_prefs.language^=1;
 if(cmd==S_THEME&&!prefs_error)next_prefs.theme^=1;
 if(cmd==S_SIZE&&!prefs_error)next_prefs.large^=1;
 if(cmd==S_IME&&!prefs_error)next_prefs.ime=(next_prefs.ime+1)%3;
 if(cmd==S_APPLY&&!prefs_error&&preferences_dirty()){
  int e=kg_preferences_save(&next_prefs,&prefs_revision);
  if(e>=0){prefs=next_prefs;prefs_error=kg_preferences_load(&prefs,&prefs_revision);kg_preferences_changed();}
  status=e>=0?"Preferences saved / IME for new apps":e==-116?"Desktop preferences changed / reload":"Preference save failed / original kept";
 }
 if(cmd==S_BACK){page=0;focus=(KwFocus){.count=11,.current=10};status="Changes take effect after reboot";}
}
static int draw(void){
 unsigned w=kg_width(),h=kg_height(),dirty=modified();
 char label[32];
 kg_begin();kg_rect(0,0,w,h,0xeaf0f5);
 if(page)draw_preferences(w);
 else {
 kg_label(8,8,"Boot settings / A:",0x20344a);
 kw_draw_button((KwRect){(int)w-180,0,172,28},S_PREFS,KU("Desktop preferences"),KW_SECONDARY,focus.current==10?KW_FOCUSED:0);
 unsigned half=(w-26)/2;
 button(8,30,half,S_XMS,next.xms?"XMS on":"XMS off",boot[0].error,0);
 button(18+half,30,half,S_EMS,next.ems?"EMS on":"EMS off",boot[0].error||!next.xms,0);
 button(8,70,40,S_LESS,"-",boot[0].error||next.files<=20,0);
 button(52,70,40,S_MORE,"+",boot[0].error||next.files>=60,0);
 k32_snprintf(label,sizeof label,"FILES=%u",next.files);kg_text(100,80,label,0x20344a);
 k32_snprintf(label,sizeof label,"BUFFERS=%u",next.buffers);button(18+half,70,half,S_BUFFERS,label,boot[0].error,0);
 button(8,110,half,S_FD98,next.fd98?"FD98 startup on":"FD98 startup off",boot[1].error,0);
 button(18+half,110,half,S_AUTO,"Apply AUTOEXEC",boot[1].error||!(dirty&2),1);
 button(8,150,half,S_CONFIG,"Apply CONFIG",boot[0].error||!(dirty&1),1);
 int quarter=(half-6)/2;
 button(18+half,150,quarter,S_RELOAD,"Reload",0,0);
 button(24+half+quarter,150,quarter,S_CLOSE,"Close",0,0);
 kw_label(8,194,status,(w-16)/8,0x20344a);
 }
 if(dialog){
  kg_begin();kg_rect(0,0,w,h,0xb4c2d0);
  unsigned y=(h-136)/2;
  kg_rect(12,y,w-24,136,0xffffff);
  kw_label(24,y+12,dialog==S_CONFIG?(!next.xms?"XMS off may stop KGUI32 after reboot":"EMS on may stop KGUI32 after reboot"):dialog==S_RELOAD?"Discard edits and reload?":"Discard unsaved settings?",(w-40)/8,0x20344a);
  kw_draw_button((KwRect){24,(int)y+79,114,36},S_OK,KU("OK"),KW_DANGER,0);
  kw_draw_button((KwRect){148,(int)y+79,114,36},S_CANCEL,KU("Cancel"),KW_SECONDARY,KW_FOCUSED);
 }
 stats.xms=next.xms;stats.ems=next.ems;stats.files=next.files;stats.buffers=next.buffers;stats.fd98=next.fd98;stats.dirty=dirty;
 stats.ready=(!boot[0].error?1:0)|(!boot[1].error?2:0);
 int e=kg_present();if(!e)stats.frames++;return e;
}
static int command(unsigned cmd){
 stats.control=cmd;
 if(cmd==S_CLOSE||cmd==S_RELOAD){
  if(modified()||preferences_dirty()){dialog=cmd;return 0;}
  if(cmd==S_CLOSE)return 1;
  reload();return 0;
 }
 if(cmd==S_AUTO){save(1);return 0;}
 if(boot[0].error&&cmd!=S_FD98)return 0;
 if(cmd==S_XMS){next.xms=!next.xms;if(!next.xms)next.ems=0;}
 if(cmd==S_EMS&&next.xms)next.ems=!next.ems;
 if(cmd==S_LESS&&next.files>20)next.files=next.files>60?60:next.files-1;
 if(cmd==S_MORE&&next.files<60)next.files=next.files<20?20:next.files+1;
 if(cmd==S_BUFFERS)next.buffers=next.buffers==8?16:8;
 if(cmd==S_FD98&&!boot[1].error)next.fd98=!next.fd98;
 if(cmd==S_CONFIG){if((old.xms&&!next.xms)||(!old.ems&&next.ems))dialog=cmd;else save(0);}
 return 0;
}
int main(void){
 if(kg_open("Settings",484,248)<0)return 1;
 if(kg_capabilities()<0||kg_subscribe(KG_INPUT_FLAGS|KG_INPUT_CLOSE|KG_INPUT_MODIFIERS)){kg_close();return 2;}
 focus.count=11;reload();int redraw=1,closed=0;
 while(!closed){
  KgEvent e;int got=kg_poll(&e);
  if(got<0)break;
  if(got){
   unsigned cmd=e.type==KG_MOUSE&&(e.key&1)?e.control:0;
   if(e.type==KG_QUIT){
    if(modified()||preferences_dirty()){dialog=S_CLOSE;closing=1;}else closed=1;
   }else if(dialog){
    if(cmd==S_CANCEL||(e.type==KG_KEY&&(e.key==27||e.key==13))){if(closing){kg_cancel_close();closing=0;}dialog=0;}
    else if(cmd==S_OK||(e.type==KG_KEY&&(e.key=='y'||e.key=='Y'))){
     unsigned confirm=dialog;dialog=0;
     if(confirm==S_CONFIG)save(0);
     if(confirm==S_RELOAD)reload();
     if(confirm==S_CLOSE)closed=1;
    }
   }else if(!kw_focus(&focus,&e)){
    if(page){
     if(e.type==KG_KEY&&e.key==13)cmd=S_LANG+focus.current;
     if(e.type==KG_KEY&&e.key==27)cmd=S_BACK;
     if(cmd>=S_LANG&&cmd<=S_SIZE){focus.current=cmd-S_LANG;preference_command(cmd);}
    }else {
    if(e.type==KG_KEY&&e.key==13)cmd=focus.current==10?S_PREFS:S_XMS+focus.current;
    if(cmd==S_PREFS||(e.type==KG_KEY&&(e.key=='p'||e.key=='P'))){
     page=1;focus=(KwFocus){.count=6};status="Choose display, input, and language";
    }
    if(e.type==KG_KEY&&e.key==27)cmd=S_CLOSE;
    if(cmd>=S_XMS&&cmd<=S_CLOSE){focus.current=cmd-S_XMS;closed=command(cmd);}
    }
   }
   redraw=1;
  }
  if(redraw&&!closed){int e=draw();redraw=e==-22;if(e<0&&!redraw)break;}
  k32_wait(K32_WAIT_TIME,16);
 }
 return kg_close()?3:0;
}
