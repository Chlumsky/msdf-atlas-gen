
#include "shadron-preview-generator.h"

#include <string>
#include <algorithm>

namespace msdf_atlas {

static const char *const shadronFillGlyphMask = R"(
template <ATLAS, RANGE, ZERO_DIST, COLOR>
glsl vec4 fillGlyph(vec2 texCoord) {
    float fill = texture((ATLAS), texCoord).r;
    return vec4(vec3(COLOR), fill);
}
)";

static const char *const shadronFillGlyphSdf = R"(
template <ATLAS, RANGE, ZERO_DIST, COLOR>
glsl vec4 fillGlyph(vec2 texCoord) {
    vec3 s = texture((ATLAS), texCoord).rgb;
    float sd = dot(vec2(RANGE), 0.5/fwidth(texCoord))*(median(s.r, s.g, s.b)-ZERO_DIST);
    float fill = clamp(sd+0.5, 0.0, 1.0);
    return vec4(vec3(COLOR), fill);
}
)";

static const char *const shadronPreviewPreamble = R"(
#include <median>

glsl struct GlyphVertex {
    vec2 coord;
    vec2 texCoord;
};

template <TEXT_SIZE>
glsl vec4 projectVertex(out vec2 texCoord, in GlyphVertex vertex) {
    vec2 coord = vertex.coord;
    float scale = 2.0/max((TEXT_SIZE).x, shadron_Aspect*(TEXT_SIZE).y);
    scale *= exp(0.0625*shadron_Mouse.z);
    coord += vec2(-0.5, 0.5)*vec2(TEXT_SIZE);
    coord *= scale*vec2(1.0, shadron_Aspect);
    texCoord = vertex.texCoord;
    return vec4(coord, 0.0, 1.0);
}
%s
#define PREVIEW_IMAGE(NAME, ATLAS, RANGE, ZERO_DIST, COLOR, VERTEX_LIST, TEXT_SIZE, DIMENSIONS) model image NAME : \
    vertex_data(GlyphVertex), \
    fragment_data(vec2), \
    vertex(projectVertex<TEXT_SIZE>, triangles, VERTEX_LIST), \
    fragment(fillGlyph<ATLAS, RANGE, ZERO_DIST, COLOR>), \
    depth(false), \
    blend(transparency), \
    background(vec4(vec3(COLOR), 0.0)), \
    dimensions(DIMENSIONS), \
    resizable(true)

)";

static std::string relativizePath(const char *base, const char *target) {
    if (target[0] == '/' || (target[0] && target[1] == ':')) // absolute path?
        return target;
    int commonPrefix = 0;
    for (int i = 0; base[i] && target[i] && base[i] == target[i]; ++i) {
        if (base[i] == '/' || base[i] == '\\')
            commonPrefix = i+1;
    }
    base += commonPrefix;
    target += commonPrefix;
    int baseNesting = 0;
    for (int i = 0; base[i]; ++i)
        if (base[i] == '/' || base[i] == '\\')
            ++baseNesting;
    std::string output;
    for (int i = 0; i < baseNesting; ++i)
        output += "../";
    output += target;
    return output;
}

static std::string escapeString(const std::string &str) {
    std::string output;
    for (int i = 0; i < (int) str.size(); ++i) {
        switch (str[i]) {
            case '"':
                output += "\\\"";
                break;
            case '\\':
                output += "\\\\";
                break;
            default:
                output.push_back(str[i]);
        }
    }
    return output;
}

bool generateShadronPreview(const FontGeometry *fonts, int fontCount, ImageType atlasType, int atlasWidth, int atlasHeight, msdfgen::Range pxRange, const unicode_t *text, const char *imageFilename, bool fullRange, const char *outputFilename) {
    if (fontCount <= 0)
        return false;
    double texelWidth = 1./atlasWidth;
    double texelHeight = 1./atlasHeight;
    bool anyGlyphs = false;
    FILE *file = fopen(outputFilename, "w");
    if (!file)
        return false;
    fprintf(file, shadronPreviewPreamble, atlasType == ImageType::HARD_MASK || atlasType == ImageType::SOFT_MASK ? shadronFillGlyphMask : shadronFillGlyphSdf);
    if (imageFilename)
        fprintf(file, "image Atlas = file(\"%s\")", escapeString(relativizePath(outputFilename, imageFilename)).c_str());
    else
        fprintf(file, "image Atlas = file()");
    fprintf(file, " : %sfilter(%s), map(repeat);\n", fullRange ? "full_range(true), " : "", atlasType == ImageType::HARD_MASK ? "nearest" : "linear");
    double pxRangeWidth = pxRange.upper-pxRange.lower;
    fprintf(file, "const vec2 txRange = vec2(%.9g, %.9g);\n", pxRangeWidth*texelWidth, pxRangeWidth*texelHeight);
    fprintf(file, "const float zeroDistanceValue = %.9g;\n\n", -pxRange.lower/(pxRange.upper-pxRange.lower));
    {
        msdfgen::FontMetrics fontMetrics = fonts->getMetrics();
        for (int i = 1; i < fontCount; ++i) {
            fontMetrics.lineHeight = std::max(fontMetrics.lineHeight, fonts[i].getMetrics().lineHeight);
            fontMetrics.ascenderY = std::max(fontMetrics.ascenderY, fonts[i].getMetrics().ascenderY);
            fontMetrics.descenderY = std::min(fontMetrics.descenderY, fonts[i].getMetrics().descenderY);
        }
        double fsScale = 1/(fontMetrics.ascenderY-fontMetrics.descenderY);
        fputs("vertex_list GlyphVertex textQuadVertices = {\n", file);
        double x = 0, y = -fsScale*fontMetrics.ascenderY;
        double textWidth = 0;
        for (const unicode_t *cp = text; *cp; ++cp) {
            if (*cp == '\r')
                continue;
            if (*cp == '\n') {
                textWidth = std::max(textWidth, x);
                x = 0;
                y -= fsScale*fontMetrics.lineHeight;
                continue;
            }
            for (int i = 0; i < fontCount; ++i) {
                const GlyphGeometry *glyph = fonts[i].getGlyph(*cp);
                if (glyph) {
                    if (!glyph->isWhitespace()) {
                        double pl, pb, pr, pt;
                        double il, ib, ir, it;
                        glyph->getQuadPlaneBounds(pl, pb, pr, pt);
                        glyph->getQuadAtlasBounds(il, ib, ir, it);
                        pl *= fsScale, pb *= fsScale, pr *= fsScale, pt *= fsScale;
                        pl += x, pb += y, pr += x, pt += y;
                        il *= texelWidth, ib *= texelHeight, ir *= texelWidth, it *= texelHeight;
                        fprintf(file, "    %.9g, %.9g, %.9g, %.9g, %.9g, %.9g, %.9g, %.9g, %.9g, %.9g, %.9g, %.9g, %.9g, %.9g, %.9g, %.9g, %.9g, %.9g, %.9g, %.9g, %.9g, %.9g, %.9g, %.9g,\n",
                            pl, pb, il, ib,
                            pr, pb, ir, ib,
                            pl, pt, il, it,
                            pr, pt, ir, it,
                            pl, pt, il, it,
                            pr, pb, ir, ib
                        );
                    }
                    double advance = glyph->getAdvance();
                    fonts[i].getAdvance(advance, cp[0], cp[1]);
                    x += fsScale*advance;
                    anyGlyphs = true;
                    break;
                }
            }
        }
        textWidth = std::max(textWidth, x);
        y += fsScale*fontMetrics.descenderY;
        fputs("};\n", file);
        fprintf(file, "const vec2 textSize = vec2(%.9g, %.9g);\n\n", textWidth, -y);
    }
    fputs("PREVIEW_IMAGE(Preview, Atlas, txRange, zeroDistanceValue, vec3(1.0), textQuadVertices, textSize, ivec2(1200, 400));\n", file);
    fputs("export png(Preview, \"preview.png\");\n", file);
    fclose(file);
    return anyGlyphs;
}

}
