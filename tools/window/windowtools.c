#include "windowtools.h"
#include "hardware.h"
#include "clipboard.h"
#include "../editor/editor.h"
static Editor editor;
static int initialized[WT_COUNT],changed[WT_COUNT],prompt,request,pending;
static char paths[WT_COUNT][FD_PATH],notice[80];
static int cx,cy,cw,ch,ct;
void control_open(void);void control_draw(void);int control_key(int key);
static void say(const char *s){fd_text(notice,s,sizeof notice);}
static void label(int x,int y,const char *s,int width,int fg,int bg){
 for(int i=0;i<width&&s[i];i++)gfx_cell(x+i,y,s[i],fg,bg,0);
}
void control_line(int row,const char *s,int hi){
 row-=ct;if(row>=0&&row<ch)label(cx,cy+row,s,cw,hi?WHITE:INK,hi?ACCENT:PAPER);
}
int wt_open(int tool,const char *path){
 if(initialized[tool]&&(!path||!*path))return 1;
 if(changed[tool]){say("Save or reload changes before opening another file");return 0;}
 if(tool==WT_EDITOR){
  if(!initialized[tool])ed_init(&editor);
  if(path&&*path){int e=ed_load(&editor,path);if(e){say("Cannot open text (8 KiB limit / Shift_JIS required)");return 0;}}
 }else if(tool==WT_SETTINGS){control_open();}
 if(path&&*path)fd_text(paths[tool],path,FD_PATH);
 initialized[tool]=1;say("Window app ready");return 1;
}
static void edit_save(void){
 if(!editor.path[0]){prompt=1;request=WT_EDITOR;paths[0][0]=0;say("Save as: type a DOS path / Enter / Esc");return;}
 int e=ed_save(&editor);changed[0]=editor.dirty;say(e?"Save failed; inspect recovery / disk space":"Saved text (Shift_JIS / CRLF)");
}
void wt_key(int tool,int key){
 if(prompt&&request==tool){
  if(prompt==2){if(key==27)prompt=0;else if(key==13){prompt=0;if(editor.path[0])ed_load(&editor,editor.path);else ed_init(&editor);changed[0]=editor.dirty;}return;}
  int n=fd_len(paths[tool]);
  if(key==27){prompt=0;say("Path cancelled");return;}
  if(key==8&&n)paths[tool][--n]=0;
  else if(key>=32&&key<127&&n<FD_PATH-1){paths[tool][n]=key;paths[tool][n+1]=0;}
  else if(key==13){prompt=0;if(tool==WT_EDITOR){fd_text(editor.path,paths[tool],FD_PATH);edit_save();}}
  return;
 }
 if(tool==WT_SETTINGS){int error=control_key(key);if((key=='s'||key=='S'||key=='r'||key=='R')&&!error)changed[tool]=0;else if(key=='x'||key=='X'||key=='e'||key=='E'||key=='f'||key=='F'||key=='+'||key=='-'||key=='['||key==']')changed[tool]=1;return;}
 if(tool==WT_EDITOR){
  if(key==18){prompt=2;request=tool;say("Discard edits? Enter confirms / Esc cancels");return;}
  if(key==23){prompt=1;request=tool;fd_text(paths[0],editor.path,FD_PATH);say("Save as: type a DOS path / Enter / Esc");return;}
  if(key==19){edit_save();return;}
  if(pending){u8 pair[2]={pending,key};pending=0;if(key<256&&sjis_trail(key))ed_insert(&editor,pair,2);changed[0]=editor.dirty;return;}
  if(key>=0&&key<256&&sjis_lead(key)){pending=key;return;}
  if(key==26)ed_undo(&editor);
  else if(key==KEY_LEFT)editor.cursor=ed_prev(&editor,editor.cursor);
  else if(key==KEY_RIGHT)editor.cursor=ed_next(&editor,editor.cursor);
  else if(key==KEY_UP)ed_vertical(&editor,-1);
  else if(key==KEY_DOWN)ed_vertical(&editor,1);
  else if(key==KEY_HOME)editor.cursor=ed_start(&editor,editor.cursor);
  else if(key==KEY_END)editor.cursor=ed_end(&editor,editor.cursor);
  else if(key==8)ed_delete(&editor,1);
  else if(key==KEY_DELETE)ed_delete(&editor,0);
  else if(key==3)clip_copy((char *)editor.data,editor.length);
  else if(key==22)ed_insert(&editor,(const u8 *)clipboard.text,clipboard.length);
  else if(key==13||key==9||(key>=32&&key<=255)){u8 c=key==13?'\n':key;ed_insert(&editor,&c,1);}
  changed[0]=editor.dirty;return;
 }
}

__attribute__((noinline)) void wt_draw(int tool,int x,int y,int w,int h,int top,int active){
 cx=x;cy=y;cw=w;ch=h-3;ct=top;
 if(tool==WT_SETTINGS)control_draw();
 else if(tool==WT_EDITOR){
  int at=0,row=0,col=0;
  for(;at<editor.length;at++){
   if(editor.data[at]=='\n'){row++;col=0;continue;}
   if(sjis_lead(editor.data[at])&&at+1<editor.length){if(col+2>w){row++;col=0;}if(row>=top&&row<top+ch)gfx_pair(x+col,y+row-top,editor.data[at],editor.data[at+1]);at++;col++;}
   else if(row>=top&&row<top+ch&&col<w)gfx_cell(x+col,y+row-top,editor.data[at],INK,PAPER,0);
   if(++col==w){row++;col=0;}
  }
  if(active){int r=0,c=0;for(int i=0;i<editor.cursor;i++){if(editor.data[i]=='\n'){r++;c=0;}else if(++c==w){r++;c=0;}}
   if(r>=top&&r<top+ch)gfx_cell(x+c,y+r-top,'_',WHITE,SELECTED,1);}
  label(x,y+ch,editor.path[0]?editor.path:"[New file]",w,INK,PAPER);
 }

 if(prompt==1&&request==tool)label(x,y+h-2,paths[tool],w,INK,SURFACE);
 else label(x,y+h-2,notice,w,MUTED,PAPER);
 label(x,y+h-1,tool==WT_SETTINGS?"S Save / R Reload / +/- FILES":"^S Save ^W SaveAs ^Z Undo ^R Reload",w,INK,SURFACE);
 if(changed[tool])gfx_cell(x+w-1,y+h-1,'*',WHITE,SELECTED,1);
}
int wt_click(int tool,int x,int y,int w,int h,int top,int px,int py){
 px-=x*8;py-=y*16;
 if(px<0||px>=w*8||py<0||py>=h*16)return 0;
 if(py/16==h-1){wt_key(tool,tool==WT_SETTINGS?'s':19);return 1;}
 if(tool==WT_EDITOR&&py<(h-3)*16){
  int target=(top+py/16)*w+px/8,at=0,pos=0;
  while(at<editor.length&&pos<target){if(editor.data[at]=='\n')pos=(pos/w+1)*w;else pos++;at=ed_next(&editor,at);}editor.cursor=at;return 1;
 }else if(tool==WT_SETTINGS){int row=top+py/16;int key=row==5?'x':row==6?'e':row==9?'+':row==12?'f':row>=19?'s':0;if(key)wt_key(tool,key);return 1;}
 return 0;
}
int wt_close(int tool){if(prompt&&request==tool){prompt=0;return 0;}if(changed[tool]){say("Unsaved work: save before closing this window");return 0;}return 1;}
int wt_clean(void){for(int i=0;i<WT_COUNT;i++)if(changed[i])return 0;return 1;}
void wt_finish(void){}
