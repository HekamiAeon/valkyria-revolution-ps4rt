// SPDX-License-Identifier: GPL-2.0-or-later
/* Valkyria Revolution (CUSA06810) native canvas UI: the game's 2D UI on a native screen other
   than 1920x1080 (valkyria_wide_canvas.h): wider than 16:9 (1080 or 1440 high, up to 32:9) or
   another 16:9 size. The game draws its UI in authored 1920x1080 units that its GUI root
   scales by height/1080, so on a wider screen the UI would hug the left edge. Hooks on the
   textured (0x764860) and solid (0x765750) UI submits and the UI projection (0x235ce0) move
   authored UI into the centred 16:9 area, widen full-screen layers to the whole width, keep
   whole-screen pictures at 16:9 between black bars and keep talk portraits at their stock
   texel scale; two
   small code patches fix the world-map cursor and the narration paper. On a 16:9 canvas the
   margin is 0, so only the size-dependent parts act: full-screen copies drawn in stock pixels
   reach the native extent and the paper keeps its stock texel scale. Installed for an active
   native canvas (valkyria_wide_patches.h), after every signature matched. */
#define _GNU_SOURCE
#include <assert.h>
#include <dlfcn.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/uio.h>
#include <unistd.h>
#include "valkyria_wide_ui.h"

typedef void (*Submit)(uintptr_t,const void*,uint32_t,uint32_t,uintptr_t);
static Submit original_submit;
static Submit original_color_submit;
static uintptr_t guest_base;
#ifdef VALKYRIA_WIDE_UI_TEST
static uintptr_t test_project_caller;
#endif
typedef void (*Project)(float*,const float*);
static Project original_project;
static const unsigned char project_prologue[]={0x55,0x48,0x89,0xe5,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x53,0x48,0x83,0xec,0x18};
#ifdef VALKYRIA_WIDE_UI_TEST
static float test_project_width=3840,test_project_height=1080,test_project_scale=1;
static unsigned test_project_mode;
#endif
struct UiLayout { float width,height,sx,sy,margin,stretch; };
/* Native wide canvases, as the GPU's valkyria_wide_canvas.h: the game's own GUI heights 1080
   and 1440 and an integer width beyond 16:9 up to 32:9 (2560x1080, 3440x1440, 3840x1440,
   3840x1080, 5120x1440). Every correction below derives from this size. */
static int wide_canvas(float width,float height) {
    return (height==1080 || height==1440) && width>0 && width<=16384 && width==floorf(width) &&
           width*9>height*16 && width*9<=height*32;
}
/* The other native 16:9 sizes (1024x576, 1280x720, 2560x1440, 3840x2160): no side margin. */
static int standard_canvas(float width,float height) {
    return (height==576 || height==720 || height==1440 || height==2160) && width*9==height*16;
}
static int native_canvas(float width,float height) {
    return wide_canvas(width,height) || standard_canvas(width,height);
}
#ifdef VALKYRIA_WIDE_UI_TEST
static int wide_canvas_name(const char *text) {
    unsigned width=0,height=0;char x=0,extra=0;
    return text && sscanf(text,"%u%c%u%c",&width,&x,&height,&extra)==3 && x=='x' && wide_canvas((float)width,(float)height);
}
#endif
static int ui_layout(struct UiLayout *ui) {
#ifdef VALKYRIA_WIDE_UI_TEST
    ui->width=test_project_width;ui->height=test_project_height;
    ui->sx=test_project_scale;ui->sy=test_project_scale;
    unsigned mode=test_project_mode;
#else
    ui->width=*(const float*)(guest_base+0xa12f54);ui->height=*(const float*)(guest_base+0xa12f58);
    ui->sx=*(const float*)(guest_base+0x968c80);ui->sy=*(const float*)(guest_base+0x968c84);
    unsigned mode=*(const unsigned*)(guest_base+0xa1279c);
#endif
    if(mode&1){ui->sx*=4.f/3.f;ui->sy*=4.f/3.f;}
    if(!native_canvas(ui->width,ui->height))return 0;
    if(!isfinite(ui->sx) || !isfinite(ui->sy) || ui->sx<=0 || ui->sy<=0 ||
       fabsf(ui->sx-ui->height/1080.f)>0.0001f || fabsf(ui->sy-ui->sx)>0.0001f)return 0;
    ui->margin=(ui->width-1920.f*ui->sx)*0.5f;
    ui->stretch=ui->width/(1920.f*ui->sx);
    return 1;
}
static void project_forward(float *out,const float *world,uintptr_t caller) {
    original_project(out,world);
#ifdef VALKYRIA_WIDE_UI_TEST
    caller=test_project_caller;
#endif
    struct UiLayout ui;
    /* Native GUI root22e9a0 multiplies by this same scale; projection235ce0
       divides by it. Cancel exactly the later physical canvas translation, for the callers
       that store the projected point straight into a UI node position (node+0x138 or the
       placement helper 0x686a50) without testing it against the screen: enemy HUD, unit
       cursor/gun sight (+20 above the head), RIcher HUD, category marker and the rest below.
       Callers that bound-test or clamp the point in projection space (0x563b61/0x5642fb,
       0x67af9e, 0x6853e6) and the world-map cursor and pins stay in that space. */
    static const uint32_t placements[]={0x62a906,0x6a5bf4,0x63965f,0x686f43,0x684c42,0x6995a5,
                                        0x6b8ba7,0x6b8dc6,0x63a62b,0x4837f0,0x65f777};
    int placement=0;
    for(unsigned i=0;i<sizeof(placements)/sizeof(placements[0]);++i)placement|=caller==placements[i];
    if(!placement || !out || !ui_layout(&ui) || !isfinite(out[0]))return;
    out[0]-=ui.margin/ui.sx;
}
static __attribute__((noinline)) void project_submit(float *out,const float *world) {
    project_forward(out,world,(uintptr_t)__builtin_return_address(0)-guest_base);
}
static _Thread_local unsigned char *scratch;
static _Thread_local size_t scratch_capacity;
static const unsigned char prologue[]={0x55,0x48,0x89,0xe5,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x53,0x50};
#ifdef VALKYRIA_WIDE_UI_TEST
static uintptr_t test_caller;
#endif
static float read_float(const unsigned char *p) { float value;memcpy(&value,p,4);return value; }
static void write_float(unsigned char *p,float value) { memcpy(p,&value,4); }
/* Reads guest memory without faulting: an address that is not what it is taken for fails closed. */
static int guest_read(uintptr_t at,void *out,size_t size) {
    struct iovec local={out,size},remote={(void*)at,size};
    return at && process_vm_readv(getpid(),&local,1,&remote,1,0)==(ssize_t)size;
}
/* Glyph coordinates are local to an affine text matrix. Translate in the
   canvas's horizontal direction, not the rotated/scaled local X direction. */
static int glyph_canvas_delta(uintptr_t descriptor,float margin,float *dx,float *dy) {
    float matrix[16];
    struct iovec local={matrix,sizeof(matrix)},remote={(void*)descriptor,sizeof(matrix)};
    if(!descriptor || process_vm_readv(getpid(),&local,1,&remote,1,0)!=(ssize_t)sizeof(matrix))return 0;
    for(unsigned i=0;i<16;++i)if(!isfinite(matrix[i]))return 0;
    if(fabsf(matrix[2])>0.000001f || fabsf(matrix[3])>0.000001f ||
       fabsf(matrix[6])>0.000001f || fabsf(matrix[7])>0.000001f || fabsf(matrix[15]-1)>0.000001f)return 0;
    float determinant=matrix[0]*matrix[5]-matrix[4]*matrix[1];
    if(!isfinite(determinant) || fabsf(determinant)<0.000001f)return 0;
    *dx=margin*matrix[5]/determinant;*dy=-margin*matrix[1]/determinant;
    return isfinite(*dx) && isfinite(*dy) && fabsf(*dx)<=100000 && fabsf(*dy)<=100000;
}
/* Experimental sweep of pixel-space screen copies. Normalized post-process
   geometry and authored GUI draws use their existing paths. A native frame
   copy uses all four stock screen corners, full UVs and an untranslated XY
   identity world transform. Preserve all payload fields and widen only XY. */
static int native_screen_rect(const unsigned char *input,uint32_t count,
                              uint32_t primitive,uintptr_t descriptor,int color) {
    if(!input || !descriptor || !((count==4 && primitive==4) || (count==6 && primitive==3)))return 0;
    unsigned corners=0;
    float depth=read_float(input+8);
    if(!isfinite(depth) || fabsf(depth)>10000)return 0;
    for(uint32_t i=0;i<count;++i){
        const unsigned char *v=input+i*(color?16:24);
        float x=read_float(v),y=read_float(v+4);
        if((x!=0 && x!=1920) || (y!=0 && y!=1080) || read_float(v+8)!=depth)return 0;
        if(!color){
            float u=read_float(v+16),t=read_float(v+20);
            if(!isfinite(u) || !isfinite(t) || fabsf(u-x/1920.f)>0.00001f || fabsf(t-y/1080.f)>0.00001f)return 0;
        }
        corners|=1u<<((x!=0?1:0)+(y!=0?2:0));
    }
    if(corners!=15)return 0;
    float matrix[16];
    struct iovec local={matrix,sizeof(matrix)},remote={(void*)descriptor,sizeof(matrix)};
    if(process_vm_readv(getpid(),&local,1,&remote,1,0)!=(ssize_t)sizeof(matrix))return 0;
    for(unsigned i=0;i<16;++i){
        if(!isfinite(matrix[i]))return 0;
        if(i==14){if(fabsf(matrix[i])>10000)return 0;continue;}
        float expected=(i==0 || i==5 || i==10 || i==15)?1.f:0.f;
        if(fabsf(matrix[i]-expected)>0.000001f)return 0;
    }
    return 1;
}
#ifdef VALKYRIA_WIDE_UI_TEST
static int test_offscreen;
#endif
/* Render-to-texture UI layers (the mission-table map: FUN_006b1920 -> FUN_00221b00) are
   drawn while the current 2D environment DAT_00a25b50 carries flag 0x10 with its target at
   +0x300 (set by the layer's DummyTop node, FUN_002220b0). Those vertices address the
   layer's own texture, not the wide screen, so the screen canvas offset must not apply. */
static int offscreen_layer(void) {
#ifdef VALKYRIA_WIDE_UI_TEST
    return test_offscreen;
#else
    uintptr_t env=*(const uintptr_t*)(guest_base+0xa25b50);
    return env && (*(const uint32_t*)(env+0xc)&0x10) && *(const uintptr_t*)(env+0x300);
#endif
}
/* World-map mission cursor (FUN_006aea50, class vtable 0x8e1808). Its position at
   cursor+0x2e0 lives in the game's full-width projection space: FUN_006acb40 moves it,
   clamps it to 10..90% of the wide viewport and hit-tests it against pins projected by
   FUN_00235ce0, and FUN_006aeeb0/FUN_006af1f0 snap it to projected pins. Those stay
   untouched. Only its copy into the 2D "cursor" node at 0x6aed4d is later moved by the
   screen canvas translation, so cancel that translation there (0 outside ultrawide). */
static volatile float cursor_dx;
static const unsigned char cursor_site[]={
    0xc4,0xc1,0x78,0x10,0x86,0xe0,0x02,0x00,0x00, /* vmovups xmm0,[r14+0x2e0] */
    0xc5,0xfa,0x11,0x00,                          /* vmovss [rax],xmm0 */
    0xc4,0xe3,0x79,0x17,0x40,0x04,0x01,           /* vextractps [rax+4],xmm0,1 */
    0xc7,0x40,0x08,0x00,0x00,0x00,0x00,           /* mov dword [rax+8],0 */
    0xe9,0xd5,0xfe,0xff,0xff};                    /* jmp 0x6aec42 */
static int install_cursor(unsigned char *site) {
    if(memcmp(site,cursor_site,sizeof(cursor_site)))return -1;
    int32_t displacement;memcpy(&displacement,site+sizeof(cursor_site)-4,4);
    uintptr_t resume=(uintptr_t)site+sizeof(cursor_site)+(intptr_t)displacement,offset=(uintptr_t)&cursor_dx;
    size_t page=(size_t)sysconf(_SC_PAGESIZE);
    unsigned char *stub=mmap(NULL,page,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    if(stub==MAP_FAILED)return -1;
    unsigned char *p=stub;
    memcpy(p,cursor_site,9);p+=9;
    *p++=0x49;*p++=0xbb;memcpy(p,&offset,8);p+=8;      /* mov r11,&cursor_dx */
    memcpy(p,"\xc4\xc1\x7a\x5c\x03",5);p+=5;           /* vsubss xmm0,xmm0,[r11] */
    memcpy(p,cursor_site+9,18);p+=18;
    *p++=0x49;*p++=0xbb;memcpy(p,&resume,8);p+=8;      /* mov r11,resume */
    memcpy(p,"\x41\xff\xe3",3);                         /* jmp r11 */
    if(mprotect(stub,page,PROT_READ|PROT_EXEC)) {munmap(stub,page);return -1;}
    /* R11 is caller-saved scratch: FUN_006aea50 never reads it and leaves through a
       ret or a tail call, and no branch targets the site's interior. */
    unsigned char replacement[sizeof(cursor_site)];memset(replacement,0xcc,sizeof(replacement));
    uintptr_t entry=(uintptr_t)stub;
    replacement[0]=0x49;replacement[1]=0xbb;memcpy(replacement+2,&entry,8);replacement[10]=0x41;replacement[11]=0xff;replacement[12]=0xe3;
    memcpy(site,replacement,sizeof(replacement));
    return 0;
}
/* Black-and-white narration composite (FUN_0020c1e0 -> 32-byte submit 0x28fe80 at 0x20d245,
   shader 7067e34c/84c2cbb0). Its full-screen quad maps the paper texture (2048x1024, 1920x1024
   authored) at one texel per screen pixel: u1=W/width, v1=H/height from the scene extent
   (xmm1=W, xmm0=H). Stock 1920x1080 ends inside the paper art; a wide screen runs past it, so
   the paper border appears at x=1920 px and the clamped bottom row smears below it. Scale the
   extent to the stock 1920x1080 first, stretching the paper like the GUI paper layer above it.
   The same holds at every other native size (16:9 1440/2160 also run past it, 720/576 crop it);
   paper_scale is 1,1 at stock 1920x1080. 0x20cfc4 (the v1 division) is a branch target and
   stays in place; nothing branches inside the replaced range. */
static volatile float paper_scale[2]={1,1};
static const unsigned char paper_site[]={
    0x0f,0xb7,0x48,0x78,                          /* movzx ecx,word [rax+0x78] */
    0x0f,0xb7,0x40,0x7a,                          /* movzx eax,word [rax+0x7a] */
    0xc5,0xfa,0x2a,0xd1,                          /* vcvtsi2ss xmm2,xmm0,ecx */
    0xc5,0xf2,0x5e,0xe2,                          /* vdivss xmm4,xmm1,xmm2 */
    0xc5,0xfa,0x2a,0xd0,                          /* vcvtsi2ss xmm2,xmm0,eax */
    0xc5,0xfa,0x5e,0xd2};                         /* vdivss xmm2,xmm0,xmm2 (0x20cfc4, kept) */
#define PAPER_REPLACED 20u
static int install_paper(unsigned char *site) {
    if(memcmp(site,paper_site,sizeof(paper_site)))return -1;
    uintptr_t resume=(uintptr_t)site+PAPER_REPLACED,scale=(uintptr_t)paper_scale;
    size_t page=(size_t)sysconf(_SC_PAGESIZE);
    unsigned char *stub=mmap(NULL,page,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    if(stub==MAP_FAILED)return -1;
    unsigned char *p=stub;
    memcpy(p,paper_site,8);p+=8;                       /* both texture extents */
    *p++=0x49;*p++=0xbb;memcpy(p,&scale,8);p+=8;       /* mov r11,paper_scale */
    memcpy(p,"\xc4\xc1\x72\x59\x0b",5);p+=5;           /* vmulss xmm1,xmm1,[r11] */
    memcpy(p,"\xc4\xc1\x7a\x59\x43\x04",6);p+=6;       /* vmulss xmm0,xmm0,[r11+4] */
    memcpy(p,paper_site+8,12);p+=12;                   /* u1 division, v1 conversion */
    *p++=0x49;*p++=0xbb;memcpy(p,&resume,8);p+=8;      /* mov r11,0x20cfc4 */
    memcpy(p,"\x41\xff\xe3",3);                         /* jmp r11 */
    if(mprotect(stub,page,PROT_READ|PROT_EXEC)) {munmap(stub,page);return -1;}
    /* R11 is unused throughout FUN_0020c1e0 (0x20c1e0..0x20d2be). XMM0/XMM1 are reloaded
       before their next reads (0x20d006, 0x20d07a/0x20d0d9/0x20d0f2). */
    unsigned char replacement[PAPER_REPLACED];memset(replacement,0xcc,sizeof(replacement));
    uintptr_t entry=(uintptr_t)stub;
    replacement[0]=0x49;replacement[1]=0xbb;memcpy(replacement+2,&entry,8);replacement[10]=0x41;replacement[11]=0xff;replacement[12]=0xe3;
    memcpy(site,replacement,sizeof(replacement));
    return 0;
}
/* Render-to-texture UI layers built from their authored size (FUN_002d3010 -> FUN_00221b00 at
   0x2d30dc; the history book's 960x1080 pages) are drawn under the screen's GUI root, which
   scales by height / 1080, while their targets stay at the authored size. The game's own
   viewport-fitted layer (the mission-table map, FUN_006b1920) sizes its target to match;
   these never did, as the stock game only ran at GUI scale >= 1 with the screen's viewport,
   scissor and depth at least 1920x1080. Below 1080 lines the content filled only the
   top-left of each page and the screen-sized depth cut the rest. Allocate the target at the
   GUI-scaled size, which the GUI-scaled content then fills exactly (identical at scale 1).
   FUN_00221b00 uses the two floats only to size the render target. */
static const unsigned char layer_site[]={
    0xc5,0xfa,0x10,0x44,0x10,0x04,                /* vmovss xmm0,[rax+rdx+4] */
    0xc5,0xfa,0x10,0x4c,0x10,0x08,                /* vmovss xmm1,[rax+rdx+8] */
    0xe8,0x1f,0xea,0xf4,0xff};                    /* call 0x221b00 */
static int install_layer_scale(unsigned char *site,uintptr_t base) {
    if(memcmp(site,layer_site,sizeof(layer_site)))return -1;
    uintptr_t scale=base+0x968c80,create=base+0x221b00;
    size_t page=(size_t)sysconf(_SC_PAGESIZE);
    unsigned char *stub=mmap(NULL,page,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    if(stub==MAP_FAILED)return -1;
    unsigned char *p=stub;
    memcpy(p,layer_site,12);p+=12;                     /* authored width and height */
    *p++=0x49;*p++=0xbb;memcpy(p,&scale,8);p+=8;       /* mov r11,&gui_scale (0x968c80) */
    memcpy(p,"\xc4\xc1\x7a\x59\x03",5);p+=5;           /* vmulss xmm0,xmm0,[r11] */
    memcpy(p,"\xc4\xc1\x72\x59\x4b\x04",6);p+=6;       /* vmulss xmm1,xmm1,[r11+4] */
    *p++=0x49;*p++=0xbb;memcpy(p,&create,8);p+=8;      /* mov r11,FUN_00221b00 */
    memcpy(p,"\x41\xff\xe3",3);                         /* jmp r11: it returns to the site */
    if(mprotect(stub,page,PROT_READ|PROT_EXEC)) {munmap(stub,page);return -1;}
    /* R11 is unused throughout FUN_002d3010 and caller-saved across the call it replaces;
       nothing branches inside the site. call r11 pushes the same kind of return address. */
    unsigned char replacement[sizeof(layer_site)];
    uintptr_t entry=(uintptr_t)stub;
    replacement[0]=0x49;replacement[1]=0xbb;memcpy(replacement+2,&entry,8); /* mov r11,stub */
    replacement[10]=0x41;replacement[11]=0xff;replacement[12]=0xd3;         /* call r11 */
    memcpy(replacement+13,"\x0f\x1f\x40\x00",4);                            /* nop */
    memcpy(site,replacement,sizeof(replacement));
    return 0;
}
/* Event world map (observed in the Jutland briefing: one 4096x2048 authored quad through
   0x229a9e at -2112..1984 x -958..1090, full UVs, opaque white vertex colour, z -10). Its
   art ends just past the 16:9 edge on the side it is scrolled toward, so the live 3D room
   behind it shows in the wide margins. Return the authored window, centred on the 16:9
   screen, that the map covers on both sides (at least 16:9). A map covering the whole
   wide canvas needs no bars. */
static int event_map_window(const unsigned char *input,uint32_t count,uint32_t primitive,
                            float min_x,float max_x,float min_y,float max_y,float canvas_dx,
                            float *left,float *right) {
    if(count!=4 || primitive!=4 || fabsf(max_x-min_x-4096)>0.5f || fabsf(max_y-min_y-2048)>0.5f)return 0;
    unsigned corners=0;
    for(uint32_t i=0;i<4;++i){
        const unsigned char *v=input+i*24;
        float x=read_float(v),y=read_float(v+4);
        int hx=x==max_x,hy=y==max_y;
        if((!hx && x!=min_x) || (!hy && y!=min_y) || read_float(v+16)!=(float)hx || read_float(v+20)!=(float)hy)return 0;
        corners|=1u<<(hx+2*hy);
    }
    if(corners!=15 || min_y>0 || max_y<1080)return 0;
    float half=fminf(960-min_x,max_x-960);
    if(!(half>=960) || half>=960+canvas_dx)return 0;
    *left=960-half;*right=960+half;
    return 1;
}
/* Screen-edge fades: solid-colour strips on the left or right screen edge over the full height,
   at most half the width, more opaque at the screen edge than inside (observed: the red frame
   while the player is hit, 0..400 x 0..1080 at alpha 0x7f fading to 0 inward, with full-width
   top and bottom strips handled as backgrounds). Return -1 for the left edge and 1 for the
   right, whose strips stay on the canvas edges instead of the centred 16:9 area; plain panels
   (no inward fade) return 0. The alpha is the high byte, as in the engine's 0xff000000|rgb. */
static int screen_edge_fade(const unsigned char *input,uint32_t count,float min_x,float max_x,float min_y,float max_y) {
    if(count!=4 || max_x-min_x>960 || min_y>2 || min_y<-32 || max_y<1078 || max_y>1112)return 0;
    int side=fabsf(min_x)<=2 ? -1 : fabsf(max_x-1920)<=2 ? 1 : 0;
    if(!side)return 0;
    unsigned outer_min=255,inner_max=0;
    for(uint32_t i=0;i<4;++i){
        const unsigned char *v=input+i*16;
        float x=read_float(v),y=read_float(v+4);
        if((fabsf(x-min_x)>0.1f && fabsf(x-max_x)>0.1f) || (fabsf(y-min_y)>0.1f && fabsf(y-max_y)>0.1f))return 0;
        unsigned alpha=v[15];
        int outer=side<0 ? fabsf(x-min_x)<=0.1f : fabsf(x-max_x)<=0.1f;
        if(outer){if(alpha<outer_min)outer_min=alpha;}
        else if(alpha>inner_max)inner_max=alpha;
    }
    return outer_min>inner_max ? side : 0;
}
/* A talk-portrait panel (revIfCharaTalkWindow): four corners sampling u 0.1875 and 0.8125
   (either way round) by v 0 and 1, in the authored 600:1080 proportion (observed at
   90..690 and 1230..1830 x 0..1080). */
static int talk_portrait(const unsigned char *input,uint32_t count,float span_x,float span_y) {
    if(count!=4 || !(span_x>0) || !(span_y>0) || fabsf(span_x*1080.f-span_y*600.f)>span_y*6.f)return 0;
    unsigned corners=0;
    for(uint32_t i=0;i<4;++i){
        float u=read_float(input+i*24+16),v=read_float(input+i*24+20);
        int left=fabsf(u-.1875f)<.0001f,right=fabsf(u-.8125f)<.0001f,top=fabsf(v)<.0001f,bottom=fabsf(v-1)<.0001f;
        if(!(left||right) || !(top||bottom))return 0;
        corners|=1u<<(right*2+bottom);
    }
    return corners==15;
}
/* Whole-screen pictures: an event can show a still over the whole screen, as in a flashback
   (observed: one quad through 0x229a9e at 0..1920 x 0..1088 with the whole texture, opaque
   white, normal blending, a 1920x1080 BC1 texture; then a pulsing 256x256 vignette at
   -49.5..1969.5 and an additive 1024x1024 glow at -5..1925 over it). It is drawn for the 16:9
   screen and hides all of it, so widening it only distorts it. A quad's texture is reached from
   its draw group, the submit's first argument (FUN_00749050): group+0x50 is the texture handle,
   handle+8 the texture, and the GNM descriptor words at texture+0x10 give the format (word 1,
   bits 20..25; 35 is BC1) and the size less one (word 2, 14 bits each). A whole opaque (BC1)
   texture in the screen's 16:9 shape, drawn with normal blending (group+0x20 is 3) over the
   whole 16:9 screen, is such a picture. No power-of-two texture has that shape, which leaves
   tints, vignettes, the narration paper and the tall illustrated backdrops (2048x2048) to the
   full-width rules. */
static int screen_picture(uintptr_t group,const unsigned char *input,uint32_t count,
                          float min_x,float max_x,float min_y,float max_y) {
    if(count!=4 || min_x>2 || max_x<1918 || min_y>2 || max_y<1078)return 0;
    unsigned corners=0,whole=0;
    for(uint32_t i=0;i<4;++i){
        const unsigned char *v=input+i*24;
        float x=read_float(v),y=read_float(v+4),u=read_float(v+16),t=read_float(v+20);
        int hx=fabsf(x-max_x)<=0.1f,hy=fabsf(y-max_y)<=0.1f;
        if((!hx && fabsf(x-min_x)>0.1f) || (!hy && fabsf(y-min_y)>0.1f) || (u!=0 && u!=1) || (t!=0 && t!=1))return 0;
        corners|=1u<<(hx+2*hy);whole|=1u<<((u!=0)+2*(t!=0));
    }
    uint32_t blend,words[2];uintptr_t handle,texture;
    if(corners!=15 || whole!=15 || !guest_read(group+0x20,&blend,4) || blend!=3 ||
       !guest_read(group+0x50,&handle,8) || !guest_read(handle+8,&texture,8) ||
       !guest_read(texture+0x14,words,8))return 0;
    const uint32_t width=(words[1]&0x3fff)+1,height=(words[1]>>14&0x3fff)+1;
    return (words[0]>>20&0x3f)==35 && width*9==height*16;
}
/* Which update a draw belongs to: the frame-end time the clock stores once an update
   (FUN_003d47f0, timing state [0xa12be8] + 0x20). 0 until the clock runs. */
#ifdef VALKYRIA_WIDE_UI_TEST
static uint64_t test_update=1;
#endif
static uint64_t update_stamp(void) {
#ifdef VALKYRIA_WIDE_UI_TEST
    return test_update;
#else
    const uintptr_t timing=*(const uintptr_t*)(guest_base+0xa12be8);
    return timing ? *(const uint64_t*)(timing+0x20) : 0;
#endif
}
static uint64_t picture_update;
/* Cuts a screen-covering quad to the authored 16:9 screen across, as that screen's edge does: a
   corner past the edge moves onto it and takes the colour and UVs found there on the way to its
   neighbour along x. */
static void crop_to_screen(unsigned char *out,const unsigned char *input) {
    for(unsigned i=0;i<4;++i){
        const unsigned char *v=input+i*24;
        const float x=read_float(v),inside=fminf(fmaxf(x,0),1920);
        if(inside==x)continue;
        for(unsigned j=0;j<4;++j){
            const unsigned char *n=input+j*24;
            const float nx=read_float(n);
            if(fabsf(read_float(n+4)-read_float(v+4))>0.1f || fabsf(nx-x)<=0.1f)continue;
            const float t=(inside-x)/(nx-x);
            write_float(out+i*24,inside);
            for(unsigned c=12;c<16;++c)out[i*24+c]=(unsigned char)lrintf((float)v[c]+((float)n[c]-(float)v[c])*t);
            for(unsigned c=16;c<24;c+=4)write_float(out+i*24+c,read_float(v+c)+(read_float(n+c)-read_float(v+c))*t);
            break;
        }
    }
}
static void canvas_forward(uintptr_t context,const void *vertices,uint32_t count,uint32_t primitive,uintptr_t descriptor,uintptr_t caller,int color) {
    Submit forward=color?original_color_submit:original_submit;
    size_t stride=color?16:24;
#ifdef VALKYRIA_WIDE_UI_TEST
    caller=test_caller;
#endif
    int gui_snapshot=!color && caller==0x20cf30;
    int authored=color?caller==0x2297d7:caller==0x229a9e || caller==0x4a4c59 || caller==0x33c2fe || gui_snapshot;
    int screen_candidate=!authored && caller<0xaf0000 &&
                         ((count==4 && primitive==4) || (count==6 && primitive==3));
    if ((!authored && !screen_candidate) || !vertices || !count || count>8192) {
        forward(context,vertices,count,primitive,descriptor);return;
    }
    struct UiLayout ui;
    int layout=ui_layout(&ui);
    cursor_dx=layout?ui.margin/ui.sx:0.f;
    paper_scale[0]=layout?1920.f/ui.width:1.f;paper_scale[1]=layout?1080.f/ui.height:1.f;
    if(!layout || offscreen_layer()){forward(context,vertices,count,primitive,descriptor);return;}
    const unsigned char *input=vertices;
    if(gui_snapshot && !native_screen_rect(input,count,primitive,descriptor,0)){
        forward(context,vertices,count,primitive,descriptor);return;
    }
    int screen_rect=screen_candidate && native_screen_rect(input,count,primitive,descriptor,color);
    if(!authored && !screen_rect){forward(context,vertices,count,primitive,descriptor);return;}
    float min_x=INFINITY,max_x=-INFINITY,min_y=INFINITY,max_y=-INFINITY;
    for (uint32_t i=0;i<count;++i) {
        float x=read_float(input+i*stride),y=read_float(input+i*stride+4);
        if (!isfinite(x) || !isfinite(y) || fabsf(x)>100000 || fabsf(y)>100000) {
            forward(context,vertices,count,primitive,descriptor);return;
        }
        if(x<min_x)min_x=x;if(x>max_x)max_x=x;
        if(y<min_y)min_y=y;if(y>max_y)max_y=y;
    }
    float canvas_dx=ui.margin/ui.sx,canvas_dy=0;
    if(!color && caller==0x4a4c59 && !glyph_canvas_delta(descriptor,canvas_dx,&canvas_dx,&canvas_dy)){
        forward(context,vertices,count,primitive,descriptor);return;
    }
    size_t bytes=((size_t)count*stride+15)&~(size_t)15;
    if(bytes>scratch_capacity) {
        unsigned char *larger=realloc(scratch,bytes);
        if(!larger) { forward(context,vertices,count,primitive,descriptor);return; }
        scratch=larger;scratch_capacity=bytes;
    }
    /* Match the original helper's padded copy; persistent input vertices remain untouched.
       Vertex coordinates remain authored UI units. GUI root scale is applied
       later by the shader, including to our translation: convert pixels first. */
    memcpy(scratch,vertices,bytes);
    int crossfade=screen_rect || gui_snapshot || (!color && caller==0x33c2fe);
    if(crossfade) {
        int full=(count==4 || screen_rect) && min_x==0 && max_x==1920 && min_y==0 && max_y==1080;
        for(uint32_t i=0;i<count && full;++i){
            float x=read_float(input+i*stride),y=read_float(input+i*stride+4);
            if((x!=0 && x!=1920) || (y!=0 && y!=1080))full=0;
        }
        if(!full){forward(context,vertices,count,primitive,descriptor);return;}
        /* Native20c1e0's fallback copy is multiplied by the GUI root AFTER
           submission, despite passing an identity descriptor here. Captured
           shader matrices confirm S=H/1080. Convert physical target extent to
           authored units, unlike the existing independent33be00 copy. */
        float target_width=gui_snapshot?ui.width/ui.sx:ui.width;
        float target_height=gui_snapshot?ui.height/ui.sy:ui.height;
        /* A flat colour only has to cover the canvas, and identical quads come both in
           pixels and GUI-scaled (FUN_0020b8d0's fade at 0x20bb20): take the larger extent,
           which covers in either unit. Only below 1080 lines (GUI scale < 1) does it differ
           from the pixel extent, where shrinking a GUI-scaled fade would expose the scene. */
        if(color){target_width=fmaxf(ui.width,ui.width/ui.sx);target_height=fmaxf(ui.height,ui.height/ui.sy);}
        for(uint32_t i=0;i<count;++i){
            write_float(scratch+i*stride,read_float(input+i*stride)*target_width/1920.f);
            write_float(scratch+i*stride+4,read_float(input+i*stride+4)*target_height/1080.f);
        }
        forward(context,scratch,count,primitive,descriptor);return;
    }
    /* 16:9: no margin, so every rule below is the identity (no translation, stretch 1). */
    if(ui.margin==0){forward(context,vertices,count,primitive,descriptor);return;}
    int background=count==4 && min_x>=-2 && min_x<=2 && max_x>=1918 && max_x<=1924;
    int edge=color && count==4 && min_x<0 && max_x>0 && max_x<960 && min_y<0 && max_y>1080;
    int side=0;
    if(color) {
        unsigned negative=0;
        for(uint32_t i=0;i<count;++i) {
            float x=read_float(input+i*stride),y=read_float(input+i*stride+4);
            if(fabsf(x-min_x)>0.1f && fabsf(x-max_x)>0.1f)background=0;
            if(fabsf(y-min_y)>0.1f && fabsf(y-max_y)>0.1f)background=0;
            if(x<0)++negative;
            if(y>=0 && y<=1080)edge=0;
        }
        edge=edge && negative==2;
        side=screen_edge_fade(input,count,min_x,max_x,min_y,max_y);
    } else {
        /* Illustrated cutscene backdrops can pan vertically through a tall
           texture (observed 1920x2048), while still covering the full viewport.
           Widen their X coordinates, retaining the authored Y pan and UVs.
           Full-screen dim layers may carry a small authored overscan (observed
           -13..1933 x -19..1107 behind the mission briefing table); widen those too. */
        int overscan=count==4 && min_x>=-32 && min_x<=2 && max_x>=1918 && max_x<=1952;
        /* Screen-centred effect layers pulse in size every frame (observed in an illustrated
           map cutscene: a full-UV vignette at -61..-8 and a tinted glow at -114..+65
           authored X, both exactly centred, Y up to 1.27x). Their size crosses the bounds
           above, so a fixed bound made them alternate between 16:9 and the wide canvas.
           Widen any single centred quad within 0.9..1.5x of the 16:9 screen on both axes
           so each frame of the pulse is handled alike. */
        float span_x=max_x-min_x,span_y=max_y-min_y;
        int centred=count==4 && fabsf(min_x+max_x-1920)<=0.5f && fabsf(min_y+max_y-1080)<=16 &&
                    span_x>=1920*0.9f && span_x<=1920*1.5f && span_y>=1080*0.9f && span_y<=1080*1.5f;
        /* The black-and-white narration paper is the 1920x1024 authored area of a 2048x1024
           texture drawn from the top (0..1920 x 0..1024, UV 0..0.9375 x 0..1); stock leaves the
           last rows to the paper composite beneath it. A full-width layer from the top covering
           at least 1024 rows is therefore full screen as well, so it stretches with that
           composite instead of staying a 16:9 box over it. Its border fills the rows below it
           (logged: two 960x56 quads, x 0..960 and 960..1920 by y 1024..1080, from a rotated
           slice of the same texture's spare columns): a layer exactly from that edge to the
           screen's bottom edge belongs to the full-screen paper wherever it sits across. */
        int paper_border=count==4 && fabsf(min_y-1024)<=0.5f && fabsf(max_y-1080)<=0.5f &&
                         min_x>=-2 && max_x<=1922;
        background=caller==0x229a9e &&
                   (((background || overscan) && min_y<=2 && max_y>=1024 && max_y-min_y<=4096) ||
                    centred || paper_border);
        for(uint32_t i=0;i<count && background;++i){
            float x=read_float(input+i*stride),y=read_float(input+i*stride+4);
            if((fabsf(x-min_x)>0.1f && fabsf(x-max_x)>0.1f) ||
               (fabsf(y-min_y)>0.1f && fabsf(y-max_y)>0.1f))background=0;
        }
    }
    /* A whole-screen picture keeps the 16:9 screen it was drawn for, and so do the full-screen
       layers drawn over it in the same update (its vignette and glow): cut to that screen and
       placed as authored UI instead of widened. */
    int picture=0;
    if(!color && caller==0x229a9e){
        const uint64_t update=update_stamp();
        picture=primitive==4 && screen_picture(context,input,count,min_x,max_x,min_y,max_y);
        if(picture)picture_update=update;
        if(picture || (background && update && update==picture_update)){crop_to_screen(scratch,input);background=0;}
    }
    for(uint32_t i=0;i<count;++i) {
        float x=read_float(scratch+i*stride);
        write_float(scratch+i*stride,background ? x*ui.stretch : (edge && x<0) || side<0 ? x : side>0 ? x+2*canvas_dx : x+canvas_dx);
        if(canvas_dy!=0)write_float(scratch+i*stride+4,read_float(scratch+i*stride+4)+canvas_dy);
    }
    /* Each talk portrait is a 3D render into a target half the screen wide (FUN_001ce860,
       960x1080 at 1080p), shown through its panel at u 0.1875..0.8125: texels 180..780, 1:1
       in the 600-unit panel. The target follows the real width while the panel keeps its
       16:9 size, so a wider screen squeezed 600 * stretch texels into it. Sample the centred
       1/stretch of that span: the stock texel scale and framing, within the portrait's
       backdrop. */
    if(!color && caller==0x229a9e && talk_portrait(input,count,max_x-min_x,max_y-min_y))
        for(uint32_t i=0;i<count;++i)
            write_float(scratch+i*stride+16,.5f+(read_float(input+i*stride+16)-.5f)/ui.stretch);
    float bar_left,bar_right;
    int bars=!color && caller==0x229a9e && event_map_window(input,count,primitive,min_x,max_x,min_y,max_y,canvas_dx,&bar_left,&bar_right);
    if(picture){bars=1;bar_left=0;bar_right=1920;}
    forward(context,scratch,count,primitive,descriptor);
    if(bars){
        /* Right after the map or picture, so later UI (vignette, text, dialogs) still draws
           on top: the same quad, state and depth, with one central texel and RGB zeroed. The
           alpha byte (high byte, as in the engine's 0xff000000|rgb colours) is kept so the
           bars fade with it. Spans reach a little past the canvas edges. */
        const float spans[2][2]={{-canvas_dx-8,bar_left},{bar_right,1920+canvas_dx+8}};
        for(unsigned b=0;b<2;++b){
            memcpy(scratch,vertices,bytes);
            for(uint32_t i=0;i<4;++i){
                unsigned char *v=scratch+i*24;
                write_float(v,(read_float(input+i*24)==max_x?spans[b][1]:spans[b][0])+canvas_dx);
                v[12]=v[13]=v[14]=0;
                write_float(v+16,0.5f);write_float(v+20,0.5f);
            }
            forward(context,scratch,count,primitive,descriptor);
        }
    }
}
static __attribute__((noinline)) void canvas_submit(uintptr_t context,const void *vertices,uint32_t count,uint32_t primitive,uintptr_t descriptor) {
    canvas_forward(context,vertices,count,primitive,descriptor,(uintptr_t)__builtin_return_address(0)-guest_base,0);
}
static __attribute__((noinline)) void color_submit(uintptr_t context,const void *vertices,uint32_t count,uint32_t primitive,uintptr_t descriptor) {
    canvas_forward(context,vertices,count,primitive,descriptor,(uintptr_t)__builtin_return_address(0)-guest_base,1);
}
static void jump(unsigned char *where,uintptr_t destination) {
    where[0]=0x48;where[1]=0xb8;memcpy(where+2,&destination,8);where[10]=0xff;where[11]=0xe0;
}
static int install(unsigned char *target,uintptr_t base) {
    if(memcmp(target,prologue,sizeof(prologue)))return -1;
    size_t page=(size_t)sysconf(_SC_PAGESIZE);
    unsigned char *trampoline=mmap(NULL,page,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    if(trampoline==MAP_FAILED)return -1;
    memcpy(trampoline,prologue,sizeof(prologue));
    jump(trampoline+sizeof(prologue),(uintptr_t)target+sizeof(prologue));
    if(mprotect(trampoline,page,PROT_READ|PROT_EXEC)) {munmap(trampoline,page);return -1;}
    guest_base=base;original_submit=(Submit)(void*)trampoline;
    unsigned char replacement[sizeof(prologue)];memset(replacement,0x90,sizeof(replacement));
    jump(replacement,(uintptr_t)canvas_submit);memcpy(target,replacement,sizeof(replacement));
    return 0;
}
static int install_color(unsigned char *target) {
    static const unsigned char prefix[]={0x41,0x57,0x41,0x56,0x53,0x4c,0x8b,0x0d};
    static const unsigned char suffix[]={0x41,0x89,0xd2};
    if(memcmp(target,prefix,8) || memcmp(target+12,suffix,3))return -1;
    int32_t displacement;memcpy(&displacement,target+8,4);
    uintptr_t global=(uintptr_t)target+12+(intptr_t)displacement;
    size_t page=(size_t)sysconf(_SC_PAGESIZE);
    unsigned char *trampoline=mmap(NULL,page,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    if(trampoline==MAP_FAILED)return -1;
    memcpy(trampoline,target,5);
    /* Relocate the RIP-relative load without a signed 32-bit distance limit. */
    trampoline[5]=0x49;trampoline[6]=0xb9;memcpy(trampoline+7,&global,8);
    memcpy(trampoline+15,"\x4d\x8b\x09",3);
    memcpy(trampoline+18,suffix,3);
    jump(trampoline+21,(uintptr_t)target+15);
    if(mprotect(trampoline,page,PROT_READ|PROT_EXEC)) {munmap(trampoline,page);return -1;}
    original_color_submit=(Submit)(void*)trampoline;
    unsigned char replacement[15];memset(replacement,0x90,sizeof(replacement));
    jump(replacement,(uintptr_t)color_submit);memcpy(target,replacement,sizeof(replacement));return 0;
}
static int install_project(unsigned char *target) {
    if(memcmp(target,project_prologue,sizeof(project_prologue)))return -1;
    size_t page=(size_t)sysconf(_SC_PAGESIZE);
    unsigned char *trampoline=mmap(NULL,page,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    if(trampoline==MAP_FAILED)return -1;
    memcpy(trampoline,target,sizeof(project_prologue));
    jump(trampoline+sizeof(project_prologue),(uintptr_t)target+sizeof(project_prologue));
    if(mprotect(trampoline,page,PROT_READ|PROT_EXEC)) {munmap(trampoline,page);return -1;}
    original_project=(Project)(void*)trampoline;
    unsigned char replacement[sizeof(project_prologue)];memset(replacement,0x90,sizeof(replacement));
    jump(replacement,(uintptr_t)project_submit);memcpy(target,replacement,sizeof(replacement));
    return 0;
}

/* Reads guest bytes without faulting: a different game build or layout fails closed. */
static int guest_matches(uintptr_t at,const unsigned char *expected,size_t size) {
    unsigned char observed[64];
    if(size>sizeof(observed))return 0;
    struct iovec local={observed,size},remote={(void*)at,size};
    return process_vm_readv(getpid(),&local,1,&remote,1,0)==(ssize_t)size && !memcmp(observed,expected,size);
}
static const unsigned char color_signature[]={0x41,0x57,0x41,0x56,0x53,0x4c,0x8b,0x0d,0xfc,0x93,0x2c,0,0x41,0x89,0xd2};
int valkyria_wide_ui_check(uintptr_t base) {
    return guest_matches(base+0x764860,prologue,sizeof(prologue)) &&
           guest_matches(base+0x765750,color_signature,sizeof(color_signature)) &&
           guest_matches(base+0x235ce0,project_prologue,sizeof(project_prologue)) ? 0 : -1;
}
int valkyria_wide_ui_install(uintptr_t base,unsigned width,unsigned height) {
    if(!native_canvas((float)width,(float)height) || valkyria_wide_ui_check(base))return -1;
    /* All three hooks or none: a failed trampoline allocation restores the hooked bytes. */
    unsigned char *const targets[3]={(unsigned char*)(base+0x764860),(unsigned char*)(base+0x765750),
                                     (unsigned char*)(base+0x235ce0)};
    const size_t sizes[3]={sizeof(prologue),sizeof(color_signature),sizeof(project_prologue)};
    unsigned char saved[3][sizeof(project_prologue)];
    _Static_assert(sizeof(prologue)<=sizeof(project_prologue) && sizeof(color_signature)<=sizeof(project_prologue),"saved size");
    for(int i=0;i<3;++i)memcpy(saved[i],targets[i],sizes[i]);
    if(install(targets[0],base) || install_color(targets[1]) || install_project(targets[2])) {
        for(int i=0;i<3;++i)memcpy(targets[i],saved[i],sizes[i]);
        original_submit=original_color_submit=NULL;original_project=NULL;guest_base=0;
        return -1;
    }
    if(wide_canvas((float)width,(float)height))
        printf("Valkyria wide canvas: %ux%u UI layout (centred 16:9 UI, full-width layers)\n",width,height);
    else
        printf("Valkyria internal resolution: %ux%u UI layout (full-screen copies at the native size)\n",width,height);
    if(!guest_matches(base+0x6aed4d,cursor_site,sizeof(cursor_site)) || install_cursor((unsigned char*)(base+0x6aed4d)))
        fputs("Valkyria wide canvas: world-map cursor signature unavailable; cursor left native\n",stderr);
    paper_scale[0]=1920.f/(float)width;paper_scale[1]=1080.f/(float)height; /* until the first UI submit */
    if(!guest_matches(base+0x20cfb0,paper_site,sizeof(paper_site)) || install_paper((unsigned char*)(base+0x20cfb0)))
        fputs("Valkyria wide canvas: narration paper signature unavailable; paper left at native texel scale\n",stderr);
    if(!guest_matches(base+0x2d30d0,layer_site,sizeof(layer_site)) || install_layer_scale((unsigned char*)(base+0x2d30d0),base))
        fputs("Valkyria wide canvas: UI layer signature unavailable; authored-size layers left native\n",stderr);
    return 0;
}
