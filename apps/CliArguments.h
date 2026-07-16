/**
 * @file apps/CliArguments.h
 * @brief Declares cli arguments facilities for the ManuMesh command-line application.
 * @ingroup manumesh_cli
 *
 * @details CLI parsing, validation, dispatch, and reporting are kept outside the geometry library so SDK behavior is independent of process-global command-line state.
 */

#pragma once

#include <string>
#include <vector>

namespace manumesh::cli {

struct Args {
    std::vector<std::string> values; ///< Raw arguments excluding the executable name.
};

/// @return true when the exact switch/value flag occurs in `args`.
bool hasFlag(const Args& args, const std::string& name);
/// @return true when `value` is registered by any command.
bool isKnownFlag(const std::string& value);
/// @return true when the registered option consumes the following token.
bool takesValue(const std::string& value);
/// Validates that every option belongs to `command` and every value is present.
/// @throws std::invalid_argument with the owning commands for misplaced flags.
void validateArgsForCommand(const std::string& command, const Args& args);
/// @return Grouped help generated from the same table used for validation.
std::string optionsHelpText();
/// Returns a flag value or caller-supplied default when absent.
std::string getArg(const Args& args, const std::string& name, const std::string& defaultValue = "");
/// Parses a complete decimal integer token or throws an option-named diagnostic.
int parseIntStrict(const std::string& value, const std::string& name);
/// Parses a complete finite floating-point token or throws an option-named diagnostic.
double parseDoubleStrict(const std::string& value, const std::string& name);
/// Fetches and strictly parses an integer option.
int getIntArg(const Args& args, const std::string& name, int defaultValue);
/// Fetches and strictly parses a floating-point option.
double getDoubleArg(const Args& args, const std::string& name, double defaultValue);
/// @return Tokens not consumed as registered options or option values.
std::vector<std::string> positionalArgs(const Args& args);
/// Parses a comma-separated finite weight list.
std::vector<double> parseWeights(const std::string& text);
/// Parses a comma-separated positive face-count list.
std::vector<int> parseFaceCounts(const std::string& text);

} // namespace manumesh::cli
