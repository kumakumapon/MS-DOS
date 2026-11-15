#ifndef KG_STOPWATCH_H
#define KG_STOPWATCH_H
/* One-second DOS clock, including midnight; capped at 99:59:59. */
typedef struct {unsigned elapsed,last,valid,running;} KmStopwatch;
static inline void km_stopwatch_tick(KmStopwatch *s,unsigned now){
 if(now>=86400)return;
 if(s->valid&&s->running){unsigned delta=now>=s->last?now-s->last:86400-s->last+now;
  s->elapsed=delta>359999-s->elapsed?359999:s->elapsed+delta;
 }
 s->last=now;s->valid=1;
}
#endif
