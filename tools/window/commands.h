#ifndef KDESK_COMMANDS_H
#define KDESK_COMMANDS_H
#define DESK_COMMANDS 21
typedef struct { const char *name, *keywords, *hint; unsigned icon; } DeskCommand;
extern const DeskCommand desk_commands[DESK_COMMANDS];
/* AND-match space-separated ASCII words, independently of case or word order. */
unsigned command_find(const char *query, int enhanced, unsigned *matches);
#endif
