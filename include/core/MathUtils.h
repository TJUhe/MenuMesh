/**
 * @file include/core/MathUtils.h
 * @brief Provides small C++14-compatible math helpers.
 * @ingroup manumesh_core
 */

#pragma once

namespace manumesh {

template <typename T> constexpr T clampValue(const T& value, const T& lower, const T& upper) {
    return value < lower ? lower : (upper < value ? upper : value);
}

} // namespace manumesh
