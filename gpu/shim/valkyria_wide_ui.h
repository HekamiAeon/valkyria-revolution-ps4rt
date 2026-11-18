// SPDX-License-Identifier: GPL-2.0-or-later
// Valkyria Revolution native canvas UI layout (valkyria_wide_ui.c).
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/// 0 when the required UI hooks' signatures match the game image at base, else -1. Read only.
int valkyria_wide_ui_check(uintptr_t base);
/// Installs the UI layout hooks for a width x height native canvas (a wide canvas, or another
/// 16:9 size: 1024x576, 1280x720, 2560x1440, 3840x2160) into the game image at base, while
/// its code is still writable (before the loader protects it). 0 on success; -1 changes
/// nothing (unsupported size or a signature mismatch). The world-map cursor, narration paper,
/// UI layer and screen plane patches are optional and reported when unavailable.
int valkyria_wide_ui_install(uintptr_t base, unsigned width, unsigned height);

#ifdef __cplusplus
}
#endif
