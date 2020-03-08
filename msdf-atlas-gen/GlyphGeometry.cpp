
#include "GlyphGeometry.h"

#include <cmath>

namespace msdf_atlas {

GlyphGeometry::GlyphGeometry() : codepoint(), bounds(), reverseWinding(), advance(), box() { }

double GlyphGeometry::simpleSignedDistance(const msdfgen::Point2 &p) const {
    double dummy;
    msdfgen::SignedDistance minDistance;
    for (const msdfgen::Contour &contour : shape.contours)
        for (const msdfgen::EdgeHolder &edge : contour.edges) {
            msdfgen::SignedDistance distance = edge->signedDistance(p, dummy);
            if (distance < minDistance)
                minDistance = distance;
        }
    return minDistance.distance;
}

bool GlyphGeometry::load(msdfgen::FontHandle *font, unicode_t codepoint) {
    if (font && msdfgen::loadGlyph(shape, font, codepoint, &advance) && shape.validate()) {
        this->codepoint = codepoint;
        shape.normalize();
        bounds = shape.getBounds();
        msdfgen::Point2 outerPoint(bounds.l-(bounds.r-bounds.l)-1, bounds.b-(bounds.t-bounds.b)-1);
        reverseWinding = simpleSignedDistance(outerPoint) > 0;
        return true;
    }
    return false;
}

void GlyphGeometry::edgeColoring(double angleThreshold, unsigned long long seed) {
    msdfgen::edgeColoringInkTrap(shape, angleThreshold, seed);
}

void GlyphGeometry::wrapBox(double scale, double range, double miterLimit) {
    box.range = range;
    box.scale = scale;
    if (bounds.l < bounds.r && bounds.b < bounds.t) {
        double l = bounds.l, b = bounds.b, r = bounds.r, t = bounds.t;
        l -= .5*range, b -= .5*range;
        r += .5*range, t += .5*range;
        if (miterLimit > 0)
            shape.boundMiters(l, b, r, t, .5*range, miterLimit, reverseWinding ? -1 : +1);
        double w = scale*(r-l);
        double h = scale*(t-b);
        box.rect.w = (int) ceil(w)+1;
        box.rect.h = (int) ceil(h)+1;
        box.translate.x = -l+.5*(box.rect.w-w)/scale;
        box.translate.y = -b+.5*(box.rect.h-h)/scale;
    } else {
        box.rect.w = 0, box.rect.h = 0;
        box.translate = msdfgen::Vector2();
    }
}

void GlyphGeometry::placeBox(int x, int y) {
    box.rect.x = x, box.rect.y = y;
}

unicode_t GlyphGeometry::getCodepoint() const {
    return codepoint;
}

const msdfgen::Shape & GlyphGeometry::getShape() const {
    return shape;
}

double GlyphGeometry::getAdvance() const {
    return advance;
}

bool GlyphGeometry::isWindingReverse() const {
    return reverseWinding;
}

void GlyphGeometry::getBoxRect(int &x, int &y, int &w, int &h) const {
    x = box.rect.x, y = box.rect.y;
    w = box.rect.w, h = box.rect.h;
}

void GlyphGeometry::getBoxSize(int &w, int &h) const {
    w = box.rect.w, h = box.rect.h;
}

double GlyphGeometry::getBoxRange() const {
    return box.range;
}

double GlyphGeometry::getBoxScale() const {
    return box.scale;
}

msdfgen::Vector2 GlyphGeometry::getBoxTranslate() const {
    return box.translate;
}

void GlyphGeometry::getQuadPlaneBounds(double &l, double &b, double &r, double &t) const {
    if (box.rect.w > 0 && box.rect.h > 0) {
        l = -box.translate.x+.5/box.scale;
        b = -box.translate.y+.5/box.scale;
        r = -box.translate.x+(box.rect.w-.5)/box.scale;
        t = -box.translate.y+(box.rect.h-.5)/box.scale;
    } else
        l = 0, b = 0, r = 0, t = 0;
}

void GlyphGeometry::getQuadAtlasBounds(double &l, double &b, double &r, double &t) const {
    if (box.rect.w > 0 && box.rect.h > 0) {
        l = box.rect.x+.5;
        b = box.rect.y+.5;
        r = box.rect.x+box.rect.w-.5;
        t = box.rect.y+box.rect.h-.5;
    } else
        l = 0, b = 0, r = 0, t = 0;
}

bool GlyphGeometry::isWhitespace() const {
    return shape.contours.empty();
}

GlyphGeometry::operator GlyphBox() const {
    GlyphBox box;
    box.codepoint = codepoint;
    box.advance = advance;
    getQuadPlaneBounds(box.bounds.l, box.bounds.b, box.bounds.r, box.bounds.t);
    box.rect.x = this->box.rect.x, box.rect.y = this->box.rect.y, box.rect.w = this->box.rect.w, box.rect.h = this->box.rect.h;
    return box;
}

}
