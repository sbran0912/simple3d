#include "math.hpp"

#include <algorithm>
#include <random>

// Importiert werden nur die tatsaechlich verwendeten std-Namen.
using std::floor;
using std::max;
using std::min;
using std::mt19937;
using std::random_device;
using std::uniform_real_distribution;

namespace {

mt19937& rng() {
    static mt19937 generator{random_device{}()};
    return generator;
}

} // namespace

Vec2 Project(double fov, const Vec3& v) {
    const double s = fov / (fov + v.z);
    return Vec2{v.x * s, v.y * s, s};
}

double Constrain(double value, double lo, double hi) {
    return min(hi, max(lo, value));
}

int Random(int n1, int n2) {
    return static_cast<int>(floor(RandomFloat(0.0, 1.0) * (n2 - n1) + n1));
}

double RandomFloat(double lo, double hi) {
    uniform_real_distribution<double> dist(0.0, 1.0);
    return dist(rng()) * (hi - lo) + lo;
}
