/* Versioned, read-only A: root enumeration. No guest/DOS pointers escape. */
#ifndef K32_DIRECTORY_H
#define K32_DIRECTORY_H
#define K32_FEATURE_DIRECTORY 64u
typedef struct {unsigned version,size,cursor,attributes,bytes,date,time;char name[13];unsigned char reserved[23];} K32DirEntry;
_Static_assert(sizeof(K32DirEntry)==64,"directory entry ABI");
#endif
