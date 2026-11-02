// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string_view>

namespace t7architecture {
// A common surface margin applies to every valid static placement, including
// generated clusters. Keep the exporter and client copies byte-identical.
inline constexpr double minimum_surface_distance = 3072.0;
inline std::uint16_t distance(std::uint16_t current, const float* origin,
                             const float* center, const float* half) {
    if (!current || current == 65535) return current;
    double pivotSquared = 0, radiusSquared = 0;
    for (unsigned axis = 0; axis < 3; ++axis) {
        if (!std::isfinite(origin[axis]) || !std::isfinite(center[axis])
            || !std::isfinite(half[axis]) || half[axis] < 0) return current;
        const double delta = double(center[axis]) - origin[axis];
        pivotSquared += delta * delta;
        radiusSquared += double(half[axis]) * half[axis];
    }
    // IW4 measures from the pivot. Include the complete world-space bounds so
    // off-center and scaled architecture retains a common surface margin.
    const auto required = std::min(65535.0,
        std::ceil(minimum_surface_distance + std::sqrt(pivotSquared) + std::sqrt(radiusSquared)));
    return std::max(current, static_cast<std::uint16_t>(required));
}
}
