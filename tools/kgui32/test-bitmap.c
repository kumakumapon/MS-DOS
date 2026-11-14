#include <assert.h>
#include <string.h>
#include "sdk/bitmap.h"
static void put(u8 *p,unsigned at,u32 v){for(unsigned i=0;i<4;i++)p[at+i]=v>>(i*8);}
int main(void){
 u8 h[54]={0};h[0]='B';h[1]='M';put(h,2,78);put(h,10,54);put(h,14,40);put(h,18,3);put(h,22,2);h[26]=1;h[28]=24;put(h,34,24);
 KgBmp b;assert(!kg_bmp_header(h,54,78,&b)&&b.width==3&&b.stride==12&&!b.topdown);
 u8 row[12]={0,0,255,0,255,0,255,0,0,0xaa,0xaa,0xaa};u16 pixels[8]={0x1234,0,0,0,0,0,0,0x5678};
 assert(!kg_bmp_row(&b,0,row,12,pixels+1,6)&&pixels[4]==0xf800&&pixels[5]==0x7e0&&pixels[6]==0x1f&&pixels[0]==0x1234&&pixels[7]==0x5678);
 assert(kg_bmp_row(&b,2,row,12,pixels+1,6)==-22&&kg_bmp_row(&b,0,row,11,pixels+1,6)==-22);
 put(h,22,(u32)-2);assert(!kg_bmp_header(h,54,78,&b)&&b.topdown);assert(!kg_bmp_row(&b,0,row,12,pixels+1,6)&&pixels[1]==0xf800);
 KgBmp before=b;u8 copy[54];memcpy(copy,h,54);
 for(unsigned at=0;at<54;at++){memcpy(h,copy,54);h[at]^=255;if(at<2||at==2||at==6||at==10||at==14||at==18||at==22||at==26||at==28||at==30||at==34)assert(kg_bmp_header(h,54,78,&b)==-22&&!memcmp(&b,&before,sizeof b));}
 memcpy(h,copy,54);assert(kg_bmp_header(h,53,78,&b)==-22&&kg_bmp_header(h,54,77,&b)==-22);
 put(h,18,257);assert(kg_bmp_header(h,54,78,&b)==-22);put(h,18,3);put(h,22,0x80000000);assert(kg_bmp_header(h,54,78,&b)==-22);
 u16 original[6];memcpy(original,pixels+1,sizeof original);kg_bitmap_invert(pixels+1,6);kg_bitmap_invert(pixels+1,6);assert(!memcmp(original,pixels+1,sizeof original));kg_bitmap_flip(pixels+1,3,2);assert(pixels[1]==original[2]&&pixels[3]==original[0]);kg_bitmap_flip(pixels+1,3,2);assert(!memcmp(original,pixels+1,sizeof original));
 u16 clipped[11];for(unsigned i=0;i<11;i++)clipped[i]=0x1234;
 assert(!kg_bitmap_fill(clipped+1,3,3,-2,-2,3,3,0xabcd));assert(clipped[1]==0xabcd&&clipped[2]==0x1234&&clipped[0]==0x1234&&clipped[10]==0x1234);
 assert(!kg_bitmap_fill(clipped+1,3,3,0x7fffffff,0x7fffffff,0x7fffffff,0x7fffffff,0));assert(clipped[1]==0xabcd);
 assert(!kg_bitmap_line(clipped+1,3,3,-1,-1,3,3,0x5678));assert(clipped[1]==0x5678&&clipped[9]==0x5678&&clipped[0]==0x1234&&clipped[10]==0x1234);
 assert(kg_bitmap_line(clipped+1,3,3,-2147483647,0,1,1,0)==-22);
 static u16 all[256*192],decoded[256*192];static u8 encoded[54+256*192*3];
 for(unsigned pass=0;pass<2;pass++){
  for(unsigned i=0;i<256*192;i++)all[i]=(u16)(i+pass*32768);
  int n=kg_bmp_encode(all,256,192,encoded,sizeof encoded);assert(n==sizeof encoded);
  assert(!kg_bmp_header(encoded,54,n,&b));
  for(unsigned y=0;y<192;y++)assert(!kg_bmp_row(&b,y,encoded+54+y*768,768,decoded,256*192));
  assert(!memcmp(all,decoded,sizeof all));
 }
 assert(kg_bmp_encode(all,385,1,encoded,sizeof encoded)==-22);
 assert(kg_bmp_encode(all,256,192,encoded,54)==-28);
 static u16 large[KG_BITMAP_PIXELS],again[KG_BITMAP_PIXELS];static u8 large_bmp[54+KG_BITMAP_PIXELS*3];
 for(unsigned i=0;i<KG_BITMAP_PIXELS;i++)large[i]=(u16)(i*19);
 int bytes=kg_bmp_encode(large,384,256,large_bmp,sizeof large_bmp);assert(bytes==sizeof large_bmp);
 assert(!kg_bmp_decode(large_bmp,bytes,&b,again,KG_BITMAP_PIXELS)&&!memcmp(large,again,sizeof large));
 u8 small[80]={0};small[0]='B';small[1]='M';put(small,14,40);put(small,18,2);put(small,22,1);small[26]=1;
 put(small,2,66);put(small,10,62);small[28]=8;put(small,46,2);small[56]=255;small[59]=255;small[62]=0;small[63]=1;
 assert(!kg_bmp_decode(small,66,&b,pixels,8)&&pixels[0]==0xf800&&pixels[1]==0x7e0);
 small[63]=2;u16 safe[8];memcpy(safe,pixels,sizeof safe);KgBmp safe_plan=b;assert(kg_bmp_decode(small,66,&b,pixels,8)==-22&&!memcmp(safe,pixels,sizeof safe)&&!memcmp(&b,&safe_plan,sizeof b));
 memset(small+46,0,34);put(small,2,58);put(small,10,54);small[28]=16;small[54]=0;small[55]=0x7c;small[56]=0xe0;small[57]=3;
 assert(!kg_bmp_decode(small,58,&b,pixels,8)&&pixels[0]==0xf800&&pixels[1]==0x7e0);
 put(small,2,62);small[28]=32;small[54]=small[55]=0;small[56]=255;small[57]=0xaa;small[58]=0;small[59]=255;small[60]=0;small[61]=0x55;
 assert(!kg_bmp_decode(small,62,&b,pixels,8)&&pixels[0]==0xf800&&pixels[1]==0x7e0);
 put(small,30,1);assert(kg_bmp_decode(small,62,&b,pixels,8)==-22);
 return 0;
}
