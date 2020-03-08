
#pragma once

#include "GlyphGeometry.h"

namespace msdf_atlas {

/**
 * Writes the positioning data and atlas layout of the glyphs into a CSV file
 * The columns are: Unicode index, horizontal advance, plane bounds (l, b, r, t), atlas bounds (l, b, r, t)
 */
bool exportCSV(const GlyphGeometry *glyphs, int glyphCount, double emSize, const char *filename);

}
