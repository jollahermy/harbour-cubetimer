// Pyraminx: native port of the bundle's WCA-patched csTimer 'pyrso' (scramble/pyraminx.js
// getWcaScramble): random state with uniformly drawn tips, rejected below 6 moves (WCA 4b3f).
#include "modules.h"
#include "mathlib.h"
#include <algorithm>

namespace ct {
namespace {

using namespace ml;

const Arr kMovePieces[4] = { { 0, 1, 3 }, { 1, 2, 5 }, { 0, 4, 2 }, { 3, 5, 4 } };
const Arr kMoveOris[4] = { { 0, 1, 0, 2 }, { 0, 1, 0, 2 }, { 0, 0, 1, 2 }, { 0, 0, 1, 2 } };
const Coord kEdgeOri = { 'o', 6, -2 };   // 32
const Coord kCornOri = { 'o', 4, 3 };    // 81

void edgePermMove(Arr &arr, int m) { acycle(arr, kMovePieces[m]); }

// oriMove(a, m) with a = cornOri << 5 | edgeOri
int oriMove(int a, int m)
{
    Arr edgeOri, cornOri;
    kEdgeOri.set(edgeOri, a & 0x1f);
    kCornOri.set(cornOri, a >> 5);
    cornOri[m]++;
    acycle(edgeOri, kMovePieces[m], 1, &kMoveOris[m]);
    return kCornOri.get(cornOri) << 5 | kEdgeOri.get(edgeOri);
}

// new mathlib.Solver(4, 2, [[0, [epermMove, 'p', 6, -1], 360], [0, oriMove, 2592]])
const Solver &solver()
{
    static const Solver s = [] {
        Solver t(4, 2);
        t.addCoordState(0, 360, { 'p', 6, -1 }, edgePermMove);
        t.addIdxState(0, 2592, oriMove);
        t.init();
        return t;
    }();
    return s;
}

std::string scramblePyraminx(Rng &rng)
{
    const Solver &solv = solver();
    std::vector<Move> sol;
    int perm, ori, tips[4];
    for (;;) {
        perm = rng.rn(360);
        ori = rng.rn(2592);
        int nTips = 0;
        for (int &t : tips) {
            t = rng.rn(3);   // 0: tip turned clockwise, 1: anticlockwise, 2: untouched
            nTips += t < 2;
        }
        if (solv.search({ perm, ori }, 0, 0, sol) && static_cast<int>(sol.size()) + nTips >= 6)
            break;   // also rejects the solved state
    }
    std::reverse(sol.begin(), sol.end());
    static const std::vector<std::string> kPow = { "'", "" };
    std::string s = Solver::toStr(sol, "ULRB", kPow) + ' ';
    for (int i = 0; i < 4; ++i)
        if (tips[i] < 2)
            s += std::string(1, "lrbu"[i]) + (tips[i] ? "' " : " ");
    return s;
}

} // namespace

std::string genPyr(Rng &rng, const std::string &eventId)
{
    return eventId == "pyram" ? scramblePyraminx(rng) : std::string();
}

bool genPyrSupported(const std::string &eventId) { return eventId == "pyram"; }

} // namespace ct
