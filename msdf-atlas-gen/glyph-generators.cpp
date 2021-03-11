
#include "glyph-generators.h"

namespace msdf_atlas {

void scanlineGenerator(const msdfgen::BitmapRef<float, 1> &output, const GlyphGeometry &glyph, const GeneratorAttributes &attribs) {
    msdfgen::rasterize(output, glyph.getShape(), glyph.getBoxScale(), glyph.getBoxTranslate(), MSDF_ATLAS_GLYPH_FILL_RULE);
}

void sdfGenerator(const msdfgen::BitmapRef<float, 1> &output, const GlyphGeometry &glyph, const GeneratorAttributes &attribs) {
    msdfgen::generateSDF(output, glyph.getShape(), glyph.getBoxRange(), glyph.getBoxScale(), glyph.getBoxTranslate(), attribs.overlapSupport);
    if (attribs.scanlinePass)
        msdfgen::distanceSignCorrection(output, glyph.getShape(), glyph.getBoxScale(), glyph.getBoxTranslate(), MSDF_ATLAS_GLYPH_FILL_RULE);
}

void psdfGenerator(const msdfgen::BitmapRef<float, 1> &output, const GlyphGeometry &glyph, const GeneratorAttributes &attribs) {
    msdfgen::generatePseudoSDF(output, glyph.getShape(), glyph.getBoxRange(), glyph.getBoxScale(), glyph.getBoxTranslate(), attribs.overlapSupport);
    if (attribs.scanlinePass)
        msdfgen::distanceSignCorrection(output, glyph.getShape(), glyph.getBoxScale(), glyph.getBoxTranslate(), MSDF_ATLAS_GLYPH_FILL_RULE);
}

void msdfGenerator(const msdfgen::BitmapRef<float, 3> &output, const GlyphGeometry &glyph, const GeneratorAttributes &attribs) {
    msdfgen::generateMSDF(output, glyph.getShape(), glyph.getBoxRange(), glyph.getBoxScale(), glyph.getBoxTranslate(), attribs.scanlinePass ? 0 : attribs.errorCorrectionThreshold, attribs.overlapSupport);
    if (attribs.scanlinePass) {
        msdfgen::distanceSignCorrection(output, glyph.getShape(), glyph.getBoxScale(), glyph.getBoxTranslate(), MSDF_ATLAS_GLYPH_FILL_RULE);
        if (attribs.errorCorrectionThreshold > 0)
            msdfgen::msdfErrorCorrection(output, attribs.errorCorrectionThreshold/(glyph.getBoxScale()*glyph.getBoxRange()));
    }
}

void mtsdfGenerator(const msdfgen::BitmapRef<float, 4> &output, const GlyphGeometry &glyph, const GeneratorAttributes &attribs) {
    msdfgen::generateMTSDF(output, glyph.getShape(), glyph.getBoxRange(), glyph.getBoxScale(), glyph.getBoxTranslate(), attribs.scanlinePass ? 0 :attribs.errorCorrectionThreshold, attribs.overlapSupport);
    if (attribs.scanlinePass) {
        msdfgen::distanceSignCorrection(output, glyph.getShape(), glyph.getBoxScale(), glyph.getBoxTranslate(), MSDF_ATLAS_GLYPH_FILL_RULE);
        if (attribs.errorCorrectionThreshold > 0)
            msdfgen::msdfErrorCorrection(output, attribs.errorCorrectionThreshold/(glyph.getBoxScale()*glyph.getBoxRange()));
    }
}

}
