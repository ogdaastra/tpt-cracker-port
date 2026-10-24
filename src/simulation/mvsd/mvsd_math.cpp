#include <cmath>
#include "simulation/mvsd/mvsd_math.h"

// https://www.dsprelated.com/showarticle/1052.php
float fast_atan2(float y, float x) {
    const float n1 = 0.97239411f;
    const float n2 = -0.19194795f;    
    float result = 0.0f;
    if (x != 0.0f) {
        const union { float flVal; uint32 nVal; } tYSign = { y };
        const union { float flVal; uint32 nVal; } tXSign = { x };
        if (fabsf(x) >= fabsf(y)) {
            union { float flVal; uint32 nVal; } tOffset = { PI };
            // Add or subtract PI based on y's sign.
            tOffset.nVal |= tYSign.nVal & 0x80000000u;
            // No offset if x is positive, so multiply by 0 or based on x's sign.
            tOffset.nVal *= tXSign.nVal >> 31;
            result = tOffset.flVal;
            const float z = y / x;
            result += (n1 + n2 * z * z) * z;
        }
        else // Use atan(y/x) = pi/2 - atan(x/y) if |y/x| > 1.
        {
            union { float flVal; uint32 nVal; } tOffset = { PI_2 };
            // Add or subtract PI/2 based on y's sign.
            tOffset.nVal |= tYSign.nVal & 0x80000000u;            
            result = tOffset.flVal;
            const float z = x / y;
            result -= (n1 + n2 * z * z) * z;            
        }
    }
    else if (y > 0.0f) {
        result = PI_2;
    }
    else if (y < 0.0f) {
        result = -PI_2;
    }
    return result;
}

bool is_same_sign(float a, float b) {
    return (a < 0) == (b < 0);
}