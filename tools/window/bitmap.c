#include "bitmap.h"
#include "../k38632/io.h"
#ifdef KDESK_BANK_TOOLS
#include "../k386/backing.h"
#include "../k386/xmem.h"
#else
/* Standalone KVIEW/KPAINT keep their conventional, indexed image format. */
#define xmem_free(handle) (void)(handle)
static int xmem_read(u16 handle,u32 at,void *out,unsigned count){(void)handle;(void)at;(void)out;(void)count;return 0;}
static int xmem_write(u16 handle,u32 at,const void *in,unsigned count){(void)handle;(void)at;(void)in;(void)count;return 0;}
#endif

static u8 row[960], header[54], mapping[256];
static u16 palette[256];
static u16 word(const u8 *p) { return p[0] | (u16)p[1] << 8; }
static u32 dword(const u8 *p) { return word(p) | (u32)word(p + 2) << 16; }
static void setword(u8 *p, unsigned n) { p[0] = n; p[1] = n >> 8; }
static void setdword(u8 *p, u32 n) { setword(p, n); setword(p + 2, n >> 16); }
static u16 rgb16(u32 color) { return (color >> 19) << 11 | (color >> 10 & 63) << 5 | (color >> 3 & 31); }
static u32 palette_base(const Bitmap *image){return image->width*(u32)image->height*2;}
static unsigned data_segment(void){unsigned ds;__asm__ volatile("movw %%ds,%w0":"=r"(ds));return ds;}
void bitmap_free(Bitmap *image) { if(image->rgb)xmem_free(image->rgb);mem_free(image->segment); *image = (Bitmap){0}; }
int bitmap_new(Bitmap *image, int width, int height, int color) {
    if (width < 1 || width > IMAGE_WIDTH || height < 1 || height > IMAGE_HEIGHT) return 0;
    unsigned segment = mem_alloc(width * (u32)height);
    if (!segment) return 0;
    mem_fill(segment, 0, width * (u32)height, color);
    bitmap_free(image); *image = (Bitmap){.segment=segment,.width=width,.height=height}; return 1;
}
int bitmap_enable_rgb(Bitmap *image) {
#ifdef KDESK_BANK_TOOLS
    if(image->rgb)return 1;
    u16 handle;
    unsigned bytes=palette_base(image)+(image->segment?0:palette_base(image)/2);
    if(!backing_init()||!xmem_new((bytes+1023)/1024,&handle))return 0;
    image->rgb=handle;return 1;
#else
    (void)image;return 0;
#endif
}
static int write_indices(Bitmap *image,u32 at,const u8 *pixels,unsigned count){
    if(!image->segment)return xmem_write(image->rgb,palette_base(image)+at,pixels,count);
    mem_copy(image->segment,at,data_segment(),(unsigned)pixels,count);return 1;
}
int bitmap_read_indices(const Bitmap *image,u32 at,u8 *pixels,unsigned count){
    if(!image->segment)return xmem_read(image->rgb,palette_base(image)+at,pixels,count);
    mem_copy(data_segment(),(unsigned)pixels,image->segment,at,count);return 1;
}
unsigned bitmap_get(const Bitmap *image,u32 at){
    u8 index=0;
    if(image->segment)return mem_get(image->segment,at);
    bitmap_read_indices(image,at,&index,1);return index;
}
void bitmap_set(Bitmap *image,u32 at,unsigned color) {
    if(image->rgb){
        u16 pixel=rgb16(gfx_rgb(color));
        if(!xmem_write(image->rgb,at*2,&pixel,2))return;
    }
    if(image->segment)mem_put(image->segment,at,color);
    else {u8 index=color;write_indices(image,at,&index,1);}
}
int bitmap_read_rgb(const Bitmap *image,u32 at,u16 *pixels,unsigned count) {
    if(image->rgb)return xmem_read(image->rgb,at*2,pixels,count*2u);
    for(unsigned i=0;i<count;i++)pixels[i]=rgb16(gfx_rgb(bitmap_get(image,at+i)));
    return 1;
}
int bitmap_transfer(Bitmap *out,u32 to,const Bitmap *in,u32 from,unsigned count) {
    if(count>IMAGE_WIDTH)return 0;
    if(out->rgb){
        u16 pixels[IMAGE_WIDTH];
        if(!bitmap_read_rgb(in,from,pixels,count)||!xmem_write(out->rgb,to*2,pixels,count*2u))return 0;
    }
    if(out->segment&&in->segment){mem_copy(out->segment,to,in->segment,from,count);return 1;}
    u8 indexed[IMAGE_WIDTH];
    return bitmap_read_indices(in,from,indexed,count)&&write_indices(out,to,indexed,count);
}
int bitmap_clone(Bitmap *out, const Bitmap *in) {
    Bitmap next={.width=in->width,.height=in->height};
    if(in->rgb){if(!bitmap_enable_rgb(&next))return 0;}
    else if(!bitmap_new(&next,in->width,in->height,WHITE))return 0;
    for(int y=0;y<in->height;y++)if(!bitmap_transfer(&next,y*(u32)in->width,in,y*(u32)in->width,in->width)){
        bitmap_free(&next);return 0;
    }
    bitmap_free(out); *out = next; return 1;
}
static int load_bitmap(Bitmap *image, const char *path,int shared) {
    int handle = p_open(path), error = -11; Bitmap next = {0}; Entry file;
    if (handle < 0) return handle;
    if (p_read(handle, header, 54) != 54 || header[0] != 'B' || header[1] != 'M') goto finish;
    u32 dib = dword(header + 14), offset = dword(header + 10), colors = dword(header + 46);
    int width = (int)dword(header + 18), signed_height = (int)dword(header + 22);
    unsigned bits = word(header + 28), height = signed_height < 0 ? -(u32)signed_height : (u32)signed_height;
    if (dib < 40 || dib > 124 || width < 1 || width > IMAGE_WIDTH || height < 1 || height > IMAGE_HEIGHT ||
        word(header + 26) != 1 || dword(header + 30) != 0 || !(bits == 1 || bits == 4 || bits == 8 || bits == 24)) goto finish;
    if(shared&&((unsigned)width>KG_SHARED_WIDTH||height>KG_SHARED_HEIGHT)){error=-27;goto finish;}
    u32 stride = ((width * bits + 31) / 32) * 4;
    if (p_stat(path, &file) < 0 || offset > file.size || height * stride > file.size - offset) goto finish;
    if (bits <= 8) {
        if (!colors) colors = 1u << bits;
        if (colors > (1u << bits) || offset < 14 + dib + colors * 4 || p_seek(handle, 14 + dib) < 0) goto finish;
        for (unsigned i = 0; i < colors; i++) {
            if (p_read(handle, row, 4) != 4) goto finish;
            mapping[i] = gfx_nearest(row[2], row[1], row[0]);
            palette[i] = rgb16((u32)row[2]<<16|(u16)row[1]<<8|row[0]);
        }
    } else if (offset < 14 + dib) goto finish;
    if(shared){
        next.width=width;next.height=height;
        if(!bitmap_enable_rgb(&next)){error=-8;goto finish;}
    }else if(!bitmap_new(&next,width,height,WHITE)){error=-8;goto finish;}
    if (p_seek(handle, offset) < 0) goto finish;
    u16 pixels[IMAGE_WIDTH];u8 indexed[IMAGE_WIDTH];
    for (unsigned y = 0; y < height; y++) {
        if (p_read(handle, row, stride) != (int)stride) goto finish;
        unsigned to = signed_height < 0 ? y : height - y - 1;
        for (int x = 0; x < width; x++) {
            unsigned color,rgb;
            if (bits == 24) {
                const u8 *p=row+x*3;
                color=gfx_nearest(p[2],p[1],p[0]);rgb=rgb16((u32)p[2]<<16|(u16)p[1]<<8|p[0]);
            }
            else {
                unsigned index = bits == 8 ? row[x] : bits == 4 ? (row[x / 2] >> ((1 - (x & 1)) * 4)) & 15 : (row[x / 8] >> (7 - (x & 7))) & 1;
                if (index >= colors) goto finish;
                color = mapping[index];rgb=palette[index];
            }
            indexed[x]=color;
            pixels[x]=rgb;
        }
        u32 at=to*(u32)width;
        if(!write_indices(&next,at,indexed,width))goto finish;
        if(shared&&!xmem_write(next.rgb,at*2,pixels,width*2u))goto finish;
        input_poll();
    }
    bitmap_free(image); *image = next; next = (Bitmap){0}; error = 0;
finish:
    p_close(handle); bitmap_free(&next); return error;
}
int bitmap_load(Bitmap *image,const char *path) {return load_bitmap(image,path,0);}
int bitmap_load_shared(Bitmap *image,const char *path) {return load_bitmap(image,path,1);}

static int save_bitmap(const Bitmap *image, const char *path,int shared) {
    char parent[FD_PATH], temp[FD_PATH], backup[FD_PATH]; Entry entry;
    fd_text(parent, path, sizeof parent); fd_parent(parent);
    if (fd_path(temp, parent, "KDIMG.TMP") || fd_path(backup, parent, "KDIMG.BAK")) return -FD_BADNAME;
    if (!fd_cmp(path, temp) || !fd_cmp(path, backup)) return -FD_BADNAME;
    int exists = p_stat(path, &entry);
    if (!exists && (entry.attr & 1)) return -5;
    int old = p_stat(backup, &entry);
    if ((exists < 0 && exists != -2) || old != -2) return -FD_EXISTS;
    int handle = p_create(temp);
    if (handle < 0) return handle;
    unsigned stride=(image->width*(shared?3:1)+3)&~3u,offset=shared?54:1078;
    u32 size=offset+stride*(u32)image->height;
    for (unsigned i = 0; i < sizeof header; i++) header[i] = 0;
    header[0] = 'B'; header[1] = 'M'; setdword(header + 2, size); setdword(header + 10, offset);
    setdword(header + 14, 40); setdword(header + 18, image->width); setdword(header + 22, image->height);
    setword(header + 26, 1); setword(header + 28, shared?24:8); setdword(header + 34, size-offset); setdword(header + 46, shared?0:256);
    int success = p_write(handle, header, 54) == 54;
    u16 pixels[IMAGE_WIDTH];
    for (int i = 0; !shared && i < 256 && success; i++) {
        u32 rgb = gfx_rgb(i); row[0] = rgb; row[1] = rgb >> 8; row[2] = rgb >> 16; row[3] = 0;
        success = p_write(handle, row, 4) == 4;
    }
    for (int y = image->height - 1; y >= 0 && success; y--) {
        u32 first=y*(u32)image->width;
        if(shared&&!bitmap_read_rgb(image,first,pixels,image->width)){success=0;break;}
        for(unsigned x=0;x<stride;x++)row[x]=0;
        if(!shared&&!bitmap_read_indices(image,first,row,image->width)){success=0;break;}
        if(shared)for(int x=0;x<image->width;x++){
                unsigned v=pixels[x],b=v&31,g=v>>5&63,r=v>>11;
                row[x*3]=(b<<3)|(b>>2);row[x*3+1]=(g<<2)|(g>>4);row[x*3+2]=(r<<3)|(r>>2);
        }
        success = p_write(handle, row, stride) == (int)stride; input_poll();
    }
    if (p_close(handle) < 0) success = 0;
    if (!success) { p_unlink(temp); return -29; }
    if (!exists && p_rename(path, backup) < 0) { p_unlink(temp); return -5; }
    if (p_rename(temp, path) < 0) {
        if (!exists && p_rename(backup, path) < 0) return -FD_ROLLBACK;
        p_unlink(temp); return -5;
    }
    if (!exists && p_unlink(backup) < 0) return 1;
    return 0;
}
int bitmap_save(const Bitmap *image,const char *path) {return save_bitmap(image,path,0);}
int bitmap_save_shared(const Bitmap *image,const char *path) {return save_bitmap(image,path,1);}
