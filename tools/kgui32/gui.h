#ifndef KGUI32_PROTOCOL_H
#define KGUI32_PROTOCOL_H
#include "../k38632/elf.h"
enum {GUI_INFO=64,GUI_EVENT,GUI_PRESENT,GUI_FONT,GUI_PROOF,GUI_SESSION,GUI_SNAPSHOT,GUI_CLIENT_VIEW,GUI_CLIENT_EVENT};
enum {GS_CREATE=1,GS_END,GS_PAUSE,GS_RESUME,GS_KEY,GS_RESTART,GS_FOCUS=8};
typedef struct {u32 version,size,slot,generation,operation,key;char path[128];} GuiSessionCommand;
typedef struct {
 u32 version,size,slot,generation,state,kind,exit_code,fault,counter,column,row,keys,cpu_ticks,runs,owned,text_revision;
 char program[16],screen[2000];
} GuiSessionView;
_Static_assert(sizeof(GuiSessionCommand)==152,"session command ABI");
_Static_assert(sizeof(GuiSessionView)==2080,"session view ABI");
/* GUI_SNAPSHOT v2 is a fixed read-only 80x25 PC-98 text/attribute snapshot.
 * The dedicated desktop owns the destination. Transport chunks remain <=1024. */
typedef struct {
 u32 version,size,slot,generation,revision,state,column,row;
 u16 codes[2000],attributes[2000];
} GuiTerminalView;
_Static_assert(sizeof(GuiTerminalView)==8032,"terminal snapshot ABI v2");
enum {GUI_MOTION=1,GUI_BUTTON=2,GUI_KEY=3,GUI_TIMER=4};
typedef struct {u32 magic,version,size,width,height,bpp,pitch,features,colors[16];} GuiInfo;
typedef struct {u32 sequence,type,x,y,buttons,key,ticks,states[2],counters[2],lost;} GuiEvent;
typedef struct {u32 pixels,x,y,width,height,pitch;} GuiPresent;
/* GUI_FONT accepts this bounded in/out request in addition to the original
 * 1520-byte ASCII table. Only the dedicated desktop can request CG-ROM data. */
typedef struct {u32 version,size,code,columns;u8 bitmap[32];} GuiGlyph;
_Static_assert(sizeof(GuiGlyph)==48,"CG-ROM glyph ABI");
/* Debugger producer, not an application interface. busy/ready guard the native
 * record. CPU samples are produced by actual protected execution. */
typedef struct {
 char signature[8];u16 version,size,busy,ready;
 u32 owner,cookie,state,error,width,height,bpp,presents,events,rejected,unknown,
     graphics,runtime,appmask,quanta,rect[4],input_lost,owned_workers,
     worker_generation[2],worker_counter[2],worker_state[2],largest_before,largest_after,
     scratch,tables,framebuffer,last_key,completed,reserved[26];
} GuiDriverRecord;
_Static_assert(sizeof(GuiInfo)==96,"GUI info ABI");
_Static_assert(sizeof(GuiEvent)==48,"GUI event ABI");
_Static_assert(sizeof(GuiDriverRecord)==256,"GUI debugger ABI");
#endif
