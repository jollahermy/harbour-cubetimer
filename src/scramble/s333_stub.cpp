// 3x3 family stub: linked into the app until the native s333 port passes test/native_diff.sh.
#include "modules.h"
namespace ct {
std::string gen333(Rng &, const std::string &) { return std::string(); }
bool gen333Supported(const std::string &) { return false; }
} // namespace ct
