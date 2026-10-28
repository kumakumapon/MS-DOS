"""Encode the original rectangle artwork as data to conserve 16-bit code space."""
import subprocess,tempfile,sys
from pathlib import Path
root=Path(__file__).resolve().parent
with tempfile.TemporaryDirectory() as temp:
 p=Path(temp);(p/'capture.c').write_text('''#include <stdio.h>
#include "utilities.h"
int gfx_colors(void){return 256;}
void input_poll(void){}
void gfx_rect(int x,int y,int w,int h,int c){printf("%d,%d,%d,%d,%d,",x,y,w,h,c);}
int main(void){for(int i=0;i<9;i++){printf("{");utility_icon(i,0,0);puts("255},");}return 0;}
''')
 subprocess.run(['cc','-I'+str(root),str(p/'capture.c'),str(root/'art_shapes.c'),'-o',str(p/'capture')],check=True)
 rows=subprocess.check_output([str(p/'capture')],text=True)
 lengths=[len(r.rstrip(',').strip('{}').split(',')) for r in rows.splitlines()]
 table='/* Generated from art_shapes.c by generate-art.py. */\nstatic const unsigned char icon_shapes[][{}]={{\n'.format(max(lengths))+rows+'};\n'
 if '--check' in sys.argv:
  if (root/'art_data.h').read_text()!=table:raise SystemExit('art_data.h is stale; run generate-art.py')
 else:(root/'art_data.h').write_text(table)
