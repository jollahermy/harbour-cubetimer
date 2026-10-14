// Shared helpers for the native scramblers: a port of the csTimer lib/mathlib.js pieces they
// rely on (csTimer, GPL-3.0): permutation/orientation coordinates, acycle, the packed pruning
// table builder and the IDA* Solver. Bit-exact with the JS bundle (lib/mathlib.js with the V4
// Lehmer-code NPerm patch): identical move tables, pruning tables and search order, so a given
// RNG seed yields exactly the same scramble string.
//
// Thread safety: Solver::search() is const and keeps its state on the stack, so one solver
// (built once, e.g. as a function-local static) can serve several worker threads.
#ifndef CT_MATHLIB_H
#define CT_MATHLIB_H

#include <cstdint>
#include <string>
#include <vector>

namespace ct {
namespace ml {

using Arr = std::vector<int>;

inline int fact(int n)
{
    static const int f[] = { 1, 1, 2, 6, 24, 120, 720, 5040, 40320, 362880, 3628800, 39916800, 479001600 };
    return f[n];
}

// mathlib.setNPerm(arr, idx, n, even); even < 0: even permutations only, 0: JS undefined.
inline void setNPerm(Arr &arr, int idx, int n, int even = 0)
{
    if (even < 0)
        idx *= 2;
    Arr avail(n);
    for (int i = 0; i < n; ++i)
        avail[i] = i;
    arr.assign(n, 0);
    int prt = 0;
    for (int i = 0; i < n - 1; ++i) {
        const int p = fact(n - 1 - i);
        const int v = idx / p;
        idx -= v * p;
        prt ^= v;
        arr[i] = avail[v];
        avail.erase(avail.begin() + v);
    }
    if (even < 0 && (prt & 1)) {
        arr[n - 1] = arr[n - 2];
        arr[n - 2] = avail[0];
    } else {
        arr[n - 1] = avail[0];
    }
}

// mathlib.getNPerm(arr, n, even)
inline int getNPerm(const Arr &arr, int n, int even = 0)
{
    int idx = 0;
    for (int i = 0; i < n - 1; ++i) {
        idx *= n - i;
        for (int j = i + 1; j < n; ++j)
            if (arr[j] < arr[i])
                ++idx;
    }
    return even < 0 ? idx / 2 : idx;
}

// mathlib.getNOri(arr, n, evenbase); evenbase < 0: the last piece's orientation is implied.
inline int getNOri(const Arr &arr, int n, int evenbase)
{
    const int base = evenbase < 0 ? -evenbase : evenbase;
    int idx = evenbase < 0 ? 0 : arr[0] % base;
    for (int i = n - 1; i > 0; --i)
        idx = idx * base + arr[i] % base;
    return idx;
}

// mathlib.setNOri(arr, idx, n, evenbase)
inline void setNOri(Arr &arr, int idx, int n, int evenbase)
{
    const int base = evenbase < 0 ? -evenbase : evenbase;
    int parity = base * n;
    arr.assign(n, 0);
    for (int i = 1; i < n; ++i) {
        arr[i] = idx % base;
        parity -= arr[i];
        idx /= base;
    }
    arr[0] = (evenbase < 0 ? parity : idx) % base;
}

// mathlib.acycle(arr, perm, pow, ori): cycle the pieces at positions perm by pow steps. With
// ori, its LAST element is a base added to every entry (JS ori.at(-1)).
template <class T>
inline void acycle(std::vector<T> &arr, const Arr &perm, int pow = 1, const Arr *ori = nullptr)
{
    const int plen = static_cast<int>(perm.size());
    std::vector<T> tmp(plen);
    for (int i = 0; i < plen; ++i)
        tmp[i] = arr[perm[i]];
    for (int i = 0; i < plen; ++i) {
        const int j = (i + pow) % plen;
        arr[perm[j]] = tmp[i];
        if (ori)
            arr[perm[j]] += (*ori)[j] - (*ori)[i] + ori->back();
    }
}

// mathlib.Coord, 'p' (permutation) and 'o' (orientation) variants.
struct Coord {
    char type;
    int length;
    int evenbase;   // 0: JS undefined
    int get(const Arr &arr) const
    {
        return type == 'p' ? getNPerm(arr, length, evenbase) : getNOri(arr, length, evenbase);
    }
    void set(Arr &arr, int idx) const
    {
        if (type == 'p')
            setNPerm(arr, idx, length, evenbase);
        else
            setNOri(arr, idx, length, evenbase);
    }
};

// mathlib.getPruning(table, index): 4-bit values packed eight to an int32 word.
inline int getPruning(const std::vector<int32_t> &table, int index)
{
    return (table[index >> 3] >> ((index & 7) << 2)) & 15;
}

inline void xorNibble(std::vector<int32_t> &table, int index, int32_t value)
{
    table[index >> 3] ^= static_cast<int32_t>(static_cast<uint32_t>(value) << ((index & 7) << 2));
}

// mathlib.createPrun over a prebuilt move table (the form Solver uses).
inline void createPrun(std::vector<int32_t> &prun, int init, int size, const std::vector<Arr> &moveTable,
                       int nMoves, int nPower, int nInv, int maxd)
{
    prun.assign((size + 7) >> 3, -1);
    xorNibble(prun, init, 15);
    int32_t val = 0;
    for (int l = 0; l <= maxd; ++l) {
        int done = 0;
        const bool inv = l >= nInv;
        const int32_t fill = (l + 1) ^ 15;
        const int find = inv ? 0xf : l;
        const int check = inv ? l : 0xf;
        for (int p = 0; p < size; ++p, val >>= 4) {
            if ((p & 7) == 0) {
                val = prun[p >> 3];
                if (!inv && val == -1) {
                    p += 7;
                    continue;
                }
            }
            if ((val & 0xf) != find)
                continue;
            bool found = false;   // JS `continue out` (inverse mode: stop at the first hit)
            for (int m = 0; m < nMoves && !found; ++m) {
                int q = p;
                for (int c = 0; c < nPower; ++c) {
                    q = moveTable[m][q];
                    if (q < 0)
                        break;
                    if (getPruning(prun, q) != check)
                        continue;
                    ++done;
                    if (inv) {
                        xorNibble(prun, p, fill);
                        found = true;
                        break;
                    }
                    xorNibble(prun, q, fill);
                }
            }
        }
        if (done == 0)
            break;
    }
}

struct Move {
    int axis;
    int pow;
};

using ArrMoveFn = void (*)(Arr &arr, int move);
using IdxMoveFn = int (*)(int idx, int move);

// mathlib.Solver: coordinates sharing one move set, searched with IDA*.
class Solver {
public:
    Solver(int nMoves, int nPower) : nMoves_(nMoves), nPower_(nPower) {}

    // State over a Coord with an array move function (JS [init, [fn, type, len, evenbase], size]).
    void addCoordState(int init, int size, Coord cord, ArrMoveFn fn) { states_.push_back({ init, size, cord, fn, nullptr, {}, {} }); }
    // State over an index move function (JS [init, fn, size]).
    void addIdxState(int init, int size, IdxMoveFn fn) { states_.push_back({ init, size, Coord(), nullptr, fn, {}, {} }); }

    void init()
    {
        for (State &st : states_) {
            st.move.assign(nMoves_, Arr(st.size, 0));
            Arr arr;
            for (int j = 0; j < nMoves_; ++j)
                for (int k = 0; k < st.size; ++k) {
                    if (st.arrFn) {
                        st.cord.set(arr, k);
                        st.arrFn(arr, j);
                        st.move[j][k] = st.cord.get(arr);
                    } else {
                        st.move[j][k] = st.idxFn(k, j);
                    }
                }
            createPrun(st.prun, st.init, st.size, st.move, nMoves_, nPower_, 256, 256);
        }
    }

    // Solver.search(state, minl, maxl); maxl <= 0 means JS undefined (99). Returns false when
    // no solution exists within the bound (JS null). sol holds the moves in search order.
    bool search(const Arr &state, int minl, int maxl, std::vector<Move> &sol) const
    {
        const int maxLen = (maxl <= 0 ? 99 : maxl) + 1;
        long long cost = 1000000000LL + 1;
        sol.clear();
        for (int length = minl; length <= maxLen; ++length)
            if (idaSearch(state, length, 0, -1, sol, cost) == 0)
                return cost > 0;
        return false;
    }

    // Solver.toStr(sol, moveMap, powerMap)
    static std::string toStr(const std::vector<Move> &sol, const std::string &moveMap,
                             const std::vector<std::string> &powerMap)
    {
        std::string s;
        for (size_t i = 0; i < sol.size(); ++i) {
            if (i)
                s += ' ';
            s += moveMap[sol[i].axis];
            s += powerMap[sol[i].pow];
        }
        return s;
    }

private:
    struct State {
        int init;
        int size;
        Coord cord;
        ArrMoveFn arrFn;
        IdxMoveFn idxFn;
        std::vector<Arr> move;
        std::vector<int32_t> prun;
    };

    int nMoves_, nPower_;
    std::vector<State> states_;

    int getPrun(const Arr &idx) const
    {
        int prun = 0;
        for (size_t i = 0; i < states_.size(); ++i) {
            const int v = getPruning(states_[i].prun, idx[i]);
            if (v > prun)
                prun = v;
        }
        return prun;
    }

    // Returns 0: solved (or out of budget), 1: dead end, 2: skip the remaining powers of this axis.
    int idaSearch(const Arr &idx, int maxl, int depth, int lm, std::vector<Move> &sol, long long &cost) const
    {
        if (--cost <= 0)
            return 0;
        const int prun = getPrun(idx);
        if (prun > maxl)
            return prun > maxl + 1 ? 2 : 1;
        if (maxl == 0)
            return 0;   // isSolved()/callback() are always true for these puzzles
        if (prun == 0 && maxl == 1)
            return 1;
        Arr next(idx.size());
        for (int axis = 0; axis < nMoves_; ++axis) {
            if (axis == lm)   // ckmv[lm] = 1 << lm: never the same axis twice in a row
                continue;
            next = idx;
            for (int pow = 0; pow < nPower_; ++pow) {
                for (size_t i = 0; i < states_.size(); ++i)
                    next[i] = states_[i].move[axis][next[i]];
                sol.resize(depth + 1);
                sol[depth] = { axis, pow };
                const int ret = idaSearch(next, maxl - 1, depth + 1, axis, sol, cost);
                if (ret == 0)
                    return 0;
                sol.resize(depth);
                if (ret == 2)
                    break;
            }
        }
        return 1;
    }
};

} // namespace ml
} // namespace ct

#endif
