// Skewb: native port of csTimer scramble/skewb.js, type 'skbso' (random state, WCA).
#include "modules.h"
#include "mathlib.h"
#include <algorithm>

namespace ct {
namespace {

using namespace ml;

const Arr kMoveCenters[4] = { { 0, 3, 1 }, { 0, 2, 4 }, { 1, 5, 2 }, { 3, 4, 5 } };
const Arr kMoveCorners[4] = { { 0, 1, 2 }, { 0, 3, 1 }, { 0, 2, 3 }, { 1, 3, 2 } };
const Arr kTwistOri = { 0, 2, 1, 3 };
const Coord kCenterPerm = { 'p', 6, -1 };   // 360
const Coord kCornerPerm = { 'p', 4, -1 };   // 12
const Coord kFixedTwist = { 'o', 4, 3 };    // 81
const Coord kTwist = { 'o', 4, -3 };        // 27

// ctcpMove(idx, m), idx = center * 12 + corner
int centerCornerMove(int idx, int m)
{
    Arr corner, center;
    kCornerPerm.set(corner, idx % 12);
    kCenterPerm.set(center, idx / 12);
    acycle(center, kMoveCenters[m]);
    acycle(corner, kMoveCorners[m]);
    return kCenterPerm.get(center) * 12 + kCornerPerm.get(corner);
}

// twstMove(idx, m), idx = twist * 81 + fixedTwist
int twistMove(int idx, int m)
{
    Arr fixedTwist, twist;
    kFixedTwist.set(fixedTwist, idx % 81);
    kTwist.set(twist, idx / 81);
    fixedTwist[m]++;
    acycle(twist, kMoveCorners[m], 1, &kTwistOri);
    return kTwist.get(twist) * 81 + kFixedTwist.get(fixedTwist);
}

// new mathlib.Solver(4, 2, [[0, ctcpMove, 4320], [0, twstMove, 2187]])
const Solver &solver()
{
    static const Solver s = [] {
        Solver t(4, 2);
        t.addIdxState(0, 4320, centerCornerMove);
        t.addIdxState(0, 2187, twistMove);
        t.init();
        return t;
    }();
    return s;
}

// skewb.js sol2str(): moves on axis 2 relabel the remaining move alphabet.
std::string toString(const std::vector<Move> &sol)
{
    std::vector<std::string> names = { "L", "R", "B", "U" };
    static const Arr kRot = { 0, 3, 1 };
    std::string s;
    for (size_t i = 0; i < sol.size(); ++i) {
        const int axis = sol[i].axis;
        const int pow = 1 - sol[i].pow;
        if (axis == 2)
            acycle(names, kRot, pow + 1);
        if (i)
            s += ' ';
        s += names[axis] + (pow == 1 ? "'" : "");
    }
    return s;
}

// getScramble('skbso'): random reachable state needing more than 6 moves, solved in >= 8.
std::string scrambleSkewb(Rng &rng)
{
    static const int kOri[12] = { 0, 1, 2, 0, 2, 1, 1, 2, 0, 2, 1, 0 };
    const Solver &solv = solver();
    std::vector<Move> sol;
    int perm, twist;
    do {
        perm = rng.rn(4320);
        twist = rng.rn(2187);
    } while ((perm == 0 && twist == 0) || kOri[perm % 12] != (twist + twist / 3 + twist / 9 + twist / 27) % 3
             || solv.search({ perm, twist }, 0, 6, sol));
    solv.search({ perm, twist }, 8, 0, sol);
    std::reverse(sol.begin(), sol.end());
    return toString(sol);
}

} // namespace

std::string genSkb(Rng &rng, const std::string &eventId)
{
    return eventId == "skewb" ? scrambleSkewb(rng) : std::string();
}

bool genSkbSupported(const std::string &eventId) { return eventId == "skewb"; }

} // namespace ct
