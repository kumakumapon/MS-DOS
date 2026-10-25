#ifndef KDESK_K386_CLIENT_H
#define KDESK_K386_CLIENT_H
#include "../k386/abi.h"
#include "../k386/resources.h"
int k386_probe(void);
int k386_command(unsigned command,unsigned id,unsigned key,const char *path);
unsigned k386_word(unsigned id,unsigned offset);
unsigned k386_counter(unsigned id);
char k386_character(unsigned id,unsigned cell);
void k386_screen(unsigned id,char *out);
unsigned k386_tick(void);
int k386_resources_available(void);
int k386_usage(unsigned id,K386Usage *usage);
int k386_history(unsigned age,K386Event *event);
void k386_report_line(unsigned row,char *out);
#endif
