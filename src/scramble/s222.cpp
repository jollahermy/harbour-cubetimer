// 2x2x2: native port of csTimer pending (stub). Returns "" -> JS fallback.
#include "modules.h"
namespace ct {
std::string gen222(Rng &, const std::string &) { return std::string(); }
bool gen222Supported(const std::string &) { return false; }
} // namespace ct
