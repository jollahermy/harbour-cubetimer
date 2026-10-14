#include "scramble.h"
#include "modules.h"

namespace ct {

static const std::vector<std::string> kCubeSuff = { "", "2", "'" };

// scramble.js mega(turns, suffixes, length). csTimer allows a turn to be a list (rndEl of it);
// none of the WCA events use that, so turns are plain strings here.
std::string Scrambler::mega(const std::vector<std::vector<std::string>> &turns,
                            const std::vector<std::string> &suffixes, int length)
{
    int donemoves = 0, lastaxis = -1;
    std::string out;
    for (int i = 0; i < length; ++i) {
        int first, second;
        do {
            first = rng_.rn(static_cast<int>(turns.size()));
            second = rng_.rn(static_cast<int>(turns[first].size()));
            if (first != lastaxis) {
                donemoves = 0;
                lastaxis = first;
            }
        } while (((donemoves >> second) & 1) != 0);
        donemoves |= 1 << second;
        if (i)
            out += ' ';
        out += turns[first][second] + rng_.rndEl(suffixes);
    }
    return out;
}

std::string Scrambler::cubeBig(int n, int length)
{
    // megascramble.js args "555wca" / "666wca" / "777wca"
    std::vector<std::vector<std::string>> t;
    if (n == 5)
        t = { { "U", "D", "Uw", "Dw" }, { "R", "L", "Rw", "Lw" }, { "F", "B", "Fw", "Bw" } };
    else if (n == 6)
        t = { { "U", "D", "Uw", "Dw", "3Uw" }, { "R", "L", "Rw", "Lw", "3Rw" }, { "F", "B", "Fw", "Bw", "3Fw" } };
    else
        t = { { "U", "D", "Uw", "Dw", "3Uw", "3Dw" }, { "R", "L", "Rw", "Lw", "3Rw", "3Lw" },
              { "F", "B", "Fw", "Bw", "3Fw", "3Bw" } };
    return mega(t, kCubeSuff, length);
}

std::string Scrambler::clockWca()
{
    // utilscramble.js case "clkwca": 14 random dial moves; the template's 4 pin '?' are sliced
    // off, but the 4 pin draws still happen (their replace() finds nothing), so keep them.
    static const std::vector<std::string> clkapp = { "0+", "1+", "2+", "3+", "4+", "5+", "6+",
                                                     "1-", "2-", "3-", "4-", "5-" };
    static const char *parts[] = { "UR", " DR", " DL", " UL", " U", " R", " D", " L", " ALL",
                                   " y2 U", " R", " D", " L", " ALL" };
    std::string out;
    for (int i = 0; i < 14; ++i)
        out += std::string(parts[i]) + rng_.rndEl(clkapp);
    static const std::vector<std::string> p1 = { "", " UR" }, p2 = { "", " DR" }, p3 = { "", " DL" }, p4 = { "", " UL" };
    rng_.rndEl(p1); rng_.rndEl(p2); rng_.rndEl(p3); rng_.rndEl(p4);
    return out;
}

std::string Scrambler::megaminxPochmann(int length)
{
    // utilscramble.js pochscramble(10, ceil(len / 10)); "~" is removed by toTxt.
    static const std::vector<std::string> pm = { "++", "--" };
    int y = (length + 9) / 10;
    std::string out;
    for (int i = 0; i < y; ++i) {
        std::string line;
        std::string last;
        for (int j = 0; j < 10; ++j) {
            last = rng_.rndEl(pm);
            line += std::string(j % 2 == 0 ? "R" : "D") + last + " ";
        }
        line += (last == "--") ? "U'" : "U";
        if (i)
            out += '\n';
        out += line;
    }
    return out;
}

// The bundle's generate(): collapse spaces/tabs, trim around newlines, trim the ends.
std::string normalizeScramble(const std::string &raw)
{
    std::string s;
    bool sp = false;
    for (char c : raw) {
        if (c == ' ' || c == '\t') {
            sp = true;
            continue;
        }
        if (c == '\n') {
            sp = false;
            while (!s.empty() && s.back() == ' ')
                s.pop_back();
            s += '\n';
            continue;
        }
        if (sp && !s.empty() && s.back() != '\n')
            s += ' ';
        sp = false;
        s += c;
    }
    while (!s.empty() && (s.back() == ' ' || s.back() == '\n'))
        s.pop_back();
    size_t b = 0;
    while (b < s.size() && (s[b] == ' ' || s[b] == '\n'))
        ++b;
    return s.substr(b);
}

bool Scrambler::supported(const std::string &e)
{
    return e == "555" || e == "555bld" || e == "666" || e == "777" || e == "clock" || e == "minx"
        || gen333Supported(e) || gen222Supported(e) || genPyrSupported(e) || genSkbSupported(e) || genSq1Supported(e);
}

std::string Scrambler::generate(const std::string &e)
{
    std::string s;
    if (e == "555") s = cubeBig(5, 60);
    else if (e == "666") s = cubeBig(6, 80);
    else if (e == "777") s = cubeBig(7, 100);
    else if (e == "clock") s = clockWca();
    else if (e == "minx") s = megaminxPochmann(70);
    else if (e == "555bld") {
        // '${["555wca",60]}#{[[""]],[...3Rw...],1}#{[[""]],[...3Uw...],1}'
        s = cubeBig(5, 60);
        s += mega({ { "" } }, { "", " 3Rw", " 3Rw2", " 3Rw'", " 3Fw", " 3Fw'" }, 1);
        s += mega({ { "" } }, { "", " 3Uw", " 3Uw2", " 3Uw'" }, 1);
    } else if (gen333Supported(e)) s = gen333(rng_, e);
    else if (gen222Supported(e)) s = gen222(rng_, e);
    else if (genPyrSupported(e)) s = genPyr(rng_, e);
    else if (genSkbSupported(e)) s = genSkb(rng_, e);
    else if (genSq1Supported(e)) s = genSq1(rng_, e);
    else
        return std::string();
    return normalizeScramble(s);
}

} // namespace ct
