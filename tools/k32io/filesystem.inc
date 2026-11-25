/* Work path validation and DOS operations used only by the resident broker. */
static int work_component(const char *name,unsigned n,char out[16]){
 if(!k32_filename(name,n,out))return 0;
 const char *base=out+3,*ext=0;
 for(const char *p=base;*p;p++)if(*p=='.')ext=p+1;
 return !same(base,"KTRASH")&&(!ext||(!same(ext,"TMP")&&!same(ext,"BAK")&&!same(ext,"KTR")));
}

static int work_path(const Owner *o,const char *name,int trash,int directory,char *out){
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

static int set_dir_attributes(const char *path,unsigned attrs){
 unsigned a=0x4301;
 __asm__ volatile("int $0x21; jnc 1f; movl $-1,%%eax; 1:":"+a"(a):"c"(attrs),"d"((unsigned)path):"cc","memory");
 return a==~0u?-5:0;
}
static int hide_dir(const char *path){
 if(!set_dir_attributes(path,2))return 0;
 return change_dir(path,1)?-117:-5;
}

static int find_entry(const char *pattern,unsigned char *dta,int next){
 unsigned a=0x2f00,old;unsigned short seg;
 __asm__ volatile("pushw %%es; int $0x21; movw %%es,%1; popw %%es":"+a"(a),"=c"(seg),"=b"(old)::"cc","memory");
 a=0x1a00;
 __asm__ volatile("int $0x21":"+a"(a):"d"((unsigned)dta):"cc","memory");
 a=next?0x4f00:0x4e00;
 __asm__ volatile("int $0x21; movzwl %%ax,%%eax; jc 1f; xorl %%eax,%%eax; jmp 2f; 1: negl %%eax; 2:":"+a"(a):"d"((unsigned)pattern),"c"(0x37):"cc","memory");
 int result=(int)a;
 a=0x1a00;
 __asm__ volatile("pushw %%ds; movw %w2,%%ds; int $0x21; popw %%ds":"+a"(a):"d"(old),"r"((unsigned)seg):"cc","memory");
 return result;
}
#else
static int change_dir(const char *,unsigned);
static int hide_dir(const char *);
static int set_dir_attributes(const char *,unsigned);
static int find_entry(const char *,unsigned char *,int);
#endif

/* Check each component before traversing it. Existing trash has to be hidden
 * and all other directories have to be ordinary writable Work directories. */
static int parents(char *path,int trash,int create,int leaf){
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

static unsigned path_length(const char *p){unsigned n=0;while(p[n])n++;return n;}
static int below(const char *parent,const char *path){
 unsigned i=0;while(parent[i]&&parent[i]==path[i])i++;
 return !parent[i]&&(!path[i]||path[i]=='\\');
}
static int folder_marker(const char *path,char marker[DOS_PATH]){
 unsigned n=path_length(path);
 static const char suffix[]="\\KRECYCLE.KTR";
 if(n+sizeof suffix>DOS_PATH)return -22;
 copy(marker,path,n);copy(marker+n,suffix,sizeof suffix);return 0;
}
static int is_recycled(const char *path){
 char marker[DOS_PATH];
 if(folder_marker(path,marker))return 0;
 int a=attributes(marker);return a>=0&&(a&0x5f)==0;
}
static int folder_recycle(const char *path,int restore){
 char marker[DOS_PATH];int e=folder_marker(path,marker);if(e)return e;
 for(unsigned i=0;i<16;i++)if(locks[i][0]&&below(path,locks[i]))return -16;
 if(restore){
  if(!is_recycled(path))return -2;
  e=set_dir_attributes(path,0);if(e)return e;
  if(remove_path(marker)){set_dir_attributes(path,6);return -117;}
  return 0;
 }
 int h=open_path(marker,1);if(h<0)return h;
 if(close_handle(h)&&close_handle(h))return -117;
 e=set_dir_attributes(path,6);
 if(e&&remove_path(marker))return -117;
 return e;
}
static int append_path(char out[DOS_PATH],const char *base,const char *relative){
 unsigned n=path_length(base),m=path_length(relative);
 if(n+1+m>=DOS_PATH)return -22;
 copy(out,base,n);out[n]='\\';copy(out+n+1,relative,m+1);return 0;
}

enum { DRIVE_LIST = K32_FS_LIST_TMP_CONTENT+1 };
static int list_entry(Owner *o,K32Fs *r,char path[DOS_PATH]){
 int next=r->action==K32_FS_NEXT;
 if(next){if(!r->token||r->token!=o->cursor)return -9;}
 else{
  o->cursor=0;
  o->list_kind=r->action==K32_FS_DRIVE_LIST?DRIVE_LIST:r->flags;
  int preview=o->list_kind==K32_FS_LIST_TMP_CONTENT;
  int e=o->list_kind==DRIVE_LIST?0:parents(path,o->list_kind==K32_FS_LIST_TRASH,0,!preview);
  if(!e&&preview){
   char stage[DOS_PATH];sibling(path,"TMP",stage);
   if(tree.owner)return -16;
   if((attributes(stage)&0x5f)!=16)return -2;
   copy(path,stage,path_length(stage)+1);
  }
  if(e==-2&&o->list_kind==K32_FS_LIST_TRASH)goto empty;
  if(e)return e;
  unsigned end=path_length(path);
  copy(path+end,path[end-1]=='\\'?"*.*":"\\*.*",path[end-1]=='\\'?4:5);
  o->cursor=token();if(!o->cursor)return -75;
 }
 for(;;next=1){
  int e=find_entry(path,o->dta,next);
  if(e==-2||e==-18){o->cursor=0;break;}
  if(e){o->cursor=0;return -5;}
  unsigned char *d=o->dta;
  char name[16];unsigned n=0;
  while(n<13&&d[30+n])n++;
  if(d[30]=='.'&&(n==1||(n==2&&d[31]=='.')))continue;
  int stage=o->list_kind==K32_FS_LIST_TMP;
  unsigned stem=n;
  if(stage){
   if((d[21]&0x5f)!=16||n<5||!same((const char *)d+30+n-4,".TMP"))continue;
   stem=n-4;
  }
  int valid=work_component((const char *)d+30,stem,name);
  if(o->list_kind==K32_FS_LIST_STRICT&&((d[21]&0x4f)||!valid)){o->cursor=0;return -117;}
  if(!valid)continue;
  if(o->list_kind==K32_FS_LIST_ARCHIVED){
   char parent[DOS_PATH],item[DOS_PATH];
   if((d[21]&0x5f)!=0x16||work_path(o,r->path,0,1,parent)||
      append_path(item,parent,name+3)||!is_recycled(item))continue;
  }else if(d[21]&0x4e)continue;
  r->entry=(K32DirEntry){.version=1,.size=sizeof r->entry,.cursor=o->cursor,.attributes=d[21],
    .time=d[22]|(unsigned)d[23]<<8,.date=d[24]|(unsigned)d[25]<<8,
    .bytes=d[26]|(unsigned)d[27]<<8|(unsigned)d[28]<<16|(unsigned)d[29]<<24};
  copy(r->entry.name,stage?(const char *)d+30:name+3,n+1);
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
 if(boot==1&&f->bytes>K32_FS_BOOT_LIMIT){int e=file_close(o,h);return e?e:-27;}
 r->token=h;r->length=f->bytes;return 0;
}

static int fs_begin(Owner *o,K32Fs *r,const char *path,int boot){
 Transaction tx={.exists=boot||r->flags,.old_length=r->length,.old_hash=r->hash,.limit=boot?K32_FS_BOOT_LIMIT:K32_IO_LIMIT};
 for(unsigned i=0;;i++){tx.target[i]=path[i];if(!path[i])break;}
 int e=begin_paths(o,&tx,!tx.exists);
 if(!e)r->token=o->tx.token;
 return e;
}

/* A protected app can select boot options, but cannot write arbitrary
 * CONFIG.SYS or AUTOEXEC.BAT content. Build the edit from the current DOS
 * file while holding the serialized broker. */
static int boot_write(Owner *o,K32Fs *r,const char *path,int startup){
 if(o->tx.phase)return -16;
 int h=file_open(o,path);if(h<0)return h;
 File *f=&o->file[file_index(o,h)];
 unsigned n=f->bytes;
 int e=n>K32_FS_BOOT_LIMIT?-27:n!=r->length?-116:data_io(f->h,n,0);
 int closed=file_close(o,h);
 if(closed)return -117;
 if(e<0)return e;
 if((unsigned)e!=n)return -5;
 unsigned hash=2166136261u;for(unsigned i=0;i<n;i++)hash=(hash^buffer[i])*16777619u;
 if(hash!=r->hash)return -116;
 KgBootSettings old={0},next;
 kg_settings_read(&old,startup,(const char *)buffer,n);
 e=kg_settings_decode(&next,&old,startup,r->flags);if(e)return e;
 char edited[K32_FS_BOOT_LIMIT];
 e=kg_settings_build(&old,&next,startup,(const char *)buffer,n,edited,sizeof edited);
 if(e<0)return e;
 unsigned bytes=(unsigned)e;
 e=fs_begin(o,r,path,1);if(e)return e;
 copy(buffer,edited,bytes);
 e=data_io(o->tx.writer,bytes,1);
 if(e>=0&&(unsigned)e!=bytes)e=-28;
 if(e>=0){do{e=advance(o);}while(e==-11);}
 if(e<0){int abort=abort_tx(o,1);return abort<0?abort:e;}
 r->token=0;r->flags=e;return 0;
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

static void expire_lists(void){for(unsigned i=0;i<8;i++)owners[i].cursor=0;}

static int folder_copy(Owner *o,K32Fs *r){
 char stage[DOS_PATH];
 if(r->action==K32_FS_TREE_BEGIN){
  if(tree.owner)return -16;
  int e=work_path(o,r->path,0,0,tree.source);
  if(!e)e=work_path(o,r->destination,0,0,tree.target);
  if(!e)e=parents(tree.source,0,0,0);
  if(!e)e=parents(tree.target,0,0,0);
  if(e)return e;
  if(below(tree.source,tree.target))return -22;
  int src=attributes(tree.source),dst=attributes(tree.target);
  if(src!=16&&src!=48)return src<0?-2:-13;
  if(dst!=-2)return dst<0?-5:-17;
  if((e=pending(tree.source))||(e=pending(tree.target)))return e;
  for(unsigned i=0;i<16;i++)if(locks[i][0]&&below(tree.source,locks[i]))return -16;
  sibling(tree.target,"TMP",stage);e=change_dir(stage,0);
  if(e)return e;
  tree.owner=o;tree.move=r->flags;
  return 0;
 }
 if(tree.owner!=o)return -9;
 sibling(tree.target,"TMP",stage);
 if(r->action==K32_FS_TREE_CANCEL){tree.owner=0;return 0;}
 if(r->action==K32_FS_TREE_PUBLISH){
  if(o->tx.phase)return -16;
  int e=attributes(tree.target);
  if(e!=-2)return e<0?-5:-17;
  if(tree.move&&(e=folder_recycle(tree.source,0)))return e;
  e=rename_path(stage,tree.target);
  if(e&&tree.move&&folder_recycle(tree.source,1))return -117;
  if(!e){tree.owner=0;expire_lists();}
  return e;
 }
 char canonical[DOS_PATH],path[DOS_PATH];
 int e=work_path(o,r->path,0,0,canonical);if(e)return e;
 unsigned depth=1;
 for(const char *p=tree.target+8;*p;p++)depth+=*p=='\\';
 for(const char *p=canonical+8;*p;p++)depth+=*p=='\\';
 if(depth>=3||path_length(tree.target+8)+path_length(canonical+8)+1>=K32_FS_PATH)return -22;
 e=append_path(path,stage,canonical+8);if(e)return e;
 if(r->action==K32_FS_TREE_MKDIR)return change_dir(path,0);
 return fs_begin(o,r,path,0);
}

static int filesystem(Owner *o,K32Fs *r){
 unsigned drive=r->action==K32_FS_DRIVE_LIST||r->action==K32_FS_DRIVE_OPEN;
 unsigned limit=r->action==K32_FS_LIST?K32_FS_LIST_TMP_CONTENT:drive?25:r->action==K32_FS_TREE_BEGIN||r->action==K32_FS_SAVE?1:0;
 if(r->version!=1||r->size!=sizeof *r||r->action<K32_FS_LIST||r->action>K32_FS_TREE_RECOVER||
    (r->action!=K32_FS_BOOT_WRITE&&r->flags>limit)||
    (r->action!=K32_FS_NEXT&&r->token)||
    (r->action!=K32_FS_BOOT_WRITE&&r->action!=K32_FS_SAVE&&(r->length||r->hash)))return -22;
 char path[DOS_PATH]={0},dest[DOS_PATH];
 if(r->action==K32_FS_DRIVES){r->length=1u|(1u<<o->drive);r->flags=o->drive;return 0;}
 if(drive){
  if(r->flags&&r->flags!=o->drive)return -15;
  if(r->action==K32_FS_DRIVE_OPEN&&!r->path[0])return -22;
  if(r->path[0]){
   int e=work_path(o,r->path,0,0,path);if(e)return e;
   unsigned n=path_length(path);for(unsigned i=3;i<=n-5;i++)path[i]=path[i+5];
   path[0]='A'+r->flags;
   for(unsigned i=3;path[i];i++)if(path[i]=='\\'){
    path[i]=0;int a=attributes(path);path[i]='\\';
    if(a<0||(a&0x5e)!=16)return -13;
   }
   if(r->action==K32_FS_DRIVE_OPEN)return fs_open(o,r,path,2);
   int a=attributes(path);if(a<0)return a;if((a&0x5e)!=16)return -13;
  }else copy(path,"A:\\",4);
  path[0]='A'+r->flags;
  return list_entry(o,r,path);
 }
 if(r->action==K32_FS_BOOT_OPEN||r->action==K32_FS_BOOT_WRITE){
  if(!same(r->path,"CONFIG.SYS")&&!same(r->path,"AUTOEXEC.BAT"))return -22;
  if(r->length>K32_FS_BOOT_LIMIT)return -27;
  copy(path,"A:\\",3);
  for(unsigned i=0;;i++){path[3+i]=r->path[i];if(!r->path[i])break;}
  return r->action==K32_FS_BOOT_OPEN?fs_open(o,r,path,1):boot_write(o,r,path,same(r->path,"AUTOEXEC.BAT"));
 }
 if(r->action==K32_FS_NEXT)return list_entry(o,r,path);
 int e=ready(o);
 if(e)return e;
 if(r->action>=K32_FS_TREE_BEGIN&&r->action<=K32_FS_TREE_CANCEL)return folder_copy(o,r);
 int trash=r->action==K32_FS_RESTORE||(r->action==K32_FS_LIST&&r->flags==K32_FS_LIST_TRASH);
 e=work_path(o,r->path,trash,r->action==K32_FS_LIST&&r->flags!=K32_FS_LIST_TMP_CONTENT,path);if(e)return e;
 if(r->action==K32_FS_LIST)return list_entry(o,r,path);
 if(r->action==K32_FS_RESTORE){
  int mirror=attributes(path);
  if(mirror==-2||mirror==-3){
   e=work_path(o,r->path,0,0,path);if(e)return e;
   e=parents(path,0,0,0);if(e)return e;
   if((attributes(path)&0x5f)!=0x16)return -2;
   e=folder_recycle(path,1);if(!e)expire_lists();return e;
  }
 }
 if(tree.owner&&tree.owner!=o&&below(tree.source,path))return -16;
 e=parents(path,trash,0,0);if(e)return e;
 if(r->action==K32_FS_OPEN)return fs_open(o,r,path,0);
 if(r->action==K32_FS_CREATE)return fs_begin(o,r,path,0);
 if(r->action==K32_FS_SAVE)return r->length>K32_IO_LIMIT||(!r->flags&&(r->length||r->hash))?-22:fs_begin(o,r,path,0);
 if(r->action==K32_FS_TREE_RECOVER){
  if(tree.owner)return -16;
  if(r->destination[K32_FS_PATH-1]||!work_component(r->destination,path_length(r->destination),dest))return -22;
  char *leaf=path;for(char *p=path;*p;p++)if(*p=='\\')leaf=p+1;
  char stage[DOS_PATH];sibling(path,"TMP",stage);
  if((attributes(stage)&0x5f)!=16)return -2;
  copy(leaf,dest+3,path_length(dest+3)+1);
  e=attributes(path);if(e!=-2)return e<0?-5:-17;
  e=pending(path);if(e)return e;
  e=rename_path(stage,path);if(!e)expire_lists();return e;
 }
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
  if(tree.owner&&tree.owner!=o&&below(tree.source,dest))return -16;
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
 if(e==-39&&r->action==K32_FS_TRASH&&is_dir){e=folder_recycle(path,0);if(!e)prune_trash(dest);}
 if(!e){
  if(r->action==K32_FS_RESTORE)prune_trash(path);
  expire_lists();
 }
 return e;
}
