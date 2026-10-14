// Pyraminx: native port of csTimer pending (stub). Returns "" -> JS fallback.
#include "modules.h"
namespace ct {
std::string genPyr(Rng &, const std::string &) { return std::string(); }
bool genPyrSupported(const std::string &) { return false; }
} // namespace ct
