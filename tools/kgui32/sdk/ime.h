/* Original bounded, app-local IME. No I/O, allocation or privileged interface. */
#ifndef KG_IME_H
#define KG_IME_H
#define KI_TEXT 96u
#define KI_CANDIDATES 16u
#define KI_DICTIONARY 32768u
#define KI_OFF 0u
#define KI_ROMAJI 1u
#define KI_KANA 2u
typedef struct {const unsigned char *data;unsigned bytes,entries;} KiDictionary;
typedef struct {
 unsigned scheme,katakana,n,cursor,pending,selecting,count,selected;
 char reading[KI_TEXT+1],roman[8],candidates[KI_CANDIDATES][KI_TEXT+1];
 const char *note;
} KiIme;
int ki_dictionary(KiDictionary *,const void *,unsigned);
int ki_active(const KiIme *);
void ki_clear(KiIme *);
int ki_input(KiIme *,unsigned,unsigned);
int ki_prepare(KiIme *);
int ki_convert(KiIme *,const KiDictionary *);
void ki_move_candidate(KiIme *,int);
void ki_script_text(const char *,unsigned,char *);
unsigned ki_preview(const KiIme *,char *);
const char *ki_result(const KiIme *);
unsigned ki_length(const char *);
#endif
