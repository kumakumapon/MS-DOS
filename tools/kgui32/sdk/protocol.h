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
#define KG_INPUT_LANGUAGE 32u
#define KG_INPUT_MENU 128u
#define KG_MENU_KEY 16384u
#define KG_CAP_LANGUAGE 64u
#define KG_CAP_SURFACES 128u
#define KG_LANGUAGE_JA 0x80000000u
#define KG_LARGE_TEXT 0x40000000u
#define KG_CAP_SUPPORTED (KG_CAP_ALL|KG_INPUT_MODIFIERS)
#define KG_MOD_SHIFT 1u
#define KG_MOD_CTRL 2u
#define KG_MOD_ALT 4u
#define KG_INPUT_FLAGS (KG_INPUT_MOTION|KG_INPUT_TAB)
enum {KG_OPEN=1,KG_FRAME,KG_POLL,KG_CLOSE,KG_CAPABILITIES,KG_SUBSCRIBE,KG_CLIP_SET,KG_CLIP_GET,KG_CANCEL_CLOSE,KG_EDIT_DOCUMENT,KG_EDIT_STATUS,KG_SURFACE_CREATE,KG_SURFACE_UPLOAD,KG_SURFACE_STATUS,KG_SURFACE_DESTROY,KG_DATE,KG_PREFERENCES_CHANGED,KG_IMAGE_COPY_BEGIN,KG_IMAGE_COPY_DATA,KG_IMAGE_PASTE_INFO,KG_IMAGE_PASTE_DATA,KG_RECENT_DOCUMENT,KG_RECENT_MOVE,KG_IME_REPORT,KG_NOTIFY,KG_UI_FORMAT};
enum {KG_NOTICE_SAVED=1,KG_NOTICE_SAVE_FAILED,KG_NOTICE_COPIED,KG_NOTICE_COPY_FAILED,KG_NOTICE_CANCELLED};
enum {KG_IME_OFF=1,KG_IME_ROMAJI,KG_IME_KANA,KG_IME_COMPOSING=4,KG_IME_CANDIDATES=8};
enum {KG_RECT=1,KG_TEXT,KG_BUTTON,KG_BITMAP,KG_BITMAP_SCALE,KG_TEXT_DOUBLE};
enum {KG_KEY=1,KG_MOUSE,KG_RESIZE,KG_FOCUS,KG_POINTER,KG_QUIT};
typedef struct {u32 op;int x,y,w,h;u32 color,id;char text[48];} KgDraw;
typedef struct {u32 type;int x,y;u32 key,width,height,seconds,control;} KgEvent;
typedef struct {
 u32 version,size,operation,slot,generation,token,revision,width,height,count;
 char title[32];KgDraw draw[KG_COMMANDS];KgEvent event;
} KgRequest;
/* The fixed public struct stays 3144 bytes. Status has no draw payload;
 * replies need the event/draw tail only for poll and clipboard reads. */
#define KG_REQUEST_HEADER 72u
#define KG_CLIP_IMAGE_SIDE 128u
#define KG_CLIP_IMAGE_BYTES (2u*KG_CLIP_IMAGE_SIDE*KG_CLIP_IMAGE_SIDE)
_Static_assert(__builtin_offsetof(KgRequest,draw)==KG_REQUEST_HEADER,"compact request header preserves draw offset");
#define KG_RECENT_COUNT 6u
#define KG_RECENT_PATH 40u
typedef struct {u32 serial,volume,bitmap;char path[KG_RECENT_PATH],destination[KG_RECENT_PATH];} KgRecentDocument;
typedef struct {u16 images[2][KG_CLIP_IMAGE_BYTES/2];KgRecentDocument documents[KG_RECENT_COUNT];} KgDesktopShared;
static inline unsigned kg_request_bytes(unsigned op){return (op==KG_SURFACE_STATUS||op==KG_PREFERENCES_CHANGED||op==KG_IMAGE_COPY_BEGIN||op==KG_IMAGE_PASTE_INFO||op==KG_IMAGE_PASTE_DATA||(op>=KG_IME_REPORT&&op<=KG_UI_FORMAT))?KG_REQUEST_HEADER:sizeof(KgRequest);}
static inline unsigned kg_reply_bytes(unsigned op){return op==KG_POLL||op==KG_CLIP_GET||op==KG_IMAGE_PASTE_DATA?sizeof(KgRequest):KG_REQUEST_HEADER;}
static inline void kg_transport_copy(void *to,const void *from,unsigned bytes){
 unsigned words=bytes/4,tail=bytes&3;unsigned char *d=to;const unsigned char *s=from;
 __asm__ volatile("cld; rep movsl":"+D"(d),"+S"(s),"+c"(words)::"memory","cc");
 __asm__ volatile("rep movsb":"+D"(d),"+S"(s),"+c"(tail)::"memory","cc");
}
/* Native-only envelope. Identity comes from K32HOST, never a user request. */
typedef struct {u32 version,size,slot,generation,request,bytes,operation;int result;} KgPacket;
_Static_assert(sizeof(KgDraw)==76,"graphical draw ABI");
_Static_assert(sizeof(KgEvent)==32,"graphical event ABI");
_Static_assert(sizeof(KgRequest)==3144,"graphical request ABI");
_Static_assert(sizeof(KgPacket)==32,"native envelope ABI");
#endif
