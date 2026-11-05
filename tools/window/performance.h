#ifndef KDESK_PERFORMANCE_H
#define KDESK_PERFORMANCE_H
#include "hardware.h"
/* Optional debugger record, not an application ABI. Read ready=1/busy=0.
 * Busy covers temporary text/attribute writes through occluder restoration,
 * including incremental drawing. dos_updates uses the former reserved word.
 * ARTIC differences cover less than one 24-bit wrap. */
typedef struct {
    char signature[8];u16 version,size,busy,ready;
    u32 owner,cookie,schedule_calls,schedule_gap_max;
    u32 full_scenes,full_scene_ticks,clock_updates,clock_preserves;
    u32 full_scene_max,dos_updates;
    u16 width,height;u32 reserved2;
} KDeskPerformance;
_Static_assert(sizeof(KDeskPerformance)==64,"KDESK performance record");
#endif
