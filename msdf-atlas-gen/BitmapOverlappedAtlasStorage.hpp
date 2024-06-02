
#include "BitmapOverlappedAtlasStorage.h"

#include <cstring>
#include <algorithm>
#include "bitmap-blit.h"

namespace msdf_atlas {

template <typename T>
T minPxVal();

template <>
byte minPxVal() {
    return 0;
}

template <>
float minPxVal() {
    return -FLT_MAX;
}

template <typename T, int N>
BitmapOverlappedAtlasStorage<T, N>::BitmapOverlappedAtlasStorage() { }

template <typename T, int N>
BitmapOverlappedAtlasStorage<T, N>::BitmapOverlappedAtlasStorage(int width, int height) : bitmap(width, height), stencil(width, height) {
    for (T *cur = (T *) bitmap, *end = cur+N*width*height; cur < end; ++cur)
        *cur = minPxVal<T>();
    memset((byte *) stencil, 0, sizeof(byte)*width*height);
}

template <typename T, int N>
BitmapOverlappedAtlasStorage<T, N>::BitmapOverlappedAtlasStorage(const BitmapOverlappedAtlasStorage<T, N> &orig, int width, int height) : bitmap(width, height) {
    for (T *cur = (T *) bitmap, *end = cur+N*width*height; cur < end; ++cur)
        *cur = minPxVal<T>();
    memset((byte *) stencil, 0, sizeof(byte)*width*height);
    blit(bitmap, orig.bitmap, 0, 0, 0, 0, std::min(width, orig.bitmap.width()), std::min(height, orig.bitmap.height()));
    blit(stencil, orig.stencil, 0, 0, 0, 0, std::min(width, orig.bitmap.width()), std::min(height, orig.bitmap.height()));
}

template <typename T, int N>
BitmapOverlappedAtlasStorage<T, N>::operator msdfgen::BitmapConstRef<T, N>() const {
    return bitmap;
}

template <typename T, int N>
BitmapOverlappedAtlasStorage<T, N>::operator msdfgen::BitmapRef<T, N>() {
    return bitmap;
}

template <typename T, int N>
BitmapOverlappedAtlasStorage<T, N>::operator msdfgen::Bitmap<T, N>() && {
    return (msdfgen::Bitmap<T, N> &&) bitmap;
}

template <typename T, int N>
template <typename S>
void BitmapOverlappedAtlasStorage<T, N>::put(int x, int y, const msdfgen::BitmapConstRef<S, N> &subBitmap) {
    blend((msdfgen::BitmapRef<T, N>) bitmap, subBitmap, x, y, 0, 0, subBitmap.width, subBitmap.height);
}

template <typename T, int N>
void BitmapOverlappedAtlasStorage<T, N>::get(int x, int y, const msdfgen::BitmapRef<T, N> &subBitmap) const {
    blit(subBitmap, bitmap, 0, 0, x, y, subBitmap.width, subBitmap.height);
}

#define MSDF_ATLAS_BITMAP_OVERLAPPED_ATLAS_STORAGE_BOUND_AREA() { \
    if (dx < 0) w += dx, sx -= dx, dx = 0; \
    if (dy < 0) h += dy, sy -= dy, dy = 0; \
    if (sx < 0) w += sx, dx -= sx, sx = 0; \
    if (sy < 0) h += sy, dy -= sy, sy = 0; \
    w = std::max(0, std::min(w, std::min(dst.width-dx, src.width-sx))); \
    h = std::max(0, std::min(h, std::min(dst.height-dy, src.height-sy))); \
}

template <typename T, int N>
void BitmapOverlappedAtlasStorage<T, N>::blend(const msdfgen::BitmapRef<byte, N> &dst, const msdfgen::BitmapConstRef<byte, N> &src, int dx, int dy, int sx, int sy, int w, int h) {
    MSDF_ATLAS_BITMAP_OVERLAPPED_ATLAS_STORAGE_BOUND_AREA();
    for (int y = 0; y < h; ++y) {
        byte *dstPixel = dst(dx, dy+y);
        const byte *srcPixel = src(sx, sy+y);
        byte *stencilPixel = stencil(dx, dy+y);
        for (int x = 0; x < w; ++x) {
            for (int i = 0; i < N; ++i, ++dstPixel, ++srcPixel) {
                if (*dstPixel != *srcPixel) {
                    if (*stencilPixel)
                        *dstPixel = minPxVal<byte>();
                    else
                        *dstPixel = *srcPixel;
                }
            }
            *stencilPixel++ = 1;
        }
    }
}

template <typename T, int N>
void BitmapOverlappedAtlasStorage<T, N>::blend(const msdfgen::BitmapRef<byte, N> &dst, const msdfgen::BitmapConstRef<float, N> &src, int dx, int dy, int sx, int sy, int w, int h) {
    MSDF_ATLAS_BITMAP_OVERLAPPED_ATLAS_STORAGE_BOUND_AREA();
    for (int y = 0; y < h; ++y) {
        byte *dstPixel = dst(dx, dy+y);
        const float *srcPixel = src(sx, sy+y);
        byte *stencilPixel = stencil(dx, dy+y);
        for (int x = 0; x < w; ++x) {
            for (int i = 0; i < N; ++i, ++dstPixel, ++srcPixel) {
                byte srcByte = msdfgen::pixelFloatToByte(*srcPixel);
                if (*dstPixel != srcByte) {
                    if (*stencilPixel)
                        *dstPixel = minPxVal<byte>();
                    else
                        *dstPixel = srcByte;
                }
            }
            *stencilPixel++ = 1;
        }
    }
}

template <typename T, int N>
void BitmapOverlappedAtlasStorage<T, N>::blend(const msdfgen::BitmapRef<float, N> &dst, const msdfgen::BitmapConstRef<float, N> &src, int dx, int dy, int sx, int sy, int w, int h) {
    MSDF_ATLAS_BITMAP_OVERLAPPED_ATLAS_STORAGE_BOUND_AREA();
    for (int y = 0; y < h; ++y) {
        float *dstPixel = dst(dx, dy+y);
        const float *srcPixel = src(sx, sy+y);
        byte *stencilPixel = stencil(dx, dy+y);
        for (int x = 0; x < w; ++x) {
            for (int i = 0; i < N; ++i, ++dstPixel, ++srcPixel) {
                if (*dstPixel != *srcPixel) {
                    if (*stencilPixel)
                        *dstPixel = minPxVal<float>();
                    else
                        *dstPixel = *srcPixel;
                }
            }
            *stencilPixel++ = 1;
        }
    }
}

#undef MSDF_ATLAS_BITMAP_OVERLAPPED_ATLAS_STORAGE_BOUND_AREA

}
