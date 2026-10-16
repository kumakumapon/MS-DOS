#ifndef KDESK_UTILITIES_H
#define KDESK_UTILITIES_H
#include "hardware.h"

typedef long long CalcNumber;
typedef struct {
    char input[20];
    CalcNumber total, operand, memory;
    int pending, repeat, fresh, error;
} Calculator;
enum { CALC_MC = 0x2000, CALC_MR, CALC_MINUS, CALC_PLUS, CALC_CE, CALC_SIGN };
void calc_init(Calculator *calc);
int calc_key(Calculator *calc, int key);
int calc_paste(Calculator *calc, const char *text, int length);
const char *calc_text(const Calculator *calc);
void utility_icon(int id, int x, int y);
void utility_digits(int x, int y, const char *text, int color);

typedef struct { int year, month, day, weekday, hour, minute, second, valid; } DeskClock;
void clock_read(DeskClock *clock);
void clock_text(const DeskClock *clock, int twelve, int date, char *text);
void clock_draw(const DeskClock *clock, int twelve, int x, int y, int width);
#endif
