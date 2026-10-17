#include <assert.h>
#include "scroll.h"
int main(void) {
    for(int total=0;total<=1024;total+=7)for(int visible=1;visible<=20;visible++) {
        int end=total>visible?total-visible:0;
        assert(scroll_clamp(-9,total,visible)==0);
        assert(scroll_clamp(total+20,total,visible)==end);
        for(int track=16;track<=288;track+=17) {
            int thumb=scroll_thumb(total,visible,track),travel=track-thumb;
            assert(thumb>=8&&thumb<=track);
            assert(scroll_pixel(0,total,visible,track)==0);
            assert(scroll_pixel(end,total,visible,track)==travel);
            assert(scroll_row(-100,total,visible,track)==0);
            assert(scroll_row(track+100,total,visible,track)==end);
            int before=-1;
            for(int px=0;px<=travel;px++) {
                int row=scroll_row(px,total,visible,track);
                assert(row>=before&&row>=0&&row<=end);before=row;
            }
        }
    }
    return 0;
}
