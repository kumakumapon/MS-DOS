#include "files-manager.h"
#include "../sdk/widgets.h"
#include "../sdk/menu.h"
#include "../../k38632/sdk/fs.h"
#include "../../k38632/sdk/libc.h"
#include "ui-files-manager.h"

enum {FM_BACK=400,FM_UP,FM_TRASH,FM_FOLDER,FM_COPY,FM_MOVE,FM_RENAME,FM_DELETE,
 FM_OPEN,FM_REFRESH,FM_OK,FM_CANCEL,FM_NAME,FM_SIZE,FM_DATE,FM_KIND,FM_DRIVE,FM_RECOVERY,FM_RESCUE};
enum {LIST_FOCUS,NAME_FOCUS,SIZE_FOCUS,DATE_FOCUS,KIND_FOCUS,UP_FOCUS,TRASH_FOCUS,
 OPEN_FOCUS,REFRESH_FOCUS,FOLDER_FOCUS,COPY_FOCUS,MOVE_FOCUS,RENAME_FOCUS,DELETE_FOCUS,BACK_FOCUS,DRIVE_FOCUS,RECOVERY_FOCUS};
#define FILE_LIMIT 192
static K32DirEntry entries[FILE_LIMIT];
static KwList list;
static KwScroll scroll;
static KwInput target;
static KwFocus focus;
static const char *root_name,*status;
static char folder[K32_FS_PATH],selected[K32_FS_PATH];
static unsigned count,cursor,loading,trash,archive_scan,action,sort,descending;
static unsigned drive_mask,volume,recovery_view;
static unsigned selected_dir;
static struct {
 unsigned phase,count,scan,item,cursor,bytes;
 char relative[FILE_LIMIT][K32_FS_PATH];
 unsigned char folders[FILE_LIMIT];
} tree;
static unsigned reader,remaining,opening;
static K32Io writer;
static unsigned char block[4096];

static const char *find_char(const char *s,char c){for(;*s;s++)if(*s==c)return s;return 0;}
static void copy_text(char *out,const char *in,unsigned cap){
 unsigned n=k32_len(in);if(n>=cap)n=cap-1;k32_memcpy(out,in,n);out[n]=0;
}
static const char *error_text(int e){
 switch(e){
 case -2:return "File or folder is missing";
 case -13:return "Read-only or protected item";
 case -16:return "File is in use";
 case -17:return "Name already exists";
 case -21:return "Folder cannot be moved here";
 case -15:return "Drive unavailable";
 case -125:return "Copy cancelled";
 case -22:return "Use a DOS 8.3 Work path";
 case -39:return "Only empty folders can be recycled";
 case -27:return "File exceeds the 1 MiB limit";
 case -28:return "Disk full / TMP kept";
 case -117:return "TMP or BAK needs recovery";
 default:return "Disk operation failed";
 }
}
static void refresh(void){
 count=list.count=list.selected=list.top=cursor=archive_scan=0;loading=1;
}
void files_manager_open(const char *root,unsigned show_drive){
 root_name=root;status=0;folder[0]=0;trash=action=sort=descending=recovery_view=0;
 K32Fs r=k32_fs_request(K32_FS_DRIVES,0,0);
 drive_mask=k32_fs(&r)?1:r.length;volume=show_drive?1:0;
 focus=(KwFocus){.count=RECOVERY_FOCUS+1};list.rows=1;refresh();
}
static int join(char out[K32_FS_PATH],const char *base,const char *name){
 unsigned n=k32_len(base),m=k32_len(name);
 if(!m||n+(n!=0)+m>=K32_FS_PATH)return 0;
 k32_memcpy(out,base,n);if(n)out[n++]='\\';
 k32_memcpy(out+n,name,m+1);return 1;
}
static int compare(const K32DirEntry *a,const K32DirEntry *b){
 unsigned x=0,y=0;
 if((a->attributes^b->attributes)&16)return a->attributes&16?-1:1;
 if(sort==1){x=a->bytes;y=b->bytes;}
 if(sort==2){x=a->date<<16|a->time;y=b->date<<16|b->time;}
 if(x!=y)return (x<y?-1:1)*(descending?-1:1);
 if(sort==3){
  const char *ae=find_char(a->name,'.'),*be=find_char(b->name,'.');
  int cmp=k32_strcmp(ae?ae:"",be?be:"");
  if(cmp)return descending?-cmp:cmp;
 }
 int cmp=k32_strcmp(a->name,b->name);
 return descending?-cmp:cmp;
}
static void sort_entries(void){
 for(unsigned i=1;i<count;i++){
  K32DirEntry e=entries[i];unsigned j=i;
  while(j&&compare(&e,&entries[j-1])<0){entries[j]=entries[j-1];j--;}
  entries[j]=e;
 }
 list.count=count;kw_reveal(&list);
}
static int finish_copy(int error,int keep){
 if(reader){
  int closed=k32_io_close(reader);reader=0;
  if((!error||error==-125)&&closed<0)error=closed;
 }
 if(writer.token){
  int e=error?k32_io_abort(&writer,keep):k32_io_commit(&writer);
  if(e<0&&!error){error=e;k32_io_abort(&writer,1);}
  if(e<0&&error==-125)error=e;
  writer.token=0;
 }
 return error;
}
static void finish_folder(int error){
 if(tree.phase){
  K32Fs r=k32_fs_request(K32_FS_TREE_CANCEL,0,0);k32_fs(&r);
  tree.phase=0;
 }
 status=error==-125?"Cancelled / TMP folder kept":error?"Folder incomplete / TMP kept":"Folder operation complete";
 refresh();
}
static int copy_finished(int error){
 if(tree.phase){
  if(error)finish_folder(error);else tree.item++;
 }else {
  status=error?error_text(error):"Copied";refresh();
 }
 return 1;
}
static int copy_step(void){
 unsigned n=remaining>sizeof block?sizeof block:remaining;
 if(!n)return copy_finished(finish_copy(0,0));
 int got=k32_io_read(reader,block,n);
 if(got!=(int)n)return copy_finished(finish_copy(got<0?got:-5,1));
 int e=k32_io_write(&writer,block,n);
 if(e!=(int)n)return copy_finished(finish_copy(e<0?e:-28,1));
 remaining-=n;return 1;
}
static int copy_start(const char *source,const char *dest,unsigned operation){
 K32Fs from=k32_fs_request(K32_FS_OPEN,source,0),to=k32_fs_request(operation,dest,0);
 int e=k32_fs(&from);if(e)return e;
 reader=from.token;remaining=from.length;
 e=k32_fs(&to);
 if(e){k32_io_close(reader);reader=0;return e;}
 writer=k32_io_name(K32_IO_WRITE,0);writer.token=to.token;return 0;
}
static void start_folder(const char *dest,unsigned move){
 K32Fs r=k32_fs_request(K32_FS_TREE_BEGIN,selected,dest);r.flags=move;
 int e=k32_fs(&r);if(e){status=error_text(e);return;}
 tree.phase=1;tree.count=tree.item=1;tree.scan=tree.cursor=tree.bytes=0;
 tree.relative[0][0]=0;tree.folders[0]=1;status="Scanning folder / Esc cancels";
}
static int folder_step(void){
 if(tree.phase==1){
  while(tree.scan<tree.count&&!tree.folders[tree.scan])tree.scan++;
  if(tree.scan==tree.count){tree.phase=2;status="Copying folder / Esc cancels";return 1;}
  char source[K32_FS_PATH];
  if(tree.relative[tree.scan][0]){
   if(!join(source,selected,tree.relative[tree.scan])){finish_folder(-22);return 1;}
  }else copy_text(source,selected,sizeof source);
  K32Fs r=k32_fs_request(tree.cursor?K32_FS_NEXT:K32_FS_LIST,source,0);
  r.flags=tree.cursor?0:K32_FS_LIST_STRICT;r.token=tree.cursor;
  int e=k32_fs(&r);tree.cursor=r.token;
  if(e){finish_folder(e);return 1;}
  if(!r.token){tree.scan++;return 1;}
  int directory=!!(r.entry.attributes&16);
  if(tree.count==FILE_LIMIT||(!directory&&(r.entry.bytes>K32_IO_LIMIT||r.entry.bytes>8u*K32_IO_LIMIT-tree.bytes))||
     !join(tree.relative[tree.count],tree.relative[tree.scan],r.entry.name)){finish_folder(-27);return 1;}
  tree.folders[tree.count++]=directory;
  if(!directory)tree.bytes+=r.entry.bytes;
  return 1;
 }
 if(tree.item==tree.count){
  K32Fs r=k32_fs_request(K32_FS_TREE_PUBLISH,0,0);int e=k32_fs(&r);
  if(!e)tree.phase=0;
  finish_folder(e);return 1;
 }
 const char *rel=tree.relative[tree.item];int e;
 if(tree.folders[tree.item]){
  K32Fs r=k32_fs_request(K32_FS_TREE_MKDIR,rel,0);e=k32_fs(&r);
  if(!e)tree.item++;
 }else{
  char source[K32_FS_PATH];
  e=join(source,selected,rel)?copy_start(source,rel,K32_FS_TREE_CREATE):-22;
 }
 if(e)finish_folder(e);
 return 1;
}
int files_manager_step(void){
 if(opening){
  int e=kg_edit_status();
  if(e!=-11){opening=0;status=e<0?(e==-16?"Both slots in use: close a task":"Cannot start Editor"):"Opened";return 1;}
 }
 if(reader)return copy_step();
 if(tree.phase)return folder_step();
 if(!loading)return 0;
 for(unsigned i=0;i<6;i++){
  K32Fs r=k32_fs_request(cursor?K32_FS_NEXT:volume?K32_FS_DRIVE_LIST:K32_FS_LIST,folder,0);
  r.token=cursor;r.flags=cursor?0:volume?volume-1:recovery_view?K32_FS_LIST_TMP:archive_scan?K32_FS_LIST_ARCHIVED:trash;
  int e=k32_fs(&r);cursor=r.token;
  if(!e&&!cursor&&trash&&!archive_scan){archive_scan=1;continue;}
  if(e||!cursor||count==FILE_LIMIT){loading=0;if(e)status=error_text(e);else if(cursor)status="First 192 items shown";sort_entries();break;}
  entries[count++]=r.entry;
 }
 return 1;
}
void files_manager_close(void){
 if(reader)finish_copy(-125,0);
 if(tree.phase)finish_folder(-125);
}
static void execute(void){
 unsigned op=action;action=0;
 if(op==FM_RESCUE){
  K32Fs r=k32_fs_request(K32_FS_TREE_RECOVER,selected,target.text);
  int e=k32_fs(&r);
  status=e?error_text(e):"TMP folder recovered / inspect its files";
  if(!e)refresh();
  return;
 }
 if(op==FM_DELETE||op==FM_TRASH){
  K32Fs r=k32_fs_request(op==FM_DELETE?K32_FS_TRASH:K32_FS_RESTORE,selected,"");
  int e=k32_fs(&r);
  status=e?error_text(e):op==FM_DELETE?"In Trash / Restore to undo":"Restored";
  if(!e)refresh();
  return;
 }
 char dest[K32_FS_PATH];
 if(!target.length||target.length>=sizeof dest||
    ((op==FM_FOLDER||op==FM_RENAME)&&find_char(target.text,'\\'))){
  status=error_text(-22);return;
 }
 if(op==FM_FOLDER||op==FM_RENAME){
  if(!join(dest,folder,target.text)){status=error_text(-22);return;}
 }else copy_text(dest,target.text,sizeof dest);
 if(op==FM_COPY){
  if(selected_dir)start_folder(dest,0);
  else {int e=copy_start(selected,dest,K32_FS_CREATE);status=e?error_text(e):"Copying / Esc cancels";}
  return;
 }
 K32Fs r=op==FM_FOLDER?k32_fs_request(K32_FS_MKDIR,dest,""):k32_fs_request(K32_FS_MOVE,selected,dest);
 int e=k32_fs(&r);
 if(e==-21&&op==FM_MOVE&&selected_dir){start_folder(dest,1);return;}
 status=e?error_text(e):op==FM_FOLDER?"Folder created":op==FM_RENAME?"Renamed":"Moved";
 if(!e)refresh();
}
static void command(unsigned cmd){
 if(cmd==FM_DRIVE){
  do{volume=volume==26?0:volume+1;}while(volume&&!(drive_mask&(1u<<(volume-1))));
  status=0;trash=recovery_view=0;folder[0]=0;refresh();return;
 }
 if(cmd==FM_RECOVERY){
  if(!volume){recovery_view=!recovery_view;trash=0;status=0;refresh();}
  return;
 }
 if(recovery_view&&cmd>=FM_FOLDER&&cmd<=FM_RENAME)return;
 if(volume&&(cmd==FM_TRASH||(cmd>=FM_FOLDER&&cmd<=FM_DELETE)))return;
 if(cmd==FM_UP){
  char *end=0;for(char *p=folder;*p;p++)if(*p=='\\')end=p;
  if(end)*end=0;else folder[0]=0;
  status=0;refresh();return;
 }
 if(cmd==FM_TRASH){recovery_view=0;trash=!trash;status=0;refresh();return;}
 if(cmd==FM_REFRESH){status=0;refresh();return;}
 if(cmd>=FM_NAME&&cmd<=FM_KIND){
  unsigned column=cmd-FM_NAME;descending=column==sort?!descending:0;sort=column;
  list.selected=list.top=0;sort_entries();return;
 }
 if(loading)return;
 if(cmd==FM_FOLDER){if(!trash){action=cmd;target=(KwInput){0};}return;}
 if(!list.count)return;
 K32DirEntry *d=&entries[list.selected];selected_dir=!!(d->attributes&16);
 if(!join(selected,folder,d->name)){status=error_text(-22);return;}
 if(recovery_view){
  if(cmd==FM_OPEN||cmd==FM_DELETE){
   selected[k32_len(selected)-4]=0;action=FM_RESCUE;target=(KwInput){0};
  }
  return;
 }
 if(cmd==FM_OPEN&&selected_dir){
  if(trash&&(d->attributes&6)==6){status="Restore folder before opening";return;}
  copy_text(folder,selected,sizeof folder);status=0;refresh();return;}
 if(trash){if(cmd==FM_DELETE)action=FM_TRASH;return;}
 if(cmd==FM_OPEN){
  if(volume){status="Drive view is read only";return;}
  if(!folder[0]){
   if(opening)return;
   int e=kg_edit_document(d->name,1);
   opening=e>=0;status=e<0?"Open app requires Work TXT or BMP":"Opening app";
  }else status="Open app is available at Work root";
  return;
 }
 if(cmd==FM_COPY||cmd==FM_MOVE||cmd==FM_RENAME||cmd==FM_DELETE){
  action=cmd;target=(KwInput){0};
  if(cmd==FM_RENAME){target.length=target.cursor=k32_len(d->name);copy_text(target.text,d->name,sizeof target.text);}
 }
}
static const unsigned controls[]={0,FM_NAME,FM_SIZE,FM_DATE,FM_KIND,FM_UP,FM_TRASH,FM_OPEN,FM_REFRESH,FM_FOLDER,FM_COPY,FM_MOVE,FM_RENAME,FM_DELETE,FM_BACK,FM_DRIVE,FM_RECOVERY};
static KwRect list_rect(void){return (KwRect){8,66,(int)kg_width()-38,(int)list.rows*16};}
int files_manager_event(const KgEvent *e){
 if(reader||tree.phase){
  if(kw_button(FM_CANCEL,e)||(e->type==KG_KEY&&e->key==27))copy_finished(reader?finish_copy(-125,0):-125);
  return 1;
 }
 if(action){
  if(kw_button(FM_CANCEL,e)||(e->type==KG_KEY&&e->key==27)){action=0;return 1;}
  if(kw_button(FM_OK,e)||(e->type==KG_KEY&&e->key==13)){execute();return 1;}
  if(action!=FM_DELETE&&action!=FM_TRASH)kw_input(&target,e);
  return 1;
 }
 unsigned cmd=0;
 if(e->type==KG_KEY){
  if(e->key==27)return -1;
  if(e->key==4)cmd=FM_DRIVE;
  if(e->key==20&&!volume)cmd=FM_RECOVERY;
  if(e->key==13)cmd=focus.current?controls[focus.current]:FM_OPEN;
 }
 if(e->type==KG_MOUSE&&(e->key&1)){
  cmd=e->control;
  for(unsigned i=1;i<sizeof controls/sizeof *controls;i++)if(controls[i]==cmd){focus.current=i;break;}
  if(kw_inside(list_rect(),e->x,e->y))focus.current=LIST_FOCUS;
 }
 if(cmd==FM_BACK)return -1;
 if(cmd>=FM_UP&&cmd<=FM_RECOVERY){command(cmd);return 1;}
 if(kw_focus(&focus,e)){
  while((focus.current==KIND_FOCUS&&kg_width()<468)||((trash||volume||recovery_view)&&focus.current>=FOLDER_FOCUS&&focus.current<DELETE_FOCUS+(volume!=0))||(volume&&(focus.current==TRASH_FOCUS||focus.current==RECOVERY_FOCUS)))kw_focus(&focus,e);
  return 1;
 }
 if(kw_scroll(&scroll,e)){list.top=scroll.top;if(list.selected<list.top||list.selected>=list.top+list.rows)list.selected=list.top;return 1;}
 if(focus.current==LIST_FOCUS&&kw_list(&list,list_rect(),e))return 1;
 return e->type==KG_RESIZE||e->type==KG_FOCUS;
}
static void button(KwRect r,unsigned id,const char *label,unsigned at,unsigned style){
 kw_draw_button(r,id,KU(label),style,at==focus.current?KW_FOCUSED:0);
}
static void modal(unsigned w,unsigned h){
 const char *title=action==FM_RESCUE?"Recover incomplete TMP folder":action==FM_TRASH?"Restore from Trash":action==FM_DELETE?"Move to Trash?":
  action==FM_FOLDER?"New folder":action==FM_COPY?"Copy to Work path":action==FM_MOVE?"Move to Work path":"Rename";
 int x=16,y=(h-152)/2;
 kg_rect(x,y,w-32,152,0xffffff);
 kg_label(x+12,y+12,title,0x20344a);
 if(action==FM_DELETE||action==FM_TRASH)kw_text(x+12,y+42,selected,(w-56)/8,0x20344a);
 else {
  kw_draw_input((KwRect){x+12,y+36,(int)w-56,30},&target,1);
  if(action==FM_RESCUE)kw_label(x+12,y+74,"Unused name in this folder",(w-56)/8,0x20344a);
 }
 kw_draw_button((KwRect){x+12,y+102,116,36},FM_OK,KU(action==FM_DELETE?"Trash":action==FM_TRASH?"Restore":action==FM_RESCUE?"Recover":"OK"),action==FM_DELETE?KW_DANGER:KW_PRIMARY,KW_FOCUSED);
 kw_draw_button((KwRect){x+140,y+102,112,36},FM_CANCEL,KU("Cancel"),KW_SECONDARY,0);
}
static void digits(char *out,unsigned n,unsigned width){
 while(width){out[--width]='0'+n%10;n/=10;}
}
static void row(const K32DirEntry *d,unsigned y,unsigned w,int active){
 char line[48],num[12];
 k32_memset(line,' ',sizeof line);line[47]=0;
 k32_memcpy(line,d->name,k32_len(d->name));
 if(d->attributes&16)k32_memcpy(line+13,KU("Folder"),k32_len(KU("Folder")));
 else {k32_snprintf(num,sizeof num,"%u",d->bytes);k32_memcpy(line+13,num,k32_len(num));}
 char *date=line+25;
 digits(date,1980+(d->date>>9),4);date[4]='-';
 digits(date+5,(d->date>>5)&15,2);date[7]='-';digits(date+8,d->date&31,2);
 if(w>=468){
  digits(date+11,d->time>>11,2);date[13]=':';digits(date+14,(d->time>>5)&63,2);
  const char *ext=find_char(d->name,'.'),*kind=d->attributes&16?KU("Folder"):ext?ext+1:KU("File");
  k32_memcpy(line+42,kind,k32_len(kind)>5?5:k32_len(kind));
 }
 kw_text(12,y,line,(w-42)/8,active?0xffffff:0x20344a);
}
int files_manager_draw(void){
 unsigned w=kg_width(),h=kg_height();int bottom=(int)h-93;
 if(w<468&&focus.current==KIND_FOCUS)focus.current=DATE_FOCUS;
 kg_menu_enable(0);kg_begin();kg_rect(0,0,w,h,0xeaf0f5);
 if(action)modal(w,h);
 else {
  char crumb[64];
  if(volume)k32_snprintf(crumb,sizeof crumb,"%c:\\%s",(int)('A'+volume-1),folder);
  else k32_snprintf(crumb,sizeof crumb,"%s%s%s",root_name,folder[0]?"\\":"",folder);
  kw_text(8,8,crumb,(w-(volume?130:198))/8,0x20344a);
  if(!volume)button((KwRect){(int)w-184,0,62,28},FM_RECOVERY,recovery_view?"Work":"TMP",RECOVERY_FOCUS,KW_SECONDARY);
  button((KwRect){(int)w-116,0,108,28},FM_DRIVE,"Drives",DRIVE_FOCUS,KW_SECONDARY);
  button((KwRect){8,30,110,34},FM_NAME,"Name",NAME_FOCUS,KW_SECONDARY);
  button((KwRect){120,30,88,34},FM_SIZE,"Bytes",SIZE_FOCUS,KW_SECONDARY);
  button((KwRect){210,30,w>=468?136:(int)w-218,34},FM_DATE,"Updated",DATE_FOCUS,KW_SECONDARY);
  if(w>=468)button((KwRect){348,30,76,34},FM_KIND,"Type",KIND_FOCUS,KW_SECONDARY);
  list.rows=bottom>84?(bottom-68)/16:1;if(list.rows>8)list.rows=8;
  kw_reveal(&list);
  KwRect area=list_rect();
  kg_rect(area.x,area.y,area.w,area.h,0xffffff);
  scroll=(KwScroll){.r={(int)w-28,66,20,area.h},.top=list.top,.rows=list.rows,.total=list.count,.dragging=scroll.dragging,.offset=scroll.offset};
  for(unsigned i=0;i<list.rows&&list.top+i<list.count;i++){
   unsigned y=66+i*16,at=list.top+i;
   int active=list.selected==at;
   if(active)kg_rect(8,y,area.w,16,0x3270a2);
   row(&entries[at],y,w,active);
  }
  kw_draw_scroll(&scroll);
  if(reader||tree.phase)button((KwRect){8,bottom,110,36},FM_CANCEL,"Cancel",BACK_FOCUS,KW_SECONDARY);
  else {
   static const unsigned top_ids[]={FM_UP,FM_TRASH,FM_OPEN,FM_REFRESH,FM_BACK};
   static const unsigned low_ids[]={FM_FOLDER,FM_COPY,FM_MOVE,FM_RENAME,FM_DELETE};
   const char *top_labels[]={"Up",trash?"Work":"Trash","Open","Refresh","Back"};
   const char *low_labels[]={"New folder","Copy","Move","Rename",recovery_view?"Recover":trash?"Restore":"Delete"};
   unsigned width=(w-16)/5;
   for(unsigned i=0;i<5;i++){
    if(!volume||i!=1)button((KwRect){8+i*width,bottom,width-3,36},top_ids[i],top_labels[i],i==4?BACK_FOCUS:UP_FOCUS+i,KW_SECONDARY);
    if(!volume&&((!trash&&!recovery_view)||i==4))button((KwRect){8+i*width,bottom+38,width-3,36},low_ids[i],i==0&&width<87?"Mkdir":low_labels[i],FOLDER_FOCUS+i,i==4&&!trash?KW_DANGER:KW_SECONDARY);
   }
  }
  kw_label(8,h-16,loading&&!tree.phase?"Loading...":status?status:volume?"Drive root / read only":recovery_view?"TMP folders / may be incomplete":trash?"Trash / Select an item to restore":"Work / Copy and Move use Work paths",(w-16)/8,0x20344a);
 }
 return kg_present();
}
