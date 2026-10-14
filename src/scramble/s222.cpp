// 2x2x2: native port of csTimer scramble/2x2x2.js, type '222so' (random state, WCA).
#include "modules.h"
#include "mathlib.h"
#include <algorithm>

namespace ct {
namespace {

using namespace ml;

const Arr kMovePieces[3] = { { 0, 2, 3, 1 }, { 0, 1, 5, 4 }, { 0, 4, 6, 2 } };
const Arr kMoveOris[3] = { {}, { 0, 1, 0, 1, 3 }, { 1, 0, 1, 0, 3 } };   // U turns twist nothing

void permMove(Arr &arr, int m) { acycle(arr, kMovePieces[m]); }
void oriMove(Arr &arr, int m) { acycle(arr, kMovePieces[m], 1, m ? &kMoveOris[m] : nullptr); }

// new mathlib.Solver(3, 3, [[0, [doPermMove, 'p', 7], 5040], [0, [doOriMove, 'o', 7, -3], 729]])
const Solver &solver()
{
    static const Solver s = [] {
        Solver t(3, 3);
        t.addCoordState(0, 5040, { 'p', 7, 0 }, permMove);
        t.addCoordState(0, 729, { 'o', 7, -3 }, oriMove);
        t.init();
        return t;
    }();
    return s;
}

// getScramble('222so'): random state needing more than 3 moves, solved within 9.
std::string scramble222(Rng &rng)
{
    const Solver &solv = solver();
    std::vector<Move> sol;
    int perm, ori;
    do {
        perm = rng.rn(5040);
        ori = rng.rn(729);
    } while ((perm == 0 && ori == 0) || solv.search({ perm, ori }, 0, 3, sol));
    solv.search({ perm, ori }, 9, 0, sol);
    std::reverse(sol.begin(), sol.end());
    static const std::vector<std::string> kPow = { "'", "2", " " };
    return Solver::toStr(sol, "URF", kPow);
}

} // namespace

std::string gen222(Rng &rng, const std::string &eventId)
{
    return eventId == "222" ? scramble222(rng) : std::string();
}

bool gen222Supported(const std::string &eventId) { return eventId == "222"; }

} // namespace ct
