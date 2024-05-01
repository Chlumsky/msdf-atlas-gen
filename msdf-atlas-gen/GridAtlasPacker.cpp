
#include "GridAtlasPacker.h"

#include <algorithm>

namespace msdf_atlas {

static int floorPOT(int x) {
    int y = 1;
    while (x >= y && y<<1)
        y <<= 1;
    return y>>1;
}

static int ceilPOT(int x) {
    int y = 1;
    while (x > y && y<<1)
        y <<= 1;
    return y;
}

static bool squareConstraint(DimensionsConstraint constraint) {
    switch (constraint) {
        case DimensionsConstraint::SQUARE:
        case DimensionsConstraint::EVEN_SQUARE:
        case DimensionsConstraint::MULTIPLE_OF_FOUR_SQUARE:
        case DimensionsConstraint::POWER_OF_TWO_SQUARE:
            return true;
        case DimensionsConstraint::NONE:
        case DimensionsConstraint::POWER_OF_TWO_RECTANGLE:
            return false;
    }
    return true;
}

void GridAtlasPacker::lowerToConstraint(int &width, int &height, DimensionsConstraint constraint) {
    if (squareConstraint(constraint))
        width = height = std::min(width, height);
    switch (constraint) {
        case DimensionsConstraint::NONE:
        case DimensionsConstraint::SQUARE:
            break;
        case DimensionsConstraint::EVEN_SQUARE:
            width &= ~1;
            height &= ~1;
            break;
        case DimensionsConstraint::MULTIPLE_OF_FOUR_SQUARE:
            width &= ~3;
            height &= ~3;
            break;
        case DimensionsConstraint::POWER_OF_TWO_RECTANGLE:
        case DimensionsConstraint::POWER_OF_TWO_SQUARE:
            if (width > 0)
                width = floorPOT(width);
            if (height > 0)
                height = floorPOT(height);
            break;
    }
}

void GridAtlasPacker::raiseToConstraint(int &width, int &height, DimensionsConstraint constraint) {
    if (squareConstraint(constraint))
        width = height = std::max(width, height);
    switch (constraint) {
        case DimensionsConstraint::NONE:
        case DimensionsConstraint::SQUARE:
            break;
        case DimensionsConstraint::EVEN_SQUARE:
            width += width&1;
            height += height&1;
            break;
        case DimensionsConstraint::MULTIPLE_OF_FOUR_SQUARE:
            width += -width&3;
            height += -height&3;
            break;
        case DimensionsConstraint::POWER_OF_TWO_RECTANGLE:
        case DimensionsConstraint::POWER_OF_TWO_SQUARE:
            if (width > 0)
                width = ceilPOT(width);
            if (height > 0)
                height = ceilPOT(height);
            break;
    }
}

double GridAtlasPacker::dimensionsRating(int width, int height, bool aligned) const {
    return ((double) width*width+(double) height*height)*(aligned ? 1-alignedColumnsBias : 1);
}

GridAtlasPacker::GridAtlasPacker() :
    columns(-1), rows(-1),
    width(-1), height(-1),
    cellWidth(-1), cellHeight(-1),
    spacing(0),
    dimensionsConstraint(DimensionsConstraint::NONE),
    cellDimensionsConstraint(DimensionsConstraint::NONE),
    hFixed(false), vFixed(false),
    scale(-1),
    minScale(1),
    fixedX(0), fixedY(0),
    unitRange(0),
    pxRange(0),
    miterLimit(0),
    pxAlignOriginX(false), pxAlignOriginY(false),
    scaleMaximizationTolerance(.001),
    alignedColumnsBias(.125),
    cutoff(false)
{ }

msdfgen::Shape::Bounds GridAtlasPacker::getMaxBounds(double &maxWidth, double &maxHeight, GlyphGeometry *glyphs, int count, double scale, double outerRange) const {
    static const double LARGE_VALUE = 1e240;
    msdfgen::Shape::Bounds maxBounds = { +LARGE_VALUE, +LARGE_VALUE, -LARGE_VALUE, -LARGE_VALUE };
    for (GlyphGeometry *glyph = glyphs, *end = glyphs+count; glyph < end; ++glyph) {
        if (!glyph->isWhitespace()) {
            double geometryScale = glyph->getGeometryScale();
            double shapeOuterRange = outerRange/geometryScale;
            geometryScale *= scale;
            const msdfgen::Shape::Bounds &shapeBounds = glyph->getShapeBounds();
            double l = shapeBounds.l, b = shapeBounds.b, r = shapeBounds.r, t = shapeBounds.t;
            l -= shapeOuterRange, b -= shapeOuterRange;
            r += shapeOuterRange, t += shapeOuterRange;
            if (miterLimit > 0)
                glyph->getShape().boundMiters(l, b, r, t, shapeOuterRange, miterLimit, 1);
            l *= geometryScale, b *= geometryScale;
            r *= geometryScale, t *= geometryScale;
            maxBounds.l = std::min(maxBounds.l, l);
            maxBounds.b = std::min(maxBounds.b, b);
            maxBounds.r = std::max(maxBounds.r, r);
            maxBounds.t = std::max(maxBounds.t, t);
            maxWidth = std::max(maxWidth, r-l);
            maxHeight = std::max(maxHeight, t-b);
        }
    }
    if (maxBounds.l >= maxBounds.r || maxBounds.b >= maxBounds.t)
        maxBounds = msdfgen::Shape::Bounds();
    Padding fullPadding = scale*(innerUnitPadding+outerUnitPadding)+innerPxPadding+outerPxPadding;
    pad(maxBounds, fullPadding);
    maxWidth += fullPadding.l+fullPadding.r;
    maxHeight += fullPadding.b+fullPadding.t;
    // If origin is pixel-aligned but not fixed, one pixel has to be added to max dimensions to allow for aligning the origin by shifting by < 1 pixel
    if (hFixed)
        maxWidth = maxBounds.r-maxBounds.l;
    else if (pxAlignOriginX)
        maxWidth += 1;
    if (vFixed)
        maxHeight = maxBounds.t-maxBounds.b;
    else if (pxAlignOriginY)
        maxHeight += 1;
    return maxBounds;
}

double GridAtlasPacker::scaleToFit(GlyphGeometry *glyphs, int count, int cellWidth, int cellHeight, msdfgen::Shape::Bounds &maxBounds, double &maxWidth, double &maxHeight) const {
    static const int BIG_VALUE = 1<<28;
    if (cellWidth <= 0)
        cellWidth = BIG_VALUE;
    if (cellHeight <= 0)
        cellHeight = BIG_VALUE;
    --cellWidth, --cellHeight; // Implicit half-pixel padding from each side to make sure that no representable values are beyond outermost pixel centers
    cellWidth -= spacing, cellHeight -= spacing;
    bool lastResult = false;
    #define TRY_FIT(scale) (maxWidth = 0, maxHeight = 0, maxBounds = getMaxBounds(maxWidth, maxHeight, glyphs, count, (scale), -(unitRange.lower+pxRange.lower/(scale))), lastResult = maxWidth <= cellWidth && maxHeight <= cellHeight)
    double minScale = 1, maxScale = 1;
    if (TRY_FIT(1)) {
        while (maxScale < 1e+32 && ((maxScale = 2*minScale), TRY_FIT(maxScale)))
            minScale = maxScale;
    } else {
        while (minScale > 1e-32 && ((minScale = .5*maxScale), !TRY_FIT(minScale)))
            maxScale = minScale;
    }
    if (minScale == maxScale)
        return 0;
    while (minScale/maxScale < 1-scaleMaximizationTolerance) {
        double midScale = .5*(minScale+maxScale);
        if (TRY_FIT(midScale))
            minScale = midScale;
        else
            maxScale = midScale;
    }
    if (!lastResult)
        TRY_FIT(minScale);
    return minScale;
}

// Can this spaghetti code be simplified?
// Idea: Maybe it could be rewritten into a while (not all properties deduced) cycle, and compute one value in each iteration
int GridAtlasPacker::pack(GlyphGeometry *glyphs, int count) {
    if (!count)
        return 0;
    GridAtlasPacker initial(*this);
    int cellCount = 0;
    if (columns > 0 && rows > 0)
        cellCount = columns*rows;
    else {
        // Count non-whitespace glyphs only
        for (const GlyphGeometry *glyph = glyphs, *end = glyphs+count; glyph < end; ++glyph) {
            if (!glyph->isWhitespace())
                ++cellCount;
        }
        if (columns > 0)
            rows = (cellCount+columns-1)/columns;
        else if (rows > 0)
            columns = (cellCount+rows-1)/rows;
        else if (width > 0 && cellWidth > 0) {
            columns = (width+spacing)/cellWidth;
            rows = (cellCount+columns-1)/columns;
        }
    }

    if (width < 0 && cellWidth > 0 && columns > 0)
        width = columns*cellWidth;
    if (height < 0 && cellHeight > 0 && rows > 0)
        height = rows*cellHeight;
    if (width != initial.width || height != initial.height)
        raiseToConstraint(width, height, dimensionsConstraint);

    if (cellWidth < 0 && width > 0 && columns > 0)
        cellWidth = (width+spacing)/columns;
    if (cellHeight < 0 && height > 0 && rows > 0)
        cellHeight = (height+spacing)/rows;
    if (cellWidth != initial.cellWidth || cellHeight != initial.cellHeight) {
        bool positiveCellWidth = cellWidth > 0, positiveCellHeight = cellHeight > 0;
        lowerToConstraint(cellWidth, cellHeight, cellDimensionsConstraint);
        if ((cellWidth == 0 && positiveCellWidth) || (cellHeight == 0 && positiveCellHeight))
            return -1;
    }

    if ((cellWidth > 0 && cellWidth-spacing-1 <= -2*pxRange.lower) || (cellHeight > 0 && cellHeight-spacing-1 <= -2*pxRange.lower)) // cells definitely too small
        return -1;

    msdfgen::Shape::Bounds maxBounds = { };
    double maxWidth = 0, maxHeight = 0;

    if (scale <= 0) {

        // If both pxRange and miterLimit is non-zero, miter bounds have to be computed for all potential scales
        if (pxRange.lower != pxRange.upper && miterLimit > 0) {

            if (cellWidth > 0 || cellHeight > 0) {
                scale = scaleToFit(glyphs, count, cellWidth, cellHeight, maxBounds, maxWidth, maxHeight);
                if (scale < minScale) {
                    scale = minScale;
                    cutoff = true;
                    maxBounds = getMaxBounds(maxWidth, maxHeight, glyphs, count, scale, -(unitRange.lower+pxRange.lower/scale));
                }
            }

            else if (width > 0 && height > 0) {
                double bestAlignedScale = 0;
                int bestCols = 0, bestAlignedCols = 0;
                for (int q = (int) sqrt(cellCount)+1; q > 0; --q) {
                    int cols = q;
                    int rows = (cellCount+cols-1)/cols;
                    int tWidth = (width+spacing)/cols;
                    int tHeight = (height+spacing)/rows;
                    lowerToConstraint(tWidth, tHeight, cellDimensionsConstraint);
                    if (tWidth > 0 && tHeight > 0) {
                        double curScale = scaleToFit(glyphs, count, tWidth, tHeight, maxBounds, maxWidth, maxHeight);
                        if (curScale > scale) {
                            scale = curScale;
                            bestCols = cols;
                        }
                        if (cols*tWidth == width && curScale > bestAlignedScale) {
                            bestAlignedScale = curScale;
                            bestAlignedCols = cols;
                        }
                    }
                    cols = (cellCount+q-1)/q;
                    rows = (cellCount+cols-1)/cols;
                    tWidth = (width+spacing)/cols;
                    tHeight = (height+spacing)/rows;
                    lowerToConstraint(tWidth, tHeight, cellDimensionsConstraint);
                    if (tWidth > 0 && tHeight > 0) {
                        double curScale = scaleToFit(glyphs, count, tWidth, tHeight, maxBounds, maxWidth, maxHeight);
                        if (curScale > scale) {
                            scale = curScale;
                            bestCols = cols;
                        }
                        if (cols*tWidth == width && curScale > bestAlignedScale) {
                            bestAlignedScale = curScale;
                            bestAlignedCols = cols;
                        }
                    }
                }
                if (!bestCols)
                    return -1;
                // If columns can be aligned with total width at a slight cost to glyph scale, use that number of columns instead
                if (bestAlignedScale >= minScale && (alignedColumnsBias+1)*bestAlignedScale >= scale) {
                    scale = bestAlignedScale;
                    bestCols = bestAlignedCols;
                }

                columns = bestCols;
                rows = (cellCount+columns-1)/columns;
                cellWidth = (width+spacing)/columns;
                cellHeight = (height+spacing)/rows;
                lowerToConstraint(cellWidth, cellHeight, cellDimensionsConstraint);
                scale = scaleToFit(glyphs, count, cellWidth, cellHeight, maxBounds, maxWidth, maxHeight);
                if (scale < minScale)
                    scale = -1;
            }

            if (scale <= 0) {
                maxBounds = getMaxBounds(maxWidth, maxHeight, glyphs, count, minScale, -(unitRange.lower+pxRange.lower/minScale));
                cellWidth = (int) ceil(maxWidth)+spacing+1;
                cellHeight = (int) ceil(maxHeight)+spacing+1;
                raiseToConstraint(cellWidth, cellHeight, cellDimensionsConstraint);
                scale = scaleToFit(glyphs, count, cellWidth, cellHeight, maxBounds, maxWidth, maxHeight);
                if (scale < minScale)
                    maxBounds = getMaxBounds(maxWidth, maxHeight, glyphs, count, scale = minScale, -(unitRange.lower+pxRange.lower/minScale));
            }

            if (initial.rows < 0 && initial.cellHeight < 0) {
                int optimalCellWidth = cellWidth, optimalCellHeight = (int) ceil(maxHeight)+spacing+1;
                raiseToConstraint(optimalCellWidth, optimalCellHeight, cellDimensionsConstraint);
                if (optimalCellHeight < cellHeight && optimalCellWidth <= cellWidth) {
                    cellWidth = optimalCellWidth;
                    cellHeight = optimalCellHeight;
                }
            }

        } else {

            Padding pxPadding = innerPxPadding+outerPxPadding;
            maxBounds = getMaxBounds(maxWidth, maxHeight, glyphs, count, 1, -unitRange.lower);
            // Undo pxPadding added by getMaxBounds before pixel scale is known
            pad(maxBounds, -pxPadding);
            maxWidth -= pxPadding.l+pxPadding.r;
            maxHeight -= pxPadding.b+pxPadding.t;
            int hSlack = 0, vSlack = 0;
            if (pxAlignOriginX && !hFixed) {
                maxWidth -= 1; // Added by getMaxBounds
                hSlack = 1;
            }
            if (pxAlignOriginY && !vFixed) {
                maxHeight -= 1; // Added by getMaxBounds
                vSlack = 1;
            }

            double extraPxWidth = -2*pxRange.lower+pxPadding.l+pxPadding.r;
            double extraPxHeight = -2*pxRange.lower+pxPadding.b+pxPadding.t;
            double hScale = 0, vScale = 0;
            if (cellWidth > 0)
                hScale = (cellWidth-hSlack-spacing-extraPxWidth-1)/maxWidth;
            if (cellHeight > 0)
                vScale = (cellHeight-vSlack-spacing-extraPxHeight-1)/maxHeight;
            if (hScale || vScale) {
                if (hScale && vScale)
                    scale = std::min(hScale, vScale);
                else
                    scale = hScale+vScale;
                if (scale < minScale) {
                    scale = minScale;
                    cutoff = true;
                }
            }

            else if (width > 0 && height > 0) {
                double bestAlignedScale = 0;
                int bestCols = 0, bestAlignedCols = 0;
                // TODO optimize to only test up to sqrt(cellCount) cols and rows like in the above branch (for (int q = (int) sqrt(cellCount)+1; ...)
                for (int cols = 1; cols < width; ++cols) {
                    int rows = (cellCount+cols-1)/cols;
                    int tWidth = (width+spacing)/cols;
                    int tHeight = (height+spacing)/rows;
                    lowerToConstraint(tWidth, tHeight, cellDimensionsConstraint);
                    if (tWidth > 0 && tHeight > 0) {
                        hScale = (tWidth-hSlack-spacing-extraPxWidth-1)/maxWidth;
                        vScale = (tHeight-vSlack-spacing-extraPxHeight-1)/maxHeight;
                        double curScale = std::min(hScale, vScale);
                        if (curScale > scale) {
                            scale = curScale;
                            bestCols = cols;
                        }
                        if (cols*tWidth == width && curScale > bestAlignedScale) {
                            bestAlignedScale = curScale;
                            bestAlignedCols = cols;
                        }
                    }
                }
                if (!bestCols)
                    return -1;
                // If columns can be aligned with total width at a slight cost to glyph scale, use that number of columns instead
                if (bestAlignedScale >= minScale && (alignedColumnsBias+1)*bestAlignedScale >= scale) {
                    scale = bestAlignedScale;
                    bestCols = bestAlignedCols;
                }

                columns = bestCols;
                rows = (cellCount+columns-1)/columns;
                cellWidth = (width+spacing)/columns;
                cellHeight = (height+spacing)/rows;
                lowerToConstraint(cellWidth, cellHeight, cellDimensionsConstraint);
                if (scale < minScale)
                    scale = -1;
            }

            if (scale <= 0) {
                cellWidth = (int) ceil(minScale*maxWidth+extraPxWidth)+hSlack+spacing+1;
                cellHeight = (int) ceil(minScale*maxHeight+extraPxHeight)+vSlack+spacing+1;
                raiseToConstraint(cellWidth, cellHeight, cellDimensionsConstraint);
                hScale = (cellWidth-hSlack-spacing-extraPxWidth-1)/maxWidth;
                vScale = (cellHeight-vSlack-spacing-extraPxHeight-1)/maxHeight;
                scale = std::min(hScale, vScale);
            }

            if (initial.rows < 0 && initial.cellHeight < 0) {
                int optimalCellWidth = cellWidth, optimalCellHeight = (int) ceil(scale*maxHeight+extraPxHeight)+vSlack+spacing+1;
                raiseToConstraint(optimalCellWidth, optimalCellHeight, cellDimensionsConstraint);
                if (optimalCellHeight < cellHeight && optimalCellWidth <= cellWidth) {
                    cellWidth = optimalCellWidth;
                    cellHeight = optimalCellHeight;
                }
            }
            maxBounds.l *= scale, maxBounds.b *= scale;
            maxBounds.r *= scale, maxBounds.t *= scale;
            maxWidth *= scale, maxHeight *= scale;
            // Redo addition of pxPadding once scale is known
            pad(maxBounds, pxPadding);
            maxWidth += pxPadding.l+pxPadding.r;
            maxHeight += pxPadding.b+pxPadding.t;

        }

    } else {
        maxBounds = getMaxBounds(maxWidth, maxHeight, glyphs, count, scale, -(unitRange.lower+pxRange.lower/scale));
        int optimalCellWidth = (int) ceil(maxWidth)+spacing+1;
        int optimalCellHeight = (int) ceil(maxHeight)+spacing+1;
        if (cellWidth < 0 || cellHeight < 0) {
            cellWidth = optimalCellWidth;
            cellHeight = optimalCellHeight;
            raiseToConstraint(cellWidth, cellHeight, cellDimensionsConstraint);
        } else if (cellWidth < optimalCellWidth || cellHeight < optimalCellHeight)
            cutoff = true;
    }

    // Compute fixed origin
    if (hFixed) {
        if (pxAlignOriginX) {
            int sl = (int) floor(maxBounds.l-.5);
            int sr = (int) ceil(maxBounds.r+.5);
            fixedX = (-sl+(cellWidth-spacing-(sr-sl))/2)/scale;
        } else
            fixedX = (-maxBounds.l+.5*(cellWidth-spacing-maxWidth))/scale;
    }
    if (vFixed) {
        if (pxAlignOriginY) {
            int sb = (int) floor(maxBounds.b-.5);
            int st = (int) ceil(maxBounds.t+.5);
            fixedY = (-sb+(cellHeight-spacing-(st-sb))/2)/scale;
        } else
            fixedY = (-maxBounds.b+.5*(cellHeight-spacing-maxHeight))/scale;
    }

    if (width < 0 || height < 0) {
        if (columns <= 0) {
            double bestRating = -1;
            for (int q = (int) sqrt(cellCount)+1; q > 0; --q) {
                int cols = q;
                int rows = (cellCount+cols-1)/cols;
                int curWidth = cols*cellWidth, curHeight = rows*cellHeight;
                raiseToConstraint(curWidth, curHeight, dimensionsConstraint);
                double rating = dimensionsRating(curWidth, curHeight, cols*cellWidth == curWidth);
                if (rating < bestRating || bestRating < 0) {
                    bestRating = rating;
                    columns = cols;
                }
                rows = q;
                cols = (cellCount+rows-1)/rows;
                curWidth = cols*cellWidth, curHeight = rows*cellHeight;
                raiseToConstraint(curWidth, curHeight, dimensionsConstraint);
                rating = dimensionsRating(curWidth, curHeight, cols*cellWidth == curWidth);
                if (rating < bestRating || bestRating < 0) {
                    bestRating = rating;
                    columns = cols;
                }
            }
            rows = (cellCount+columns-1)/columns;
        }
        width = columns*cellWidth, height = rows*cellHeight;
        raiseToConstraint(width, height, dimensionsConstraint);
        // raiseToConstraint may have increased dimensions significantly, rerun if cell dimensions can be optimized.
        if (dimensionsConstraint != DimensionsConstraint::NONE && initial.cellWidth < 0 && initial.cellHeight < 0) {
            cellWidth = initial.cellWidth;
            cellHeight = initial.cellHeight;
            columns = initial.columns;
            rows = initial.rows;
            scale = initial.scale;
            return pack(glyphs, count);
        }
    }

    if (columns < 0) {
        columns = (width+spacing)/cellWidth;
        rows = (cellCount+columns-1)/columns;
    }
    if (rows*cellHeight > height)
        rows = height/cellHeight;

    GlyphGeometry::GlyphAttributes attribs = { };
    attribs.scale = scale;
    attribs.range = unitRange+pxRange/scale;
    attribs.innerPadding = innerUnitPadding+1/scale*innerPxPadding;
    attribs.outerPadding = outerUnitPadding+1/scale*outerPxPadding;
    attribs.miterLimit = miterLimit;
    attribs.pxAlignOriginX = pxAlignOriginX;
    attribs.pxAlignOriginY = pxAlignOriginY;
    int col = 0, row = 0;
    for (GlyphGeometry *glyph = glyphs, *end = glyphs+count; glyph < end; ++glyph) {
        if (!glyph->isWhitespace()) {
            glyph->frameBox(attribs, cellWidth-spacing, cellHeight-spacing, hFixed ? &fixedX : nullptr, vFixed ? &fixedY : nullptr);
            glyph->placeBox(col*cellWidth, height-(row+1)*cellHeight);
            if (++col >= columns) {
                if (++row >= rows) {
                    return end-glyph-1;
                }
                col = 0;
            }
        }
    }

    return 0;
}

void GridAtlasPacker::setFixedOrigin(bool horizontal, bool vertical) {
    hFixed = horizontal, vFixed = vertical;
}

void GridAtlasPacker::setCellDimensions(int width, int height) {
    cellWidth = width, cellHeight = height;
}

void GridAtlasPacker::unsetCellDimensions() {
    cellWidth = -1, cellHeight = -1;
}

void GridAtlasPacker::setCellDimensionsConstraint(DimensionsConstraint dimensionsConstraint) {
    cellDimensionsConstraint = dimensionsConstraint;
}

void GridAtlasPacker::setColumns(int columns) {
    this->columns = columns;
}

void GridAtlasPacker::setRows(int rows) {
    this->rows = rows;
}

void GridAtlasPacker::unsetColumns() {
    columns = -1;
}

void GridAtlasPacker::unsetRows() {
    rows = -1;
}

void GridAtlasPacker::setDimensions(int width, int height) {
    this->width = width, this->height = height;
}

void GridAtlasPacker::unsetDimensions() {
    width = -1, height = -1;
}

void GridAtlasPacker::setDimensionsConstraint(DimensionsConstraint dimensionsConstraint) {
    this->dimensionsConstraint = dimensionsConstraint;
}

void GridAtlasPacker::setSpacing(int spacing) {
    this->spacing = spacing;
}

void GridAtlasPacker::setScale(double scale) {
    this->scale = scale;
}

void GridAtlasPacker::setMinimumScale(double minScale) {
    this->minScale = minScale;
}

void GridAtlasPacker::setUnitRange(msdfgen::Range unitRange) {
    this->unitRange = unitRange;
}

void GridAtlasPacker::setPixelRange(msdfgen::Range pxRange) {
    this->pxRange = pxRange;
}

void GridAtlasPacker::setMiterLimit(double miterLimit) {
    this->miterLimit = miterLimit;
}

void GridAtlasPacker::setOriginPixelAlignment(bool align) {
    pxAlignOriginX = align, pxAlignOriginY = align;
}

void GridAtlasPacker::setOriginPixelAlignment(bool alignX, bool alignY) {
    pxAlignOriginX = alignX, pxAlignOriginY = alignY;
}

void GridAtlasPacker::setInnerUnitPadding(const Padding &padding) {
    innerUnitPadding = padding;
}

void GridAtlasPacker::setOuterUnitPadding(const Padding &padding) {
    outerUnitPadding = padding;
}

void GridAtlasPacker::setInnerPixelPadding(const Padding &padding) {
    innerPxPadding = padding;
}

void GridAtlasPacker::setOuterPixelPadding(const Padding &padding) {
    outerPxPadding = padding;
}

void GridAtlasPacker::getDimensions(int &width, int &height) const {
    width = this->width, height = this->height;
}

void GridAtlasPacker::getCellDimensions(int &width, int &height) const {
    width = cellWidth, height = cellHeight;
}

int GridAtlasPacker::getColumns() const {
    return columns;
}

int GridAtlasPacker::getRows() const {
    return rows;
}

double GridAtlasPacker::getScale() const {
    return scale;
}

msdfgen::Range GridAtlasPacker::getPixelRange() const {
    return pxRange+scale*unitRange;
}

void GridAtlasPacker::getFixedOrigin(double &x, double &y) {
    x = fixedX-.5/scale;
    y = fixedY-.5/scale;
}

bool GridAtlasPacker::hasCutoff() const {
    return cutoff;
}

}
