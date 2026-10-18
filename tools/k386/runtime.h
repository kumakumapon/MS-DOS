#ifndef K386_RUNTIME_H
#define K386_RUNTIME_H
#include "abi.h"
int k386_start(unsigned id,const char *command);
void k386_broker(unsigned id);
void k386_reap(unsigned id);
void k386_release(unsigned id);
#endif
