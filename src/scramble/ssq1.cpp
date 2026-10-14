// Square-1 random-state scrambler (csTimer type 'sqrs'): port of
// scramble/scramble_sq1_new.js (csTimer commit 2547d82) — square1SolverGetRandomScramble
// = Search_solution(search, FullCube_randomCube()), with the Shape/Square pruning tables
// built exactly as Shape_init()/Square_init() do. Follows the JS line by line, including
// 32-bit bit-op semantics (JS >>>/& on signed ints) and the RNG draw order, so the same
// seed reproduces csTimer's output.
#include "modules.h"
#include "mathlib.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace ct {
namespace {

// mathlib.js bitCount (operates on 32-bit ints; all call sites keep x < 2^24)
int bitCount(int x)
{
    x -= (x >> 1) & 1431655765;
    x = ((x >> 2) & 858993459) + (x & 858993459);
    x = ((x >> 4) + x) & 252645135;
    x += x >> 8;
    x += x >> 16;
    return x & 63;
}

// mathlib.js binarySearch (returns the index, or -low-1 when absent)
int binarySearch(const std::vector<int> &a, int key)
{
    int low = 0, high = static_cast<int>(a.size()) - 1;
    while (low <= high) {
        int mid = low + ((high - low) >> 1);
        int midVal = a[mid];
        if (midVal < key)
            low = mid + 1;
        else if (midVal > key)
            high = mid - 1;
        else
            return mid;
    }
    return -low - 1;
}

using ml::Arr;
using ml::getNPerm;
using ml::setNPerm;

// mathlib.js circle(arr, i0, i1, ...): rotate the listed positions by one.
void circle(Arr &arr, std::initializer_list<int> idx)
{
    const int *p = idx.begin();
    const int len = static_cast<int>(idx.size());
    const int temp = arr[p[len - 1]];
    for (int i = len - 1; i > 0; --i)
        arr[p[i]] = arr[p[i - 1]];
    arr[p[0]] = temp;
}

struct SqCubie {
    uint32_t ul, ur, dl, dr;
    int ml;
    SqCubie() : ul(0x011233), ur(0x455677), dl(0x998bba), dr(0xddcffe), ml(0) {}

    int pieceAt(int idx) const
    {
        uint32_t ret;
        if (idx < 6)
            ret = ul >> ((5 - idx) << 2);
        else if (idx < 12)
            ret = ur >> ((11 - idx) << 2);
        else if (idx < 18)
            ret = dl >> ((17 - idx) << 2);
        else
            ret = dr >> ((23 - idx) << 2);
        return static_cast<int>(ret & 0xf);
    }

    void setPiece(int idx, int value)
    {
        if (idx < 6) {
            ul &= ~(0xfu << ((5 - idx) << 2));
            ul |= static_cast<uint32_t>(value) << ((5 - idx) << 2);
        } else if (idx < 12) {
            ur &= ~(0xfu << ((11 - idx) << 2));
            ur |= static_cast<uint32_t>(value) << ((11 - idx) << 2);
        } else if (idx < 18) {
            dl &= ~(0xfu << ((17 - idx) << 2));
            dl |= static_cast<uint32_t>(value) << ((17 - idx) << 2);
        } else {
            dr &= ~(0xfu << ((23 - idx) << 2));
            dr |= static_cast<uint32_t>(value) << ((23 - idx) << 2);
        }
    }

    void doMove(int move)
    {
        uint32_t temp;
        move <<= 2;
        if (move > 24) {
            move = 48 - move;
            temp = ul;
            ul = (ul >> move | ur << (24 - move)) & 0xffffff;
            ur = (ur >> move | temp << (24 - move)) & 0xffffff;
        } else if (move > 0) {
            temp = ul;
            ul = (ul << move | ur >> (24 - move)) & 0xffffff;
            ur = (ur << move | temp >> (24 - move)) & 0xffffff;
        } else if (move == 0) {
            temp = ur;
            ur = dl;
            dl = temp;
            ml = 1 - ml;
        } else if (move >= -24) {
            move = -move;
            temp = dl;
            dl = (dl << move | dr >> (24 - move)) & 0xffffff;
            dr = (dr << move | temp >> (24 - move)) & 0xffffff;
        } else {
            move = 48 + move;
            temp = dl;
            dl = (dl >> move | dr << (24 - move)) & 0xffffff;
            dr = (dr >> move | temp << (24 - move)) & 0xffffff;
        }
    }
};

struct Shape {
    int top, bottom, parity;
    Shape() : top(0), bottom(0), parity(0) {}
};

struct Square {
    int cornperm, edgeperm;
    bool topEdgeFirst, botEdgeFirst;
    int ml;
    Square() : cornperm(0), edgeperm(0), topEdgeFirst(false), botEdgeFirst(false), ml(0) {}
};

struct Tables {
    std::vector<int> ShapeIdx;     // Shape_ShapeIdx (3678 entries)
    std::vector<int> ShapePrun;    // 7536
    std::vector<int> ShapeTopMove; // 7356
    std::vector<int> ShapeBotMove; // 7356
    std::vector<int> ShapeTwist;   // 7356
    std::vector<int> SqTwist;      // 40320
    std::vector<int> SqTop;        // 40320
    std::vector<int> SqBot;        // 40320
    std::vector<int> SqPrun;       // 80640
};

int shapeGetIdx(const Tables &t, const Shape &s)
{
    return binarySearch(t.ShapeIdx, (s.top << 12) | s.bottom) * 2 + s.parity;
}

void shapeSetIdx(const Tables &t, Shape &s, int idx)
{
    s.parity = idx & 1;
    s.top = t.ShapeIdx[idx >> 1];
    s.bottom = s.top & 4095;
    s.top >>= 12;
}

int shapeGetShape2Idx(const Tables &t, int shp)
{
    return binarySearch(t.ShapeIdx, shp & 0xffffff) * 2 + (shp >> 24);
}

// Shape_topMove / Shape_bottomMove: turn one layer to the next position where a twist is
// possible; returns the turn amount (in 30-degree steps) and updates the parity.
int shapeLayerMove(int &layer, int &parity)
{
    int move = 0, moveParity = 0;
    do {
        if ((layer & 2048) == 0) {
            move += 1;
            layer <<= 1;
        } else {
            move += 2;
            layer = (layer << 2) ^ 12291;
        }
        moveParity = 1 - moveParity;
    } while ((bitCount(layer & 63) & 1) != 0);
    if ((bitCount(layer) & 2) == 0)
        parity ^= moveParity;
    return move;
}

void shapeInit(Tables &t)
{
    static const int halflayer[13] = { 0, 3, 6, 12, 15, 24, 27, 30, 48, 51, 54, 60, 63 };
    t.ShapeIdx.clear();
    for (int i = 0; i < 28561; ++i) {
        int dr = halflayer[i % 13];
        int dl = halflayer[(i / 13) % 13];
        int ur = halflayer[((i / 13) / 13) % 13];
        int ul = halflayer[((i / 13) / 13) / 13];
        int value = ul << 18 | ur << 12 | dl << 6 | dr;
        if (bitCount(value) == 16)
            t.ShapeIdx.push_back(value);
    }
    t.ShapeTopMove.assign(7356, 0);
    t.ShapeBotMove.assign(7356, 0);
    t.ShapeTwist.assign(7356, 0);
    Shape s;
    for (int i = 0; i < 7356; ++i) {
        shapeSetIdx(t, s, i);
        t.ShapeTopMove[i] = shapeLayerMove(s.top, s.parity);
        t.ShapeTopMove[i] |= shapeGetIdx(t, s) << 4;
        shapeSetIdx(t, s, i);
        t.ShapeBotMove[i] = shapeLayerMove(s.bottom, s.parity);
        t.ShapeBotMove[i] |= shapeGetIdx(t, s) << 4;
        shapeSetIdx(t, s, i);
        int temp = s.top & 63;
        int p1 = bitCount(temp);
        int p3 = bitCount(s.bottom & 4032);
        s.parity ^= 1 & (p1 & p3) >> 1;
        s.top = (s.top & 4032) | ((s.bottom >> 6) & 63);
        s.bottom = (s.bottom & 63) | (temp << 6);
        t.ShapeTwist[i] = shapeGetIdx(t, s);
    }
    t.ShapePrun.assign(7536, -1);
    t.ShapePrun[shapeGetShape2Idx(t, 14378715)] = 0;
    t.ShapePrun[shapeGetShape2Idx(t, 31157686)] = 0;
    t.ShapePrun[shapeGetShape2Idx(t, 23967451)] = 0;
    t.ShapePrun[shapeGetShape2Idx(t, 7191990)] = 0;
    int done = 4, done0 = 0, depth = -1;
    while (done != done0) {
        done0 = done;
        ++depth;
        for (int i = 0; i < 7536; ++i) {
            if (t.ShapePrun[i] == depth) {
                int m = 0, idx = i;
                do {
                    idx = t.ShapeTopMove[idx];
                    m += idx & 15;
                    idx >>= 4;
                    if (t.ShapePrun[idx] == -1) {
                        ++done;
                        t.ShapePrun[idx] = depth + 1;
                    }
                } while (m != 12);
                m = 0;
                idx = i;
                do {
                    idx = t.ShapeBotMove[idx];
                    m += idx & 15;
                    idx >>= 4;
                    if (t.ShapePrun[idx] == -1) {
                        ++done;
                        t.ShapePrun[idx] = depth + 1;
                    }
                } while (m != 12);
                idx = t.ShapeTwist[i];
                if (t.ShapePrun[idx] == -1) {
                    ++done;
                    t.ShapePrun[idx] = depth + 1;
                }
            }
        }
    }
}

void squareInit(Tables &t)
{
    t.SqTwist.assign(40320, 0);
    t.SqTop.assign(40320, 0);
    t.SqBot.assign(40320, 0);
    Arr pos;
    for (int i = 0; i < 40320; ++i) {
        setNPerm(pos, i, 8);
        circle(pos, { 2, 4 });
        circle(pos, { 3, 5 });
        t.SqTwist[i] = getNPerm(pos, 8);
        setNPerm(pos, i, 8);
        circle(pos, { 0, 3, 2, 1 });
        t.SqTop[i] = getNPerm(pos, 8);
        setNPerm(pos, i, 8);
        circle(pos, { 4, 7, 6, 5 });
        t.SqBot[i] = getNPerm(pos, 8);
    }
    t.SqPrun.assign(80640, -1);
    t.SqPrun[0] = 0;
    int depth = 0, done = 1;
    while (done < 80640) {
        bool inv = depth >= 11;
        int find = inv ? -1 : depth;
        int check = inv ? depth : -1;
        ++depth;
        auto process = [&](int i) {
            int idx = i >> 1;
            int ml = i & 1;
            int idxx = (t.SqTwist[idx] << 1) | (1 - ml);
            if (t.SqPrun[idxx] == check) {
                ++done;
                t.SqPrun[inv ? i : idxx] = depth;
                if (inv)
                    return; // continue OUT
            }
            idxx = idx;
            for (int m = 0; m < 4; ++m) {
                idxx = t.SqTop[idxx];
                if (t.SqPrun[(idxx << 1) | ml] == check) {
                    ++done;
                    t.SqPrun[inv ? i : ((idxx << 1) | ml)] = depth;
                    if (inv)
                        return; // continue OUT
                }
            }
            for (int m = 0; m < 4; ++m) {
                idxx = t.SqBot[idxx];
                if (t.SqPrun[(idxx << 1) | ml] == check) {
                    ++done;
                    t.SqPrun[inv ? i : ((idxx << 1) | ml)] = depth;
                    if (inv)
                        return; // continue OUT
                }
            }
        };
        for (int i = 0; i < 80640; ++i)
            if (t.SqPrun[i] == find)
                process(i);
    }
}

const Tables &tables()
{
    static Tables t = [] {
        Tables t;
        shapeInit(t);
        squareInit(t);
        return t;
    }();
    return t;
}

int fullCubeGetParity(const SqCubie &obj)
{
    std::array<int, 24> arr {};
    int cnt = 0;
    arr[0] = obj.pieceAt(0);
    for (int i = 1; i < 24; ++i) {
        if (obj.pieceAt(i) != arr[cnt])
            arr[++cnt] = obj.pieceAt(i);
    }
    int p = 0;
    for (int a = 0; a < 16; ++a)
        for (int b = a + 1; b < 16; ++b)
            if (a <= cnt && b <= cnt && arr[a] > arr[b]) // JS: undefined compares false
                p ^= 1;
    return p;
}

// Six-bit shape of one half layer: which of its 6 slots hold corners (odd piece ids).
int halfLayerShape(uint32_t half)
{
    int x = static_cast<int>(half & 0x111111u);
    x |= x >> 3;
    x |= x >> 6;
    return (x & 15) | ((x >> 12) & 48);
}

int fullCubeGetShapeIdx(const Tables &t, const SqCubie &obj)
{
    const int urx = halfLayerShape(obj.ur), ulx = halfLayerShape(obj.ul);
    const int drx = halfLayerShape(obj.dr), dlx = halfLayerShape(obj.dl);
    return shapeGetShape2Idx(t, fullCubeGetParity(obj) << 24 | ulx << 18 | urx << 12 | dlx << 6 | drx);
}

void fullCubeGetSquare(const SqCubie &obj, Square &sq)
{
    Arr prm(8);
    for (int a = 0; a < 8; ++a)
        prm[a] = obj.pieceAt(a * 3 + 1) >> 1;
    sq.cornperm = getNPerm(prm, 8);
    sq.topEdgeFirst = obj.pieceAt(0) == obj.pieceAt(1);
    int a = sq.topEdgeFirst ? 2 : 0;
    int b;
    for (b = 0; b < 4; a += 3, ++b)
        prm[b] = obj.pieceAt(a) >> 1;
    sq.botEdgeFirst = obj.pieceAt(12) == obj.pieceAt(13);
    a = sq.botEdgeFirst ? 14 : 12;
    for (; b < 8; a += 3, ++b)
        prm[b] = obj.pieceAt(a) >> 1;
    sq.edgeperm = getNPerm(prm, 8);
    sq.ml = obj.ml;
}

SqCubie fullCubeRandomCube(const Tables &t, Rng &rng)
{
    SqCubie f;
    int indice = rng.rn(3678);
    int shape = t.ShapeIdx[indice];
    int corner = (0x01234567 << 1) | 0x11111111;
    int edge = 0x01234567 << 1;
    int n_corner = 8, n_edge = 8;
    for (int i = 0; i < 24; i++) {
        if (((shape >> i) & 1) == 0) { // edge
            int rnd = rng.rn(n_edge) << 2;
            f.setPiece(23 - i, (edge >> rnd) & 0xf);
            int m = (1 << rnd) - 1;
            edge = (edge & m) + ((edge >> 4) & ~m);
            --n_edge;
        } else { // corner
            int rnd = rng.rn(n_corner) << 2;
            f.setPiece(23 - i, (corner >> rnd) & 0xf);
            f.setPiece(22 - i, (corner >> rnd) & 0xf);
            int m = (1 << rnd) - 1;
            corner = (corner & m) + ((corner >> 4) & ~m);
            --n_corner;
            ++i;
        }
    }
    f.ml = rng.rn(2);
    return f;
}

struct Search {
    std::array<int, 160> move {};   // JS grows the array; 160 > the 100 + 32 search bound
    SqCubie d;
    Square sq;
    SqCubie c;
    int length1, maxlen2;
    std::string sol;

    Search() : length1(0), maxlen2(0) {}

    bool init2(const Tables &t)
    {
        d = c;
        for (int i = 0; i < length1; ++i)
            d.doMove(move[i]);
        fullCubeGetSquare(d, sq);
        int edge = sq.edgeperm;
        int corner = sq.cornperm;
        int ml = sq.ml;
        int prun = std::max(t.SqPrun[(sq.edgeperm << 1) | ml], t.SqPrun[(sq.cornperm << 1) | ml]);
        for (int i = prun; i < maxlen2; ++i) {
            if (phase2(t, edge, corner, sq.topEdgeFirst, sq.botEdgeFirst, ml, i, length1, 0)) {
                for (int j = 0; j < i; ++j)
                    d.doMove(move[length1 + j]);
                sol = move2string(i + length1);
                return true;
            }
        }
        return false;
    }

    std::string move2string(int len)
    {
        std::string s;
        int top = 0, bottom = 0;
        for (int i = len - 1; i >= 0; i--) {
            int val = move[i];
            if (val > 0) {
                val = 12 - val;
                top = (val > 6) ? (val - 12) : val;
            } else if (val < 0) {
                val = 12 + val;
                bottom = (val > 6) ? (val - 12) : val;
            } else {
                // JS emits "`/`" at the phase-1 boundary; toTxt() strips the backticks
                // (replace(/`(.*?)`/g, '$1')), so the published scramble has a plain "/".
                std::string twst = "/";
                if (top == 0 && bottom == 0) {
                    s += twst;
                } else {
                    s += " (" + std::to_string(top) + "," + std::to_string(bottom) + ")" + twst;
                }
                top = bottom = 0;
            }
        }
        if (top == 0 && bottom == 0) {
        } else {
            s += " (" + std::to_string(top) + "," + std::to_string(bottom) + ") ";
        }
        return s;
    }

    bool phase1(const Tables &t, int shape, int prunvalue, int maxl, int depth, int lm)
    {
        int m, prunx, shapex;
        if (prunvalue == 0 && maxl < 4)
            return maxl == 0 && init2(t);
        if (lm != 0) {
            shapex = t.ShapeTwist[shape];
            prunx = t.ShapePrun[shapex];
            if (prunx < maxl) {
                move[depth] = 0;
                if (phase1(t, shapex, prunx, maxl - 1, depth + 1, 0))
                    return true;
            }
        }
        shapex = shape;
        if (lm <= 0) {
            m = 0;
            while (true) {
                m += t.ShapeTopMove[shapex];
                shapex = m >> 4;
                m &= 15;
                if (m >= 12)
                    break;
                prunx = t.ShapePrun[shapex];
                if (prunx > maxl) {
                    break;
                } else if (prunx < maxl) {
                    move[depth] = m;
                    if (phase1(t, shapex, prunx, maxl - 1, depth + 1, 1))
                        return true;
                }
            }
        }
        shapex = shape;
        if (lm <= 1) {
            m = 0;
            while (true) {
                m += t.ShapeBotMove[shapex];
                shapex = m >> 4;
                m &= 15;
                if (m >= 6)
                    break;
                prunx = t.ShapePrun[shapex];
                if (prunx > maxl) {
                    break;
                } else if (prunx < maxl) {
                    move[depth] = -m;
                    if (phase1(t, shapex, prunx, maxl - 1, depth + 1, 2))
                        return true;
                }
            }
        }
        return false;
    }

    bool phase2(const Tables &t, int edge, int corner, bool topEdgeFirst, bool botEdgeFirst, int ml,
                int maxl, int depth, int lm)
    {
        if (maxl == 0 && !topEdgeFirst && botEdgeFirst)
            return true;
        if (lm != 0 && topEdgeFirst == botEdgeFirst) {
            int edgex = t.SqTwist[edge];
            int cornerx = t.SqTwist[corner];
            if (t.SqPrun[(edgex << 1) | (1 - ml)] < maxl && t.SqPrun[(cornerx << 1) | (1 - ml)] < maxl) {
                move[depth] = 0;
                if (phase2(t, edgex, cornerx, topEdgeFirst, botEdgeFirst, 1 - ml, maxl - 1, depth + 1, 0))
                    return true;
            }
        }
        if (lm <= 0) {
            bool topEdgeFirstx = !topEdgeFirst;
            int edgex = topEdgeFirstx ? t.SqTop[edge] : edge;
            int cornerx = topEdgeFirstx ? corner : t.SqTop[corner];
            int m = topEdgeFirstx ? 1 : 2;
            int prun1 = t.SqPrun[(edgex << 1) | ml];
            int prun2 = t.SqPrun[(cornerx << 1) | ml];
            while (m < 12 && prun1 <= maxl && prun1 <= maxl) {   // (sic) as in csTimer: prun2 unchecked
                if (prun1 < maxl && prun2 < maxl) {
                    move[depth] = m;
                    if (phase2(t, edgex, cornerx, topEdgeFirstx, botEdgeFirst, ml, maxl - 1, depth + 1, 1))
                        return true;
                }
                topEdgeFirstx = !topEdgeFirstx;
                if (topEdgeFirstx) {
                    edgex = t.SqTop[edgex];
                    prun1 = t.SqPrun[(edgex << 1) | ml];
                    m += 1;
                } else {
                    cornerx = t.SqTop[cornerx];
                    prun2 = t.SqPrun[(cornerx << 1) | ml];
                    m += 2;
                }
            }
        }
        if (lm <= 1) {
            bool botEdgeFirstx = !botEdgeFirst;
            int edgex = botEdgeFirstx ? t.SqBot[edge] : edge;
            int cornerx = botEdgeFirstx ? corner : t.SqBot[corner];
            int m = botEdgeFirstx ? 1 : 2;
            int prun1 = t.SqPrun[(edgex << 1) | ml];
            int prun2 = t.SqPrun[(cornerx << 1) | ml];
            while (m < (maxl > 6 ? 6 : 12) && prun1 <= maxl && prun1 <= maxl) {
                if (prun1 < maxl && prun2 < maxl) {
                    move[depth] = -m;
                    if (phase2(t, edgex, cornerx, topEdgeFirst, botEdgeFirstx, ml, maxl - 1, depth + 1, 2))
                        return true;
                }
                botEdgeFirstx = !botEdgeFirstx;
                if (botEdgeFirstx) {
                    edgex = t.SqBot[edgex];
                    prun1 = t.SqPrun[(edgex << 1) | ml];
                    m += 1;
                } else {
                    cornerx = t.SqBot[cornerx];
                    prun2 = t.SqPrun[(cornerx << 1) | ml];
                    m += 2;
                }
            }
        }
        return false;
    }

    std::string solution(const Tables &t, const SqCubie &cc)
    {
        c = cc;
        int shape = fullCubeGetShapeIdx(t, c);
        for (length1 = t.ShapePrun[shape]; length1 < 100; ++length1) {
            maxlen2 = std::min(32 - length1, 17);
            if (phase1(t, shape, t.ShapePrun[shape], length1, 0, -1))
                break;
        }
        return sol;
    }
};

} // namespace

std::string genSq1(Rng &rng, const std::string &eventId)
{
    if (eventId != "sq1")
        return std::string();
    const Tables &t = tables();
    Search search;
    SqCubie cube = fullCubeRandomCube(t, rng);
    return search.solution(t, cube);
}

bool genSq1Supported(const std::string &eventId)
{
    return eventId == "sq1";
}

} // namespace ct
