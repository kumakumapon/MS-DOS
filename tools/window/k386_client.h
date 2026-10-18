#ifndef KDESK_K386_CLIENT_H
#define KDESK_K386_CLIENT_H
#include "../k386/abi.h"
int k386_probe(void);
int k386_command(unsigned command,unsigned id,unsigned key,const char *path);
unsigned k386_word(unsigned id,unsigned offset);
unsigned k386_counter(unsigned id);
char k386_character(unsigned id,unsigned cell);
unsigned k386_tick(void);
#endif
