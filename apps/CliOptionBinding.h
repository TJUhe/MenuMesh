/**
 * @file apps/CliOptionBinding.h
 * @brief Declares cli option binding facilities for the ManuMesh command-line application.
 * @ingroup manumesh_cli
 *
 * @details CLI parsing, validation, dispatch, and reporting are kept outside the geometry library so SDK behavior is independent of process-global command-line state.
 */

#pragma once

#include "CliArguments.h"
#include "algorithms/feature_detection/FeatureTypes.h"
#include "algorithms/simplification/SimplificationTypes.h"

namespace manumesh::cli {

/// Binds validated CLI tokens to the public simplification option structure.
simplification::SimplifyOptions parseSimplifyOptions(const Args& args);
/// Binds validated CLI tokens to the standalone feature option structure.
feature::FeatureOptions parseFeatureOptions(const Args& args);

} // namespace manumesh::cli
