
#include "csv-export.h"

#include <cstdio>

namespace msdf_atlas {

bool exportCSV(const GlyphGeometry *glyphs, int glyphCount, double emSize, const char *filename) {
    FILE *f = fopen(filename, "w");
    if (!f)
        return false;

    double fsScale = 1/emSize;
    for (int i = 0; i < glyphCount; ++i) {
        double l, b, r, t;
        fprintf(f, "%u,%.17g,", glyphs[i].getCodepoint(), fsScale*glyphs[i].getAdvance());
        glyphs[i].getQuadPlaneBounds(l, b, r, t);
        fprintf(f, "%.17g,%.17g,%.17g,%.17g,", fsScale*l, fsScale*b, fsScale*r, fsScale*t);
        glyphs[i].getQuadAtlasBounds(l, b, r, t);
        fprintf(f, "%.17g,%.17g,%.17g,%.17g\n", l, b, r, t);
    }

    fclose(f);
    return true;
}

}
