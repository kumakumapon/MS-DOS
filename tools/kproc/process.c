#include "process.h"
static void copy(void *to,const void *from,unsigned n){unsigned char *d=to;const unsigned char *s=from;for(unsigned i=0;i<n;i++)d[i]=s[i];}
static unsigned token(KpState *s){return s->serial==0x7fffffffu?0:++s->serial;}
static KpProcess *find(KpState *s,unsigned value){for(unsigned i=0;i<KP_PROCESSES;i++)if(value&&s->task[i].token==value)return &s->task[i];return 0;}
static KpFd *fd(KpProcess *p,unsigned value){for(unsigned i=0;i<KP_FDS;i++)if(value&&p->fd[i].token==value)return &p->fd[i];return 0;}
static void collect(KpState *s){for(unsigned i=0;i<KP_PIPES;i++)if(s->pipe[i].used){unsigned refs=0;for(unsigned j=0;j<KP_PROCESSES;j++)for(unsigned k=0;k<KP_FDS;k++)refs+=s->task[j].fd[k].token&&s->task[j].fd[k].pipe==i;if(!refs)s->pipe[i]=(KpPipe){0};}}
static void streams_close(KpState *s,KpProcess *p){for(unsigned i=0;i<KP_FDS;i++)p->fd[i]=(KpFd){0};p->in=p->out=0;collect(s);}
static void complete(KpState *s,KpProcess *p,unsigned code,unsigned reason){p->state=KP_DONE;p->code=code;p->reason=reason;p->count=0;s->peer[p-s->task]=0;streams_close(s,p);}
static int empty(KpState *s){for(unsigned i=4;i<KP_PROCESSES;i++)if(!s->task[i].token)return (int)i;return -11;}
int kp_attach(KpState *s,const KpIdentity *id){
 for(unsigned i=0;i<KP_PROCESSES;i++){KpProcess *p=&s->task[i];if(p->state==KP_RUNNING&&p->slot==id->slot&&p->generation==id->generation&&p->physical==id->physical&&p->epoch==id->epoch)return (int)p->token;}
 if(id->slot>=4)return -116;
 int i=(int)id->slot;if(s->task[i].token)return -16;unsigned t=token(s);if(!t)return -75;
 s->peer[i]=0;
 s->task[i]=(KpProcess){.token=t,.state=KP_RUNNING,.slot=id->slot,.generation=id->generation,.physical=id->physical,.epoch=id->epoch};return (int)t;
}
static int executable(const unsigned char *s,unsigned n){
 if(!n||n>127||s[n-1])return -22;
 unsigned length=0,dot=0;
 for(unsigned i=0;i+1<n;i++){unsigned c=s[i];if(c<32||c>126||!c)return -22;if(!i&&c==' ')return -22;if(!dot&&c=='.'){if(!i||i>8)return -22;dot=i;}if(c==' '&&!length)length=i;}
 if(!length)length=n-1;
 if(!dot||length!=dot+4||s[dot+1]!='E'||s[dot+2]!='X'||s[dot+3]!='E')return -22;
 for(unsigned i=0;i<dot;i++){unsigned c=s[i];if(!((c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'))return -22;}
 if((dot==3&&((s[0]=='C'&&s[1]=='O'&&s[2]=='N')||(s[0]=='A'&&s[1]=='U'&&s[2]=='X')||(s[0]=='P'&&s[1]=='R'&&s[2]=='N')||(s[0]=='N'&&s[1]=='U'&&s[2]=='L')))||(dot==4&&((s[0]=='C'&&s[1]=='O'&&s[2]=='M')||(s[0]=='L'&&s[1]=='P'&&s[2]=='T'))&&s[3]>='1'&&s[3]<='9'))return -13;
 return 0;
}
static int inherit(KpState *s,KpProcess *parent,KpProcess *child,unsigned source,unsigned write){
 if(!source)return -9;
 KpFd *f=fd(parent,source);if(!f||f->write!=write)return -9;
 unsigned i=0;while(i<KP_FDS&&child->fd[i].token)i++;if(i==KP_FDS)return -24;
 unsigned t=token(s);if(!t)return -75;child->fd[i]=(KpFd){t,f->pipe,write};if(write)child->out=t;else child->in=t;return 0;
}
int kp_request(KpState *s,unsigned caller,K32Io *r,unsigned char *data){
 KpProcess *p=find(s,caller);if(!p||p->state!=KP_RUNNING)return -116;
 if(r->version!=1||r->size!=sizeof *r||r->reserved||r->operation<KP_INFO||r->operation>KP_ALLOW_PEER)return -22;
 if(r->operation!=KP_SPAWN&&r->flags)return -22;
 switch(r->operation){
 case KP_INFO:r->token=p->token;r->offset=p->parent;r->length=p->slot;r->hash=p->generation;return 0;
 case KP_SPAWN:{
  if(r->flags&~3u)return -22;
  int e=executable(data,r->bytes);if(e)return e;int at=empty(s);if(at<0)return at;
  unsigned t=token(s);if(!t)return -75;s->peer[at]=0;KpProcess *c=&s->task[at];*c=(KpProcess){.token=t,.parent=p->token,.state=KP_QUEUED,.slot=4};copy(c->command,data,r->bytes);
  if(r->flags&1){e=inherit(s,p,c,r->offset,0);if(e)goto fail;}
  if(r->flags&2){e=inherit(s,p,c,r->exists,1);if(e)goto fail;}
  r->token=t;return 0;
  fail:*c=(KpProcess){0};collect(s);return e;
 }
 case KP_WAIT:case KP_CANCEL:{
  KpProcess *c=find(s,r->token);if(!c||c->parent!=p->token)return -10;
  if(r->operation==KP_CANCEL){if(c->state!=KP_DONE)c->cancel=1;return 0;}
  if(c->state!=KP_DONE)return -11;
  r->length=c->code;r->hash=c->reason;*c=(KpProcess){0};return 0;
 }
 case KP_ALLOW_PEER:{
  if(r->bytes||r->data)return -22;
  KpProcess *peer=find(s,r->token);
  if(r->token&&(!peer||peer->state!=KP_RUNNING||peer==p))return -116;
  s->peer[p-s->task]=r->token;
  return 0;
 }
 case KP_SEND:{
  if(!r->bytes||r->bytes>KP_MESSAGE_BYTES)return -22;
  KpProcess *c=find(s,r->token);if(!c||c->state==KP_DONE)return -116;
  if(c->parent!=p->token&&p->parent!=c->token&&s->peer[c-s->task]!=p->token)return -1;
  if(c->count==KP_MESSAGES)return -11;
  KpMessage *m=&c->message[(c->head+c->count)%KP_MESSAGES];*m=(KpMessage){.sender=p->token,.bytes=r->bytes};copy(m->data,data,r->bytes);c->count++;return 0;
 }
 case KP_RECEIVE:{if(r->bytes>KP_MESSAGE_BYTES)return -22;if(!p->count)return -11;KpMessage *m=&p->message[p->head];if(r->bytes<m->bytes)return -28;copy(data,m->data,m->bytes);r->token=m->sender;unsigned n=m->bytes;p->head=(p->head+1)%KP_MESSAGES;p->count--;return (int)n;}
 case KP_PIPE:{
  unsigned i=0,a=KP_FDS,b=KP_FDS;for(;i<KP_PIPES&&s->pipe[i].used;i++);if(i==KP_PIPES)return -24;
  for(unsigned j=0;j<KP_FDS;j++)if(!p->fd[j].token){if(a==KP_FDS)a=j;else{b=j;break;}}
  if(b==KP_FDS)return -24;
  unsigned read=token(s),write=token(s);if(!read||!write)return -75;
  s->pipe[i]=(KpPipe){.used=1};p->fd[a]=(KpFd){read,i,0};p->fd[b]=(KpFd){write,i,1};r->token=read;r->offset=write;return 0;
 }
 case KP_CLOSE:{KpFd *f=fd(p,r->token);if(!f)return -9;if(p->in==f->token)p->in=0;if(p->out==f->token)p->out=0;*f=(KpFd){0};collect(s);return 0;}
 case KP_READ:case KP_WRITE:{
  unsigned write=r->operation==KP_WRITE,value=r->token?r->token:write?p->out:p->in;if(!value)return -38;
  KpFd *f=fd(p,value);if(!f||f->write!=write)return -9;if(r->bytes>256)return -22;
  KpPipe *pipe=&s->pipe[f->pipe];unsigned peers=0;
  for(unsigned i=0;i<KP_PROCESSES;i++)for(unsigned j=0;j<KP_FDS;j++){KpFd *other=&s->task[i].fd[j];peers+=other->token&&other->pipe==f->pipe&&other->write!=write;}
  if(!r->bytes)return 0;
  if(write&&!peers)return -32;
  unsigned n=write?KP_PIPE_BYTES-pipe->count:pipe->count;if(n>r->bytes)n=r->bytes;if(!n)return !write&&!peers?0:-11;
  for(unsigned i=0;i<n;i++){if(write)pipe->data[(pipe->head+pipe->count+i)%KP_PIPE_BYTES]=data[i];else data[i]=pipe->data[(pipe->head+i)%KP_PIPE_BYTES];}
  if(write)pipe->count+=n;else{pipe->head=(pipe->head+n)%KP_PIPE_BYTES;pipe->count-=n;}return (int)n;
 }
 default:return -38;
 }
}
void kp_pump(KpState *s,const KpHost *host){
 for(unsigned i=0;i<KP_PROCESSES;i++){
  KpProcess *p=&s->task[i];if(p->state!=KP_RUNNING)continue;KpIdentity id={0};
  if(!host->inspect(host->context,p->slot,&id)||id.generation!=p->generation||id.physical!=p->physical||id.epoch!=p->epoch)complete(s,p,116,KP_REPLACED);
  else if(id.state==KP_RECLAIMING){p->cancel=1;if(!host->cancel(host->context,p->slot))complete(s,p,id.code,id.fault?KP_FAULT:KP_CANCELLED);}
  else if(id.state!=KP_RUNNING)complete(s,p,id.code,id.fault?KP_FAULT:id.state==KP_DONE?KP_CANCELLED:KP_EXIT);
 }
 /* Orphans cannot retain endpoints or kill a later generation in their slot. */
 for(unsigned pass=0;pass<KP_PROCESSES;pass++)for(unsigned i=0;i<KP_PROCESSES;i++){
  KpProcess *p=&s->task[i];if(!p->token||!p->parent)continue;KpProcess *parent=find(s,p->parent);if(!parent||parent->state==KP_DONE||parent->cancel)p->cancel=1;
  if(p->cancel&&p->state!=KP_DONE){
   if(p->state==KP_RUNNING){KpIdentity id={0};if(host->inspect(host->context,p->slot,&id)&&id.generation==p->generation&&id.physical==p->physical&&id.epoch==p->epoch){if(id.state==KP_RECLAIMING)continue;if(id.state==KP_RUNNING&&host->cancel(host->context,p->slot))continue;}}
   complete(s,p,130,KP_CANCELLED);
  }
 }
 for(unsigned i=0;i<KP_PROCESSES;i++){KpProcess *p=&s->task[i];if(p->state==KP_DONE&&(!p->parent||!find(s,p->parent)||(find(s,p->parent)->state==KP_DONE||find(s,p->parent)->cancel)))*p=(KpProcess){0};}
 /* At most one DOS launch per pump. Never recurse through the host syscall. */
 for(unsigned i=0;i<KP_PROCESSES;i++)if(s->task[i].state==KP_QUEUED){
  KpProcess *p=&s->task[i];KpIdentity id={0};int e=host->launch(host->context,p->command,(p->in?1u:0u)|(p->out?2u:0u),&id);
  if(e)complete(s,p,(unsigned)-e,KP_START_FAILED);
  else{p->state=KP_RUNNING;p->slot=id.slot;p->generation=id.generation;p->physical=id.physical;p->epoch=id.epoch;}break;
 }
 collect(s);
}
