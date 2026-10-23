#ifndef K386_APP_H
#define K386_APP_H
/* MZ extension at byte 28; relocation table follows the 44-byte descriptor. */
#define KAPP_HEADER_SIZE 72
#define KAPP_ABI 0x0100
#define KAPP_TEXT 1
/* 0: ordinary DOS image, 1: supported text application, -1: invalid descriptor. */
int kapp_header(const unsigned char *header, unsigned length);
#endif
