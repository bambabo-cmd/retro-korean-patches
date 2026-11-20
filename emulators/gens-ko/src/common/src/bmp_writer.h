/* Gens KO 2026-11-20: bounded BMP encoder, GPLv2. */
#ifndef GENS_BMP_WRITER_H
#define GENS_BMP_WRITER_H
#include <vector>
#include <stdint.h>
#include <stddef.h>
static inline void gens_bmp_u32(std::vector<unsigned char>&out,size_t at,uint32_t v){for(int i=0;i<4;++i)out[at+i]=(unsigned char)(v>>(i*8));}
static inline bool gens_bmp_encode(const unsigned char *screen,bool rgb555,int width,int height,int pitch,std::vector<unsigned char>&out){
    out.clear();if(!screen||width<=0||height<=0||width>8192||height>8192||pitch<width*2)return false;
    size_t row=((size_t)width*3+3)&~(size_t)3,total=54+row*height;if(total>0xFFFFFFFFu)return false;
    out.assign(total,0);out[0]='B';out[1]='M';gens_bmp_u32(out,2,(uint32_t)total);gens_bmp_u32(out,10,54);gens_bmp_u32(out,14,40);
    gens_bmp_u32(out,18,(uint32_t)width);gens_bmp_u32(out,22,(uint32_t)height);out[26]=1;out[28]=24;gens_bmp_u32(out,34,(uint32_t)(row*height));
    for(int y=0;y<height;++y){const unsigned char *src=screen+(size_t)(height-1-y)*pitch;unsigned char *dst=out.data()+54+(size_t)y*row;
        for(int x=0;x<width;++x){unsigned v=src[x*2]|(src[x*2+1]<<8);unsigned r=(v>>(rgb555?10:11))&31,g=(v>>5)&(rgb555?31:63),b=v&31;
            dst[x*3]=(unsigned char)((b<<3)|(b>>2));dst[x*3+1]=(unsigned char)(rgb555?((g<<3)|(g>>2)):((g<<2)|(g>>4)));dst[x*3+2]=(unsigned char)((r<<3)|(r>>2));}}
    return true;
}
#endif
