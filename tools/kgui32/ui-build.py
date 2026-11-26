"""Original per-module Shift-JIS labels; never bundle a full table per app."""
import json,re,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parent
SOURCES={name:ROOT/path for name,path in {
 'desktop':'desktop.c','gallery':'gallery.c','editor':'apps/editor.c','files':'apps/files.c',
 'files-manager':'apps/files-manager.c','settings':'apps/settings.c','words':'apps/words.c','image':'apps/image.c','calculator':'apps/calculator.c','clock':'apps/clock.c','calendar':'apps/calendar.c',
 'picker':'sdk/picker.c','recovery':'sdk/recovery.c','widgets':'sdk/widgets-draw.c','menu':'sdk/menu-draw.c'}.items()}
def literal(text):return '"'+''.join('\\x%02x'%b for b in text.encode('shift_jis'))+'"'
STRING=r'"(?:\\.|[^"\\])*"'

def calls(text,names):
 for match in re.finditer(r'\b('+'|'.join(names)+r')\s*\(',text):
  args=[];start=at=match.end();depth=1;quote=None;escaped=False
  while depth and at<len(text):
   c=text[at]
   if quote:
    if escaped:escaped=False
    elif c=='\\':escaped=True
    elif c==quote:quote=None
   elif c in ('"',"'"):quote=c
   elif c in '({[':depth+=1
   elif c in ')}]':
    depth-=1
    if not depth:args.append(text[start:at].strip())
   elif c==',' and depth==1:args.append(text[start:at].strip());start=at+1
   at+=1
  yield match[1],args

def button_bounds(text,rows,cell_units=False):
 for name,args in calls(text,('kg_button','button')):
  expected,width,label=(6,3,5) if name=='kg_button' else (4,2,3)
  if len(args)!=expected or not args[width].isdigit():continue
  limit=int(args[width])-2 if cell_units else (int(args[width])-4)//8
  for key in map(json.loads,re.findall(STRING,args[label])):
   if key in rows:assert len(rows[key].encode('shift_jis'))<=limit,(key,args[width],rows[key])

def ui_literals(text, name):
 labels={'KU':(0,), 'ui_label':(2,), 'kg_label':(2,), 'kw_label':(2,),
         'kg_button':(5,), 'kw_draw_dialog':(2,3)}
 if name=='desktop':labels['button']=(3,)
 if name=='files-manager':labels['button']=(2,)
 if name=='settings':labels.update(button=(4,),preference_button=(4,))
 if name=='image':labels['image_button']=(5,)
 keys=set()
 for call,args in calls(text,labels):
  for i in labels[call]:
   if i<len(args):keys.update(map(json.loads,re.findall(STRING,args[i])))
 for block in re.findall(r'\bKmItem\s+\w+\[\s*\]\s*=\s*\{(.*?)\};',text,re.S):
  keys.update(map(json.loads,re.findall(r'\{\s*('+STRING+r')\s*,',block)))
 return keys-{''}

def build(dest,window=False):
 rows={}
 for line in (ROOT/'ui/strings.tsv').read_text().splitlines():
  if not line or line.startswith('#'):continue
  a,b=line.split('\t');assert a not in rows and len(b.encode('shift_jis'))<=47,a
  rows[a]=b
 sources={'window':ROOT/'../window/window.c'} if window else SOURCES
 missing=[]
 modules={}
 for name,path in sources.items():
  text=path.read_text()
  if name=='desktop':text+='\n'+(ROOT/'shell-ui.c').read_text()
  if name in ('editor','words'):text+='\n'+(ROOT/'sdk/ime.c').read_text()
  if not window:missing.extend(f'{path.relative_to(ROOT)}: {key}' for key in sorted(ui_literals(text,name)-rows.keys()))
  button_bounds(text,rows,cell_units=window)
  modules[name]=[key for key in rows if json.dumps(key) in text or name=='desktop' and key in ('Files','Editor','Images / BMP24','Images / BMP','User dictionary','Calculator','Clock')]
 if missing:raise SystemExit('Missing Japanese UI labels:\n'+'\n'.join(missing))
 dest.mkdir(parents=True,exist_ok=True)
 for name,keys in modules.items():
  assert keys,name
  header='i18n.h' if window else '../sdk/i18n.h'
  output=['/* Generated original bounded UI labels. */',f'#include "{header}"','static const KuPair ku_labels[]={']
  output += ['{%s,%s},'%(json.dumps(key),literal(rows[key])) for key in keys]
  output += ['};','#define KU(s) ku_lookup(ku_labels,sizeof ku_labels/sizeof *ku_labels,(s))',
    '#define kg_label(x,y,s,c) kg_text((x),(y),KU(s),(c))',
    '#define kw_label(x,y,s,n,c) kw_text((x),(y),KU(s),(n),(c))',
    '#define kg_button(id,x,y,w,h,s) kg_button((id),(x),(y),(w),(h),KU(s))']
  if name!='widgets':output += ['#define kw_draw_dialog(w,h,t,s) kw_draw_dialog((w),(h),KU(t),KU(s))']
  (dest/('ui-'+name+'.h')).write_text('\n'.join(output)+'\n')
 print('UI: %d original labels, %d module tables'%(len(rows),len(sources)))
if __name__=='__main__':build(Path(sys.argv[1]),sys.argv[2:]==['window'])
