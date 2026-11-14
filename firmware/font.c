/* SPDX-License-Identifier: MIT
 * Original, manually authored 5x7 row drawings, 2026-11-14.
 * No glyphs extracted from ALM firmware or imported font libraries.
 * Each five-bit row runs left (bit 4) to right (bit 0).
 */
#include "font.h"
#include "pnw_board.h"
#include <stdint.h>
typedef struct { char letter; uint8_t rows[7]; } Glyph;
static const Glyph glyphs[]={
 {' ',{0,0,0,0,0,0,0}},
 {'A',{14,17,17,31,17,17,17}}, {'B',{30,17,17,30,17,17,30}},
 {'C',{15,16,16,16,16,16,15}}, {'D',{28,18,17,17,17,18,28}},
 {'E',{31,16,16,30,16,16,31}}, {'F',{31,16,16,30,16,16,16}},
 {'G',{14,17,16,23,17,17,14}}, {'H',{17,17,17,31,17,17,17}},
 {'I',{14,4,4,4,4,4,14}}, {'J',{7,2,2,2,18,18,12}},
 {'K',{17,18,20,24,20,18,17}}, {'L',{16,16,16,16,16,16,31}},
 {'M',{17,27,21,21,17,17,17}}, {'N',{17,25,25,21,19,19,17}},
 {'O',{14,17,17,17,17,17,14}}, {'P',{30,17,17,30,16,16,16}},
 {'Q',{14,17,17,17,21,18,13}}, {'R',{30,17,17,30,20,18,17}},
 {'S',{15,16,16,14,1,1,30}}, {'T',{31,4,4,4,4,4,4}},
 {'U',{17,17,17,17,17,17,14}}, {'V',{17,17,17,17,17,10,4}},
 {'W',{17,17,17,21,21,27,17}}, {'X',{17,17,10,4,10,17,17}},
 {'Y',{17,17,10,4,4,4,4}}, {'Z',{31,1,2,4,8,16,31}},
 {'0',{14,17,19,21,25,17,14}}, {'1',{4,12,4,4,4,4,14}},
 {'2',{14,17,1,2,4,8,31}}, {'3',{30,1,1,14,1,1,30}},
 {'4',{2,6,10,18,31,2,2}}, {'5',{31,16,16,30,1,1,30}},
 {'6',{14,16,16,30,17,17,14}}, {'7',{31,1,2,4,8,8,8}},
 {'8',{14,17,17,14,17,17,14}}, {'9',{14,17,17,15,1,1,14}},
 {'?',{14,17,1,2,4,0,4}}, {'-',{0,0,0,31,0,0,0}},
 {'/',{1,2,2,4,8,8,16}}, {':',{0,4,4,0,4,4,0}},
 {'.',{0,0,0,0,0,6,6}}, {'>',{16,8,4,2,4,8,16}},
 {'+',{0,4,4,31,4,4,0}}, {'%',{25,26,2,4,8,22,6}}
};
static const uint8_t *rows(char letter) {
    if(letter>='a' && letter<='z')letter=(char)(letter-'a'+'A');
    for(unsigned i=0;i<sizeof(glyphs)/sizeof(glyphs[0]);++i)
        if(glyphs[i].letter==letter)return glyphs[i].rows;
    return rows('?');
}
/* Transparent glyphs; callers clear the framebuffer before composing a page.
 * Entirely off-screen glyphs stop rendering; individual pixels are clipped. */
unsigned original_text(unsigned x,unsigned y,const char *text,unsigned scale) {
    if(!text || scale<1 || scale>3 || x>=128 || y>=32)return x;
    while(*text && x<128) {
        const uint8_t *glyph=rows(*text++);
        for(unsigned row=0;row<7;++row)for(unsigned col=0;col<5;++col) {
            if(!(glyph[row]&(16u>>col)))continue;
            for(unsigned dy=0;dy<scale;++dy)for(unsigned dx=0;dx<scale;++dx) {
                unsigned px=x+col*scale+dx,py=y+row*scale+dy;
                if(px<128 && py<32)pnw_oled_pixel(px,py,true);
            }
        }
        x+=6*scale;
    }
    return x;
}
