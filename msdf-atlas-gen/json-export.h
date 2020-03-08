
#pragma once

#include <msdfgen.h>
#include <msdfgen-ext.h>
#include "types.h"
#include "GlyphGeometry.h"

namespace msdf_atlas {

/// Writes the font and glyph metrics and atlas layout data into a comprehensive JSON file
bool exportJSON(msdfgen::FontHandle *font, const GlyphGeometry *glyphs, int glyphCount, double fontSize, double pxRange, int atlasWidth, int atlasHeight, ImageType imageType, const char *filename);

}
