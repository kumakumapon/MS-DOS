#include "io.h"
#include "services.h"
int k32_io_path(unsigned drive,const char *name,unsigned variant,char path[24]){
 if(drive>25||variant>2)return -22;
 unsigned n=0;while(n<16&&name[n])n++;char root[16];if(!k32_filename(name,n,root))return -22;
 unsigned dot=0;for(unsigned i=3;root[i];i++)if(root[i]=='.')dot=i;
 if(!dot)return -22;
 const char *extension=root+dot+1;
 if((extension[0]=='T'&&extension[1]=='M'&&extension[2]=='P'&&!extension[3])||(extension[0]=='B'&&extension[1]=='A'&&extension[2]=='K'&&!extension[3]))return -22;
 const char *prefix="A:\\WORK\\";for(unsigned i=0;i<8;i++)path[i]=prefix[i];path[0]='A'+drive;
 for(unsigned i=0;i<=n;i++)path[8+i]=root[3+i];
 if(variant){unsigned at=8+dot-3;const char *ext=variant==1?"TMP":"BAK";for(unsigned i=0;i<4;i++)path[at+1+i]=ext[i];}
 return 0;
}
int k32_io_validate(const K32Io *r){
 if(r->version!=1||r->size!=sizeof *r||r->reserved||r->operation<1||r->operation>K32_IO_RECOVER)return -22;
 unsigned maxflags=r->operation==K32_IO_OPEN||r->operation==K32_IO_RECOVER?2:r->operation==K32_IO_ABORT?1:0;
 if(r->flags>maxflags||(r->operation==K32_IO_RECOVER&&!r->flags))return -22;
 if((r->operation==K32_IO_READ||r->operation==K32_IO_WRITE)&&r->bytes>4096)return -22;
 if(r->operation==K32_IO_BEGIN||r->operation==K32_IO_RECOVER){if(r->exists>1||r->length>K32_IO_LIMIT||(!r->exists&&(r->length||r->hash)))return -22;}
 if(r->operation==K32_IO_RECOVER&&(r->bytes<1||r->bytes>12||!r->exists))return -22;
 if(r->operation==K32_IO_STAT||r->operation==K32_IO_OPEN||r->operation==K32_IO_BEGIN||r->operation==K32_IO_RECOVER){char path[24];return k32_io_path(0,r->name,0,path);}
 return 0;
}
int k32_io_publish(const char *target,const char *temporary,const char *backup,unsigned existed,const K32IoPublish *io){
 int e;if(existed&&(e=io->rename(target,backup)))return e;
 e=io->rename(temporary,target);
 if(e){if(existed&&io->rename(backup,target))return -117;return e;}
 return existed&&io->remove(backup)?1:0;
}
