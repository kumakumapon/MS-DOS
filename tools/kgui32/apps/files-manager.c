#include "files-manager.h"
#include "../sdk/widgets.h"
#include "../sdk/menu.h"
#include "../sdk/bitmap.h"
#include "../sdk/text.h"
#include "../../k38632/sdk/fs.h"
#include "../../k38632/sdk/libc.h"
#include "ui-files-manager.h"

enum {FM_BACK=400,FM_UP,FM_TRASH,FM_FOLDER,FM_COPY,FM_MOVE,FM_RENAME,FM_DELETE,
 FM_OPEN,FM_REFRESH,FM_OK,FM_CANCEL,FM_NAME,FM_SIZE,FM_DATE,FM_KIND,FM_DRIVE,FM_RECOVERY,FM_RESCUE,
 FM_FIND,FM_FIND_NEXT,FM_BREADCRUMB=430,FM_VOLUME=440,FM_SYSTEM=467};
enum {LIST_FOCUS,NAME_FOCUS,SIZE_FOCUS,DATE_FOCUS,KIND_FOCUS,UP_FOCUS,TRASH_FOCUS,
 OPEN_FOCUS,REFRESH_FOCUS,FOLDER_FOCUS,COPY_FOCUS,MOVE_FOCUS,RENAME_FOCUS,DELETE_FOCUS,BACK_FOCUS,DRIVE_FOCUS,RECOVERY_FOCUS,CRUMB_FOCUS};
#define DRIVE_FILTER 27u
#define FILE_LIMIT 192
static K32DirEntry entries[FILE_LIMIT];
static KwList list;
static KwScroll scroll;
static KwInput target,query;
static KwFocus focus;
static const char *root_name,*status;
static char folder[K32_FS_PATH],selected[K32_FS_PATH],preview_stage[K32_FS_PATH],preview_folder[K32_FS_PATH];
static unsigned count,cursor,loading,trash,archive_scan,action,sort,descending;
static unsigned drive_mask,volume,recovery_view,preview,preview_ok;
static unsigned drive_panel,drive_focus,show_system;
static unsigned selected_dir;
static struct {
 unsigned phase,count,scan,item,cursor,bytes,move;
 char destination[K32_FS_PATH];
 char relative[FILE_LIMIT][K32_FS_PATH];
 unsigned char folders[FILE_LIMIT];
} tree;
static unsigned reader,remaining,copy_bytes,opening;
static K32Io writer;
static unsigned char block[4096];
static char preview_name[13];
static char *preview_text;
static unsigned preview_type,preview_bytes,preview_top,preview_surface,preview_fd,preview_loaded;
static unsigned preview_match,preview_search;
static KgBmp preview_bitmap;

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
 root_name=root;status=0;folder[0]=0;trash=action=sort=descending=recovery_view=preview=0;
 drive_panel=show_system=0;
 K32Fs r=k32_fs_request(K32_FS_DRIVES,0,0);
 drive_mask=k32_fs(&r)?1:r.length;volume=show_drive?1:0;
 focus=(KwFocus){.count=CRUMB_FOCUS+4};list.rows=1;refresh();
}
static int join(char out[K32_FS_PATH],const char *base,const char *name){
 unsigned n=k32_len(base),m=k32_len(name);
 if(!m||n+(n!=0)+m>=K32_FS_PATH)return 0;
 k32_memcpy(out,base,n);if(n)out[n++]='\\';
 k32_memcpy(out+n,name,m+1);return 1;
}
static int parent_path(char *path){
 if(!*path)return 0;
 char *end=path;for(char *p=path;*p;p++)if(*p=='\\')end=p;
 *end=0;return 1;
}
static unsigned path_depth(const char *path){
 unsigned depth=!!*path;for(;*path;path++)depth+=*path=='\\';return depth;
}
static void go_to_crumb(unsigned depth){
 char *path=preview?preview_folder:folder;
 if(!depth)path[0]=0;
 else for(char *p=path;*p;p++)if(*p=='\\'&&!--depth){*p=0;break;}
 focus.current=LIST_FOCUS;status=0;refresh();
}
static int drive_visible(unsigned option){
 return !option||option==DRIVE_FILTER||(option<=26&&(drive_mask&(1u<<(option-1))));
}
static void choose_drive(unsigned option){
 if(option==DRIVE_FILTER){show_system^=1;refresh();return;}
 if(!drive_visible(option))return;
 volume=option;drive_panel=0;status=0;trash=recovery_view=0;folder[0]=0;
 focus.current=LIST_FOCUS;refresh();
}
static void close_preview(void){
 if(preview_surface)kg_surface_destroy(preview_surface);
 if(preview_fd)k32_io_close(preview_fd);
 if(preview_text)k32_free(preview_text);
 preview_text=0;preview_type=preview_surface=preview_fd=0;
}
static int read_preview(int fd,unsigned char *out,unsigned bytes){
 for(unsigned at=0;at<bytes;){
  unsigned n=bytes-at;if(n>4096)n=4096;
  int got=k32_io_read(fd,out+at,n);if(got<=0)return -5;
  at+=got;
 }
 return 0;
}
static void open_preview(const K32DirEntry *entry,const char *relative){
 const char *ext=find_char(entry->name,'.');
 unsigned bitmap=ext&&!k32_strcmp(ext,".BMP");
 if(!bitmap&&(!ext||k32_strcmp(ext,".TXT"))){status="Select a TXT or BMP file";return;}
 K32Fs r=k32_fs_request(K32_FS_TMP_OPEN,preview_stage,relative);
 int e=k32_fs(&r);if(e){status=error_text(e);return;}
 unsigned bytes=r.length;
 copy_text(preview_name,entry->name,sizeof preview_name);
 preview_top=preview_loaded=preview_search=0;preview_match=bytes;query=(KwInput){0};status=0;
 if(!bitmap){
  preview_text=bytes<=K32_IO_LIMIT?k32_alloc(bytes?bytes:1):0;
  if(!preview_text){k32_io_close(r.token);status=bytes>K32_IO_LIMIT?"Preview limit: 1 MiB":"Preview read failed";return;}
  preview_bytes=bytes;preview_type=1;preview_fd=r.token;
  return;
 }
 unsigned char *data=bytes<=KG_BITMAP_FILE_BYTES?k32_alloc(bytes?bytes:1):0;
 e=data?read_preview(r.token,data,bytes):-27;
 int closed=k32_io_close(r.token);if(!e&&closed<0)e=closed;
 if(!e){
  e=kg_bmp_header(data,bytes,bytes,&preview_bitmap);
  if(!e){
   unsigned seen=preview_bitmap.width*preview_bitmap.height;
   u16 *pixels=k32_alloc(seen*2);
   e=pixels?kg_bmp_decode(data,bytes,&preview_bitmap,pixels,seen):-12;
   if(!e){
    e=kg_surface_create(preview_bitmap.width,preview_bitmap.height);
    if(e>0){preview_surface=e;e=kg_surface_upload(preview_surface,pixels,seen*2);}
   }
   if(pixels)k32_free(pixels);
  }
 }
 if(data)k32_free(data);
 if(e){close_preview();status="Preview read failed";return;}
 preview_type=2;
}
static int load_preview(void){
 if(!preview_fd)return 0;
 unsigned n=preview_bytes-preview_loaded;if(n>4096)n=4096;
 int got=n?k32_io_read(preview_fd,preview_text+preview_loaded,n):0;
 if(got>0){preview_loaded+=got;if(preview_loaded<preview_bytes)return 0;}
 int closed=k32_io_close(preview_fd);preview_fd=0;
 if(closed<0||preview_loaded!=preview_bytes||!kt_valid(preview_text,preview_bytes,KT_LINES|KT_CRLF)){
  close_preview();status="Preview read failed";
 }
 return 1;
}
static unsigned text_row(unsigned at,char line[48],unsigned cols){
 unsigned size=0;
 while(at<preview_bytes){
  unsigned char c=preview_text[at];
  if(c=='\r'||c=='\n'){
   at++;if(c=='\r'&&at<preview_bytes&&preview_text[at]=='\n')at++;
   break;
  }
  unsigned n=kt_next(preview_text,preview_bytes,at)-at;
  if(size+n>cols)break;
  for(unsigned i=0;i<n;i++)line[size++]=c=='\t'?' ':preview_text[at+i];
  at+=n;
 }
 line[size]=0;return at;
}
static unsigned preview_columns(void){
 unsigned cols=(kg_width()-24)/(kg_large_text()?16:8);return cols>47?47:cols;
}
static unsigned row_at(unsigned offset,unsigned cols){
 unsigned at=offset;
 while(at&&preview_text[at-1]!='\n')at--;
 char line[48];
 while(at<offset){unsigned next=text_row(at,line,cols);if(next>offset)break;at=next;}
 return at;
}
static void preview_scroll(unsigned count,int back){
 unsigned cols=preview_columns();char line[48];
 while(count--){
  if(back){if(!preview_top)break;preview_top=row_at(preview_top-1,cols);}
  else {unsigned next=text_row(preview_top,line,cols);if(next>=preview_bytes)break;preview_top=next;}
 }
}
static void find_preview(void){
 if(!query.length)return;
 unsigned start=preview_match<preview_bytes?kt_next(preview_text,preview_bytes,preview_match):preview_top;
 for(unsigned pass=0;pass<2;pass++){
  unsigned end=pass?start:preview_bytes;
  for(unsigned at=pass?0:start;at<end&&query.length<=preview_bytes-at;at=kt_next(preview_text,preview_bytes,at))
   if(!k32_memcmp(preview_text+at,query.text,query.length)){
    preview_match=at;preview_top=row_at(at,preview_columns());status="Match found";return;
   }
 }
 preview_match=preview_bytes;status="Text not found";
}
static int search_input(const KgEvent *e){
 if(e->type!=KG_KEY||e->key!=22)return kw_input(&query,e);
 char text[KW_INPUT_BYTES];int n=kg_clip_get(text,sizeof text);
 return n>0?kw_insert(&query,text,n):0;
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
 kg_notify(error==-125?KG_NOTICE_CANCELLED:error?KG_NOTICE_COPY_FAILED:KG_NOTICE_COPIED);
 status=error==-125?"Cancelled / TMP folder kept":error?"Folder incomplete / TMP kept":"Folder operation complete";
 refresh();
}
static int copy_finished(int error){
 if(tree.phase){
  if(error)finish_folder(error);else tree.item++;
 }else {
  kg_notify(error==-125?KG_NOTICE_CANCELLED:error?KG_NOTICE_COPY_FAILED:KG_NOTICE_COPIED);
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
 reader=from.token;remaining=copy_bytes=from.length;
 e=k32_fs(&to);
 if(e){k32_io_close(reader);reader=0;return e;}
 writer=k32_io_name(K32_IO_WRITE,0);writer.token=to.token;return 0;
}
static void start_folder(const char *dest,unsigned move){
 K32Fs r=k32_fs_request(K32_FS_TREE_BEGIN,selected,dest);r.flags=move;
 int e=k32_fs(&r);if(e){kg_notify(KG_NOTICE_COPY_FAILED);status=error_text(e);return;}
 tree.phase=1;tree.count=tree.item=1;tree.scan=tree.cursor=tree.bytes=0;
 tree.move=move;copy_text(tree.destination,dest,sizeof tree.destination);
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
  if(!e){tree.phase=0;if(tree.move)kg_note_move(selected,tree.destination);}
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
 if(preview_fd)return load_preview();
 if(opening){
  int e=kg_edit_status();
  if(e!=-11){opening=0;status=e<0?(e==-16?"Both slots in use: close a task":"Cannot start Editor"):"Opened";return 1;}
 }
 if(reader)return copy_step();
 if(tree.phase)return folder_step();
 if(!loading)return 0;
 for(unsigned i=0;i<6;i++){
  K32Fs r=k32_fs_request(cursor?K32_FS_NEXT:volume?K32_FS_DRIVE_LIST:K32_FS_LIST,preview?preview_stage:folder,preview?preview_folder:0);
  r.token=cursor;r.flags=cursor?0:volume?(volume-1)|(show_system?K32_FS_DRIVE_SYSTEM:0):preview?K32_FS_LIST_TMP_CONTENT:recovery_view?K32_FS_LIST_TMP:archive_scan?K32_FS_LIST_ARCHIVED:trash?K32_FS_LIST_TRASH:show_system?K32_FS_LIST_SYSTEM:0;
  int e=k32_fs(&r);cursor=r.token;
  if(!e&&!cursor&&trash&&!archive_scan){archive_scan=1;continue;}
  if(e||!cursor||count==FILE_LIMIT){loading=0;if(preview)preview_ok=!e;if(e)status=error_text(e);else if(cursor)status="First 192 items shown";sort_entries();break;}
  entries[count++]=r.entry;
 }
 return 1;
}
void files_manager_close(void){
 close_preview();
 if(reader)finish_copy(-125,0);
 if(tree.phase)finish_folder(-125);
}
static void execute(void){
 unsigned op=action;action=0;
 if(op==FM_RESCUE){
  K32Fs r=k32_fs_request(K32_FS_TREE_RECOVER,selected,target.text);
  int e=k32_fs(&r);
  status=e?error_text(e):"TMP folder recovered / inspect its files";
  if(!e){preview=0;refresh();}
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
  else {int e=copy_start(selected,dest,K32_FS_CREATE);if(e)kg_notify(KG_NOTICE_COPY_FAILED);status=e?error_text(e):"Copying / Esc cancels";}
  return;
 }
 K32Fs r=op==FM_FOLDER?k32_fs_request(K32_FS_MKDIR,dest,""):k32_fs_request(K32_FS_MOVE,selected,dest);
 int e=k32_fs(&r);
 if(e==-21&&op==FM_MOVE&&selected_dir){start_folder(dest,1);return;}
 status=e?error_text(e):op==FM_FOLDER?"Folder created":op==FM_RENAME?"Renamed":"Moved";
 if(!e){if(op!=FM_FOLDER)kg_note_move(selected,r.destination);refresh();}
}
static void command(unsigned cmd){
 if(preview){
  if(cmd==FM_UP||cmd==FM_BACK){if(cmd==FM_BACK||!parent_path(preview_folder))preview=0;status=0;focus.current=LIST_FOCUS;refresh();return;}
  if(cmd==FM_DELETE&&!loading&&preview_ok){
   copy_text(selected,preview_stage,sizeof selected);action=FM_RESCUE;target=(KwInput){0};
  }
  if(cmd==FM_OPEN&&!loading&&list.count){
   char relative[K32_FS_PATH];K32DirEntry *entry=&entries[list.selected];
   if(!join(relative,preview_folder,entry->name)){status=error_text(-22);return;}
   if(entry->attributes&16){copy_text(preview_folder,relative,sizeof preview_folder);status=0;focus.current=LIST_FOCUS;refresh();}
   else open_preview(entry,relative);
  }
  if(cmd!=FM_REFRESH&&(cmd<FM_NAME||cmd>FM_KIND))return;
 }
 if(cmd==FM_DRIVE){
  drive_panel=1;drive_focus=volume;return;
 }
 if(cmd==FM_RECOVERY){
  if(!volume){recovery_view=!recovery_view;trash=0;status=0;refresh();}
  return;
 }
 if(recovery_view&&cmd>=FM_FOLDER&&cmd<=FM_RENAME)return;
 if(volume&&(cmd==FM_TRASH||(cmd>=FM_FOLDER&&cmd<=FM_DELETE)))return;
 if(cmd==FM_UP){
  parent_path(folder);
  status=0;refresh();return;
 }
 if(cmd==FM_TRASH){recovery_view=0;trash=!trash;status=0;refresh();return;}
 if(cmd==FM_REFRESH){status=0;preview_ok=0;refresh();return;}
 if(cmd>=FM_NAME&&cmd<=FM_KIND){
  unsigned column=cmd-FM_NAME;descending=column==sort?!descending:0;sort=column;
  list.selected=list.top=0;sort_entries();return;
 }
 if(loading)return;
 if(cmd==FM_FOLDER){if(!trash){action=cmd;target=(KwInput){0};}return;}
 if(!list.count)return;
 K32DirEntry *d=&entries[list.selected];selected_dir=!!(d->attributes&16);
 if((d->attributes&6)&&!trash){if(cmd==FM_OPEN||cmd==FM_COPY||cmd==FM_MOVE||cmd==FM_RENAME||cmd==FM_DELETE)status="Read-only or protected item";return;}
 if(!join(selected,folder,d->name)){status=error_text(-22);return;}
 if(recovery_view){
  if(cmd==FM_OPEN||cmd==FM_DELETE){
   selected[k32_len(selected)-4]=0;copy_text(preview_stage,selected,sizeof preview_stage);
   preview=1;preview_folder[0]=0;preview_ok=0;status=0;focus.current=LIST_FOCUS;refresh();
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
static const unsigned controls[]={0,FM_NAME,FM_SIZE,FM_DATE,FM_KIND,FM_UP,FM_TRASH,FM_OPEN,FM_REFRESH,FM_FOLDER,FM_COPY,FM_MOVE,FM_RENAME,FM_DELETE,FM_BACK,FM_DRIVE,FM_RECOVERY,
 FM_BREADCRUMB,FM_BREADCRUMB+1,FM_BREADCRUMB+2,FM_BREADCRUMB+3};
#define FILE_LIST_Y 66
static KwRect list_rect(void){return (KwRect){8,FILE_LIST_Y,(int)kg_width()-38,(int)(list.rows*list.line_height)};}
int files_manager_event(const KgEvent *e){
 if(preview_fd){
  if(kw_progress_cancel(e,FM_CANCEL)||kw_button(FM_BACK,e)){close_preview();kg_notify(KG_NOTICE_CANCELLED);}
  return 1;
 }
 if(preview_type){
  unsigned key=e->type==KG_KEY?e->key:0;
  if(kw_button(FM_BACK,e)||(key==27&&!preview_search)||(key==8&&!preview_search)){close_preview();status=0;return 1;}
  if(preview_type==1&&!preview_fd){
   if(key==6||kw_button(FM_FIND,e)){preview_search^=1;return 1;}
   unsigned rows=(kg_height()-103)/(kg_large_text()?32:16);
   if(key==262||kw_button(FM_UP,e))preview_scroll(rows,1);
   else if(key==263||kw_button(FM_OPEN,e))preview_scroll(rows,0);
   else if(key==256||key==257)preview_scroll(1,key==256);
   else if(preview_search){
    if(key==27)preview_search=0;
    else if(key==13||kw_button(FM_FIND_NEXT,e))find_preview();
    else if(search_input(e)==2){preview_match=preview_bytes;status=0;}
   }else if(key==260)preview_top=0;
   else if(key==261&&preview_bytes)preview_top=row_at(preview_bytes-1,preview_columns());
  }
  return 1;
 }
 if(reader||tree.phase){
  if(kw_progress_cancel(e,FM_CANCEL))copy_finished(reader?finish_copy(-125,0):-125);
  return 1;
 }
 if(action){
  if(kw_button(FM_CANCEL,e)||(e->type==KG_KEY&&e->key==27)){action=0;return 1;}
  if(kw_button(FM_OK,e)||(e->type==KG_KEY&&e->key==13)){execute();return 1;}
  if(action!=FM_DELETE&&action!=FM_TRASH)kw_input(&target,e);
  return 1;
 }
 if(drive_panel){
  unsigned key=e->type==KG_KEY?e->key:0,id=e->control;
  if(key==27||key==4||kw_button(FM_DRIVE,e)){drive_panel=0;return 1;}
  if(key==256||key==257||key==9){
   unsigned step=key==256||(key==9&&(id&KG_MOD_SHIFT))?DRIVE_FILTER:1;
   do{drive_focus=(drive_focus+step)%(DRIVE_FILTER+1);}while(!drive_visible(drive_focus));
  }else if(key==13)choose_drive(drive_focus);
  else if(e->type==KG_MOUSE&&(e->key&1)){
   if(id>=FM_VOLUME&&id<FM_VOLUME+DRIVE_FILTER)choose_drive(id-FM_VOLUME);
   if(id==FM_SYSTEM){drive_focus=DRIVE_FILTER;choose_drive(drive_focus);}
  }
  return 1;
 }
 unsigned cmd=0;
 if(e->type==KG_KEY){
  if(e->key==27){if(!preview)return -1;command(FM_UP);return 1;}
  if(preview&&(e->key=='r'||e->key=='R'))cmd=FM_DELETE;
  if(preview&&e->key==8)cmd=FM_UP;
  if(e->key==4)cmd=FM_DRIVE;
  if(e->key==20&&!volume)cmd=FM_RECOVERY;
  if(e->key==13)cmd=focus.current?controls[focus.current]:FM_OPEN;
 }
 if(e->type==KG_MOUSE&&(e->key&1)){
  cmd=e->control;
  for(unsigned i=1;i<sizeof controls/sizeof *controls;i++)if(controls[i]==cmd){focus.current=i;break;}
  if(kw_inside(list_rect(),e->x,e->y))focus.current=LIST_FOCUS;
 }
 if(cmd==FM_BACK){if(!preview)return -1;command(cmd);return 1;}
 if(cmd>=FM_BREADCRUMB&&cmd<=FM_BREADCRUMB+path_depth(preview?preview_folder:folder)){go_to_crumb(cmd-FM_BREADCRUMB);return 1;}
 if(cmd>=FM_UP&&cmd<=FM_RECOVERY){command(cmd);return 1;}
 if(kw_focus(&focus,e)){
  while((preview&&(focus.current==TRASH_FOCUS||focus.current==DRIVE_FOCUS||focus.current==RECOVERY_FOCUS))||(focus.current==KIND_FOCUS&&kg_width()<468)||((trash||volume||recovery_view)&&focus.current>=FOLDER_FOCUS&&focus.current<DELETE_FOCUS+(volume!=0))||(volume&&(focus.current==TRASH_FOCUS||focus.current==RECOVERY_FOCUS))||focus.current>CRUMB_FOCUS+path_depth(preview?preview_folder:folder))kw_focus(&focus,e);
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
static void row(const K32DirEntry *d,unsigned y,unsigned w,int active,unsigned format){
 char line[48],num[12];
 if(kg_large_text()){
  if(d->attributes&16)k32_snprintf(line,sizeof line,"%s /",d->name);
  else k32_snprintf(line,sizeof line,"%s %u",d->name,d->bytes);
  kw_readable(12,y,line,w-42,active?0xffffff:0x20344a);return;
 }
 k32_memset(line,' ',sizeof line);line[47]=0;
 k32_memcpy(line,d->name,k32_len(d->name));
 if(d->attributes&16)k32_memcpy(line+13,KU("Folder"),k32_len(KU("Folder")));
 else {k32_snprintf(num,sizeof num,"%u",d->bytes);k32_memcpy(line+13,num,k32_len(num));}
 char *date=line+25;
 unsigned year=1980+(d->date>>9),month=(d->date>>5)&15,day=d->date&31;
 if((format&3)==2){
  digits(date,day,2);date[2]='/';digits(date+3,month,2);date[5]='/';digits(date+6,year,4);
 }else{
  digits(date,year,4);date[4]=date[7]=(format&3)?'/':'-';
  digits(date+5,month,2);digits(date+8,day,2);
 }
 if(w>=468){
  unsigned hour=d->time>>11;
  if(format&4){date[16]=hour>=12?'P':'A';hour%=12;if(!hour)hour=12;}
  digits(date+11,hour,2);date[13]=':';digits(date+14,(d->time>>5)&63,2);
  const char *ext=find_char(d->name,'.'),*kind=d->attributes&16?KU("Folder"):ext?ext+1:KU("File");
  k32_memcpy(line+42,kind,k32_len(kind)>5?5:k32_len(kind));
 }
 kw_text(12,y,line,(w-42)/8,active?0xffffff:0x20344a);
}
static void draw_preview(unsigned w,unsigned h){
 kw_readable(8,4,preview_name,w-16,0x20344a);
 int bottom=h-58;
 if(preview_fd){
  kw_draw_progress((KwRect){8,48,(int)w-16,36},KU("Reading TXT preview"),preview_loaded,preview_bytes,FM_CANCEL);
  return;
 }
 kg_rect(8,38,w-16,bottom-40,0xffffff);
 if(preview_type==2){
  unsigned width=preview_bitmap.width,height=preview_bitmap.height;
  unsigned available=w-24,vertical=bottom-46;
  if(width>available){height=height*available/width;width=available;}
  if(height>vertical){width=width*vertical/height;height=vertical;}
  if(width&&height)kg_bitmap_scaled(preview_surface,(w-width)/2,42,0,0,preview_bitmap.width,preview_bitmap.height,width,height);
 }else if(!preview_fd){
  unsigned cell=kg_large_text()?16:8,rows=(bottom-41)/(cell*2),cols=preview_columns();
  if(rows>17)rows=17;
  char line[48];unsigned at=preview_top;
  for(unsigned row=0;row<rows&&at<preview_bytes;row++){
   unsigned next=text_row(at,line,cols),y=40+row*cell*2;
   if(preview_match>=at&&preview_match<next)kg_rect(8,y,w-16,cell*2,0xeaf0f5);
   kw_readable(12,y,line,cols*cell,0x20344a);at=next;
  }
  if(preview_search){
   kw_draw_input((KwRect){8,bottom,(int)w-218,34},&query,1);
   button((KwRect){(int)w-206,bottom,102,34},FM_FIND_NEXT,"Next match",~0u,KW_PRIMARY);
  }else{
   button((KwRect){8,bottom,84,34},FM_UP,"Page up",~0u,KW_SECONDARY);
   button((KwRect){96,bottom,92,34},FM_OPEN,"Page down",~0u,KW_SECONDARY);
   button((KwRect){192,bottom,64,34},FM_FIND,"Find",~0u,KW_SECONDARY);
  }
 }
 button((KwRect){(int)w-96,bottom,88,34},FM_BACK,"Back",~0u,KW_SECONDARY);
 kw_label(8,h-16,preview_fd?"Reading text...":status?status:"Read-only preview",(w-16)/8,0x20344a);
}
static void draw_crumbs(unsigned w){
 const char *path=preview?preview_folder:folder;
 unsigned depth=path_depth(path),width=(w-16)/(depth+1);if(width>132)width=132;
 char root[13]="A:\\";
 if(volume)root[0]='A'+volume-1;
 if(preview){
  const char *leaf=preview_stage;
  for(const char *p=leaf;*p;p++)if(*p=='\\')leaf=p+1;
  k32_snprintf(root,sizeof root,"%s.TMP",leaf);
 }
 const char *label=preview||volume?root:root_name;
 for(unsigned i=0;i<=depth;i++){
  char name[13];
  if(i){unsigned n=0;while(*path&&*path!='\\'&&n<sizeof name-1)name[n++]=*path++;name[n]=0;if(*path=='\\')path++;label=name;}
  button((KwRect){8+i*width,0,width-3,28},FM_BREADCRUMB+i,label,CRUMB_FOCUS+i,i==depth?KW_PRIMARY:KW_SECONDARY);
 }
}
static void draw_drives(unsigned w){
 kg_label(8,8,"Drives",0x20344a);
 button((KwRect){(int)w-116,0,108,28},FM_DRIVE,"Back",~0u,KW_SECONDARY);
 kg_rect(8,30,164,136,0xdce7ef);
 unsigned y=32;
 for(unsigned id=0;id<=26;id++)if(drive_visible(id)){
  char name[3]={'A'+id-1,':',0};
  kw_draw_button((KwRect){12,y,156,32},FM_VOLUME+id,id?name:KU("Work"),volume==id?KW_PRIMARY:KW_SECONDARY,drive_focus==id?KW_FOCUSED:0);
  y+=33;
 }
 kw_draw_button((KwRect){12,y,156,32},FM_SYSTEM,KU(show_system?"Hide system files":"Show system files"),KW_SECONDARY,drive_focus==DRIVE_FILTER?KW_FOCUSED:0);
}
int files_manager_draw(void){
 unsigned w=kg_width(),h=kg_height();int bottom=(int)h-93;
 if(w<468&&focus.current==KIND_FOCUS)focus.current=DATE_FOCUS;
 kg_menu_enable(0);kg_begin();kg_rect(0,0,w,h,0xeaf0f5);
 if(reader||tree.phase){
  char label[44];
  unsigned done=copy_bytes-remaining,total=copy_bytes;
  if(tree.phase==1){
   done=tree.scan;total=0;
   k32_snprintf(label,sizeof label,"%s %u",KU("Scanning folder"),tree.count);
  }else if(tree.phase==2){
   unsigned fraction=reader&&copy_bytes?done*64u/copy_bytes:0;
   done=(tree.item-1)*64+fraction;total=(tree.count-1)*64;
   k32_snprintf(label,sizeof label,"%s %u/%u",KU("Copying folder"),tree.item-1,tree.count-1);
  }else k32_snprintf(label,sizeof label,"%s %u/%u KB",KU("Copying file"),done/1024,(total+1023)/1024);
  kw_draw_progress((KwRect){8,bottom,(int)w-16,36},label,done,total,FM_CANCEL);
 }
 else if(preview_type)draw_preview(w,h);
 else if(action)modal(w,h);
 else if(drive_panel)draw_drives(w);
 else {
  unsigned reserved=preview?0:volume?116:184;
  if(!volume&&!preview)button((KwRect){(int)w-184,0,62,28},FM_RECOVERY,recovery_view?"Work":"TMP",RECOVERY_FOCUS,KW_SECONDARY);
  if(!preview)button((KwRect){(int)w-116,0,108,28},FM_DRIVE,"Drives",DRIVE_FOCUS,KW_SECONDARY);
  draw_crumbs(w-reserved);
  int top=FILE_LIST_Y;
  button((KwRect){8,top-34,110,32},FM_NAME,"Name",NAME_FOCUS,KW_SECONDARY);
  button((KwRect){120,top-34,88,32},FM_SIZE,"Bytes",SIZE_FOCUS,KW_SECONDARY);
  button((KwRect){210,top-34,w>=468?136:(int)w-218,32},FM_DATE,"Updated",DATE_FOCUS,KW_SECONDARY);
  if(w>=468)button((KwRect){348,top-34,76,32},FM_KIND,"Type",KIND_FOCUS,KW_SECONDARY);
  list.line_height=kg_large_text()?32:16;
  list.rows=bottom>top+(int)list.line_height?(bottom-top)/list.line_height:1;if(list.rows>7)list.rows=7;
  kw_reveal(&list);
  KwRect area=list_rect();
  kg_rect(area.x,area.y,area.w,area.h,0xffffff);
  scroll=(KwScroll){.r={(int)w-28,top,20,area.h},.top=list.top,.rows=list.rows,.total=list.count,.dragging=scroll.dragging,.offset=scroll.offset};
  int format=kg_ui_format();
  for(unsigned i=0;i<list.rows&&list.top+i<list.count;i++){
   unsigned y=top+i*list.line_height,at=list.top+i;
   int active=list.selected==at;
   if(active)kg_rect(8,y,area.w,list.line_height,0x3270a2);
   row(&entries[at],y,w,active,format<0?0:(unsigned)format);
  }
  kw_draw_scroll(&scroll);
  {
   static const unsigned top_ids[]={FM_UP,FM_TRASH,FM_OPEN,FM_REFRESH,FM_BACK};
   static const unsigned low_ids[]={FM_FOLDER,FM_COPY,FM_MOVE,FM_RENAME,FM_DELETE};
   const char *top_labels[]={"Up",trash?"Work":"Trash","Open","Refresh","Back"};
   const char *low_labels[]={"New folder","Copy","Move","Rename",preview?"Recover":recovery_view?"Preview":trash?"Restore":"Delete"};
   unsigned width=(w-16)/5;
   for(unsigned i=0;i<5;i++){
    if((!preview||i!=1)&&(!volume||i!=1))button((KwRect){8+i*width,bottom,width-3,36},top_ids[i],top_labels[i],i==4?BACK_FOCUS:UP_FOCUS+i,KW_SECONDARY);
    if(!volume&&(!preview||preview_ok)&&((!trash&&!recovery_view)||i==4))button((KwRect){8+i*width,bottom+38,width-3,36},low_ids[i],i==0&&width<87?"Mkdir":low_labels[i],FOLDER_FOCUS+i,i==4&&!trash?KW_DANGER:KW_SECONDARY);
   }
  }
  kw_label(8,h-16,loading&&!tree.phase?"Loading...":status?status:volume?"Drive root / read only":preview?count?"Read-only preview / TMP may be incomplete":"Empty TMP / partial copy":recovery_view?"TMP folders / may be incomplete":trash?"Trash / Select an item to restore":"Work / Copy and Move use Work paths",(w-16)/8,0x20344a);
 }
 return kg_present();
}
