// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <optional>

#include "video_core/texture_cache/types.h"

namespace VideoCore {

struct ImageInfo;

[[nodiscard]] std::optional<Extent3D> GetAliasCopyExtent(const ImageInfo& src,
                                                         const ImageInfo& dst);

// ps4rt: the game's texture pool gave this address to another compressed texture of the same
// format (adapted from the Code Fairy port's overlap-dimension check). Another size or tiling
// lays its texels out differently in guest memory, so the cached image cannot serve as a view.
[[nodiscard]] bool IsCompressedPoolReuse(const ImageInfo& lhs, const ImageInfo& rhs);

} // namespace VideoCore
