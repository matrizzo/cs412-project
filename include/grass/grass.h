#pragma once

#include <stdexcept>

/*
 * This function should be called by attackers on a successful control flow
 * hijack
 */
void hijack_flow();

namespace grass {

// Base exception class used for GRASS errors
class GrassException : public std::runtime_error {
public:
    GrassException(const std::string &reason);
    virtual ~GrassException();
};

} // namespace grass
