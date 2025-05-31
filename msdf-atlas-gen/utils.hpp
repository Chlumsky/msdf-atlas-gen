
#pragma once

namespace msdf_atlas {

/// Floor positive integer to nearest lower or equal power of two
inline int floorToPOT(int x) {
    int y = 1;
    while (x >= y && y<<1)
        y <<= 1;
    return y>>1;
}

/// Ceil positive integer to nearest higher or equal power of two
inline int ceilToPOT(int x) {
    int y = 1;
    while (x > y && y<<1)
        y <<= 1;
    return y;
}

}
