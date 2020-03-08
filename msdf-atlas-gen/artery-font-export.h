
#pragma once

#include <msdfgen.h>
#include <msdfgen-ext.h>
#include "types.h"
#include "GlyphGeometry.h"

namespace msdf_atlas {

/// Encodes the atlas bitmap and its layout into an Artery Atlas Font file
template <typename REAL, typename T, int N>
bool exportArteryFont(msdfgen::FontHandle *font, const GlyphGeometry *glyphs, int glyphCount, double fontSize, double pxRange, const msdfgen::BitmapConstRef<T, N> &atlas, ImageType imageType, ImageFormat imageFormat, const char *filename);

}
