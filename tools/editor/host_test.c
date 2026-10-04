#include "editor.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static Editor e;
static void core_tests(void) {
    u8 text[] = {'A',0x93,0xfa,'B','\n',0x96,0x7b,'\t','X'};
    ed_init(&e); assert(!ed_insert(&e, text, sizeof text));
    e.cursor = 3; ed_delete(&e, 1);
    assert(e.length == 7 && e.cursor == 1 && e.data[1] == 'B');
    ed_undo(&e); assert(e.length == 9 && e.cursor == 3);
    ed_undo(&e); assert(e.length == 7);
    ed_undo(&e); e.cursor = 1; ed_delete(&e, 0); assert(e.length == 7);
    ed_undo(&e); e.cursor = 3; ed_vertical(&e, 1);
    assert(e.cursor == 7 && ed_column(&e) == 2); /* no position inside tab/pair */
    ed_vertical(&e, -1); assert(e.cursor == 3);
    e.cursor = 6; assert(ed_prev(&e, 7) == 5); /* ASCII-valued trail 0x7b */
    e.cursor = 5; ed_delete_line(&e); assert(e.length == 5);
    ed_undo(&e); assert(e.length == 9);
    u8 bad[] = {0x93}; int before = e.length;
    assert(ed_insert(&e, bad, 1) == ED_BAD_TEXT && e.length == before);
    ed_init(&e);
    u8 c = 'A'; for (int i = 0; i < ED_CAP; i++) e.data[i] = c;
    e.length = e.cursor = ED_CAP;
    assert(ed_insert(&e, &c, 1) == 8 && e.length == ED_CAP);
    ed_delete(&e, 1); assert(e.length == ED_CAP - 1);
    ed_undo(&e); assert(e.length == ED_CAP);
    assert(sjis_vram(0x93, 0xfa) == (0x8000 | (0x7c << 8) | 0x26)); /* 日 = JIS 0x467c */
    puts("CORE OK");
}
int main(int argc, char **argv) {
    if (argc == 2 && !strcmp(argv[1], "CORE")) { core_tests(); return 0; }
    if (argc < 3) return 2;
    ed_init(&e);
    const u8 keep[] = {'K','E','E','P'};
    ed_insert(&e, keep, 4);
    int error = ed_load(&e, argv[2]);
    if (error) {
        assert(e.length == 4 && !memcmp(e.data, "KEEP", 4));
    } else if (!strcmp(argv[1], "SAVE")) {
        if (argc > 3) fd_text(e.path, argv[3], FD_PATH);
        e.cursor = e.length; u8 c = '!';
        error = ed_insert(&e, &c, 1);
        if (!error) error = ed_save(&e);
    } else if (!strcmp(argv[1], "ROUND")) error = ed_save(&e);
    else if (strcmp(argv[1], "LOAD")) return 2;
    printf("ERROR %d LENGTH %d DIRTY %d RECOVERY %s\n", error, e.length, e.dirty, e.recovery);
    return error ? 1 : 0;
}
