// SPDX-License-Identifier: GPL-2.0-or-later
#include "valkyria_quality.h"

#include <cstdio>

#include "ps4rt_graphics.h"

namespace ValkyriaQuality {

std::vector<Write> Writes(int quality) {
    if (quality == Ps4RtGraphics::QualityHigh) {
        return {
            // The renderer's constructor, mov dword [r15+0xad0/4/8],level: the size level of
            // each map (0 is 4096 a side, 1 is 2048, 2 is 1024), as shipped 0, 1, 1.
            {0x1cf0ae, {0x41, 0xc7, 0x87, 0xd0, 0x0a, 0, 0, 0, 0, 0, 0}, {0x41, 0xc7, 0x87, 0xd0, 0x0a, 0, 0, 2, 0, 0, 0}},
            {0x1cf0b9, {0x41, 0xc7, 0x87, 0xd4, 0x0a, 0, 0, 1, 0, 0, 0}, {0x41, 0xc7, 0x87, 0xd4, 0x0a, 0, 0, 2, 0, 0, 0}},
            {0x1cf0c4, {0x41, 0xc7, 0x87, 0xd8, 0x0a, 0, 0, 1, 0, 0, 0}, {0x41, 0xc7, 0x87, 0xd8, 0x0a, 0, 0, 2, 0, 0, 0}},
            // A scene's setup, the only other writer: mov edx,[rax+0x270] and mov eax,[rax+0x274]
            // (the scene's own levels for the first two maps) become mov edx,2 and mov eax,2;
            // the stores that follow each, mov [rcx+0xad0],edx and mov [rcx+0xad4],eax, stay.
            {0x4c2f86, {0x8b, 0x90, 0x70, 0x02, 0, 0}, {0xba, 2, 0, 0, 0, 0x90}},
            {0x4c2f8c, {0x89, 0x91, 0xd0, 0x0a, 0, 0}, {0x89, 0x91, 0xd0, 0x0a, 0, 0}},
            {0x4c2f92, {0x8b, 0x80, 0x74, 0x02, 0, 0}, {0xb8, 2, 0, 0, 0, 0x90}},
            {0x4c2f98, {0x89, 0x81, 0xd4, 0x0a, 0, 0}, {0x89, 0x81, 0xd4, 0x0a, 0, 0}},
        };
    }
    if (quality == Ps4RtGraphics::QualityLow) {
        return {
            // The renderer's constructor, mov dword [r15+0x4ac],1: its "draw the shadow maps"
            // switch starts off.
            {0x1ceaff, {0x41, 0xc7, 0x87, 0xac, 0x04, 0, 0, 1, 0, 0, 0}, {0x41, 0xc7, 0x87, 0xac, 0x04, 0, 0, 0, 0, 0, 0}},
            // The frame's test of it, cmp dword [rdi+0x4ac],0 ; je (clear the maps and skip
            // their passes), and the only other writer, the debug menu's mov [rbx+0x4ac],eax.
            {0x1b8a4b, {0x83, 0xbf, 0xac, 0x04, 0, 0, 0, 0x0f, 0x84, 0xaf, 0x01, 0, 0},
                       {0x83, 0xbf, 0xac, 0x04, 0, 0, 0, 0x0f, 0x84, 0xaf, 0x01, 0, 0}},
            {0x468099, {0x89, 0x83, 0xac, 0x04, 0, 0}, {0x89, 0x83, 0xac, 0x04, 0, 0}},
        };
    }
    return {};
}

bool Start(uintptr_t base, int quality, std::string* problem) {
    if (quality == Ps4RtGraphics::QualityMax) return true;
    const std::vector<Write> writes = Writes(quality);
    if (writes.empty()) {
        if (problem) *problem = "no such quality";
        return false;
    }
    if (!ValkyriaWidePatches::Matches(base, writes, problem)) return false;
    ValkyriaWidePatches::Apply(base, writes);
    std::printf("Valkyria graphics quality: %s\n", quality == Ps4RtGraphics::QualityHigh
                                                       ? "High, shadow maps at 1024"
                                                       : "Low, no shadow maps");
    return true;
}

} // namespace ValkyriaQuality
