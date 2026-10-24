#include "simulation/circuits/util.h"

float restrict_double_to_flt(double n) {
    float max = std::numeric_limits<float>::max();
    if (n > max) return max;
    if (n < -max) return -max;
    return (float)n;
}