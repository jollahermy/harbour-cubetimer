// 4x4x4 (444, 444bld): native port of csTimer's random-state scrambler scramble/scramble_444.js
// (three-phase reduction by Chen Shuang, GWT-compiled to JS), csTimer commit 2547d82, GPL-3.0.
// The final 3x3 stage uses the min2phase port in s333.cpp (solveFacelet333).
//
// Follows the JS step by step, including the priority queue and the stable sorts that decide
// which phase-1/phase-2 candidates are tried first, so the same seed gives the same scramble.
// Center1Raw2Sym (only needed while building the Center1 move table) is a local of the table
// builder instead of a module variable that is reset to null afterwards.
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

const int PHASE1_SOLS = 10000, PHASE2_ATTS = 500, PHASE2_SOLS = 100, MAX_SEARCH_DEPTH = 60;
const int EDGE3_MAX_PRUN = 10;
const int N_C1SYM = 15582;

inline int bitCount(int x) { return __builtin_popcount(static_cast<unsigned>(x)); }

// mathlib.circle(arr, a, b, c, d): arr[a] <- arr[d] <- arr[c] <- arr[b] <- arr[a]
template <class A> inline void circle4(A &arr, int a, int b, int c, int d)
{
    const int temp = arr[d];
    arr[d] = arr[c];
    arr[c] = arr[b];
    arr[b] = arr[a];
    arr[a] = temp;
}
template <class A> inline void circle2(A &arr, int a, int b) { std::swap(arr[a], arr[b]); }

// swap(arr, a, b, c, d, key): key 0 = cycle forward, 1 = double swap, 2 = cycle backward
// always_inline: lets GCC see each array size (otherwise identical-code folding of the 8/16/24
// instantiations triggers false -Warray-bounds reports)
template <size_t N> __attribute__((always_inline)) inline void swap4(array<int, N> &arr, int a, int b, int c, int d, int key)
{
    int temp;
    switch (key) {
    case 0:
        temp = arr[d];
        arr[d] = arr[c];
        arr[c] = arr[b];
        arr[b] = arr[a];
        arr[a] = temp;
        return;
    case 1:
        temp = arr[a];
        arr[a] = arr[c];
        arr[c] = temp;
        temp = arr[b];
        arr[b] = arr[d];
        arr[d] = temp;
        return;
    case 2:
        temp = arr[a];
        arr[a] = arr[b];
        arr[b] = arr[c];
        arr[c] = arr[d];
        arr[d] = temp;
        return;
    }
}

template <class A> int parity0(const A &arr, int n)
{
    int parity = 0, mask = 0;
    for (int i = 0; i < n; i++) {
        const int val = arr[i];
        parity ^= val - bitCount(mask & ((1 << val) - 1));
        mask |= 1 << val;
    }
    return parity & 1;
}

const array<int, 24> centerFacelet = { { 5, 6, 10, 9, 53, 54, 58, 57, 37, 38, 42, 41, 85, 86, 90, 89, 21, 22, 26, 25, 69, 70, 74, 73 } };
const int cornerFacelet[8][3] = { { 15, 16, 35 }, { 12, 32, 67 }, { 0, 64, 83 }, { 3, 80, 19 },
                                  { 51, 47, 28 }, { 48, 79, 44 }, { 60, 95, 76 }, { 63, 31, 92 } };
const int edgeFacelet[24][2] = { { 13, 33 }, { 4, 65 }, { 2, 81 }, { 11, 17 }, { 61, 94 }, { 52, 78 },
                                 { 50, 46 }, { 59, 30 }, { 75, 40 }, { 68, 87 }, { 27, 88 }, { 20, 39 },
                                 { 34, 14 }, { 66, 8 }, { 82, 1 }, { 18, 7 }, { 93, 62 }, { 77, 56 },
                                 { 45, 49 }, { 29, 55 }, { 36, 71 }, { 91, 72 }, { 84, 23 }, { 43, 24 } };
const char *const move2str1[36] = { "U  ", "U2 ", "U' ", "R  ", "R2 ", "R' ", "F  ", "F2 ", "F' ",
                                    "D  ", "D2 ", "D' ", "L  ", "L2 ", "L' ", "B  ", "B2 ", "B' ",
                                    "Uw ", "Uw2", "Uw'", "Rw ", "Rw2", "Rw'", "Fw ", "Fw2", "Fw'",
                                    "Dw ", "Dw2", "Dw'", "Lw ", "Lw2", "Lw'", "Bw ", "Bw2", "Bw'" };
const array<int, 29> move2std = { { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 19, 21, 22, 23, 25, 28, 30, 31, 32, 34, 36 } };
const array<int, 21> move3std = { { 0, 1, 2, 4, 6, 7, 8, 9, 10, 11, 13, 15, 16, 17, 19, 22, 25, 28, 31, 34, 36 } };
const array<int, 9> move2rot = { { 35, 1, 34, 2, 4, 6, 22, 5, 19 } };
const array<int, 36> pmv = { { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0 } };
const array<int, 20> pmove = { { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1 } };
const array<int, 12> rl2std = { { 0, 9, 14, 23, 27, 28, 41, 42, 46, 55, 60, 69 } };
const array<int, 8> syminv0 = { { 0, 1, 6, 3, 4, 5, 2, 7 } };
const array<int, 13> factX = { { 1, 1, 1, 3, 12, 60, 360, 2520, 20160, 181440, 1814400, 19958400, 239500800 } };
const array<int, 12> FullEdgeMap = { { 0, 2, 4, 6, 1, 3, 7, 5, 8, 9, 10, 11 } };

struct CnkTable {
    array<array<int, 32>, 32> c {};
    CnkTable()
    {
        for (int i = 0; i < 32; ++i) {
            c[i][0] = c[i][i] = 1;
            for (int j = 1; j < i; ++j)
                c[i][j] = c[i - 1][j - 1] + c[i - 1][j];
        }
    }
};
const array<array<int, 32>, 32> &Cnk()
{
    static const CnkTable t;
    return t.c;
}

// ---------------------------------------------------------------- pieces
struct Center1 {
    array<int, 24> ct {};
    Center1()
    {
        for (int i = 0; i < 24; ++i)
            ct[i] = i < 8 ? 1 : 0;
    }
};

void doMoveCenterCube(array<int, 24> &ct, int m)
{
    const int key = m % 3;
    switch (m / 3) {
    case 6: swap4(ct, 8, 20, 12, 16, key); swap4(ct, 9, 21, 13, 17, key); // fallthrough
    case 0: swap4(ct, 0, 1, 2, 3, key); break;
    case 7: swap4(ct, 1, 15, 5, 9, key); swap4(ct, 2, 12, 6, 10, key); // fallthrough
    case 1: swap4(ct, 16, 17, 18, 19, key); break;
    case 8: swap4(ct, 2, 19, 4, 21, key); swap4(ct, 3, 16, 5, 22, key); // fallthrough
    case 2: swap4(ct, 8, 9, 10, 11, key); break;
    case 9: swap4(ct, 10, 18, 14, 22, key); swap4(ct, 11, 19, 15, 23, key); // fallthrough
    case 3: swap4(ct, 4, 5, 6, 7, key); break;
    case 10: swap4(ct, 0, 8, 4, 14, key); swap4(ct, 3, 11, 7, 13, key); // fallthrough
    case 4: swap4(ct, 20, 21, 22, 23, key); break;
    case 11: swap4(ct, 1, 20, 7, 18, key); swap4(ct, 0, 23, 6, 17, key); // fallthrough
    case 5: swap4(ct, 12, 13, 14, 15, key); break;
    }
}

void c1rot(Center1 &c, int r)
{
    switch (r) {
    case 0: doMoveCenterCube(c.ct, 19); doMoveCenterCube(c.ct, 28); break;
    case 1: doMoveCenterCube(c.ct, 21); doMoveCenterCube(c.ct, 32); break;
    case 2:
        swap4(c.ct, 0, 3, 1, 2, 1); swap4(c.ct, 8, 11, 9, 10, 1); swap4(c.ct, 4, 7, 5, 6, 1);
        swap4(c.ct, 12, 15, 13, 14, 1); swap4(c.ct, 16, 19, 21, 22, 1); swap4(c.ct, 17, 18, 20, 23, 1);
        break;
    case 3:
        doMoveCenterCube(c.ct, 18); doMoveCenterCube(c.ct, 29); doMoveCenterCube(c.ct, 24); doMoveCenterCube(c.ct, 35);
        break;
    }
}

// one step of the 48-symmetry walk used everywhere: rot0, (j odd) rot1, (j%8==7) rot2, (j%16==15) rot3
void c1step(Center1 &c, int j)
{
    c1rot(c, 0);
    if (j % 2 == 1) c1rot(c, 1);
    if (j % 8 == 7) c1rot(c, 2);
    if (j % 16 == 15) c1rot(c, 3);
}

int c1get(const Center1 &c)
{
    int idx = 0, r = 8;
    for (int i = 23; i >= 0; --i)
        if (c.ct[i] == 1)
            idx += Cnk()[i][r--];
    return idx;
}

void c1set(Center1 &c, int idx)
{
    int r = 8;
    for (int i = 23; i >= 0; --i) {
        c.ct[i] = 0;
        if (idx >= Cnk()[i][r]) {
            idx -= Cnk()[i][r--];
            c.ct[i] = 1;
        }
    }
}

int c1rotThres(const Center1 &c, const array<int, 24> &rotPerm, int thres)
{
    int idx = 0, r = 8;
    for (int i = 23; i >= 0; --i) {
        if (c.ct[rotPerm[i]] == 1)
            idx += Cnk()[i][r--];
        if (idx >= thres)
            return -1;
    }
    return idx;
}

struct Center2 {
    array<int, 8> rl {};
    array<int, 16> ct {};
    int parity = 0;
};

int c2getct(const Center2 &c)
{
    int idx = 0, r = 8;
    for (int i = 14; i >= 0; --i)
        if (c.ct[i] != c.ct[15])
            idx += Cnk()[i][r--];
    return idx;
}

int c2getrl(const Center2 &c)
{
    int idx = 0, r = 4;
    for (int i = 6; i >= 0; --i)
        if (c.rl[i] != c.rl[7])
            idx += Cnk()[i][r--];
    return idx * 2 + c.parity;
}

void c2move(Center2 &c, int m)
{
    c.parity ^= pmv[m];
    const int key = m % 3;
    switch (m / 3) {
    case 0: swap4(c.ct, 0, 1, 2, 3, key); break;
    case 1: swap4(c.rl, 0, 1, 2, 3, key); break;
    case 2: swap4(c.ct, 8, 9, 10, 11, key); break;
    case 3: swap4(c.ct, 4, 5, 6, 7, key); break;
    case 4: swap4(c.rl, 4, 5, 6, 7, key); break;
    case 5: swap4(c.ct, 12, 13, 14, 15, key); break;
    case 6: swap4(c.ct, 0, 1, 2, 3, key); swap4(c.rl, 0, 5, 4, 1, key); swap4(c.ct, 8, 9, 12, 13, key); break;
    case 7: swap4(c.rl, 0, 1, 2, 3, key); swap4(c.ct, 1, 15, 5, 9, key); swap4(c.ct, 2, 12, 6, 10, key); break;
    case 8: swap4(c.ct, 8, 9, 10, 11, key); swap4(c.rl, 0, 3, 6, 5, key); swap4(c.ct, 3, 2, 5, 4, key); break;
    case 9: swap4(c.ct, 4, 5, 6, 7, key); swap4(c.rl, 3, 2, 7, 6, key); swap4(c.ct, 11, 10, 15, 14, key); break;
    case 10: swap4(c.rl, 4, 5, 6, 7, key); swap4(c.ct, 0, 8, 4, 14, key); swap4(c.ct, 3, 11, 7, 13, key); break;
    case 11: swap4(c.ct, 12, 13, 14, 15, key); swap4(c.rl, 1, 4, 7, 2, key); swap4(c.ct, 1, 0, 7, 6, key); break;
    }
}

void c2rot(Center2 &c, int r)
{
    switch (r) {
    case 0: c2move(c, 19); c2move(c, 28); break;
    case 1: c2move(c, 21); c2move(c, 32); break;
    case 2:
        swap4(c.ct, 0, 3, 1, 2, 1); swap4(c.ct, 8, 11, 9, 10, 1); swap4(c.ct, 4, 7, 5, 6, 1);
        swap4(c.ct, 12, 15, 13, 14, 1); swap4(c.rl, 0, 3, 5, 6, 1); swap4(c.rl, 1, 2, 4, 7, 1);
        break;
    }
}

void c2set(Center2 &c, const array<int, 24> &ct, int edgeParity)
{
    for (int i = 0; i < 16; ++i)
        c.ct[i] = ct[i] % 3;
    for (int i = 0; i < 8; ++i)
        c.rl[i] = ct[i + 16];
    c.parity = edgeParity;
}

void c2setct(Center2 &c, int idx)
{
    int r = 8;
    c.ct[15] = 0;
    for (int i = 14; i >= 0; --i) {
        if (idx >= Cnk()[i][r]) {
            idx -= Cnk()[i][r--];
            c.ct[i] = 1;
        } else {
            c.ct[i] = 0;
        }
    }
}

void c2setrl(Center2 &c, int idx)
{
    c.parity = idx & 1;
    idx >>= 1;
    int r = 4;
    c.rl[7] = 0;
    for (int i = 6; i >= 0; --i) {
        if (idx >= Cnk()[i][r]) {
            idx -= Cnk()[i][r--];
            c.rl[i] = 1;
        } else {
            c.rl[i] = 0;
        }
    }
}

struct Center3 {
    array<int, 8> ud {}, rl {}, fb {};
    int parity = 0;
};

void c3move(Center3 &c, int i)
{
    c.parity ^= pmove[i];
    switch (i) {
    case 0: case 1: case 2: swap4(c.ud, 0, 1, 2, 3, i % 3); break;
    case 3: swap4(c.rl, 0, 1, 2, 3, 1); break;
    case 4: case 5: case 6: swap4(c.fb, 0, 1, 2, 3, (i - 1) % 3); break;
    case 7: case 8: case 9: swap4(c.ud, 4, 5, 6, 7, (i - 1) % 3); break;
    case 10: swap4(c.rl, 4, 5, 6, 7, 1); break;
    case 11: case 12: case 13: swap4(c.fb, 4, 5, 6, 7, (i + 1) % 3); break;
    case 14: swap4(c.ud, 0, 1, 2, 3, 1); swap4(c.rl, 0, 5, 4, 1, 1); swap4(c.fb, 0, 5, 4, 1, 1); break;
    case 15: swap4(c.rl, 0, 1, 2, 3, 1); swap4(c.fb, 1, 4, 7, 2, 1); swap4(c.ud, 1, 6, 5, 2, 1); break;
    case 16: swap4(c.fb, 0, 1, 2, 3, 1); swap4(c.ud, 3, 2, 5, 4, 1); swap4(c.rl, 0, 3, 6, 5, 1); break;
    case 17: swap4(c.ud, 4, 5, 6, 7, 1); swap4(c.rl, 3, 2, 7, 6, 1); swap4(c.fb, 3, 2, 7, 6, 1); break;
    case 18: swap4(c.rl, 4, 5, 6, 7, 1); swap4(c.fb, 0, 3, 6, 5, 1); swap4(c.ud, 0, 3, 4, 7, 1); break;
    case 19: swap4(c.fb, 4, 5, 6, 7, 1); swap4(c.ud, 0, 7, 6, 1, 1); swap4(c.rl, 1, 4, 7, 2, 1); break;
    }
}

void c3set(Center3 &c, const array<int, 24> &ct, int eXcParity)
{
    const int parity = ((ct[0] % 3 > ct[8] % 3) ^ (ct[8] % 3 > ct[16] % 3) ^ (ct[0] % 3 > ct[16] % 3)) ? 0 : 1;
    for (int i = 0; i < 8; ++i) {
        c.ud[i] = (ct[i] / 3) ^ 1;
        c.fb[i] = (ct[i + 8] / 3) ^ 1;
        c.rl[i] = (ct[i + 16] / 3) ^ 1 ^ parity;
    }
    c.parity = parity ^ eXcParity;
}

void c3setct(Center3 &c, int idx)
{
    c.parity = idx & 1;
    idx >>= 1;
    int idxrl = rl2std[idx % 12];
    idx /= 12;
    int r = 4;
    for (int i = 7; i >= 0; --i) {
        c.rl[i] = 0;
        if (idxrl >= Cnk()[i][r]) {
            idxrl -= Cnk()[i][r--];
            c.rl[i] = 1;
        }
    }
    int idxfb = idx % 35;
    idx /= 35;
    r = 4;
    c.fb[7] = 0;
    for (int i = 6; i >= 0; --i) {
        if (idxfb >= Cnk()[i][r]) {
            idxfb -= Cnk()[i][r--];
            c.fb[i] = 1;
        } else {
            c.fb[i] = 0;
        }
    }
    r = 4;
    c.ud[7] = 0;
    for (int i = 6; i >= 0; --i) {
        if (idx >= Cnk()[i][r]) {
            idx -= Cnk()[i][r--];
            c.ud[i] = 1;
        } else {
            c.ud[i] = 0;
        }
    }
}

struct Corner {
    array<int, 8> cp = { { 0, 1, 2, 3, 4, 5, 6, 7 } };
    array<int, 8> co {};
};

void cornMult0(const Corner &a, const Corner &b, Corner &prod)
{
    for (int corn = 0; corn < 8; ++corn) {
        prod.cp[corn] = a.cp[b.cp[corn]];
        const int oriA = a.co[b.cp[corn]];
        const int oriB = b.co[corn];
        int ori = oriA + (oriA < 3 ? oriB : 6 - oriB);
        ori = ori % 3;
        if ((oriA >= 3) != (oriB >= 3))
            ori += 3;
        prod.co[corn] = ori;
    }
}

struct Edge3 {
    array<int, 12> edge {}, edgeo {}, temp {};
    bool isStd = true;
};

struct EdgeCube {
    array<int, 24> ep {};
    EdgeCube()
    {
        for (int i = 0; i < 24; ++i)
            ep[i] = i;
    }
};

void doMoveEdge(array<int, 24> &ep, int m)
{
    const int key = m % 3;
    switch (m / 3) {
    case 6: swap4(ep, 9, 22, 11, 20, key); // fallthrough
    case 0: swap4(ep, 0, 1, 2, 3, key); swap4(ep, 12, 13, 14, 15, key); break;
    case 7: swap4(ep, 2, 16, 6, 12, key); // fallthrough
    case 1: swap4(ep, 11, 15, 10, 19, key); swap4(ep, 23, 3, 22, 7, key); break;
    case 8: swap4(ep, 3, 19, 5, 13, key); // fallthrough
    case 2: swap4(ep, 0, 11, 6, 8, key); swap4(ep, 12, 23, 18, 20, key); break;
    case 9: swap4(ep, 8, 23, 10, 21, key); // fallthrough
    case 3: swap4(ep, 4, 5, 6, 7, key); swap4(ep, 16, 17, 18, 19, key); break;
    case 10: swap4(ep, 14, 0, 18, 4, key); // fallthrough
    case 4: swap4(ep, 1, 20, 5, 21, key); swap4(ep, 13, 8, 17, 9, key); break;
    case 11: swap4(ep, 7, 15, 1, 17, key); // fallthrough
    case 5: swap4(ep, 2, 9, 4, 10, key); swap4(ep, 14, 21, 16, 22, key); break;
    }
}

// ---------------------------------------------------------------- tables
struct Tables {
    // Moves
    array<int, 37> std2move {}, std3move {};
    array<array<bool, 36>, 37> ckmv {};
    array<array<bool, 28>, 29> ckmv2 {};
    array<array<bool, 20>, 21> ckmv3 {};
    array<int, 36> skipAxis {};
    array<int, 28> skipAxis2 {};
    array<int, 20> skipAxis3 {};
    array<array<int, 24>, 36> epMoveMap {};
    // Center1
    vector<array<int, 36>> Center1SymMove;
    vector<int> Center1Sym2Raw, Center1SymPrun;
    array<array<int, 48>, 48> SymMult {};
    array<array<int, 36>, 48> SymMove {};
    array<int, 48> SymInv {}, finish {};
    array<array<int, 24>, 48> Center1RotPerm {};
    // Center2
    vector<array<int, 28>> rlmv, ctmv;
    vector<array<int, 16>> rlrot, ctrot;
    vector<int8_t> ctprun;
    // Center3
    vector<array<int, 20>> ctmove;
    vector<int8_t> prun3;
    array<int, 70> std2rl {};
    // Edge3
    vector<uint32_t> Edge3Prun;
    vector<int> Edge3Sym2Raw, Edge3Sym2Mask, symstate, Edge3Raw2Sym;
    array<array<int, 12>, 168> mvrot {}, mvroto {};
    array<vector<int>, 20> Edge3SymMove;
    // Corners
    array<Corner, 18> CornerMoveCube;

    Tables();

    int getMvSym(const array<int, 12> &ep, int mv, int assumeIdx = -1) const;
    int getPruning(int index) const { return static_cast<int>((Edge3Prun[index >> 4] >> ((index & 15) << 1)) & 3); }
    int getprun(int edge) const;
    int getprun0(int edge, int prun) const
    {
        const int depm3 = getPruning(edge);
        if (depm3 == 3)
            return EDGE3_MAX_PRUN;
        return static_cast<int>(((0x49249249u << depm3) >> prun) & 3) + prun - 1;
    }
    int c1getsym(Center1 c) const;        // runtime path (Center1Raw2Sym == null)
    int c3getct(const Center3 &c) const;
    void e3move(Edge3 &e, int i) const;

private:
    void setPruning(int index, int value) { Edge3Prun[index >> 4] ^= static_cast<uint32_t>(3 ^ value) << ((index & 15) << 1); }
    void initMoves();
    void initSymMeta();
    void initCenter1(vector<int> &raw2sym);
    void initCenter2();
    void initCenter3();
    void initEdge3();
    void initEdge3Prun();
};

const Tables &T();

// Edge3 helpers that do not need tables
void e3circlex(Edge3 &e, int a, int b, int c, int d)
{
    const int temp = e.edgeo[d];
    e.edgeo[d] = e.edge[c];
    e.edge[c] = e.edgeo[b];
    e.edgeo[b] = e.edge[a];
    e.edge[a] = temp;
}

void e3swapx(Edge3 &e, int x, int y)
{
    const int temp = e.edge[x];
    e.edge[x] = e.edgeo[y];
    e.edgeo[y] = temp;
}

void e3std(Edge3 &e)
{
    for (int i = 0; i < 12; ++i)
        e.temp[e.edgeo[i]] = i;
    for (int i = 0; i < 12; ++i) {
        e.edge[i] = e.temp[e.edge[i]];
        e.edgeo[i] = i;
    }
    e.isStd = true;
}

int get12Perm(const array<int, 12> &arr, int end, bool returnMask)
{
    int idx = 0, mask = 0;
    for (int i = 0; i < end; i++) {
        const int val = arr[i];
        idx = idx * (12 - i) + val - bitCount(mask & ((1 << val) - 1));
        mask |= 1 << val;
    }
    return returnMask ? mask : idx;
}

int e3get(Edge3 &e, int end, bool returnMask = false)
{
    if (!e.isStd)
        e3std(e);
    return get12Perm(e.edge, end, returnMask);
}

void e3set(Edge3 &e, int idx)
{
    uint32_t vall = 0x76543210u, valh = 0xba98u;
    int parity = 0;
    for (int i = 0; i < 11; ++i) {
        const int p = factX[11 - i];
        int v = idx / p;
        idx %= p;
        parity ^= v;
        v <<= 2;
        if (v >= 32) {
            v -= 32;
            e.edge[i] = static_cast<int>((valh >> v) & 15);
            const uint32_t m = (1u << v) - 1;
            valh = (valh & m) + ((valh >> 4) & ~m);
        } else {
            e.edge[i] = static_cast<int>((vall >> v) & 15);
            const uint32_t m = (1u << v) - 1;
            vall = (vall & m) + ((vall >> 4) & ~m) + (valh << 28);
            valh >>= 4;
        }
    }
    if ((parity & 1) == 0) {
        e.edge[11] = static_cast<int>(vall);
    } else {
        e.edge[11] = e.edge[10];
        e.edge[10] = static_cast<int>(vall);
    }
    for (int i = 0; i < 12; ++i)
        e.edgeo[i] = i;
    e.isStd = true;
}

// $set_6: from a full edge cube (24 wings); returns the parity
int e3setFromCube(Edge3 &e, const array<int, 24> &ep)
{
    for (int i = 0; i < 12; ++i) {
        e.temp[i] = i;
        e.edge[i] = ep[FullEdgeMap[i] + 12] % 12;
    }
    int parity = 1;
    for (int i = 0; i < 12; ++i)
        while (e.edge[i] != i) {
            const int t = e.edge[i];
            e.edge[i] = e.edge[t];
            e.edge[t] = t;
            std::swap(e.temp[i], e.temp[t]);
            parity ^= 1;
        }
    for (int i = 0; i < 12; ++i)
        e.edge[i] = e.temp[ep[FullEdgeMap[i]] % 12];
    return parity;
}

void Tables::e3move(Edge3 &e, int i) const
{
    e.isStd = false;
    switch (i) {
    case 0: circle4(e.edge, 0, 4, 1, 5); circle4(e.edgeo, 0, 4, 1, 5); break;
    case 1: swap4(e.edge, 0, 4, 1, 5, 1); swap4(e.edgeo, 0, 4, 1, 5, 1); break;
    case 2: circle4(e.edge, 0, 5, 1, 4); circle4(e.edgeo, 0, 5, 1, 4); break;
    case 3: swap4(e.edge, 5, 10, 6, 11, 1); swap4(e.edgeo, 5, 10, 6, 11, 1); break;
    case 4: circle4(e.edge, 0, 11, 3, 8); circle4(e.edgeo, 0, 11, 3, 8); break;
    case 5: swap4(e.edge, 0, 11, 3, 8, 1); swap4(e.edgeo, 0, 11, 3, 8, 1); break;
    case 6: circle4(e.edge, 0, 8, 3, 11); circle4(e.edgeo, 0, 8, 3, 11); break;
    case 7: circle4(e.edge, 2, 7, 3, 6); circle4(e.edgeo, 2, 7, 3, 6); break;
    case 8: swap4(e.edge, 2, 7, 3, 6, 1); swap4(e.edgeo, 2, 7, 3, 6, 1); break;
    case 9: circle4(e.edge, 2, 6, 3, 7); circle4(e.edgeo, 2, 6, 3, 7); break;
    case 10: swap4(e.edge, 4, 8, 7, 9, 1); swap4(e.edgeo, 4, 8, 7, 9, 1); break;
    case 11: circle4(e.edge, 1, 9, 2, 10); circle4(e.edgeo, 1, 9, 2, 10); break;
    case 12: swap4(e.edge, 1, 9, 2, 10, 1); swap4(e.edgeo, 1, 9, 2, 10, 1); break;
    case 13: circle4(e.edge, 1, 10, 2, 9); circle4(e.edgeo, 1, 10, 2, 9); break;
    case 14: swap4(e.edge, 0, 4, 1, 5, 1); swap4(e.edgeo, 0, 4, 1, 5, 1); circle2(e.edge, 9, 11); circle2(e.edgeo, 8, 10); break;
    case 15: swap4(e.edge, 5, 10, 6, 11, 1); swap4(e.edgeo, 5, 10, 6, 11, 1); circle2(e.edge, 1, 3); circle2(e.edgeo, 0, 2); break;
    case 16: swap4(e.edge, 0, 11, 3, 8, 1); swap4(e.edgeo, 0, 11, 3, 8, 1); circle2(e.edge, 5, 7); circle2(e.edgeo, 4, 6); break;
    case 17: swap4(e.edge, 2, 7, 3, 6, 1); swap4(e.edgeo, 2, 7, 3, 6, 1); circle2(e.edge, 8, 10); circle2(e.edgeo, 9, 11); break;
    case 18: swap4(e.edge, 4, 8, 7, 9, 1); swap4(e.edgeo, 4, 8, 7, 9, 1); circle2(e.edge, 0, 2); circle2(e.edgeo, 1, 3); break;
    case 19: swap4(e.edge, 1, 9, 2, 10, 1); swap4(e.edgeo, 1, 9, 2, 10, 1); circle2(e.edge, 4, 6); circle2(e.edgeo, 5, 7); break;
    }
}

void e3rot(const Tables &t, Edge3 &e, int r)
{
    e.isStd = false;
    switch (r) {
    case 0: t.e3move(e, 14); t.e3move(e, 17); break;
    case 1:
        e3circlex(e, 11, 5, 10, 6); e3circlex(e, 5, 10, 6, 11); e3circlex(e, 1, 2, 3, 0);
        e3circlex(e, 4, 9, 7, 8); e3circlex(e, 8, 4, 9, 7); e3circlex(e, 0, 1, 2, 3);
        break;
    case 2:
        e3swapx(e, 4, 5); e3swapx(e, 5, 4); e3swapx(e, 11, 8); e3swapx(e, 8, 11); e3swapx(e, 7, 6); e3swapx(e, 6, 7);
        e3swapx(e, 9, 10); e3swapx(e, 10, 9); e3swapx(e, 1, 1); e3swapx(e, 0, 0); e3swapx(e, 3, 3); e3swapx(e, 2, 2);
        break;
    }
}

void e3rotate(const Tables &t, Edge3 &e, int r)
{
    while (r >= 2) {
        r -= 2;
        e3rot(t, e, 1);
        e3rot(t, e, 2);
    }
    if (r != 0)
        e3rot(t, e, 0);
}

int Tables::getMvSym(const array<int, 12> &ep, int mv, int assumeIdx) const
{
    int mrIdx = mv << 3;
    int idx = 0, mask = 0;
    if (assumeIdx >= 0 && move3std[mv] % 3 == 1) {
        mrIdx |= assumeIdx & 0x7;
        idx = assumeIdx >> 3;
    } else {
        const array<int, 12> &movo = mvroto[mrIdx], &mov = mvrot[mrIdx];
        for (int i = 0; i < 4; i++) {
            const int val = movo[ep[mov[i]]];
            idx = idx * (12 - i) + val - bitCount(mask & ((1 << val) - 1));
            mask |= 1 << val;
        }
        idx = Edge3Raw2Sym[idx];
        mrIdx |= idx & 7;
        idx >>= 3;
    }
    const array<int, 12> &movo = mvroto[mrIdx], &mov = mvrot[mrIdx];
    mask = Edge3Sym2Mask[idx];
    for (int i = 4; i < 10; i++) {
        const int val = movo[ep[mov[i]]];
        idx = idx * (12 - i) + val - bitCount(mask & ((1 << val) - 1));
        mask |= 1 << val;
    }
    return idx << 3 | (mrIdx & 0x7);
}

int Tables::getprun(int edge) const
{
    Edge3 e;
    int depth = 0;
    int depm3 = getPruning(edge);
    if (depm3 == 3)
        return EDGE3_MAX_PRUN;
    while (edge != 0) {
        depm3 = (depm3 + 2) % 3;
        const int symcord1 = edge / 20160;
        const int cord1 = Edge3Sym2Raw[symcord1];
        const int cord2 = edge % 20160;
        e3set(e, cord1 * 20160 + cord2);
        for (int m = 0; m < 17; ++m) {
            const int idx = getMvSym(e.edge, m) >> 3;
            if (getPruning(idx) == depm3) {
                ++depth;
                edge = idx;
                break;
            }
        }
    }
    return depth;
}

int Tables::c1getsym(Center1 c) const
{
    for (int j = 0; j < 48; ++j) {
        const int n = c1get(c);
        const auto it = std::lower_bound(Center1Sym2Raw.begin(), Center1Sym2Raw.end(), n);   // raw2sym_0 (binary search)
        if (it != Center1Sym2Raw.end() && *it == n)
            return static_cast<int>(it - Center1Sym2Raw.begin()) * 64 + j;
        c1step(c, j);
    }
    return -1;   // unreachable for valid states (JS: undefined)
}

int Tables::c3getct(const Center3 &c) const
{
    int idx = 0, r = 4;
    for (int i = 6; i >= 0; --i)
        if (c.ud[i] != c.ud[7])
            idx += Cnk()[i][r--];
    idx *= 35;
    r = 4;
    for (int i = 6; i >= 0; --i)
        if (c.fb[i] != c.fb[7])
            idx += Cnk()[i][r--];
    idx *= 12;
    const int check = c.fb[7] ^ c.ud[7];
    int idxrl = 0;
    r = 4;
    for (int i = 7; i >= 0; --i)
        if (c.rl[i] != check)
            idxrl += Cnk()[i][r--];
    return c.parity + 2 * (idx + std2rl[idxrl]);
}

void Tables::initMoves()
{
    for (int i = 0; i < 29; ++i)
        std2move[move2std[i]] = i;
    for (int i = 0; i < 21; ++i)
        std3move[move3std[i]] = i;
    for (int i = 0; i < 36; ++i) {
        for (int j = 0; j < 36; ++j)
            ckmv[i][j] = i / 3 == j / 3 || ((i / 3) % 3 == (j / 3) % 3 && i > j);
        ckmv[36][i] = false;
    }
    for (int i = 0; i < 29; ++i)
        for (int j = 0; j < 28; ++j)
            ckmv2[i][j] = ckmv[move2std[i]][move2std[j]];
    for (int i = 0; i < 21; ++i)
        for (int j = 0; j < 20; ++j)
            ckmv3[i][j] = ckmv[move3std[i]][move3std[j]];
    for (int i = 0; i < 36; ++i) {
        skipAxis[i] = 36;
        for (int j = i; j < 36; ++j)
            if (!ckmv[i][j]) {
                skipAxis[i] = j - 1;
                break;
            }
    }
    for (int i = 0; i < 28; ++i) {
        skipAxis2[i] = 28;
        for (int j = i; j < 28; ++j)
            if (!ckmv2[i][j]) {
                skipAxis2[i] = j - 1;
                break;
            }
    }
    for (int i = 0; i < 20; ++i) {
        skipAxis3[i] = 20;
        for (int j = i; j < 20; ++j)
            if (!ckmv3[i][j]) {
                skipAxis3[i] = j - 1;
                break;
            }
    }
    for (int i = 0; i < 36; ++i) {
        EdgeCube edge;
        doMoveEdge(edge.ep, i);
        for (int j = 0; j < 24; j++)
            epMoveMap[i][edge.ep[j]] = j;
    }
}

void Tables::initSymMeta()
{
    Center1 c;
    for (int i = 0; i < 24; ++i)
        c.ct[i] = i;
    Center1 d = c, e = c, f = c;
    for (int i = 0; i < 48; ++i) {
        for (int j = 0; j < 48; ++j) {
            for (int k = 0; k < 48; ++k) {
                if (c.ct == d.ct) {
                    SymMult[i][j] = k;
                    if (k == 0)
                        SymInv[i] = j;
                }
                c1step(d, k);
            }
            c1step(c, j);
        }
        c1step(c, i);
    }
    for (int i = 0; i < 48; ++i) {
        c = e;
        for (int j = 0; j < SymInv[i]; ++j)   // Center1Rotate(c, SymInv[i])
            c1step(c, j);
        for (int j = 0; j < 36; ++j) {
            d = c;
            doMoveCenterCube(d.ct, j);
            for (int r = 0; r < i; ++r)   // Center1Rotate(d, i)
                c1step(d, r);
            for (int k = 0; k < 36; ++k) {
                f = e;
                doMoveCenterCube(f.ct, k);
                if (f.ct == d.ct) {
                    SymMove[i][j] = k;
                    break;
                }
            }
        }
    }
    c1set(c, 0);
    for (int i = 0; i < 48; ++i) {
        finish[SymInv[i]] = c1get(c);
        c1step(c, i);
    }
}

void Tables::initCenter1(vector<int> &raw2sym)
{
    const int nRaw = Cnk()[21][8];
    Center1 c, d;
    // initCenter1Sym2Raw (with Center1Raw2Sym allocated)
    for (int i = 0; i < 24; i++)
        c.ct[i] = i;
    for (int s = 0; s < 48; s++) {
        Center1RotPerm[s] = c.ct;
        c1step(c, s);
    }
    vector<uint32_t> occ(22984, 0);
    raw2sym.assign(735471, 0);
    Center1Sym2Raw.assign(N_C1SYM, 0);
    int count = 0;
    for (int i = 0; i < nRaw; ++i) {
        if ((occ[i >> 5] & (1u << (i & 31))) != 0)
            continue;
        c1set(c, i);
        for (int j = 0; j < 48; ++j) {
            const int idx = c1rotThres(c, Center1RotPerm[j], nRaw);
            if (idx == -1)
                continue;
            occ[idx >> 5] |= 1u << (idx & 31);
            raw2sym[idx] = count << 6 | SymInv[j];
        }
        Center1Sym2Raw[count++] = i;
    }
    // $getsym with Center1Raw2Sym present
    auto getsymFast = [&](const Center1 &cc) {
        for (int s = 0; s < 48; s++) {
            const int idx = c1rotThres(cc, Center1RotPerm[s], nRaw);
            if (idx != -1) {
                const int ret = raw2sym[idx];
                return (ret & ~0x3f) | SymMult[s][ret & 0x3f];
            }
        }
        return -1;
    };
    // initCenter1MoveTable
    array<int, 36> undef;
    undef.fill(-1);
    Center1SymMove.assign(N_C1SYM, undef);
    for (int i = 0; i < N_C1SYM; ++i) {
        c1set(d, Center1Sym2Raw[i]);
        for (int m = 0; m < 36; ++m) {
            if (m % 3 == 1 || Center1SymMove[i][m] != -1)
                continue;
            c = d;
            doMoveCenterCube(c.ct, m);
            const int idx = getsymFast(c);
            Center1SymMove[i][m] = idx;
            const int invM = SymMove[idx & 0x3f][(m / 3) * 3 + 2 - m % 3];
            if (Center1SymMove[idx >> 6][invM] == -1)
                Center1SymMove[idx >> 6][invM] = i << 6 | SymInv[idx & 0x3f];
        }
    }
    for (int i = 0; i < N_C1SYM; i++)
        for (int m = 0; m < 36; m += 3) {
            const int idx = Center1SymMove[i][m];
            const int nextM = SymMove[idx & 0x3f][m];
            const int nextIdx = Center1SymMove[idx >> 6][nextM];
            const int symx = SymMult[idx & 0x3f][nextIdx & 0x3f];
            Center1SymMove[i][m + 1] = (nextIdx & ~0x3f) | symx;
        }
    // initCenter1Prun
    Center1SymPrun.assign(N_C1SYM, -1);
    Center1SymPrun[0] = 0;
    int depth = 0, done = 1;
    while (done != N_C1SYM) {
        const bool inv = depth > 4;
        const int select = inv ? -1 : depth;
        const int check = inv ? depth : -1;
        ++depth;
        for (int i = 0; i < N_C1SYM; ++i) {
            if (Center1SymPrun[i] != select)
                continue;
            for (int m = 0; m < 27; ++m) {
                const int idx = static_cast<int>(static_cast<unsigned>(Center1SymMove[i][m]) >> 6);
                if (Center1SymPrun[idx] != check)
                    continue;
                ++done;
                if (inv) {
                    Center1SymPrun[i] = depth;
                    break;
                }
                Center1SymPrun[idx] = depth;
            }
        }
    }
}

void Tables::initCenter2()
{
    Center2 c, d;
    rlmv.assign(70, {});
    rlrot.assign(70, {});
    ctrot.assign(6435, {});
    ctmv.assign(6435, {});
    for (int i = 0; i < 70; ++i)
        for (int m = 0; m < 28; ++m) {
            c2setrl(c, i);
            c2move(c, move2std[m]);
            rlmv[i][m] = c2getrl(c);
        }
    for (int i = 0; i < 70; ++i) {
        c2setrl(c, i);
        for (int j = 0; j < 16; ++j) {
            rlrot[i][j] = c2getrl(c);
            c2rot(c, 0);
            if (j % 2 == 1) c2rot(c, 1);
            if (j % 8 == 7) c2rot(c, 2);
        }
    }
    for (int i = 0; i < 6435; ++i) {
        c2setct(c, i);
        for (int j = 0; j < 16; ++j) {
            ctrot[i][j] = c2getct(c);
            c2rot(c, 0);
            if (j % 2 == 1) c2rot(c, 1);
            if (j % 8 == 7) c2rot(c, 2);
        }
    }
    for (int i = 0; i < 6435; ++i) {
        c2setct(c, i);
        for (int m = 0; m < 28; ++m) {
            d = c;
            c2move(d, move2std[m]);
            ctmv[i][m] = c2getct(d);
        }
    }
    ctprun.assign(450450, -1);
    ctprun[0] = ctprun[18] = ctprun[28] = ctprun[46] = ctprun[54] = ctprun[56] = 0;
    int depth = 0, done = 6;
    while (done != 450450) {
        const bool inv = depth > 6;
        const int select = inv ? -1 : depth;
        const int check = inv ? depth : -1;
        ++depth;
        for (int i = 0; i < 450450; ++i) {
            if (ctprun[i] != select)
                continue;
            const int ct = i / 70, rl = i % 70;
            for (int m = 0; m < 23; ++m) {
                const int idx = ctmv[ct][m] * 70 + rlmv[rl][m];
                if (ctprun[idx] != check)
                    continue;
                ++done;
                if (inv) {
                    ctprun[i] = static_cast<int8_t>(depth);
                    break;
                }
                ctprun[idx] = static_cast<int8_t>(depth);
            }
        }
    }
}

void Tables::initCenter3()
{
    std2rl.fill(0);
    for (int i = 0; i < 12; ++i)
        std2rl[rl2std[i]] = i;
    Center3 c, d;
    ctmove.assign(29400, {});
    for (int i = 0; i < 29400; ++i) {
        c3setct(c, i);
        for (int m = 0; m < 20; ++m) {
            d = c;
            c3move(d, m);
            ctmove[i][m] = c3getct(d);
        }
    }
    prun3.assign(29400, -1);
    prun3[0] = 0;
    int depth = 0, done = 1;
    while (done != 29400) {
        for (int i = 0; i < 29400; ++i) {
            if (prun3[i] != depth)
                continue;
            for (int m = 0; m < 17; ++m)
                if (prun3[ctmove[i][m]] == -1) {
                    prun3[ctmove[i][m]] = static_cast<int8_t>(depth + 1);
                    ++done;
                }
        }
        ++depth;
    }
}

void Tables::initEdge3()
{
    // initEdge3MvRot
    Edge3 e;
    for (int m = 0; m < 21; ++m)
        for (int r = 0; r < 8; ++r) {
            e3set(e, 0);
            e3move(e, m);
            e3rotate(*this, e, r);
            mvrot[m << 3 | r] = e.edge;
            e3std(e);
            mvroto[m << 3 | r] = e.temp;
        }
    // initEdge3Sym2Raw
    Edge3Sym2Raw.assign(1538, 0);
    Edge3Sym2Mask.assign(1538, 0);
    symstate.assign(1538, 0);
    Edge3Raw2Sym.assign(11880, 0);
    vector<int> occ(1485, 0);
    int count = 0;
    for (int i = 0; i < 11880; ++i) {
        if ((occ[i >> 3] & (1 << (i & 7))) != 0)
            continue;
        e3set(e, i * factX[8]);
        Edge3Sym2Raw[count] = i;
        Edge3Sym2Mask[count] = e3get(e, 4, true);
        for (int j = 0; j < 8; ++j) {
            const int idx = e3get(e, 4);
            if (idx == i)
                symstate[count] |= 1 << j;
            occ[idx >> 3] |= 1 << (idx & 7);
            Edge3Raw2Sym[idx] = count << 3 | syminv0[j];
            e3rot(*this, e, 0);
            if (j % 2 == 1) {
                e3rot(*this, e, 1);
                e3rot(*this, e, 2);
            }
        }
        count++;
    }
    for (int m = 0; m < 20; m++)
        Edge3SymMove[m].assign(1538, -1);   // -1: JS undefined
    for (int i = 0; i < 1538; i++) {
        e3set(e, Edge3Sym2Raw[i] * factX[8]);
        for (int m = 0; m < 20; ++m) {
            if (move3std[m] % 3 != 1)
                continue;
            const int idx = getMvSym(e.edge, m);
            Edge3SymMove[m][i] = ((idx >> 3) / 20160) << 3 | (idx & 0x7);
        }
    }
}

void Tables::initEdge3Prun()
{
    Edge3 e, f, g;
    Edge3Prun.assign(1937880, 0xffffffffu);
    int depth = 0, done = 1;
    setPruning(0, 0);
    static const int bfsMoves[17] = { 1, 0, 2, 3, 5, 4, 6, 8, 7, 9, 10, 12, 11, 13, 14, 15, 16 };
    while (done != 31006080) {
        const bool inv = depth > 9;
        const int depm3 = depth % 3, dep1m3 = (depth + 1) % 3, dep2m3 = (depth + 2) % 3;
        const int find = inv ? 3 : depm3;
        const int chk = inv ? depm3 : 3;
        const uint32_t findMask = static_cast<uint32_t>(find) * 0x55555555u;
        if (depth >= EDGE3_MAX_PRUN - 1)
            break;
        for (int i0 = 0; i0 < 31006080; i0 += 16) {
            uint32_t val = Edge3Prun[i0 >> 4];
            const uint32_t chkmask = val ^ findMask;
            if ((!inv && val == 0xffffffffu) || ((chkmask - 0x55555555u) & ~chkmask & 0xaaaaaaaau) == 0)
                continue;
            for (int i = i0, end = i0 + 16; i < end; ++i, val >>= 2) {
                if (static_cast<int>(val & 3) != find)
                    continue;
                const int symcord1 = i / 20160;
                const int cord1 = Edge3Sym2Raw[symcord1];
                const int cord2 = i % 20160;
                e3set(e, cord1 * 20160 + cord2);
                for (int mi = 0; mi < 17; ++mi) {
                    const int m = bfsMoves[mi];
                    int idx = getMvSym(e.edge, m, Edge3SymMove[m][symcord1]);
                    const int symx = idx & 7;
                    idx >>= 3;
                    const int prun = getPruning(idx);
                    if (prun != chk) {
                        if (prun == dep2m3 || (prun == depm3 && idx < i))
                            mi = skipAxis3[m];
                        continue;
                    }
                    setPruning(inv ? i : idx, dep1m3);
                    ++done;
                    if (inv)
                        break;
                    const int symcord1x = idx / 20160;
                    int symState = symstate[symcord1x];
                    if (symState == 1)
                        continue;
                    f = e;
                    e3move(f, m);
                    e3rotate(*this, f, symx);
                    for (int j = 1; (symState >>= 1) != 0; ++j) {
                        if ((symState & 1) != 1)
                            continue;
                        g = f;
                        e3rotate(*this, g, j);
                        const int idxx = symcord1x * 20160 + e3get(g, 10) % 20160;
                        if (getPruning(idxx) == chk) {
                            setPruning(idxx, dep1m3);
                            ++done;
                        }
                    }
                }
            }
        }
        ++depth;
    }
}

Tables::Tables()
{
    initMoves();
    // CornerCube move cubes (mathlib.setNPerm + setTwist)
    const int init[6][2] = { { 15120, 0 }, { 21021, 1494 }, { 8064, 1236 }, { 9, 0 }, { 1230, 412 }, { 224, 137 } };
    for (int a = 0; a < 6; ++a) {
        Corner &c = CornerMoveCube[a * 3];
        int idx = init[a][0];
        array<int, 8> avail = { { 0, 1, 2, 3, 4, 5, 6, 7 } };
        int n = 8;
        static const int f[8] = { 1, 1, 2, 6, 24, 120, 720, 5040 };
        for (int i = 0; i < 7; ++i) {
            const int p = f[7 - i];
            const int v = idx / p;
            idx -= v * p;
            c.cp[i] = avail[v];
            for (int k = v; k < n - 1; ++k)
                avail[k] = avail[k + 1];
            --n;
        }
        c.cp[7] = avail[0];
        int tw = init[a][1], twst = 0;
        for (int i = 6; i >= 0; --i) {
            twst += c.co[i] = tw % 3;
            tw /= 3;
        }
        c.co[7] = (15 - twst) % 3;
    }
    for (int a = 0; a < 18; a += 3)
        for (int p = 0; p < 2; ++p)
            cornMult0(CornerMoveCube[a + p], CornerMoveCube[a], CornerMoveCube[a + p + 1]);
    initSymMeta();
    {
        vector<int> raw2sym;
        initCenter1(raw2sym);
    }
    initCenter2();
    initCenter3();
    initEdge3();
    initEdge3Prun();
}

const Tables &T()
{
    static const Tables t;   // thread-safe one-time init
    return t;
}

// ---------------------------------------------------------------- full cube + search
struct FullCube {
    EdgeCube edge;
    array<int, 24> center {};
    Corner corner;
    array<int, 60> moveBuffer {};
    int value = 0, length1 = 0, length2 = 0, length3 = 0, sym = 0, moveLength = 0;
    int edgeAvail = 0, centerAvail = 0, cornerAvail = 0;
    bool add1 = false;
    FullCube()
    {
        for (int i = 0; i < 24; ++i)
            center[i] = centerFacelet[i] >> 4;
    }
    void move(int m) { moveBuffer[moveLength++] = m; }
    const array<int, 24> &getCenter()
    {
        while (centerAvail < moveLength)
            doMoveCenterCube(center, moveBuffer[centerAvail++]);
        return center;
    }
    const Corner &getCorner()
    {
        Corner temps;
        while (cornerAvail < moveLength) {
            cornMult0(corner, T().CornerMoveCube[moveBuffer[cornerAvail++] % 18], temps);
            corner = temps;
        }
        return corner;
    }
    const array<int, 24> &getEdge()
    {
        while (edgeAvail < moveLength)
            doMoveEdge(edge.ep, moveBuffer[edgeAvail++]);
        return edge.ep;
    }
};

array<int, 96> toFacelet(FullCube &c)
{
    c.getCenter();
    c.getCorner();
    c.getEdge();
    array<int, 96> f {};
    for (int i = 0; i < 24; i++)
        f[centerFacelet[i]] = c.center[i];
    for (int i = 0; i < 24; i++) {
        f[edgeFacelet[i][0]] = edgeFacelet[c.edge.ep[i]][0] >> 4;
        f[edgeFacelet[i][1]] = edgeFacelet[c.edge.ep[i]][1] >> 4;
    }
    for (int k = 0; k < 8; k++) {
        const int j = c.corner.cp[k], ori = c.corner.co[k];
        for (int n = 0; n < 3; n++)
            f[cornerFacelet[k][(n + ori) % 3]] = cornerFacelet[j][n] >> 4;
    }
    return f;
}

void fromFacelet(FullCube &c, const array<int, 96> &f)
{
    for (int i = 0; i < 24; i++)
        c.center[i] = f[centerFacelet[i]];
    for (int i = 0; i < 24; i++)
        for (int j = 0; j < 24; j++)
            if (f[edgeFacelet[i][0]] == (edgeFacelet[j][0] >> 4) && f[edgeFacelet[i][1]] == (edgeFacelet[j][1] >> 4))
                c.edge.ep[i] = j;
    for (int i = 0; i < 8; i++) {
        int ori;
        for (ori = 0; ori < 3; ori++)
            if (f[cornerFacelet[i][ori]] == 0 || f[cornerFacelet[i][ori]] == 3)
                break;
        const int col1 = f[cornerFacelet[i][(ori + 1) % 3]];
        const int col2 = f[cornerFacelet[i][(ori + 2) % 3]];
        for (int j = 0; j < 8; j++)
            if (col1 == (cornerFacelet[j][1] >> 4) && col2 == (cornerFacelet[j][2] >> 4)) {
                c.corner.cp[i] = j;
                c.corner.co[i] = ori % 3;
                break;
            }
    }
}

// GWT java.util.PriorityQueue over FullCube indices, ordered by $compare_0 (c2.value - c1.value).
class PQueue {
public:
    explicit PQueue(vector<FullCube> &pool) : pool_(pool) {}
    void clear() { array_.clear(); }
    int size() const { return static_cast<int>(array_.size()); }
    const vector<int> &array() const { return array_; }
    void offer(int e)
    {
        int node = size();
        array_.push_back(e);
        while (node > 0) {
            const int childNode = node;
            node = (node - 1) >> 1;
            if (cmp(array_[node], e) <= 0) {
                array_[childNode] = e;
                return;
            }
            array_[childNode] = array_[node];
        }
        array_[node] = e;
    }
    int poll()
    {
        const int value = array_[0];
        const int lastValue = array_.back();
        array_.pop_back();
        if (!array_.empty()) {
            array_[0] = lastValue;
            mergeHeaps(0);
        }
        return value;
    }

private:
    vector<FullCube> &pool_;
    vector<int> array_;
    int cmp(int a, int b) const { return pool_[b].value - pool_[a].value; }
    void mergeHeaps(int node)
    {
        const int heapSize = size();
        const int value = array_[node];
        while (node * 2 + 1 < heapSize) {
            const int leftChild = 2 * node + 1, rightChild = leftChild + 1;
            int smallestChild = leftChild;
            if (rightChild < heapSize && cmp(array_[rightChild], array_[leftChild]) < 0)
                smallestChild = rightChild;
            if (cmp(value, array_[smallestChild]) < 0)
                break;
            array_[node] = array_[smallestChild];
            node = smallestChild;
        }
        array_[node] = value;
    }
};

class Search4 {
public:
    Search4() : p1sols_(pool_) { pool_.reserve(PHASE2_ATTS + 1); }
    std::string solve(const FullCube &start);

private:
    const Tables &t_ = T();
    FullCube c_, c1_, c2_;
    vector<FullCube> pool_;   // phase-1 candidates referenced by the queue
    PQueue p1sols_;
    vector<FullCube> arr2_ = vector<FullCube>(PHASE2_SOLS);
    int arr2idx_ = 0, p1SolsCnt_ = 0, length1_ = 0, length2_ = 0;
    bool add1_ = false;
    array<int, 15> move1_ {};
    array<int, 20> move2_ {}, move3_ {};
    array<array<int, 12>, 20> tempep_ {};
    array<int, 24> epInv_ {};
    Center2 ct2_;
    Center3 ct3_;
    Edge3 e12_;

    bool init2(int sym);
    bool init3();
    bool phase1(int ct, int sym, int maxl, int lm, int depth);
    bool phase2(int ct, int rl, int maxl, int lm, int depth);
    bool phase3(const array<int, 12> &eplast, int ct, int prun, int maxl, int lm, int depth);
    int getSolvedSym(const array<int, 24> &cube) const;
    std::string getMoveString(FullCube &c) const;
};

bool Search4::init2(int sym)
{
    c1_ = c_;
    for (int i = 0; i < length1_; ++i)
        c1_.move(move1_[i]);
    switch (t_.finish[sym]) {
    case 0:
        c1_.move(24);
        c1_.move(35);
        move1_[length1_] = 24;
        move1_[length1_ + 1] = 35;
        add1_ = true;
        sym = 19;
        break;
    case 12869:
        c1_.move(18);
        c1_.move(29);
        move1_[length1_] = 18;
        move1_[length1_ + 1] = 29;
        add1_ = true;
        sym = 34;
        break;
    case 735470:
        add1_ = false;
        sym = 0;
        break;
    }
    c2set(ct2_, c1_.getCenter(), parity0(c1_.getEdge(), 24));
    const int s2ct = c2getct(ct2_), s2rl = c2getrl(ct2_);
    const int ctp = t_.ctprun[s2ct * 70 + s2rl];
    c1_.value = ctp + length1_;
    c1_.length1 = length1_;
    c1_.add1 = add1_;
    c1_.sym = sym;
    ++p1SolsCnt_;
    int next;
    if (p1sols_.size() < PHASE2_ATTS) {
        pool_.push_back(c1_);
        next = static_cast<int>(pool_.size()) - 1;
    } else {
        next = p1sols_.poll();
        if (pool_[next].value > c1_.value)
            pool_[next] = c1_;
    }
    p1sols_.offer(next);
    return p1SolsCnt_ == PHASE1_SOLS;
}

bool checkPhase2Edge(const Tables &t, const array<int, 24> &epInv, const array<int, 20> &moves, int length)
{
    int parity = 0;
    for (int i = 0; i < 12; i++) {
        int e = epInv[i], eo = epInv[i + 12];
        for (int j = 0; j < length; j++) {
            const array<int, 24> &moveMap = t.epMoveMap[moves[j]];
            e = moveMap[e];
            eo = moveMap[eo];
        }
        if ((e < 12) != (eo >= 12))
            return false;
        parity ^= e >= 12 ? 1 : 0;
    }
    return parity == 0;
}

bool Search4::init3()
{
    if (!checkPhase2Edge(t_, epInv_, move2_, length2_))
        return false;
    c2_ = c1_;
    for (int i = 0; i < length2_; ++i)
        c2_.move(move2_[i]);
    const int eparity = e3setFromCube(e12_, c2_.getEdge());
    c3set(ct3_, c2_.getCenter(), eparity ^ parity0(c2_.getCorner().cp, 8));
    const int ct = t_.c3getct(ct3_);
    e3get(e12_, 10);   // standardizes e12_ (getsym below relies on it)
    const int prun = t_.getprun(t_.getMvSym(e12_.edge, 20) >> 3);   // getprun($getsym_0(e12))
    arr2_[arr2idx_] = c2_;
    arr2_[arr2idx_].value = length1_ + length2_ + std::max(prun, static_cast<int>(t_.prun3[ct]));
    arr2_[arr2idx_].length2 = length2_;
    ++arr2idx_;
    return arr2idx_ == static_cast<int>(arr2_.size());
}

bool Search4::phase1(int ct, int sym, int maxl, int lm, int depth)
{
    if (ct == 0)
        return maxl == 0 && init2(sym);
    for (int axis = 0; axis < 27; axis += 3) {
        if (axis == lm || axis == lm - 9 || axis == lm - 18)
            continue;
        for (int power = 0; power < 3; ++power) {
            const int m = axis + power;
            int ctx = t_.Center1SymMove[ct][t_.SymMove[sym][m]];
            const int prun = t_.Center1SymPrun[static_cast<unsigned>(ctx) >> 6];
            if (prun >= maxl) {
                if (prun > maxl)
                    break;
                continue;
            }
            const int symx = t_.SymMult[sym][ctx & 63];
            ctx = static_cast<int>(static_cast<unsigned>(ctx) >> 6);
            move1_[depth] = m;
            if (phase1(ctx, symx, maxl - 1, axis, depth + 1))
                return true;
        }
    }
    return false;
}

bool Search4::phase2(int ct, int rl, int maxl, int lm, int depth)
{
    if (ct == 0 && t_.ctprun[rl] == 0 && maxl < 5)
        return maxl == 0 && init3();
    for (int m = 0; m < 23; ++m) {
        if (t_.ckmv2[lm][m]) {
            m = t_.skipAxis2[m];
            continue;
        }
        const int ctx = t_.ctmv[ct][m], rlx = t_.rlmv[rl][m];
        const int prun = t_.ctprun[ctx * 70 + rlx];
        if (prun >= maxl) {
            if (prun > maxl)
                m = t_.skipAxis2[m];
            continue;
        }
        move2_[depth] = move2std[m];
        if (phase2(ctx, rlx, maxl - 1, m, depth + 1))
            return true;
    }
    return false;
}

bool Search4::phase3(const array<int, 12> &eplast, int ct, int prun, int maxl, int lm, int depth)
{
    if (maxl == 0)
        return true;
    array<int, 12> &ep = tempep_[depth];
    if (lm != 20) {
        const array<int, 12> &movo = t_.mvroto[lm << 3], &mov = t_.mvrot[lm << 3];
        for (int i = 0; i < 12; i++)
            ep[i] = movo[eplast[mov[i]]];
    }
    for (int m = 0; m < 17; m++) {
        if (t_.ckmv3[lm][m]) {
            m = t_.skipAxis3[m];
            continue;
        }
        const int ctx = t_.ctmove[ct][m];
        const int prun1 = t_.prun3[ctx];
        if (prun1 >= maxl) {
            if (prun1 > maxl && m < 14)
                m = t_.skipAxis3[m];
            continue;
        }
        const int prunx = t_.getprun0(t_.getMvSym(ep, m) >> 3, prun);
        if (prunx >= maxl) {
            if (prunx > maxl && m < 14)
                m = t_.skipAxis3[m];
            continue;
        }
        if (phase3(ep, ctx, prunx, maxl - 1, m, depth + 1)) {
            move3_[depth] = m;
            return true;
        }
    }
    return false;
}

int Search4::getSolvedSym(const array<int, 24> &cube) const
{
    Center1 c;
    c.ct = cube;
    for (int j = 0; j < 48; ++j) {
        bool check = true;
        for (int i = 0; i < 24; ++i)
            if (c.ct[i] != (centerFacelet[i] >> 4)) {
                check = false;
                break;
            }
        if (check)
            return j;
        c1step(c, j);
    }
    return -1;
}

std::string Search4::getMoveString(FullCube &c) const
{
    vector<int> fixedMoves(c.moveLength - (c.add1 ? 2 : 0));
    int idx = 0;
    for (int i = 0; i < c.length1; ++i)
        fixedMoves[idx++] = c.moveBuffer[i];
    int sym = c.sym;
    for (int i = c.length1 + (c.add1 ? 2 : 0); i < c.moveLength; ++i) {
        const int sm = t_.SymMove[sym][c.moveBuffer[i]];
        if (sm >= 27) {
            fixedMoves[idx++] = sm - 9;
            sym = t_.SymMult[sym][move2rot[sm - 27]];
        } else {
            fixedMoves[idx++] = sm;
        }
    }
    const int finishSym = t_.SymMult[t_.SymInv[sym]][getSolvedSym(c.getCenter())];
    vector<int> ret;
    sym = finishSym;
    for (int i = idx - 1; i >= 0; --i) {
        int move = fixedMoves[i];
        move = (move / 3) * 3 + (2 - move % 3);
        const int sm = t_.SymMove[sym][move];
        if (sm >= 27) {
            ret.push_back(sm - 9);
            sym = t_.SymMult[sym][move2rot[sm - 27]];
        } else {
            ret.push_back(sm);
        }
    }
    std::string out;
    int axis = -1;
    int pows[3] = { 0, 0, 0 };
    for (size_t i = 0; i < ret.size(); ++i) {
        const int move = ret[i];
        if (axis != (move / 3) % 3) {
            for (int k = 0; k < 3; k++)
                if (pows[k] % 4) {
                    out += std::string(move2str1[k * 9 + axis * 3 + pows[k] - 1]) + ' ';
                    pows[k] = 0;
                }
            axis = (move / 3) % 3;
        }
        pows[move / 9] += move % 3 + 1;
    }
    for (int k = 0; k < 3; k++)
        if (pows[k] % 4) {
            out += std::string(move2str1[k * 9 + axis * 3 + pows[k] - 1]) + ' ';
            pows[k] = 0;
        }
    return out;
}

std::string Search4::solve(const FullCube &start)
{
    c_ = start;
    Center1 cu, cf, cr;
    for (int i = 0; i < 24; ++i) {
        cu.ct[i] = c_.getCenter()[i] % 3 == 0 ? 1 : 0;
        cf.ct[i] = c_.center[i] % 3 == 1 ? 1 : 0;
        cr.ct[i] = c_.center[i] % 3 == 2 ? 1 : 0;
    }
    const int ud = t_.c1getsym(cu), fb = t_.c1getsym(cf), rl = t_.c1getsym(cr);
    const int udprun = t_.Center1SymPrun[ud >> 6], fbprun = t_.Center1SymPrun[fb >> 6], rlprun = t_.Center1SymPrun[rl >> 6];
    p1SolsCnt_ = 0;
    arr2idx_ = 0;
    p1sols_.clear();
    pool_.clear();
    for (length1_ = std::min({ udprun, fbprun, rlprun }); length1_ < MAX_SEARCH_DEPTH; ++length1_)
        if ((rlprun <= length1_ && phase1(rl >> 6, rl & 63, length1_, -1, 0))
            || (udprun <= length1_ && phase1(ud >> 6, ud & 63, length1_, -1, 0))
            || (fbprun <= length1_ && phase1(fb >> 6, fb & 63, length1_, -1, 0)))
            break;

    vector<int> p1SolsArr = p1sols_.array();
    std::stable_sort(p1SolsArr.begin(), p1SolsArr.end(), [this](int a, int b) { return pool_[a].value < pool_[b].value; });
    int maxLength2 = 9, length12 = 0;
    do {
        bool found = false;
        for (length12 = pool_[p1SolsArr[0]].value; length12 < MAX_SEARCH_DEPTH; ++length12) {
            for (size_t i = 0; i < p1SolsArr.size(); ++i) {
                const FullCube &cc = pool_[p1SolsArr[i]];
                if (cc.value > length12)
                    break;
                if (length12 - cc.length1 > maxLength2)
                    continue;
                c1_ = cc;
                const array<int, 24> &ep = c1_.getEdge();
                c2set(ct2_, c1_.getCenter(), parity0(ep, 24));
                const int s2ct = c2getct(ct2_), s2rl = c2getrl(ct2_);
                length1_ = cc.length1;
                length2_ = length12 - cc.length1;
                for (int e = 0; e < 24; e++)
                    epInv_[ep[e]] = e;
                if (phase2(s2ct, s2rl, length2_, 28, 0)) {
                    found = true;
                    break;
                }
            }
            if (found)
                break;   // JS `break OUT` leaves length12 unincremented
        }
        ++maxLength2;
    } while (length12 == MAX_SEARCH_DEPTH);

    std::stable_sort(arr2_.begin(), arr2_.end(), [](const FullCube &a, const FullCube &b) { return a.value < b.value; });
    int index = 0, maxLength3 = 13, length123 = 0;
    do {
        bool found = false;
        for (length123 = arr2_[0].value; length123 < MAX_SEARCH_DEPTH; ++length123) {
            for (int i = 0; i < std::min(arr2idx_, PHASE2_SOLS); ++i) {
                FullCube &a = arr2_[i];
                if (a.value > length123)
                    break;
                a.length3 = length123 - a.length1 - a.length2;
                if (a.length3 > maxLength3)
                    continue;
                const int eparity = e3setFromCube(e12_, a.getEdge());
                c3set(ct3_, a.getCenter(), eparity ^ parity0(a.getCorner().cp, 8));
                const int ct = t_.c3getct(ct3_);
                e3get(e12_, 10);
                tempep_[0] = e12_.edge;
                const int prun = t_.getprun(t_.getMvSym(e12_.edge, 20) >> 3);
                if (prun <= a.length3 && phase3(tempep_[0], ct, prun, a.length3, 20, 0)) {
                    index = i;
                    found = true;
                    break;
                }
            }
            if (found)
                break;
        }
        ++maxLength3;
    } while (length123 == MAX_SEARCH_DEPTH);

    FullCube solcube = arr2_[index];
    length1_ = solcube.length1;
    length2_ = solcube.length2;
    const int length3 = solcube.length3;
    for (int i = 0; i < length3; ++i)
        solcube.move(move3std[move3_[i]]);
    // to333Facelet
    const array<int, 96> f = toFacelet(solcube);
    static const int map4to3[9] = { 0, 1, 3, 4, 5, 7, 12, 13, 15 };
    std::string f3(54, ' ');
    for (int fidx = 0; fidx < 6; fidx++)
        for (int i = 0; i < 9; i++)
            f3[fidx * 9 + i] = "URFDLB"[f[fidx << 4 | map4to3[i]]];
    const std::string sol3 = solveFacelet333(f3);
    // split(' ') and keep /^[URFDLB][2']?$/
    size_t pos = 0;
    while (pos <= sol3.size()) {
        size_t sp = sol3.find(' ', pos);
        if (sp == std::string::npos)
            sp = sol3.size();
        const std::string tok = sol3.substr(pos, sp - pos);
        pos = sp + 1;
        if (tok.empty() || tok.size() > 2)
            continue;
        const size_t face = std::string("URFDLB").find(tok[0]);
        if (face == std::string::npos)
            continue;
        int pw = 0;
        if (tok.size() == 2) {
            if (tok[1] == '2') pw = 1;
            else if (tok[1] == '\'') pw = 2;
            else continue;
        }
        solcube.move(static_cast<int>(face) * 3 + pw);
    }
    return getMoveString(solcube);
}

// partialSolvedState(0xffffff, 0xffffff, 0xff) -> facelets, then genFacelet()'s $fromFacelet.
FullCube randomState(Rng &rng)
{
    auto rndPerm = [&rng](int n) {
        vector<int> arr(n);
        for (int i = 0; i < n; i++)
            arr[i] = i;
        for (int i = 0; i < n - 1; i++) {
            const int k = rng.rn(n - i);
            std::swap(arr[i], arr[i + k]);
        }
        return arr;
    };
    array<int, 96> facelet {};
    bool solved = true;
    for (int tries = 0; solved && tries < 100; tries++) {
        FullCube cc;
        const vector<int> ctPerm = rndPerm(24);
        for (int i = 0; i < 24; i++)
            cc.center[i] = centerFacelet[ctPerm[i]] >> 4;
        const vector<int> edPerm = rndPerm(24);
        for (int i = 0; i < 24; i++)
            cc.edge.ep[i] = edPerm[i];
        const vector<int> cnPerm = rndPerm(8);
        int coSum = 24;
        for (int i = 0; i < 8; i++) {
            const int co = rng.rn(3);
            cc.corner.co[i] = co;
            cc.corner.cp[i] = cnPerm[i];
            coSum -= co;
        }
        if (coSum % 3 != 0)
            cc.corner.co[0] = (cc.corner.co[0] + coSum) % 3;
        facelet = toFacelet(cc);
        for (int i = 0; i < 96; i++)
            if (facelet[i] != facelet[i >> 4 << 4])
                solved = false;
    }
    FullCube c;
    fromFacelet(c, facelet);
    return c;
}

std::string scramble444(Rng &rng)
{
    const FullCube start = randomState(rng);
    Search4 search;
    return search.solve(start);
}

} // namespace

std::string gen444(Rng &rng, const std::string &eventId)
{
    if (eventId == "444")
        return scramble444(rng);
    if (eventId == "444bld") {
        // '${444wca}#{[[""]],[""," x"," x2"," x'"," z"," z'"],1}#{[[""]],[""," y"," y2"," y'"],1}'
        static const std::vector<std::string> kRotX = { "", " x", " x2", " x'", " z", " z'" };
        static const std::vector<std::string> kRotY = { "", " y", " y2", " y'" };
        std::string s = scramble444(rng);
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

bool gen444Supported(const std::string &eventId) { return eventId == "444" || eventId == "444bld"; }

} // namespace ct
