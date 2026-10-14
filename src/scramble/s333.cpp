// 333 family (333, 333oh, 333bld, 333fm): native port of csTimer pending (stub). Returns "" -> JS fallback.
#include "modules.h"
namespace ct {
std::string gen333(Rng &, const std::string &) { return std::string(); }
bool gen333Supported(const std::string &) { return false; }
} // namespace ct
