// WCA scramblers, C++ port of csTimer (https://github.com/cs0x7f/cstimer, commit 2547d82,
// GPL-3.0). Each generator follows the csTimer source line by line, including the order and
// number of random draws, so that with the same seed the output is identical to csTimer's.
#ifndef CT_SCRAMBLE_H
#define CT_SCRAMBLE_H

#include <string>
#include "rng.h"

namespace ct {

class Scrambler {
public:
    void seed(const std::string &s) { rng_.seed(s); }
    // App event id (e.g. "333", "444bld", "minx"); returns the normalized scramble text
    // ('\n' between Megaminx lines), or "" if the event is not available natively.
    std::string generate(const std::string &eventId);
    static bool supported(const std::string &eventId);

private:
    Rng rng_;
    std::string mega(const std::vector<std::vector<std::string>> &turns,
                     const std::vector<std::string> &suffixes, int length);
    std::string cubeBig(int n, int length);      // 555wca / 666wca / 777wca
    std::string clockWca();                      // clkwca
    std::string megaminxPochmann(int length);    // mgmp
};

std::string normalizeScramble(const std::string &raw);

} // namespace ct

#endif
