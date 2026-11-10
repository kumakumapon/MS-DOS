#include <assert.h>
#include <string.h>
#include "client-policy.h"
#include "document.h"
static KgRequest request;static KgClient client;static u32 serial;
static void open_window(void){request=(KgRequest){.version=1,.size=sizeof request,.operation=KG_OPEN,.width=328,.height=200};strcpy(request.title,"Application");}
int main(void){
 open_window();assert(kg_policy(&client,&request,7,&serial,640,400)==1);assert(client.width==344&&client.height==200&&client.generation==7);
 assert(kg_policy(&client,&request,7,&serial,640,400)==-16);
 request.operation=KG_FRAME;request.count=1;request.draw[0]=(KgDraw){.op=KG_RECT,.w=344,.h=200,.color=0xffffff};
 assert(kg_policy(&client,&request,8,&serial,640,400)==-116);
 u32 rev=client.revision;request.token=2;assert(kg_policy(&client,&request,7,&serial,640,400)==-116&&client.revision==rev);request.token=1;
 assert(kg_policy(&client,&request,7,&serial,640,400)==0&&client.revision==rev+1);
 rev=client.revision;request.draw[0].w=345;assert(kg_policy(&client,&request,7,&serial,640,400)==-22&&client.revision==rev);
 request.draw[0].w=344;request.draw[0].x=-1;assert(kg_validate(&request,344,200)==-22);request.draw[0].x=0;
 request.draw[0].color=0x1000000;assert(kg_validate(&request,344,200)==-22);request.draw[0].color=0;
 request.count=41;assert(kg_validate(&request,344,200)==-22);request.count=1;
 request.draw[0]=(KgDraw){.op=KG_TEXT,.x=336,.y=184};strcpy(request.draw[0].text,"A");assert(!kg_validate(&request,344,200));strcpy(request.draw[0].text,"AB");assert(kg_validate(&request,344,200)==-22);
 memset(request.draw[0].text,'X',48);assert(kg_validate(&request,344,200)==-22);
 request.draw[0]=(KgDraw){.op=KG_BUTTON,.x=0,.y=0,.w=20,.h=24};strcpy(request.draw[0].text,"ABC");assert(kg_validate(&request,344,200)==-22);
 request.version=2;assert(kg_validate(&request,344,200)==-22);request.version=1;request.size--;assert(kg_validate(&request,344,200)==-22);
 KgEvent e={.type=KG_KEY,.key='7'};kg_enqueue(&client,&e);request=(KgRequest){.version=1,.size=sizeof request,.operation=KG_POLL,.token=1};
 assert(kg_policy(&client,&request,7,&serial,640,400)==1&&request.event.key=='7');assert(kg_policy(&client,&request,7,&serial,640,400)==0);
 e=(KgEvent){.type=KG_RESIZE,.width=360,.height=200};kg_enqueue(&client,&e);e.width=400;kg_enqueue(&client,&e);assert(client.count==1&&client.width==400);
 for(unsigned i=0;i<20;i++){e=(KgEvent){.type=KG_KEY,.key=i};kg_enqueue(&client,&e);}assert(client.count==16&&client.dropped==5);
 request.operation=KG_CAPABILITIES;assert(kg_policy(&client,&request,7,&serial,640,400)==KG_CAP_SUPPORTED);
 request.operation=KG_SUBSCRIBE;request.count=32;assert(kg_policy(&client,&request,7,&serial,640,400)==-22);
 request.count=3;assert(!kg_policy(&client,&request,7,&serial,640,400)&&client.input_flags==3);
 client.count=0;e=(KgEvent){.type=KG_POINTER,.x=1};kg_enqueue(&client,&e);e.x=2;kg_enqueue(&client,&e);assert(client.count==1&&client.event[client.head].x==2);
 for(int i=0;i<13;i++){e.type=KG_KEY;kg_enqueue(&client,&e);}e.type=KG_POINTER;kg_enqueue(&client,&e);assert(client.count==14);e.type=KG_MOUSE;e.key=0;kg_enqueue(&client,&e);assert(client.count==15);
 request.operation=KG_CLIP_SET;request.count=1025;assert(kg_policy(&client,&request,7,&serial,640,400)==-22);
 request.count=1024;assert(!kg_policy(&client,&request,7,&serial,640,400));
 request.operation=KG_CLIP_GET;request.token=999;assert(kg_policy(&client,&request,7,&serial,640,400)==-116);request.token=1;
 request.operation=KG_CANCEL_CLOSE;assert(kg_policy(&client,&request,7,&serial,640,400)==-22);
 request.operation=KG_SUBSCRIBE;request.count=KG_CAP_ALL;assert(!kg_policy(&client,&request,7,&serial,640,400));
 request.operation=KG_CANCEL_CLOSE;assert(!kg_policy(&client,&request,7,&serial,640,400)&&client.cancelled==1);
 char command[64];assert(!kg_document_command(1,"note.txt",command)&&!strcmp(command,"KGEDIT.EXE work NOTE.TXT"));
 const char *invalid[]={"../A.TXT","A:NOTE.TXT","NUL.TXT","COM1.TXT","A.EXE","A..TXT","LONGERNAM.TXT","A*.TXT"};
 for(unsigned i=0;i<sizeof invalid/sizeof *invalid;i++)assert(kg_document_command(0,invalid[i],command)==-22);
 assert(kg_document_command(2,"A.TXT",command)==-22);
 request.operation=KG_EDIT_DOCUMENT;request.count=1;strcpy(request.title,"NOTE.TXT");request.token=999;assert(kg_policy(&client,&request,7,&serial,640,400)==-116);request.token=1;
 assert(!kg_policy(&client,&request,7,&serial,640,400)&&client.edit_pending&&client.edit_namespace==1);
 assert(kg_policy(&client,&request,7,&serial,640,400)==-16);request.operation=KG_EDIT_STATUS;assert(kg_policy(&client,&request,7,&serial,640,400)==-11);
 client.edit_pending=0;client.edit_result=-16;assert(kg_policy(&client,&request,7,&serial,640,400)==-16);
 client.edit_pending=1;
 request.operation=KG_CLOSE;assert(!kg_policy(&client,&request,7,&serial,640,400));assert(!client.token&&!client.count&&!client.edit_pending);
 request.operation=KG_POLL;assert(kg_policy(&client,&request,7,&serial,640,400)==-116);
 open_window();assert(kg_policy(&client,&request,8,&serial,800,600)==2);assert(client.generation==8&&request.token==2);
 request=(KgRequest){.version=1,.size=sizeof request,.operation=KG_SUBSCRIBE,.token=2,.count=KG_CAP_SUPPORTED};
 assert(!kg_policy(&client,&request,8,&serial,800,600));client.count=client.head=0;
 e=(KgEvent){.type=KG_KEY,.key=259,.control=KG_MOD_SHIFT};kg_enqueue(&client,&e);assert(client.event[0].control==KG_MOD_SHIFT);
 request.count=KG_CAP_ALL;assert(!kg_policy(&client,&request,8,&serial,800,600));client.count=client.head=0;kg_enqueue(&client,&e);assert(client.event[0].control==0);
 return 0;
}
