
#include "rectangle-packing.h"

#include <vector>
#include "RectanglePacker.h"

namespace msdf_atlas {

static void copyRectanglePlacement(Rectangle &dst, const Rectangle &src, int offset) {
    dst.x = src.x+offset;
    dst.y = src.y+offset;
}

static void copyRectanglePlacement(OrientedRectangle &dst, const OrientedRectangle &src, int offset) {
    dst.x = src.x+offset;
    dst.y = src.y+offset;
    dst.rotated = src.rotated;
}

template <typename RectangleType>
int packRectangles(RectangleType *rectangles, int count, int width, int height, int sizeOffset, int padding) {
    if (sizeOffset || padding)
        for (int i = 0; i < count; ++i) {
            rectangles[i].w += sizeOffset+padding;
            rectangles[i].h += sizeOffset+padding;
        }
    int result = RectanglePacker(width+sizeOffset-padding, height+sizeOffset-padding).pack(rectangles, count);
    if (sizeOffset || padding)
        for (int i = 0; i < count; ++i) {
            rectangles[i].x += padding;
            rectangles[i].y += padding;
            rectangles[i].w -= sizeOffset+padding;
            rectangles[i].h -= sizeOffset+padding;
        }
    return result;
}

template <class SizeSelector, typename RectangleType>
std::pair<int, int> packRectangles(RectangleType *rectangles, int count, int sizeOffset, int padding) {
    std::vector<RectangleType> rectanglesCopy(count);
    int totalArea = 0;
    for (int i = 0; i < count; ++i) {
        rectanglesCopy[i].w = rectangles[i].w+sizeOffset+padding;
        rectanglesCopy[i].h = rectangles[i].h+sizeOffset+padding;
        totalArea += rectangles[i].w*rectangles[i].h;
    }
    std::pair<int, int> dimensions;
    SizeSelector sizeSelector(totalArea);
    int width, height;
    while (sizeSelector(width, height)) {
        if (!RectanglePacker(width+sizeOffset-padding, height+sizeOffset-padding).pack(rectanglesCopy.data(), count)) {
            dimensions.first = width;
            dimensions.second = height;
            for (int i = 0; i < count; ++i)
                copyRectanglePlacement(rectangles[i], rectanglesCopy[i], padding);
            --sizeSelector;
        } else
            ++sizeSelector;
    }
    return dimensions;
}

}
