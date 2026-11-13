#ifndef KGUI32_CLIENT_PROTOCOL_H
#define KGUI32_CLIENT_PROTOCOL_H
#include "../../k38632/elf.h"
#define KG_FEATURE 32u
#define KG_COMMANDS 40
#define KG_EVENTS 16
#define KG_INPUT_MOTION 1u
#define KG_INPUT_TAB 2u
#define KG_INPUT_CLOSE 4u
#define KG_INPUT_KEYS 8u
#define KG_CAP_ALL 15u
#define KG_INPUT_MODIFIERS 16u
#define KG_CAP_BITMAP 32u
#define KG_CAP_SUPPORTED (KG_CAP_ALL|KG_INPUT_MODIFIERS)
#define KG_MOD_SHIFT 1u
#define KG_MOD_CTRL 2u
#define KG_MOD_ALT 4u
#define KG_INPUT_FLAGS (KG_INPUT_MOTION|KG_INPUT_TAB)
enum {KG_OPEN=1,KG_FRAME,KG_POLL,KG_CLOSE,KG_CAPABILITIES,KG_SUBSCRIBE,KG_CLIP_SET,KG_CLIP_GET,KG_CANCEL_CLOSE,KG_EDIT_DOCUMENT,KG_EDIT_STATUS,KG_SURFACE_CREATE,KG_SURFACE_UPLOAD,KG_SURFACE_STATUS,KG_SURFACE_DESTROY};
enum {KG_RECT=1,KG_TEXT,KG_BUTTON,KG_BITMAP};
enum {KG_KEY=1,KG_MOUSE,KG_RESIZE,KG_FOCUS,KG_POINTER,KG_QUIT};
typedef struct {u32 op;int x,y,w,h;u32 color,id;char text[48];} KgDraw;
typedef struct {u32 type;int x,y;u32 key,width,height,seconds,control;} KgEvent;
typedef struct {
 u32 version,size,operation,slot,generation,token,revision,width,height,count;
 char title[32];KgDraw draw[KG_COMMANDS];KgEvent event;
} KgRequest;
/* Native-only envelope. Identity comes from K32HOST, never a user request. */
typedef struct {u32 version,size,slot,generation,request,bytes,operation;int result;} KgPacket;
_Static_assert(sizeof(KgDraw)==76,"graphical draw ABI");
_Static_assert(sizeof(KgEvent)==32,"graphical event ABI");
_Static_assert(sizeof(KgRequest)==3144,"graphical request ABI");
_Static_assert(sizeof(KgPacket)==32,"native envelope ABI");
#endif
