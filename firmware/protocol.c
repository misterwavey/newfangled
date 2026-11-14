#include "pnw_board.h"
bool pnw_dac_frame(unsigned channel,unsigned code,uint16_t *frame) {
    if(channel>=8 || code>1023 || !frame) return false;
    *frame=(uint16_t)((channel<<12)|(code<<2));
    return true;
}
