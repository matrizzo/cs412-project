#pragma once

#include <string>
#include <vector>

namespace grass {

/*
 * Recursively searches the directory at path dir, and returns the paths of all
 * files that match the regular expression in pattern
 */
std::vector<std::string> SearchDirectory(const std::string &dir,
    const std::string &pattern);

} // namespace grass
