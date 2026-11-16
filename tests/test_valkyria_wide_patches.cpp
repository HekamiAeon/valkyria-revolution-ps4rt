// SPDX-License-Identifier: GPL-2.0-or-later
// Valkyria wide canvas startup patches on a synthetic image (no game data): supported sizes,
// all-or-nothing verification, and the two padding stubs executed. Optional argument: a
// decrypted eboot.elf, checked read-only to match every patch's original bytes.
#include <sys/mman.h>

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "gpu/shim/valkyria_wide_patches.h"

using namespace ValkyriaWidePatches;

namespace {

constexpr size_t ImageSize = 0xa14000;

struct Image {
    uint8_t* data = static_cast<uint8_t*>(mmap(nullptr, ImageSize, PROT_READ | PROT_WRITE | PROT_EXEC,
                                               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    Image() { assert(data != MAP_FAILED); }
    ~Image() { munmap(data, ImageSize); }
    uintptr_t base() const { return reinterpret_cast<uintptr_t>(data); }
    float& f(uint64_t at) { return *reinterpret_cast<float*>(data + at); }
    uint32_t& u(uint64_t at) { return *reinterpret_cast<uint32_t*>(data + at); }
    void put(uint64_t at, std::initializer_list<uint8_t> bytes) {
        std::copy(bytes.begin(), bytes.end(), data + at);
    }
};

/// An image with every patch's original bytes (as the game build has them).
void Stock(Image& image, int width, int height) {
    for (const Write& write : Writes(width, height))
        std::copy(write.before.begin(), write.before.end(), image.data + write.at);
}

/// Stand-ins around the two rel32 calls: each call returns into a ret; FUN_003d5d60 returns,
/// FUN_001ce860 returns (ecx << 32 | edx).
void Callees(Image& image) {
    image.put(0x3d669e, {0xc3});
    image.put(0x3d5d60, {0xc3});
    image.put(0x1ce852, {0xc3});
    image.put(0x1ce860, {0x89, 0xd0, 0x48, 0xc1, 0xe1, 0x20, 0x48, 0x09, 0xc8, 0xc3});
}

/// The UI hooks' original prologues (valkyria_wide_ui.c).
void Prologues(Image& image) {
    image.put(0x764860, {0x55, 0x48, 0x89, 0xe5, 0x41, 0x57, 0x41, 0x56, 0x41, 0x55, 0x41, 0x54, 0x53, 0x50});
    image.put(0x765750, {0x41, 0x57, 0x41, 0x56, 0x53, 0x4c, 0x8b, 0x0d, 0xfc, 0x93, 0x2c, 0, 0x41, 0x89, 0xd2});
    image.put(0x235ce0, {0x55, 0x48, 0x89, 0xe5, 0x41, 0x57, 0x41, 0x56, 0x41, 0x55, 0x41, 0x54, 0x53,
                         0x48, 0x83, 0xec, 0x18});
}

void SceneTemplate(Image& image) {
    image.f(0xa12f54) = 1920, image.f(0xa12f58) = 1080, image.f(0xa12f64) = 1920;
    image.f(0xa12f68) = 1080, image.f(0xa12f70) = 960, image.f(0xa12f74) = 540;
    image.f(0x968c80) = 1, image.f(0x968c84) = 1;
    image.u(0xa1279c) = 0x103;
}

void RunScene(Image& image) {
    reinterpret_cast<void (*)()>(image.data + 0x3d6699)();
}
uint64_t RunColour(Image& image, uint32_t width, uint32_t height) {
    return reinterpret_cast<uint64_t (*)(uint64_t, uint64_t, uint64_t, uint64_t)>(
        image.data + 0x1ce84d)(0, 0, width, height);
}

void TestSizes() {
    for (auto [w, h] : {std::pair{2560, 1080}, {3440, 1440}, {3840, 1440}, {3840, 1080},
                        {5120, 1440}})
        assert(Wide(w, h) && Supported(w, h) && !Writes(w, h).empty());
    for (auto [w, h] : {std::pair{1024, 576}, {1280, 720}, {2560, 1440}, {3840, 2160}})
        assert(!Wide(w, h) && Supported(w, h) && !Writes(w, h).empty());
    for (auto [w, h] : {std::pair{1920, 1080}, {1600, 900}, {5120, 2880}, {5120, 1080},
                        {2564, 1080}, {2560, 1200}, {0, 1080}, {-2560, 1080}})
        assert(!Supported(w, h) && Writes(w, h).empty());
    // Every write keeps its size and no two writes overlap.
    const auto writes = Writes(5120, 1440);
    for (size_t i = 0; i < writes.size(); ++i) {
        assert(!writes[i].before.empty() && writes[i].before.size() == writes[i].after.size());
        for (size_t j = 0; j < i; ++j)
            assert(writes[i].at + writes[i].before.size() <= writes[j].at ||
                   writes[j].at + writes[j].before.size() <= writes[i].at ||
                   (writes[i].before == writes[i].after && writes[j].before == writes[j].after));
        if (writes[i].at >= 0x8b3f48 && writes[i].at < 0x8b4000)
            assert(writes[i].at + writes[i].after.size() <= 0x8b4000);
    }
    std::puts("PASS: wide sizes 1080/1440 high up to 32:9 (width multiple of 8) and 16:9 "
              "576/720/1440/2160; 1080p and other sizes rejected; writes sized, disjoint, stubs "
              "inside the code page");
}

void TestStaticWrites() {
    for (auto [w, h] : {std::pair{3440, 1440}, {2560, 1080}, {1280, 720}, {3840, 2160}}) {
        Image image;
        Stock(image, w, h);
        std::string problem;
        const auto writes = Writes(w, h);
        assert(Matches(image.base(), writes, &problem));
        Apply(image.base(), writes);
        // The same operands as the verified debugger launch.
        for (uint64_t at : {0x20a5ef, 0x20a65c, 0x20c391, 0x20c79c}) assert(image.u(at) == unsigned(w));
        for (uint64_t at : {0x20a5f4, 0x20a661, 0x20c396, 0x20c7a1}) assert(image.u(at) == unsigned(h));
        assert(image.f(0x20986d) == w && image.f(0x209871) == h);
        for (uint64_t at : {0x7b3a30, 0x7b3a40})
            assert(image.f(at) == 0 && image.f(at + 4) == 0 && image.f(at + 8) == w &&
                   image.f(at + 12) == h);
        assert(image.data[0x6b19f8] == 0x73 && image.data[0x6b19f9] == 0x9a);
        const uint8_t blur[] = {0x48, 0x8b, 0x0d, 0x8d, 0xdb, 0x63, 0x00};
        assert(!std::memcmp(image.data + 0x1bddbc, blur, sizeof(blur)));
        assert(0x1bddc3 + 0x63db8d == 0x7fb950);
        // Event radial-blur centre keys: scaled by the 1920x1080 template, not the live size.
        assert(!std::memcmp(image.data + 0x1a14ea, "\xc5\xfa\x10\x0d", 4) &&
               0x1a14f2 + int32_t(image.u(0x1a14ee)) == 0x7f0ff4);
        assert(!std::memcmp(image.data + 0x1a14f9, "\xc5\xfa\x10\x05", 4) &&
               0x1a1501 + int32_t(image.u(0x1a14fd)) == 0x7f0ff8);
        assert(image.u(0x74fdf7) == unsigned(w) && image.u(0x74fdfb) == unsigned(h) &&
               image.u(0x74fe07) == unsigned(w));
        // Screen fade: its struct load becomes lea rcx,[FadeExtent], which holds the screen in
        // GUI units (W * 1080 / H by 1080): the stock 1920x1080 at every 16:9 size.
        assert(!std::memcmp(image.data + 0x20b9c9, "\x48\x8d\x0d", 3) &&
               0x20b9d0 + int32_t(image.u(0x20b9cc)) == 0x8b3ff8);
        assert(image.f(0x8b3ff8) == float(1080.0 * w / h) && image.f(0x8b3ffc) == 1080);
        if (w * 9 == h * 16) assert(image.f(0x8b3ff8) == 1920);
        // A second application finds patched bytes and refuses.
        assert(!Matches(image.base(), writes, &problem) && !problem.empty());
    }
    std::puts("PASS: menu/transition snapshots, blur viewport, copy rectangles, map branch, talk "
              "faces, radial blur (centre keys included), screen fade extent and display defaults equal the verified debugger values");
}

void TestStubs() {
    for (auto [w, h] : {std::pair{3440, 1440}, {5120, 1440}, {2560, 1080}, {3840, 1080},
                        {1024, 576}, {1280, 720}, {2560, 1440}, {3840, 2160}}) {
        Image image;
        Stock(image, w, h);
        Callees(image);
        // Stock code: the scene globals stay 1920x1080, the colour extent is passed through.
        SceneTemplate(image);
        RunScene(image);
        assert(image.f(0xa12f54) == 1920 && image.u(0xa1279c) == 0x103);
        assert(RunColour(image, 1920, 1080) == (1080ull << 32 | 1920));
        const auto writes = Writes(w, h);
        Apply(image.base(), writes);
        RunScene(image);
        const float gui = h / 1080.f;
        assert(image.f(0xa12f54) == w && image.f(0xa12f58) == h && image.f(0xa12f64) == w &&
               image.f(0xa12f68) == h && image.f(0xa12f70) == w / 2.f &&
               image.f(0xa12f74) == h / 2.f && image.f(0x968c80) == gui &&
               image.f(0x968c84) == gui && image.u(0xa1279c) == 0x102);
        // An extent that is not the stock template is left alone.
        SceneTemplate(image);
        image.f(0xa12f54) = 1280;
        RunScene(image);
        assert(image.f(0xa12f54) == 1280 && image.f(0xa12f58) == 1080 &&
               image.f(0x968c80) == 1 && image.u(0xa1279c) == 0x103);
        SceneTemplate(image);
        image.f(0xa12f58) = 720;
        RunScene(image);
        assert(image.f(0xa12f54) == 1920 && image.f(0xa12f58) == 720 && image.u(0xa1279c) == 0x103);
        // The scene colour object: stock 1920x1080 becomes the canvas; anything else passes.
        assert(RunColour(image, 1920, 1080) == (uint64_t(h) << 32 | unsigned(w)));
        assert(RunColour(image, 1920, 720) == (720ull << 32 | 1920));
        assert(RunColour(image, 1280, 1080) == (1080ull << 32 | 1280));
        assert(RunColour(image, 960, 540) == (540ull << 32 | 960));
    }
    std::puts("PASS: scene stub sets extent/half/GUI scale and clears the Pro flag only on the "
              "stock template; colour stub resizes only the stock extent; both reach the game "
              "functions");
}

void TestFailClosed() {
    Image image;
    Stock(image, 3440, 1440);
    const auto writes = Writes(3440, 1440);
    std::vector<uint8_t> before(image.data, image.data + ImageSize);
    std::string problem;
    // One byte of one site differs: nothing is written.
    for (size_t index : {size_t{0}, writes.size() / 2, writes.size() - 1}) {
        image.data[writes[index].at] ^= 1;
        assert(!Matches(image.base(), writes, &problem) && problem.find("0x") != std::string::npos);
        assert(!Start(image.base(), 3440, 1440, &problem));
        image.data[writes[index].at] ^= 1;
    }
    // Patch sites match but the UI hooks' signatures are absent.
    assert(!Start(image.base(), 3440, 1440, &problem) && problem == "UI hook signatures differ");
    assert(!Start(image.base(), 1920, 1080, &problem) && problem == "unsupported size");
    assert(std::equal(before.begin(), before.end(), image.data));
    // Unreadable guest memory fails without faulting.
    void* hole = mmap(nullptr, ImageSize, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert(hole != MAP_FAILED);
    assert(!Matches(reinterpret_cast<uintptr_t>(hole), writes, &problem));
    munmap(hole, ImageSize);
    std::puts("PASS: any mismatched byte, missing UI signature, unsupported size or unreadable "
              "image leaves the game unchanged");
}

void TestStart() {
    Image image;
    Stock(image, 3840, 1080);
    Callees(image);
    Prologues(image);
    std::string problem;
    assert(Start(image.base(), 3840, 1080, &problem));
    assert(image.data[0x764860] == 0x48 && image.data[0x765750] == 0x48 && image.data[0x235ce0] == 0x48);
    SceneTemplate(image);
    RunScene(image);
    assert(image.f(0xa12f54) == 3840 && image.f(0xa12f58) == 1080 && image.f(0x968c80) == 1);
    assert(RunColour(image, 1920, 1080) == (1080ull << 32 | 3840));
    // 16:9: the same hooks (no margin to apply; full-screen copies and the paper still scale).
    for (auto [w, h] : {std::pair{1024, 576}, {1280, 720}, {2560, 1440}, {3840, 2160}}) {
        Image plain;
        Stock(plain, w, h);
        Callees(plain);
        assert(!Start(plain.base(), w, h, &problem) && problem == "UI hook signatures differ");
        assert(plain.data[0x3d6699] == 0xe8 && plain.data[0x3d669a] == 0xc2); // nothing written
        problem.clear();
        Prologues(plain);
        assert(Start(plain.base(), w, h, &problem));
        assert(plain.data[0x764860] == 0x48 && plain.data[0x765750] == 0x48 && plain.data[0x235ce0] == 0x48);
        SceneTemplate(plain);
        RunScene(plain);
        assert(plain.f(0xa12f54) == w && plain.f(0xa12f58) == h && plain.f(0x968c80) == h / 1080.f &&
               plain.u(0xa1279c) == 0x102);
        assert(RunColour(plain, 1920, 1080) == (uint64_t(h) << 32 | unsigned(w)));
    }
    std::puts("PASS: start installs the UI hooks and every patch together on every native canvas, "
              "nothing without the hook signatures");
}

/// The PT_LOAD bytes of an eboot.elf at their virtual addresses.
bool LoadElf(const char* path, Image& image) {
    std::ifstream file(path, std::ios::binary);
    std::vector<uint8_t> elf{std::istreambuf_iterator<char>(file), {}};
    if (elf.size() < 64 || std::memcmp(elf.data(), "\x7f" "ELF", 4)) return false;
    uint64_t phoff;
    uint16_t phentsize, phnum;
    std::memcpy(&phoff, &elf[0x20], 8);
    std::memcpy(&phentsize, &elf[0x36], 2);
    std::memcpy(&phnum, &elf[0x38], 2);
    for (unsigned i = 0; i < phnum; ++i) {
        const uint8_t* ph = &elf[phoff + i * phentsize];
        uint32_t type;
        uint64_t offset, vaddr, filesz;
        std::memcpy(&type, ph, 4);
        std::memcpy(&offset, ph + 8, 8);
        std::memcpy(&vaddr, ph + 16, 8);
        std::memcpy(&filesz, ph + 32, 8);
        if (type != 1 || vaddr + filesz > ImageSize || offset + filesz > elf.size()) continue;
        std::memcpy(image.data + vaddr, &elf[offset], filesz);
    }
    return true;
}

} // namespace

int main(int argc, char** argv) {
    TestSizes();
    TestStaticWrites();
    TestStubs();
    TestFailClosed();
    TestStart();
    if (argc > 1) {
        Image image;
        assert(LoadElf(argv[1], image));
        for (auto [w, h] : {std::pair{2560, 1080}, {3440, 1440}, {3840, 1440}, {3840, 1080},
                            {5120, 1440}, {1024, 576}, {1280, 720}, {2560, 1440}, {3840, 2160}}) {
            std::string problem;
            if (!Matches(image.base(), Writes(w, h), &problem)) {
                std::printf("FAIL: %s: %s\n", argv[1], problem.c_str());
                return 1;
            }
        }
        if (argc > 2) {
            Apply(image.base(), Writes(3440, 1440));
            std::ofstream(argv[2], std::ios::binary)
                .write(reinterpret_cast<const char*>(image.data), ImageSize);
        }
        std::printf("PASS: every patch site matches %s\n", argv[1]);
    }
    return 0;
}
