#ifndef KP_SHELL_PARSE_H
#define KP_SHELL_PARSE_H
typedef struct {char command[2][128],input[13],output[13];unsigned count;} KpShell;
int kp_shell_parse(const char *,KpShell *);
#endif
