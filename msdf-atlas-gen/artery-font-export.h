
#pragma once

#include <msdfgen.h>
#include <msdfgen-ext.h>
#include "types.h"
#include "GlyphGeometry.h"

namespace msdf_atlas {

struct ArteryFontExportProperties {
    GlyphIdentifierType glyphIdentifierType;
    double fontSize;
    double pxRange;
    ImageType imageType;
    ImageFormat imageFormat;
};

/// Encodes the atlas bitmap and its layout into an Artery Atlas Font file
template <typename REAL, typename T, int N>
bool exportArteryFont(msdfgen::FontHandle *font, const GlyphGeometry *glyphs, int glyphCount, const msdfgen::BitmapConstRef<T, N> &atlas, const char *filename, const ArteryFontExportProperties &properties);

}
