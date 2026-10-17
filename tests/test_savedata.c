/* Persistent metadata roundtrip on an isolated temporary save directory. */
#undef NDEBUG
#include "../src/runtime_savedata.c"
#include <assert.h>

static char test_user[]="/tmp/valkyria-savedata-XXXXXX";
static char mounted_host[700];
const char *runtime_file_user_dir(void) { return test_user; }
int runtime_file_mount(const char *guest,const char *host) {
    assert(!strcmp(guest,"/savedata0"));
    snprintf(mounted_host,sizeof(mounted_host),"%s",host);
    return 0;
}
void runtime_file_unmount(const char *guest) { assert(!strcmp(guest,"/savedata0")); }
uintptr_t runtime_lookup(const RuntimeExport *table,size_t count,const char *nid) {
    (void)table; (void)count; (void)nid; return 0;
}
int main(void) {
    assert(mkdtemp(test_user));
    struct { float value; uint32_t canary; } progress={-1,0x12345678};
    assert(save_clear_progress()==ERR_NOT_INITIALIZED);
    assert(save_get_progress(&progress.value)==ERR_NOT_INITIALIZED && progress.value==-1);
    assert(save_initialize(NULL)==0 && save_clear_progress()==0);
    assert(save_get_progress(NULL)==ERR_PARAMETER);
    assert(save_get_progress(&progress.value)==0 && progress.value==0);
    assert(progress.canary==0x12345678);

    runtime_savedata_configure("CUSA06810");
    DirName name={"PORTTEST"};
    Mount2 request={.user=1,.dir=&name,.blocks=96,.mode=MODE_CREATE2};
    MountResult mounted;
    assert(save_mount2(&request,&mounted)==0 && mounted.status==1);
    assert(strstr(mounted_host,test_user)==mounted_host);
    const char title[]="Valkyria roundtrip";
    uint32_t marker=0xaabbccdd;
    assert(save_set_param(&mounted.point,1,title,sizeof(title))==0);
    assert(save_set_param(&mounted.point,4,&marker,sizeof(marker))==0);
    struct { char text[8]; uint64_t canary; } short_title={{0},0x123456789abcdef0};
    uint64_t got=99;
    assert(save_get_param(&mounted.point,1,short_title.text,sizeof(short_title.text),&got)==0);
    assert(got==8 && !strcmp(short_title.text,"Valkyri") && short_title.canary==0x123456789abcdef0);
    struct { Param param; uint64_t canary; } all={.canary=0xfedcba9876543210};
    assert(save_get_param(&mounted.point,0,&all.param,sizeof(all.param),&got)==0);
    assert(got==sizeof(Param) && !strcmp(all.param.title,title) && all.param.user_param==marker);
    assert(all.canary==UINT64_C(0xfedcba9876543210));
    got=99;
    assert(save_get_param(&mounted.point,0,&all.param,1,&got)==ERR_PARAMETER && got==99);
    assert(save_get_param(&mounted.point,1,short_title.text,0,NULL)==ERR_PARAMETER);
    assert(save_get_param(&mounted.point,6,short_title.text,8,NULL)==ERR_PARAMETER);
    assert(save_umount(&mounted.point)==0);
    assert(save_get_param(&mounted.point,0,&all.param,sizeof(all.param),NULL)==ERR_NOT_MOUNTED);
    request.mode=0;
    assert(save_mount2(&request,&mounted)==0 && mounted.status==0);
    uint32_t read_marker=0;
    assert(save_get_param(&mounted.point,4,&read_marker,sizeof(read_marker),&got)==0);
    assert(read_marker==marker && got==4);
    int64_t mtime=0;
    assert(save_get_param(&mounted.point,5,&mtime,sizeof(mtime),&got)==0 && mtime>0 && got==8);
    assert(save_umount(&mounted.point)==0 && save_terminate()==0);
    assert(save_get_progress(&progress.value)==ERR_NOT_INITIALIZED);
    remove_tree(test_user);
    puts("PASS: SaveData progress bounds and persistent metadata roundtrip");
}
