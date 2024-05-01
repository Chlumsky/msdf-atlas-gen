
#pragma once

#include "Padding.h"
#include "GlyphGeometry.h"

namespace msdf_atlas {

/**
 * This class computes the layout of a static uniform grid atlas and may optionally
 * also find the minimum required dimensions and/or the maximum glyph scale
 */
class GridAtlasPacker {

public:
    GridAtlasPacker();

    /// Computes the layout for the array of glyphs. Returns 0 on success
    int pack(GlyphGeometry *glyphs, int count);

    /// Sets whether the origin point should be at the same position in each glyph, separately for horizontal and vertical dimension
    void setFixedOrigin(bool horizontal, bool vertical);
    void setCellDimensions(int width, int height);
    void unsetCellDimensions();
    void setCellDimensionsConstraint(DimensionsConstraint dimensionsConstraint);
    void setColumns(int columns);
    void setRows(int rows);
    void unsetColumns();
    void unsetRows();

    /// Sets the atlas's fixed dimensions
    void setDimensions(int width, int height);
    /// Sets the atlas's dimensions to be determined during pack
    void unsetDimensions();
    /// Sets the constraint to be used when determining dimensions
    void setDimensionsConstraint(DimensionsConstraint dimensionsConstraint);
    /// Sets the spacing between glyph boxes
    void setSpacing(int spacing);
    /// Sets fixed glyph scale
    void setScale(double scale);
    /// Sets the minimum glyph scale
    void setMinimumScale(double minScale);
    /// Sets the unit component of the total distance range
    void setUnitRange(msdfgen::Range unitRange);
    /// Sets the pixel component of the total distance range
    void setPixelRange(msdfgen::Range pxRange);
    /// Sets the miter limit for bounds computation
    void setMiterLimit(double miterLimit);
    /// Sets whether each glyph's origin point should stay aligned with the pixel grid
    void setOriginPixelAlignment(bool align);
    void setOriginPixelAlignment(bool alignX, bool alignY);
    /// Sets the unit component of width of additional padding that is part of each glyph quad
    void setInnerUnitPadding(const Padding &padding);
    /// Sets the unit component of width of additional padding around each glyph quad
    void setOuterUnitPadding(const Padding &padding);
    /// Sets the pixel component of width of additional padding that is part of each glyph quad
    void setInnerPixelPadding(const Padding &padding);
    /// Sets the pixel component of width of additional padding around each glyph quad
    void setOuterPixelPadding(const Padding &padding);

    /// Outputs the atlas's final dimensions
    void getDimensions(int &width, int &height) const;
    /// Outputs the horizontal and vertical difference between the bottom left corners of consecutive grid cells
    void getCellDimensions(int &width, int &height) const;
    /// Returns the final number of grid columns
    int getColumns() const;
    /// Returns the final number of grid rows
    int getRows() const;
    /// Returns the final glyph scale
    double getScale() const;
    /// Returns the final combined pixel range (including converted unit range)
    msdfgen::Range getPixelRange() const;
    /// Outputs the position of the origin within each cell, each value is only valid if the origin is fixed in the respective dimension
    void getFixedOrigin(double &x, double &y);
    /// Returns true if the explicitly constrained cell dimensions aren't large enough to fit each glyph fully
    bool hasCutoff() const;

private:
    int columns, rows;
    int width, height;
    int cellWidth, cellHeight;
    int spacing;
    DimensionsConstraint dimensionsConstraint;
    DimensionsConstraint cellDimensionsConstraint;
    bool hFixed, vFixed;
    double scale;
    double minScale;
    double fixedX, fixedY;
    msdfgen::Range unitRange;
    msdfgen::Range pxRange;
    double miterLimit;
    bool pxAlignOriginX, pxAlignOriginY;
    Padding innerUnitPadding, outerUnitPadding;
    Padding innerPxPadding, outerPxPadding;
    double scaleMaximizationTolerance;
    double alignedColumnsBias;
    bool cutoff;

    static void lowerToConstraint(int &width, int &height, DimensionsConstraint constraint);
    static void raiseToConstraint(int &width, int &height, DimensionsConstraint constraint);

    double dimensionsRating(int width, int height, bool aligned) const;
    msdfgen::Shape::Bounds getMaxBounds(double &maxWidth, double &maxHeight, GlyphGeometry *glyphs, int count, double scale, double outerRange) const;
    double scaleToFit(GlyphGeometry *glyphs, int count, int cellWidth, int cellHeight, msdfgen::Shape::Bounds &maxBounds, double &maxWidth, double &maxHeight) const;

};

}
