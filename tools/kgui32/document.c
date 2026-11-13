#include "document.h"
#include "../k38632/services.h"
int kg_document_command(unsigned work,const char *name,char out[64]){
 if(work>1)return -22;
 unsigned n=0;while(n<13&&name[n])n++;
 char file[16];if(!k32_filename(name,n,file)||n<5||file[n-1]!='.')return -22;
 int bitmap=file[n]=='B'&&file[n+1]=='M'&&file[n+2]=='P';
 if((bitmap&&work)||(!bitmap&&(file[n]!='T'||file[n+1]!='X'||file[n+2]!='T')))return -22;
 const char *prefix=bitmap?"KGIMAGE.EXE root ":work?"KGEDIT.EXE work ":"KGEDIT.EXE root ";unsigned at=0;
 for(;prefix[at];at++)out[at]=prefix[at];
 for(unsigned i=3;file[i];i++)out[at++]=file[i];
 out[at]=0;return 0;
}
