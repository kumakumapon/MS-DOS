#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "shell.h"
int main(void){KpShell p;assert(!kp_shell_parse("  ktool32.exe echo \"a | b\" | ktool32.exe upper > out.txt < in.txt",&p));assert(p.count==2&&!strcmp(p.command[0],"KTOOL32.EXE echo \"a | b\"")&&!strcmp(p.command[1],"KTOOL32.EXE upper")&&!strcmp(p.input,"IN.TXT")&&!strcmp(p.output,"OUT.TXT"));
 const char *bad[]={"","| X.EXE","X.EXE |","X.EXE || Y.EXE","X.EXE | Y.EXE | Z.EXE","X.EXE >","X.EXE > A.TXT > B.TXT","X.EXE < ../A.TXT","X.EXE > A:B.TXT","X.EXE \"oops","X.EXE & Y.EXE","X.EXE > LONGER123.TXT"};for(unsigned i=0;i<sizeof bad/sizeof *bad;i++)assert(kp_shell_parse(bad[i],&p)==-22);
 char longline[129];memset(longline,'x',128);longline[128]=0;assert(kp_shell_parse(longline,&p)==-22);puts("bounded shell quoting, pipelines and Work redirection parser PASS");}
