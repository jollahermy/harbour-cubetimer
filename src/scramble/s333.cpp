// 3x3x3 family (333, 333oh, 333bld, 333fm): native port of csTimer's random-state scrambler,
// scramble/scramble_333_edit.js getAnyScramble() + lib/min2phase.js (two-phase solver by Chen
// Shuang), csTimer commit 2547d82, GPL-3.0.
//
// Differences from the JS, none of which change the output:
// - Pruning tables are built completely once (min2phase.initFull()), instead of csTimer's
//   progressive partial init; test/native_diff.sh compares against the JS with initFull().
// - The random cube is handed to the solver as cubies, not via a facelet string (min2phase's
//   fromFacelet() is the exact inverse of mathlib's toFaceCube(): same facelet tables).
// - Search::next() / isRec (re-entrant search) is not needed and not ported.
#include "modules.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace ct {
namespace {

using std::array;
using std::vector;

// JS shifts use the low 5 bits of the count (lm can be negative here).
inline int shr(int x, int n) { return x >> (n & 31); }

const int N_MOVES = 18, N_MOVES2 = 10;
const int N_FLIP = 2048, N_FLIP_SYM = 336, N_TWST = 2187, N_TWST_SYM = 324;
const int N_PERM = 40320, N_PERM_SYM = 2768, N_MPERM = 24, N_SLICE = 495, N_COMB = 140;
const int MAX_PRE_MOVES = 20, MIN_P1LENGTH_PRE = 7, MAX_DEPTH2 = 13;
const int SYM_E2C_MAGIC = 0x00DDDD00;

const array<int, 18> ud2std = { { 0, 1, 2, 4, 7, 9, 10, 11, 13, 16, 3, 5, 6, 8, 12, 14, 15, 17 } };
const int urfMove[6][18] = {
    { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17 },
    { 6, 7, 8, 0, 1, 2, 3, 4, 5, 15, 16, 17, 9, 10, 11, 12, 13, 14 },
    { 3, 4, 5, 6, 7, 8, 0, 1, 2, 12, 13, 14, 15, 16, 17, 9, 10, 11 },
    { 2, 1, 0, 5, 4, 3, 8, 7, 6, 11, 10, 9, 14, 13, 12, 17, 16, 15 },
    { 8, 7, 6, 2, 1, 0, 5, 4, 3, 17, 16, 15, 11, 10, 9, 14, 13, 12 },
    { 5, 4, 3, 8, 7, 6, 2, 1, 0, 14, 13, 12, 17, 16, 15, 11, 10, 9 },
};
const char *const move2str[18] = { "U ", "U2", "U'", "R ", "R2", "R'", "F ", "F2", "F'",
                                   "D ", "D2", "D'", "L ", "L2", "L'", "B ", "B2", "B'" };

struct Basics {
    array<int, 18> std2ud {};
    array<int, 11> ckmv2bit {};
    array<array<int, 13>, 13> Cnk {};
    array<int, 13> fact {};
    Basics()
    {
        for (int i = 0; i < 18; i++)
            std2ud[ud2std[i]] = i;
        for (int i = 0; i < 10; i++) {
            const int ix = ud2std[i] / 3;
            for (int j = 0; j < 10; j++) {
                const int jx = ud2std[j] / 3;
                ckmv2bit[i] |= ((ix == jx) || ((ix % 3 == jx % 3) && (ix >= jx)) ? 1 : 0) << j;
            }
        }
        fact[0] = 1;
        for (int i = 0; i < 13; i++) {
            if (i < 12)
                fact[i + 1] = fact[i] * (i + 1);
            Cnk[i][0] = Cnk[i][i] = 1;
            for (int j = 1; j < 13; j++)
                Cnk[i][j] = j <= i ? Cnk[i - 1][j - 1] + Cnk[i - 1][j] : 0;
        }
    }
};
const Basics &B()
{
    static const Basics b;
    return b;
}

// ---------------------------------------------------------------- coordinates on cubie arrays
// Pieces: low nibble = piece index, bits 4+ = orientation (as in min2phase).
template <size_t N> void setNPerm(array<int, N> &arr, int idx, int n)
{
    n--;
    uint32_t val = 0x76543210u;
    for (int i = 0; i < n; ++i) {
        const int p = B().fact[n - i];
        int v = idx / p;
        idx %= p;
        v <<= 2;
        arr[i] = (arr[i] & 0xf0) | static_cast<int>((val >> v) & 0xf);
        const uint32_t m = (1u << v) - 1;
        val = (val & m) + ((val >> 4) & ~m);
    }
    arr[n] = (arr[n] & 0xf0) | static_cast<int>(val & 0xf);
}

template <size_t N> int getNPerm(const array<int, N> &arr, int n)
{
    int idx = 0;
    uint32_t val = 0x76543210u;
    for (int i = 0; i < n - 1; ++i) {
        const int v = (arr[i] & 0xf) << 2;
        idx = (n - i) * idx + static_cast<int>((val >> v) & 0xf);
        val -= 0x11111110u << v;
    }
    return idx;
}

template <size_t N> void setNPermFull(array<int, N> &arr, int idx, int n)
{
    arr[n - 1] = arr[n - 1] & 0xf0;
    for (int i = n - 2; i >= 0; --i) {
        arr[i] = (arr[i] & 0xf0) | idx % (n - i);
        idx /= n - i;
        for (int j = i + 1; j < n; ++j)
            if ((arr[j] & 0xf) >= (arr[i] & 0xf))
                arr[j] += 1;
    }
}

template <size_t N> int getNPermFull(const array<int, N> &arr, int n)
{
    int idx = 0;
    for (int i = 0; i < n; ++i) {
        idx *= n - i;
        for (int j = i + 1; j < n; ++j)
            if ((arr[j] & 0xf) < (arr[i] & 0xf))
                ++idx;
    }
    return idx;
}

template <size_t N> int getComb(const array<int, N> &arr, int mask)
{
    int idxC = 0, r = 4;
    for (int i = static_cast<int>(N) - 1; i >= 0; i--)
        if (((arr[i] & 0xf) & 0xc) == mask)
            idxC += B().Cnk[i][r--];
    return idxC;
}

template <size_t N> void setComb(array<int, N> &arr, int idxC, int mask)
{
    int r = 4, fill = static_cast<int>(N) - 1;
    for (int i = static_cast<int>(N) - 1; i >= 0; i--) {
        // JS: Cnk[i][-1] is undefined and the comparison is false.
        if (r >= 0 && idxC >= B().Cnk[i][r]) {
            idxC -= B().Cnk[i][r--];
            arr[i] = (arr[i] & 0xf0) | r | mask;
        } else {
            if ((fill & 0xc) == mask)
                fill -= 4;
            arr[i] = (arr[i] & 0xf0) | fill--;
        }
    }
}

int getNParity(int idx, int n)
{
    int p = 0;
    for (int i = n - 2; i >= 0; i--) {
        p ^= idx % (n - i);
        idx /= n - i;
    }
    return p & 1;
}

struct Cubie {
    array<int, 8> ca = { { 0, 1, 2, 3, 4, 5, 6, 7 } };
    array<int, 12> ea = { { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 } };

    Cubie &initCoord(int cperm, int twst, int eperm, int flip)
    {
        setNPerm(ca, cperm, 8);
        setTwst(twst);
        setNPermFull(ea, eperm, 12);
        setFlip(flip);
        return *this;
    }
    void setFlip(int idx)
    {
        int parity = 0;
        for (int i = 10; i >= 0; i--, idx >>= 1) {
            ea[i] = (ea[i] & 0xf) | (idx & 1) << 4;
            parity ^= ea[i];
        }
        ea[11] = (ea[11] & 0xf) | (parity & 0x10);
    }
    int getFlip() const
    {
        int idx = 0;
        for (int i = 0; i < 11; i++)
            idx = idx << 1 | ((ea[i] >> 4) & 1);
        return idx;
    }
    void setTwst(int idx)
    {
        int twst = 15;
        for (int i = 6; i >= 0; i--, idx /= 3) {
            ca[i] = (ca[i] & 0xf) | (idx % 3) << 4;
            twst -= ca[i] >> 4;
        }
        ca[7] = (ca[7] & 0xf) | (twst % 3) << 4;
    }
    int getTwst() const
    {
        int idx = 0;
        for (int i = 0; i < 7; i++)
            idx += (idx << 1) + (ca[i] >> 4);
        return idx;
    }
    void setCPerm(int idx) { setNPerm(ca, idx, 8); }
    void setEPerm(int idx) { setNPerm(ea, idx, 8); }
    int getEPerm() const { return getNPerm(ea, 8); }
    int getSlice() const { return 494 - getComb(ea, 8); }
    void setSlice(int idx) { setComb(ea, 494 - idx, 8); }
    int getMPerm() const { return getNPermFull(ea, 12) % 24; }
    void setMPerm(int idx) { setNPermFull(ea, idx, 12); }
    int getCComb() const { return getComb(ca, 0); }
    void setCComb(int idx) { setComb(ca, idx, 0); }

    Cubie &invFrom(const Cubie &cc)
    {
        for (int edge = 0; edge < 12; edge++)
            ea[cc.ea[edge] & 0xf] = (edge & 0xf) | (cc.ea[edge] & 0x10);
        for (int corn = 0; corn < 8; corn++)
            ca[cc.ca[corn] & 0xf] = corn | ((0x40 >> (cc.ca[corn] >> 4)) & 0x30);
        return *this;
    }
};

void edgeMult(const Cubie &a, const Cubie &b, Cubie &prod)
{
    for (int ed = 0; ed < 12; ed++)
        prod.ea[ed] = a.ea[b.ea[ed] & 0xf] ^ (b.ea[ed] & 0x10);
}

void cornMult(const Cubie &a, const Cubie &b, Cubie &prod)
{
    for (int corn = 0; corn < 8; corn++) {
        const int ori = ((a.ca[b.ca[corn] & 0xf] >> 4) + (b.ca[corn] >> 4)) % 3;
        prod.ca[corn] = (a.ca[b.ca[corn] & 0xf] & 0xf) | ori << 4;
    }
}

void cornMultFull(const Cubie &a, const Cubie &b, Cubie &prod)
{
    for (int corn = 0; corn < 8; corn++) {
        const int oriA = a.ca[b.ca[corn] & 0xf] >> 4;
        const int oriB = b.ca[corn] >> 4;
        int ori = oriA + ((oriA < 3) ? oriB : 6 - oriB);
        ori = ori % 3 + ((oriA < 3) == (oriB < 3) ? 0 : 3);
        prod.ca[corn] = (a.ca[b.ca[corn] & 0xf] & 0xf) | ori << 4;
    }
}

int esym2csym(int idx) { return idx ^ ((SYM_E2C_MAGIC >> ((idx & 0xf) << 1)) & 3); }

inline int getPruning(const vector<uint32_t> &table, int index)
{
    return static_cast<int>((table[index >> 3] >> ((index & 7) << 2)) & 0xf);
}
inline void setPruning(vector<uint32_t> &table, int index, int value)
{
    table[index >> 3] ^= static_cast<uint32_t>(value) << ((index & 7) << 2);
}
inline int getPruningMax(int maxValue, const vector<uint32_t> &table, int index)
{
    return std::min(maxValue, getPruning(table, index));
}
inline bool hasZero(uint32_t val) { return ((val - 0x11111111u) & ~val & 0x88888888u) != 0; }

// ---------------------------------------------------------------- tables (built once)
struct Tables {
    array<Cubie, 18> moveCube;
    Cubie urf1, urf2;
    array<Cubie, 16> SymCube;
    array<array<int, 16>, 16> SymMult {}, SymMultInv {};
    array<array<int, 18>, 16> SymMove {}, SymMoveUD {};
    array<int, 18 * 8> Sym8Move {};
    vector<int> FlipS2R, FlipR2S, FlipSelfSym, FlipS2RF;
    vector<int> TwstS2R, TwstR2S, TwstSelfSym;
    vector<int> EPermS2R, EPermR2S, PermSelfSym, Perm2CombP, PermInvEdgeSym;
    vector<int> TwstMove, FlipMove, SliceMove, SliceConj;
    vector<int> CPermMove, EPermMove, MPermMove, MPermConj, CCombPMove, CCombPConj;
    vector<uint32_t> SliceTwstPrun, SliceFlipPrun, TwstFlipPrun, MCPermPrun, EPermCCombPPrun;
    int TwstFlipPrunMax = 15, SliceTwstPrunMax = 15, SliceFlipPrunMax = 15, MCPermPrunMax = 15,
        EPermCCombPPrunMax = 15;

    Tables();
    void cornConjugate(const Cubie &a, int idx, Cubie &b) const;
    void edgeConjugate(const Cubie &a, int idx, Cubie &b) const;
    int getFlipSym(const Cubie &c) const { return FlipR2S[c.getFlip()]; }
    int getTwstSym(const Cubie &c) const { return TwstR2S[c.getTwst()]; }
    int getEPermSym(const Cubie &c) const { return EPermR2S[getNPerm(c.ea, 8)]; }
    int getCPermSym(const Cubie &c) const { return esym2csym(EPermR2S[getNPerm(c.ca, 8)]); }
    int getPermSymInv(int idx, int sym, bool isCorner) const
    {
        int idxi = PermInvEdgeSym[idx];
        if (isCorner)
            idxi = esym2csym(idxi);
        return (idxi & 0xfff0) | SymMult[idxi & 0xf][sym];
    }
    void urfConjugate(Cubie &c) const
    {
        Cubie temps;
        cornMult(urf2, c, temps);
        cornMult(temps, urf1, c);
        edgeMult(urf2, c, temps);
        edgeMult(temps, urf1, c);
    }

private:
    enum Coord { kFlip = 0, kTwst = 1, kEPerm = 2 };   // (EPERM is an errno macro)
    void initSym2Raw(int nRaw, vector<int> &sym2raw, vector<int> &raw2sym, vector<int> &selfSym, Coord coord);
    int initRawSymPrun(vector<uint32_t> &prun, int nRaw, int nSym, const vector<int> *rawMove,
                       const vector<int> *rawConj, const vector<int> &symMove, const vector<int> &selfSym, int flags) const;
};

void Tables::cornConjugate(const Cubie &a, int idx, Cubie &b) const
{
    const Cubie &sinv = SymCube[SymMultInv[0][idx]];
    const Cubie &s = SymCube[idx];
    for (int corn = 0; corn < 8; corn++) {
        const int oriA = sinv.ca[a.ca[s.ca[corn] & 0xf] & 0xf] >> 4;
        const int oriB = a.ca[s.ca[corn] & 0xf] >> 4;
        const int ori = (oriA < 3) ? oriB : (3 - oriB) % 3;
        b.ca[corn] = (sinv.ca[a.ca[s.ca[corn] & 0xf] & 0xf] & 0xf) | ori << 4;
    }
}

void Tables::edgeConjugate(const Cubie &a, int idx, Cubie &b) const
{
    const Cubie &sinv = SymCube[SymMultInv[0][idx]];
    const Cubie &s = SymCube[idx];
    for (int ed = 0; ed < 12; ed++)
        b.ea[ed] = sinv.ea[a.ea[s.ea[ed] & 0xf] & 0xf] ^ (a.ea[s.ea[ed] & 0xf] & 0x10) ^ (s.ea[ed] & 0x10);
}

void Tables::initSym2Raw(int nRaw, vector<int> &sym2raw, vector<int> &raw2sym, vector<int> &selfSym, Coord coord)
{
    raw2sym.assign(nRaw, -1);   // -1: JS undefined
    sym2raw.assign(nRaw, 0);
    selfSym.assign(nRaw, 0);
    Cubie c, d;
    int count = 0;
    const int symInc = coord >= 2 ? 1 : 2;
    for (int i = 0; i < nRaw; i++) {
        if (raw2sym[i] != -1)
            continue;
        if (coord == kFlip)
            c.setFlip(i);
        else if (coord == kTwst)
            c.setTwst(i);
        else
            c.setEPerm(i);
        for (int s = 0; s < 16; s += symInc) {
            if (coord != kTwst)
                edgeConjugate(c, s, d);
            else
                cornConjugate(c, s, d);
            const int idx = coord == kFlip ? d.getFlip() : coord == kTwst ? d.getTwst() : d.getEPerm();
            if (coord == kFlip)
                FlipS2RF[count << 3 | s >> 1] = idx;
            if (idx == i)
                selfSym[count] |= 1 << (s / symInc);
            raw2sym[idx] = (count << 4 | s) / symInc;
        }
        sym2raw[count++] = i;
    }
    sym2raw.resize(count);
    selfSym.resize(count);
}

int Tables::initRawSymPrun(vector<uint32_t> &prun, int nRaw, int nSym, const vector<int> *rawMove,
                           const vector<int> *rawConj, const vector<int> &symMove, const vector<int> &selfSym, int flags) const
{
    const int SYM_SHIFT = flags & 0xf;
    const int E2C = ((flags >> 4) & 1) == 1 ? 0x00DDDD00 : 0;
    const bool IS_PHASE2 = ((flags >> 5) & 1) == 1;
    const int INV_DEPTH = (flags >> 8) & 0xf;
    const int MAX_DEPTH = (flags >> 12) & 0xf;
    const int SYM_MASK = (1 << SYM_SHIFT) - 1;
    const bool ISTFP = rawMove == nullptr;
    const int N_SIZE = nRaw * nSym;
    const int nMoves = IS_PHASE2 ? 10 : 18;
    const int NEXT_AXIS_MAGIC = nMoves == 10 ? 0x42 : 0x92492;

    prun.assign((N_SIZE >> 3) + 1, 0xffffffffu);
    setPruning(prun, 0, 0 ^ 0xf);
    int depth = 0;
    while (depth < MAX_DEPTH) {   // full init, as min2phase.initFull()
        const bool inv = depth > INV_DEPTH;
        const int select = inv ? 0xf : depth;
        const uint32_t selArrMask = static_cast<uint32_t>(select) * 0x11111111u;
        const int check = inv ? depth : 0xf;
        depth++;
        const int xorVal = depth ^ 0xf;
        uint32_t val = 0;
        for (int i = 0; i < N_SIZE; i++, val >>= 4) {
            if ((i & 7) == 0) {
                val = prun[i >> 3];
                if (!hasZero(val ^ selArrMask)) {
                    i += 7;
                    continue;
                }
            }
            if (static_cast<int>(val & 0xf) != select)
                continue;
            const int raw = i % nRaw;
            const int sym = i / nRaw;
            int flip = 0, fsym = 0;
            if (ISTFP) {
                flip = FlipR2S[raw];
                fsym = flip & 7;
                flip >>= 3;
            }
            for (int m = 0; m < nMoves; m++) {
                int symx = symMove[sym * nMoves + m];
                int rawx;
                if (ISTFP)
                    rawx = FlipS2RF[FlipMove[flip * nMoves + Sym8Move[m << 3 | fsym]] ^ fsym ^ (symx & SYM_MASK)];
                else
                    rawx = (*rawConj)[(*rawMove)[raw * nMoves + m] << SYM_SHIFT | (symx & SYM_MASK)];
                symx >>= SYM_SHIFT;
                const int idx = symx * nRaw + rawx;
                const int p = getPruning(prun, idx);
                if (p != check) {
                    if (p < depth - 1)
                        m += (NEXT_AXIS_MAGIC >> m) & 3;
                    continue;
                }
                if (inv) {
                    setPruning(prun, i, xorVal);
                    break;
                }
                setPruning(prun, idx, xorVal);
                for (int j = 1, ss = selfSym[symx]; (ss >>= 1) != 0; j++) {
                    if ((ss & 1) != 1)
                        continue;
                    int idxx = symx * nRaw;
                    if (ISTFP)
                        idxx += FlipS2RF[FlipR2S[rawx] ^ j];
                    else
                        idxx += (*rawConj)[rawx << SYM_SHIFT | (j ^ ((E2C >> (j << 1)) & 3))];
                    if (getPruning(prun, idxx) == check)
                        setPruning(prun, idxx, xorVal);
                }
            }
        }
    }
    setPruning(prun, N_SIZE, (depth + 1) ^ 0xf);
    return depth + 1;
}

Tables::Tables()
{
    const Basics &b = B();
    moveCube[0].initCoord(15120, 0, 119750400, 0);
    moveCube[3].initCoord(21021, 1494, 323403417, 0);
    moveCube[6].initCoord(8064, 1236, 29441808, 550);
    moveCube[9].initCoord(9, 0, 5880, 0);
    moveCube[12].initCoord(1230, 412, 2949660, 0);
    moveCube[15].initCoord(224, 137, 328552, 137);
    for (int a = 0; a < 18; a += 3)
        for (int p = 0; p < 2; p++) {
            edgeMult(moveCube[a + p], moveCube[a], moveCube[a + p + 1]);
            cornMult(moveCube[a + p], moveCube[a], moveCube[a + p + 1]);
        }
    urf1.initCoord(2531, 1373, 67026819, 1367);
    urf2.initCoord(2089, 1906, 322752913, 2040);

    // initBasic(): symmetry cubes and tables
    Cubie c, d, f2, u4, lr2;
    f2.initCoord(28783, 0, 259268407, 0);
    u4.initCoord(15138, 0, 119765538, 7);
    lr2.initCoord(5167, 0, 83473207, 0);
    for (int i = 0; i < 8; i++)
        lr2.ca[i] |= 3 << 4;
    for (int i = 0; i < 16; i++) {
        SymCube[i] = c;
        cornMultFull(c, u4, d);
        edgeMult(c, u4, d);
        c = d;
        if (i % 4 == 3) {
            cornMultFull(c, lr2, d);
            edgeMult(c, lr2, d);
            c = d;
        }
        if (i % 8 == 7) {
            cornMultFull(c, f2, d);
            edgeMult(c, f2, d);
            c = d;
        }
    }
    for (int i = 0; i < 16; i++)
        for (int j = 0; j < 16; j++) {
            SymMult[i][j] = i ^ j ^ ((0x14ab4 >> j) & (i << 1) & 2);
            SymMultInv[SymMult[i][j]][j] = i;
        }
    c = Cubie();
    for (int s = 0; s < 16; s++)
        for (int j = 0; j < 18; j++) {
            cornConjugate(moveCube[j], SymMultInv[0][s], c);
            for (int m = 0; m < 18; m++) {
                if (moveCube[m].ca != c.ca)
                    continue;
                SymMove[s][j] = m;
                SymMoveUD[s][b.std2ud[j]] = b.std2ud[m];
                break;
            }
            if (s % 2 == 0)
                Sym8Move[j << 3 | s >> 1] = SymMove[s][j];
        }

    FlipS2RF.assign(N_FLIP_SYM * 8, 0);
    initSym2Raw(N_FLIP, FlipS2R, FlipR2S, FlipSelfSym, kFlip);
    initSym2Raw(N_TWST, TwstS2R, TwstR2S, TwstSelfSym, kTwst);
    initSym2Raw(N_PERM, EPermS2R, EPermR2S, PermSelfSym, kEPerm);

    Perm2CombP.assign(N_PERM_SYM, 0);
    PermInvEdgeSym.assign(N_PERM_SYM, 0);
    Cubie cc;
    for (int i = 0; i < N_PERM_SYM; i++) {
        setNPerm(cc.ea, EPermS2R[i], 8);
        Perm2CombP[i] = getComb(cc.ea, 0) + getNParity(EPermS2R[i], 8) * 70;
        c.invFrom(cc);
        PermInvEdgeSym[i] = EPermR2S[c.getEPerm()];
    }

    // coordinate move tables (initSymMoveTable and the raw ones)
    c = Cubie();
    d = Cubie();
    FlipMove.assign(N_FLIP_SYM * N_MOVES, 0);
    for (int i = 0; i < N_FLIP_SYM; i++) {
        c.setFlip(FlipS2R[i]);
        for (int j = 0; j < N_MOVES; j++) {
            edgeMult(c, moveCube[j], d);
            FlipMove[i * N_MOVES + j] = getFlipSym(d);
        }
    }
    TwstMove.assign(N_TWST_SYM * N_MOVES, 0);
    for (int i = 0; i < N_TWST_SYM; i++) {
        c.setTwst(TwstS2R[i]);
        for (int j = 0; j < N_MOVES; j++) {
            cornMult(c, moveCube[j], d);
            TwstMove[i * N_MOVES + j] = getTwstSym(d);
        }
    }
    EPermMove.assign(N_PERM_SYM * N_MOVES2, 0);
    for (int i = 0; i < N_PERM_SYM; i++) {
        c.setEPerm(EPermS2R[i]);
        for (int j = 0; j < N_MOVES2; j++) {
            edgeMult(c, moveCube[ud2std[j]], d);
            EPermMove[i * N_MOVES2 + j] = getEPermSym(d);
        }
    }
    CPermMove.assign(N_PERM_SYM * N_MOVES2, 0);
    for (int i = 0; i < N_PERM_SYM; i++) {
        c.setCPerm(EPermS2R[i]);
        for (int j = 0; j < N_MOVES2; j++) {
            cornMult(c, moveCube[ud2std[j]], d);
            CPermMove[i * N_MOVES2 + j] = getCPermSym(d);
        }
    }
    SliceMove.assign(N_SLICE * N_MOVES, 0);
    SliceConj.assign(N_SLICE * 8, 0);
    for (int i = 0; i < N_SLICE; i++) {
        c.setSlice(i);
        for (int j = 0; j < N_MOVES; j++) {
            edgeMult(c, moveCube[j], d);
            SliceMove[i * N_MOVES + j] = d.getSlice();
        }
        for (int j = 0; j < 16; j += 2) {
            edgeConjugate(c, SymMultInv[0][j], d);
            SliceConj[i << 3 | j >> 1] = d.getSlice();
        }
    }
    MPermMove.assign(N_MPERM * N_MOVES2, 0);
    MPermConj.assign(N_MPERM * 16, 0);
    for (int i = 0; i < N_MPERM; i++) {
        c.setMPerm(i);
        for (int j = 0; j < N_MOVES2; j++) {
            edgeMult(c, moveCube[ud2std[j]], d);
            MPermMove[i * N_MOVES2 + j] = d.getMPerm();
        }
        for (int j = 0; j < 16; j++) {
            edgeConjugate(c, SymMultInv[0][j], d);
            MPermConj[i << 4 | j] = d.getMPerm();
        }
    }
    CCombPMove.assign(N_COMB * N_MOVES2, 0);
    CCombPConj.assign(N_COMB * 16, 0);
    for (int i = 0; i < N_COMB; i++) {
        c.setCComb(i % 70);
        for (int j = 0; j < N_MOVES2; j++) {
            cornMult(c, moveCube[ud2std[j]], d);
            CCombPMove[i * N_MOVES2 + j] = d.getCComb() + 70 * (((0xA5 >> j) & 1) ^ (i / 70));
        }
        for (int j = 0; j < 16; j++) {
            cornConjugate(c, SymMultInv[0][j], d);
            CCombPConj[i << 4 | j] = d.getCComb() + 70 * (i / 70);
        }
    }

    TwstFlipPrunMax = initRawSymPrun(TwstFlipPrun, N_FLIP, N_TWST_SYM, nullptr, nullptr, TwstMove, TwstSelfSym, 0x19603);
    SliceTwstPrunMax = initRawSymPrun(SliceTwstPrun, N_SLICE, N_TWST_SYM, &SliceMove, &SliceConj, TwstMove, TwstSelfSym, 0x69603);
    SliceFlipPrunMax = initRawSymPrun(SliceFlipPrun, N_SLICE, N_FLIP_SYM, &SliceMove, &SliceConj, FlipMove, FlipSelfSym, 0x69603);
    MCPermPrunMax = initRawSymPrun(MCPermPrun, 24, N_PERM_SYM, &MPermMove, &MPermConj, CPermMove, PermSelfSym, 0x8ea34);
    EPermCCombPPrunMax = initRawSymPrun(EPermCCombPPrun, N_COMB, N_PERM_SYM, &CCombPMove, &CCombPConj, EPermMove, PermSelfSym, 0x7d824);
}

const Tables &T()
{
    static const Tables t;   // thread-safe one-time init
    return t;
}

// ---------------------------------------------------------------- search
struct CoordCube {
    int twst = 0, flip = 0, slice = 0, prun = 0, twstc = 0, flipc = 0;

    bool setWithPrun(const Tables &t, const Cubie &cc, int depth)
    {
        twst = t.getTwstSym(cc);
        flip = t.getFlipSym(cc);
        prun = getPruningMax(t.TwstFlipPrunMax, t.TwstFlipPrun, (twst >> 3) << 11 | t.FlipS2RF[flip ^ (twst & 7)]);
        if (prun > depth)
            return false;
        slice = cc.getSlice();
        prun = std::max({ prun,
                          getPruningMax(t.SliceTwstPrunMax, t.SliceTwstPrun, (twst >> 3) * N_SLICE + t.SliceConj[slice << 3 | (twst & 7)]),
                          getPruningMax(t.SliceFlipPrunMax, t.SliceFlipPrun, (flip >> 3) * N_SLICE + t.SliceConj[slice << 3 | (flip & 7)]) });
        if (prun > depth)
            return false;
        Cubie pc;
        t.cornConjugate(cc, 1, pc);
        t.edgeConjugate(cc, 1, pc);
        twstc = t.getTwstSym(pc);
        flipc = t.getFlipSym(pc);
        prun = std::max(prun, getPruningMax(t.TwstFlipPrunMax, t.TwstFlipPrun, (twstc >> 3) << 11 | t.FlipS2RF[flipc ^ (twstc & 7)]));
        return prun <= depth;
    }

    int doMovePrun(const Tables &t, const CoordCube &cc, int m)
    {
        slice = t.SliceMove[cc.slice * N_MOVES + m];
        flip = t.FlipMove[(cc.flip >> 3) * N_MOVES + t.Sym8Move[m << 3 | (cc.flip & 7)]] ^ (cc.flip & 7);
        twst = t.TwstMove[(cc.twst >> 3) * N_MOVES + t.Sym8Move[m << 3 | (cc.twst & 7)]] ^ (cc.twst & 7);
        prun = std::max({ getPruningMax(t.SliceTwstPrunMax, t.SliceTwstPrun, (twst >> 3) * N_SLICE + t.SliceConj[slice << 3 | (twst & 7)]),
                          getPruningMax(t.SliceFlipPrunMax, t.SliceFlipPrun, (flip >> 3) * N_SLICE + t.SliceConj[slice << 3 | (flip & 7)]),
                          getPruningMax(t.TwstFlipPrunMax, t.TwstFlipPrun, (twst >> 3) << 11 | t.FlipS2RF[flip ^ (twst & 7)]) });
        return prun;
    }

    int doMovePrunConj(const Tables &t, const CoordCube &cc, int m)
    {
        m = t.SymMove[3][m];
        flipc = t.FlipMove[(cc.flipc >> 3) * N_MOVES + t.Sym8Move[m << 3 | (cc.flipc & 7)]] ^ (cc.flipc & 7);
        twstc = t.TwstMove[(cc.twstc >> 3) * N_MOVES + t.Sym8Move[m << 3 | (cc.twstc & 7)]] ^ (cc.twstc & 7);
        return getPruningMax(t.TwstFlipPrunMax, t.TwstFlipPrun, (twstc >> 3) << 11 | t.FlipS2RF[flipc ^ (twstc & 7)]);
    }
};

// One Search per solve: all state lives in the object, the shared tables are read-only.
class Search {
public:
    // min2phase Search.solution(); inverse = verbose INVERSE_SOLUTION. Filters: -1 = none.
    std::string solution(const Cubie &cube, int maxDepth, int probeMax, int probeMin, bool inverse,
                         int firstAxisFilter, int lastAxisFilter);

private:
    const Tables &t_ = T();
    const Basics &b_ = B();
    array<int, 64> move_ {};
    vector<int> moveSol_;
    bool haveSol_ = false;
    std::string solStr_;
    array<CoordCube, 32> nodeUD_;
    array<Cubie, 32> phase1Cubie_;
    array<Cubie, MAX_PRE_MOVES + 1> preMoveCubes_;
    array<int, MAX_PRE_MOVES + 1> preMoves_ {};
    array<Cubie, 6> urfCubieCube_;
    int valid1_ = 0, preMoveLen_ = 0, maxPreMoves_ = 0, depth1_ = 0, length1_ = 0, urfIdx_ = 0;
    bool allowShorter_ = false, inverse_ = false;
    int sol_ = 0, probe_ = 0, probeMax_ = 0, probeMin_ = 0;
    array<int, 6> firstFilters_ {}, lastFilters_ {};
    int firstFilter_ = 0, lastFilter_ = 0;

    int phase1PreMoves(int maxl, int lm, const Cubie &cc);
    int initPhase2Pre();
    int initPhase2(const Cubie &phase2Cubie);
    int phase1(const CoordCube &node, int maxl, int lm);
    void appendSolMove(int curMove);
    int phase2(int edge, int esym, int corn, int csym, int mid, int maxl, int depth, int lm);
    std::string solutionToString() const;
};

std::string Search::solution(const Cubie &cube, int maxDepth, int probeMax, int probeMin, bool inverse,
                             int firstAxisFilter, int lastAxisFilter)
{
    sol_ = maxDepth + 1;
    probe_ = 0;
    probeMax_ = probeMax;
    probeMin_ = std::min(probeMin, probeMax);
    inverse_ = inverse;
    haveSol_ = false;
    firstFilters_.fill(0);
    lastFilters_.fill(0);
    for (int i = 0; i < 3; i++) {
        if (firstAxisFilter >= 0) {
            const int sh = (urfMove[(3 - i) % 3][firstAxisFilter * 3] / 3) * 3;
            firstFilters_[i] |= 0xe07 << sh;
            lastFilters_[i + 3] |= 0xe07 << sh;
        }
        if (lastAxisFilter >= 0) {
            const int sh = (urfMove[(3 - i) % 3][lastAxisFilter * 3] / 3) * 3;
            lastFilters_[i] |= 0xe07 << sh;
            firstFilters_[i + 3] |= 0xe07 << sh;
        }
    }
    // initSearch() with conjMask 0 (try the inverse and all three axes)
    maxPreMoves_ = MAX_PRE_MOVES;
    Cubie cc = cube;
    for (int i = 0; i < 6; i++) {
        urfCubieCube_[i] = cc;
        t_.urfConjugate(cc);
        if (i % 3 == 2)
            cc = Cubie().invFrom(cc);
    }
    // search()
    for (length1_ = 0; length1_ < sol_; length1_++)
        for (urfIdx_ = 0; urfIdx_ < 6; urfIdx_++) {
            firstFilter_ = firstFilters_[urfIdx_];
            lastFilter_ = lastFilters_[urfIdx_];
            if (phase1PreMoves(maxPreMoves_, -30, urfCubieCube_[urfIdx_]) == 0)
                return haveSol_ ? solStr_ : "Error 8";
        }
    return haveSol_ ? solStr_ : "Error 7";
}

int Search::phase1PreMoves(int maxl, int lm, const Cubie &cc)
{
    if (maxl == maxPreMoves_ - 1 && (shr(lastFilter_, lm) & 1) != 0)
        return 1;
    preMoveLen_ = maxPreMoves_ - maxl;
    if (preMoveLen_ == 0 || (shr(0x36FB7, lm) & 1) == 0) {
        depth1_ = length1_ - preMoveLen_;
        phase1Cubie_[0] = cc;
        allowShorter_ = depth1_ == MIN_P1LENGTH_PRE && preMoveLen_ != 0;
        if (nodeUD_[depth1_ + 1].setWithPrun(t_, cc, depth1_) && phase1(nodeUD_[depth1_ + 1], depth1_, -1) == 0)
            return 0;
    }
    if (maxl == 0 || preMoveLen_ + MIN_P1LENGTH_PRE >= length1_)
        return 1;
    int skipMoves = 0;
    if (maxl == 1 || preMoveLen_ + 1 + MIN_P1LENGTH_PRE >= length1_)   // last pre move
        skipMoves |= 0x36FB7;
    lm = (lm / 3) * 3;   // JS ~~(lm / 3) * 3: truncation toward zero, as in C++
    for (int m = 0; m < 18; m++) {
        if (m == lm || m == lm - 9 || m == lm + 9) {
            m += 2;
            continue;
        }
        if ((skipMoves & (1 << m)) != 0)
            continue;
        cornMult(t_.moveCube[m], cc, preMoveCubes_[maxl]);
        edgeMult(t_.moveCube[m], cc, preMoveCubes_[maxl]);
        preMoves_[maxPreMoves_ - maxl] = m;
        if (phase1PreMoves(maxl - 1, m, preMoveCubes_[maxl]) == 0)
            return 0;
    }
    return 1;
}

int Search::initPhase2Pre()
{
    if (probe_ >= (haveSol_ ? probeMin_ : probeMax_))
        return 0;
    ++probe_;
    for (int i = valid1_; i < depth1_; i++) {
        cornMult(phase1Cubie_[i], t_.moveCube[move_[i]], phase1Cubie_[i + 1]);
        edgeMult(phase1Cubie_[i], t_.moveCube[move_[i]], phase1Cubie_[i + 1]);
    }
    valid1_ = depth1_;
    int ret = initPhase2(phase1Cubie_[depth1_]);
    if (ret == 0 || preMoveLen_ == 0 || ret == 2)
        return ret;
    const int m = (preMoves_[preMoveLen_ - 1] / 3) * 3 + 1;
    cornMult(t_.moveCube[m], phase1Cubie_[depth1_], phase1Cubie_[depth1_ + 1]);
    edgeMult(t_.moveCube[m], phase1Cubie_[depth1_], phase1Cubie_[depth1_ + 1]);
    preMoves_[preMoveLen_ - 1] += 2 - preMoves_[preMoveLen_ - 1] % 3 * 2;
    ret = initPhase2(phase1Cubie_[depth1_ + 1]);
    preMoves_[preMoveLen_ - 1] += 2 - preMoves_[preMoveLen_ - 1] % 3 * 2;
    return ret;
}

int Search::initPhase2(const Cubie &phase2Cubie)
{
    int p2corn = t_.getCPermSym(phase2Cubie);
    const int p2csym = p2corn & 0xf;
    p2corn >>= 4;
    int p2edge = t_.getEPermSym(phase2Cubie);
    const int p2esym = p2edge & 0xf;
    p2edge >>= 4;
    const int p2mid = phase2Cubie.getMPerm();
    const int prun = std::max(
        getPruningMax(t_.EPermCCombPPrunMax, t_.EPermCCombPPrun,
                      p2edge * N_COMB + t_.CCombPConj[(t_.Perm2CombP[p2corn] & 0xff) << 4 | t_.SymMultInv[p2esym][p2csym]]),
        getPruningMax(t_.MCPermPrunMax, t_.MCPermPrun, p2corn * N_MPERM + t_.MPermConj[p2mid << 4 | p2csym]));
    const int maxDep2 = std::min(MAX_DEPTH2, sol_ - length1_);
    if (prun >= maxDep2)
        return prun > maxDep2 ? 2 : 1;
    int depth2;
    for (depth2 = maxDep2 - 1; depth2 >= prun; depth2--) {
        const int ret = phase2(p2edge, p2esym, p2corn, p2csym, p2mid, depth2, depth1_, 10);
        if (ret < 0)
            break;
        depth2 -= ret;
        moveSol_.clear();
        for (int i = 0; i < depth1_ + depth2; i++)
            appendSolMove(move_[i]);
        for (int i = preMoveLen_ - 1; i >= 0; i--)
            appendSolMove(preMoves_[i]);
        sol_ = static_cast<int>(moveSol_.size());
        solStr_ = solutionToString();
        haveSol_ = true;
    }
    if (depth2 != maxDep2 - 1)   // at least one solution has been found
        return probe_ >= probeMin_ ? 0 : 1;
    return 1;
}

int Search::phase1(const CoordCube &node, int maxl, int lm)
{
    if (maxl == depth1_ - 1 && (shr(firstFilter_, lm) & 1) != 0)
        return 1;
    if (node.prun == 0 && maxl < 5) {
        if (allowShorter_ || maxl == 0) {
            depth1_ -= maxl;
            const int ret = initPhase2Pre();
            depth1_ += maxl;
            return ret;
        }
        return 1;
    }
    for (int axis = 0; axis < 18; axis += 3) {
        if (axis == lm || axis == lm - 9)
            continue;
        for (int power = 0; power < 3; power++) {
            const int m = axis + power;
            int prun = nodeUD_[maxl].doMovePrun(t_, node, m);
            if (prun > maxl)
                break;
            else if (prun == maxl)
                continue;
            prun = nodeUD_[maxl].doMovePrunConj(t_, node, m);
            if (prun > maxl)
                break;
            else if (prun == maxl)
                continue;
            move_[depth1_ - maxl] = m;
            valid1_ = std::min(valid1_, depth1_ - maxl);
            const int ret = phase1(nodeUD_[maxl], maxl - 1, axis);
            if (ret == 0)
                return 0;
            else if (ret == 2)
                break;
        }
    }
    return 1;
}

void Search::appendSolMove(int curMove)
{
    if (moveSol_.empty()) {
        moveSol_.push_back(curMove);
        return;
    }
    const int axisCur = curMove / 3;
    const int axisLast = moveSol_.back() / 3;
    if (axisCur == axisLast) {
        const int pow = (curMove % 3 + moveSol_.back() % 3 + 1) % 4;
        if (pow == 3)
            moveSol_.pop_back();
        else
            moveSol_.back() = axisCur * 3 + pow;
        return;
    }
    const size_t n = moveSol_.size();
    if (n > 1 && axisCur % 3 == axisLast % 3 && axisCur == moveSol_[n - 2] / 3) {
        const int pow = (curMove % 3 + moveSol_[n - 2] % 3 + 1) % 4;
        if (pow == 3) {
            moveSol_[n - 2] = moveSol_[n - 1];
            moveSol_.pop_back();
        } else {
            moveSol_[n - 2] = axisCur * 3 + pow;
        }
        return;
    }
    moveSol_.push_back(curMove);
}

int Search::phase2(int edge, int esym, int corn, int csym, int mid, int maxl, int depth, int lm)
{
    if (depth1_ == 0 && depth == 1 && (shr(firstFilter_, ud2std[lm]) & 1) != 0)
        return -1;
    if (edge == 0 && corn == 0 && mid == 0 && (preMoveLen_ > 0 || (shr(lastFilter_, ud2std[lm]) & 1) == 0))
        return maxl;
    const int moveMask = b_.ckmv2bit[lm];
    for (int m = 0; m < 10; m++) {
        if (((moveMask >> m) & 1) != 0) {
            m += (0x42 >> m) & 3;
            continue;
        }
        const int midx = t_.MPermMove[mid * N_MOVES2 + m];
        int cornx = t_.CPermMove[corn * N_MOVES2 + t_.SymMoveUD[csym][m]];
        const int csymx = t_.SymMult[cornx & 0xf][csym];
        cornx >>= 4;
        if (getPruningMax(t_.MCPermPrunMax, t_.MCPermPrun, cornx * N_MPERM + t_.MPermConj[midx << 4 | csymx]) >= maxl)
            continue;
        int edgex = t_.EPermMove[edge * N_MOVES2 + t_.SymMoveUD[esym][m]];
        const int esymx = t_.SymMult[edgex & 0xf][esym];
        edgex >>= 4;
        if (getPruningMax(t_.EPermCCombPPrunMax, t_.EPermCCombPPrun,
                          edgex * N_COMB + t_.CCombPConj[(t_.Perm2CombP[cornx] & 0xff) << 4 | t_.SymMultInv[esymx][csymx]]) >= maxl)
            continue;
        const int edgei = t_.getPermSymInv(edgex, esymx, false);
        const int corni = t_.getPermSymInv(cornx, csymx, true);
        if (getPruningMax(t_.EPermCCombPPrunMax, t_.EPermCCombPPrun,
                          (edgei >> 4) * N_COMB + t_.CCombPConj[(t_.Perm2CombP[corni >> 4] & 0xff) << 4 | t_.SymMultInv[edgei & 0xf][corni & 0xf]]) >= maxl)
            continue;
        const int ret = phase2(edgex, esymx, cornx, csymx, midx, maxl - 1, depth + 1, m);
        if (ret >= 0) {
            move_[depth] = ud2std[m];
            return ret;
        }
    }
    return -1;
}

std::string Search::solutionToString() const
{
    std::string sb;
    const int urf = inverse_ ? (urfIdx_ + 3) % 6 : urfIdx_;
    const int n = static_cast<int>(moveSol_.size());
    for (int k = 0; k < n; ++k) {
        const int s = urf < 3 ? k : n - 1 - k;
        sb += move2str[urfMove[urf][moveSol_[s]]];
        sb += ' ';
    }
    return sb;
}

// ---------------------------------------------------------------- scramble_333 getAnyScramble
// Only the fully random case (all masks 0xf...f), with mathlib's rn/getNPerm/getNParity.
int mlGetNPerm(const array<int, 12> &arr, int n)
{
    int idx = 0;
    for (int i = 0; i < n - 1; ++i) {
        idx *= n - i;
        for (int j = i + 1; j < n; ++j)
            if (arr[j] < arr[i])
                ++idx;
    }
    return idx;
}

// fixOri(arr, cntU = n, base) with every entry unknown: n-1 random, the last fixes the sum.
int fixOriAll(Rng &rng, array<int, 12> &arr, int n, int base)
{
    int sum = 0, idx = 0;
    for (int i = 0; i < n - 1; i++) {
        arr[i] = rng.rn(base);
        sum += arr[i];
        idx = idx * base + arr[i];
    }
    arr[n - 1] = ((base << 4) - sum) % base;
    return idx;
}

// fixPerm(arr, cntU = n, parity) with every entry unknown.
int fixPermAll(Rng &rng, array<int, 12> &arr, int n, int parity)
{
    array<int, 12> val = { { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 } };
    int last = 0, i = 0;
    for (int cnt = n; i < n && cnt > 0; i++) {
        const int r = rng.rn(cnt);
        arr[i] = val[r];
        for (int j = r; j < 11; j++)
            val[j] = val[j + 1];
        if (cnt-- == 2)
            last = i;
    }
    if (getNParity(mlGetNPerm(arr, n), n) == 1 - parity)
        std::swap(arr[i - 1], arr[last]);
    return mlGetNPerm(arr, n);
}

std::string anyScramble(Rng &rng, int firstAxisFilter, int lastAxisFilter)
{
    std::string solution;
    do {
        array<int, 12> eo {}, ep {}, co {}, cp {};
        const int neo = fixOriAll(rng, eo, 12, 2);
        const int nco = fixOriAll(rng, co, 8, 3);
        const int nep = fixPermAll(rng, ep, 12, -1);
        const int ncp = fixPermAll(rng, cp, 8, getNParity(nep, 12));
        if (ncp + nco + nep + neo == 0)
            continue;   // solved: draw again (the loop condition sees the old solution)
        rng.rn(1);      // rndEl(_rndpre) over [[]]
        rng.rn(1);      // rndEl(_rndapp)
        Cubie cc;
        for (int i = 0; i < 12; i++) {
            cc.ea[i] = ep[i] | eo[i] << 4;
            if (i < 8)
                cc.ca[i] = cp[i] | co[i] << 4;
        }
        // search.solution(posit, 21, 1e9, 50, 2, lastAxisFilter, firstAxisFilter)
        Search search;
        solution = search.solution(cc, 21, 1000000000, 50, true, lastAxisFilter, firstAxisFilter);
    } while (solution.size() <= 3);
    return solution;   // whitespace is normalized by the caller
}

} // namespace

std::string gen333(Rng &rng, const std::string &eventId)
{
    if (eventId == "333" || eventId == "333oh")
        return anyScramble(rng, -1, -1);
    if (eventId == "333fm")
        return "R' U' F " + anyScramble(rng, 2, 1) + "R' U' F";
    if (eventId == "333bld") {
        // csTimer '333ni': '${333}#{[[""]],["","Rw ","Rw2 ","Rw' ","Fw ","Fw' "],1}#{[[""]],["","Uw","Uw2","Uw'"],1}'
        // Each mega([[""]], suffixes, 1) draws rn(1), rn(1), then the suffix.
        static const std::vector<std::string> kRotX = { "", "Rw ", "Rw2 ", "Rw' ", "Fw ", "Fw' " };
        static const std::vector<std::string> kRotY = { "", "Uw", "Uw2", "Uw'" };
        std::string s = anyScramble(rng, -1, -1);
        rng.rn(1);
        rng.rn(1);
        s += rng.rndEl(kRotX);
        rng.rn(1);
        rng.rn(1);
        s += rng.rndEl(kRotY);
        return s;
    }
    return std::string();
}

bool gen333Supported(const std::string &eventId)
{
    return eventId == "333" || eventId == "333oh" || eventId == "333bld" || eventId == "333fm";
}

} // namespace ct
