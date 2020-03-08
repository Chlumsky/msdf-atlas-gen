
#pragma once

#include <msdfgen.h>
#include <msdfgen-ext.h>
#include "types.h"
#include "GlyphGeometry.h"

namespace msdf_atlas {

/// Generates a Shadron script that displays a string using the generated atlas
bool generateShadronPreview(msdfgen::FontHandle *font, const GlyphGeometry *glyphs, int glyphCount, ImageType atlasType, int atlasWidth, int atlasHeight, double pxRange, const unicode_t *text, const char *imageFilename, const char *outputFilename);

}
