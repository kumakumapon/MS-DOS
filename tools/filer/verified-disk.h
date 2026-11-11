#ifndef VERIFIED_DISK_H
#define VERIFIED_DISK_H
/* Real-mode DOS boundary only. No user paths, pointers or disk writes. */
int fd_hdd_verified(unsigned drive);
unsigned fd_disk_critical(int install,unsigned previous);
#endif
