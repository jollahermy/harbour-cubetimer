// Test driver: scramble_cli <seedPrefix> <event> <count>
// For i in [0, count): seed(seedPrefix + i), print one scramble per line ('\n' shown as " | ").
#include <cstdio>
#include <cstdlib>
#include <string>
#include <chrono>
#include "scramble.h"

int main(int argc, char **argv)
{
    if (argc < 4) {
        std::fprintf(stderr, "usage: %s seedPrefix event count\n", argv[0]);
        return 2;
    }
    ct::Scrambler s;
    int n = std::atoi(argv[3]);
    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < n; ++i) {
        s.seed(std::string(argv[1]) + std::to_string(i));
        std::string out = s.generate(argv[2]);
        std::string line;
        for (char c : out) line += (c == '\n') ? std::string(" | ") : std::string(1, c);
        std::printf("%s\n", line.c_str());
    }
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
    std::fprintf(stderr, "%d scrambles in %lld ms\n", n, static_cast<long long>(ms));
    return 0;
}
