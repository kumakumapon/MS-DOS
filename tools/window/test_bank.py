"""Inspect the real linked overlay layout and packaged DOS relocation contract."""
import subprocess,sys,struct
from pathlib import Path
elf,exe=sys.argv[1:]
symbols={name:int(value,16) for value,kind,name in (line.split() for line in subprocess.check_output(['nm','--defined-only',elf],text=True).splitlines())}
s=symbols;raw=Path(exe).read_bytes();payload=raw[struct.unpack_from('<H',raw,8)[0]*16:]
assert 0<s['__resident_end']<=s['__bank_base']<s['__code_end']<=65536
for name in ['_start','kdesk_boot','mem_copy','wt_draw','wt_file']:
 assert s[name]<s['__bank_base'],name+' must remain callable while a bank is replaced'
for name in ['main','tools_entry']:
 assert s['__bank_base']<=s[name]<s['__code_end']
assert s['__gui_size'] and s['__tools_size']
assert s['__gui_load']>=65536
assert s['__tools_load']>=s['__gui_load']+s['__gui_size']
assert s['__data_load']>=s['__tools_load']+s['__tools_size']
assert s['__mz_image_end']==len(payload)
assert s['__stack_top']<=65520 and s['__stack_top']-s['__bss_end']>=12288
assert struct.unpack_from('<H',payload,s['__mz_reloc_ds'])[0]*16==s['__data_load']
for name in ['gui','tools']:
 at=s['__'+name+'_load'];assert any(payload[at:at+s['__'+name+'_size']])
print('PASS resident trampoline, shared 16-bit bank bounds, distinct load images and MZ data relocation')
