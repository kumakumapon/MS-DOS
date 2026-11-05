#ifndef KGUI32_CLIENT_PROTOCOL_H
#define KGUI32_CLIENT_PROTOCOL_H
#include "../../k38632/elf.h"
#define KG_FEATURE 32u
#define KG_COMMANDS 40
#define KG_EVENTS 16
enum {KG_OPEN=1,KG_FRAME,KG_POLL,KG_CLOSE};
enum {KG_RECT=1,KG_TEXT,KG_BUTTON};
enum {KG_KEY=1,KG_MOUSE,KG_RESIZE,KG_FOCUS};
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
