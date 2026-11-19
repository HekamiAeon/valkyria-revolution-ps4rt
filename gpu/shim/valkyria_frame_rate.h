// SPDX-License-Identifier: GPL-2.0-or-later
// Valkyria Revolution (CUSA06810) engine frame rate: the game's own 30 updates a second, or 60
// (experimental). The game updates and draws once every two display refreshes; these startup
// patches let it update once a refresh and keep what it counts in updates, or in whole 30 Hz
// frames, at its authored speed. Applied in memory while the loader still has the game's
// code writable, all or nothing; the files on disk are never changed. They are loaded at
// every launch unless the settings file switches them off, and the rate is chosen while the
// game runs (SetRate): an update two refreshes long is the shipped rate through the same
// patches, so only the clock's minimum changes.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "valkyria_wide_patches.h"

namespace ValkyriaFrameRate {

using ValkyriaWidePatches::Write;

/// Every write of the option, each with the original bytes it replaces; a write whose bytes
/// stay the same is only verified. `sixty` is the rate the game's clock starts with.
std::vector<Write> Writes(bool sixty = true);
/// The whole option at base, after every original byte matched: the writes, then the hooks on
/// the camera clip samplers (valkyria_camera_cuts.h). False changes nothing (the unmodified
/// game at its own 30) and names the reason.
bool Start(uintptr_t base, std::string* problem, bool sixty = true);
/// After Start: 60 updates a second, or the game's own 30, from the next update on (the
/// clock's minimum tick count). False while the game has not made its clock yet.
bool SetRate(uintptr_t base, bool sixty);

} // namespace ValkyriaFrameRate
