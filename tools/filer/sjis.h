#ifndef SJIS_H
#define SJIS_H
static inline int sjis_lead(unsigned c) {
    return (c >= 0x81 && c <= 0x9f) || (c >= 0xe0 && c <= 0xef);
}
static inline int sjis_trail(unsigned c) {
    return c >= 0x40 && c <= 0xfc && c != 0x7f;
}
/* PC-98 TVRAM stores ten in the high byte (bit 7 selects kanji), ku-0x20 low. */
static inline unsigned sjis_vram(unsigned lead, unsigned trail) {
    unsigned ku = (lead - (lead <= 0x9f ? 0x81 : 0xc1)) * 2 + 0x21;
    unsigned ten;
    if (trail >= 0x9f) { ku++; ten = trail - 0x7e; }
    else ten = trail - (trail >= 0x80 ? 0x20 : 0x1f);
    return 0x8000 | (ten << 8) | (ku - 0x20);
}
#endif
