
#include "image-save.h"

#include <cstdio>
#include <msdfgen-ext.h>

namespace msdf_atlas {

template <int N>
bool saveImageBinary(const msdfgen::BitmapConstRef<byte, N> &bitmap, const char *filename);
template <int N>
bool saveImageBinaryLE(const msdfgen::BitmapConstRef<float, N> &bitmap, const char *filename);
template <int N>
bool saveImageBinaryBE(const msdfgen::BitmapConstRef<float, N> &bitmap, const char *filename);

template <int N>
bool saveImageText(const msdfgen::BitmapConstRef<byte, N> &bitmap, const char *filename);
template <int N>
bool saveImageText(const msdfgen::BitmapConstRef<float, N> &bitmap, const char *filename);

template <int N>
bool saveImage(const msdfgen::BitmapConstRef<byte, N> &bitmap, ImageFormat format, const char *filename) {
    switch (format) {
        case ImageFormat::PNG:
            return msdfgen::savePng(bitmap, filename);
        case ImageFormat::BMP:
            return msdfgen::saveBmp(bitmap, filename);
        case ImageFormat::TIFF:
            return false;
        case ImageFormat::TEXT:
            return saveImageText(bitmap, filename);
        case ImageFormat::TEXT_FLOAT:
            return false;
        case ImageFormat::BINARY:
            return saveImageBinary(bitmap, filename);
        case ImageFormat::BINARY_FLOAT:
        case ImageFormat::BINARY_FLOAT_BE:
            return false;
        default:;
    }
    return false;
}

template <int N>
bool saveImage(const msdfgen::BitmapConstRef<float, N> &bitmap, ImageFormat format, const char *filename) {
    switch (format) {
        case ImageFormat::PNG:
            return msdfgen::savePng(bitmap, filename);
        case ImageFormat::BMP:
            return msdfgen::saveBmp(bitmap, filename);
        case ImageFormat::TIFF:
            return msdfgen::saveTiff(bitmap, filename);
        case ImageFormat::TEXT:
            return false;
        case ImageFormat::TEXT_FLOAT:
            return saveImageText(bitmap, filename);
        case ImageFormat::BINARY:
            return false;
        case ImageFormat::BINARY_FLOAT:
            return saveImageBinaryLE(bitmap, filename);
        case ImageFormat::BINARY_FLOAT_BE:
            return saveImageBinaryBE(bitmap, filename);
        default:;
    }
    return false;
}

template <int N>
bool saveImageBinary(const msdfgen::BitmapConstRef<byte, N> &bitmap, const char *filename) {
    bool success = false;
    if (FILE *f = fopen(filename, "wb")) {
        success = fwrite(bitmap.pixels, 1, N*bitmap.width*bitmap.height, f) == N*bitmap.width*bitmap.height;
        fclose(f);
    }
    return success;
}

template <int N>
bool
    #ifdef __BIG_ENDIAN__
        saveImageBinaryBE
    #else
        saveImageBinaryLE
    #endif
        (const msdfgen::BitmapConstRef<float, N> &bitmap, const char *filename) {
    bool success = false;
    if (FILE *f = fopen(filename, "wb")) {
        success = fwrite(bitmap.pixels, sizeof(float), N*bitmap.width*bitmap.height, f) == N*bitmap.width*bitmap.height;
        fclose(f);
    }
    return success;
}

template <int N>
bool
    #ifdef __BIG_ENDIAN__
        saveImageBinaryLE
    #else
        saveImageBinaryBE
    #endif
        (const msdfgen::BitmapConstRef<float, N> &bitmap, const char *filename) {
    bool success = false;
    if (FILE *f = fopen(filename, "wb")) {
        const float *p = bitmap.pixels;
        int count = N*bitmap.width*bitmap.height;
        int written = 0;
        for (int i = 0; i < count; ++i) {
            const unsigned char *b = reinterpret_cast<const unsigned char *>(p++);
            for (int i = sizeof(float)-1; i >= 0; --i)
                written += fwrite(b+i, 1, 1, f);
        }
        success = written == sizeof(float)*count;
        fclose(f);
    }
    return success;
}


template <int N>
bool saveImageText(const msdfgen::BitmapConstRef<byte, N> &bitmap, const char *filename) {
    bool success = false;
    if (FILE *f = fopen(filename, "wb")) {
        const byte *p = bitmap.pixels;
        for (int y = 0; y < bitmap.height; ++y) {
            for (int x = 0; x < N*bitmap.width; ++x) {
                fprintf(f, x ? " %02X" : "%02X", (unsigned) *p++);
            }
            fprintf(f, "\n");
        }
        fclose(f);
    }
    return success;
}

template <int N>
bool saveImageText(const msdfgen::BitmapConstRef<float, N> &bitmap, const char *filename) {
    bool success = false;
    if (FILE *f = fopen(filename, "wb")) {
        const float *p = bitmap.pixels;
        for (int y = 0; y < bitmap.height; ++y) {
            for (int x = 0; x < N*bitmap.width; ++x) {
                fprintf(f, x ? " %g" : "%g", *p++);
            }
            fprintf(f, "\n");
        }
        fclose(f);
    }
    return success;
}

}
