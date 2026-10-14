// Skewb: native port of csTimer pending (stub). Returns "" -> JS fallback.
#include "modules.h"
namespace ct {
std::string genSkb(Rng &, const std::string &) { return std::string(); }
bool genSkbSupported(const std::string &) { return false; }
} // namespace ct
