#ifndef K32_PROCESS_SDK_H
#define K32_PROCESS_SDK_H
#include "k32x.h"
#include "../../kproc/process.h"
static inline K32Io k32_process_request(unsigned op){return (K32Io){.version=1,.size=sizeof(K32Io),.operation=op};}
static inline int k32_process_call(K32Io *r){return k32_call(18,(unsigned)r,sizeof *r);}
static inline int k32_process_info(K32Io *r){*r=k32_process_request(KP_INFO);return k32_process_call(r);}
static inline int k32_spawn(const char *command,unsigned input,unsigned output){
 unsigned n=0;while(n<127&&command[n])n++;if(n==127)return -22;
 K32Io r=k32_process_request(KP_SPAWN);r.data=(unsigned)command;r.bytes=n+1;r.flags=(input?1:0)|(output?2:0);r.offset=input;r.exists=output;int e=k32_process_call(&r);return e<0?e:(int)r.token;
}
static inline int k32_process_wait(unsigned child,unsigned *code,unsigned *reason){K32Io r=k32_process_request(KP_WAIT);r.token=child;int e=k32_process_call(&r);if(!e){if(code)*code=r.length;if(reason)*reason=r.hash;}return e;}
static inline int k32_process_cancel(unsigned child){K32Io r=k32_process_request(KP_CANCEL);r.token=child;return k32_process_call(&r);}
static inline int k32_message_send(unsigned to,const void *p,unsigned n){K32Io r=k32_process_request(KP_SEND);r.token=to;r.data=(unsigned)p;r.bytes=n;return k32_process_call(&r);}
static inline int k32_message_receive(unsigned *from,void *p,unsigned n){K32Io r=k32_process_request(KP_RECEIVE);r.data=(unsigned)p;r.bytes=n;int e=k32_process_call(&r);if(e>=0&&from)*from=r.token;return e;}
static inline int k32_pipe(unsigned ends[2]){K32Io r=k32_process_request(KP_PIPE);int e=k32_process_call(&r);if(!e){ends[0]=r.token;ends[1]=r.offset;}return e;}
static inline int k32_pipe_close(unsigned value){K32Io r=k32_process_request(KP_CLOSE);r.token=value;return k32_process_call(&r);}
static inline int k32_pipe_io(unsigned op,unsigned value,void *p,unsigned n){K32Io r=k32_process_request(op);r.token=value;r.data=(unsigned)p;r.bytes=n;return k32_process_call(&r);}
static inline int k32_pipe_read(unsigned value,void *p,unsigned n){return k32_pipe_io(KP_READ,value,p,n);}
static inline int k32_pipe_write(unsigned value,const void *p,unsigned n){return k32_pipe_io(KP_WRITE,value,(void *)p,n);}
/* Console fallback is explicit: EOF (zero) never becomes keyboard input. */
static inline int k32_stream_write(const void *p,unsigned bytes){
 if(!(k32_info()->features&K32_FEATURE_PROCESS))return k32_write(p,bytes);
 unsigned at=0;while(at<bytes){unsigned n=bytes-at;if(n>256)n=256;int e=k32_pipe_write(0,(const unsigned char *)p+at,n);if(e==-38)return at?(int)at:k32_write(p,bytes);if(e==-11){k32_wait(K32_WAIT_TIME,16);continue;}if(e<=0)return e<0?e:-5;at+=(unsigned)e;}return (int)at;
}
#endif
