
#include "shadron-preview-generator.h"

#include <string>
#include <algorithm>

namespace msdf_atlas {

static const char * const shadronFillGlyphMask = R"(
template <ATLAS, RANGE, COLOR>
glsl vec4 fillGlyph(vec2 texCoord) {
    float fill = texture((ATLAS), texCoord).r;
    return vec4(vec3(COLOR), fill);
}
)";

static const char * const shadronFillGlyphSdf = R"(
template <ATLAS, RANGE, COLOR>
glsl vec4 fillGlyph(vec2 texCoord) {
    vec3 s = texture((ATLAS), texCoord).rgb;
    float sd = dot(vec2(RANGE), 0.5/fwidth(texCoord))*(median(s.r, s.g, s.b)-0.5);
    float fill = clamp(sd+0.5, 0.0, 1.0);
    return vec4(vec3(COLOR), fill);
}
)";

static const char * const shadronPreviewPreamble = R"(
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
#define PREVIEW_IMAGE(NAME, ATLAS, RANGE, COLOR, VERTEX_LIST, TEXT_SIZE, DIMENSIONS) model image NAME : \
    vertex_data(GlyphVertex), \
    fragment_data(vec2), \
    vertex(projectVertex<TEXT_SIZE>, triangles, VERTEX_LIST), \
    fragment(fillGlyph<ATLAS, RANGE, COLOR>), \
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

bool generateShadronPreview(msdfgen::FontHandle *font, const GlyphGeometry *glyphs, int glyphCount, ImageType atlasType, int atlasWidth, int atlasHeight, double pxRange, const unicode_t *text, const char *imageFilename, bool fullRange, const char *outputFilename) {
    double texelWidth = 1./atlasWidth;
    double texelHeight = 1./atlasHeight;
    FILE *file = fopen(outputFilename, "w");
    if (!file)
        return false;
    fprintf(file, shadronPreviewPreamble, atlasType == ImageType::HARD_MASK || atlasType == ImageType::SOFT_MASK ? shadronFillGlyphMask : shadronFillGlyphSdf);
    if (imageFilename)
        fprintf(file, "image Atlas = file(\"%s\")", relativizePath(outputFilename, imageFilename).c_str());
    else
        fprintf(file, "image Atlas = file()");
    fprintf(file, " : %sfilter(%s), map(repeat);\n", fullRange ? "full_range(true), " : "", atlasType == ImageType::HARD_MASK ? "nearest" : "linear");
    fprintf(file, "const vec2 txRange = vec2(%.9g, %.9g);\n\n", pxRange*texelWidth, pxRange*texelHeight);
    {
        msdfgen::FontMetrics fontMetrics;
        if (!msdfgen::getFontMetrics(fontMetrics, font))
            return false;
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
            for (int i = 0; i < glyphCount; ++i) {
                if (glyphs[i].getCodepoint() == *cp) {
                    if (!glyphs[i].isWhitespace()) {
                        double pl, pb, pr, pt;
                        double il, ib, ir, it;
                        glyphs[i].getQuadPlaneBounds(pl, pb, pr, pt);
                        glyphs[i].getQuadAtlasBounds(il, ib, ir, it);
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
                    x += fsScale*glyphs[i].getAdvance();
                    double kerning;
                    if (msdfgen::getKerning(kerning, font, cp[0], cp[1]))
                        x += fsScale*kerning;
                    break;
                }
            }
        }
        textWidth = std::max(textWidth, x);
        y += fsScale*fontMetrics.descenderY;
        fputs("};\n", file);
        fprintf(file, "const vec2 textSize = vec2(%.9g, %.9g);\n\n", textWidth, -y);
    }
    fputs("PREVIEW_IMAGE(Preview, Atlas, txRange, vec3(1.0), textQuadVertices, textSize, ivec2(1200, 400));\n", file);
    fputs("export png(Preview, \"preview.png\");\n", file);
    fclose(file);
    return true;
}

}
