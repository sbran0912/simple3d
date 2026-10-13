#include "color.hpp"

#include <cctype>
#include <cstdint>

// Importiert werden nur die tatsaechlich verwendeten std-Namen.
using std::string;
using std::uint32_t;

namespace {

int hexVal(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

} // namespace

ColorState ParseHex(const string& s) {
    string h = s;
    if (!h.empty() && h[0] == '#') {
        h.erase(h.begin());
    }
    if (h.size() == 3) {
        h = string{h[0], h[0], h[1], h[1], h[2], h[2]};
    }
    if (h.size() != 6) {
        return ColorState{1, 1, 1, 1};
    }

    uint32_t n = 0;
    for (char c : h) {
        const int v = hexVal(c);
        if (v < 0) {
            return ColorState{1, 1, 1, 1};
        }
        n = (n << 4) | static_cast<uint32_t>(v);
    }

    return ColorState{
        static_cast<double>((n >> 16) & 0xff) / 255,
        static_cast<double>((n >> 8) & 0xff) / 255,
        static_cast<double>(n & 0xff) / 255,
        1,
    };
}

ColorState ParseColor(const string& hex) { return ParseHex(hex); }

ColorState ParseColor(double gray) {
    const double g = gray / 255;
    return ColorState{g, g, g, 1};
}

ColorState ParseColor(double r, double g, double b) {
    return ColorState{r / 255, g / 255, b / 255, 1};
}

ColorState ParseColor(double r, double g, double b, double a) {
    return ColorState{r / 255, g / 255, b / 255, a / 255};
}

bool ColEq(const ColorState& a, const ColorState& b) {
    return a.R == b.R && a.G == b.G && a.B == b.B && a.A == b.A;
}
