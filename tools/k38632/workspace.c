/* Original SDK4 transaction policy; same implementation in DOS and fault tests. */
#include "services.h"
#include "workspace.h"
int k32_work_path_at(unsigned drive,const char *name,char path[24]){
 if(drive>25)return -22;
 unsigned n=0;while(n<16&&name[n])n++;
 char root[16];if(n>12||!k32_filename(name,n,root))return -22;
 if(n<5||root[n-1]!='.'||root[n]!='T'||root[n+1]!='X'||root[n+2]!='T')return -22;
 const char *prefix="A:\\WORK\\";for(unsigned i=0;i<8;i++)path[i]=prefix[i];
 path[0]='A'+drive;
 for(unsigned i=0;i<=n;i++)path[8+i]=root[3+i];
 return 0;
}
int k32_work_path(const char *name,char path[24]){return k32_work_path_at(0,name,path);}
int k32_work_space(K32WorkInfo *out,unsigned spc,unsigned free,unsigned bps,unsigned total){
 if(!spc||spc>128||(spc&(spc-1))||bps<128||bps>4096||(bps&(bps-1))||free>total||!total)return -5;
 unsigned cluster=spc*bps;if(cluster>65536||total>~0u/cluster)return -75;
 out->sector_bytes=bps;out->cluster_sectors=spc;out->free_clusters=free;out->total_clusters=total;
 out->free_bytes=free*cluster;out->total_bytes=total*cluster;return 0;
}
int k32_work_request(const K32Work *r){
 if(r->version!=1||r->size!=sizeof *r||r->flags||r->reserved[0]||r->reserved[1]||r->reserved[2])return -22;
 if(r->operation<1||r->operation>5)return -38;
 if(r->operation==K32_WORK_INFO){
  if(r->bytes!=sizeof(K32WorkInfo)||r->exists||r->length||r->hash)return -22;
  for(unsigned i=0;i<sizeof r->name;i++)if(r->name[i])return -22;
  return 0;
 }
 if(r->operation==K32_WORK_FIRST)return r->bytes==64?0:-22;
 char path[24];if(k32_work_path(r->name,path))return -22;
 if(r->operation==K32_WORK_SAVE&&(r->bytes>K32_WORK_LIMIT||r->exists>1||r->length>K32_WORK_LIMIT))return -22;
 return 0;
}
int k32_work_save_at(unsigned drive,const K32Work *r,const K32WorkIO *io,unsigned char buffer[4096]){
 char target[24],temp[24],backup[24];int e=k32_work_request(r);
 if(e||r->operation!=K32_WORK_SAVE)return e?e:-22;
 if(k32_work_path_at(drive,r->name,target))return -22;
 unsigned dot=0;
 for(unsigned i=0;i<24;i++){temp[i]=backup[i]=target[i];if(target[i]=='.')dot=i;if(!target[i])break;}
 temp[dot+1]='T';temp[dot+2]='M';temp[dot+3]='P';
 backup[dot+1]='B';backup[dot+2]='A';backup[dot+3]='K';
 /* Validate the entire user document before any filesystem mutation. */
 for(unsigned at=0;at<r->bytes;){unsigned n=r->bytes-at;if(n>4096)n=4096;
  if((e=io->copy(r->data+at,buffer,n)))return e;
  for(unsigned i=0;i<n;i++)if(buffer[i]!='\n'&&buffer[i]!='\t'&&(buffer[i]<32||buffer[i]>126))return -22;
  at+=n;
 }
 unsigned length=0,hash=0;int exists=io->stat(target,&length,&hash);
 if(exists<0)return exists;
 if((unsigned)exists!=r->exists||(exists&&(length!=r->length||hash!=r->hash)))return -116;
 /* stat probes both reserved names, refusing foreign or interrupted files. */
 int b=io->stat(backup,&length,&hash);if(b<0&&b!=-27)return b;if(b)return -117;
 if(io->space){unsigned free,cluster;e=io->space(&free,&cluster);if(e)return e;
  if(!cluster||cluster>65536||(cluster&(cluster-1)))return -5;
  unsigned required=(r->bytes+cluster-1)/cluster*cluster;if(required>free)return -28;
 }
 int h=io->create(temp);if(h<0)return h;
 e=0;
 for(unsigned at=0;at<r->bytes;){unsigned n=r->bytes-at;if(n>4096)n=4096;
  if((e=io->copy(r->data+at,buffer,n)))break;
  int written=io->write(h,buffer,n);if(written!=(int)n){e=written<0?written:-28;break;}at+=n;
 }
 int closed=io->close(h);if(!e&&closed)e=closed;
 if(e){if(io->remove(temp))return -117;return e;}
 if(exists&&(e=io->rename(target,backup))){if(io->remove(temp))return -117;return e;}
 e=io->rename(temp,target);
 if(e){int restore=exists?io->rename(backup,target):0;if(restore)return -117;if(io->remove(temp))return -117;return e;}
 return exists&&io->remove(backup)?1:0;
}
int k32_work_save(const K32Work *r,const K32WorkIO *io,unsigned char buffer[4096]){return k32_work_save_at(0,r,io,buffer);}
