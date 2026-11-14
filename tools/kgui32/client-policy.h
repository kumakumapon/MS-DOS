#ifndef KGUI32_CLIENT_POLICY_H
#define KGUI32_CLIENT_POLICY_H
#include "sdk/protocol.h"
#include "surface.h"
typedef struct {
 u32 generation,token,revision,width,height,head,count,received,dropped,input_flags,cancelled;
 char title[32];KgEvent event[KG_EVENTS];
 u32 edit_pending,edit_namespace;int edit_result;char edit_name[13];KgSurface surface,extra_surface;
} KgClient;
KgSurface *kg_client_surface(KgClient *,const KgRequest *);
int kg_validate(const KgRequest *,unsigned,unsigned);
int kg_policy(KgClient *,KgRequest *,u32,u32 *,unsigned,unsigned);
void kg_enqueue(KgClient *,const KgEvent *);
#endif
