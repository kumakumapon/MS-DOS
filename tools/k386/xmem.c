/* Original K386 extension: trusted XMS handles never enter guest registers. */
#include "xmem.h"
KXMemory k386_xmem={.signature={'K','3','8','6','X','M','S','1'},
    .version=KX_VERSION,.size=sizeof(KXMemory)};
unsigned xmem_used(unsigned id,k16 owner,unsigned *count){
    unsigned kb=0;*count=0;
    for(unsigned i=0;i<KX_BLOCKS;i++){KXBlock *b=&k386_xmem.blocks[id][i];
        if(b->handle&&(!owner||b->owner==owner)){kb+=b->bytes/1024;(*count)++;}}
    return kb;
}
static KXBlock *find(unsigned id,k16 owner,k16 token){
    for(unsigned i=0;i<KX_BLOCKS;i++){KXBlock *b=&k386_xmem.blocks[id][i];
        if(b->handle&&b->owner==owner&&b->token==token&&token)return b;}
    return 0;
}
static int drop(unsigned id,KXBlock *b){
    if(!xmem_free(b->handle))return 8;
    if(k386_xmem.pending[id]==b->token)k386_xmem.pending[id]=0;
    *b=(KXBlock){0};return 0;
}
void xmem_release(unsigned id,k16 owner){
    for(unsigned i=0;i<KX_BLOCKS;i++){KXBlock *b=&k386_xmem.blocks[id][i];
        if(b->handle&&(!owner||b->owner==owner)){
            if(k386_xmem.pending[id]==b->token)k386_xmem.pending[id]=0;
            if(drop(id,b))b->owner=0; /* revoke authority even if the driver cannot free */
        }}
}
int xmem_service(unsigned id,k16 owner,K386Frame *f,k8 *buffer){
    unsigned op=f->ebx&65535,error=0,count;
    KXBlock *b=find(id,owner,f->edx&65535);
    f->eflags&=~1u;
    if(op==KX_QUERY){f->eax=0x389d;f->ebx=KX_VERSION;f->ecx=KX_QUOTA_KB;f->edx=KX_CHUNK;return 0;}
    if(op==KX_USAGE){f->ecx=xmem_used(id,0,&count);f->edx=count;f->esi=xmem_used(id,owner,&count);f->edi=count;}
    else if(op==KX_ALLOC){
        if(k386_xmem.pending[id])b=find(id,owner,k386_xmem.pending[id]);
        else{
            unsigned kb=f->ecx,used=xmem_used(id,0,&count);
            if(!kb||kb>KX_QUOTA_KB)error=87;
            else if(kb>KX_QUOTA_KB-used||count==KX_BLOCKS||k386_xmem.serial==65535)error=8;
            else{
                for(unsigned i=0;i<KX_BLOCKS;i++)if(!k386_xmem.blocks[id][i].handle){b=&k386_xmem.blocks[id][i];break;}
                k16 handle=0;
                if(!xmem_new(kb,&handle))error=8;
                else{*b=(KXBlock){(k32)kb*1024,0,owner,handle,++k386_xmem.serial,0};k386_xmem.pending[id]=b->token;}
            }
        }
        if(!error){
            if(!b)error=6;
            else{
                for(unsigned i=0;i<KX_CHUNK;i++)buffer[i]=0;
                unsigned budget=KX_ZERO_SLICE;
                while(b->initialized<b->bytes&&budget){unsigned n=b->bytes-b->initialized;if(n>KX_CHUNK)n=KX_CHUNK;
                    if(!xmem_write(b->handle,b->initialized,buffer,n)){
                        k386_xmem.pending[id]=0;if(drop(id,b))b->owner=0;error=8;break;
                    }
                    b->initialized+=n;budget-=n;}
                if(!error&&b->initialized<b->bytes)return 1;
                if(!error){f->edx=b->token;k386_xmem.pending[id]=0;}
            }
        }
    }else if(op==KX_FREE)error=b?drop(id,b):6;
    else if(op==KX_READ||op==KX_WRITE){
        k32 n=f->ecx,off=f->edi&65535,at=(f->es&65535)*16+off;
        if(!b||b->initialized!=b->bytes)error=6;
        else if(n>KX_CHUNK||off+n>0x10000||at>=0xa0000||n>0xa0000-at||f->esi>b->bytes||n>b->bytes-f->esi)error=87;
        else if(op==KX_READ){if(!xmem_read(b->handle,f->esi,buffer,n)||!xmem_guest(id,at,buffer,n,1))error=8;}
        else if(!xmem_guest(id,at,buffer,n,0)||!xmem_write(b->handle,f->esi,buffer,n))error=8;
    }else error=1;
    f->eax=error;if(error)f->eflags|=1;return 0;
}
_Static_assert(sizeof(KXBlock)==16,"XMS diagnostic block layout");
_Static_assert(sizeof(KXMemory)==276,"XMS diagnostic layout");
