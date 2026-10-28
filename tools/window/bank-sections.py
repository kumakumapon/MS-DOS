"""Keep code bank sections separate; retain ordinary data/relocation sections."""
import subprocess,sys
source,target,bank=sys.argv[1:]
sections=[]
for line in subprocess.check_output(['objdump','-h',source],text=True).splitlines():
 p=line.split()
 if len(p)>2 and p[1].startswith('.text'):
  sections+=['--rename-section',p[1]+'=.'+bank+p[1]]
subprocess.run(['objcopy',*sections,source,target],check=True)
