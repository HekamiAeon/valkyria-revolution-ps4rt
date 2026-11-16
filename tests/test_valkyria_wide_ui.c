// SPDX-License-Identifier: GPL-2.0-or-later
// Native canvas UI layout (gpu/shim/valkyria_wide_ui.c) on synthetic guest code and vertices: every
// rule on all five wide canvases (2560x1080, 3440x1440, 3840x1440, 3840x1080, 5120x1440), the
// other 16:9 native sizes and the exclusions. Needs same-process memory reads (process_vm_readv).
#define VALKYRIA_WIDE_UI_TEST
#include "gpu/shim/valkyria_wide_ui.c"

static unsigned char captured[8192*24+16];
static uintptr_t captured_context,captured_descriptor;
static uint32_t captured_count,captured_primitive;
static size_t test_stride=24;
static float invoke_matrix[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
/* Wide canvases of both GUI heights, from just past 16:9 to 32:9; index 3 is the default. */
#define CANVASES 5u
static const float canvas_widths[CANVASES]={2560,3440,3840,3840,5120},canvas_heights[CANVASES]={1080,1440,1440,1080,1440};
static void use_canvas(unsigned k){test_project_width=canvas_widths[k];test_project_height=canvas_heights[k];test_project_scale=canvas_heights[k]/1080.f;}
/* The authored canvas translation and the full-width stretch on canvas k. */
static float canvas_dx(unsigned k){return (canvas_widths[k]/(canvas_heights[k]/1080.f)-1920)/2;}
static float canvas_stretch(unsigned k){return canvas_widths[k]/(1920*canvas_heights[k]/1080.f);}
static int close_to(float a,float b){return fabsf(a-b)<0.01f;}
__attribute__((used)) static uintptr_t color_global=UINT64_C(0x1234abcd9876ef01);
__attribute__((used)) static uintptr_t observed_color_global;
static unsigned submit_log_count;
static unsigned char submit_log[4][96];
__attribute__((used,noinline)) static void record_submit(uintptr_t context,const void *vertices,uint32_t count,uint32_t primitive,uintptr_t descriptor) {
    captured_context=context;captured_descriptor=descriptor;captured_count=count;captured_primitive=primitive;
    memcpy(captured,vertices,((size_t)count*test_stride+15)&~(size_t)15);
    if(submit_log_count<4)memcpy(submit_log[submit_log_count],vertices,count*test_stride<96?count*test_stride:96);
    ++submit_log_count;
}
__attribute__((naked,noinline)) static void fake_guest(uintptr_t context __attribute__((unused)),const void *vertices __attribute__((unused)),uint32_t count __attribute__((unused)),uint32_t primitive __attribute__((unused)),uintptr_t descriptor __attribute__((unused))) {
    __asm__ volatile("push %rbp; mov %rsp,%rbp; push %r15; push %r14; push %r13; push %r12; push %rbx; push %rax; call record_submit; add $8,%rsp; pop %rbx; pop %r12; pop %r13; pop %r14; pop %r15; pop %rbp; ret");
}
static void invoke(unsigned char *source,uint32_t count) {
    fake_guest(0x12345678,source,count,0x87654321,(uintptr_t)invoke_matrix);
    assert(captured_context==0x12345678 && captured_count==count && captured_primitive==0x87654321 && captured_descriptor==(uintptr_t)invoke_matrix);
}
__attribute__((naked,noinline)) static void fake_color_guest(uintptr_t context __attribute__((unused)),const void *vertices __attribute__((unused)),uint32_t count __attribute__((unused)),uint32_t primitive __attribute__((unused)),uintptr_t descriptor __attribute__((unused))) {
    __asm__ volatile("push %r15; push %r14; push %rbx; mov color_global(%rip),%r9; mov %edx,%r10d; mov %r9,observed_color_global(%rip); call record_submit; pop %rbx; pop %r14; pop %r15; ret");
}
static void invoke_color(unsigned char *source,uint32_t count) {
    test_stride=16;
    fake_color_guest(0x12345678,source,count,0x87654321,UINT64_C(0xaabbccddeeff0011));
    assert(captured_context==0x12345678 && captured_count==count && captured_primitive==0x87654321 && captured_descriptor==UINT64_C(0xaabbccddeeff0011));
    assert(observed_color_global==color_global);
}
static unsigned project_calls;
__attribute__((used,noinline)) static void record_project(float *out,const float *world) {
    ++project_calls;
    for(unsigned i=0;i<4;++i)out[i]=world[i]+(float)(i*10);
}
__attribute__((naked,noinline)) static void fake_project(float *out __attribute__((unused)),const float *world __attribute__((unused))) {
    __asm__ volatile("push %rbp; mov %rsp,%rbp; push %r15; push %r14; push %r13; push %r12; push %rbx; sub $24,%rsp; call record_project; add $24,%rsp; pop %rbx; pop %r12; pop %r13; pop %r14; pop %r15; pop %rbp; ret");
}
static void test_projection(void) {
    size_t page=(size_t)sysconf(_SC_PAGESIZE);uintptr_t start=(uintptr_t)fake_project&~(page-1);
    assert(!mprotect((void*)start,2*page,PROT_READ|PROT_WRITE|PROT_EXEC));
    unsigned char expected[sizeof(project_prologue)];memcpy(expected,(void*)fake_project,sizeof(expected));
    ((unsigned char*)(uintptr_t)fake_project)[0]^=1;
    assert(install_project((unsigned char*)(uintptr_t)fake_project)==-1);
    memcpy((void*)fake_project,expected,sizeof(expected));
    assert(!install_project((unsigned char*)(uintptr_t)fake_project));
    assert(!mprotect((void*)start,2*page,PROT_READ|PROT_EXEC));
    const float input[]={1714,292,0,1};float saved[4];memcpy(saved,input,sizeof(input));
    float out[4];test_project_caller=0x62a906;
    for(unsigned i=0;i<8;++i){fake_project(out,input);assert(out[0]==754 && out[1]==302 && out[2]==20 && out[3]==31);assert(!memcmp(saved,input,sizeof(input)));}
    assert(project_calls==8);
    test_project_caller=0x6a5bf4;
    for(unsigned i=0;i<8;++i){fake_project(out,input);assert(out[0]==754 && out[1]==302 && out[2]==20 && out[3]==31);assert(!memcmp(saved,input,sizeof(input)));}
    test_project_caller=0x63965f;
    for(unsigned i=0;i<8;++i){fake_project(out,input);assert(out[0]==754 && out[1]==302 && out[2]==20 && out[3]==31);assert(!memcmp(saved,input,sizeof(input)));}
    test_project_caller=0x686f43;
    for(unsigned i=0;i<8;++i){fake_project(out,input);assert(out[0]==754 && out[1]==302 && out[2]==20 && out[3]==31);assert(!memcmp(saved,input,sizeof(input)));}
    test_project_caller=0x684c42;
    for(unsigned i=0;i<8;++i){fake_project(out,input);assert(out[0]==754 && out[1]==302 && out[2]==20 && out[3]==31);assert(!memcmp(saved,input,sizeof(input)));}
    test_project_caller=0x6995a5;
    for(unsigned i=0;i<8;++i){fake_project(out,input);assert(out[0]==754 && out[1]==302 && out[2]==20 && out[3]==31);assert(!memcmp(saved,input,sizeof(input)));}
    test_project_caller=0x6b8ba7;
    for(unsigned i=0;i<8;++i){fake_project(out,input);assert(out[0]==754 && out[1]==302 && out[2]==20 && out[3]==31);assert(!memcmp(saved,input,sizeof(input)));}
    test_project_caller=0x123456;fake_project(out,input);assert(out[0]==1714);
    /* Bound-testing/clamping callers and the world-map cursor stay in projection space. */
    for(uintptr_t c=0;c<6;++c){test_project_caller=(uintptr_t[]){0x563b61,0x5642fb,0x67af9e,0x6853e6,0x6ad163,0x6aef2e}[c];
        fake_project(out,input);assert(out[0]==1714);}
    test_project_caller=0x62a906;test_project_width=1920;fake_project(out,input);assert(out[0]==1714);
    test_project_width=3840;test_project_scale=2;fake_project(out,input);assert(out[0]==1714);
    test_project_width=5120;test_project_height=1440;test_project_scale=4.f/3.f;
    const uintptr_t callers[]={0x62a906,0x6a5bf4,0x63965f,0x686f43,0x684c42,0x6995a5,0x6b8ba7,
                               0x6b8dc6,0x63a62b,0x4837f0,0x65f777};
    for(unsigned j=0;j<sizeof(callers)/sizeof(callers[0]);++j){test_project_caller=callers[j];
        for(unsigned i=0;i<8;++i){fake_project(out,input);assert(fabsf(out[0]-754)<0.001f && out[1]==302 && out[2]==20 && out[3]==31);assert(!memcmp(saved,input,sizeof(input)));}}
    test_project_scale=1;test_project_mode=1;fake_project(out,input);assert(fabsf(out[0]-754)<0.001f);
    test_project_mode=0;test_project_scale=0;fake_project(out,input);assert(out[0]==1714);
    test_project_scale=NAN;fake_project(out,input);assert(out[0]==1714);
    test_project_scale=1;
    for(unsigned k=0;k<CANVASES;++k){use_canvas(k);
        for(unsigned j=0;j<sizeof(callers)/sizeof(callers[0]);++j){test_project_caller=callers[j];
            for(unsigned i=0;i<4;++i){fake_project(out,input);assert(close_to(out[0],1714-canvas_dx(k)) && out[1]==302 && out[2]==20 && out[3]==31);assert(!memcmp(saved,input,sizeof(input)));}}}
    use_canvas(3);
    puts("PASS: projection sweep removes the authored margin on every wide canvas");
    puts("PASS: native projection trampoline ABI, fresh HP/TARGET/forecast/lock-on/shop/quest/speech/talk anchors only, input/YZW preserved, no accumulated drift, native16:9/other caller exclusion, UI scale/mode, invalid scale fallback, signature rejection");
}
static void test_screen_rect_sweep(void) {
    float matrix[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,-10,1};
    unsigned char input[6*24+16],saved[sizeof(input)];
    const float xy[6][2]={{0,0},{1920,0},{0,1080},{1920,1080},{0,1080},{1920,0}};
    const float *widths=canvas_widths,*heights=canvas_heights;
    const uintptr_t callers[]={0x20af72,0x22bb64,0x1732ce,0x12345};
    for(int color=0;color<=1;++color){
        size_t stride=color?16:24;test_stride=stride;
        for(unsigned mode=0;mode<CANVASES;++mode){
            test_project_width=widths[mode];test_project_height=heights[mode];test_project_scale=heights[mode]/1080.f;
            for(unsigned triangles=0;triangles<2;++triangles){
                unsigned count=triangles?6:4,primitive=triangles?3:4;
                memset(input,0x51,sizeof(input));
                for(unsigned i=0;i<count;++i){
                    write_float(input+i*stride,xy[i][0]);write_float(input+i*stride+4,xy[i][1]);write_float(input+i*stride+8,-10);
                    if(!color){write_float(input+i*stride+16,xy[i][0]/1920);write_float(input+i*stride+20,xy[i][1]/1080);}
                }
                memcpy(saved,input,sizeof(input));
                for(unsigned c=0;c<sizeof(callers)/sizeof(callers[0]);++c){
                    test_caller=callers[c];
                    for(unsigned repeat=0;repeat<3;++repeat){
                        canvas_forward(0,input,count,primitive,(uintptr_t)matrix,0,color);
                        assert(!memcmp(input,saved,sizeof(input)));
                        for(unsigned i=0;i<count;++i){
                            if(read_float(captured+i*stride)!=(xy[i][0]>0?widths[mode]:0))
                                fprintf(stderr,"screen test: color=%d mode=%u triangles=%u caller=%lx vertex=%u x=%g classifier=%d\n",color,mode,triangles,(unsigned long)test_caller,i,read_float(captured+i*stride),native_screen_rect(input,count,primitive,(uintptr_t)matrix,color));
                            assert(read_float(captured+i*stride)==(xy[i][0]>0?widths[mode]:0));
                            assert(read_float(captured+i*stride+4)==(xy[i][1]>0?heights[mode]:0));
                            assert(!memcmp(captured+i*stride+8,input+i*stride+8,stride-8));
                        }
                    }
                }
                matrix[12]=960;canvas_forward(0,input,count,primitive,(uintptr_t)matrix,0,color);assert(!memcmp(captured,input,count*stride));matrix[12]=0;
                matrix[0]=4.f/3.f;canvas_forward(0,input,count,primitive,(uintptr_t)matrix,0,color);assert(!memcmp(captured,input,count*stride));matrix[0]=1;
                matrix[0]=NAN;canvas_forward(0,input,count,primitive,(uintptr_t)matrix,0,color);assert(!memcmp(captured,input,count*stride));matrix[0]=1;
                canvas_forward(0,input,count,primitive,1,0,color);assert(!memcmp(captured,input,count*stride));
                write_float(input,20);canvas_forward(0,input,count,primitive,(uintptr_t)matrix,0,color);assert(!memcmp(captured,input,count*stride));memcpy(input,saved,sizeof(input));
                if(!color){write_float(input+16,.25);canvas_forward(0,input,count,primitive,(uintptr_t)matrix,0,color);assert(!memcmp(captured,input,count*stride));memcpy(input,saved,sizeof(input));}
                for(unsigned i=0;i<count;++i){write_float(input+i*stride,xy[i][0]/960-1);write_float(input+i*stride+4,1-xy[i][1]/540);}
                canvas_forward(0,input,count,primitive,(uintptr_t)matrix,0,color);assert(!memcmp(captured,input,count*stride));memcpy(input,saved,sizeof(input));
                test_project_width=1920;test_project_height=1080;test_project_scale=1;
                canvas_forward(0,input,count,primitive,(uintptr_t)matrix,0,color);assert(!memcmp(captured,input,count*stride));
                test_project_width=widths[mode];test_project_height=heights[mode];test_project_scale=heights[mode]/1080.f;
            }
        }
    }
    test_project_width=3840;test_project_height=1080;test_project_scale=1;test_stride=24;
    puts("PASS: screen-rectangle sweep across callers, both heights, textured/solid quad/triangle paths; cached input, UV, colour/alpha and depth preserved; normalized geometry, partial rectangles, transformed UI, invalid pointers/matrices and default16:9 excluded");
}
static void test_gui_snapshot(void) {
    const float matrix[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    const float xy[4][2]={{0,0},{1920,0},{0,1080},{1920,1080}};
    const float *widths=canvas_widths,*heights=canvas_heights;
    unsigned char source[96],saved[96];memset(source,0x73,sizeof(source));
    for(unsigned i=0;i<4;++i){
        write_float(source+i*24,xy[i][0]);write_float(source+i*24+4,xy[i][1]);write_float(source+i*24+8,-10);
        write_float(source+i*24+16,xy[i][0]/1920);write_float(source+i*24+20,xy[i][1]/1080);
    }
    memcpy(saved,source,sizeof(source));test_caller=0x20cf30;test_stride=24;
    for(unsigned mode=0;mode<CANVASES;++mode){
        test_project_width=widths[mode];test_project_height=heights[mode];test_project_scale=heights[mode]/1080.f;
        for(unsigned repeat=0;repeat<8;++repeat){
            canvas_forward(0,source,4,4,(uintptr_t)matrix,0,0);
            assert(!memcmp(saved,source,sizeof(source)));
            for(unsigned i=0;i<4;++i){
                /* Verify final pixels after the captured queued GUI root. */
                assert(fabsf(read_float(captured+i*24)*test_project_scale-(xy[i][0]>0?widths[mode]:0))<0.001f);
                assert(fabsf(read_float(captured+i*24+4)*test_project_scale-(xy[i][1]>0?heights[mode]:0))<0.001f);
                assert(!memcmp(captured+i*24+8,source+i*24+8,16));
            }
        }
        write_float(source+16,.1f);canvas_forward(0,source,4,4,(uintptr_t)matrix,0,0);assert(!memcmp(captured,source,sizeof(source)));memcpy(source,saved,sizeof(source));
        write_float(source,50);canvas_forward(0,source,4,4,(uintptr_t)matrix,0,0);assert(!memcmp(captured,source,sizeof(source)));memcpy(source,saved,sizeof(source));
    }
    test_project_width=1920;test_project_height=1080;test_project_scale=1;
    canvas_forward(0,source,4,4,(uintptr_t)matrix,0,0);assert(!memcmp(captured,source,sizeof(source)));
    test_project_width=3840;
    puts("PASS: captured GUI snapshot fallback fills exact native pixels after queued root on every wide canvas; no doubled scale, input/UV/alpha/depth preserved, repeat/default/partial/UV fallback checks");
}
static void test_rotated_glyph_translation(void) {
    unsigned char source[96],saved[96];memset(source,0x52,sizeof(source));
    for(unsigned i=0;i<4;++i){write_float(source+i*24,12.f+i*17);write_float(source+i*24+4,8.f+i*9);}
    memcpy(saved,source,sizeof(source));test_caller=0x4a4c59;test_stride=24;
    const float *widths=canvas_widths,*heights=canvas_heights;
    const float transforms[][3]={{0,1,1},{0.04180117f,1,1},{-0.12f,1,1},{0,2,.5f},{.24f,.75f,1.6f}};
    for(unsigned mode=0;mode<CANVASES;++mode){
        test_project_width=widths[mode];test_project_height=heights[mode];test_project_scale=heights[mode]/1080.f;
        for(unsigned t=0;t<sizeof(transforms)/sizeof(transforms[0]);++t){
            float c=cosf(transforms[t][0]),s=sinf(transforms[t][0]);
            float matrix[16]={c*transforms[t][1],s*transforms[t][1],0,0,-s*transforms[t][2],c*transforms[t][2],0,0,0,0,1,0,957.55908f,262.04181f,-10,1};
            float matrix_saved[16];memcpy(matrix_saved,matrix,sizeof(matrix));
            for(unsigned repeat=0;repeat<8;++repeat){
                canvas_forward(0,source,4,4,(uintptr_t)matrix,0,0);
                assert(!memcmp(source,saved,sizeof(source)) && !memcmp(matrix,matrix_saved,sizeof(matrix)));
                assert(captured_descriptor==(uintptr_t)matrix);
                for(unsigned i=0;i<4;++i){
                    float x=read_float(source+i*24),y=read_float(source+i*24+4);
                    float cx=read_float(captured+i*24),cy=read_float(captured+i*24+4);
                    float dx=(matrix[0]*(cx-x)+matrix[4]*(cy-y))*test_project_scale;
                    float dy=(matrix[1]*(cx-x)+matrix[5]*(cy-y))*test_project_scale;
                    assert(fabsf(dx-(widths[mode]-1920*test_project_scale)*.5f)<.003f);
                    assert(fabsf(dy)<.003f);
                    assert(!memcmp(source+i*24+8,captured+i*24+8,16));
                }
            }
            test_project_width=1920;test_project_height=1080;test_project_scale=1;
            canvas_forward(0,source,4,4,(uintptr_t)matrix,0,0);assert(!memcmp(source,captured,sizeof(source)));
            test_project_width=widths[mode];test_project_height=heights[mode];test_project_scale=heights[mode]/1080.f;
        }
        float matrix[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
        canvas_forward(0,source,4,4,1,0,0);assert(!memcmp(source,captured,sizeof(source)));
        matrix[0]=0;canvas_forward(0,source,4,4,(uintptr_t)matrix,0,0);assert(!memcmp(source,captured,sizeof(source)));
        matrix[0]=NAN;canvas_forward(0,source,4,4,(uintptr_t)matrix,0,0);assert(!memcmp(source,captured,sizeof(source)));
        matrix[0]=1;matrix[2]=.2f;canvas_forward(0,source,4,4,(uintptr_t)matrix,0,0);assert(!memcmp(source,captured,sizeof(source)));
    }
    test_project_width=3840;test_project_height=1080;test_project_scale=1;
    puts("PASS: tilted/scaled glyph matrices yield horizontal-only physical translation on every wide canvas; unchanged vertical placement, cached vertices/matrix and payload; repeats/default16:9/invalid/singular/3D fallback");
}
static void test_offscreen_layer(void) {
    /* Mission-table map layer: authored map panel, full-layer background, glyphs and a
       solid fill drawn into the 2048x1440 render-to-texture target stay untranslated. */
    const float matrix[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    const float *widths=canvas_widths,*heights=canvas_heights;
    const float px[]={0,2048,0,2048},py[]={0,0,1434,1434};
    const float bx[]={0,1920,0,1920},by[]={0,0,1080,1080};
    const uintptr_t callers[]={0x229a9e,0x4a4c59,0x33c2fe,0x20cf30,0x12345};
    unsigned char source[96],saved[96];
    for(unsigned mode=0;mode<CANVASES;++mode){
        test_project_width=widths[mode];test_project_height=heights[mode];test_project_scale=heights[mode]/1080.f;
        for(unsigned shape=0;shape<2;++shape){
            memset(source,0x3c,sizeof(source));
            for(unsigned i=0;i<4;++i){write_float(source+i*24,shape?bx[i]:px[i]);write_float(source+i*24+4,shape?by[i]:py[i]);
                write_float(source+i*24+16,(shape?bx[i]:px[i])/(shape?1920.f:2048.f));write_float(source+i*24+20,(shape?by[i]:py[i])/(shape?1080.f:1434.f));}
            memcpy(saved,source,sizeof(source));
            for(unsigned c=0;c<sizeof(callers)/sizeof(callers[0]);++c){
                test_caller=callers[c];test_stride=24;test_offscreen=1;
                for(unsigned repeat=0;repeat<4;++repeat){
                    canvas_forward(0,source,4,4,(uintptr_t)matrix,0,0);
                    assert(!memcmp(source,saved,sizeof(source)) && !memcmp(captured,source,sizeof(source)));
                }
                test_offscreen=0;canvas_forward(0,source,4,4,(uintptr_t)matrix,0,0);
                if(callers[c]==0x229a9e || callers[c]==0x4a4c59 || (shape && (callers[c]==0x33c2fe || callers[c]==0x20cf30)))
                    assert(memcmp(captured,source,sizeof(source))); /* on-screen UI still widened/centred */
            }
        }
        test_caller=0x2297d7;test_stride=16;
        const float cx[]={1920,1920,0,0},cy[]={0,300,0,300};
        for(unsigned i=0;i<4;++i){write_float(source+i*16,cx[i]);write_float(source+i*16+4,cy[i]);}
        memcpy(saved,source,sizeof(source));test_offscreen=1;
        canvas_forward(0,source,4,4,(uintptr_t)matrix,0,1);assert(!memcmp(source,saved,sizeof(source)) && !memcmp(captured,source,64));
        test_offscreen=0;canvas_forward(0,source,4,4,(uintptr_t)matrix,0,1);assert(memcmp(captured,source,64));
    }
    test_project_width=3840;test_project_height=1080;test_project_scale=1;test_stride=24;
    puts("PASS: render-to-texture UI layers (mission-table map) bypass the wide-screen canvas on every wide canvas for authored/glyph/fade/snapshot/screen-rect/colour paths; on-screen UI unchanged");
}
/* Runs the (patched) site with the guest's live registers: R14 = cursor object, RAX = node. */
__attribute__((naked,noinline)) static void run_cursor_site(void *object __attribute__((unused)),void *node __attribute__((unused)),void *site __attribute__((unused))) {
    __asm__ volatile("push %r14; push %rbx; mov %rdi,%r14; mov %rsi,%rax; call *%rdx; pop %rbx; pop %r14; ret");
}
static void test_cursor_site(void) {
    size_t page=(size_t)sysconf(_SC_PAGESIZE);
    unsigned char *code=mmap(NULL,page,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    assert(code!=MAP_FAILED);memset(code,0xcc,page);
    unsigned char *site=code+0x180;code[0x180+sizeof(cursor_site)-0x12b]=0xc3; /* the jmp target returns */
    memcpy(site,cursor_site,sizeof(cursor_site));site[9]^=1;
    assert(install_cursor(site)==-1 && site[9]==(cursor_site[9]^1));site[9]^=1;
    assert(!install_cursor(site) && memcmp(site,cursor_site,sizeof(cursor_site)));
    assert(!mprotect(code,page,PROT_READ|PROT_EXEC));
    static float object[0x300/4] __attribute__((aligned(16)));
    float node[4];
    object[0x2e0/4]=2246.4f;object[0x2e4/4]=777.86f;object[0x2e8/4]=5;object[0x2ec/4]=1;
    unsigned char source[4*24+16];memset(source,0,sizeof(source));
    for(unsigned i=0;i<4;++i){write_float(source+i*24,i&1?120:100);write_float(source+i*24+4,i&2?120:100);}
    for(unsigned l=0;l<=CANVASES;++l){
        if(l<CANVASES)use_canvas(l);else{test_project_width=1920;test_project_height=1080;test_project_scale=1;}
        test_caller=0x229a9e;cursor_dx=-1;invoke(source,4);
        const float dx=l<CANVASES?canvas_dx(l):0;assert(fabsf(cursor_dx-dx)<0.001f);
        for(unsigned run=0;run<4;++run){
            node[0]=node[1]=node[2]=node[3]=9;run_cursor_site(object,node,site);
            assert(fabsf(node[0]-(2246.4f-dx))<0.001f && node[1]==777.86f && node[2]==0 && node[3]==9);
            assert(object[0x2e0/4]==2246.4f && object[0x2e4/4]==777.86f && object[0x2e8/4]==5 && object[0x2ec/4]==1);
        }
    }
    test_project_width=3840;test_project_height=1080;test_project_scale=1;
    munmap(code,page);
    puts("PASS: world-map cursor node cancels only the canvas translation (the authored margin on every wide canvas, 0 at 16:9); cursor state, Y and Z write untouched; signature rejection");
}
/* Live vertex dump of the Jutland event map: x,y,z,RGBA,u,v per vertex. */
static void event_map_quad(unsigned char *source,float min_x,float min_y) {
    const float xs[]={min_x+4096,min_x+4096,min_x,min_x},ys[]={min_y,min_y+2048,min_y,min_y+2048};
    memset(source,0xa5,4*24+16);
    for(unsigned i=0;i<4;++i){
        unsigned char *v=source+i*24;
        write_float(v,xs[i]);write_float(v+4,ys[i]);write_float(v+8,-10);memset(v+12,0xff,4);
        write_float(v+16,xs[i]>min_x?1:0);write_float(v+20,ys[i]>min_y?1:0);
    }
}
static void invoke_strip(unsigned char *source) {
    fake_guest(0x12345678,source,4,4,(uintptr_t)invoke_matrix);
    assert(captured_context==0x12345678 && captured_count==4 && captured_primitive==4 && captured_descriptor==(uintptr_t)invoke_matrix);
}
static void test_event_map(void) {
    unsigned char source[4*24+16],saved[sizeof(source)];
    const float *widths=canvas_widths,*heights=canvas_heights;
    test_caller=0x229a9e;test_stride=24;test_offscreen=0;
    for(unsigned mode=0;mode<CANVASES;++mode){
        test_project_width=widths[mode];test_project_height=heights[mode];test_project_scale=heights[mode]/1080.f;
        /* Observed: scrolled to its right end; then the same map scrolled to its left end. */
        const float starts[]={-2112,-64};
        for(unsigned s=0;s<2;++s){
            event_map_quad(source,starts[s],-958);memcpy(saved,source,sizeof(source));
            submit_log_count=0;invoke_strip(source);assert(!memcmp(source,saved,sizeof(source)));
            assert(submit_log_count==3);
            for(unsigned i=0;i<4;++i){
                assert(fabsf(read_float(submit_log[0]+i*24)-(read_float(source+i*24)+canvas_dx(mode)))<0.01f);
                assert(!memcmp(submit_log[0]+i*24+4,source+i*24+4,20));
            }
            const float map_dx=canvas_dx(mode),expect[2][2]={{-8,map_dx-64},{map_dx+1984,1928+2*map_dx}};
            for(unsigned b=0;b<2;++b)for(unsigned i=0;i<4;++i){
                const unsigned char *v=submit_log[1+b]+i*24;
                int high=read_float(source+i*24)>starts[s];
                assert(fabsf(read_float(v)-expect[b][high])<0.01f);
                assert(!memcmp(v+4,source+i*24+4,8));
                assert(v[12]==0 && v[13]==0 && v[14]==0 && v[15]==0xff);
                assert(read_float(v+16)==0.5f && read_float(v+20)==0.5f);
            }
            /* Physical seams match the map's own edges on screen (1195/3925 at 5120x1440). */
            assert(fabsf((map_dx-64)*test_project_scale-(widths[mode]-2048*test_project_scale)/2)<0.01f);
        }
        /* Bars fade with the map: alpha is kept, RGB zeroed. */
        event_map_quad(source,-2112,-958);for(unsigned i=0;i<4;++i)source[i*24+15]=0x40;
        submit_log_count=0;invoke_strip(source);assert(submit_log_count==3 && submit_log[1][15]==0x40 && submit_log[2][12]==0);
        /* No bars: map covering the whole wide canvas, short of 16:9, partial height,
           partial UVs, other sizes, other callers, off-screen layers, 16:9 output. */
        event_map_quad(source,-1100,-958);submit_log_count=0;invoke_strip(source);assert(submit_log_count==1);
        event_map_quad(source,0,-958);submit_log_count=0;invoke_strip(source);assert(submit_log_count==3);
        event_map_quad(source,1920-4096-40,-958);submit_log_count=0;invoke_strip(source);assert(submit_log_count==1);
        event_map_quad(source,-2112,100);submit_log_count=0;invoke_strip(source);assert(submit_log_count==1);
        event_map_quad(source,-2112,-958);write_float(source+16,0.5f);submit_log_count=0;invoke_strip(source);assert(submit_log_count==1);
        event_map_quad(source,-2112,-958);write_float(source,1000);write_float(source+24,1000);submit_log_count=0;invoke_strip(source);assert(submit_log_count==1);
        event_map_quad(source,-2112,-958);test_caller=0x4a4c59;submit_log_count=0;invoke_strip(source);assert(submit_log_count==1);test_caller=0x229a9e;
        submit_log_count=0;invoke(source,4);assert(submit_log_count==1);
        test_offscreen=1;submit_log_count=0;invoke_strip(source);assert(submit_log_count==1 && !memcmp(captured,source,96));test_offscreen=0;
    }
    test_project_width=1920;test_project_height=1080;test_project_scale=1;
    event_map_quad(source,-2112,-958);submit_log_count=0;invoke_strip(source);assert(submit_log_count==1 && !memcmp(captured,source,96));
    test_project_width=3840;test_project_height=1080;test_project_scale=1;
    puts("PASS: event map bars outside the map's symmetric window right after the map (seams at its edges, alpha kept, Y/Z preserved, input untouched) on every wide canvas; full-cover/short/partial/other-caller/off-screen/16:9 exclusions");
}
static void centred_quad(unsigned char *source,float x0,float y0,float y1,float u0,float u1) {
    memset(source,0x5c,4*24+16);
    for(unsigned i=0;i<4;++i){
        unsigned char *v=source+i*24;int hx=i<2,hy=i&1;
        write_float(v,hx?1920-x0:x0);write_float(v+4,hy?y1:y0);write_float(v+8,-10);
        v[12]=0x64;v[13]=0x46;v[14]=0x28;v[15]=0xff;
        write_float(v+16,hx?u1:u0);write_float(v+20,hy?.21f:.1475f);
    }
}
static void test_wide_canvas(void) {
    for(unsigned k=0;k<CANVASES;++k)assert(wide_canvas(canvas_widths[k],canvas_heights[k]));
    const float bad[][2]={{1920,1080},{2560,1440},{3840,2160},{3840,1600},{5760,1080},{3440.5f,1440},{0,1080},{-3440,1440},{NAN,1440},{INFINITY,1440}};
    for(unsigned i=0;i<sizeof(bad)/sizeof(bad[0]);++i)assert(!wide_canvas(bad[i][0],bad[i][1]));
    /* Native canvases: the wide ones and the other 16:9 sizes; never the stock 1920x1080. */
    const float standard[][2]={{1024,576},{1280,720},{2560,1440},{3840,2160}};
    for(unsigned i=0;i<4;++i)assert(native_canvas(standard[i][0],standard[i][1]) && !wide_canvas(standard[i][0],standard[i][1]));
    for(unsigned k=0;k<CANVASES;++k)assert(native_canvas(canvas_widths[k],canvas_heights[k]));
    const float other[][2]={{1920,1080},{1600,900},{1024,600},{1366,768},{3840,1600},{NAN,576}};
    for(unsigned i=0;i<sizeof(other)/sizeof(other[0]);++i)assert(!native_canvas(other[i][0],other[i][1]));
    assert(wide_canvas_name("3440x1440") && wide_canvas_name("2560x1080") && wide_canvas_name("5120x1440"));
    assert(!wide_canvas_name(NULL) && !wide_canvas_name("") && !wide_canvas_name("1920x1080") && !wide_canvas_name("3440X1440") &&
           !wide_canvas_name("3440x1440 ") && !wide_canvas_name("3440x") && !wide_canvas_name("3440x1440x2") && !wide_canvas_name("3840x1600"));
    /* Unsupported sizes leave every submit untouched. */
    unsigned char source[4*24+16];memset(source,0x33,sizeof(source));
    const float xs[]={1920,1920,0,0},ys[]={0,1080,0,1080};
    for(unsigned i=0;i<4;++i){write_float(source+i*24,xs[i]);write_float(source+i*24+4,ys[i]);write_float(source+i*24+16,xs[i]/1920);write_float(source+i*24+20,ys[i]/1080);}
    const float sizes[][2]={{3840,1600},{5760,1080},{1600,900}};
    const uintptr_t callers[]={0x229a9e,0x4a4c59};
    test_stride=24;test_offscreen=0;
    for(unsigned z=0;z<3;++z)for(unsigned c=0;c<2;++c){
        test_project_width=sizes[z][0];test_project_height=sizes[z][1];test_project_scale=sizes[z][1]/1080.f;test_caller=callers[c];
        invoke(source,4);assert(!memcmp(captured,source,96));
    }
    use_canvas(3);
    puts("PASS: wide canvases 2560x1080..5120x1440 accepted (and 16:9 576/720/1440/2160 as native canvases); stock 1080p, other heights, beyond 32:9, invalid sizes and malformed names rejected; unsupported sizes leave UI untouched");
}
static void test_centred_pulse(void) {
    unsigned char source[4*24+16],saved[sizeof(source)];
    const float *widths=canvas_widths,*heights=canvas_heights;
    /* Observed frames of the pulsing vignette (full UVs) and tinted glow (atlas sub-rect):
       inset, flush and overscanned, including the frames that used to stay 16:9. */
    const float frames[][3]={{-60.54f,-61.52f,1149.52f},{-39.13f,-32.46f,1120.46f},{-7.62f,-4.82f,1092.82f},
                             {-113.72f,-147.25f,1235.25f},{.85f,-27.6f,1115.6f},{20.66f,-11.86f,1099.86f},
                             {52.01f,22.22f,1065.78f},{64.78f,37.43f,1050.57f}};
    test_caller=0x229a9e;test_stride=24;test_offscreen=0;
    for(unsigned mode=0;mode<CANVASES;++mode){
        test_project_width=widths[mode];test_project_height=heights[mode];test_project_scale=heights[mode]/1080.f;
        for(unsigned f=0;f<sizeof(frames)/sizeof(frames[0]);++f){
            centred_quad(source,frames[f][0],frames[f][1],frames[f][2],f<3?0:.917f,f<3?1:.9795f);
            memcpy(saved,source,sizeof(source));
            for(unsigned n=0;n<4;++n){
                submit_log_count=0;invoke(source,4);assert(submit_log_count==1 && !memcmp(source,saved,sizeof(source)));
                for(unsigned i=0;i<4;++i){
                    assert(close_to(read_float(captured+i*24),read_float(source+i*24)*canvas_stretch(mode)));
                    assert(!memcmp(captured+i*24+4,source+i*24+4,20));
                }
            }
            /* Physically centred on the wide screen, with the authored proportion of its width. */
            float l=read_float(captured+2*24)*test_project_scale,r=read_float(captured)*test_project_scale;
            assert(fabsf(l+r-widths[mode])<0.01f);
            assert(fabsf(l/widths[mode]-frames[f][0]/1920)<0.0001f);
        }
        /* Kept on the centred 16:9 canvas: off-centre, under 0.9x or over 1.5x on either axis,
           vertically off-centre, other callers; untouched on off-screen layers. */
        centred_quad(source,-40,-32,1120,0,1);write_float(source,1962);write_float(source+24,1962);
        invoke(source,4);assert(close_to(read_float(captured),1962+canvas_dx(mode)));
        centred_quad(source,100,-32,1120,0,1);invoke(source,4);assert(close_to(read_float(captured),1820+canvas_dx(mode)));
        centred_quad(source,-500,-32,1120,0,1);invoke(source,4);assert(close_to(read_float(captured),2420+canvas_dx(mode)));
        centred_quad(source,-40,60,1020,0,1);invoke(source,4);assert(close_to(read_float(captured),1960+canvas_dx(mode)));
        centred_quad(source,-40,-300,1380,0,1);invoke(source,4);assert(close_to(read_float(captured),1960+canvas_dx(mode)));
        centred_quad(source,-40,-32,1150,0,1);invoke(source,4);assert(close_to(read_float(captured),1960+canvas_dx(mode)));
        centred_quad(source,-40,-32,1120,0,1);test_caller=0x4a4c59;invoke(source,4);assert(close_to(read_float(captured),1960+canvas_dx(mode)));
        test_caller=0x229a9e;test_offscreen=1;invoke(source,4);assert(!memcmp(captured,source,96));test_offscreen=0;
    }
    test_project_width=1920;test_project_height=1080;test_project_scale=1;
    centred_quad(source,-40,-32,1120,0,1);invoke(source,4);assert(!memcmp(captured,source,96));
    test_project_width=3840;test_project_height=1080;test_project_scale=1;
    puts("PASS: pulsing centred vignette/glow layers widen on every observed frame (inset, flush, overscanned) on every wide canvas, centred with authored proportion, Y/UV/colour/input preserved; off-centre/size/vertical/caller/off-screen/16:9 exclusions");
}
/* rax=layout node, rdx=0 as at 0x2d30d0: the site loads the authored size from node+4/+8
   and calls FUN_00221b00. */
__attribute__((naked,noinline)) static void run_layer_site(const void *node __attribute__((unused)),
                                                           const unsigned char *site __attribute__((unused))) {
    __asm__ volatile("mov %rdi,%rax; xor %edx,%edx; call *%rsi; ret");
}
static void test_layer_site(void) {
    /* A stand-in image: the site at 0x2d30d0, FUN_00221b00 recording its two floats, and the
       GUI scale pair at 0x968c80, all at their game offsets from one base. */
    const size_t span=0x96a000;
    unsigned char *base=mmap(NULL,span,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    assert(base!=MAP_FAILED);
    static float recorded[2];
    unsigned char *site=base+0x2d30d0,*create=base+0x221b00;
    memcpy(site,layer_site,sizeof(layer_site));site[sizeof(layer_site)]=0xc3; /* ret at 0x2d30e1 */
    uintptr_t out=(uintptr_t)recorded;
    create[0]=0x48;create[1]=0xb8;memcpy(create+2,&out,8);                   /* movabs rax,recorded */
    memcpy(create+10,"\xc5\xfa\x11\x00\xc5\xfa\x11\x48\x04\xc3",10);         /* store xmm0, xmm1; ret */
    float *gui=(float*)(base+0x968c80);
    float node[3]={0,960,1080};
    assert(!mprotect(base,span,PROT_READ|PROT_EXEC));
    recorded[0]=recorded[1]=-1;run_layer_site(node,site);
    assert(recorded[0]==960 && recorded[1]==1080); /* the game's own call */
    assert(!mprotect(base,span,PROT_READ|PROT_WRITE));
    site[3]^=1;assert(install_layer_scale(site,(uintptr_t)base)==-1 && site[3]==(layer_site[3]^1));site[3]^=1;
    assert(!install_layer_scale(site,(uintptr_t)base) && memcmp(site,layer_site,sizeof(layer_site)) && site[sizeof(layer_site)]==0xc3);
    const float heights[]={576,720,1080,1440,2160};
    for(unsigned i=0;i<5;++i){
        assert(!mprotect(base,span,PROT_READ|PROT_WRITE));
        gui[0]=gui[1]=heights[i]/1080.f;
        assert(!mprotect(base,span,PROT_READ|PROT_EXEC));
        for(unsigned run=0;run<3;++run){
            recorded[0]=recorded[1]=-1;run_layer_site(node,site);
            assert(recorded[0]==960*gui[0] && recorded[1]==1080*gui[1]);
            assert((int)recorded[0]==(int)(heights[i]*8/9) && (int)recorded[1]==(int)heights[i]);
        }
    }
    assert(node[1]==960 && node[2]==1080);
    munmap(base,span);
    puts("PASS: authored-size UI layers (history book pages) allocated at the GUI scale: 960x1080 becomes 512x576, 640x720, 960x1080, 1280x1440, 1920x2160; layout data untouched; signature rejection");
}
/* xmm1=W, xmm0=H, rax=texture as at 0x20cfb0; the stand-in for 0x20cfc8 stores u1 (xmm4)
   and v1 (xmm2) to rdi. */
__attribute__((naked,noinline)) static void run_paper_site(const void *texture __attribute__((unused)),float *out __attribute__((unused)),
                                                           const unsigned char *site __attribute__((unused)),
                                                           float width __attribute__((unused)),float height __attribute__((unused))) {
    __asm__ volatile("vmovaps %xmm0,%xmm3; vmovaps %xmm1,%xmm0; vmovaps %xmm3,%xmm1; mov %rdi,%rax; mov %rsi,%rdi; call *%rdx; ret");
}
static void test_paper_site(void) {
    size_t page=(size_t)sysconf(_SC_PAGESIZE);
    unsigned char *code=mmap(NULL,page,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    assert(code!=MAP_FAILED);memset(code,0xcc,page);
    unsigned char *site=code+0x100;
    memcpy(site,paper_site,sizeof(paper_site));
    memcpy(site+sizeof(paper_site),"\xc5\xfa\x11\x27\xc5\xfa\x11\x57\x04\xc3",10); /* vmovss [rdi],xmm4; vmovss [rdi+4],xmm2; ret */
    unsigned char texture[0x80];memset(texture,0x5a,sizeof(texture));
    uint16_t extent[2]={2048,1024};memcpy(texture+0x78,extent,4);
    float out[2];
    assert(!mprotect(code,page,PROT_READ|PROT_EXEC));
    run_paper_site(texture,out,site,3440,1440);assert(out[0]==3440.f/2048 && out[1]==1440.f/1024); /* game's own result */
    assert(!mprotect(code,page,PROT_READ|PROT_WRITE));
    site[3]^=1;assert(install_paper(site)==-1 && site[3]==(paper_site[3]^1));site[3]^=1;
    assert(!install_paper(site) && memcmp(site,paper_site,PAPER_REPLACED) &&
           !memcmp(site+PAPER_REPLACED,paper_site+PAPER_REPLACED,sizeof(paper_site)-PAPER_REPLACED));
    assert(!mprotect(code,page,PROT_READ|PROT_EXEC));
    unsigned char source[4*24+16];memset(source,0,sizeof(source));
    for(unsigned i=0;i<4;++i){write_float(source+i*24,i&1?120:100);write_float(source+i*24+4,i&2?120:100);}
    test_caller=0x229a9e;test_stride=24;test_offscreen=0;
    const uint16_t textures[2][2]={{2048,1024},{1024,512}};
    const float standard[4][2]={{1024,576},{1280,720},{2560,1440},{3840,2160}};
    for(unsigned l=0;l<=CANVASES+4;++l){
        /* Every wide canvas, every other 16:9 native size, then 1600x900 (not a native canvas:
           the game's own result). */
        if(l<CANVASES)use_canvas(l);
        else if(l<CANVASES+4){test_project_width=standard[l-CANVASES][0];test_project_height=standard[l-CANVASES][1];test_project_scale=test_project_height/1080.f;}
        else{test_project_width=1600;test_project_height=900;test_project_scale=900/1080.f;}
        paper_scale[0]=paper_scale[1]=-1;invoke(source,4);
        const float w=test_project_width,h=test_project_height;
        for(unsigned t=0;t<2;++t){
            memcpy(texture+0x78,textures[t],4);
            for(unsigned run=0;run<4;++run){
                out[0]=out[1]=-1;run_paper_site(texture,out,site,w,h);
                if(l<CANVASES+4)assert(fabsf(out[0]-1920.f/textures[t][0])<1e-5f && fabsf(out[1]-1080.f/textures[t][1])<1e-5f);
                else assert(out[0]==w/textures[t][0] && out[1]==h/textures[t][1]);
            }
        }
        assert(texture[0x77]==0x5a && texture[0x7c]==0x5a);
    }
    test_project_width=3840;test_project_height=1080;test_project_scale=1;
    munmap(code,page);
    puts("PASS: narration paper composite UVs equal stock 1920x1080 on every native canvas, wide and 16:9 (any paper size), game's own result on other sizes; v1 branch target kept; signature rejection");
}
/* The other 16:9 sizes: only the paper patch, scaled for the launch (no UI hooks run). */
/* Screen-edge fades (the red frame while the player is hit): solid strips on the left/right
   screen edge over the full height, fading inward, stay on the canvas edges; full-width top and
   bottom strips stretch as backgrounds. Constant panels, reversed fades and short strips keep
   the centred 16:9 placement. */
static void put_color_quad(unsigned char *input,const float xy[4][2],const uint32_t colors[4]) {
    memset(input,0x5a,4*16+16);
    for(unsigned i=0;i<4;++i){
        write_float(input+i*16,xy[i][0]);write_float(input+i*16+4,xy[i][1]);write_float(input+i*16+8,-10);
        memcpy(input+i*16+12,&colors[i],4);
    }
}
static void test_screen_edge_fade(void) {
    unsigned char input[4*16+16];
    const uint32_t fade_left[4]={0,0,0x7f00007f,0x7f00007f},fade_right[4]={0,0,0x7f00007f,0x7f00007f};
    const uint32_t constant[4]={0x7f00007f,0x7f00007f,0x7f00007f,0x7f00007f},reversed[4]={0x7f00007f,0x7f00007f,0,0};
    const float left[4][2]={{400,0},{400,1080},{-0.f,0},{-0.f,1080}},right[4][2]={{1520,0},{1520,1080},{1920,0},{1920,1080}};
    const float short_left[4][2]={{400,0},{400,540},{0,0},{0,540}},top[4][2]={{1920,0},{1920,400},{0,0},{0,400}};
    const uint32_t top_colors[4]={0x7f00007f,0,0x7f00007f,0};
    test_caller=0x2297d7;test_stride=16;
    for(unsigned k=0;k<CANVASES;++k){
        use_canvas(k);
        const float dx=canvas_dx(k),stretch=canvas_stretch(k);
        struct {const float (*xy)[2];const uint32_t *colors;int mode;} cases[]={
            {left,fade_left,0},{right,fade_right,2},{left,constant,1},{left,reversed,1},{short_left,fade_left,1},{top,top_colors,3}};
        for(unsigned c=0;c<sizeof(cases)/sizeof(cases[0]);++c){
            put_color_quad(input,cases[c].xy,cases[c].colors);
            unsigned char saved[sizeof(input)];memcpy(saved,input,sizeof(input));
            canvas_forward(0,input,4,4,0,0,1);
            assert(!memcmp(input,saved,sizeof(input)) && captured_count==4);
            for(unsigned i=0;i<4;++i){
                const float x=cases[c].xy[i][0];
                const float want=cases[c].mode==0?x:cases[c].mode==2?x+2*dx:cases[c].mode==3?x*stretch:x+dx;
                if(!close_to(read_float(captured+i*16),want))
                    fprintf(stderr,"edge fade: canvas=%u case=%u vertex=%u x=%g want=%g\n",k,c,i,read_float(captured+i*16),want);
                assert(close_to(read_float(captured+i*16),want));
                assert(!memcmp(captured+i*16+4,input+i*16+4,12));
            }
        }
        /* The right strip ends exactly on the canvas edge (GUI units). */
        put_color_quad(input,right,fade_right);canvas_forward(0,input,4,4,0,0,1);
        assert(close_to(read_float(captured+2*16),canvas_widths[k]/test_project_scale));
    }
    /* 16:9 keeps the stock coordinates. */
    test_project_width=1920;test_project_height=1080;test_project_scale=1;
    put_color_quad(input,right,fade_right);canvas_forward(0,input,4,4,0,0,1);assert(!memcmp(captured,input,64));
    use_canvas(3);test_stride=24;test_caller=0;
    puts("PASS: screen-edge fades (left/right, full height, fading inward) stay on every wide canvas edge; full-width strips stretch; constant panels, reversed fades, short strips and 16:9 keep stock placement");
}
/* The other 16:9 native sizes: no margin, so authored UI, glyphs, solid panels, edge fades,
   backgrounds, the event map, the GUI snapshot (scaled by the GUI root itself) and projected
   anchors keep their exact stock values; full-screen copies drawn in stock pixels (identity
   world, full UVs) and the saved-frame crossfade reach the native extent; the paper takes the
   stock texel scale. */
static void test_standard_canvas(void) {
    const float standard[4][2]={{1024,576},{1280,720},{2560,1440},{3840,2160}};
    float copy_matrix[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,-10,1};
    unsigned char source[6*24+16],saved[sizeof(source)];
    const float full_x[]={1920,1920,0,0},full_y[]={0,1080,0,1080};
    test_offscreen=0;
    for(unsigned z=0;z<4;++z){
        const float w=standard[z][0],h=standard[z][1];
        test_project_width=w;test_project_height=h;test_project_scale=h/1080.f;
        struct UiLayout ui;assert(ui_layout(&ui) && ui.margin==0 && ui.stretch==1);
        /* Authored textured quads: a full-width background, an overscanned dim layer, a panel, a
           glyph and the event map, all exactly as submitted. */
        const float quads[5][4]={{0,1920,0,1080},{-13,1933,-19,1107},{300,700,200,400},{100,140,50,90},{-2112,1984,-958,1090}};
        const uintptr_t authored[]={0x229a9e,0x229a9e,0x229a9e,0x4a4c59,0x229a9e};
        for(unsigned q=0;q<5;++q){
            memset(source,0x3c,sizeof(source));
            for(unsigned i=0;i<4;++i){unsigned char *v=source+i*24;
                write_float(v,i<2?quads[q][1]:quads[q][0]);write_float(v+4,i&1?quads[q][3]:quads[q][2]);write_float(v+8,-10);
                write_float(v+16,i<2?1:0);write_float(v+20,i&1?1:0);}
            memcpy(saved,source,sizeof(source));test_caller=authored[q];test_stride=24;
            submit_log_count=0;invoke(source,4);
            assert(submit_log_count==1 && !memcmp(captured,source,96) && !memcmp(source,saved,sizeof(source)));
        }
        assert(fabsf(paper_scale[0]-1920.f/w)<1e-6f && fabsf(paper_scale[1]-1080.f/h)<1e-6f && cursor_dx==0);
        /* Solid authored panels and a red screen-edge fade. */
        memset(source,0,sizeof(source));
        const float edge_x[]={0,0,400,400},edge_y[]={0,1080,0,1080};
        for(unsigned i=0;i<4;++i){unsigned char *v=source+i*16;write_float(v,edge_x[i]);write_float(v+4,edge_y[i]);v[12]=0xff;v[15]=edge_x[i]>0?0:0x7f;}
        test_caller=0x2297d7;test_stride=16;
        canvas_forward(0,source,4,4,(uintptr_t)invoke_matrix,0,1);assert(!memcmp(captured,source,64));
        /* A full-screen copy in stock pixels, textured and solid, quad and triangle list: the
           native extent, UV/colour/depth untouched. A solid one never shrinks: below 1080
           lines it keeps the stock extent, which the GUI scale maps onto the canvas (the
           screen fade at 0x20bb20 after its extent patch). */
        const float xy[6][2]={{0,0},{1920,0},{0,1080},{1920,1080},{0,1080},{1920,0}};
        for(int color=0;color<=1;++color)for(unsigned tri=0;tri<2;++tri){
            const size_t stride=color?16:24;const unsigned count=tri?6:4;test_stride=stride;
            memset(source,0x51,sizeof(source));
            for(unsigned i=0;i<count;++i){unsigned char *v=source+i*stride;
                write_float(v,xy[i][0]);write_float(v+4,xy[i][1]);write_float(v+8,-10);
                if(!color){write_float(v+16,xy[i][0]/1920);write_float(v+20,xy[i][1]/1080);}}
            const float ew=color?fmaxf(w,1920):w,eh=color?fmaxf(h,1080):h;
            const uintptr_t copiers[]={0x22bb64,0x20bb20};
            for(unsigned c=0;c<(color?2u:1u);++c){
                memcpy(saved,source,sizeof(source));test_caller=copiers[c];
                canvas_forward(0,source,count,tri?3:4,(uintptr_t)copy_matrix,0,color);
                assert(!memcmp(source,saved,sizeof(source)));
                for(unsigned i=0;i<count;++i){
                    assert(close_to(read_float(captured+i*stride),xy[i][0]>0?ew:0) && close_to(read_float(captured+i*stride+4),xy[i][1]>0?eh:0));
                    assert(!memcmp(captured+i*stride+8,source+i*stride+8,stride-8));
                }
            }
        }
        /* The saved-frame crossfade reaches the native extent; the GUI snapshot copy stays in
           GUI units (the GUI root scales it to the screen). */
        memset(source,0x22,sizeof(source));
        for(unsigned i=0;i<4;++i){unsigned char *v=source+i*24;write_float(v,full_x[i]);write_float(v+4,full_y[i]);write_float(v+8,-10);write_float(v+16,full_x[i]/1920);write_float(v+20,full_y[i]/1080);}
        test_caller=0x33c2fe;test_stride=24;invoke(source,4);
        for(unsigned i=0;i<4;++i)assert(read_float(captured+i*24)==(full_x[i]>0?w:0) && read_float(captured+i*24+4)==(full_y[i]>0?h:0));
        test_caller=0x20cf30;canvas_forward(0,source,4,4,(uintptr_t)copy_matrix,0,0);
        for(unsigned i=0;i<4;++i)assert(close_to(read_float(captured+i*24),full_x[i]) && close_to(read_float(captured+i*24+4),full_y[i]));
        /* Projected anchors: no margin to cancel. */
        const float input[]={1714,292,0,1};float out[4];
        test_project_caller=0x63a62b;fake_project(out,input);assert(out[0]==1714 && out[1]==302);
    }
    use_canvas(3);test_stride=24;
    puts("PASS: 16:9 576/720/1440/2160 native canvases: authored UI, glyphs, panels, edge fades, backgrounds, event map, GUI snapshot and anchors keep stock values; stock-pixel full-screen copies and the crossfade reach the native extent, solid fades never shrink below it; paper at stock texel scale");
}
/* Narration paper layer as logged at 3440x1440: one quad x 0..1920, y 0..1024, z -10, white,
   UV 0..0.9375 x 0..1 (1920x1024 of a 2048x1024 texture), caller 0x229a9e. */
static void test_paper_layer(void) {
    unsigned char source[4*24+16],saved[sizeof(source)];
    const float px[]={1920,1920,0,0},py[]={0,1024,0,1024};
    test_caller=0x229a9e;test_stride=24;test_offscreen=0;
    for(unsigned k=0;k<CANVASES;++k){
        use_canvas(k);
        memset(source,0xa5,sizeof(source));
        for(unsigned i=0;i<4;++i){
            unsigned char *v=source+i*24;
            write_float(v,px[i]);write_float(v+4,py[i]);write_float(v+8,-10);memset(v+12,0xff,4);
            write_float(v+16,px[i]>0?.9375f:0);write_float(v+20,py[i]>0?1:0);
        }
        memcpy(saved,source,sizeof(source));
        for(unsigned n=0;n<4;++n){
            invoke(source,4);assert(!memcmp(source,saved,sizeof(source)));
            for(unsigned i=0;i<4;++i){
                assert(close_to(read_float(captured+i*24),px[i]*canvas_stretch(k)));
                assert(!memcmp(captured+i*24+4,source+i*24+4,20));
            }
        }
        /* Physically the whole wide screen, like the paper composite beneath it. */
        assert(read_float(captured+2*24)==0 && fabsf(read_float(captured)*test_project_scale-canvas_widths[k])<0.01f);
        /* Under 1024 rows, not from the top, partial width, other callers: centred 16:9.
           Untouched on off-screen layers. */
        write_float(source+24+4,1000);write_float(source+3*24+4,1000);
        invoke(source,4);assert(close_to(read_float(captured),1920+canvas_dx(k)));
        memcpy(source,saved,sizeof(source));for(unsigned i=0;i<4;++i)write_float(source+i*24+4,py[i]+40);
        invoke(source,4);assert(close_to(read_float(captured),1920+canvas_dx(k)));
        memcpy(source,saved,sizeof(source));write_float(source,1800);write_float(source+24,1800);
        invoke(source,4);assert(close_to(read_float(captured),1800+canvas_dx(k)));
        memcpy(source,saved,sizeof(source));test_caller=0x4a4c59;
        invoke(source,4);assert(close_to(read_float(captured),1920+canvas_dx(k)));test_caller=0x229a9e;
        test_offscreen=1;invoke(source,4);assert(!memcmp(captured,source,96));test_offscreen=0;
        /* Its border below, as logged at 3840x1080: x 0..960 and 960..1920 by y 1024..1080,
           u 0.9375..0.9648 / 0.9648..0.9922, v 0..0.9375. Both halves stretch with the paper
           (together the whole width); a quad not reaching the bottom edge, or not starting
           at the paper's edge (a bottom prompt), stays in the centred 16:9 area. */
        for(unsigned half=0;half<2;++half){
            const float bx[]={960+960.f*half,960+960.f*half,960.f*half,960.f*half},by[]={1024,1080,1024,1080};
            for(unsigned i=0;i<4;++i){
                unsigned char *v=source+i*24;
                write_float(v,bx[i]);write_float(v+4,by[i]);
                write_float(v+16,.9375f+.0273f*half+(i<2?.0273f:0));write_float(v+20,i&1?.9375f:0);
            }
            memcpy(saved,source,sizeof(source));
            invoke(source,4);assert(!memcmp(source,saved,sizeof(source)));
            for(unsigned i=0;i<4;++i){
                assert(close_to(read_float(captured+i*24),bx[i]*canvas_stretch(k)));
                assert(!memcmp(captured+i*24+4,source+i*24+4,20));
            }
            write_float(source+24+4,1070);write_float(source+3*24+4,1070);
            invoke(source,4);assert(close_to(read_float(captured),bx[0]+canvas_dx(k)));
            memcpy(source,saved,sizeof(source));
            write_float(source+4,1000);write_float(source+2*24+4,1000);
            invoke(source,4);assert(close_to(read_float(captured),bx[0]+canvas_dx(k)));
            memcpy(source,saved,sizeof(source));
        }
    }
    test_project_width=1920;test_project_height=1080;test_project_scale=1;
    invoke(source,4);assert(!memcmp(captured,source,96));
    test_project_width=3840;test_project_height=1080;test_project_scale=1;
    puts("PASS: narration paper layer (1920x1024 from the top) and its 960x56 border halves below fill every wide canvas width with Y/UV/colour/depth/input preserved; short/lowered/partial/caller/off-screen/16:9 exclusions, bottom quads off the paper edge stay 16:9");
}
/* A talk-portrait panel as observed: authored x0..x0+600 by 0..1080, u 0.1875..0.8125 of the
   half-screen face target (mirrored when flip). */
static void portrait_quad(unsigned char *source,float x0,int flip) {
    const float px[]={600,600,0,0},py[]={0,1080,0,1080};
    memset(source,0xa5,4*24+16);
    for(unsigned i=0;i<4;++i){
        unsigned char *v=source+i*24;
        write_float(v,x0+px[i]);write_float(v+4,py[i]);write_float(v+8,-10);memset(v+12,0xff,4);
        write_float(v+16,(px[i]>0)!=flip?.8125f:.1875f);write_float(v+20,py[i]>0?1:0);
    }
}
static void test_talk_portrait(void) {
    unsigned char source[4*24+16],saved[sizeof(source)];
    test_caller=0x229a9e;test_stride=24;test_offscreen=0;
    for(unsigned k=0;k<CANVASES;++k){
        use_canvas(k);
        /* The face target is half the screen wide: the panel's 600 units show 600 * scale
           texels of it, 1:1 as at 1080p, centred on the target. */
        const float target=canvas_widths[k]/2,texels=600*test_project_scale;
        for(unsigned side=0;side<2;++side)for(int flip=0;flip<2;++flip){
            const float x0=side?1230:90;
            portrait_quad(source,x0,flip);memcpy(saved,source,sizeof(source));
            invoke(source,4);assert(!memcmp(source,saved,sizeof(source)));
            for(unsigned i=0;i<4;++i){
                const unsigned char *in=source+i*24,*out=captured+i*24;
                assert(close_to(read_float(out),read_float(in)+canvas_dx(k)));
                assert(!memcmp(out+4,in+4,12) && !memcmp(out+20,in+20,4));
                const float u=read_float(in+16),expected=.5f+(u-.5f)*texels/(target*.625f);
                assert(fabsf(read_float(out+16)-expected)<0.00001f);
            }
            const float u0=read_float(captured+16),u2=read_float(captured+2*24+16);
            assert(fabsf(fabsf(u0-u2)*target-texels)<0.01f && fabsf(u0+u2-1)<0.00001f);
        }
        /* Kept as authored: the frame atlas pieces drawn over the panel (observed UVs), other
           proportions, a corner off the portrait UVs, other callers, the solid submit's
           layout, off-screen layers. */
        portrait_quad(source,90,0);
        for(unsigned i=0;i<4;++i)write_float(source+i*24+16,read_float(source+i*24+16)>.5f?.8555f:.2676f);
        invoke(source,4);for(unsigned i=0;i<4;++i)assert(read_float(captured+i*24+16)==read_float(source+i*24+16));
        portrait_quad(source,90,0);write_float(source,990);write_float(source+24,990);
        invoke(source,4);for(unsigned i=0;i<4;++i)assert(read_float(captured+i*24+16)==read_float(source+i*24+16));
        portrait_quad(source,90,0);write_float(source+20,.5f);
        invoke(source,4);for(unsigned i=0;i<4;++i)assert(read_float(captured+i*24+16)==read_float(source+i*24+16));
        portrait_quad(source,90,0);test_caller=0x4a4c59;
        invoke(source,4);for(unsigned i=0;i<4;++i)assert(read_float(captured+i*24+16)==read_float(source+i*24+16));
        test_caller=0x229a9e;test_offscreen=1;invoke(source,4);assert(!memcmp(captured,source,96));test_offscreen=0;
    }
    /* 16:9 canvases (target 960 * scale): stock UVs already show the 1080p framing. */
    const float standard[][2]={{1024,576},{1280,720},{1920,1080},{2560,1440},{3840,2160}};
    for(unsigned s=0;s<5;++s){
        test_project_width=standard[s][0];test_project_height=standard[s][1];test_project_scale=standard[s][1]/1080.f;
        portrait_quad(source,90,0);invoke(source,4);
        for(unsigned i=0;i<4;++i)assert(read_float(captured+i*24+16)==read_float(source+i*24+16));
    }
    use_canvas(3);
    puts("PASS: talk portraits sample their half-screen face target 1:1 and centred on every wide canvas (both panels, mirrored UVs), positions shifted as authored UI, V/colour/depth/input preserved; atlas frame/proportion/UV/caller/off-screen exclusions; 16:9 sizes untouched");
}
int main(void) {
    test_projection();
    size_t page=(size_t)sysconf(_SC_PAGESIZE);uintptr_t start=(uintptr_t)fake_guest&~(page-1);
    assert(!mprotect((void*)start,2*page,PROT_READ|PROT_WRITE|PROT_EXEC));
    assert(!install((unsigned char*)(uintptr_t)fake_guest,0));
    uintptr_t color_page=(uintptr_t)fake_color_guest&~(page-1);
    assert(!mprotect((void*)color_page,2*page,PROT_READ|PROT_WRITE|PROT_EXEC));
    assert(!install_color((unsigned char*)(uintptr_t)fake_color_guest));
    assert(!mprotect((void*)color_page,2*page,PROT_READ|PROT_EXEC));
    assert(!mprotect((void*)start,2*page,PROT_READ|PROT_EXEC));
    unsigned char source[114*24+16],saved[sizeof(source)];memset(source,0xa5,sizeof(source));
    for(unsigned i=0;i<114;++i) {write_float(source+i*24,(float)i);write_float(source+i*24+4,100);}
    memcpy(saved,source,sizeof(source));test_caller=0x4a4c59;
    for(unsigned run=0;run<8;++run) {
        invoke(source,113);assert(!memcmp(source,saved,sizeof(source)));
        for(unsigned i=0;i<113;++i) {assert(read_float(captured+i*24)==960+(float)i);assert(!memcmp(captured+i*24+4,source+i*24+4,20));}
    }
    test_caller=0x1e451c;invoke(source,113);assert(!memcmp(captured,source,(113*24+15)&~15));
    const float xs[]={1922,1922,0,0},ys[]={0,1090,0,1090};
    for(unsigned i=0;i<4;++i) {write_float(source+i*24,xs[i]);write_float(source+i*24+4,ys[i]);}
    memcpy(saved,source,sizeof(source));test_caller=0x229a9e;invoke(source,4);
    assert(!memcmp(source,saved,sizeof(source)));
    for(unsigned i=0;i<4;++i) {assert(read_float(captured+i*24)==xs[i]*2);assert(!memcmp(captured+i*24+4,source+i*24+4,20));}
    write_float(source,NAN);invoke(source,4);assert(!memcmp(source,captured,96));
    test_caller=0x2297d7;
    const float color_x[]={1920,1920,0,0},color_y[]={0,300,0,300};
    for(unsigned i=0;i<4;++i) {write_float(source+i*16,color_x[i]);write_float(source+i*16+4,color_y[i]);}
    memcpy(saved,source,sizeof(source));invoke_color(source,4);assert(!memcmp(source,saved,sizeof(source)));
    for(unsigned i=0;i<4;++i) {assert(read_float(captured+i*16)==color_x[i]*2);assert(!memcmp(captured+i*16+4,source+i*16+4,12));}
    const float edge_x[]={575,284,-11,-302},edge_y[]={-144,1224,-269,1099};
    for(unsigned i=0;i<4;++i) {write_float(source+i*16,edge_x[i]);write_float(source+i*16+4,edge_y[i]);}
    memcpy(saved,source,sizeof(source));invoke_color(source,4);assert(!memcmp(source,saved,sizeof(source)));
    for(unsigned i=0;i<4;++i) {assert(read_float(captured+i*16)==(edge_x[i]<0?edge_x[i]:edge_x[i]+960));assert(!memcmp(captured+i*16+4,source+i*16+4,12));}
    test_caller=0x12345;invoke_color(source,4);assert(!memcmp(source,captured,64));
    for(unsigned k=0;k<CANVASES;++k){
        use_canvas(k);
        const float scale=test_project_scale,margin=(canvas_widths[k]-1920*scale)/2;
        test_caller=0x4a4c59;test_stride=24;
        for(unsigned i=0;i<4;++i){write_float(source+i*24,960);write_float(source+i*24+4,540);}
        memcpy(saved,source,sizeof(source));
        for(unsigned n=0;n<8;++n){invoke(source,4);assert(!memcmp(source,saved,sizeof(source)));for(unsigned i=0;i<4;++i){assert(fabsf(read_float(captured+i*24)*scale-canvas_widths[k]/2)<0.01f);assert(fabsf(read_float(captured+i*24+4)*scale-canvas_heights[k]/2)<0.001f);assert(!memcmp(captured+i*24+4,source+i*24+4,20));}}
        for(unsigned i=0;i<4;++i){write_float(source+i*24,xs[i]);write_float(source+i*24+4,ys[i]);}
        memcpy(saved,source,sizeof(source));test_caller=0x229a9e;invoke(source,4);assert(!memcmp(source,saved,sizeof(source)));
        for(unsigned i=0;i<4;++i){assert(fabsf(read_float(captured+i*24)*scale-xs[i]*canvas_widths[k]/1920)<0.01f);assert(!memcmp(captured+i*24+4,source+i*24+4,20));}
        for(unsigned i=0;i<4;++i){write_float(source+i*16,color_x[i]);write_float(source+i*16+4,color_y[i]);}
        memcpy(saved,source,sizeof(source));test_caller=0x2297d7;invoke_color(source,4);assert(!memcmp(source,saved,sizeof(source)));
        for(unsigned i=0;i<4;++i){assert(fabsf(read_float(captured+i*16)*scale-color_x[i]*canvas_widths[k]/1920)<0.01f);assert(!memcmp(captured+i*16+4,source+i*16+4,12));}
        for(unsigned i=0;i<4;++i){write_float(source+i*16,edge_x[i]);write_float(source+i*16+4,edge_y[i]);}
        memcpy(saved,source,sizeof(source));invoke_color(source,4);assert(!memcmp(source,saved,sizeof(source)));
        for(unsigned i=0;i<4;++i){assert(fabsf(read_float(captured+i*16)*scale-(edge_x[i]*scale+(edge_x[i]<0?0:margin)))<0.01f);assert(!memcmp(captured+i*16+4,source+i*16+4,12));}
        test_project_scale=scale*1.5f;invoke_color(source,4);assert(!memcmp(source,captured,64));
    }
    use_canvas(3);
    puts("PASS: scale-aware glyph/panel offsets, full-screen fills and edge shading on every wide canvas; input preservation; post-shader physical centring and wrong-scale fallback");
    const float fade_x[]={0,0,1920,1920},fade_y[]={0,1080,0,1080};
    const float *widths=canvas_widths,*heights=canvas_heights;
    test_caller=0x33c2fe;test_stride=24;
    for(unsigned mode=0;mode<CANVASES;++mode){
        test_project_width=widths[mode];test_project_height=heights[mode];test_project_scale=heights[mode]/1080.f;
        for(unsigned i=0;i<4;++i){write_float(source+i*24,fade_x[i]);write_float(source+i*24+4,fade_y[i]);}
        memcpy(saved,source,sizeof(source));
        invoke(source,4);assert(!memcmp(source,saved,sizeof(source)));
        for(unsigned i=0;i<4;++i){assert(read_float(captured+i*24)==(fade_x[i]>0?widths[mode]:0));assert(read_float(captured+i*24+4)==(fade_y[i]>0?heights[mode]:0));assert(!memcmp(captured+i*24+8,source+i*24+8,16));}
        write_float(source,12);invoke(source,4);assert(!memcmp(source,captured,96));
    }
    puts("PASS: saved-frame crossfade full native extent on every wide canvas; UV/opacity/depth/input preserved; non-fullscreen exclusion");
    const float pan_x[]={1920,1920,0,0},pan_y[]={-186.63724f,1861.3628f,-186.63724f,1861.3628f};
    for(unsigned mode=0;mode<CANVASES;++mode){
        test_project_width=widths[mode];test_project_height=heights[mode];test_project_scale=heights[mode]/1080.f;
        for(unsigned i=0;i<4;++i){write_float(source+i*24,pan_x[i]);write_float(source+i*24+4,pan_y[i]);}
        memcpy(saved,source,sizeof(source));test_caller=0x229a9e;
        for(unsigned n=0;n<8;++n){invoke(source,4);assert(!memcmp(source,saved,sizeof(source)));
            for(unsigned i=0;i<4;++i){assert(close_to(read_float(captured+i*24),pan_x[i]*canvas_stretch(mode)));assert(!memcmp(captured+i*24+4,source+i*24+4,20));}}
        /* The displayed edges fill the complete wide canvas after GUI scale. */
        assert(fabsf(read_float(captured)*test_project_scale-widths[mode])<0.001f);
        test_caller=0x4a4c59;invoke(source,4);assert(close_to(read_float(captured),pan_x[0]+canvas_dx(mode)));
        test_caller=0x229a9e;
        for(unsigned i=0;i<4;++i)write_float(source+i*24,pan_x[i]/2);
        invoke(source,4);assert(close_to(read_float(captured),960+canvas_dx(mode))); /* book-size page remains centred, not widened */
        for(unsigned i=0;i<4;++i){write_float(source+i*24,pan_x[i]);write_float(source+i*24+4,pan_y[i]+500);}
        invoke(source,4);assert(close_to(read_float(captured),1920+canvas_dx(mode))); /* does not cover top edge */
    }
    puts("PASS: panning illustrated backdrops fill every wide canvas width; Y/UV/opacity/source preserved; glyph/page/partial-panel exclusions");
    const float dim_x[]={1933,1933,-13,-13},dim_y[]={-19,1107,-19,1107};
    for(unsigned mode=0;mode<CANVASES;++mode){
        test_project_width=widths[mode];test_project_height=heights[mode];test_project_scale=heights[mode]/1080.f;
        test_caller=0x229a9e;test_stride=24;
        for(unsigned i=0;i<4;++i){write_float(source+i*24,dim_x[i]);write_float(source+i*24+4,dim_y[i]);}
        memcpy(saved,source,sizeof(source));
        for(unsigned n=0;n<4;++n){invoke(source,4);assert(!memcmp(source,saved,sizeof(source)));
            for(unsigned i=0;i<4;++i){assert(close_to(read_float(captured+i*24),dim_x[i]*canvas_stretch(mode)));assert(!memcmp(captured+i*24+4,source+i*24+4,20));}}
        /* Physical extent covers the full wide screen, centred like the authored layer. */
        assert(read_float(captured+2*24)*test_project_scale<=0 && read_float(captured)*test_project_scale>=widths[mode]);
        /* Larger offsets, partial height, or other callers keep the centred canvas. */
        write_float(source+2*24,-40);write_float(source+3*24,-40);invoke(source,4);assert(close_to(read_float(captured),1933+canvas_dx(mode)));
        memcpy(source,saved,sizeof(source));write_float(source+24+4,900);write_float(source+3*24+4,900);invoke(source,4);assert(close_to(read_float(captured),1933+canvas_dx(mode)));
        memcpy(source,saved,sizeof(source));test_caller=0x4a4c59;invoke(source,4);assert(close_to(read_float(captured),1933+canvas_dx(mode)));
        memcpy(source,saved,sizeof(source));
    }
    test_project_width=3840;test_project_height=1080;test_project_scale=1;
    puts("PASS: overscanned full-screen dim layer widens to the whole native width on every wide canvas; larger offsets, partial height and other callers stay centred");
    test_wide_canvas();
    test_screen_rect_sweep();
    test_gui_snapshot();
    test_rotated_glyph_translation();
    test_offscreen_layer();
    test_cursor_site();
    test_event_map();
    test_centred_pulse();
    test_paper_site();
    test_layer_site();
    test_standard_canvas();
    test_screen_edge_fade();
    test_paper_layer();
    test_talk_portrait();
    puts("PASS: both native trampoline ABIs/all arguments, relocated colour global load, persistent input preservation, repeated glyphs without drift, scene-copy exclusion, full-width backgrounds/gradient, outer-edge shading, invalid-coordinate fallback");
    return 0;
}
