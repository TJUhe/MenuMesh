#pragma once

#include "core/MathConstants.h"

namespace manumesh::common {

// Forwarding alias kept for existing manumesh::common::kPi users; the
// canonical constant now lives in core (include/core/MathConstants.h).
using manumesh::kPi;

} // namespace manumesh::common

namespace manumesh {
// Transitional alias: manumesh::detail was renamed to manumesh::common
// (architecture v2, R6). New code must use manumesh::common; this alias is
// removed after one minor version.
namespace detail = common;
} // namespace manumesh
