
#pragma once

#include "image-save.h"

#include <cstdio>
#include <msdfgen-ext.h>

namespace msdf_atlas {

template <int N>
bool saveImageBinary(msdfgen::BitmapConstSection<byte, N> bitmap, const char *filename);
template <int N>
bool saveImageBinaryLE(msdfgen::BitmapConstSection<float, N> bitmap, const char *filename);
template <int N>
bool saveImageBinaryBE(msdfgen::BitmapConstSection<float, N> bitmap, const char *filename);

template <int N>
bool saveImageText(msdfgen::BitmapConstSection<byte, N> bitmap, const char *filename);
template <int N>
bool saveImageText(msdfgen::BitmapConstSection<float, N> bitmap, const char *filename);

template <int N>
bool saveImage(const msdfgen::BitmapConstSection<byte, N> &bitmap, ImageFormat format, const char *filename) {
    switch (format) {
    #ifndef MSDFGEN_DISABLE_PNG
        case ImageFormat::PNG:
            return msdfgen::savePng(bitmap, filename);
    #endif
        case ImageFormat::BMP:
            return msdfgen::saveBmp(bitmap, filename);
        case ImageFormat::TIFF:
            return false;
        case ImageFormat::RGBA:
            return msdfgen::saveRgba(bitmap, filename);
        case ImageFormat::FL32:
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
bool saveImage(const msdfgen::BitmapConstSection<float, N> &bitmap, ImageFormat format, const char *filename) {
    switch (format) {
    #ifndef MSDFGEN_DISABLE_PNG
        case ImageFormat::PNG:
            return msdfgen::savePng(bitmap, filename);
    #endif
        case ImageFormat::BMP:
            return msdfgen::saveBmp(bitmap, filename);
        case ImageFormat::TIFF:
            return msdfgen::saveTiff(bitmap, filename);
        case ImageFormat::RGBA:
            return msdfgen::saveRgba(bitmap, filename);
        case ImageFormat::FL32:
            return msdfgen::saveFl32(bitmap, filename);
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
bool saveImageBinary(msdfgen::BitmapConstSection<byte, N> bitmap, const char *filename) {
    bool success = false;
    if (FILE *f = fopen(filename, "wb")) {
        size_t written = 0;
        for (int y = 0; y < bitmap.height; ++y)
            written += fwrite(bitmap(0, y), 1, (size_t) N*bitmap.width, f);
        success = written == (size_t) N*bitmap.width*bitmap.height;
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
        (msdfgen::BitmapConstSection<float, N> bitmap, const char *filename)
{
    bool success = false;
    if (FILE *f = fopen(filename, "wb")) {
        size_t written = 0;
        for (int y = 0; y < bitmap.height; ++y)
            written += fwrite(bitmap(0, y), sizeof(float), (size_t) N*bitmap.width, f);
        success = written == (size_t) N*bitmap.width*bitmap.height;
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
        (msdfgen::BitmapConstSection<float, N> bitmap, const char *filename)
{
    bool success = false;
    if (FILE *f = fopen(filename, "wb")) {
        size_t written = 0;
        for (int y = 0; y < bitmap.height; ++y) {
            const float *p = bitmap(0, y);
            for (int x = 0; x < bitmap.width; ++x) {
                const unsigned char *b = reinterpret_cast<const unsigned char *>(p++);
                for (int i = sizeof(float)-1; i >= 0; --i)
                    written += fwrite(b+i, 1, 1, f);
            }
        }
        success = written == sizeof(float)*N*bitmap.width*bitmap.height;
        fclose(f);
    }
    return success;
}


template <int N>
bool saveImageText(msdfgen::BitmapConstSection<byte, N> bitmap, const char *filename) {
    bool success = false;
    if (FILE *f = fopen(filename, "wb")) {
        success = true;
        for (int y = 0; y < bitmap.height; ++y) {
            const byte *p = bitmap(0, y);
            for (int x = 0; x < N*bitmap.width; ++x)
                success &= fprintf(f, x ? " %02X" : "%02X", (unsigned) *p++) > 0;
            success &= fprintf(f, "\n") > 0;
        }
        fclose(f);
    }
    return success;
}

template <int N>
bool saveImageText(msdfgen::BitmapConstSection<float, N> bitmap, const char *filename) {
    bool success = false;
    if (FILE *f = fopen(filename, "wb")) {
        success = true;
        for (int y = 0; y < bitmap.height; ++y) {
            const float *p = bitmap(0, y);
            for (int x = 0; x < N*bitmap.width; ++x)
                success &= fprintf(f, x ? " %g" : "%g", *p++) > 0;
            success &= fprintf(f, "\n") > 0;
        }
        fclose(f);
    }
    return success;
}

}
