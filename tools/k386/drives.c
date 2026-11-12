#include "drives.h"
#include "../filer/filer.h"
static unsigned upper(unsigned c){return c>='a'&&c<='z'?c-32:c;}
void k386_drives_init(K386Drives *d,unsigned hdd){
 *d=(K386Drives){0};d->hdd=hdd<26?hdd:K386_NO_HDD;
 fd_text(d->cwd[0],"A:\\",68);if(d->hdd>0&&d->hdd<26){fd_text(d->cwd[1],"A:\\",68);d->cwd[1][0]='A'+d->hdd;}
}
int k386_drive_slot(const K386Drives *d,unsigned drive){return drive==0?0:d->hdd<26&&drive==d->hdd?1:-1;}
int k386_drive_path(const K386Drives *d,const char *input,char out[128],int wild){
 char name[128];unsigned drive=d->current;int n=0,last=0;
 if(!input[0])return 3;
 if((input[0]=='\\'||input[0]=='/')&&(input[1]=='\\'||input[1]=='/'))return 3;
 if(input[1]==':'){unsigned letter=upper((unsigned char)input[0]);if(letter<'A'||letter>'Z')return 15;drive=letter-'A';}
 int slot=k386_drive_slot(d,drive);if(slot<0)return 15;
 if(input[1]==':'&&input[2]!='\\'&&input[2]!='/'){input+=2;if(!*input)input=".";}
 for(;input[n]&&n<127;n++){char c=input[n];if(c=='\\'||c=='/')last=n+1;name[n]=(c=='*'||c=='?')?'X':c;}
 if(input[n]||!n)return 3;
 name[n]=0;
 for(int i=0;i<n;i++)if((input[i]=='*'||input[i]=='?')&&(!wild||i<last))return 3;
 if(fd_path(out,d->cwd[slot][0]?d->cwd[slot]:"A:\\",name)||out[0]!=(char)('A'+drive))return 3;
 for(int i=3;out[i];){char stem[9];int at=0;
  while(out[i]&&out[i]!='\\'){if(out[i]=='.'){while(out[i]&&out[i]!='\\')i++;break;}if(at<8)stem[at++]=out[i];i++;}
  stem[at]=0;if(!fd_cmp(stem,"COM0")||!fd_cmp(stem,"LPT0"))return 5;if(out[i])i++;
 }
 if(wild){int at=fd_len(out)-(n-last);for(int i=last;i<n;i++)if(input[i]=='*'||input[i]=='?')out[at+i-last]=input[i];}
 return 0;
}
int k386_drive_cwd(K386Drives *d,const char *path){
 int slot=k386_drive_slot(d,(unsigned)(path[0]-'A'));if(slot<0)return 15;
 unsigned length=fd_len(path);if(length<3||length>66||path[1]!=':'||path[2]!='\\')return 3;
 fd_text(d->cwd[slot],path,68);return 0;
}
