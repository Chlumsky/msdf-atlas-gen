
#pragma once

#include <msdfgen.h>
#include <msdfgen-ext.h>
#include "types.h"
#include "Rectangle.h"
#include "Padding.h"
#include "GlyphBox.h"

namespace msdf_atlas {

/// Represents the shape geometry of a single glyph as well as its configuration
class GlyphGeometry {

public:
    struct GlyphAttributes {
        double scale;
        msdfgen::Range range;
        Padding innerPadding, outerPadding;
        double miterLimit;
        bool pxAlignOriginX, pxAlignOriginY;
    };

    GlyphGeometry();
    /// Loads glyph geometry from font
    bool load(msdfgen::FontHandle *font, double geometryScale, msdfgen::GlyphIndex index, bool preprocessGeometry = true);
    bool load(msdfgen::FontHandle *font, double geometryScale, unicode_t codepoint, bool preprocessGeometry = true);
    /// Applies edge coloring to glyph shape
    void edgeColoring(void (*fn)(msdfgen::Shape &, double, unsigned long long), double angleThreshold, unsigned long long seed);
    /// Computes the dimensions of the glyph's box as well as the transformation for the generator function
    void wrapBox(const GlyphAttributes &glyphAttributes);
    void wrapBox(double scale, double range, double miterLimit, bool pxAlignOrigin = false);
    void wrapBox(double scale, double range, double miterLimit, bool pxAlignOriginX, bool pxAlignOriginY);
    /// Computes the glyph's transformation and alignment (unless specified) for given dimensions
    void frameBox(const GlyphAttributes &glyphAttributes, int width, int height, const double *fixedX, const double *fixedY);
    void frameBox(double scale, double range, double miterLimit, int width, int height, const double *fixedX, const double *fixedY, bool pxAlignOrigin = false);
    void frameBox(double scale, double range, double miterLimit, int width, int height, const double *fixedX, const double *fixedY, bool pxAlignOriginX, bool pxAlignOriginY);
    /// Sets the glyph's box's position in the atlas
    void placeBox(int x, int y);
    /// Sets the glyph's box's rectangle in the atlas
    void setBoxRect(const Rectangle &rect);
    /// Returns the glyph's index within the font
    int getIndex() const;
    /// Returns the glyph's index as a msdfgen::GlyphIndex
    msdfgen::GlyphIndex getGlyphIndex() const;
    /// Returns the Unicode codepoint represented by the glyph or 0 if unknown
    unicode_t getCodepoint() const;
    /// Returns the glyph's identifier specified by the supplied identifier type
    int getIdentifier(GlyphIdentifierType type) const;
    /// Returns the glyph's geometry scale
    double getGeometryScale() const;
    /// Returns the glyph's shape
    const msdfgen::Shape &getShape() const;
    /// Returns the glyph's shape's raw bounds
    const msdfgen::Shape::Bounds &getShapeBounds() const;
    /// Returns the glyph's advance
    double getAdvance() const;
    /// Returns the glyph's box in the atlas
    Rectangle getBoxRect() const;
    /// Outputs the position and dimensions of the glyph's box in the atlas
    void getBoxRect(int &x, int &y, int &w, int &h) const;
    /// Outputs the dimensions of the glyph's box in the atlas
    void getBoxSize(int &w, int &h) const;
    /// Returns the range needed to generate the glyph's SDF
    msdfgen::Range getBoxRange() const;
    /// Returns the projection needed to generate the glyph's bitmap
    msdfgen::Projection getBoxProjection() const;
    /// Returns the scale needed to generate the glyph's bitmap
    double getBoxScale() const;
    /// Returns the translation vector needed to generate the glyph's bitmap
    msdfgen::Vector2 getBoxTranslate() const;
    /// Outputs the bounding box of the glyph as it should be placed on the baseline
    void getQuadPlaneBounds(double &l, double &b, double &r, double &t) const;
    /// Outputs the bounding box of the glyph in the atlas
    void getQuadAtlasBounds(double &l, double &b, double &r, double &t) const;
    /// Returns true if the glyph is a whitespace and has no geometry
    bool isWhitespace() const;
    /// Simplifies to GlyphBox
    operator GlyphBox() const;

private:
    int index;
    unicode_t codepoint;
    double geometryScale;
    msdfgen::Shape shape;
    msdfgen::Shape::Bounds bounds;
    double advance;
    struct {
        Rectangle rect;
        msdfgen::Range range;
        double scale;
        msdfgen::Vector2 translate;
        Padding outerPadding;
    } box;

};

msdfgen::Range operator+(msdfgen::Range a, msdfgen::Range b);

}
