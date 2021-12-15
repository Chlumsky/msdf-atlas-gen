
#pragma once

#include <vector>
#include "Rectangle.h"

namespace msdf_atlas {

/// Guillotine 2D single bin packer
class RectanglePacker {

public:
    RectanglePacker();
    RectanglePacker(int width, int height);
    /// Expands the packing area - both width and height must be greater or equal to the previous value
    void expand(int width, int height);
    /// Packs the rectangle array, returns how many didn't fit (0 on success)
    int pack(Rectangle *rectangles, int count);
    int pack(OrientedRectangle *rectangles, int count);

private:
    std::vector<Rectangle> spaces;

    static int rateFit(int w, int h, int sw, int sh);

    void splitSpace(int index, int w, int h);

};

}
