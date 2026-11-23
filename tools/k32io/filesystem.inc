/* Work path validation and DOS operations used only by the resident broker. */
static int work_component(const char *name,unsigned n,char out[16]){
 if(!k32_filename(name,n,out))return 0;
 const char *base=out+3,*ext=0;
 for(const char *p=base;*p;p++)if(*p=='.')ext=p+1;
 return !same(base,"KTRASH")&&(!ext||(!same(ext,"TMP")&&!same(ext,"BAK")));
}

static int work_path(const Owner *o,const char *name,int trash,int directory,char out[DOS_PATH]){
 const char *prefix=trash?"A:\\WORK\\KTRASH":"A:\\WORK";
 unsigned n=trash?14:7,at=0;
 copy(out,prefix,n+1);
 out[0]='A'+o->drive;
 if(!name[0])return directory?0:-22;
 for(unsigned depth=0;depth<3;depth++){
  unsigned start=at;
  while(at<K32_FS_PATH&&name[at]&&name[at]!='\\')at++;
  char component[16];
  if(at==K32_FS_PATH||!work_component(name+start,at-start,component))return -22;
  out[n++]='\\';
  copy(out+n,component+3,at-start+1);
  n+=at-start;
  if(!name[at])return 0;
  at++;
 }
 return -22;
}

static void sibling(const char *path,const char *ext,char out[DOS_PATH]){
 unsigned end=0,dot=0;
 while(path[end]){
  if(path[end]=='\\')dot=0;
  if(path[end]=='.')dot=end;
  end++;
 }
 if(dot)end=dot;
 copy(out,path,end);
 out[end]='.';
 copy(out+end+1,ext,4);
}

static int pending(const char *path){
 char part[DOS_PATH];
 sibling(path,"TMP",part);
 for(unsigned i=0;i<16;i++)if(locks[i][0]&&same(locks[i],part))return -16;
 int a=attributes(part);
 if(a!=-2)return a<0?-5:-117;
 sibling(path,"BAK",part);
 a=attributes(part);
 return a==-2?0:a<0?-5:-117;
}

#ifndef K32IO_TEST
static int change_dir(const char *path,unsigned remove){
 unsigned a=remove?0x3a00:0x3900;
 __asm__ volatile("int $0x21; jnc 1f; movl $-1,%%eax; 1:":"+a"(a):"d"((unsigned)path):"cc","memory");
 return a==~0u?-5:0;
}

static int hide_dir(const char *path){
 unsigned a=0x4301;
 __asm__ volatile("int $0x21; jnc 1f; movl $-1,%%eax; 1:":"+a"(a):"c"(0x02),"d"((unsigned)path):"cc","memory");
 if(a!=~0u)return 0;
 return change_dir(path,1)?-117:-5;
}

static int find_entry(const char *pattern,unsigned char *dta,int next){
 unsigned a=0x2f00,old;unsigned short seg;
 __asm__ volatile("pushw %%es; int $0x21; movw %%es,%1; popw %%es":"+a"(a),"=c"(seg),"=b"(old)::"cc","memory");
 a=0x1a00;
 __asm__ volatile("int $0x21":"+a"(a):"d"((unsigned)dta):"cc","memory");
 a=next?0x4f00:0x4e00;
 __asm__ volatile("int $0x21; movzwl %%ax,%%eax; jc 1f; xorl %%eax,%%eax; jmp 2f; 1: negl %%eax; 2:":"+a"(a):"d"((unsigned)pattern),"c"(0x10):"cc","memory");
 int result=(int)a;
 a=0x1a00;
 __asm__ volatile("pushw %%ds; movw %w2,%%ds; int $0x21; popw %%ds":"+a"(a):"d"(old),"r"((unsigned)seg):"cc","memory");
 return result;
}
#else
static int change_dir(const char *,unsigned);
static int hide_dir(const char *);
static int find_entry(const char *,unsigned char *,int);
#endif

/* Check each component before traversing it. Existing trash has to be hidden
 * and all other directories have to be ordinary writable Work directories. */
static int parents(char path[DOS_PATH],int trash,int create,int leaf){
 for(unsigned i=8;path[i];i++){
  int last=!path[i+1]&&leaf;
  if(path[i]!='\\'&&!last)continue;
  unsigned end=i+last;
  char saved=path[end];path[end]=0;
  int hidden=trash&&end==14;
  int a=attributes(path);
  int e=0;
  if(a==-2&&create){
   e=change_dir(path,0);
   if(!e&&hidden)e=hide_dir(path);
  }else if(a<0)e=a==-2||a==-3?-2:-5;
  else if((a&0x5f)!=(hidden?0x12:0x10))e=-13;
  path[end]=saved;
  if(e)return e;
 }
 return 0;
}

static int list_entry(Owner *o,K32Fs *r,char path[DOS_PATH]){
 int next=r->action==K32_FS_NEXT;
 if(next){if(!r->token||r->token!=o->cursor)return -9;}
 else{
  o->cursor=0;
  int e=parents(path,r->flags,0,1);
  if(e==-2&&r->flags)goto empty;
  if(e)return e;
  unsigned end=0;while(path[end])end++;
  copy(path+end,"\\*.*",5);
  o->cursor=token();if(!o->cursor)return -75;
 }
 for(;;next=1){
  int e=find_entry(path,o->dta,next);
  if(e==-2||e==-18){o->cursor=0;break;}
  if(e){o->cursor=0;return -5;}
  unsigned char *d=o->dta;
  char name[16];unsigned n=0;
  while(n<13&&d[30+n])n++;
  if((d[21]&0x4e)||!work_component((const char *)d+30,n,name))continue;
  r->entry=(K32DirEntry){.version=1,.size=sizeof r->entry,.cursor=o->cursor,.attributes=d[21],
    .time=d[22]|(unsigned)d[23]<<8,.date=d[24]|(unsigned)d[25]<<8,
    .bytes=d[26]|(unsigned)d[27]<<8|(unsigned)d[28]<<16|(unsigned)d[29]<<24};
  copy(r->entry.name,name+3,n+1);
  r->token=o->cursor;
  return 0;
 }
 empty:r->token=0;r->entry=(K32DirEntry){0};return 0;
}

static int fs_open(Owner *o,K32Fs *r,const char *path,int boot){
 int lock=0;
 if(!boot){
  char part[DOS_PATH];sibling(path,"TMP",part);
  lock=lock_path(part);
  if(lock<0)return lock;
 }
 int h=file_open(o,path);
 if(h<0){if(lock)locks[lock-1][0]=0;return h;}
 File *f=&o->file[file_index(o,h)];f->lock=lock;
 if(boot&&f->bytes>K32_FS_BOOT_LIMIT){int e=file_close(o,h);return e?e:-27;}
 r->token=h;r->length=f->bytes;return 0;
}

static int fs_begin(Owner *o,K32Fs *r,const char *path,int boot){
 Transaction tx={.exists=boot,.old_length=r->length,.old_hash=r->hash,
  .limit=boot?K32_FS_BOOT_LIMIT:K32_IO_LIMIT};
 for(unsigned i=0;;i++){tx.target[i]=path[i];if(!path[i])break;}
 sibling(path,"TMP",tx.temporary);
 sibling(path,"BAK",tx.backup);
 int e=begin_paths(o,&tx,!boot);
 if(!e)r->token=o->tx.token;
 return e;
}

/* DOS 4 only renames directories within the same parent. Recycle empty
 * directories by preparing an empty destination before DOS checks/removes
 * the source. A failed removal leaves the original contents in place. */
static int transfer_empty_dir(const char *path,const char *dest){
 for(unsigned i=0;i<16;i++)if(locks[i][0]){
  unsigned j=0;while(path[j]&&path[j]==locks[i][j])j++;
  if(!path[j]&&locks[i][j]=='\\')return -16;
 }
 int e=change_dir(dest,0);if(e)return e;
 if(!change_dir(path,1))return 0;
 return change_dir(dest,1)?-117:-39;
}

static void prune_trash(char path[DOS_PATH]){
 unsigned end=0;while(path[end])end++;
 while(end>14){
  while(end>14&&path[end]!='\\')end--;
  if(end==14)break;
  path[end]=0;if(change_dir(path,1))break;
 }
}

static int filesystem(Owner *o,K32Fs *r){
 if(r->version!=1||r->size!=sizeof *r||r->action<K32_FS_LIST||r->action>K32_FS_BOOT_WRITE||
    r->flags>(r->action==K32_FS_LIST?K32_FS_LIST_TRASH:0)||
    (r->action!=K32_FS_NEXT&&r->token)||
    (r->action!=K32_FS_BOOT_WRITE&&(r->length||r->hash)))return -22;
 char path[DOS_PATH]={0},dest[DOS_PATH];
 if(r->action>=K32_FS_BOOT_OPEN){
  if(!same(r->path,"CONFIG.SYS")&&!same(r->path,"AUTOEXEC.BAT"))return -22;
  if(r->length>K32_FS_BOOT_LIMIT)return -27;
  copy(path,"A:\\",3);
  for(unsigned i=0;;i++){path[3+i]=r->path[i];if(!r->path[i])break;}
  return r->action==K32_FS_BOOT_OPEN?fs_open(o,r,path,1):fs_begin(o,r,path,1);
 }
 int e=ready(o);
 if(e)return e;
 if(r->action==K32_FS_NEXT)return list_entry(o,r,path);
 int trash=r->action==K32_FS_RESTORE||(r->action==K32_FS_LIST&&r->flags);
 e=work_path(o,r->path,trash,r->action==K32_FS_LIST,path);if(e)return e;
 if(r->action==K32_FS_LIST)return list_entry(o,r,path);
 e=parents(path,trash,0,0);if(e)return e;
 if(r->action==K32_FS_OPEN)return fs_open(o,r,path,0);
 if(r->action==K32_FS_CREATE)return fs_begin(o,r,path,0);
 if(r->action==K32_FS_MKDIR){
  int a=attributes(path);
  return a==-2?change_dir(path,0):a<0?-5:-17;
 }
 int a=attributes(path);
 if(a<0)return a==-2?-2:-5;
 if(a&0x4f)return -13;
 int is_dir=a&16;
 e=pending(path);if(e)return e;
 if(r->action==K32_FS_MOVE){
  e=work_path(o,r->destination,0,0,dest);if(e)return e;
  if(is_dir){
   unsigned i=0,end=0;
   while(path[i]&&dest[i]&&path[i]==dest[i]){if(path[i]=='\\')end=i;i++;}
   for(i=end+1;path[i];i++)if(path[i]=='\\')return -21;
   for(i=end+1;dest[i];i++)if(dest[i]=='\\')return -21;
   char mirror[DOS_PATH];
   work_path(o,r->path,1,0,mirror);
   int trash_attributes=attributes(mirror);
   if(trash_attributes>=0)return -16;
   /* Do not invalidate another client's cursor or open child transaction. */
   for(i=0;i<16;i++)if(locks[i][0]){
    unsigned j=0;while(path[j]&&path[j]==locks[i][j])j++;
    if(!path[j]&&locks[i][j]=='\\')return -16;
   }
  }
 }else{
  e=work_path(o,r->path,r->action==K32_FS_TRASH,0,dest);if(e)return e;
 }
 e=parents(dest,r->action==K32_FS_TRASH,r->action==K32_FS_TRASH,0);if(e)return e;
 a=attributes(dest);
 if(a!=-2)return a<0?-5:-17;
 e=pending(dest);if(e)return e;
 e=is_dir&&r->action!=K32_FS_MOVE?transfer_empty_dir(path,dest):rename_path(path,dest);
 if(!e){
  if(r->action==K32_FS_RESTORE)prune_trash(path);
  for(unsigned i=0;i<8;i++)owners[i].cursor=0;
 }
 return e;
}
