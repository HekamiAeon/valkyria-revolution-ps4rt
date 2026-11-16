// SPDX-License-Identifier: GPL-2.0-or-later
// Valkyria Revolution (CUSA06810) native canvas: the startup patches that make the game render
// its scene, UI and display buffer at another native size instead of 1920x1080: wider than 16:9
// (1080 or 1440 high, up to 32:9, valkyria_wide_canvas.h) or another 16:9 size. Applied in memory while the loader
// still has the game's code writable; the files on disk are never changed.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ValkyriaWidePatches {

/// The game image base: the loader's first low mapping (src/runtime_memory.c LOW_MIN).
inline constexpr uintptr_t GuestBase = 0x800000000;

struct Write {
    uint64_t at; ///< eboot virtual address (image offset)
    std::vector<uint8_t> before, after;
};

/// A supported wide canvas: valkyria_wide_canvas.h plus a width that is a multiple of 8 (the
/// engine's render targets require it).
bool Wide(int width, int height);
/// A wide canvas, or another 16:9 size of the menu (1024x576, 1280x720, 2560x1440, 3840x2160).
bool Supported(int width, int height);
/// Every write for a width x height canvas, each with the original bytes it replaces; empty
/// for an unsupported size.
std::vector<Write> Writes(int width, int height);
/// Every write's original bytes are at base (read without faulting). Else names the first
/// mismatch in problem.
bool Matches(uintptr_t base, const std::vector<Write>& writes, std::string* problem);
/// Writes after (call only after Matches).
void Apply(uintptr_t base, const std::vector<Write>& writes);
/// The whole canvas at base: patches and UI hooks (valkyria_wide_ui.h), all or nothing.
/// False leaves the game at 1920x1080 and names the reason.
bool Start(uintptr_t base, int width, int height, std::string* problem);

} // namespace ValkyriaWidePatches
