#include "grass/grass.h"

namespace grass {

GrassException::GrassException(const std::string &reason)
    : std::runtime_error(reason) {}

GrassException::~GrassException() {}

} // namespace grass
