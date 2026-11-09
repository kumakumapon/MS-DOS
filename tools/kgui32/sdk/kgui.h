#ifndef KGUI32_SDK_H
#define KGUI32_SDK_H
#include "protocol.h"
#include "../../k38632/sdk/k32x.h"
int kg_open(const char *,unsigned,unsigned);
void kg_begin(void);
int kg_rect(int,int,int,int,u32);
int kg_text(int,int,const char *,u32);
int kg_button(u32,int,int,int,int,const char *);
int kg_present(void);
int kg_poll(KgEvent *);
int kg_close(void);
int kg_capabilities(void);
int kg_subscribe(unsigned);
int kg_clip_set(const char *,unsigned);
int kg_clip_get(char *,unsigned);
int kg_cancel_close(void);
int kg_edit_document(const char *,unsigned);
int kg_edit_status(void);
unsigned kg_width(void);
unsigned kg_height(void);
#endif
