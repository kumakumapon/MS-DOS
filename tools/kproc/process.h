/* Bounded process/IPC/stream policy. No DOS, CPU mode or physical pointers. */
#ifndef K32_PROCESS_MODEL_H
#define K32_PROCESS_MODEL_H
#include "../k38632/io.h"
#define K32_FEATURE_PROCESS 512u
enum {KP_INFO=32,KP_SPAWN,KP_WAIT,KP_CANCEL,KP_SEND,KP_RECEIVE,KP_PIPE,KP_READ,KP_WRITE,KP_CLOSE,KP_ALLOW_PEER};
enum {KP_QUEUED=1,KP_RUNNING,KP_DONE,KP_RECLAIMING};
enum {KP_EXIT,KP_FAULT,KP_CANCELLED,KP_START_FAILED,KP_REPLACED};
enum {KP_PROCESSES=8,KP_FDS=8,KP_PIPES=4,KP_PIPE_BYTES=512,KP_MESSAGE_BYTES=64,KP_MESSAGES=4};
typedef struct {unsigned token,pipe,write;} KpFd;
typedef struct {unsigned sender,bytes;unsigned char data[KP_MESSAGE_BYTES];} KpMessage;
typedef struct {
 unsigned token,parent,slot,generation,physical,epoch,state,cancel,code,reason,in,out;
 char command[128];KpFd fd[KP_FDS];KpMessage message[KP_MESSAGES];unsigned head,count;
} KpProcess;
typedef struct {unsigned used,head,count;unsigned char data[KP_PIPE_BYTES];} KpPipe;
typedef struct {unsigned serial;KpProcess task[KP_PROCESSES];KpPipe pipe[KP_PIPES];
 unsigned peer[KP_PROCESSES];
} KpState;
typedef struct {unsigned slot,generation,physical,epoch,state,code,fault;} KpIdentity;
typedef struct {
 void *context;
 int (*launch)(void *,const char *,unsigned,KpIdentity *);
 int (*inspect)(void *,unsigned,KpIdentity *);
 int (*cancel)(void *,unsigned);
} KpHost;
int kp_attach(KpState *,const KpIdentity *);
int kp_request(KpState *,unsigned,K32Io *,unsigned char *);
void kp_pump(KpState *,const KpHost *);
#endif
