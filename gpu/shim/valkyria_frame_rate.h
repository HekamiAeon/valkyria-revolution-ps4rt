// SPDX-License-Identifier: GPL-2.0-or-later
// Valkyria Revolution (CUSA06810) at 60 updates a second (experimental, off by default). The
// game updates and draws once every two display refreshes; these startup patches let it update
// once a refresh and keep what it counts in updates, or in whole 30 Hz frames, at its authored
// speed. Applied in memory while the loader still has the game's code writable, all or
// nothing; the files on disk are never changed.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "valkyria_wide_patches.h"

namespace ValkyriaFrameRate {

using ValkyriaWidePatches::Write;

/// Every write of the option, each with the original bytes it replaces.
std::vector<Write> Writes();
/// The whole option at base, after every original byte matched. False changes nothing (the
/// game runs at its own 30) and names the reason.
bool Start(uintptr_t base, std::string* problem);

} // namespace ValkyriaFrameRate
