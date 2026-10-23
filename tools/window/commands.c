#include "commands.h"

const DeskCommand desk_commands[DESK_COMMANDS] = {
    {"File manager", "files explorer folder fd98", "Browse files and folders", 0},
    {"Text editor", "edit document", "Edit a text file", 1},
    {"System settings", "cpanel configuration", "Configure this PC-98", 2},
    {"BASIC", "program language", "Write and run programs", 3},
    {"DOS shell", "command terminal", "Full-screen DOS; EXIT returns", 4},
    {"Image viewer", "picture bitmap bmp", "Open a BMP picture", 5},
    {"Image editor", "paint drawing bitmap", "Draw and edit pictures", 6},
    {"Calculator", "math calc", "Quick arithmetic", 7},
    {"Clock", "time date", "Time and calendar", 8},
    {"Memo", "note clipboard text", "Your desktop notes", 1},
    {"Task manager", "tasks processes cpu resources", "Manage running tasks", 2},
    {"DOS 1", "session terminal", "Independent DOS session", 4},
    {"DOS 2", "session terminal", "Independent DOS session", 4},
    {"Help", "keyboard mouse shortcuts", "Keyboard and mouse controls", 1},
    {"Tile left", "window layout arrange", "", 4},
    {"Tile right", "window layout arrange", "", 4},
    {"Restore window", "layout size position", "", 4},
    {"Show desktop", "hide minimize all windows", "Hide windows; keep work running", 0},
    {"Launcher", "apps applications icons", "Browse app icons", 0},
    {"About", "version information", "Version and credits", 2},
    {"Maximize window", "layout full screen", "", 4},
    {"Hello KDESK/386", "hello khello exe example", "Open an EXE app in a DOS window", 4},
    {"Background counter", "counter exe", "Run a background EXE application", 4},
    {"Keyboard echo", "echo keys exe", "Type in an independent DOS window", 4},
};
static char lower(char c) { return c >= 'A' && c <= 'Z' ? c + 'a' - 'A' : c; }
static int contains(const char *text, const char *word, unsigned length) {
    for (; *text; text++) {
        unsigned i = 0;
        while (i < length && text[i] && lower(text[i]) == lower(word[i])) i++;
        if (i == length) return 1;
    }
    return 0;
}
unsigned command_find(const char *query, int enhanced, unsigned *matches) {
    unsigned count = 0;
    for (unsigned id = 0; id < DESK_COMMANDS; id++) {
        if (!enhanced && (id == 11 || id == 12 || id >= 21)) continue;
        const char *word = query; int match = 1;
        while (*word) {
            while (*word == ' ') word++;
            unsigned length = 0; while (word[length] && word[length] != ' ') length++;
            if (!length) break;
            if (!contains(desk_commands[id].name, word, length) &&
                !contains(desk_commands[id].keywords, word, length)) { match = 0; break; }
            word += length;
        }
        if (match) matches[count++] = id;
    }
    return count;
}
