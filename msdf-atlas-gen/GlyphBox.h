
#pragma once

#include "types.h"

namespace msdf_atlas {

/// The glyph box - its bounds in plane and atlas
struct GlyphBox {
    unicode_t codepoint;
    double advance;
    struct {
        double l, b, r, t;
    } bounds;
    struct {
        int x, y, w, h;
    } rect;

};

}
