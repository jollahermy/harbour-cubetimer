// Square-1: native port of csTimer pending (stub). Returns "" -> JS fallback.
#include "modules.h"
namespace ct {
std::string genSq1(Rng &, const std::string &) { return std::string(); }
bool genSq1Supported(const std::string &) { return false; }
} // namespace ct
