#include "utilities.h"

#define SCALE 10000ULL
#define LIMIT 999999999999ULL
/* Keep the real-mode COM independent of compiler 64-bit division helpers. */
static unsigned long long divide(unsigned long long n, unsigned long long d, unsigned long long *remainder) {
    unsigned long long q = 0, r = 0;
    for (int bit = 63; bit >= 0; bit--) {
        r = (r << 1) | ((n >> bit) & 1);
        if (r >= d) { r -= d; q |= 1ULL << bit; }
    }
    if (remainder) *remainder = r;
    return q;
}
static unsigned long long magnitude(CalcNumber n) { return n < 0 ? -n : n; }
static int parse(const char *text, CalcNumber *out) {
    int sign = 1, whole = 0, fraction = 0, dot = 0, digits = 0;
    unsigned long long value = 0;
    if (*text == '-' || *text == '+') { if (*text++ == '-') sign = -1; }
    for (; *text; text++) {
        if (*text == '.' && !dot) { dot = 1; continue; }
        if (*text < '0' || *text > '9') return 0;
        if (dot ? ++fraction > 4 : ++whole > 8) return 0;
        value = value * 10 + *text - '0'; digits++;
    }
    if (!digits) return 0;
    for (; fraction < 4; fraction++) value *= 10;
    if (value > LIMIT) return 0;
    *out = sign < 0 ? -(CalcNumber)value : (CalcNumber)value; return 1;
}
static void format(Calculator *calc, CalcNumber value) {
    char *out = calc->input; unsigned long long fraction;
    unsigned integer = divide(magnitude(value), SCALE, &fraction);
    if (value < 0) *out++ = '-';
    fd_number(out, integer); out += fd_len(out);
    if (fraction) {
        *out++ = '.';
        unsigned f = fraction;
        for (unsigned place = 1000; place; place /= 10) { *out++ = '0' + f / place; f %= place; }
        while (out[-1] == '0') out--;
    }
    *out = 0;
}
static int operation(Calculator *calc, CalcNumber a, CalcNumber b, int op, CalcNumber *out) {
    CalcNumber result;
    if (op == '+') result = a + b;
    else if (op == '-') result = a - b;
    else {
        unsigned long long aa = magnitude(a), bb = magnitude(b), value, remainder;
        if (op == '*') {
            /* Bound the product before multiplying; both inputs can have 12 digits. */
            if (bb && aa > divide(LIMIT * SCALE, bb, 0)) { calc->error = 2; return 0; }
            value = divide(aa * bb + SCALE / 2, SCALE, 0);
        } else {
            if (!bb) { calc->error = 1; return 0; }
            value = divide(aa * SCALE, bb, &remainder);
            if (remainder * 2 >= bb) value++;
        }
        if (value > LIMIT) { calc->error = 2; return 0; }
        result = (a < 0) != (b < 0) ? -(CalcNumber)value : (CalcNumber)value;
    }
    if (magnitude(result) > LIMIT) { calc->error = 2; return 0; }
    *out = result; return 1;
}
void calc_init(Calculator *calc) { *calc = (Calculator){.input = "0", .fresh = 1}; }
const char *calc_text(const Calculator *calc) {
    return calc->error == 1 ? "Divide by zero" : calc->error ? "Overflow" : calc->input;
}
int calc_key(Calculator *calc, int key) {
    CalcNumber value = 0;
    if (key == 'c' || key == 'C') { CalcNumber memory = calc->memory; calc_init(calc); calc->memory = memory; return 1; }
    if (key == CALC_MC) { calc->memory = 0; return 1; }
    if (key == CALC_MR) { calc->error = 0; format(calc, calc->memory); calc->fresh = 1; if(!calc->pending)calc->repeat=0; return 1; }
    if (key == CALC_CE) { calc->error = 0; fd_text(calc->input, "0", sizeof calc->input); calc->fresh = 0; return 1; }
    if ((key >= '0' && key <= '9') || key == '.') {
        if (calc->error) { CalcNumber memory = calc->memory; calc_init(calc); calc->memory = memory; }
        if(calc->fresh && !calc->pending)calc->repeat=0;
        char next[20]; fd_text(next, calc->fresh ? "0" : calc->input, sizeof next);
        int length = fd_len(next), start = next[0] == '-';
        if (key == '.') { for (int i = 0; next[i]; i++) if (next[i] == '.') return 1; }
        else if (length == start + 1 && next[start] == '0') length = start;
        if (length + 1 >= (int)sizeof next) return 2;
        next[length++] = key; next[length] = 0;
        if (!parse(next, &value)) return 2;
        fd_text(calc->input, next, sizeof calc->input); calc->fresh = 0; return 1;
    }
    if (calc->error) return 0;
    parse(calc->input, &value);
    if (key == 8) {
        int n = fd_len(calc->input);
        if (calc->fresh || n <= 1 || (n == 2 && calc->input[0] == '-')) fd_text(calc->input, "0", sizeof calc->input);
        else calc->input[n - 1] = 0;
        calc->fresh = 0; return 1;
    }
    if (key == CALC_SIGN || key == 's' || key == 'S') {
        if (calc->input[0] == '-') { for (int i = 0; calc->input[i]; i++) calc->input[i] = calc->input[i + 1]; }
        else { int n = fd_len(calc->input); for (int i = n; i >= 0; i--) calc->input[i + 1] = calc->input[i]; calc->input[0] = '-'; }
        calc->fresh = 0; return 1;
    }
    if (key == '%') {
        unsigned long long scaled = divide(magnitude(value) + 50, 100, 0);
        format(calc, value < 0 ? -(CalcNumber)scaled : (CalcNumber)scaled); calc->fresh = 1; return 1;
    }
    if (key == CALC_PLUS || key == CALC_MINUS) {
        CalcNumber memory;
        if (operation(calc, calc->memory, value, key == CALC_PLUS ? '+' : '-', &memory)) calc->memory = memory;
        return 1;
    }
    if (key == '+' || key == '-' || key == '*' || key == '/') {
        if (calc->pending && !calc->fresh && !operation(calc, calc->total, value, calc->pending, &value)) return 1;
        calc->total = value; format(calc, value); calc->pending = key; calc->repeat = 0; calc->fresh = 1; return 1;
    }
    if (key == '=' || key == 13) {
        int op = calc->pending ? calc->pending : calc->repeat;
        if (calc->pending) { calc->operand = value; calc->repeat = calc->pending; }
        if (op && !operation(calc, calc->pending ? calc->total : value, calc->operand, op, &value)) return 1;
        calc->total = value; format(calc, value); calc->pending = 0; calc->fresh = 1; return 1;
    }
    return 0;
}
int calc_paste(Calculator *calc, const char *text, int length) {
    while (length && (*text == ' ' || *text == '\r' || *text == '\n')) { text++; length--; }
    while (length && (text[length-1] == ' ' || text[length-1] == '\r' || text[length-1] == '\n')) length--;
    if (length < 1 || length >= 20) return 0;
    char input[20]; for (int i = 0; i < length; i++) {if(!text[i])return 0;input[i] = text[i];} input[length] = 0;
    CalcNumber value; if (!parse(input, &value)) return 0;
    calc->error = 0; format(calc, value); calc->fresh = 1; if(!calc->pending)calc->repeat=0; return 1;
}

#ifndef CALCULATOR_TEST
/* Original 16x16 monochrome drawings, enlarged to 32x32 in the icon tiles. */
static const u16 icons[9][16] = {
 {0,0x7c00,0x4600,0x43fc,0x4004,0x7ffc,0x4004,0x4004,0x4004,0x4004,0x4004,0x4004,0x7ffc,0,0,0},
 {0,0x3ff0,0x2018,0x201c,0x2004,0x2ff4,0x2004,0x2ff4,0x2004,0x2ff4,0x2004,0x2ff4,0x2004,0x3ffc,0,0},
 {0,0x0180,0x05a0,0x1ff8,0x0c30,0x381c,0x1198,0x33cc,0x33cc,0x1198,0x381c,0x0c30,0x1ff8,0x05a0,0x0180,0},
 {0,0x7ffc,0x4424,0x4424,0x47e4,0x4004,0x4004,0x4ff4,0x4814,0x4814,0x4814,0x4814,0x7ffc,0,0,0},
 {0,0x7ffc,0x4004,0x4004,0x5004,0x4804,0x4404,0x4804,0x5004,0x41e4,0x4004,0x4004,0x7ffc,0,0,0},
 {0,0x7ffc,0x4004,0x4304,0x4304,0x4004,0x4014,0x4034,0x4c64,0x5e44,0x73c4,0x6184,0x7ffc,0,0,0},
 {0,0x000c,0x001c,0x0038,0x0070,0x00e0,0x01c0,0x0380,0x0700,0x0f00,0x1e00,0x3c00,0x3800,0x7000,0x6000,0},
 {0,0x3ffc,0x2004,0x2ff4,0x2814,0x2ff4,0x2004,0x2924,0x2924,0x2004,0x2924,0x2924,0x2004,0x3ffc,0,0},
 {0,0x03c0,0x0c30,0x1008,0x2184,0x2184,0x4182,0x4182,0x41e2,0x4002,0x2004,0x2004,0x1008,0x0c30,0x03c0,0},
};
void utility_icon(int id, int x, int y) {
    static const int colors[] = {GOLD,ACCENT,HANDLE,SELECTED,INK,DESKTOP_LIGHT,ACCENT,SELECTED,DESKTOP};
    gfx_rect(x+2,y+2,32,32,SHADOW); gfx_rect(x,y,32,32,colors[id]);
    if(gfx_colors()==256) {
        u8 scan[32];u16 segment;__asm__ volatile("movw %%ds,%0":"=r"(segment));
        for(int row=0;row<16;row++) {
            for(int col=0;col<16;col++)scan[col*2]=scan[col*2+1]=icons[id][row]&(0x8000>>col)?WHITE:colors[id];
            gfx_write_span(x,y+row*2,segment,(unsigned)scan,32);gfx_write_span(x,y+row*2+1,segment,(unsigned)scan,32);
            if(!(row&3))input_poll();
        }
    } else for(int row=0;row<16;row++)for(int col=0;col<16;col++)
        if(icons[id][row]&(0x8000>>col))gfx_rect(x+col*2,y+row*2,2,2,WHITE);
}
void utility_digits(int x,int y,const char *text,int color) {
    static const u8 segments[] = {0x3f,6,0x5b,0x4f,0x66,0x6d,0x7d,7,0x7f,0x6f};
    for(;*text;text++,x+=16) {
        if(*text=='.') {gfx_rect(x+2,y+21,3,3,color);continue;}
        if(*text==':') {gfx_rect(x+6,y+6,3,3,color);gfx_rect(x+6,y+17,3,3,color);continue;}
        unsigned bits=*text=='-'?0x40:*text>='0'&&*text<='9'?segments[*text-'0']:0;
        if(bits&1) gfx_rect(x+3,y,8,2,color);
        if(bits&2) gfx_rect(x+11,y+2,2,9,color);
        if(bits&4) gfx_rect(x+11,y+13,2,9,color);
        if(bits&8) gfx_rect(x+3,y+22,8,2,color);
        if(bits&16) gfx_rect(x+1,y+13,2,9,color);
        if(bits&32) gfx_rect(x+1,y+2,2,9,color);
        if(bits&64) gfx_rect(x+3,y+11,8,2,color);
    }
}
static void date_read(DeskClock *clock) {
    unsigned a=0x2a00,c=0,d=0;
    __asm__ volatile("int $0x21" : "+a"(a),"+c"(c),"+d"(d) :: "cc","memory");
    clock->year=c&65535;clock->month=(d>>8)&255;clock->day=d&255;clock->weekday=a&255;
}
void clock_read(DeskClock *clock) {
    date_read(clock);int day=clock->day;
    unsigned a=0x2c00,c=0,d=0;
    __asm__ volatile("int $0x21" : "+a"(a),"+c"(c),"+d"(d) :: "cc","memory");
    clock->hour=(c>>8)&255;clock->minute=c&255;clock->second=(d>>8)&255;
    date_read(clock);
    if(day!=clock->day) {
        a=0x2c00;c=d=0;
        __asm__ volatile("int $0x21" : "+a"(a),"+c"(c),"+d"(d) :: "cc","memory");
        clock->hour=(c>>8)&255;clock->minute=c&255;clock->second=(d>>8)&255;
    }
    clock->valid=clock->year>=1980&&clock->year<=2099&&clock->month>=1&&clock->month<=12&&clock->day>=1&&clock->day<=31&&clock->weekday<7&&clock->hour<24&&clock->minute<60&&clock->second<60;
}
static void two(char *out,int n) {out[0]='0'+n/10;out[1]='0'+n%10;}
void clock_text(const DeskClock *clock,int twelve,int date,char *text) {
    char *out=text;
    if(!clock->valid) {fd_text(text,"Clock unavailable",32);return;}
    if(date) {two(out,clock->year/100);two(out+2,clock->year%100);out[4]='-';two(out+5,clock->month);out[7]='-';two(out+8,clock->day);out[10]=' ';out+=11;}
    int hour=twelve?clock->hour%12:clock->hour;if(twelve&&!hour)hour=12;
    two(out,hour);out[2]=':';two(out+3,clock->minute);out[5]=':';two(out+6,clock->second);out+=8;
    if(twelve){*out++=' ';*out++=clock->hour<12?'A':'P';*out++='M';}*out=0;
}
static void stroke(int x,int y,int xx,int yy,int color) {
    int dx=xx>x?xx-x:x-xx,dy=yy>y?y-yy:yy-y,sx=x<xx?1:-1,sy=y<yy?1:-1,error=dx+dy;
    for(;;){gfx_rect(x,y,1,1,color);if(x==xx&&y==yy)break;int e=error*2;if(e>=dy){error+=dy;x+=sx;}if(e<=dx){error+=dx;y+=sy;}}
}
void clock_draw(const DeskClock *clock,int twelve,int x,int y,int width) {
    static const short sine[60]={0,105,208,309,407,500,588,669,743,809,866,914,951,978,995,1000,995,978,951,914,866,809,743,669,588,500,407,309,208,105,0,-105,-208,-309,-407,-500,-588,-669,-743,-809,-866,-914,-951,-978,-995,-1000,-995,-978,-951,-914,-866,-809,-743,-669,-588,-500,-407,-309,-208,-105};
    int center=x+width/2;char time[32];clock_text(clock,twelve,0,time);
    gfx_rect(center-100,y+48,200,136,PAPER);
    for(int i=0;i<14;i++)p_cell((center-64)/8+i,(y+48)/16,' ',0);
    if(!clock->valid){for(int i=0;time[i];i++)gfx_cell((center-80)/8+i,(y+64)/16,time[i],INK,PAPER,0);return;}
    char digits[9];for(int i=0;i<8;i++)digits[i]=time[i];digits[8]=0;
    utility_digits(center-64,y+50,digits,ACCENT);
    char date[32];clock_text(clock,0,1,date);date[10]=0;
    static const char *days[]={"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
    for(int i=0;date[i];i++)gfx_cell((center-64)/8+i,(y+80)/16,date[i],INK,PAPER,0);
    for(int i=0;i<3;i++)gfx_cell((center+24)/8+i,(y+80)/16,days[clock->weekday][i],INK,PAPER,0);
    if(twelve)for(int i=0;i<2;i++)gfx_cell((center+72)/8+i,(y+64)/16,time[9+i],INK,PAPER,0);
    for(int i=0;i<60;i++) {
        int xx=center+sine[i]*38/1000,yy=y+140-sine[(i+15)%60]*38/1000;
        gfx_rect(xx,yy,i%5?1:3,i%5?1:3,MUTED);
    }
    int hands[]={clock->hour%12*5+clock->minute/12,clock->minute,clock->second},lengths[]={20,28,32},colors[]={INK,ACCENT,GOLD};
    for(int i=0;i<3;i++)stroke(center,y+140,center+sine[hands[i]]*lengths[i]/1000,y+140-sine[(hands[i]+15)%60]*lengths[i]/1000,colors[i]);
    gfx_rect(center-2,y+138,4,4,INK);
    /* Mirror a complete value for screen readers and deterministic verification. */
    for(int i=0;time[i];i++)p_cell((center-64)/8+i,(y+48)/16,time[i],0);
}
#endif
