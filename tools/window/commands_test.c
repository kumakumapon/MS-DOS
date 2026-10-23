#include "commands.h"
#include <assert.h>
#include <string.h>
int main(void) {
    unsigned ids[DESK_COMMANDS];
    assert(command_find("  MaTH  ", 0, ids) == 1 && ids[0] == 7);
    assert(command_find("window right", 1, ids) == 1 && ids[0] == 15);
    assert(command_find("bitmap", 1, ids) == 2 && ids[0] == 5 && ids[1] == 6);
    assert(command_find("dos 2", 0, ids) == 0);
    assert(command_find("dos 2", 1, ids) == 1 && ids[0] == 12);
    assert(command_find("hello", 0, ids) == 0);
    assert(command_find("hello", 1, ids) == 1 && ids[0] == 21);
    assert(command_find("impossible application", 1, ids) == 0);
    assert(command_find("", 1, ids) == DESK_COMMANDS);
    assert(command_find(" ", 0, ids) == DESK_COMMANDS - 5);
    for (unsigned i = 0; i < DESK_COMMANDS - 5; i++) {
        assert(ids[i] != 11 && ids[i] != 12);
        assert(strlen(desk_commands[ids[i]].name) < 32);
    }
    return 0;
}
