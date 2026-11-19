#ifndef KP_SHELL_PARSE_H
#define KP_SHELL_PARSE_H
enum {KP_SHELL_COMMANDS=3};
typedef struct {char command[KP_SHELL_COMMANDS][128],input[13],output[13];unsigned count;} KpShell;
int kp_shell_parse(const char *,KpShell *);
#endif
