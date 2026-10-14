#include "rng.h"

namespace ct {

Rng::Rng() { seed("0"); }

void Rng::reset()
{
    acc_ = brs_ = cnt_ = 0;
    for (int i = 0; i < 256; ++i)
        m_[i] = r_[i] = 0;
    gnt_ = 0;
}

void Rng::seed(const std::string &str)
{
    // isaac.seed(array): reset, add the seed words into r[], then two mixing passes.
    reset();
    for (size_t i = 0; i < str.size(); ++i)
        r_[i & 0xff] += static_cast<unsigned char>(str[i]);
    uint32_t a, b, c, d, e, f, g, h;
    a = b = c = d = e = f = g = h = 0x9e3779b9u;
    auto mix = [&]() {
        a ^= b << 11; d += a; b += c;
        b ^= c >> 2;  e += b; c += d;
        c ^= d << 8;  f += c; d += e;
        d ^= e >> 16; g += d; e += f;
        e ^= f << 10; h += e; f += g;
        f ^= g >> 4;  a += f; g += h;
        g ^= h << 8;  b += g; h += a;
        h ^= a >> 9;  c += h; a += b;
    };
    for (int i = 0; i < 4; ++i)
        mix();
    for (int i = 0; i < 256; i += 8) {
        a += r_[i]; b += r_[i + 1]; c += r_[i + 2]; d += r_[i + 3];
        e += r_[i + 4]; f += r_[i + 5]; g += r_[i + 6]; h += r_[i + 7];
        mix();
        m_[i] = a; m_[i + 1] = b; m_[i + 2] = c; m_[i + 3] = d;
        m_[i + 4] = e; m_[i + 5] = f; m_[i + 6] = g; m_[i + 7] = h;
    }
    for (int i = 0; i < 256; i += 8) {
        a += m_[i]; b += m_[i + 1]; c += m_[i + 2]; d += m_[i + 3];
        e += m_[i + 4]; f += m_[i + 5]; g += m_[i + 6]; h += m_[i + 7];
        mix();
        m_[i] = a; m_[i + 1] = b; m_[i + 2] = c; m_[i + 3] = d;
        m_[i + 4] = e; m_[i + 5] = f; m_[i + 6] = g; m_[i + 7] = h;
    }
    prng();
    gnt_ = 256;
}

void Rng::prng()
{
    cnt_ += 1;
    brs_ += cnt_;
    for (int i = 0; i < 256; ++i) {
        switch (i & 3) {
        case 0: acc_ ^= acc_ << 13; break;
        case 1: acc_ ^= acc_ >> 6; break;
        case 2: acc_ ^= acc_ << 2; break;
        case 3: acc_ ^= acc_ >> 16; break;
        }
        acc_ = m_[(i + 128) & 0xff] + acc_;
        uint32_t x = m_[i];
        uint32_t y = m_[(x >> 2) & 0xff] + (acc_ + brs_);
        m_[i] = y;
        brs_ = m_[(y >> 10) & 0xff] + x;
        r_[i] = brs_;
    }
}

uint32_t Rng::rand()
{
    if (!gnt_--) {
        prng();
        gnt_ = 255;
    }
    return r_[gnt_];
}

double Rng::random()
{
    uint32_t hi = rand() >> 5;   // evaluated first, as in the JS expression
    uint32_t lo = rand() >> 6;
    return (hi * 67108864.0 + lo) / 9007199254740992.0;
}

} // namespace ct
