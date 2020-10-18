
#include "json-export.h"

namespace msdf_atlas {

static const char * imageTypeString(ImageType type) {
    switch (type) {
        case ImageType::HARD_MASK:
            return "hardmask";
        case ImageType::SOFT_MASK:
            return "softmask";
        case ImageType::SDF:
            return "sdf";
        case ImageType::PSDF:
            return "psdf";
        case ImageType::MSDF:
            return "msdf";
        case ImageType::MTSDF:
            return "mtsdf";
    }
    return nullptr;
}

bool exportJSON(msdfgen::FontHandle *font, const GlyphGeometry *glyphs, int glyphCount, GlyphIdentifierType glyphIdentifierType, double fontSize, double pxRange, int atlasWidth, int atlasHeight, ImageType imageType, const char *filename) {
    msdfgen::FontMetrics fontMetrics;
    if (!msdfgen::getFontMetrics(fontMetrics, font))
        return false;
    double fsScale = 1/fontMetrics.emSize;

    FILE *f = fopen(filename, "w");
    if (!f)
        return false;
    fputs("{", f);

    // Atlas properties
    fputs("\"atlas\":{", f); {
        fprintf(f, "\"type\":\"%s\",", imageTypeString(imageType));
        if (imageType == ImageType::SDF || imageType == ImageType::PSDF || imageType == ImageType::MSDF || imageType == ImageType::MTSDF)
            fprintf(f, "\"distanceRange\":%.17g,", pxRange);
        fprintf(f, "\"size\":%.17g,", fontSize);
        fprintf(f, "\"width\":%d,", atlasWidth);
        fprintf(f, "\"height\":%d,", atlasHeight);
        fputs("\"yOrigin\":\"bottom\"", f);
    } fputs("},", f);

    // Font metrics
    fputs("\"metrics\":{", f); {
        fprintf(f, "\"lineHeight\":%.17g,", fsScale*fontMetrics.lineHeight);
        fprintf(f, "\"ascender\":%.17g,", fsScale*fontMetrics.ascenderY);
        fprintf(f, "\"descender\":%.17g,", fsScale*fontMetrics.descenderY);
        fprintf(f, "\"underlineY\":%.17g,", fsScale*fontMetrics.underlineY);
        fprintf(f, "\"underlineThickness\":%.17g", fsScale*fontMetrics.underlineThickness);
    } fputs("},", f);

    // Glyph mapping
    fputs("\"glyphs\":[", f);
    for (int i = 0; i < glyphCount; ++i) {
        fputs(i == 0 ? "{" : ",{", f);
        switch (glyphIdentifierType) {
            case GlyphIdentifierType::GLYPH_INDEX:
                fprintf(f, "\"index\":%d,", glyphs[i].getIndex());
                break;
            case GlyphIdentifierType::UNICODE_CODEPOINT:
                fprintf(f, "\"unicode\":%u,", glyphs[i].getCodepoint());
                break;
        }
        fprintf(f, "\"advance\":%.17g", fsScale*glyphs[i].getAdvance());
        double l, b, r, t;
        glyphs[i].getQuadPlaneBounds(l, b, r, t);
        if (l || b || r || t)
            fprintf(f, ",\"planeBounds\":{\"left\":%.17g,\"bottom\":%.17g,\"right\":%.17g,\"top\":%.17g}", fsScale*l, fsScale*b, fsScale*r, fsScale*t);
        glyphs[i].getQuadAtlasBounds(l, b, r, t);
        if (l || b || r || t)
            fprintf(f, ",\"atlasBounds\":{\"left\":%.17g,\"bottom\":%.17g,\"right\":%.17g,\"top\":%.17g}", l, b, r, t);
        fputs("}", f);
    }
    fputs("],", f);

    // Kerning pairs
    fputs("\"kerning\":[", f);
    bool firstPair = true;
    for (int i = 0; i < glyphCount; ++i) {
        for (int j = 0; j < glyphCount; ++j) {
            double kerning;
            if (msdfgen::getKerning(kerning, font, glyphs[i].getCodepoint(), glyphs[j].getCodepoint()) && kerning) {
                fputs(firstPair ? "{" : ",{", f);
                fprintf(f, "\"unicode1\":%u,", glyphs[i].getCodepoint());
                fprintf(f, "\"unicode2\":%u,", glyphs[j].getCodepoint());
                fprintf(f, "\"advance\":%.17g", fsScale*kerning);
                fputs("}", f);
                firstPair = false;
            }
        }
    }
    fputs("]", f);

    fputs("}\n", f);
    fclose(f);
    return true;
}

}
