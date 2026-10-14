// Random numbers for the WCA scramblers: a bit-exact C++ port of csTimer's RNG
// (lib/isaac.js: ISAAC by Yves-Marie K. Rinquin, MIT; lib/mathlib.js randGen/rn/rndEl, GPL-3.0).
// Bit-exact on purpose: with the same seed string the C++ scramblers must reproduce csTimer's
// output, which is how the port is verified.
#ifndef CT_RNG_H
#define CT_RNG_H

#include <cstdint>
#include <string>
#include <vector>

namespace ct {

class Rng {
public:
    Rng();
    // mathlib.setSeed(0, str): ISAAC seeded with the UTF-16 code units of str (ASCII here).
    void seed(const std::string &str);
    // isaac.random(): 53-bit double in [0, 1).
    double random();
    // mathlib.rn(n) = ~~(random() * n)
    int rn(int n) { return static_cast<int>(random() * n); }
    template <class T> const T &rndEl(const std::vector<T> &v) { return v[rn(static_cast<int>(v.size()))]; }

private:
    uint32_t m_[256], r_[256];
    uint32_t acc_, brs_, cnt_;
    int gnt_;
    void reset();
    void prng();
    uint32_t rand();
};

} // namespace ct

#endif
