/* Diagnostic-only backend. Unavailable guest imports stay unresolved so the
 * loader reports the first real compatibility gap instead of faking success. */
#include "runtime.h"
#include "gpu/ps4rtgpu.h"
#include <stdio.h>

#define UNAVAILABLE(service) \
    uintptr_t runtime_##service##_resolve(const char *name) { (void)name; return 0; }
#define REPORT(service) \
    void runtime_##service##_report(void) { puts("Headless: " #service " unavailable"); }
UNAVAILABLE(ajm)
UNAVAILABLE(audio)
UNAVAILABLE(pad)
UNAVAILABLE(videodec)
REPORT(ajm)
REPORT(audio)
REPORT(pad)
int runtime_pad_describe(void) { puts("Gamepad: unavailable (headless build)"); return 0; }
void ps4rtgpu_register_kernel(void) {}
uintptr_t ps4rtgpu_resolve(const char *name) { (void)name; return 0; }
int ps4rtgpu_init(const Ps4RtGpuConfig *config) {
    (void)config;
    fputs("Headless build requires --cpu-only; graphics are unavailable\n", stderr);
    return -1;
}
unsigned ps4rtgpu_symbol_count(void) { return 0; }
int ps4rtgpu_handle_fault(void *context, void *address) { (void)context; (void)address; return 0; }
void ps4rtgpu_dump_guest_writes(void *context) { (void)context; }
int ps4rtgpu_text_input_begin(const char *text, const char *prompt) { (void)text; (void)prompt; return 0; }
int ps4rtgpu_text_input_poll(char *text, uint64_t size) { (void)text; (void)size; return 2; }
int ps4rtgpu_choice_begin(const char *title, const char *const *items, const char *const *images, int count, int focus) {
    (void)title; (void)items; (void)images; (void)count; (void)focus; return 0;
}
int ps4rtgpu_choice_poll(void) { return -2; }
int vulkan_smoke(void) { fputs("Vulkan unavailable in headless build\n", stderr); return 1; }
