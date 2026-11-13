#ifndef K386_BANK_H
#define K386_BANK_H
#include "abi.h"
/* Immutable span descriptions refer only to trusted resident data. */
typedef struct {void *local;k16 offset,bytes;} KBankSpan;
void kbank_copy(k16 segment,const KBankSpan *span,unsigned count,int save);
#endif
