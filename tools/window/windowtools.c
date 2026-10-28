#include "windowtools.h"
#include "bitmap.h"
#include "clipboard.h"
#include "../editor/editor.h"
static Editor editor;
static Bitmap pictures[2],undo,copied;
static u8 image_row[IMAGE_WIDTH];
static u16 image_pixels[IMAGE_WIDTH],image_palette[256];
static int palette_ready;
static int pan[2],color=196,image_tool,stroke,sx,sy,last_x,last_y,selection,x1,y1,x2,y2;
static const char *image_tools[]={"Pencil","Line","Rectangle","Fill","Pick","Erase","Select"};
static int inside(const Bitmap *b,int x,int y){return x>=0&&y>=0&&x<b->width&&y<b->height;}
static void publish(const Bitmap *b){clip_image(b->segment,b->width,b->height);}
static void pixel(Bitmap *b,int x,int y,int c){if(inside(b,x,y))mem_put(b->segment,y*(u32)b->width+x,c);}
static void stroke_line(Bitmap *b,int ax,int ay,int bx,int by,int c){
 int dx=bx>ax?bx-ax:ax-bx,dy=by>ay?ay-by:by-ay;
 int xx=ax<bx?1:-1,yy=ay<by?1:-1,error=dx+dy;
 for(;;){pixel(b,ax,ay,c);if(ax==bx&&ay==by)break;int e=error*2;if(e>=dy){error+=dy;ax+=xx;}if(e<=dx){error+=dx;ay+=yy;}}
}
static int checkpoint(void){return bitmap_clone(&undo,&pictures[1]);}
static void fill_image(Bitmap *b,int x,int y){
 int old=mem_get(b->segment,y*(u32)b->width+x);if(old==color)return;
 unsigned queue=mem_alloc(b->width*(u32)b->height*2);if(!queue)return;
 u32 head=0,tail=1;unsigned n=y*b->width+x;
 mem_put(queue,0,n);mem_put(queue,1,n>>8);pixel(b,x,y,color);
 while(head<tail){n=mem_get(queue,head*2)|(unsigned)mem_get(queue,head*2+1)<<8;head++;
  for(int d=0;d<4;d++){int xx=n%b->width+(d==0?-1:d==1?1:0),yy=n/b->width+(d==2?-1:d==3?1:0);
   if(inside(b,xx,yy)&&mem_get(b->segment,yy*(u32)b->width+xx)==old){unsigned at=yy*b->width+xx;pixel(b,xx,yy,color);mem_put(queue,tail*2,at);mem_put(queue,tail*2+1,at>>8);tail++;}}
  if(!(head&255))input_poll();
 }
 mem_free(queue);
}

static int initialized[WT_COUNT],changed[WT_COUNT],prompt,request,pending;
static char paths[WT_COUNT][FD_PATH],owned[WT_COUNT][FD_PATH],notice[80];
static int overwrite;
static int cx,cy,cw,ch,ct;
const char *control_status(void);void control_open(void);void control_draw(void);int control_key(int key);
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
 else {Bitmap *b=&pictures[tool-WT_VIEW];
  const char *target=path;
  if(tool==WT_VIEW&&(!path||!*path))target="COLORS.BMP";
  if(target&&*target){if(bitmap_load(b,target)){say("Cannot open BMP; current image retained");return 0;}fd_text(paths[tool],target,FD_PATH);}
  else if(!bitmap_new(b,224,144,WHITE)){say("Not enough image memory");return 0;}
  publish(b);
 }
 if(path&&*path)fd_text(paths[tool],path,FD_PATH);
 initialized[tool]=1;fd_text(owned[tool],tool==WT_EDITOR?editor.path:paths[tool],FD_PATH);say("Window app ready");return 1;
}
static void image_save(int tool);
static void edit_save(void){
 if(!editor.path[0]){prompt=1;request=WT_EDITOR;paths[0][0]=0;say("Save as: type a DOS path / Enter / Esc");return;}
 int e=ed_save(&editor);if(!e)fd_text(owned[0],editor.path,FD_PATH);changed[0]=editor.dirty;say(e?"Save failed; inspect recovery / disk space":"Saved text (Shift_JIS / CRLF)");
}
static void text_position(int limit,int width,int *row,int *col){
 *row=*col=0;
 for(int at=0;at<limit;at=ed_next(&editor,at)){
  if(editor.data[at]=='\n'){(*row)++;*col=0;continue;}
  int cells=ed_width(&editor,at,*col);
  if(*col+cells>width){(*row)++;*col=0;cells=ed_width(&editor,at,0);}
  *col+=cells;if(*col>=width){(*row)++;*col=0;}
 }
}
void wt_key(int tool,int key){
 if(prompt&&request==tool){
  if(prompt==3){
   if(key==27){prompt=0;fd_text(paths[tool],owned[tool],FD_PATH);say("Replacement cancelled");}
   else if(key==13){prompt=0;overwrite=1;if(tool==WT_EDITOR){fd_text(editor.path,paths[tool],FD_PATH);edit_save();}else image_save(tool);}
   return;
  }
  if(prompt==2){if(key==27)prompt=0;else if(key==13){prompt=0;
   if(tool==WT_EDITOR){if(editor.path[0])ed_load(&editor,editor.path);else ed_init(&editor);changed[0]=editor.dirty;}
   else {changed[tool]=0;initialized[tool]=0;if(!wt_open(tool,paths[tool])){initialized[tool]=1;changed[tool]=1;}}
  }return;}
  int n=fd_len(paths[tool]);
  if(key==27){prompt=0;say("Path cancelled");return;}
  if(key==8&&n)paths[tool][--n]=0;
  else if(key>=32&&key<127&&n<FD_PATH-1){paths[tool][n]=key;paths[tool][n+1]=0;}
  else if(key==13){Entry entry;
   if(p_stat(paths[tool],&entry)==0&&fd_cmp(paths[tool],owned[tool])){prompt=3;say("Replace existing file? Enter confirms / Esc cancels");return;}
   prompt=0;if(tool==WT_EDITOR){fd_text(editor.path,paths[tool],FD_PATH);edit_save();}else image_save(tool);
  }
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
  else if(key==3){if(editor.length>=CLIP_CAPACITY)say("Text exceeds the 1024-byte clipboard limit");else clip_copy((char *)editor.data,editor.length);}
  else if(key==22)ed_insert(&editor,(const u8 *)clipboard.text,clipboard.length);
  else if(key==13||key==9||(key>=32&&key<=255)){u8 c=key==13?'\n':key;ed_insert(&editor,&c,1);}
  changed[0]=editor.dirty;return;
 }
 if(key==3||key==24){
  Bitmap *b=&pictures[tool-WT_VIEW];int ax=tool==WT_PAINT&&selection?x1:0,ay=tool==WT_PAINT&&selection?y1:0;
  int ww=tool==WT_PAINT&&selection?x2-x1+1:b->width,hh=tool==WT_PAINT&&selection?y2-y1+1:b->height;
  if(!bitmap_new(&copied,ww,hh,WHITE)){say("Not enough clipboard memory");return;}
  for(int y=0;y<hh;y++)mem_copy(copied.segment,y*(u32)ww,b->segment,(ay+y)*(u32)b->width+ax,ww);
  publish(&copied);say("Copied image");
  if(key==24&&tool==WT_PAINT&&checkpoint()){for(int y=0;y<hh;y++)mem_fill(b->segment,(ay+y)*(u32)b->width+ax,ww,WHITE);changed[tool]=1;}
  return;
 }
 if(key==KEY_LEFT&&pan[tool-WT_VIEW]>=16)pan[tool-WT_VIEW]-=16;
 if(key==KEY_RIGHT)pan[tool-WT_VIEW]+=16;
 if(tool==WT_VIEW)return;
 Bitmap *b=&pictures[1];
 if(key==19)image_save(tool);
 else if(key==23){prompt=1;request=tool;say("Save as: type BMP path / Enter / Esc");}
 else if(key==18){prompt=2;request=tool;say("Discard image edits? Enter confirms / Esc cancels");}
 else if(key==26&&undo.segment){Bitmap old=*b;*b=undo;undo=old;changed[tool]=1;publish(b);}
 else if(key==22&&copied.segment&&checkpoint()){
  int ww=copied.width<b->width?copied.width:b->width,hh=copied.height<b->height?copied.height:b->height;
  for(int y=0;y<hh;y++)mem_copy(b->segment,y*(u32)b->width,copied.segment,y*(u32)copied.width,ww);
  changed[tool]=1;publish(b);say("Pasted image");
 }
 else if(key=='+'||key=='=')color=(color+1)&255;
 else if(key=='-')color=(color+255)&255;
 else {if(key>='A'&&key<='Z')key+=32;const char *keys="plrfies";for(int i=0;keys[i];i++)if(key==keys[i]){image_tool=i;selection=0;}}

}

__attribute__((noinline)) void wt_draw(int tool,int x,int y,int w,int h,int top,int active){
 cx=x;cy=y;cw=w;ch=h-3;ct=top;
 if(tool==WT_SETTINGS)control_draw();
 else if(tool==WT_EDITOR){
  int row=0,col=0;
  for(int at=0;at<editor.length;at=ed_next(&editor,at)){
   if(editor.data[at]=='\n'){row++;col=0;continue;}
   int cells=ed_width(&editor,at,col),next=ed_next(&editor,at);
   if(col+cells>w){row++;col=0;cells=ed_width(&editor,at,0);}
   if(row>=top&&row<top+ch){
    if(next-at==2)gfx_pair(x+col,y+row-top,editor.data[at],editor.data[at+1]);
    else if(editor.data[at]!='\t')gfx_cell(x+col,y+row-top,editor.data[at],INK,PAPER,0);
   }
   col+=cells;if(col>=w){row++;col=0;}
  }
  if(active){text_position(editor.cursor,w,&row,&col);if(row>=top&&row<top+ch)gfx_cell(x+col,y+row-top,'_',WHITE,SELECTED,1);}
  label(x,y+ch,editor.path[0]?editor.path:"[New file]",w,INK,PAPER);
 }else {
  Bitmap *b=&pictures[tool-WT_VIEW];int width=w*8;if(width>b->width)width=b->width;
  int height=(ch-1)*16,remaining=b->height-top*16;if(height>remaining)height=remaining;
  int *left=&pan[tool-WT_VIEW];if(*left>b->width-width)*left=b->width-width;
  label(x,y,tool==WT_VIEW?"BMP viewer":image_tools[image_tool],w,INK,SURFACE);
  unsigned ds;__asm__ volatile("movw %%ds,%w0":"=r"(ds));
  int colors=gfx_colors();
  if(colors>256&&!palette_ready){for(int i=0;i<256;i++){u32 rgb=gfx_rgb(i);image_palette[i]=((rgb>>19)<<11)|(((rgb>>10)&63)<<5)|((rgb>>3)&31);}palette_ready=1;}
  for(int yy=0;yy<height;yy++){
   mem_copy(ds,(unsigned)image_row,b->segment,(yy+top*16)*(u32)b->width+*left,width);
   if(colors>256){for(int xx=0;xx<width;xx++)image_pixels[xx]=image_palette[image_row[xx]];gfx_write_span(x*8,(y+1)*16+yy,ds,(unsigned)image_pixels,width);}
   else if(colors==256)gfx_write_span(x*8,(y+1)*16+yy,ds,(unsigned)image_row,width);
   else for(int xx=0;xx<width;xx++)gfx_pixel(x*8+xx,(y+1)*16+yy,image_row[xx]);
  }
  if(tool==WT_PAINT){for(int i=0;i<16&&i*2+1<w;i++)gfx_rect((x+i*2)*8,(y+ch)*16,14,14,(color&240)+i);}
  else label(x,y+ch,paths[tool],w,INK,PAPER);
 }

 if(prompt==1&&request==tool){label(x,y+h-2,"Save as:",w,INK,SURFACE);label(x+9,y+h-2,paths[tool],w-9,INK,SURFACE);}
 else label(x,y+h-2,tool==WT_SETTINGS?control_status():notice,w,MUTED,PAPER);
 label(x,y+h-1,tool==WT_SETTINGS?"S Save / R Reload / +/- FILES":tool==WT_VIEW?"^C Copy / arrows pan / PgUp PgDn":tool==WT_PAINT?"P L R F I E S / ^S Save ^Z Undo ^C ^V":"^S Save ^W SaveAs ^Z Undo ^R Reload",w,INK,SURFACE);
 if(changed[tool])gfx_cell(x+w-1,y+h-1,'*',WHITE,SELECTED,1);
}
int wt_click(int tool,int x,int y,int w,int h,int top,int px,int py){
 px-=x*8;py-=y*16;
 if(px<0||px>=w*8||py<0||py>=h*16)return 0;
 if(py/16==h-1){wt_key(tool,tool==WT_SETTINGS?'s':19);return 1;}
 if(tool==WT_PAINT&&py/16==h-3){color=(color&240)+px/16;return 1;}
 if(tool==WT_PAINT&&py>=16&&py<(h-3)*16)return wt_pointer(tool,x,y,w,h,top,px+x*8,py+y*16,1);
 if(tool==WT_EDITOR&&py<(h-3)*16){
  int at=0,row=0,col=0,target_row=top+py/16,target_col=px/8;
  while(at<editor.length){
   if(row>target_row||(row==target_row&&col>=target_col))break;
   if(editor.data[at]=='\n'){row++;col=0;}
   else {int cells=ed_width(&editor,at,col);if(col+cells>w){row++;col=0;cells=ed_width(&editor,at,0);}col+=cells;if(col>=w){row++;col=0;}}
   at=ed_next(&editor,at);
  }
  editor.cursor=at;return 1;
 }else if(tool==WT_SETTINGS){int row=top+py/16;int key=row==5?'x':row==6?'e':row==9?'+':row==12?'f':row>=19?'s':0;if(key)wt_key(tool,key);return 1;}
 return 0;
}
int wt_close(int tool){
 if(prompt&&request==tool){prompt=0;return 0;}
 if(changed[tool]){say("Unsaved work: save or Ctrl+R reload before closing");return 0;}
 if(tool>=WT_VIEW){
  Bitmap *b=&pictures[tool-WT_VIEW];
  if(clipboard.segment==b->segment){if(copied.segment)publish(&copied);else clip_image(0,0,0);}
  bitmap_free(b);if(tool==WT_PAINT){bitmap_free(&undo);stroke=selection=0;}
 }
 initialized[tool]=0;return 1;
}
int wt_clean(void){for(int i=0;i<WT_COUNT;i++)if(changed[i])return 0;return 1;}
void wt_finish(void){clip_image(0,0,0);bitmap_free(&pictures[0]);bitmap_free(&pictures[1]);bitmap_free(&undo);bitmap_free(&copied);}
static void image_save(int tool){
 if(!paths[tool][0]){prompt=1;request=tool;fd_text(paths[tool],"DRAW.BMP",FD_PATH);say("Save as: type BMP path / Enter / Esc");return;}
 Entry entry;
 if(!overwrite&&p_stat(paths[tool],&entry)==0&&fd_cmp(paths[tool],owned[tool])){prompt=3;request=tool;say("Replace existing file? Enter confirms / Esc cancels");return;}
 overwrite=0;
 int error=bitmap_save(&pictures[tool-WT_VIEW],paths[tool]);
 if(error>=0){changed[tool]=0;fd_text(owned[tool],paths[tool],FD_PATH);}
 say(error<0?"Save failed; image retained (check KDIMG.TMP / BAK)":"Saved BMP");
}
int wt_pointer(int tool,int x,int y,int w,int h,int top,int px,int py,int buttons){
 if(tool!=WT_PAINT)return 0;
 Bitmap *b=&pictures[1];px=px-x*8+pan[1];py=py-(y+1)*16+top*16;
 int valid=inside(b,px,py)&&py-top*16>=0&&py-top*16<(h-4)*16&&px-pan[1]>=0&&px-pan[1]<w*8;
 if(!(buttons&1)){
  if(!stroke)return 0;
  stroke=0;
  if(!valid)return 0;
  if(image_tool==1){stroke_line(b,sx,sy,px,py,color);changed[tool]=1;}
  if(image_tool==2){stroke_line(b,sx,sy,px,sy,color);stroke_line(b,sx,py,px,py,color);stroke_line(b,sx,sy,sx,py,color);stroke_line(b,px,sy,px,py,color);changed[tool]=1;}
  if(image_tool==6){x1=sx<px?sx:px;x2=sx>px?sx:px;y1=sy<py?sy:py;y2=sy>py?sy:py;selection=1;say("Selected image region; Ctrl+C/X copies/cuts");}
  publish(b);return 1;
 }
 if(!valid)return 0;
 if(!stroke){
  if(image_tool==4){color=mem_get(b->segment,py*(u32)b->width+px);return 1;}
  if(image_tool!=6&&!checkpoint()){say("Cannot reserve undo; image retained");return 0;}
  stroke=1;sx=last_x=px;sy=last_y=py;
  if(image_tool==3){fill_image(b,px,py);changed[tool]=1;publish(b);return 1;}
 }
 if(image_tool==0||image_tool==5){stroke_line(b,last_x,last_y,px,py,image_tool==5?WHITE:color);last_x=px;last_y=py;changed[tool]=1;publish(b);return 1;}
 return 0;
}
int wt_rows(int tool,int width){
 if(tool==WT_SETTINGS)return 23;
 if(tool>=WT_VIEW)return (pictures[tool-WT_VIEW].height+15)/16+1;
 int row,col;text_position(editor.length,width,&row,&col);return row+1;
}
int wt_scroll(int tool,int width,int height,int top,int follow){
 int rows=height-3,total=wt_rows(tool,width);if(rows<1)rows=1;
 if(tool==WT_EDITOR&&follow){int row,col;text_position(editor.cursor,width,&row,&col);if(row<top)top=row;if(row>=top+rows)top=row-rows+1;}
 if(top>total-rows)top=total-rows;
 if(top<0)top=0;
 return top;
}
