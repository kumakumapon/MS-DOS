/* Small SDK example: no host UI, display/input belong to this V86 session. */
#include "../filer/filer.h"
#include "sdk.h"
int main(void) {
    char number[12];fd_number(number,kapp_session());
    p_output("Hello KDESK/386 - EXE application\r\nSession: ");p_output(number);
    p_output("\r\nABI 1.0 / private memory / read-only files\r\nPress any key to return.\r\n");
    kapp_yield();
    unsigned a=0x0800;__asm__ volatile("int $0x21":"+a"(a)::"cc","memory");
    return 0;
}
