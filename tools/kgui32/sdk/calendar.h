#ifndef KGUI32_CALENDAR_H
#define KGUI32_CALENDAR_H

static inline unsigned calendar_days(unsigned year, unsigned month){
 static const unsigned days[]={31,28,31,30,31,30,31,31,30,31,30,31};
 unsigned leap=year%4==0&&(year%100!=0||year%400==0);
 return days[month-1]+(month==2&&leap);
}

/* Gregorian calendar, Sunday = zero. DOS date years are 1980–2099. */
static inline unsigned calendar_weekday(unsigned year, unsigned month){
 static const unsigned offset[]={0,3,2,5,0,3,5,1,4,6,2,4};
 if(month<3)year--;
 return (year+year/4-year/100+year/400+offset[month-1]+1)%7;
}

static inline int calendar_move(unsigned *year,unsigned *month,int delta){
 int index=(int)(*year-1980)*12+(int)*month-1+delta;
 if(index<0||index>=120*12)return 0;
 *year=1980+(unsigned)index/12;
 *month=(unsigned)index%12+1;
 return 1;
}
#endif
