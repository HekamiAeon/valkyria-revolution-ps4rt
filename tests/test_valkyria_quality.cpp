// SPDX-License-Identifier: GPL-2.0-or-later
// Valkyria graphics quality startup patches on a synthetic image (no game data): well-formed
// writes, all or nothing, and what High and Low leave in the game's code. With an eboot.elf
// as the argument, also that every patch site matches that game build.
#undef NDEBUG
#include <sys/mman.h>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "gpu/shim/ps4rt_graphics.h"
#include "gpu/shim/valkyria_quality.h"

namespace {

using namespace Ps4RtGraphics;
using ValkyriaQuality::Write;
using ValkyriaQuality::Writes;

constexpr size_t ImageSize = 0xa30000;

struct Image {
    uint8_t* data = static_cast<uint8_t*>(mmap(nullptr, ImageSize, PROT_READ | PROT_WRITE,
                                               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    Image() { assert(data != MAP_FAILED); }
    ~Image() { munmap(data, ImageSize); }
    uintptr_t base() const { return reinterpret_cast<uintptr_t>(data); }
    /// The shipped bytes of every site of both qualities.
    void ship() {
        for (const int quality : {int(QualityHigh), int(QualityLow)})
            for (const Write& write : Writes(quality))
                std::copy(write.before.begin(), write.before.end(), data + write.at);
    }
    std::vector<uint8_t> bytes() const { return {data, data + ImageSize}; }
    int32_t imm(uint64_t at) const {
        int32_t value;
        std::memcpy(&value, data + at, 4);
        return value;
    }
};

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
    // Max, and a value that is no quality, have no writes; High and Low have well-formed ones
    // that do not overlap, and each changes something.
    assert(Writes(QualityMax).empty() && Writes(-1).empty() && Writes(QualityCount).empty());
    for (const int quality : {int(QualityHigh), int(QualityLow)}) {
        const std::vector<Write> writes = Writes(quality);
        bool changes = false;
        for (size_t i = 0; i < writes.size(); ++i) {
            assert(!writes[i].before.empty() && writes[i].before.size() == writes[i].after.size());
            assert(writes[i].at + writes[i].before.size() <= ImageSize);
            changes |= writes[i].before != writes[i].after;
            for (size_t j = 0; j < i; ++j)
                assert(writes[j].at + writes[j].before.size() <= writes[i].at ||
                       writes[i].at + writes[i].before.size() <= writes[j].at);
        }
        assert(changes);
    }

    std::string problem;
    {
        // Max changes nothing, whatever the image holds.
        Image image;
        const std::vector<uint8_t> empty = image.bytes();
        assert(ValkyriaQuality::Start(image.base(), QualityMax, &problem) && image.bytes() == empty);
        // Not the game the patches were read from: nothing is written and the site is named.
        assert(!ValkyriaQuality::Start(image.base(), QualityHigh, &problem) && image.bytes() == empty);
        assert(problem == "game code differs at 0x1cf0ae");
        assert(!ValkyriaQuality::Start(image.base(), QualityLow, &problem) && image.bytes() == empty);
        assert(problem == "game code differs at 0x1ceaff");
        assert(!ValkyriaQuality::Start(image.base(), QualityCount, &problem) && image.bytes() == empty);
        assert(problem == "no such quality");
    }
    for (const int quality : {int(QualityHigh), int(QualityLow)}) {
        // One changed byte anywhere in a quality's sites, the verified-only ones too: nothing
        // is written.
        const std::vector<Write> writes = Writes(quality);
        for (const Write& write : writes) {
            for (const size_t byte : {size_t(0), write.before.size() - 1}) {
                Image image;
                image.ship();
                image.data[write.at + byte] ^= 0x40;
                const std::vector<uint8_t> before = image.bytes();
                assert(!ValkyriaQuality::Start(image.base(), quality, &problem));
                assert(image.bytes() == before);
            }
        }
    }
    {
        // High: the three size levels start at 2 (1024 a side) and a scene's setup stores 2
        // for the first two maps; the stores themselves and the shadow switch are untouched.
        Image image;
        image.ship();
        const std::vector<uint8_t> shipped = image.bytes();
        assert(image.imm(0x1cf0ae + 7) == 0 && image.imm(0x1cf0b9 + 7) == 1 && image.imm(0x1cf0c4 + 7) == 1);
        assert(ValkyriaQuality::Start(image.base(), QualityHigh, &problem));
        assert(image.imm(0x1cf0ae + 7) == 2 && image.imm(0x1cf0b9 + 7) == 2 && image.imm(0x1cf0c4 + 7) == 2);
        const uint8_t scene[] = {0xba, 2, 0, 0, 0, 0x90,             // mov edx,2 ; nop
                                 0x89, 0x91, 0xd0, 0x0a, 0, 0,       // mov [rcx+0xad0],edx
                                 0xb8, 2, 0, 0, 0, 0x90,             // mov eax,2 ; nop
                                 0x89, 0x81, 0xd4, 0x0a, 0, 0};      // mov [rcx+0xad4],eax
        assert(!std::memcmp(image.data + 0x4c2f86, scene, sizeof(scene)));
        // Nothing else changed: put the changed bytes back and it is the shipped image.
        std::vector<uint8_t> now = image.bytes();
        for (const Write& write : Writes(QualityHigh))
            std::copy(write.before.begin(), write.before.end(), now.begin() + write.at);
        assert(now == shipped);
        assert(image.imm(0x1ceaff + 7) == 1);
        // A second start on the patched image finds it changed and leaves it.
        const std::vector<uint8_t> patched = image.bytes();
        assert(!ValkyriaQuality::Start(image.base(), QualityHigh, &problem) && image.bytes() == patched);
    }
    {
        // Low: the renderer's shadow map switch starts at 0; one byte of the image differs.
        Image image;
        image.ship();
        const std::vector<uint8_t> shipped = image.bytes();
        assert(image.imm(0x1ceaff + 7) == 1);
        assert(ValkyriaQuality::Start(image.base(), QualityLow, &problem));
        assert(image.imm(0x1ceaff + 7) == 0);
        std::vector<uint8_t> now = image.bytes();
        size_t differing = 0;
        for (size_t i = 0; i < now.size(); ++i) differing += now[i] != shipped[i];
        assert(differing == 1);
        assert(image.imm(0x1cf0ae + 7) == 0 && image.imm(0x1cf0b9 + 7) == 1 && image.imm(0x1cf0c4 + 7) == 1);
    }
    std::puts("PASS: graphics quality: Max writes nothing; High holds the three shadow map levels at 1024 "
              "(start values and the scene's stores); Low starts the shadow map switch off; any changed "
              "byte in a site, verified-only ones too, leaves the image untouched");
    if (argc > 1) {
        Image image;
        assert(LoadElf(argv[1], image));
        for (const int quality : {int(QualityHigh), int(QualityLow)}) {
            if (!ValkyriaWidePatches::Matches(image.base(), Writes(quality), &problem)) {
                std::printf("FAIL: %s: %s\n", argv[1], problem.c_str());
                return 1;
            }
        }
        std::printf("PASS: every graphics quality patch site matches %s\n", argv[1]);
    }
    return 0;
}
