#include "clipboard.h"
#include "utilities.h"
#include "scroll.h"
#include "tasks.h"
#include "k386_client.h"
#include "commands.h"
#include "../k386/app.h"

/* Window positions use 8 x 16 cells. Four graphics planes hold both the
 * 16-color scene and CG-ROM glyphs; hidden text VRAM mirrors the labels. */
enum { LAUNCHER, FILES, MEMO, HELP, ABOUT, CALCULATOR, CLOCK, TASKS, DOS1, DOS2, WINDOWS };
enum { LAUNCH_APPS = 9 };
typedef struct { int x, y, w, h, open, hidden, selected, ox, oy, top; } Window;
static Window windows[WINDOWS];
/* Only geometry is restored: task state, text and scroll position stay live. */
static short restored[WINDOWS][6];
/* 0 normal, 1 maximized, 2 left, 3 right; all placements retain one normal rect. */
static unsigned char placement[WINDOWS];
static unsigned desktop_restore;
static int desktop_active;
static int panel_open, panel_selected, panel_top, panel_all;
static unsigned panel_count, panel_matches[DESK_COMMANDS];
static char panel_query[32];
static Calculator calculator;
static DeskClock desk_clock;
static int icon_view = 1, file_icons = 1, twelve_hour, scene_dirty, enhanced;
static char dos_path[2][128];
static unsigned dos_edit[2];
static int dos_follow[2]={1,1};
static int dos_horizontal[2];
static u32 dos_tick;
static u32 resource_tick;
static unsigned event_age;
static int scrolling = -1, scroll_grab;
static u32 clock_tick;
/* Union of window damage, kept separate from the status/taskbar rows. */
static int damage_left=640, damage_top=400, damage_right, damage_bottom;
static void damage(int x,int y,int w,int h) {
    if(x<damage_left)damage_left=x;
    if(y<damage_top)damage_top=y;
    if(x+w>damage_right)damage_right=x+w;
    if(y+h>damage_bottom)damage_bottom=y+h;
}
static void invalidate(int id) {
    if(id<0)return;
    Window *w=&windows[id];
    if(w->open&&!w->hidden)damage(w->x*8+w->ox,w->y*16+w->oy,w->w*8+5,w->h*16+5);
}
static void present(void) {
    if(damage_right>640)damage_right=640;
    if(damage_bottom>400)damage_bottom=400;
    if(damage_right>damage_left&&damage_bottom>damage_top)
        gfx_present(damage_left,damage_top,damage_right-damage_left,damage_bottom-damage_top);
    gfx_present(0,368,640,32);
    damage_left=640;damage_top=400;damage_right=damage_bottom=0;
}
static int order[WINDOWS], active = -1, mode, dirty, confirm, exiting;
static char home[FD_PATH], folder[FD_PATH], note_path[FD_PATH];
#define MEMO_CAPACITY 1024
#define MEMO_OFFSET (128*sizeof(Entry))
static char status[80] = "Ready - Ctrl+K finds apps / Ctrl+D shows desktop";
static int used, count, truncated, list_error, memo_blocked, select_all;
static unsigned entries_segment, data_segment;
/* Keep editable text in the existing DOS-owned data allocation, outside the
 * near data/stack segment. Directory refreshes never touch this tail. */
static char memo_get(int at){return mem_get(entries_segment,MEMO_OFFSET+at);}
static void memo_put(int at,char ch){mem_put(entries_segment,MEMO_OFFSET+at,ch);}
static const char *titles[] = {"Launcher", "Files", "Memo", "Help", "About", "Calculator", "Clock", "Tasks", "DOS 1", "DOS 2"};
static const char *apps[] = {"FD98.COM", "EDIT98.COM", "CPANEL.COM", "RBASIC.COM", "COMMAND.COM", "KVIEW.COM", "KPAINT.COM"};
static const char *labels[] = {"File manager", "Text editor", "System settings", "BASIC", "DOS shell (EXIT returns)", "Image viewer", "Image editor", "Calculator", "Clock"};
static const char *icon_labels[] = {"File manager", "Text editor", "Settings", "BASIC", "DOS shell", "Image viewer", "Image editor", "Calculator", "Clock"};
static const char *task_labels[] = {"Apps", "Files", "Memo", "Help", "About", "Calc", "Clock", "Tasks", "DOS 1", "DOS 2"};

static void label(int x, int y, const char *s, int width, int fg, int bg, int selected) {
    for (int i = 0; i < width && s[i]; i++) gfx_cell(x + i, y, s[i], fg, bg, selected);
}
static void text(int x, int y, const char *s, int width, int selected) {
    label(x, y, s, width, selected ? WHITE : INK, selected ? SELECTED : PAPER, selected);
}
static void line(Window *w, int row, const char *s, int selected) {
    if (row < w->h - 5) text(w->x + 2, w->y + 3 + row, s, w->w - 4, selected);
}
static void draw_resources(Window *w){
    if(!k386_resources_available())return;
    if(w->selected<DOS1){line(w,10,"Select DOS 1 or DOS 2 for resource controls",0);return;}
    if(k386_command(K386_DESCRIBE,w->selected-DOS1,event_age,0))return;
    char row[72];for(unsigned i=0;i<6;i++){k386_report_line(i,row);line(w,10+i,row,0);}
}
static void button(int x, int y, int width, const char *caption, int pressed) {
    int bg = pressed ? ACCENT : BUTTON;
    gfx_rect(x * 8, y * 16, width * 8, 16, bg);
    label(x + 1, y, caption, width - 2, pressed ? WHITE : INK, bg, pressed);
    gfx_rect(x * 8, y * 16, width * 8, 1, pressed?ACCENT:WHITE);
    gfx_rect(x * 8, y * 16 + 15, width * 8, 1, pressed?GOLD:INACTIVE);
    gfx_rect((x + width) * 8 - 1, y * 16+1, 1, 14, SURFACE);
}
static void message(const char *s) { fd_text(status, s, sizeof status); }
static void refresh(void) {
    Entry entries[128];
    count = p_list(folder, entries, 128, &truncated);
    if(count>0)mem_copy(entries_segment,0,data_segment,(unsigned)entries,count*sizeof(Entry));
    list_error = count < 0;
    if (list_error) count = 0;
    if (windows[FILES].selected >= count) windows[FILES].selected = count ? count - 1 : 0;
}
static void file_entry(int index,Entry *e) {
    mem_copy(data_segment,(unsigned)e,entries_segment,index*sizeof(Entry),sizeof(Entry));
}
static void focus(int id) {
    if(active!=id || !windows[id].open || windows[id].hidden)scene_dirty=1;
    invalidate(active);
    int at = 0;
    while (order[at] != id) at++;
    for (; at < WINDOWS - 1; at++) order[at] = order[at + 1];
    order[WINDOWS - 1] = id;
    if(!windows[id].open){
        task_start(id);
        if(id>=DOS1&&enhanced&&(k386_word(id-DOS1,0)==K386_ENDED||k386_word(id-DOS1,0)>=K386_FAULTED)){
            dos_follow[id-DOS1]=1;
            if(k386_command(K386_CREATE,id-DOS1,0,"KSH.EXE"))message("Cannot start KSH.EXE; use Run to choose a program");
        }
    }
    windows[id].open = 1; windows[id].hidden = 0;
    active = id; mode = 0; invalidate(id);
}
static void next(void) {
    int start = active;
    for (int i = 1; i <= WINDOWS; i++) {
        int id = (start + i + WINDOWS) % WINDOWS;
        if (windows[id].open) { focus(id); return; }
    }
    active = -1; mode = 0;
}
static void place_window(int kind) {
    if(active<0){message("No active window to arrange");return;}
    Window *w=&windows[active];short *r=restored[active];
    if(!kind&&!placement[active]){message("Window already restored");return;}
    invalidate(active);scene_dirty=1;mode=0;
    if(!kind) {
        w->x=r[0];w->y=r[1];w->w=r[2];w->h=r[3];w->ox=r[4];w->oy=r[5];
        message("Window restored");
    } else {
        if(!placement[active]) {
            r[0]=w->x;r[1]=w->y;r[2]=w->w;r[3]=w->h;r[4]=w->ox;r[5]=w->oy;
        }
        w->x=kind==3?40:0;w->y=1;w->w=kind==1?79:kind==2?40:39;
        w->h=22;w->ox=w->oy=0;
        message(kind==1?"Window maximized; Ctrl+G or double-click title restores":
            kind==2?"Window tiled left; Find apps / Restore window undoes layout":
                    "Window tiled right; Find apps / Restore window undoes layout");
    }
    placement[active]=kind;invalidate(active);
}
static void maximize(void) { place_window(placement[active]==1?0:1); }
static void show_desktop(void) {
    scene_dirty=1;
    if(desktop_restore) {
        unsigned visible=desktop_restore;desktop_restore=0;
        int wanted=-1;
        for(int id=0;id<WINDOWS;id++)if((visible&(1u<<id))&&windows[id].open) {
            windows[id].hidden=0;invalidate(id);wanted=id;
        }
        if(desktop_active>=0&&windows[desktop_active].open&&(visible&(1u<<desktop_active)))wanted=desktop_active;
        if(wanted>=0)focus(wanted);
        message("Desktop windows restored");
    } else {
        desktop_active=active;
        for(int id=0;id<WINDOWS;id++)if(windows[id].open&&!windows[id].hidden) {
            invalidate(id);desktop_restore|=1u<<id;windows[id].hidden=1;
        }
        active=-1;mode=0;message("Desktop shown; Ctrl+D or Desk restores your windows");
    }
}
static int task_width(void) {
    int count=0;for(int i=0;i<WINDOWS;i++)count+=windows[i].open;
    int width=count?74/count:14;return width>14?14:width;
}
static void taskbar(void) {
    gfx_rect(0,368,640,16,BORDER);gfx_rect(0,384,640,16,SURFACE);
    gfx_clear_text(0,23,80,2);label(1,23,status,78,WHITE,BORDER,0);
    button(0,24,6,"Desk",active<0);
    int x=6,width=task_width();
    for(int id=0;id<WINDOWS;id++)if(windows[id].open) {
        button(x,24,width,task_labels[id],active==id);
        gfx_rect(x*8+3,397,width*8-6,2,windows[id].hidden?MUTED:active==id?GOLD:ACCENT);
        x+=width;
    }
}
static int save(void) {
    char temp[FD_PATH], backup[FD_PATH], io[2048]; Entry existing;
    if (memo_blocked) { message("NOTE.TXT needs Text editor; original file retained"); return 0; }
    if (fd_path(temp, home, "KDTMP.TMP") || fd_path(backup, home, "KDOLD.TMP")) return 0;
    int present = p_stat(note_path, &existing), old = p_stat(backup, &existing);
    if ((present < 0 && present != -2) || old != -2) {
        message("Cannot save: read error or KDOLD.TMP already exists"); return 0;
    }
    int handle = p_create(temp), length = 0;
    if (handle < 0) { message("Cannot create KDTMP.TMP; original file retained"); return 0; }
    for (int i = 0; i < used; i++) {
        char ch=memo_get(i);
        if (ch == '\n') io[length++] = '\r';
        io[length++] = ch;
    }
    int wrote = p_write(handle, io, length), closed = p_close(handle);
    if (wrote != length || closed < 0) {
        p_unlink(temp); message("Save failed; original file and text retained"); return 0;
    }
    if (present == 0 && p_rename(note_path, backup) < 0) {
        p_unlink(temp); message("Cannot replace NOTE.TXT; original file retained"); return 0;
    }
    if (p_rename(temp, note_path) < 0) {
        if (present == 0 && p_rename(backup, note_path) < 0) {
            message("Save failed; recover original from KDOLD.TMP"); return 0;
        }
        p_unlink(temp); message("Save failed; original file and text retained"); return 0;
    }
    dirty = 0;
    if (present == 0 && p_unlink(backup) < 0) message("Saved NOTE.TXT; backup remains in KDOLD.TMP");
    else message("Saved NOTE.TXT");
    return 1;
}
static void load(void) {
    char io[2048];
    used = 0; memo_blocked = 0; select_all = 0;
    int handle = p_open(note_path);
    if (handle < 0) {
        if (handle != -2) memo_blocked = 1;
        return;
    }
    int n = p_read(handle, io, sizeof io);
    p_close(handle);
    if (n < 0 || n == (int)sizeof io) memo_blocked = 1;
    for (int i = 0; i < n; i++) {
        unsigned char ch = io[i];
        if (ch == '\r') continue;
        if (!(ch == '\n' || (ch >= 32 && ch <= 126)) || used == MEMO_CAPACITY - 1) {
            memo_blocked = 1; break;
        }
        memo_put(used++,ch);
    }
    if (memo_blocked) { used = 0; message("NOTE.TXT needs Text editor; original file retained"); }
}
static void enter_graphics(void) { gfx_start(); clip_state(1); }
/* Tagged applications run in independent guest sessions. Never replace a live
 * session as a side effect of opening a file or search result. */
static int launch_app(const char *program,const char *arguments) {
    unsigned char header[96];int h=p_open(program);
    if(h<0)return 0;
    int n=p_read(h,header,sizeof header);p_close(h);
    int kind=n<0?0:kapp_header(header,n);
    if(!kind)return 0;
    if(kind<0){message("Unsupported KDESK/386 application version or header");return 1;}
    if(!enhanced){message("This application needs KDESK/386; boot the enhanced image");return 1;}
    int slot;
    for(slot=0;slot<2;slot++)if(k386_word(slot,0)==K386_ENDED||k386_word(slot,0)>=K386_FAULTED)break;
    if(slot==2){message("Both DOS windows are in use; end one in Tasks, then open the app");return 1;}
    char command[128];int length=fd_len(program),args=fd_len(arguments);
    if(length+args+1>=128){message("Application command is too long");return 1;}
    fd_text(command,program,sizeof command);
    if(args){command[length++]=' ';fd_text(command+length,arguments,sizeof command-length);}
    if(k386_command(K386_CREATE,slot,0,command)){message("Cannot load KDESK/386 application");return 1;}
    dos_edit[slot]=0;dos_follow[slot]=1;windows[DOS1+slot].top=0;
    focus(DOS1+slot);message((const char *)header+40);return 1;
}
static void launch(const char *program, const char *arguments) {
    if(launch_app(program,arguments))return;
    scene_dirty=1; damage(0,0,640,400);
    clip_mouse_save(); clip_state(2); gfx_stop(); p_finish();
    int result = p_exec(program, arguments);
    clip_mouse_resume();
    if (result < 0) message("Cannot start program; check its path and memory");
    else {
        p_output("\r\nPress any key or click to return to KDESK...");
        mouse_start(); int previous = mouse_read().buttons;
        while (!key_ready()) {
            Mouse current = mouse_read();
            if ((current.buttons & 1) && !(previous & 1)) break;
            previous = current.buttons; input_idle();
        }
        if (key_ready()) key_read();
        mouse_stop(); message("Program returned to desktop");
    }
    if (!dirty) load();
    enter_graphics(); refresh();
}
static void tool(int id, const char *arguments) {
    if (id == 0) { refresh(); focus(FILES); return; }
    if (id == 7) { focus(CALCULATOR); return; }
    if (id == 8) { clock_read(&desk_clock); focus(CLOCK); return; }
    char path[FD_PATH];
    if (fd_path(path, home, enhanced&&id==4?"KSH.EXE":apps[id])) { message("Application path is too long"); return; }
    launch(path, arguments);
}
static void panel_close(void) {
    panel_open=0;scene_dirty=1;damage(96,32,453,325);
    message("App search closed");
}
static void panel_start(void) {
    panel_query[0]=0;panel_selected=panel_top=panel_all=0;
    panel_count=command_find(panel_query,enhanced,panel_matches);
    panel_open=1;mode=0;scene_dirty=1;damage(96,32,453,325);
    message("Find apps: type a name / arrows choose / Enter opens / Esc cancels");
}
static void panel_run(void) {
    if(!panel_count)return;
    unsigned id=panel_matches[panel_selected];panel_close();
    if(id<9){tool(id,"");return;}
    switch(id) {
    case 9:focus(MEMO);break;
    case 10:focus(TASKS);break;
    case 11:case 12:focus(DOS1+id-11);break;
    case 13:focus(HELP);break;
    case 14:place_window(2);break;
    case 15:place_window(3);break;
    case 16:place_window(0);break;
    case 17:show_desktop();break;
    case 18:focus(LAUNCHER);break;
    case 19:focus(ABOUT);break;
    case 20:place_window(1);break;
    case 21:case 22:case 23:{
        static const char *programs[]={"KHELLO.EXE","COUNTER.EXE","ECHO.EXE"};
        char path[FD_PATH];
        if(!fd_path(path,home,programs[id-21]))launch(path,"");
        break;
    }
    }
}
static void panel_key(int key) {
    if(key==27||key==11){panel_close();return;}
    if(key==13){panel_run();return;}
    int length=fd_len(panel_query),changed=0;
    if(key==1)panel_all=1;
    else if(key==21||(key==8&&panel_all)){panel_query[0]=0;panel_all=0;changed=1;}
    else if(key==8&&length){panel_query[length-1]=0;changed=1;}
    else if(key>=32&&key<=126) {
        if(panel_all){length=0;panel_all=0;}
        if(length<(int)sizeof panel_query-1){panel_query[length]=key;panel_query[length+1]=0;changed=1;}
    }
    if(changed){panel_count=command_find(panel_query,enhanced,panel_matches);panel_selected=panel_top=0;}
    if(panel_count) {
        if(key==KEY_UP&&panel_selected)panel_selected--;
        if(key==KEY_DOWN&&panel_selected+1<(int)panel_count)panel_selected++;
        if(key==KEY_HOME)panel_selected=0;
        if(key==KEY_END)panel_selected=panel_count-1;
        if(key==KEY_PAGEUP){panel_selected=panel_selected>5?panel_selected-5:0;panel_top=panel_top>5?panel_top-5:0;}
        if(key==KEY_PAGEDOWN){panel_selected=panel_selected+5<(int)panel_count?panel_selected+5:(int)panel_count-1;panel_top+=5;}
        int last=(int)panel_count-5;if(last<0)last=0;
        if(panel_top>last)panel_top=last;
        if(panel_selected<panel_top)panel_top=panel_selected;
        if(panel_selected>=panel_top+5)panel_top=panel_selected-4;
    }
    damage(96,32,453,325);
}
static void file_open(void) {
    Window *w = &windows[FILES];
    if (!count) return;
    Entry value;file_entry(w->selected,&value);Entry *entry=&value;
    char path[FD_PATH];
    if (fd_path(path, folder, entry->name)) { message("File path is too long"); return; }
    if (entry->attr & FD_DIR) {
        fd_text(folder, path, sizeof folder); w->selected = w->top = 0; refresh(); return;
    }
    const char *extension = entry->name;
    for (int i = 0; entry->name[i]; i++) if (entry->name[i] == '.') extension = entry->name + i;
    if (!fd_cmp(extension, ".COM") || !fd_cmp(extension, ".EXE")) launch(path, "");
    else if (!fd_cmp(extension, ".TXT") || !fd_cmp(extension, ".BAS")) {
        /* The bundled DOS tools parse bare DOS paths, so refuse ambiguous spaces. */
        for (int i = 0; path[i]; i++) if (path[i] == ' ') { message("Open paths without spaces using the DOS shell"); return; }
        tool(!fd_cmp(extension, ".BAS") ? 3 : 1, path);
    } else if (!fd_cmp(extension, ".BMP")) tool(5, path);
    else message("Open COM/EXE, TXT, BAS or BMP; use FD98 for other files");
}
/* Every pane owns its scroll row. Icons reflow and text wraps, so no content
 * needs a horizontal scrollbar at the supported minimum window sizes. */
typedef struct { int x,y,cols,rows,step,columns,total,height; } Pane;
static const char help_text[] =
    "F1 Help   F2 Launcher   F3 Files\nF4 Memo   F5 About      F9: Paint\n"
    "Drag title: move / corner: resize\nF6 then arrows: move window\n"
    "F7 then arrows: resize window\nF8: minimize  Tab/taskbar: restore\n"
    "Esc: close    F10: exit to DOS\nMemo: ASCII text, Ctrl+S saves\n"
    "V: icons/list; Calc + Clock\nScroll: arrows, track or drag thumb\n"
    "PgUp/PgDn: page; Home/End: ends\nFiles: Enter opens, BS goes up\n"
    "Files F: open FD98 file tools\nCtrl+G / title double-click: maximize\n"
    "Taskbar: switch, minimize, restore\nCtrl+K: find apps and window layouts\n"
    "Ctrl+D / Desk: show desktop, restore\nDesktop DOS icons: open a session";
static const char about_text[] =
    "Kuma Desktop 1.9\n\nOriginal PC-98 window system\n"
    "MS-DOS 4.0 / 640 x 400\n\nCopyright 2026 kumakumapon\n"
    "MIT license - see KDLIC.TXT";
static int document(Window *w, Pane *v, int id, int paint) {
    const char *s=id==HELP?help_text:about_text;
    int length=id==MEMO?used:fd_len(s), row=0,col=0;
    for(int i=0;i<length;i++) {
        char ch=id==MEMO?memo_get(i):s[i];
        if(ch=='\n') {row++;col=0;continue;}
        if(paint&&row>=w->top&&row<w->top+v->rows)
            gfx_cell(v->x+col,v->y+row-w->top,ch,id==MEMO&&select_all?WHITE:INK,id==MEMO&&select_all?SELECTED:PAPER,id==MEMO&&select_all);
        if(++col==v->cols){row++;col=0;}
    }
    if(paint&&id==MEMO&&active==MEMO&&row>=w->top&&row<w->top+v->rows)
        gfx_cell(v->x+col,v->y+row-w->top,'_',WHITE,SELECTED,1);
    return row+1;
}
static int icons(int id) { return id==LAUNCHER?icon_view:file_icons; }
static void pane(int id, Pane *v) {
    Window *w=&windows[id];
    v->x=w->x+2;v->y=w->y+(id==FILES?5:3);v->cols=w->w-6;
    v->columns=1;v->step=1;
    int height=w->h-(id==FILES?7:id==HELP||id==ABOUT?5:6);
    if(id>=DOS1)v->total=25;
    else if(id==LAUNCHER||id==FILES) {
        v->step=icons(id)?4:id==LAUNCHER?2:1;
        v->columns=icons(id)?v->cols/14:1;
        v->total=((id==LAUNCHER?LAUNCH_APPS:count)+v->columns-1)/v->columns;
    } else v->total=document(w,v,id,0);
    v->height=height;
    v->rows=(height+(icons(id)&&(id==LAUNCHER||id==FILES)?1:0))/v->step;
    if(v->rows<1)v->rows=1;
    w->top=scroll_clamp(w->top,v->total,v->rows);
}
static void reveal(int id) {
    Pane v;pane(id,&v);Window *w=&windows[id];
    int row=id==MEMO?v.total-1:w->selected/v.columns;
    if(row<w->top)w->top=row;
    if(row>=w->top+v.rows)w->top=row-v.rows+1;
}
static void scrollbar(Window *w, Pane *v) {
    if(v->total<=v->rows)return;
    int x=w->x+w->w-3,y=v->y,h=v->height*16,track=h-32;
    gfx_rect(x*8,y*16,16,h,SURFACE);
    button(x,y,2,"",0);label(x,y,"^",1,INK,BUTTON,0);
    button(x,y+h/16-1,2,"",0);label(x,y+h/16-1,"v",1,INK,BUTTON,0);
    int top=scroll_pixel(w->top,v->total,v->rows,track),size=scroll_thumb(v->total,v->rows,track);
    gfx_rect(x*8+1,y*16+16+top,14,size,BUTTON);
    gfx_rect(x*8+2,y*16+17+top,12,1,WHITE);
    gfx_rect(x*8+13,y*16+16+top,1,size,BORDER);
    gfx_rect(x*8+2,y*16+15+top+size,12,1,BORDER);
}
static int file_icon(Entry *entry) {
    if(entry->attr&FD_DIR)return 0;
    const char *ext=entry->name;
    for(int i=0;entry->name[i];i++)if(entry->name[i]=='.')ext=entry->name+i;
    if(!fd_cmp(ext,".COM")||!fd_cmp(ext,".EXE"))return 4;
    if(!fd_cmp(ext,".BAS"))return 3;
    if(!fd_cmp(ext,".BMP"))return 5;
    return 1;
}
static void draw_items(int id, Pane *v) {
    Window *w=&windows[id];int limit=id==LAUNCHER?LAUNCH_APPS:count;
    for(int row=0;row<v->rows;row++)for(int col=0;col<v->columns;col++) {
        int i=(w->top+row)*v->columns+col;if(i>=limit)continue;
        int x=v->x+col*14,y=v->y+row*v->step,selected=i==w->selected;
        int bg=selected?SELECTED:PAPER;
        Entry entry;if(id==FILES)file_entry(i,&entry);
        const char *name=id==LAUNCHER?icon_labels[i]:entry.name;
        if(icons(id)) {
            gfx_rect(x*8,y*16,13*8,3*16,bg);
            if(selected)gfx_rect(x*8,y*16,2,3*16,GOLD);
            utility_icon(id==LAUNCHER?i:file_icon(&entry),x*8+36,y*16);
            label(x,y+2,name,13,selected?WHITE:INK,bg,selected);
        } else {
            char caption[32];
            if(id==FILES) {
                fd_text(caption,entry.attr&FD_DIR?"<DIR> ":"      ",sizeof caption);
                fd_text(caption+6,name,sizeof caption-6);name=caption;
            } else name=labels[i];
            gfx_rect(x*8,y*16,v->cols*8,16,bg);
            label(x+(id==LAUNCHER),y,name,v->cols-1,selected?WHITE:INK,bg,selected);
        }
    }
}
static const char *calc_buttons[6][4] = {
    {"MC","MR","M-","M+"},{"CE","C","BS","/"},{"7","8","9","*"},
    {"4","5","6","-"},{"1","2","3","+"},{"+/-","0",".","="}
};
static const int calc_keys[6][4] = {
    {CALC_MC,CALC_MR,CALC_MINUS,CALC_PLUS},{CALC_CE,'c',8,'/'},
    {'7','8','9','*'},{'4','5','6','-'},{'1','2','3','+'},{CALC_SIGN,'0','.','='}
};
static void calculator_key(int key) {
    if(key == 3) {
        if(!calculator.error) {clip_copy(calculator.input,fd_len(calculator.input));message("Copied calculator result");}
    } else if(key == 22) {
        if(calc_paste(&calculator,(const char *)clipboard.text,clipboard.length)) message("Pasted calculator number");
        else message("Clipboard needs a number: up to 8 integer / 4 decimal digits");
    } else {
        int changed=calc_key(&calculator,key);
        if(changed) message(changed==2?"Calculator input limit: 8 integer / 4 decimal digits":"Calculator ready");
    }
}
static void calculator_value(Window *w) {
    const char *value=calc_text(&calculator);
    gfx_clear_text(w->x+2,w->y+3,w->w-4,3);
    gfx_rect((w->x+2)*8,(w->y+5)*16,(w->w-4)*8,16,PAPER);
    gfx_rect((w->x+2)*8,(w->y+3)*16,(w->w-4)*8,32,SURFACE);
    if(calculator.error) label(w->x+3,w->y+3,value,w->w-6,INK,SURFACE,0);
    else utility_digits((w->x+w->w-3)*8-fd_len(value)*16,(w->y+3)*16+3,value,INK);
    const char *prefix="Value: ";int at=w->x+2;
    for(int i=0;prefix[i];i++)p_cell(at++,w->y+3,prefix[i],0);
    for(int i=0;value[i];i++)p_cell(at++,w->y+3,value[i],0);
    if(calculator.memory) text(w->x+2,w->y+5,"M",1,0);
    if(calculator.pending) {char op[]={calculator.pending,0};text(w->x+w->w-4,w->y+5,op,1,0);}
 }
static void draw_calculator(Window *w) {
    calculator_value(w);
    int left=w->x+(w->w-32)/2;
    for(int row=0;row<6;row++)for(int col=0;col<4;col++)button(left+col*8,w->y+6+row*2,7,calc_buttons[row][col],0);
    button(w->x+2,w->y+w->h-2,8,"Copy",0);button(w->x+11,w->y+w->h-2,9,"Paste",0);button(w->x+21,w->y+w->h-2,5,"%",0);
}
static void clock_controls(Window *w) {
    button(w->x+2,w->y+w->h-2,10,twelve_hour?"24-hour":"12-hour",0);
    button(w->x+13,w->y+w->h-2,8,"Copy",0);
    clock_draw(&desk_clock,twelve_hour,w->x*8,w->y*16,w->w*8);
}
static void frame(int x, int y, int w, int h) {
    gfx_rect(x * 8 + 5, y * 16 + 5, w * 8, h * 16, SHADOW);
    gfx_rect(x * 8, y * 16, w * 8, h * 16, PAPER);
    gfx_rect(x * 8, y * 16, w * 8, 1, WHITE);
    gfx_rect(x * 8, y * 16, 1, h * 16, WHITE);
    gfx_rect(x * 8, (y + h) * 16 - 2, w * 8, 2, BORDER);
    gfx_rect((x + w) * 8 - 2, y * 16, 2, h * 16, BORDER);
    gfx_rect(x*8+2,y*16+2,w*8-5,1,SURFACE);
    gfx_rect(x*8+2,y*16+2,1,h*16-5,SURFACE);
    gfx_clear_text(x, y, w + 1, h + 1);
}
static void draw_panel(void) {
    frame(12,2,56,20);
    gfx_rect(99,37,442,31,ACCENT);
    label(14,3,"Find apps",40,WHITE,ACCENT,0);
    label(64,3,"x",1,WHITE,ACCENT,0);
    gfx_rect(111,79,418,34,ACCENT);gfx_rect(112,80,416,32,SURFACE);
    label(15,5,panel_query[0]?panel_query:"Type a name...",49,panel_all?WHITE:INK,panel_all?SELECTED:SURFACE,panel_all);
    if(panel_query[0]&&!panel_all)label(15+fd_len(panel_query),5,"_",1,ACCENT,SURFACE,0);
    if(!panel_count)text(16,9,"No matching apps",45,0);
    for(int row=0;row<5&&panel_top+row<(int)panel_count;row++) {
        int index=panel_top+row,y=7+row*2,selected=index==panel_selected;
        const DeskCommand *c=&desk_commands[panel_matches[index]];
        int bg=selected?SELECTED:PAPER;
        gfx_rect(112,y*16,416,32,bg);
        if(selected)gfx_rect(112,y*16,3,32,GOLD);
        utility_icon(c->icon,120,y*16);
        label(20,y,c->name,42,selected?WHITE:INK,bg,selected);
        char context[44]="Arrange: ";const char *hint=c->hint;
        if(!*hint){fd_text(context+9,active>=0?titles[active]:"choose a window first",sizeof context-9);hint=context;}
        label(20,y+1,hint,44,selected?SURFACE:MUTED,bg,0);
    }
    char footer[48];fd_number(footer,panel_count);
    fd_text(footer+fd_len(footer)," results - arrows / Enter / Esc",sizeof footer-fd_len(footer));
    label(14,20,footer,52,MUTED,PAPER,0);
    button(59,20,3,"^",0);button(63,20,3,"v",0);
}
static void title_button(int x,int y,int kind) {
    button(x,y,3,kind==0?"_":kind==1?"+":kind==2?"=":"x",0);
    /* Keep ordinary text VRAM labels for accessibility/debugging. */
    gfx_rect(x*8+2,y*16+2,20,12,BUTTON);
    int px=x*8+8,py=y*16+4;
    if(kind==0)gfx_rect(px,py+6,8,2,INK);
    else if(kind==3)for(int i=0;i<7;i++) {
        gfx_rect(px+i,py+i,2,1,SELECTED);gfx_rect(px+6-i,py+i,2,1,SELECTED);
    } else {
        if(kind==2){gfx_rect(px+2,py,7,6,INK);gfx_rect(px+3,py+1,5,4,BUTTON);}
        gfx_rect(px,py+(kind==2?2:0),8,8,INK);
        gfx_rect(px+1,py+(kind==2?4:2),6,5,BUTTON);
    }
}
static void draw_window(int id) {
    Window *w = &windows[id]; gfx_origin(w->ox, w->oy); int title_color = active == id ? ACCENT : INACTIVE;
    frame(w->x, w->y, w->w, w->h);
    gfx_rect(w->x * 8 + 3, w->y * 16 + 5, w->w * 8 - 6, 31, title_color);
    gfx_rect(w->x * 8 + 3, w->y * 16 + 5, w->w * 8 - 6, 2, active==id?HANDLE:SURFACE);
    gfx_rect(w->x*8+3,w->y*16+34,w->w*8-6,2,active==id?SELECTED:MUTED);
    label(w->x + 2, w->y + 1, titles[id], w->w - 17, active == id ? WHITE : INK, title_color, 0);
    title_button(w->x+w->w-13,w->y+1,0);
    title_button(w->x+w->w-9,w->y+1,placement[id]==1?2:1);
    title_button(w->x+w->w-5,w->y+1,3);
    Pane v;
    if(id<CALCULATOR||id>=DOS1)pane(id,&v);
    if (id == LAUNCHER || id == FILES) {
        draw_items(id,&v);
        if(id==LAUNCHER) {
            char selected[48]="Selected: ";fd_text(selected+10,icon_labels[w->selected],sizeof selected-10);
            text(w->x+2,w->y+w->h-3,selected,w->w-4,0);
            button(w->x+2,w->y+w->h-2,9,icon_view?"List":"Icons",0);
            button(w->x+12,w->y+w->h-2,8,"Open",0);
            if(w->w>=34)button(w->x+22,w->y+w->h-2,8,"Find",0);
            if(w->w>=42)text(w->x+32,w->y+w->h-2,"Ctrl+K",w->w-34,0);
        } else {
            gfx_rect((w->x+2)*8-1,(w->y+3)*16-2,(w->w-4)*8,29,SURFACE);
            gfx_rect((w->x+2)*8,(w->y+3)*16-1,(w->w-4)*8-2,26,PAPER);
            line(w,0,folder,0);
            if(list_error)line(w,2,"Directory read failed",0);
            else if(!count)line(w,2,"Empty directory",0);
            button(w->x+2,w->y+w->h-2,9,"Parent",0);
            button(w->x+12,w->y+w->h-2,8,"Open",0);
            button(w->x+21,w->y+w->h-2,8,file_icons?"List":"Icons",0);
            if(w->w>=39)button(w->x+30,w->y+w->h-2,7,"FD98",0);
            if(truncated)text(w->x+w->w-8,w->y+3,"128+",4,0);
        }
    } else if (id == MEMO) {
        if (memo_blocked) line(w, 0, "Open NOTE.TXT in the Text editor", 0);
        else document(w,&v,MEMO,1);
        button(w->x + 2, w->y + w->h - 2, 9, "Save", 0);
        text(w->x + 2, w->y + w->h - 3, dirty ? "NOTE.TXT *" : "NOTE.TXT", w->w - 4, 0);
        button(w->x + 12, w->y + w->h - 2, 7, "Copy", 0);
        button(w->x + 20, w->y + w->h - 2, 6, "Cut", 0);
        if (w->w >= 40) button(w->x + 27, w->y + w->h - 2, 8, "Paste", 0);
        if (w->w >= 46) button(w->x + 36, w->y + w->h - 2, 7, "All", 0);
    } else if (id == CALCULATOR) {
        draw_calculator(w);
    } else if (id == CLOCK) {
        clock_controls(w);
    } else if(id==TASKS) {
        for(int i=0;i<WINDOWS&&i<w->h-6;i++) {
            char row[48];int at=0;unsigned n=desktop_tasks[i].id;
            row[at++]='0'+n/100%10;row[at++]='0'+n/10%10;row[at++]='0'+n%10;row[at++]=' ';
            fd_text(row+at,titles[i],sizeof row-at);at=fd_len(row);
            while(at<16)row[at++]=' ';
            static const char *dos_states[]={"Ended","Running","Waiting","Stopped","Fault"};
            unsigned state=i>=DOS1&&enhanced?k386_word(i-DOS1,0):0;
            fd_text(row+at,i>=DOS1&&enhanced?dos_states[state<=4?state:4]:task_state(i),sizeof row-at);
            line(w,i,row,w->selected==i);
        }
        draw_resources(w);
        button(w->x+2,w->y+w->h-2,8,"Switch",0);
        button(w->x+11,w->y+w->h-2,7,"Stop",0);
        button(w->x+19,w->y+w->h-2,8,"Start",0);
        if(w->w>=36)button(w->x+28,w->y+w->h-2,6,"End",0);
        if(k386_resources_available()&&w->w>=66){
            static const char *controls[]={"Prio","Limit","Break"};
            for(int i=0;i<3;i++)button(w->x+38+i*8,w->y+w->h-2,7,controls[i],0);
        }
    } else if(id>=DOS1) {
        unsigned session=id-DOS1;
        if(!enhanced){line(w,0,"Use the KDESK/386 experimental disk",0);line(w,1,"DOS windows need K386 without EMM386",0);}
        else {
            if(dos_horizontal[session]>80-(w->w-6))dos_horizontal[session]=80-(w->w-6);
            for(int row=0;row<w->h-6;row++)for(int col=0;col<w->w-6;col++) {
                unsigned cell=(w->top+row)*80+dos_horizontal[session]+col;
                if(cell<2000)gfx_cell(w->x+2+col,w->y+3+row,k386_character(session,cell),INK,PAPER,0);
            }
            if(dos_edit[session])text(w->x+2,w->y+w->h-3,dos_path[session],w->w-4,0);
            else {static const char *states[]={"Ended","Running","Waiting","Stopped","Fault"};unsigned state=k386_word(session,0);text(w->x+2,w->y+w->h-3,states[state<=4?state:4],w->w-4,0);}
            button(w->x+w->w-9,w->y+w->h-3,3,"<",0);button(w->x+w->w-5,w->y+w->h-3,3,">",0);
            button(w->x+2,w->y+w->h-2,7,"Run",0);button(w->x+10,w->y+w->h-2,7,"Pause",0);
            button(w->x+18,w->y+w->h-2,8,"Resume",0);if(w->w>=35)button(w->x+27,w->y+w->h-2,6,"End",0);
        }
    } else document(w,&v,id,1);
    if(id<CALCULATOR||id>=DOS1)scrollbar(w,&v);
    for (int i = 0; i < 3; i++)
        gfx_rect((w->x + w->w) * 8 - 7 + i * 2, (w->y + w->h) * 16 - 5 - i * 3, 1, 3 + i * 3, MUTED);
    gfx_origin(0, 0);
}
static void compose(int foreground) {
    gfx_target(1);
    gfx_rect(0, 0, 640, 400, DESKTOP); gfx_clear_text(0, 0, 80, 25);
    if (gfx_colors() == 256) for (int y = 16; y < 368; y++)
        gfx_rect(0, y, 640, 1, 232 + (y - 16) * 23 / 351);
    else for (int y = 64; y < 352; y += 32) gfx_rect(0, y, 640, 1, DESKTOP_LIGHT);
    label(60,21,"PC-98 / MS-DOS",18,HANDLE,DESKTOP,0);
    /* Original geometric wallpaper; the system palette remains available to images. */
    for(int i=0;i<4;i++) {
        gfx_rect(480+i*24,256+i*16,128-i*24,1,DESKTOP_LIGHT);
        gfx_rect(480+i*24,256+i*16,1,72-i*16,DESKTOP_LIGHT);
    }
    if(enhanced)for(int i=0;i<2;i++) {
        int x=58+i*11;
        gfx_rect(x*8,48,80,64,DESKTOP_LIGHT);
        utility_icon(4,x*8+24,54);
        label(x+2,6,titles[DOS1+i],6,WHITE,DESKTOP_LIGHT,0);
    }
    gfx_rect(0, 0, 640, 16, INK);
    button(0,0,14,"Kuma Desktop",panel_open);
    static const char *menus[] = {"Apps", "Files", "Memo", "Help", "About", "Paint", "Exit", "Tasks"};
    static const int menu_x[] = {22, 30, 39, 47, 55, 63, 71, 14};
    for (int i = 0; i < 8; i++) button(menu_x[i], 0, 7, menus[i], 0);
    for (int i = 0; i < WINDOWS; i++) {
        int id = order[i];
        if (id != active && windows[id].open && !windows[id].hidden) draw_window(id);
    }
    if(foreground && active>=0 && !windows[active].hidden)draw_window(active);
    taskbar();
    if (confirm) {
        label(1, 23, "Unsaved memo: Enter saves / D discards / Esc cancels", 78, WHITE, BORDER, 0);
        frame(17, 9, 46, 8);
        gfx_rect(17 * 8 + 3, 9 * 16 + 8, 46 * 8 - 6, 28, ACCENT);
        label(19, 10, "Unsaved memo", 40, WHITE, ACCENT, 0);
        text(20, 12, "Save changes before closing?", 40, 0);
        button(20, 14, 10, "Save", 0); button(34, 14, 11, "Discard", 0); button(49, 14, 11, "Cancel", 0);
    } else if (mode) label(1, 23, mode == 1 ? "MOVE: arrows change position; Enter finishes" : "RESIZE: arrows change size; Enter finishes", 78, WHITE, BORDER, 0);
    if(panel_open)draw_panel();
}
static void draw(void) {
    compose(1);present();scene_dirty=0;
}
static void close_window(void) {
    invalidate(active);scene_dirty=1;
    if(active>=DOS1&&enhanced)k386_command(K386_END,active-DOS1,0,0);
    task_exit(active);windows[active].open = 0; next();
}
static int handle_confirm(int key) {
    if (!confirm) return 0;
    if (key == 27) { confirm = 0; exiting = 0; message("Close cancelled"); }
    else if (key == 13 || key == 'd' || key == 'D') {
        if (key == 13 && !save()) return 1;
        if (key != 13 && !exiting) load();
        dirty = 0; confirm = 0;
        if (exiting) return 2;
        close_window(); message("Memo closed");
    }
    return 1;
}
static int paste_memo(void) {
    char incoming[1024]; int n = clipboard.length, length = 0;
    if (memo_blocked || n >= CLIP_CAPACITY) { message("Cannot paste into this memo"); return 0; }
    for (int i = 0; i < n; i++) {
        unsigned char ch = clipboard.text[i];
        if (ch == '\r') { if (i + 1 < n && clipboard.text[i + 1] == '\n') continue; ch = '\n'; }
        if (!(ch == '\n' || (ch >= 32 && ch <= 126))) { message("Clipboard needs ASCII text; use Text editor for Japanese"); return 0; }
        incoming[length++] = ch;
    }
    int start = select_all ? 0 : used;
    if (start + length > 1023) { message("Clipboard exceeds memo capacity; text retained"); return 0; }
    mem_copy(entries_segment,MEMO_OFFSET+start,data_segment,(unsigned)incoming,length);
    if (length || select_all) dirty = 1;
    used = start + length; select_all = 0; reveal(MEMO); message("Pasted clipboard"); return 1;
}
static int handle_key(int key) {
    int decision = handle_confirm(key);
    if (decision == 2) return 1;
    if (decision) return 0;
    if(panel_open){panel_key(key);return 0;}
    if(key==11){panel_start();return 0;}
    if(key==4){show_desktop();return 0;}
    if (key == KEY_F1 + 8) { tool(6, ""); return 0; }
    if (key == KEY_F1 + 9) {
        if (dirty) { confirm = 1; exiting = 1; return 0; }
        return 1;
    }
    if (key >= KEY_F1 && key <= KEY_F1 + 4) {
        int id = key == KEY_F1 ? HELP : key == KEY_F1 + 1 ? LAUNCHER : key == KEY_F1 + 2 ? FILES : key == KEY_F1 + 3 ? MEMO : ABOUT;
        if (id == FILES) refresh();
        focus(id); return 0;
    }
    if (key == 9) { next(); return 0; }
    if(key==20) {focus(TASKS);return 0;}
    if(key==14||key==15){focus(key==14?DOS1:DOS2);return 0;}
    if (active < 0) return 0;
    Window *w = &windows[active];
    if(key==7||key==1004){maximize();return 0;}
    if (key == KEY_F1 + 5 || key == KEY_F1 + 6) {
        if(placement[active])place_window(0);
        int wanted = key == KEY_F1 + 5 ? 1 : 2;
        mode = mode == wanted ? 0 : wanted;
        if (mode) w->ox = w->oy = 0; /* Keyboard geometry uses whole cells. */
        return 0;
    }
    if (mode && (key == 13 || key == 27)) { mode = 0; return 0; }
    if (mode) {
        int dx = key == KEY_RIGHT ? 1 : key == KEY_LEFT ? -1 : 0;
        int dy = key == KEY_DOWN ? 1 : key == KEY_UP ? -1 : 0;
        if (mode == 1) {
            if (w->x + dx >= 0 && w->x + dx + w->w < 80) w->x += dx;
            if (w->y + dy >= 1 && w->y + dy + w->h < 24) w->y += dy;
        } else {
            if (w->w + dx >= (active == CALCULATOR ? 36 : active == CLOCK ? 32 : 30) && w->x + w->w + dx < 80) w->w += dx;
            if (w->h + dy >= (active == CALCULATOR ? 20 : active == CLOCK ? 15 : 10) && w->y + w->h + dy < 24) w->h += dy;
        }
        return 0;
    }
    if (key == KEY_F1 + 7) { invalidate(active);scene_dirty=1;w->hidden = 1; active = -1;
        for (int i = WINDOWS - 1; i >= 0; i--) if (windows[order[i]].open && !windows[order[i]].hidden) { active = order[i]; break; }
        message("Window minimized; Tab restores"); return 0;
    }
    if(key==1003){close_window();return 0;}
    if(key==27&&active>=DOS1&&enhanced){if(dos_edit[active-DOS1])dos_edit[active-DOS1]=0;else task_post(active,TASK_KEY,key);return 0;}
    if (key == 27) {
        if (active == MEMO && dirty) { confirm = 1; exiting = 0; }
        else close_window();
        return 0;
    }
    if(!task_post(active,TASK_KEY,key))message("Task stopped or input queue full");
    return 0;
}
/* Guest ASCII keys only enqueue input. Paint after the VM produces output,
 * rather than repainting the desktop twice for every input batch. */
static int guest_input(int id,unsigned key) {
    return !panel_open&&enhanced&&id>=DOS1&&!dos_edit[id-DOS1]&&!mode&&!confirm&&key<128
        &&key!=4&&key!=11&&key!=7&&key!=9&&key!=12&&key!=14&&key!=15&&key!=20&&key!=16&&key!=18&&key!=5
        &&(key!=3||!k386_resources_available());
}
static __attribute__((noinline)) int app_key(int id,int key) {
    Window *w=&windows[id];
    if(id==TASKS) {
        if(key==KEY_UP&&w->selected)w->selected--;
        if(key==KEY_DOWN&&w->selected+1<WINDOWS)w->selected++;
        int selected=w->selected;
        if(key==13)focus(selected);
        if(selected>=DOS1&&enhanced) {
            invalidate(selected);scene_dirty=1;
            unsigned session=selected-DOS1;
            if(k386_resources_available()){
                if(key=='p'||key=='P')k386_command(K386_PRIORITY,session,0xffff,0);
                if(key=='l'||key=='L')k386_command(K386_BUDGET,session,0xffff,0);
                if(key=='b'||key=='B')if(k386_command(K386_INTERRUPT,session,0,0))message("No running DOS program to interrupt");
                if(key=='['&&event_age+1<K386_EVENTS&&!k386_command(K386_DESCRIBE,session,event_age+1,0))event_age++;
                if(key==']'&&event_age)event_age--;
            }
            if(key=='s'||key=='S')k386_command(K386_STOP,selected-DOS1,0,0);
            if(key=='r'||key=='R'){if(k386_word(selected-DOS1,0)>=K386_FAULTED||k386_word(selected-DOS1,0)==K386_ENDED)k386_command(K386_CREATE,selected-DOS1,0,"KSH.EXE");else if(k386_command(K386_RESUME,selected-DOS1,0,0))message("Cannot resume; raise or disable the CPU limit in Tasks");focus(selected);}
            if(key=='k'||key=='K')k386_command(K386_END,selected-DOS1,0,0);
            return 0;
        }
        if((key=='s'||key=='S')&&selected!=TASKS)task_stop(selected);
        if(key=='r'||key=='R') {if(desktop_tasks[selected].state==TASK_STOPPED)task_resume(selected);else if(desktop_tasks[selected].state==TASK_EXITED)focus(selected);else if(desktop_tasks[selected].state==TASK_FAULTED){task_start(selected);focus(selected);}}
        if((key=='k'||key=='K')&&selected!=TASKS) {focus(selected);if(selected==MEMO&&dirty){confirm=1;exiting=0;}else close_window();}
        return 0;
    }
    if(id>=DOS1&&enhanced) {
        unsigned session=id-DOS1;
        if(key==1001){if(dos_horizontal[session])dos_horizontal[session]--;return 0;}
        if(key==1002){if(dos_horizontal[session]<80-(w->w-6))dos_horizontal[session]++;return 0;}
        if(key==12){dos_edit[session]=1;dos_path[session][0]=0;message("COM/EXE command: type a name, Enter runs (files read-only)");return 0;}
        if(key==16){k386_command(K386_STOP,session,0,0);return 0;}
        if(key==18){if(k386_command(K386_RESUME,session,0,0))message("Cannot resume; raise or disable the CPU limit in Tasks");return 0;}
        if(key==5){k386_command(K386_END,session,0,0);return 0;}
        if(dos_edit[session]) {
            int n=fd_len(dos_path[session]);
            if(key==8&&n)dos_path[session][--n]=0;
            else if(key>=32&&key<=126&&n<127){dos_path[session][n]=key;dos_path[session][n+1]=0;}
            else if(key==13){if(k386_command(K386_CREATE,session,0,dos_path[session]))message("Cannot load program (path, memory or unsupported image)");else{dos_edit[session]=0;dos_follow[session]=1;w->top=0;message("V86 program running; files read-only");}}
        } else if(key==3&&k386_resources_available()){
            if(k386_command(K386_INTERRUPT,session,0,0))message("No running DOS program to interrupt");else message("DOS program interrupted");
        } else if(key==KEY_PAGEUP){dos_follow[session]=0;if(w->top)w->top--;}
        else if(key==KEY_PAGEDOWN){if(w->top<25-(w->h-6))w->top++;dos_follow[session]=w->top>=25-(w->h-6);}
        else if(key>=0&&key<=127){dos_follow[session]=1;if(k386_command(K386_KEY,session,key,0)){message("Input queue full; resume the session and retry");scene_dirty=1;}}
        return 0;
    }
    if(id==MEMO||id==HELP||id==ABOUT) {
        Pane v;pane(id,&v);int top=w->top;
        if(key==KEY_UP)top--;else if(key==KEY_DOWN)top++;
        else if(key==KEY_PAGEUP)top-=v.rows;else if(key==KEY_PAGEDOWN)top+=v.rows;
        else if(key==KEY_HOME)top=0;else if(key==KEY_END)top=v.total;
        else top=w->top;
        if(key>=KEY_UP&&key<=KEY_PAGEDOWN) {w->top=scroll_clamp(top,v.total,v.rows);return 0;}
    }
    if (id == LAUNCHER || id == FILES) {
        if(key=='v'||key=='V') {
            if(id==LAUNCHER)icon_view=!icon_view;else file_icons=!file_icons;
            w->top=0;reveal(id);
            message(id==LAUNCHER?(icon_view?"Launcher icon view":"Launcher list view"):(file_icons?"Files icon view":"Files list view"));return 0;
        }
        if(id==FILES&&(key=='f'||key=='F')) {
            char path[FD_PATH];
            if(!fd_path(path,home,apps[0])&&p_chdir(folder)>=0) {launch(path,"");p_chdir(home);}
            else message("Cannot open FD98 in this directory");
            return 0;
        }
        Pane v;pane(id,&v);
        int limit=id==LAUNCHER?LAUNCH_APPS:count,step=v.columns;
        if(key==KEY_PAGEUP||key==KEY_PAGEDOWN)step*=v.rows;
        if((key==KEY_UP||key==KEY_PAGEUP)&&w->selected>0)w->selected=w->selected>step?w->selected-step:0;
        if((key==KEY_DOWN||key==KEY_PAGEDOWN)&&w->selected+1<limit)w->selected=w->selected+step<limit?w->selected+step:limit-1;
        if(key==KEY_LEFT&&w->selected>0)w->selected--;
        if(key==KEY_RIGHT&&w->selected+1<limit)w->selected++;
        if(key==KEY_HOME)w->selected=0;
        if(key==KEY_END&&limit)w->selected=limit-1;
        if(key>=KEY_UP&&key<=KEY_PAGEDOWN)reveal(id);
        if(key==13) {if(id==LAUNCHER)tool(w->selected,"");else file_open();}
        if(id==FILES&&key==8) {fd_parent(folder);w->selected=w->top=0;refresh();}
    } else if (id == CALCULATOR) {
        calculator_key(key);
    } else if (id == CLOCK) {
        if(key=='t'||key=='T') {twelve_hour=!twelve_hour;message(twelve_hour?"Clock 12-hour display":"Clock 24-hour display");}
        if(key==3) {char value[32];clock_text(&desk_clock,twelve_hour,1,value);clip_copy(value,fd_len(value));message("Copied clock date/time");}
    } else if (id == MEMO) {
        if (memo_blocked) { message("Use Text editor for non-ASCII or larger NOTE.TXT"); return 0; }
        if (key == 1) { select_all = !select_all; message(select_all ? "Selected all memo text" : "Selection cleared"); }
        else if (key == 3 || key == 24) {
            char value[MEMO_CAPACITY];mem_copy(data_segment,(unsigned)value,entries_segment,MEMO_OFFSET,used);
            clip_copy(value, used); message("Copied memo to clipboard");
            if (key == 24 && used) { used = 0; dirty = 1; select_all = 0; message("Cut memo to clipboard"); }
        } else if (key == 22) paste_memo();
        else if (key == 19) save();
        else if (key == 8 && select_all) { used = 0; dirty = 1; select_all = 0; }
        else if (key == 8 && used) { used--; dirty = 1; }
        else if ((key == 13 || (key >= 32 && key <= 126)) && (select_all || used < MEMO_CAPACITY - 1)) {
            if (select_all) { used = 0; select_all = 0; }
            memo_put(used++,key == 13 ? '\n' : key); dirty = 1;
        } else if (used == MEMO_CAPACITY - 1) message("Memo full (1023 characters); use the Text editor for larger files");
        if(key==8||key==13||key==24||(key>=32&&key<=126))reveal(MEMO);
    }
    return 0;
}

static int live_drag, drag, drag_id, drag_mx, drag_my, drag_x, drag_y, drag_w, drag_h;
static int preview_x, preview_y, preview_w, preview_h, preview_visible;
static int last_file = -1, last_app = -1;
static u32 app_click;
static u32 last_click;
static int last_title=-1,title_x,title_y;
static u32 title_click;
static int clamp(int n, int low, int high) { return n < low ? low : n > high ? high : n; }
static __attribute__((noinline)) int mouse_event(Mouse now, int previous, int *redraw) {
    int cx = now.x / 8, cy = now.y / 16;
    if(scrolling>=0) {
        Window *w=&windows[scrolling];Pane v;pane(scrolling,&v);
        int top=scroll_row(now.y-w->oy-v.y*16-16-scroll_grab,v.total,v.rows,v.height*16-32);
        if(top!=w->top){w->top=top;*redraw=1;}
        if(scrolling>=DOS1)dos_follow[scrolling-DOS1]=top>=v.total-v.rows;
        if(!(now.buttons&1))scrolling=-1;
        return 0;
    }
    if (drag && (previous & 1)) {
        if((unsigned)(now.x-drag_mx+4)>8||(unsigned)(now.y-drag_my+4)>8)last_title=-1;
        Window *w = &windows[drag_id];
        if (!w->open || w->hidden || confirm) { drag = 0; return 0; }
        int x = w->x, y = w->y, width = w->w, height = w->h;
        if (drag == 1) {
            int px = clamp(drag_x + now.x - drag_mx, 0, 640 - w->w * 8 - 5);
            int py = clamp(drag_y + now.y - drag_my, 16, 368 - w->h * 16);
            if (live_drag == 1) { gfx_drag_move(px, py); damage(px,py,w->w*8+5,w->h*16+5); }
            x = px / 8; y = py / 16;
            if (!(now.buttons & 1)) { w->ox = gfx_colors() == 256 ? px & 7 : 0; w->oy = gfx_colors() == 256 ? py & 15 : 0; gfx_drag_end(); live_drag = 0; }
        } else {
            width = clamp(drag_w + (now.x - drag_mx) / 8, drag_id == CALCULATOR ? 36 : drag_id == CLOCK ? 32 : 30, 79 - w->x);
            height = clamp(drag_h + (now.y - drag_my) / 16, drag_id == CALCULATOR ? 20 : drag_id == CLOCK ? 15 : 10, 23 - w->y - (w->oy != 0));
        }
        gfx_origin(w->ox, w->oy);
        if (preview_visible) gfx_outline(preview_x, preview_y, preview_w, preview_h);
        preview_visible = 0;
        if (!(now.buttons & 1)) {
            w->x = x; w->y = y; w->w = width; w->h = height;
            drag = 0; *redraw = 1;
        } else if (live_drag != 1) {
            preview_x = x; preview_y = y; preview_w = width; preview_h = height;
            gfx_outline(x, y, width, height); preview_visible = 1;
        }
        gfx_origin(0, 0);
        return 0;
    }
    if (!(now.buttons & 1)) { drag = 0; return 0; }
    if (previous & 1) return 0;
    int previous_file = last_file, previous_app = last_app, previous_title=last_title;
    last_title=-1;
    last_app = -1;
    last_file = -1; /* Only consecutive clicks in the same file list open it. */
    *redraw = 1;
    if(panel_open) {
        if(cx<12||cx>=68||cy<2||cy>=22||(cy==3&&cx>=63)){panel_close();return 0;}
        if(cy==5||cy==6){panel_all=1;return 0;}
        if(cy==20&&cx>=59&&cx<66){if(cx<62)panel_key(KEY_PAGEUP);else if(cx>=63)panel_key(KEY_PAGEDOWN);return 0;}
        if(cy>=7&&cy<17) {
            int index=panel_top+(cy-7)/2;
            if(index<(int)panel_count){panel_selected=index;panel_run();}
        }
        return 0;
    }
    if (confirm) {
        if (cy == 14) {
            if (cx >= 20 && cx < 30) return 13;
            if (cx >= 34 && cx < 45) return 'd';
            if (cx >= 49 && cx < 60) return 27;
        }
        return 0;
    }
    if (cy == 0) {
        if(cx<14)return 11;
        static const int menu_x[] = {22, 30, 39, 47, 55, 63, 71, 14};
        static const int menu_key[] = {KEY_F1 + 1, KEY_F1 + 2, KEY_F1 + 3, KEY_F1, KEY_F1 + 4, KEY_F1 + 8, KEY_F1 + 9, 20};
        for (int i = 0; i < 8; i++) if (cx >= menu_x[i] && cx < menu_x[i] + 7) return menu_key[i];
        return 0;
    }
    if (cy == 24) {
        if(cx<6)return 4;
        int slot=(cx-6)/task_width();
        for(int id=0;id<WINDOWS;id++)if(windows[id].open&&!slot--) {
            if(active==id&&!windows[id].hidden)return KEY_F1+7;
            focus(id);break;
        }
        return 0;
    }
    for (int i = WINDOWS - 1; i >= 0; i--) {
        int id = order[i]; Window *w = &windows[id];
        cx = (now.x - w->ox) / 8; cy = (now.y - w->oy) / 16;
        if (!w->open || w->hidden || now.x < w->x * 8 + w->ox || now.x >= (w->x + w->w) * 8 + w->ox ||
            now.y < w->y * 16 + w->oy || now.y >= (w->y + w->h) * 16 + w->oy) continue;
        focus(id);
        if (cy == w->y + 1 && cx >= w->x + w->w - 5 && cx < w->x + w->w - 2) return id>=DOS1?1003:27;
        if (cy == w->y + 1 && cx >= w->x + w->w - 9 && cx < w->x + w->w - 6) return 1004;
        if (cy == w->y + 1 && cx >= w->x + w->w - 13 && cx < w->x + w->w - 10) return KEY_F1 + 7;
        if (cx >= w->x + w->w - 2 && cy >= w->y + w->h - 2) drag = 2;
        else if (cy <= w->y + 1) {
            if(previous_title==id&&((now.ticks-title_click)&0xffffff)<153600&&
               (unsigned)(now.x-title_x+4)<=8&&(unsigned)(now.y-title_y+4)<=8)return 1004;
            last_title=id;title_click=now.ticks;title_x=now.x;title_y=now.y;drag=1;
        }
        if(placement[id])drag=0;
        if (drag) {
            last_file = -1; drag_id = id; drag_mx = now.x; drag_my = now.y;
            drag_x = w->x * 8 + w->ox; drag_y = w->y * 16 + w->oy; drag_w = w->w; drag_h = w->h; return 0;
        }
        Pane v;
        if(id<CALCULATOR||id>=DOS1) {
            pane(id,&v);
            int py=now.y-w->oy-v.y*16,h=v.height*16,track=h-32;
            if(cx>=w->x+w->w-3&&cx<w->x+w->w-1&&py>=0&&py<h&&v.total>v.rows) {
                int top=scroll_pixel(w->top,v.total,v.rows,track),size=scroll_thumb(v.total,v.rows,track);
                if(py<16)w->top--;
                else if(py>=h-16)w->top++;
                else if(py<16+top)w->top-=v.rows;
                else if(py>=16+top+size)w->top+=v.rows;
                else {scrolling=id;scroll_grab=py-16-top;}
                w->top=scroll_clamp(w->top,v.total,v.rows);if(id>=DOS1)dos_follow[id-DOS1]=w->top>=v.total-v.rows;return 0;
            }
        }
        if(id>=DOS1&&cy==w->y+w->h-3) {if(cx>=w->x+w->w-9&&cx<w->x+w->w-6)return 1001;if(cx>=w->x+w->w-5&&cx<w->x+w->w-2)return 1002;}
        if(id>=DOS1&&cy==w->y+w->h-2) {
            int col=cx-w->x;
            if(col>=2&&col<9)return 12;
            if(col>=10&&col<17)return 16;
            if(col>=18&&col<26)return 18;
            if(w->w>=35&&col>=27&&col<33)return 5;
        } else if(id==TASKS) {
            int row=cy-w->y-3;
            if(row>=0&&row<WINDOWS)w->selected=row;
            if(cy==w->y+w->h-2) {
                int col=cx-w->x;
                if(col>=2&&col<10)return 13;
                if(col>=11&&col<18)return 's';
                if(col>=19&&col<27)return 'r';
                if(w->w>=36&&col>=28&&col<34)return 'k';
                if(k386_resources_available()&&w->w>=66){
                    if(col>=38&&col<62&&(col-38)%8<7)return "plb"[(col-38)/8];
                }
            }
        } else if (id == LAUNCHER || id == FILES) {
            if(cy==w->y+w->h-2) {
                if(cx>=w->x+2&&cx<w->x+11)return id==LAUNCHER?'v':8;
                if(cx>=w->x+12&&cx<w->x+20)return 13;
                if(id==LAUNCHER&&w->w>=34&&cx>=w->x+22&&cx<w->x+30)return 11;
                if(id==FILES&&cx>=w->x+21&&cx<w->x+29)return 'v';
                if(id==FILES&&w->w>=39&&cx>=w->x+30&&cx<w->x+37)return 'f';
            }
            int row=cy-v.y,col=cx-v.x;
            if(row>=0&&row<v.rows*v.step&&col>=0&&col<v.cols) {
                if(icons(id)&&(row%4==3||col%14==13||col/14>=v.columns))return 0;
                if(!icons(id)&&row%v.step)return 0;
                int selected=(w->top+row/v.step)*v.columns+(icons(id)?col/14:0);
                if(selected<(id==LAUNCHER?LAUNCH_APPS:count)) {
                    w->selected=selected;
                    int previous_item=id==LAUNCHER?previous_app:previous_file;
                    u32 tick=id==LAUNCHER?app_click:last_click;
                    if((id==LAUNCHER&&!icon_view)||(previous_item==selected&&((now.ticks-tick)&0xffffff)<153600)) {
                        if(id==LAUNCHER)tool(selected,"");else file_open();
                    } else if(id==LAUNCHER){last_app=selected;app_click=now.ticks;}
                    else {last_file=selected;last_click=now.ticks;}
                }
            }
        } else if(id == CALCULATOR) {
            if(cy==w->y+w->h-2) {
                int col=cx-w->x;
                if(col>=2&&col<10)return 3;
                if(col>=11&&col<20)return 22;
                if(col>=21&&col<26)return '%';
            }
            int row=cy-w->y-6,col=cx-w->x-(w->w-32)/2;
            if(row>=0&&row<12&&!(row&1)&&col>=0&&col<32&&col%8<7)return calc_keys[row/2][col/8];
        } else if(id == CLOCK && cy==w->y+w->h-2) {
            int col=cx-w->x;
            if(col>=2&&col<12)return 't';
            if(col>=13&&col<21)return 3;
        } else if (id == MEMO && cy == w->y + w->h - 2) {
            int col = cx - w->x;
            if (col >= 2 && col < 11) save();
            else if (col >= 12 && col < 19) return 3;
            else if (col >= 20 && col < 26) return 24;
            else if (w->w >= 40 && col >= 27 && col < 35) return 22;
            else if (w->w >= 46 && col >= 36 && col < 43) return 1;
        }
        return 0;
    }
    if(enhanced&&now.y>=48&&now.y<112)for(int i=0;i<2;i++)
        if(now.x>=(58+i*11)*8&&now.x<(68+i*11)*8){focus(DOS1+i);break;}
    last_file = -1; return 0;
}
static int clock_update(int moving) {
    if(!windows[CLOCK].open || ((input_ticks()-clock_tick)&0xffffff)<153600) return 0;
    clock_tick=input_ticks();DeskClock next;clock_read(&next);
    int changed=next.second!=desk_clock.second||next.minute!=desk_clock.minute||next.hour!=desk_clock.hour||next.day!=desk_clock.day||next.month!=desk_clock.month||next.year!=desk_clock.year||next.valid!=desk_clock.valid;
    desk_clock=next;
    if(!changed||windows[CLOCK].hidden||confirm||moving||mode)return 0;
    if(panel_open){invalidate(CLOCK);scene_dirty=1;return 1;}
    Window *w=&windows[CLOCK];int x=w->x*8+w->ox+w->w*4-100,y=w->y*16+w->oy+48,above=0;
    for(int i=0;i<WINDOWS;i++) {
        int id=order[i];if(id==CLOCK){above=1;continue;}Window *other=&windows[id];
        if(above&&other->open&&!other->hidden&&x<other->x*8+other->ox+other->w*8+5&&x+200>other->x*8+other->ox&&y<other->y*16+other->oy+other->h*16+5&&y+136>other->y*16+other->oy){invalidate(CLOCK);scene_dirty=1;return 1;}
    }
    gfx_pointer_hide();gfx_target(1);gfx_origin(w->ox,w->oy);clock_draw(&desk_clock,twelve_hour,w->x*8,w->y*16,w->w*8);gfx_origin(0,0);gfx_present(x,y,200,136);
    Mouse now=mouse_position();gfx_pointer_show(now.x,now.y);return 0;
}
static void resources_update(int moving){
    if(active!=TASKS||!k386_resources_available()||moving||mode||confirm||panel_open||
       windows[TASKS].selected<DOS1||((input_ticks()-resource_tick)&0xffffff)<307200)return;
    Window *w=&windows[TASKS];int rows=w->h-15;if(rows<=0)return;if(rows>6)rows=6;
    resource_tick=input_ticks();gfx_pointer_hide();gfx_target(1);gfx_origin(w->ox,w->oy);
    gfx_rect((w->x+2)*8,(w->y+13)*16,(w->w-4)*8,rows*16,PAPER);
    gfx_clear_text(w->x+2,w->y+13,w->w-4,rows);draw_resources(w);gfx_origin(0,0);
    gfx_present((w->x+2)*8+w->ox,(w->y+13)*16+w->oy,(w->w-4)*8,rows*16);
    Mouse now=mouse_position();gfx_pointer_show(now.x,now.y);
}
static int dispatch_events(void) {
    unsigned slot;TaskEvent event;int redraw=0;
    for(int budget=0;budget<32&&task_next(&slot,&event);budget++) {
        int error=event.kind!=TASK_KEY&&event.kind!=TASK_TICK;
        if(!error&&event.kind==TASK_TICK)redraw|=clock_update(drag||scrolling>=0);
        else if(!error){int input=guest_input(slot,event.value);app_key(slot,event.value);redraw|=!input;}
        task_complete(slot,error);
    }
    return redraw;
}
int main(void) {
    p_init();
    u16 segment;__asm__ volatile("movw %%ds,%0":"=r"(segment));data_segment=segment;
    entries_segment=mem_alloc(MEMO_OFFSET+MEMO_CAPACITY);
    if(!entries_segment){p_output("Cannot allocate directory buffer\r\n");return 1;}
    if (p_cwd(home) < 0) { p_output("Cannot read working directory\r\n"); return 1; }
    fd_text(folder, home, sizeof folder);
    if (fd_path(note_path, home, "NOTE.TXT")) return 1;
    for (int i = 0; i < WINDOWS; i++) {
        windows[i] = (Window){3 + i * 4, 2 + i, 48, 16, 0, 0, 0, 0, 0, 0}; order[i] = i;
    }
    windows[CALCULATOR]=(Window){5,2,38,20,0,0,0,0,0,0};
    windows[CLOCK]=(Window){39,3,38,17,0,0,0,0,0,0};
    windows[TASKS]=(Window){8,2,48,18,0,0,0,0,0,0};
    windows[DOS1]=(Window){2,2,46,17,0,0,0,0,0,0};windows[DOS2]=(Window){32,5,46,17,0,0,0,0,0,0};
    enhanced=k386_probe();
    if(k386_resources_available())windows[TASKS]=(Window){3,1,74,22,0,0,0,0,0,0};
    calc_init(&calculator);clock_read(&desk_clock);clock_tick=input_ticks();
    load(); refresh(); focus(LAUNCHER); enter_graphics();
    damage(0,0,640,400);draw(); Mouse previous = mouse_read(); gfx_pointer_show(previous.x, previous.y);
    for (;;) {
        Mouse now = mouse_read(); int ready = key_ready(), redraw = 0, stop = 0;
        if(windows[CLOCK].open&&((input_ticks()-clock_tick)&0xffffff)>=153600)
            task_post(CLOCK,TASK_TICK,0);
        redraw|=dispatch_events();
        if(enhanced&&((input_ticks()-dos_tick)&0xffffff)>=3072) {
            dos_tick=input_ticks();unsigned changed=k386_tick();
            if(changed){for(int id=DOS1;id<=DOS2;id++)if((changed&(1u<<(id-DOS1)))&&windows[id].open&&!windows[id].hidden){
                if(dos_follow[id-DOS1]){int top=k386_word(id-DOS1,__builtin_offsetof(K386View,row))-(windows[id].h-7);windows[id].top=scroll_clamp(top,25,windows[id].h-6);}
                invalidate(id);scene_dirty=1;redraw=1;}}
        }
        int previous_active=active,previous_mode=mode,previous_confirm=confirm;
        Window before=active<0?(Window){0}:windows[active];
        int partial=!drag&&!mode&&!confirm&&!panel_open&&!redraw&&!scene_dirty;
        int panel_partial=panel_open&&!drag&&!mode&&!confirm&&!redraw&&!scene_dirty;
        if (clipboard.command) {
            int request = clipboard.command; clipboard.result = 0;
            if (request != 1 || confirm || mode || panel_open || memo_blocked) clipboard.result = 1;
            else { focus(MEMO); if (!paste_memo()) clipboard.result = 2; }
            clipboard.command = 0; clipboard.sequence++; redraw = 1;
        }
        if (now.x != previous.x || now.y != previous.y || now.buttons != previous.buttons || ready || redraw) {
            gfx_pointer_hide();
            for (int i = 0; i < 16; i++) {
                int key = mouse_event(now, previous.buttons, &redraw);
                if(key){stop=handle_key(key);redraw|=dispatch_events();if(stop)break;}
                previous = now;
                if (!mouse_queued()) break;
                now = mouse_read();
            }
            if (stop) break;
            if (ready) {
                redraw|=drag||scrolling>=0;
                drag = 0; scrolling=-1; gfx_drag_end(); live_drag = 0; last_file = last_app = last_title = -1;
                for(int i=0;i<16&&key_ready();i++){int key=key_read();redraw|=!guest_input(active,key);stop=handle_key(key);redraw|=dispatch_events();if(stop)break;}
                redraw|=scene_dirty;
                if (stop) break;
            }
            redraw|=dispatch_events();
            if (redraw) {
                if(previous_active>=0&&before.open&&!before.hidden)
                    damage(before.x*8+before.ox,before.y*16+before.oy,before.w*8+5,before.h*16+5);
                invalidate(active);
                if(previous_confirm||confirm)damage(136,144,373,133);
                if(preview_visible) {
                    damage(preview_x*8,preview_y*16,preview_w*8+8,preview_h*16+16);
                    scene_dirty=1;
                }
                preview_visible=0;
                if(panel_partial&&panel_open&&!confirm&&!scene_dirty) {
                    gfx_target(1);draw_panel();taskbar();present();
                } else if(partial&&active==previous_active&&active>=0&&!drag&&!mode&&!previous_mode&&!confirm&&!scene_dirty) {
                    gfx_target(1);draw_window(active);
                    taskbar();
                    present();
                } else draw();
            }
            if (drag == 1 && !live_drag) {
                Window *w = &windows[drag_id];
                if(gfx_colors()==256){compose(0);gfx_target(0);}
                live_drag = gfx_drag_begin(w->x * 8 + w->ox, w->y * 16 + w->oy, w->w * 8 + 5, w->h * 16 + 5) ? 1 : 2;
            }
            gfx_pointer_show(now.x, now.y);
        }
        resources_update(drag||scrolling>=0);
        previous = now; input_idle();
    }
    if(enhanced){k386_command(K386_END,0,0,0);k386_command(K386_END,1,0,0);}
    mem_free(entries_segment);clip_state(0); gfx_stop(); p_finish(); p_output("KDESK exited.\r\n"); return 0;
}
