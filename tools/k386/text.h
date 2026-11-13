/* Original PC-98 80x25 text policy. Callbacks address PRIVATE session TVRAM. */
#ifndef K386_TEXT_H
#define K386_TEXT_H
#define K98_COLUMNS 80u
#define K98_ROWS 25u
#define K98_CELLS 2000u
#define K98_WHITE 0xe1u
typedef struct {unsigned short column,row,attribute,pending,escape,which,parameter[4],saved_column,saved_row;} K98Text;
typedef struct {
 void *context;
 unsigned (*get)(void *,unsigned,unsigned);
 void (*set)(void *,unsigned,unsigned,unsigned);
 void (*fill)(void *,unsigned,unsigned,unsigned,unsigned);
 void (*scroll)(void *,unsigned);
} K98Surface;
void k98_reset(K98Text *);
void k98_output(K98Text *,unsigned,const K98Surface *);
unsigned k98_sjis(unsigned);
unsigned k98_glyph(const unsigned short *,unsigned,unsigned *);
void k98_row(const unsigned short *,unsigned short *,unsigned char *);
unsigned k98_ink(unsigned,unsigned,unsigned,unsigned,unsigned);
unsigned k98_rgb(unsigned);
#endif
