// Per-event native scramblers (C++ ports of csTimer, GPL-3.0). Each module lives in its own file
// and returns "" for events it does not (yet) implement, so the app falls back to the JS worker.
#ifndef CT_MODULES_H
#define CT_MODULES_H
#include <string>
#include "rng.h"
namespace ct {
// s333.cpp: 333 family (333, 333oh, 333bld, 333fm)
std::string gen333(Rng &rng, const std::string &eventId);
bool gen333Supported(const std::string &eventId);
// s222.cpp: 2x2x2
std::string gen222(Rng &rng, const std::string &eventId);
bool gen222Supported(const std::string &eventId);
// spyr.cpp: Pyraminx
std::string genPyr(Rng &rng, const std::string &eventId);
bool genPyrSupported(const std::string &eventId);
// sskb.cpp: Skewb
std::string genSkb(Rng &rng, const std::string &eventId);
bool genSkbSupported(const std::string &eventId);
// ssq1.cpp: Square-1
std::string genSq1(Rng &rng, const std::string &eventId);
bool genSq1Supported(const std::string &eventId);
// s444.cpp: 4x4x4 (444, 444bld)
std::string gen444(Rng &rng, const std::string &eventId);
bool gen444Supported(const std::string &eventId);
// s333.cpp: min2phase solve of a 54-facelet string (scramble_333.solvFacelet), for the 4x4 3x3 stage.
std::string solveFacelet333(const std::string &facelet);
} // namespace ct
#endif
