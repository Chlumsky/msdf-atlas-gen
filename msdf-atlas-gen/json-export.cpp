
#include "json-export.h"

#include "GlyphGeometry.h"

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

bool exportJSON(const FontGeometry *fonts, int fontCount, double fontSize, double pxRange, int atlasWidth, int atlasHeight, ImageType imageType, const char *filename, bool kerning) {
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

    if (fontCount > 1)
        fputs("\"variants\":[", f);
    for (int i = 0; i < fontCount; ++i) {
        const FontGeometry &font = fonts[i];
        if (fontCount > 1)
            fputs(i == 0 ? "{" : ",{", f);

        // Font metrics
        fputs("\"metrics\":{", f); {
            const msdfgen::FontMetrics &metrics = font.getMetrics();
            fprintf(f, "\"emSize\":%.17g,", metrics.emSize);
            fprintf(f, "\"lineHeight\":%.17g,", metrics.lineHeight);
            fprintf(f, "\"ascender\":%.17g,", metrics.ascenderY);
            fprintf(f, "\"descender\":%.17g,", metrics.descenderY);
            fprintf(f, "\"underlineY\":%.17g,", metrics.underlineY);
            fprintf(f, "\"underlineThickness\":%.17g", metrics.underlineThickness);
        } fputs("},", f);

        // Glyph mapping
        fputs("\"glyphs\":[", f);
        bool firstGlyph = true;
        for (const GlyphGeometry &glyph : font.getGlyphs()) {
            fputs(firstGlyph ? "{" : ",{", f);
            switch (font.getPreferredIdentifierType()) {
                case GlyphIdentifierType::GLYPH_INDEX:
                    fprintf(f, "\"index\":%d,", glyph.getIndex());
                    break;
                case GlyphIdentifierType::UNICODE_CODEPOINT:
                    fprintf(f, "\"unicode\":%u,", glyph.getCodepoint());
                    break;
            }
            fprintf(f, "\"advance\":%.17g", glyph.getAdvance());
            double l, b, r, t;
            glyph.getQuadPlaneBounds(l, b, r, t);
            if (l || b || r || t)
                fprintf(f, ",\"planeBounds\":{\"left\":%.17g,\"bottom\":%.17g,\"right\":%.17g,\"top\":%.17g}", l, b, r, t);
            glyph.getQuadAtlasBounds(l, b, r, t);
            if (l || b || r || t)
                fprintf(f, ",\"atlasBounds\":{\"left\":%.17g,\"bottom\":%.17g,\"right\":%.17g,\"top\":%.17g}", l, b, r, t);
            fputs("}", f);
            firstGlyph = false;
        } fputs("]", f);

        // Kerning pairs
        if (kerning) {
            fputs(",\"kerning\":[", f);
            bool firstPair = true;
            switch (font.getPreferredIdentifierType()) {
                case GlyphIdentifierType::GLYPH_INDEX:
                    for (const std::pair<std::pair<int, int>, double> &kernPair : font.getKerning()) {
                        fputs(firstPair ? "{" : ",{", f);
                        fprintf(f, "\"index1\":%d,", kernPair.first.first);
                        fprintf(f, "\"index2\":%d,", kernPair.first.second);
                        fprintf(f, "\"advance\":%.17g", kernPair.second);
                        fputs("}", f);
                        firstPair = false;
                    }
                    break;
                case GlyphIdentifierType::UNICODE_CODEPOINT:
                    for (const std::pair<std::pair<int, int>, double> &kernPair : font.getKerning()) {
                        const GlyphGeometry *glyph1 = font.getGlyph(msdfgen::GlyphIndex(kernPair.first.first));
                        const GlyphGeometry *glyph2 = font.getGlyph(msdfgen::GlyphIndex(kernPair.first.second));
                        if (glyph1 && glyph2 && glyph1->getCodepoint() && glyph2->getCodepoint()) {
                            fputs(firstPair ? "{" : ",{", f);
                            fprintf(f, "\"unicode1\":%u,", glyph1->getCodepoint());
                            fprintf(f, "\"unicode2\":%u,", glyph2->getCodepoint());
                            fprintf(f, "\"advance\":%.17g", kernPair.second);
                            fputs("}", f);
                            firstPair = false;
                        }
                    }
                    break;
            } fputs("]", f);
        }

        if (fontCount > 1)
            fputs("}", f);
    }
    if (fontCount > 1)
        fputs("]", f);

    fputs("}\n", f);
    fclose(f);
    return true;
}

}
