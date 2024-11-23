
#include "RectanglePacker.h"

namespace msdf_atlas {

#define WORST_FIT 0x7fffffff

template <typename T>
static void removeFromUnorderedVector(std::vector<T> &vector, size_t index) {
    if (index != vector.size()-1)
        vector[index] = (T &&) vector.back();
    vector.pop_back();
}

int RectanglePacker::rateFit(int w, int h, int sw, int sh) {
    return std::min(sw-w, sh-h);
}

RectanglePacker::RectanglePacker() : RectanglePacker(0, 0) { }

RectanglePacker::RectanglePacker(int width, int height) {
    if (width > 0 && height > 0)
        spaces.push_back(Rectangle { 0, 0, width, height });
}

void RectanglePacker::expand(int width, int height) {
    if (width > 0 && height > 0) {
        int oldWidth = 0, oldHeight = 0;
        for (const Rectangle &space : spaces) {
            if (space.x+space.w > oldWidth)
                oldWidth = space.x+space.w;
            if (space.y+space.h > oldHeight)
                oldHeight = space.y+space.h;
        }
        spaces.push_back(Rectangle { 0, 0, width, height });
        splitSpace(int(spaces.size()-1), oldWidth, oldHeight);
    }
}

void RectanglePacker::splitSpace(int index, int w, int h) {
    Rectangle space = spaces[index];
    removeFromUnorderedVector(spaces, index);
    Rectangle a = { space.x, space.y+h, w, space.h-h };
    Rectangle b = { space.x+w, space.y, space.w-w, h };
    if (w*(space.h-h) < h*(space.w-w))
        a.w = space.w;
    else
        b.h = space.h;
    if (a.w > 0 && a.h > 0)
        spaces.push_back(a);
    if (b.w > 0 && b.h > 0)
        spaces.push_back(b);
}

int RectanglePacker::pack(Rectangle *rectangles, int count) {
    struct RectangleCache {
        Rectangle *rectangle;
        int bestFit;
        int bestSpaceIndex;
    };
    std::vector<RectangleCache> remainingRects;
    remainingRects.reserve(count);
    int modifiedSpaceIndex0 = -1, modifiedSpaceIndex1 = -1;
    bool spaceAdded = false;
    RectangleCache initialCache = { rectangles, WORST_FIT, modifiedSpaceIndex0 };
    for (Rectangle *end = rectangles+count; initialCache.rectangle < end; ++initialCache.rectangle) {
        if (initialCache.rectangle->w > 0 && initialCache.rectangle->h > 0)
            remainingRects.push_back(initialCache);
    }
    while (!remainingRects.empty()) {
        int bestRectFit = WORST_FIT;
        int bestRectIndex = -1;
        for (int rectIndex = 0; rectIndex < (int) remainingRects.size(); ++rectIndex) {
            RectangleCache &rectCache = remainingRects[rectIndex];
            if (rectCache.bestSpaceIndex == modifiedSpaceIndex0 || rectCache.bestSpaceIndex == modifiedSpaceIndex1) {
                int bestSpaceFit = WORST_FIT;
                for (int spaceIndex = 0; spaceIndex < (int) spaces.size() && bestSpaceFit > 0; ++spaceIndex) {
                    if (spaces[spaceIndex].w >= rectCache.rectangle->w && spaces[spaceIndex].h >= rectCache.rectangle->h) {
                        int fit = rateFit(rectCache.rectangle->w, rectCache.rectangle->h, spaces[spaceIndex].w, spaces[spaceIndex].h);
                        if (fit < bestSpaceFit) {
                            bestSpaceFit = fit;
                            rectCache.bestSpaceIndex = spaceIndex;
                        }
                    }
                }
                rectCache.bestFit = bestSpaceFit;
            }
            if (rectCache.bestFit < bestRectFit) {
                bestRectFit = rectCache.bestFit;
                bestRectIndex = rectIndex;
            }
        }
        if (bestRectIndex < 0) // None of the remaining rectangles fits
            break;
        RectangleCache &rectCache = remainingRects[bestRectIndex];
        rectCache.rectangle->x = spaces[rectCache.bestSpaceIndex].x;
        rectCache.rectangle->y = spaces[rectCache.bestSpaceIndex].y;
        modifiedSpaceIndex0 = rectCache.bestSpaceIndex;
        modifiedSpaceIndex1 = (int) (spaces.size()-1);
        splitSpace(rectCache.bestSpaceIndex, rectCache.rectangle->w, rectCache.rectangle->h);
        spaceAdded = modifiedSpaceIndex1 < (int) (spaces.size()-1);
        removeFromUnorderedVector(remainingRects, bestRectIndex);
    }
    return (int) remainingRects.size();

    /*std::vector<int> remainingRects(count);
    for (int i = 0; i < count; ++i)
        remainingRects[i] = i;
    while (!remainingRects.empty()) {
        int bestFit = WORST_FIT;
        int bestSpace = -1;
        int bestRect = -1;
        for (size_t i = 0; i < spaces.size(); ++i) {
            const Rectangle &space = spaces[i];
            for (size_t j = 0; j < remainingRects.size(); ++j) {
                const Rectangle &rect = rectangles[remainingRects[j]];
                if (rect.w == space.w && rect.h == space.h) {
                    bestSpace = i;
                    bestRect = j;
                    goto BEST_FIT_FOUND;
                }
                if (rect.w <= space.w && rect.h <= space.h) {
                    int fit = rateFit(rect.w, rect.h, space.w, space.h);
                    if (fit < bestFit) {
                        bestSpace = i;
                        bestRect = j;
                        bestFit = fit;
                    }
                }
            }
        }
        if (bestSpace < 0 || bestRect < 0)
            break;
    BEST_FIT_FOUND:
        Rectangle &rect = rectangles[remainingRects[bestRect]];
        rect.x = spaces[bestSpace].x;
        rect.y = spaces[bestSpace].y;
        splitSpace(bestSpace, rect.w, rect.h);
        removeFromUnorderedVector(remainingRects, bestRect);
    }
    return (int) remainingRects.size();*/
}

int RectanglePacker::pack(OrientedRectangle *rectangles, int count) {
    std::vector<int> remainingRects(count);
    for (int i = 0; i < count; ++i)
        remainingRects[i] = i;
    while (!remainingRects.empty()) {
        int bestFit = WORST_FIT;
        int bestSpace = -1;
        int bestRect = -1;
        bool bestRotated = false;
        for (size_t i = 0; i < spaces.size(); ++i) {
            const Rectangle &space = spaces[i];
            for (size_t j = 0; j < remainingRects.size(); ++j) {
                const OrientedRectangle &rect = rectangles[remainingRects[j]];
                if (rect.w == space.w && rect.h == space.h) {
                    bestSpace = i;
                    bestRect = j;
                    bestRotated = false;
                    goto BEST_FIT_FOUND;
                }
                if (rect.h == space.w && rect.w == space.h) {
                    bestSpace = i;
                    bestRect = j;
                    bestRotated = true;
                    goto BEST_FIT_FOUND;
                }
                if (rect.w <= space.w && rect.h <= space.h) {
                    int fit = rateFit(rect.w, rect.h, space.w, space.h);
                    if (fit < bestFit) {
                        bestSpace = i;
                        bestRect = j;
                        bestRotated = false;
                        bestFit = fit;
                    }
                }
                if (rect.h <= space.w && rect.w <= space.h) {
                    int fit = rateFit(rect.h, rect.w, space.w, space.h);
                    if (fit < bestFit) {
                        bestSpace = i;
                        bestRect = j;
                        bestRotated = true;
                        bestFit = fit;
                    }
                }
            }
        }
        if (bestSpace < 0 || bestRect < 0)
            break;
    BEST_FIT_FOUND:
        OrientedRectangle &rect = rectangles[remainingRects[bestRect]];
        rect.x = spaces[bestSpace].x;
        rect.y = spaces[bestSpace].y;
        rect.rotated = bestRotated;
        if (bestRotated)
            splitSpace(bestSpace, rect.h, rect.w);
        else
            splitSpace(bestSpace, rect.w, rect.h);
        removeFromUnorderedVector(remainingRects, bestRect);
    }
    return (int) remainingRects.size();
}

}
