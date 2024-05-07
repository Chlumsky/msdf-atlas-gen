
#include "GlyphGeometry.h"

#include <cmath>
#include <core/ShapeDistanceFinder.h>

namespace msdf_atlas {

GlyphGeometry::GlyphGeometry() : index(), codepoint(), geometryScale(), bounds(), advance(), box() { }

bool GlyphGeometry::load(msdfgen::FontHandle *font, double geometryScale, msdfgen::GlyphIndex index, bool preprocessGeometry) {
    if (font && msdfgen::loadGlyph(shape, font, index, msdfgen::FONT_SCALING_NONE, &advance) && shape.validate()) {
        this->index = index.getIndex();
        this->geometryScale = geometryScale;
        codepoint = 0;
        advance *= geometryScale;
        #ifdef MSDFGEN_USE_SKIA
            if (preprocessGeometry)
                msdfgen::resolveShapeGeometry(shape);
        #endif
        shape.normalize();
        bounds = shape.getBounds();
        #ifdef MSDFGEN_USE_SKIA
            if (!preprocessGeometry)
        #endif
        {
            // Determine if shape is winded incorrectly and reverse it in that case
            msdfgen::Point2 outerPoint(bounds.l-(bounds.r-bounds.l)-1, bounds.b-(bounds.t-bounds.b)-1);
            if (msdfgen::SimpleTrueShapeDistanceFinder::oneShotDistance(shape, outerPoint) > 0) {
                for (msdfgen::Contour &contour : shape.contours)
                    contour.reverse();
            }
        }
        return true;
    }
    return false;
}

bool GlyphGeometry::load(msdfgen::FontHandle *font, double geometryScale, unicode_t codepoint, bool preprocessGeometry) {
    msdfgen::GlyphIndex index;
    if (msdfgen::getGlyphIndex(index, font, codepoint)) {
        if (load(font, geometryScale, index, preprocessGeometry)) {
            this->codepoint = codepoint;
            return true;
        }
    }
    return false;
}

void GlyphGeometry::edgeColoring(void (*fn)(msdfgen::Shape &, double, unsigned long long), double angleThreshold, unsigned long long seed) {
    fn(shape, angleThreshold, seed);
}

void GlyphGeometry::wrapBox(const GlyphAttributes &glyphAttributes) {
    double scale = glyphAttributes.scale*geometryScale;
    msdfgen::Range range = glyphAttributes.range/geometryScale;
    Padding fullPadding = (glyphAttributes.innerPadding+glyphAttributes.outerPadding)/geometryScale;
    box.range = range;
    box.scale = scale;
    if (bounds.l < bounds.r && bounds.b < bounds.t) {
        double l = bounds.l, b = bounds.b, r = bounds.r, t = bounds.t;
        l += range.lower, b += range.lower;
        r -= range.lower, t -= range.lower;
        if (glyphAttributes.miterLimit > 0)
            shape.boundMiters(l, b, r, t, -range.lower, glyphAttributes.miterLimit, 1);
        l -= fullPadding.l, b -= fullPadding.b;
        r += fullPadding.r, t += fullPadding.t;
        if (glyphAttributes.pxAlignOriginX) {
            int sl = (int) floor(scale*l-.5);
            int sr = (int) ceil(scale*r+.5);
            box.rect.w = sr-sl;
            box.translate.x = -sl/scale;
        } else {
            double w = scale*(r-l);
            box.rect.w = (int) ceil(w)+1;
            box.translate.x = -l+.5*(box.rect.w-w)/scale;
        }
        if (glyphAttributes.pxAlignOriginY) {
            int sb = (int) floor(scale*b-.5);
            int st = (int) ceil(scale*t+.5);
            box.rect.h = st-sb;
            box.translate.y = -sb/scale;
        } else {
            double h = scale*(t-b);
            box.rect.h = (int) ceil(h)+1;
            box.translate.y = -b+.5*(box.rect.h-h)/scale;
        }
        box.outerPadding = glyphAttributes.scale*glyphAttributes.outerPadding;
    } else {
        box.rect.w = 0, box.rect.h = 0;
        box.translate = msdfgen::Vector2();
    }
}

void GlyphGeometry::wrapBox(double scale, double range, double miterLimit, bool pxAlignOrigin) {
    GlyphAttributes attribs = { };
    attribs.scale = scale;
    attribs.range = range;
    attribs.miterLimit = miterLimit;
    attribs.pxAlignOriginX = pxAlignOrigin;
    attribs.pxAlignOriginY = pxAlignOrigin;
    wrapBox(attribs);
}

void GlyphGeometry::wrapBox(double scale, double range, double miterLimit, bool pxAlignOriginX, bool pxAlignOriginY) {
    GlyphAttributes attribs = { };
    attribs.scale = scale;
    attribs.range = range;
    attribs.miterLimit = miterLimit;
    attribs.pxAlignOriginX = pxAlignOriginX;
    attribs.pxAlignOriginY = pxAlignOriginY;
    wrapBox(attribs);
}

void GlyphGeometry::frameBox(const GlyphAttributes &glyphAttributes, int width, int height, const double *fixedX, const double *fixedY) {
    double scale = glyphAttributes.scale*geometryScale;
    msdfgen::Range range = glyphAttributes.range/geometryScale;
    Padding fullPadding = (glyphAttributes.innerPadding+glyphAttributes.outerPadding)/geometryScale;
    box.range = range;
    box.scale = scale;
    box.rect.w = width;
    box.rect.h = height;
    if (fixedX && fixedY) {
        box.translate.x = *fixedX/geometryScale;
        box.translate.y = *fixedY/geometryScale;
    } else {
        double l = bounds.l, b = bounds.b, r = bounds.r, t = bounds.t;
        l += range.lower, b += range.lower;
        r -= range.lower, t -= range.lower;
        if (glyphAttributes.miterLimit > 0)
            shape.boundMiters(l, b, r, t, -range.lower, glyphAttributes.miterLimit, 1);
        l -= fullPadding.l, b -= fullPadding.b;
        r += fullPadding.r, t += fullPadding.t;
        if (fixedX)
            box.translate.x = *fixedX/geometryScale;
        else if (glyphAttributes.pxAlignOriginX) {
            int sl = (int) floor(scale*l-.5);
            int sr = (int) ceil(scale*r+.5);
            box.translate.x = (-sl+(box.rect.w-(sr-sl))/2)/scale;
        } else {
            double w = scale*(r-l);
            box.translate.x = -l+.5*(box.rect.w-w)/scale;
        }
        if (fixedY)
            box.translate.y = *fixedY/geometryScale;
        else if (glyphAttributes.pxAlignOriginY) {
            int sb = (int) floor(scale*b-.5);
            int st = (int) ceil(scale*t+.5);
            box.translate.y = (-sb+(box.rect.h-(st-sb))/2)/scale;
        } else {
            double h = scale*(t-b);
            box.translate.y = -b+.5*(box.rect.h-h)/scale;
        }
    }
    box.outerPadding = glyphAttributes.scale*glyphAttributes.outerPadding;
}

void GlyphGeometry::frameBox(double scale, double range, double miterLimit, int width, int height, const double *fixedX, const double *fixedY, bool pxAlignOrigin) {
    GlyphAttributes attribs = { };
    attribs.scale = scale;
    attribs.range = range;
    attribs.miterLimit = miterLimit;
    attribs.pxAlignOriginX = pxAlignOrigin;
    attribs.pxAlignOriginY = pxAlignOrigin;
    frameBox(attribs, width, height, fixedX, fixedY);
}

void GlyphGeometry::frameBox(double scale, double range, double miterLimit, int width, int height, const double *fixedX, const double *fixedY, bool pxAlignOriginX, bool pxAlignOriginY) {
    GlyphAttributes attribs = { };
    attribs.scale = scale;
    attribs.range = range;
    attribs.miterLimit = miterLimit;
    attribs.pxAlignOriginX = pxAlignOriginX;
    attribs.pxAlignOriginY = pxAlignOriginY;
    frameBox(attribs, width, height, fixedX, fixedY);
}

void GlyphGeometry::placeBox(int x, int y) {
    box.rect.x = x, box.rect.y = y;
}

void GlyphGeometry::setBoxRect(const Rectangle &rect) {
    box.rect = rect;
}

int GlyphGeometry::getIndex() const {
    return index;
}

msdfgen::GlyphIndex GlyphGeometry::getGlyphIndex() const {
    return msdfgen::GlyphIndex(index);
}

unicode_t GlyphGeometry::getCodepoint() const {
    return codepoint;
}

int GlyphGeometry::getIdentifier(GlyphIdentifierType type) const {
    switch (type) {
        case GlyphIdentifierType::GLYPH_INDEX:
            return index;
        case GlyphIdentifierType::UNICODE_CODEPOINT:
            return (int) codepoint;
    }
    return 0;
}

double GlyphGeometry::getGeometryScale() const {
    return geometryScale;
}

const msdfgen::Shape &GlyphGeometry::getShape() const {
    return shape;
}

const msdfgen::Shape::Bounds &GlyphGeometry::getShapeBounds() const {
    return bounds;
}

double GlyphGeometry::getAdvance() const {
    return advance;
}

Rectangle GlyphGeometry::getBoxRect() const {
    return box.rect;
}

void GlyphGeometry::getBoxRect(int &x, int &y, int &w, int &h) const {
    x = box.rect.x, y = box.rect.y;
    w = box.rect.w, h = box.rect.h;
}

void GlyphGeometry::getBoxSize(int &w, int &h) const {
    w = box.rect.w, h = box.rect.h;
}

msdfgen::Range GlyphGeometry::getBoxRange() const {
    return box.range;
}

msdfgen::Projection GlyphGeometry::getBoxProjection() const {
    return msdfgen::Projection(msdfgen::Vector2(box.scale), box.translate);
}

double GlyphGeometry::getBoxScale() const {
    return box.scale;
}

msdfgen::Vector2 GlyphGeometry::getBoxTranslate() const {
    return box.translate;
}

void GlyphGeometry::getQuadPlaneBounds(double &l, double &b, double &r, double &t) const {
    if (box.rect.w > 0 && box.rect.h > 0) {
        double invBoxScale = 1/box.scale;
        l = geometryScale*(-box.translate.x+(box.outerPadding.l+.5)*invBoxScale);
        b = geometryScale*(-box.translate.y+(box.outerPadding.b+.5)*invBoxScale);
        r = geometryScale*(-box.translate.x+(-box.outerPadding.r+box.rect.w-.5)*invBoxScale);
        t = geometryScale*(-box.translate.y+(-box.outerPadding.t+box.rect.h-.5)*invBoxScale);
    } else
        l = 0, b = 0, r = 0, t = 0;
}

void GlyphGeometry::getQuadAtlasBounds(double &l, double &b, double &r, double &t) const {
    if (box.rect.w > 0 && box.rect.h > 0) {
        l = box.rect.x+box.outerPadding.l+.5;
        b = box.rect.y+box.outerPadding.b+.5;
        r = box.rect.x-box.outerPadding.r+box.rect.w-.5;
        t = box.rect.y-box.outerPadding.t+box.rect.h-.5;
    } else
        l = 0, b = 0, r = 0, t = 0;
}

bool GlyphGeometry::isWhitespace() const {
    return shape.contours.empty();
}

GlyphGeometry::operator GlyphBox() const {
    GlyphBox box;
    box.index = index;
    box.advance = advance;
    getQuadPlaneBounds(box.bounds.l, box.bounds.b, box.bounds.r, box.bounds.t);
    box.rect.x = this->box.rect.x, box.rect.y = this->box.rect.y, box.rect.w = this->box.rect.w, box.rect.h = this->box.rect.h;
    return box;
}

msdfgen::Range operator+(msdfgen::Range a, msdfgen::Range b) {
    return msdfgen::Range(a.lower+b.lower, a.upper+b.upper);
}

}
