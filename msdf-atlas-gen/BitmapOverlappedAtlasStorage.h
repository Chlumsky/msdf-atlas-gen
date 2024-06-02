
#pragma once

#include "AtlasStorage.h"

namespace msdf_atlas {

/** An implementation of AtlasStorage represented by a bitmap in memory (msdfgen::Bitmap)
 *  which has improved handling of partially overlapping glyph rectangles
 */
template <typename T, int N>
class BitmapOverlappedAtlasStorage {

public:
    BitmapOverlappedAtlasStorage();
    BitmapOverlappedAtlasStorage(int width, int height);
    BitmapOverlappedAtlasStorage(const BitmapOverlappedAtlasStorage<T, N> &orig, int width, int height);
    operator msdfgen::BitmapConstRef<T, N>() const;
    operator msdfgen::BitmapRef<T, N>();
    operator msdfgen::Bitmap<T, N>() &&;
    template <typename S>
    void put(int x, int y, const msdfgen::BitmapConstRef<S, N> &subBitmap);
    void get(int x, int y, const msdfgen::BitmapRef<T, N> &subBitmap) const;

private:
    msdfgen::Bitmap<T, N> bitmap;
    msdfgen::Bitmap<byte, 1> stencil;

    void blend(const msdfgen::BitmapRef<byte, N> &dst, const msdfgen::BitmapConstRef<byte, N> &src, int dx, int dy, int sx, int sy, int w, int h);
    void blend(const msdfgen::BitmapRef<byte, N> &dst, const msdfgen::BitmapConstRef<float, N> &src, int dx, int dy, int sx, int sy, int w, int h);
    void blend(const msdfgen::BitmapRef<float, N> &dst, const msdfgen::BitmapConstRef<float, N> &src, int dx, int dy, int sx, int sy, int w, int h);

};

}

#include "BitmapOverlappedAtlasStorage.hpp"
