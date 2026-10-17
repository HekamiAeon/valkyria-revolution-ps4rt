// Exercise the same event-flag implementation used by the native GPU/runtime bridge.
#undef NDEBUG
#include "../gpu/shadps4/core/libraries/kernel/event_flag.cpp"
#include <cassert>
#include <cstdlib>
#include <iostream>

static thread_local int test_priority=700;
extern "C" int32_t runtime_thread_current_priority() { return test_priority; }
void assert_fail_impl() { std::abort(); }
[[noreturn]] void unreachable_impl() { std::abort(); }
namespace Common::Log {
bool ShouldLog(Level) { return false; }
void Write(std::string_view, Level, const char*, int, const char*, std::string&&) {}
}
void Core::Loader::SymbolsResolver::AddSymbol(const char*,const char*,const char*,SymbolType,u64) {}

using namespace Libraries::Kernel;
using namespace std::chrono_literals;
static void queued(OrbisKernelEventFlag flag,size_t count) {
    const auto end=std::chrono::steady_clock::now()+2s;
    while (flag->PendingWaiters()!=count) {
        assert(std::chrono::steady_clock::now()<end);
        std::this_thread::yield();
    }
}
int main() {
    OrbisKernelEventFlag flag{};
    assert(sceKernelCreateEventFlag(nullptr,"test",0,0,nullptr)==ORBIS_KERNEL_ERROR_EINVAL);
    assert(sceKernelCreateEventFlag(&flag,"test",3,0,nullptr)==ORBIS_KERNEL_ERROR_EINVAL);
    assert(sceKernelCreateEventFlag(&flag,"test",0x21,0x15,nullptr)==0);
    struct { u64 pattern, canary; } result{0,0x123456789abcdef0};
    assert(sceKernelPollEventFlag(flag,5,0x21,&result.pattern)==0 && result.pattern==0x15);
    assert(result.canary==0x123456789abcdef0);
    assert(sceKernelPollEventFlag(flag,0x10,1,&result.pattern)==0 && result.pattern==0x10);
    assert(sceKernelClearEventFlag(flag,~u64{0x10})==0);
    assert(sceKernelPollEventFlag(flag,1,2,&result.pattern)==ORBIS_KERNEL_ERROR_EBUSY);
    assert(sceKernelPollEventFlag(flag,0,1,nullptr)==ORBIS_KERNEL_ERROR_EINVAL);
    assert(sceKernelPollEventFlag(flag,1,0,nullptr)==ORBIS_KERNEL_ERROR_EINVAL);
    assert(sceKernelPollEventFlag(flag,1,0x31,nullptr)==ORBIS_KERNEL_ERROR_EINVAL);
    u32 timeout=1000;
    assert(sceKernelWaitEventFlag(flag,1,1,&result.pattern,&timeout)==ORBIS_KERNEL_ERROR_ETIMEDOUT);
    assert(timeout==0 && result.pattern==0 && result.canary==0x123456789abcdef0);

    // Two disjoint clear-on-match waiters must both complete from a single set.
    int a=-1,b=-1;
    u64 a_result=0,b_result=0;
    std::thread first([&] { a=sceKernelWaitEventFlag(flag,1,0x21,&a_result,nullptr); });
    std::thread second([&] { b=sceKernelWaitEventFlag(flag,2,0x21,&b_result,nullptr); });
    queued(flag,2);
    assert(sceKernelSetEventFlag(flag,3)==0);
    first.join(); second.join();
    assert(a==0 && b==0 && (a_result&1) && (b_result&2));
    assert(sceKernelPollEventFlag(flag,3,2,&result.pattern)==ORBIS_KERNEL_ERROR_EBUSY);

    std::thread canceled([&] { a=sceKernelWaitEventFlag(flag,4,1,&a_result,nullptr); });
    queued(flag,1);
    int count=-1;
    assert(sceKernelCancelEventFlag(flag,8,&count)==0 && count==1);
    canceled.join();
    assert(a==ORBIS_KERNEL_ERROR_ECANCELED && a_result==8);
    assert(sceKernelPollEventFlag(flag,8,0x11,nullptr)==0);

    std::thread deleted([&] { a=sceKernelWaitEventFlag(flag,4,1,nullptr,nullptr); });
    queued(flag,1);
    assert(sceKernelDeleteEventFlag(flag)==0);
    deleted.join();
    assert(a==ORBIS_KERNEL_ERROR_EACCES);

    // Lower numeric PS4 priority must win a consuming wait, regardless of FIFO order.
    assert(sceKernelCreateEventFlag(&flag,"priority",0x22,0,nullptr)==0);
    std::thread low([&] { test_priority=900; b=sceKernelWaitEventFlag(flag,1,0x21,nullptr,nullptr); });
    queued(flag,1);
    std::thread high([&] { test_priority=300; a=sceKernelWaitEventFlag(flag,1,0x21,nullptr,nullptr); });
    queued(flag,2);
    assert(sceKernelSetEventFlag(flag,1)==0);
    high.join();
    assert(a==0);
    queued(flag,1);
    assert(sceKernelCancelEventFlag(flag,0,nullptr)==0);
    low.join();
    assert(b==ORBIS_KERNEL_ERROR_ECANCELED);
    assert(sceKernelDeleteEventFlag(flag)==0);
    std::cout << "PASS: event-flag ABI, clearing, timeouts, concurrent waiters, cancel/delete and priority\n";
}
