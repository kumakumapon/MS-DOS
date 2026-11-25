#ifndef KGUI32_SDK_H
#define KGUI32_SDK_H
#include "protocol.h"
#include "../../k38632/sdk/k32x.h"
int kg_open(const char *,unsigned,unsigned);
void kg_begin(void);
int kg_rect(int,int,int,int,u32);
int kg_text(int,int,const char *,u32);
int kg_text_double(int,int,const char *,u32);
unsigned kg_large_text(void);
int kg_button(u32,int,int,int,int,const char *);
int kg_button_color(u32,int,int,int,int,const char *,u32);
int kg_present(void);
int kg_poll(KgEvent *);
int kg_close(void);
int kg_date(unsigned *,unsigned *,unsigned *);
int kg_capabilities(void);
int kg_preferences_changed(void);
int kg_subscribe(unsigned);
/* Images are opaque, row-major RGB565 with up to 128 pixels per side.
 * Read into scratch and use the result only after all chunks succeed. */
int kg_note_document(const char *,unsigned,unsigned);
int kg_recent_volume(const char *);
int kg_clip_image_copy(const u16 *,unsigned,unsigned);
int kg_clip_image_paste(u16 *,unsigned,unsigned *,unsigned *);
int kg_clip_set(const char *,unsigned);
int kg_clip_get(char *,unsigned);
int kg_cancel_close(void);
int kg_edit_document(const char *,unsigned);
int kg_edit_status(void);
int kg_surface_create(unsigned,unsigned);
int kg_surface_create_extra(unsigned,unsigned);
int kg_surface_upload(unsigned,const u16 *,unsigned);
int kg_surface_destroy(unsigned);
int kg_bitmap(unsigned,int,int,unsigned,unsigned,unsigned,unsigned);
int kg_bitmap_scaled(unsigned,int,int,unsigned,unsigned,unsigned,unsigned,unsigned,unsigned);
int kg_bitmap_scaled_clip(unsigned,int,int,unsigned,unsigned,unsigned,unsigned,unsigned,unsigned,unsigned,unsigned,unsigned,unsigned);
unsigned kg_width(void);
unsigned kg_height(void);
#endif
