/* Original DOS program, executed inside the guest V86 context, not a host UI. */
#include "../filer/filer.h"
static u8 line[130],dta[43],buffer[512];
static void output(const char *s){p_output(s);}
static void number(u32 n){char text[12];fd_number(text,n);output(text);}
static int same(const char *a,const char *b){return !fd_cmp(a,b);}
static void prompt(void) {
    char cwd[68]="A:\\";unsigned a=0x4700,d=0,i=(unsigned)(cwd+3);
    __asm__ volatile("int $0x21":"+a"(a):"d"(d),"S"(i):"cc","memory");
    cwd[67]=0;output(cwd);output(">");
}
static void read_line(void) {
    line[0]=127;line[1]=0;unsigned a=0x0a00,d=(unsigned)line;
    __asm__ volatile("int $0x21":"+a"(a):"d"(d):"cc","memory");line[2+line[1]]=0;
}
static int find(const char *mask,int next) {
    unsigned a=next?0x4f00:0x4e00,c=0x10,d=(unsigned)mask;
    __asm__ volatile("int $0x21; jc 1f; xor %%eax,%%eax; jmp 2f; 1: neg %%eax; 2:":"+a"(a):"c"(c),"d"(d):"cc","memory");
    return (int)a;
}
static void list(const char *mask) {
    unsigned a=0x1a00,d=(unsigned)dta;
    __asm__ volatile("int $0x21":"+a"(a):"d"(d):"cc","memory");
    int error=find(mask[0]?mask:"*.*",0);
    if(error){output("No matching files\r\n");return;}
    do{dta[42]=0;output((char *)dta+30);output(dta[21]&0x10?"  <DIR>":"  ");
        if(!(dta[21]&0x10))number((u32)dta[26]|(u32)dta[27]<<8|(u32)dta[28]<<16|(u32)dta[29]<<24);
        output("\r\n");error=find("",1);
    }while(!error);
}
static void type_file(const char *path) {
    int h=p_open(path);if(h<0){output("Cannot open file\r\n");return;}
    int n;while((n=p_read(h,buffer,sizeof buffer))>0)p_write(1,buffer,n);
    p_close(h);output("\r\n");
}
static void clock_time(void) {
    unsigned a=0x2c00,c,d;__asm__ volatile("int $0x21":"+a"(a),"=c"(c),"=d"(d)::"cc","memory");
    char text[10];unsigned values[3]={c>>8&255,c&255,d>>8&255};
    for(int i=0;i<3;i++){text[i*3]='0'+values[i]/10;text[i*3+1]='0'+values[i]%10;text[i*3+2]=i==2?0:':';}
    output(text);output("\r\n");
}
static void execute(const char *name,const char *args) {
    char path[128];fd_text(path,name,sizeof path);int n=fd_len(path),extension=0;
    for(int i=n-1;i>=0&&path[i]!='\\'&&path[i]!='/';i--)if(path[i]=='.')extension=1;
    if(!extension&&n+4>=128){output("Program name too long\r\n");return;}
    if(!extension)fd_text(path+n,".EXE",128-n);
    int status=p_exec(path,args);
    if(!extension&&status==-2){fd_text(path+n,".COM",128-n);status=p_exec(path,args);}
    if(status<0){output("Cannot execute program: ");number(-status);output("\r\n");}
    else if(status){output("Program returned: ");number(status);output("\r\n");}
}
int main(void) {
    output("K386 DOS shell (read-only)\r\nHELP lists commands\r\n");
    for(;;){
        prompt();read_line();char *command=(char *)line+2;while(*command==' ')command++;
        char *args=command;while(*args&&*args!=' '){if(*args>='a'&&*args<='z')*args-=32;args++;}
        if(*args)*args++=0;
        while(*args==' ')args++;
        if(!command[0])continue;
        if(same(command,"EXIT"))return 0;
        if(same(command,"HELP"))output("VER DIR TYPE CD ECHO CLS TIME EXIT\r\nKDESK/386 EXE apps: NAME [arguments]\r\nFiles are read-only; four process levels\r\n");
        else if(same(command,"VER"))output("MS-DOS 4.00 / K386 guest shell\r\n");
        else if(same(command,"ECHO")){output(args);output("\r\n");}
        else if(same(command,"DIR"))list(args);
        else if(same(command,"TYPE"))type_file(args);
        else if(same(command,"TIME"))clock_time();
        else if(same(command,"CLS")){unsigned a=0x0c00;__asm__ volatile("int $0x18":"+a"(a)::"cc","memory");}
        else if(same(command,"CD")){if(!args[0]){prompt();output("\r\n");}else{unsigned a=0x3b00,d=(unsigned)args;__asm__ volatile("int $0x21; jnc 1f; neg %%eax; 1:":"+a"(a):"d"(d):"cc","memory");if((int)a<0)output("Directory unavailable\r\n");}}
        else execute(command,args);
    }
}
