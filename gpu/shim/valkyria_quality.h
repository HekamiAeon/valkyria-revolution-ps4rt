// SPDX-License-Identifier: GPL-2.0-or-later
// Valkyria Revolution (CUSA06810) graphics quality (Ps4RtGraphics::Quality), the option for
// slow GPUs. Max is the game as shipped. High and Low reduce its shadows, the one part of a
// frame the game lets go of with nothing else changing:
//   High: the three shadow maps at the smallest size the game keeps. It makes every map at
//         4096, 2048 and 1024 a side and picks one by a level per map (as shipped the first
//         at 4096 and the others at 2048; a scene's setup stores its own for the first two).
//         The levels start at 1024 and a scene's setup stores 1024.
//   Low:  no shadow maps. The renderer's own switch for them (the "Use Shadow Map" item of
//         the game's debug menu) starts off: a frame clears the maps and skips drawing into
//         them, and the scene is lit as if nothing cast a shadow.
// Measured on a Radeon 610M (integrated), a battle in an open area at 1280x720 without an
// upscaler, GPU time a frame: Max 41.4 ms (23.9 frames a second), High 37.5 ms (26.4), Low
// 33.3 ms (29.6). The rest was not worth a change: depth of field off (a switch of the
// game's) saved 0.3 ms, its glare is 1.3 ms of small passes with no switch, and the debug
// menu's "Filter Off" removes the painted look.
// Startup patches like the port's others (valkyria_wide_patches.h): applied in memory while
// the loader still has the game's code writable, after every original byte matched, all or
// nothing; the files on disk are never changed. Max writes nothing.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "valkyria_wide_patches.h"

namespace ValkyriaQuality {

using ValkyriaWidePatches::Write;

/// Every write of a quality, each with the original bytes it replaces; a write whose bytes
/// stay the same is only verified. None for Max, or for a value that is no quality.
std::vector<Write> Writes(int quality);
/// The quality at base, after every original byte matched. False changes nothing (the game's
/// own shadows) and names the reason. Max is true and changes nothing.
bool Start(uintptr_t base, int quality, std::string* problem);

} // namespace ValkyriaQuality
