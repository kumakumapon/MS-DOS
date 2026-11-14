"""Original per-module Shift-JIS labels; never bundle a full table per app."""
import json,re,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parent
SOURCES={name:ROOT/path for name,path in {
 'desktop':'desktop.c','editor':'apps/editor.c','files':'apps/files.c',
 'image':'apps/image.c','calculator':'apps/calculator.c','clock':'apps/clock.c',
 'picker':'sdk/picker.c','recovery':'sdk/recovery.c','widgets':'sdk/widgets-draw.c'}.items()}
def literal(text):return '"'+''.join('\\x%02x'%b for b in text.encode('shift_jis'))+'"'
def button_bounds(text,rows):
 for match in re.finditer(r'\b(kg_button|button)\(',text):
  args=[];start=at=match.end();depth=1;quote=None;escaped=False
  while depth and at<len(text):
   c=text[at]
   if quote:
    if escaped:escaped=False
    elif c=='\\':escaped=True
    elif c==quote:quote=None
   elif c in ('"',"'"):quote=c
   elif c=='(':depth+=1
   elif c==')':
    depth-=1
    if not depth:args.append(text[start:at].strip())
   elif c==',' and depth==1:args.append(text[start:at].strip());start=at+1
   at+=1
  expected,width,label=(6,3,5) if match[1]=='kg_button' else (4,2,3)
  if len(args)!=expected or not args[width].isdigit():continue
  limit=(int(args[width])-4)//8
  for literal in re.findall(r'"(?:\\.|[^"\\])*"',args[label]):
   key=json.loads(literal)
   if key in rows:assert len(rows[key].encode('shift_jis'))<=limit,(key,args[width],rows[key])
def build(dest):
 rows={}
 for line in (ROOT/'ui/strings.tsv').read_text().splitlines():
  if not line or line.startswith('#'):continue
  a,b=line.split('\t');assert a not in rows and len(b.encode('shift_jis'))<=47,a
  rows[a]=b
 dest.mkdir(parents=True,exist_ok=True)
 for name,path in SOURCES.items():
  text=path.read_text()
  button_bounds(text,rows)
  keys=[key for key in rows if json.dumps(key) in text or name=='desktop' and key in ('Files','Editor','Images / BMP24','Calculator','Clock')]
  assert keys,name
  output=['/* Generated original bounded UI labels. */','#include "../sdk/i18n.h"','static const KuPair ku_labels[]={']
  output += ['{%s,%s},'%(json.dumps(key),literal(rows[key])) for key in keys]
  output += ['};','#define KU(s) ku_lookup(ku_labels,sizeof ku_labels/sizeof *ku_labels,(s))',
    '#define kg_label(x,y,s,c) kg_text((x),(y),KU(s),(c))',
    '#define kw_label(x,y,s,n,c) kw_text((x),(y),KU(s),(n),(c))',
    '#define kg_button(id,x,y,w,h,s) kg_button((id),(x),(y),(w),(h),KU(s))']
  if name!='widgets':output += ['#define kw_draw_dialog(w,h,t,s) kw_draw_dialog((w),(h),KU(t),KU(s))']
  (dest/('ui-'+name+'.h')).write_text('\n'.join(output)+'\n')
 print('UI: %d original labels, %d module tables'%(len(rows),len(SOURCES)))
if __name__=='__main__':build(Path(sys.argv[1]))
