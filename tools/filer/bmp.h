/* Header policy shared by the privileged writer and unprivileged decoder.
 * Caller supplies 54 readable bytes; unsigned arithmetic is bounded first. */
#ifndef PC98_BMP24_H
#define PC98_BMP24_H
static unsigned bmp32(const unsigned char *p){return p[0]|(unsigned)p[1]<<8|(unsigned)p[2]<<16|(unsigned)p[3]<<24;}
static int bmp24_valid(const unsigned char *p,unsigned n){
 if(p[0]!='B'||p[1]!='M'||bmp32(p+2)!=n||bmp32(p+6)||bmp32(p+14)!=40||p[26]!=1||p[27]||p[28]!=24||p[29]||bmp32(p+30))return 0;
 unsigned w=bmp32(p+18);int h=(int)bmp32(p+22);
 if(!w||w>256||!h||h < -192||h>192)return 0;
 unsigned rows=h<0?(unsigned)-h:(unsigned)h,offset=bmp32(p+10),size=((w*3+3)&~3u)*rows;
 return offset>=54&&offset<=4096&&offset<=n&&size==n-offset&&(!bmp32(p+34)||bmp32(p+34)==size);
}
#endif
