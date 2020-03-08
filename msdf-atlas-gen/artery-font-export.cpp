
#include "artery-font-export.h"

#include <artery-font/std-artery-font.h>
#include <artery-font/stdio-serialization.h>
#include "image-encode.h"

namespace msdf_atlas {

static artery_font::ImageType convertImageType(ImageType imageType) {
    switch (imageType) {
        case ImageType::HARD_MASK:
        case ImageType::SOFT_MASK:
            return artery_font::IMAGE_LINEAR_MASK;
        case ImageType::SDF:
            return artery_font::IMAGE_SDF;
        case ImageType::PSDF:
            return artery_font::IMAGE_PSDF;
        case ImageType::MSDF:
            return artery_font::IMAGE_MSDF;
        case ImageType::MTSDF:
            return artery_font::IMAGE_MTSDF;
    }
    return artery_font::IMAGE_NONE;
}

template <typename T, int N>
static bool encodeTiff(std::vector<byte> &output, const msdfgen::BitmapConstRef<T, N> &atlas) {
    // TODO
    return false;
}

template <typename T>
static artery_font::PixelFormat getPixelFormat();

template <>
artery_font::PixelFormat getPixelFormat<byte>() {
    return artery_font::PIXEL_UNSIGNED8;
}
template <>
artery_font::PixelFormat getPixelFormat<float>() {
    return artery_font::PIXEL_FLOAT32;
}

template <typename REAL, typename T, int N>
bool exportArteryFont(msdfgen::FontHandle *font, const GlyphGeometry *glyphs, int glyphCount, double fontSize, double pxRange, const msdfgen::BitmapConstRef<T, N> &atlas, ImageType imageType, ImageFormat imageFormat, const char *filename) {
    artery_font::StdArteryFont<REAL> arfont = { };
    arfont.metadataFormat = artery_font::METADATA_NONE;

    if (glyphCount > 0) {
        msdfgen::FontMetrics fontMetrics;
        if (!msdfgen::getFontMetrics(fontMetrics, font))
            return false;
        double fsScale = 1/fontMetrics.emSize;
        artery_font::StdFontVariant<REAL> fontVariant = { };
        fontVariant.codepointType = artery_font::CP_UNICODE;
        fontVariant.imageType = convertImageType(imageType);
        fontVariant.metrics.fontSize = REAL(fontSize);
        if (imageType != ImageType::HARD_MASK)
            fontVariant.metrics.distanceRange = REAL(pxRange);
        fontVariant.metrics.emSize = REAL(fsScale*fontMetrics.emSize);
        fontVariant.metrics.ascender = REAL(fsScale*fontMetrics.ascenderY);
        fontVariant.metrics.descender = REAL(fsScale*fontMetrics.descenderY);
        fontVariant.metrics.lineHeight = REAL(fsScale*fontMetrics.lineHeight);
        fontVariant.metrics.underlineY = REAL(fsScale*fontMetrics.underlineY);
        fontVariant.metrics.underlineThickness = REAL(fsScale*fontMetrics.underlineThickness);
        fontVariant.glyphs = artery_font::StdList<artery_font::Glyph<REAL> >(glyphCount);
        for (int i = 0; i < glyphCount; ++i) {
            artery_font::Glyph<REAL> &glyph = fontVariant.glyphs[i];
            glyph.codepoint = glyphs[i].getCodepoint();
            glyph.image = 0;
            double l, b, r, t;
            glyphs[i].getQuadPlaneBounds(l, b, r, t);
            glyph.planeBounds.l = REAL(fsScale*l);
            glyph.planeBounds.b = REAL(fsScale*b);
            glyph.planeBounds.r = REAL(fsScale*r);
            glyph.planeBounds.t = REAL(fsScale*t);
            glyphs[i].getQuadAtlasBounds(l, b, r, t);
            glyph.imageBounds.l = REAL(l);
            glyph.imageBounds.b = REAL(b);
            glyph.imageBounds.r = REAL(r);
            glyph.imageBounds.t = REAL(t);
            glyph.advance.h = REAL(fsScale*glyphs[i].getAdvance());
            glyph.advance.v = REAL(0);
            for (int j = 0; j < glyphCount; ++j) {
                double kerning;
                if (msdfgen::getKerning(kerning, font, glyphs[i].getCodepoint(), glyphs[j].getCodepoint()) && kerning) {
                    artery_font::KernPair<REAL> kernPair = { };
                    kernPair.codepoint1 = glyphs[i].getCodepoint();
                    kernPair.codepoint2 = glyphs[j].getCodepoint();
                    kernPair.advance.h = REAL(fsScale*kerning);
                    fontVariant.kernPairs.vector.push_back((artery_font::KernPair<REAL> &&) kernPair);
                }
            }
        }
        arfont.variants.vector.push_back((artery_font::StdFontVariant<REAL> &&) fontVariant);
    }

    {
        artery_font::StdImage image = { };
        image.width = atlas.width;
        image.height = atlas.height;
        image.channels = N;
        image.imageType = convertImageType(imageType);
        switch (imageFormat) {
            case ImageFormat::PNG:
                image.encoding = artery_font::IMAGE_PNG;
                image.pixelFormat = artery_font::PIXEL_UNSIGNED8;
                if (!encodePng(image.data.vector, atlas))
                    return false;
                break;
            case ImageFormat::TIFF:
                image.encoding = artery_font::IMAGE_TIFF;
                image.pixelFormat = artery_font::PIXEL_FLOAT32;
                if (!encodeTiff(image.data.vector, atlas))
                    return false;
                break;
            case ImageFormat::BINARY:
                image.pixelFormat = artery_font::PIXEL_UNSIGNED8;
                goto BINARY_EITHER;
            case ImageFormat::BINARY_FLOAT:
                image.pixelFormat = artery_font::PIXEL_FLOAT32;
                goto BINARY_EITHER;
            BINARY_EITHER:
                if (image.pixelFormat != getPixelFormat<T>())
                    return false;
                image.encoding = artery_font::IMAGE_RAW_BINARY;
                image.rawBinaryFormat.rowLength = N*sizeof(T)*atlas.width;
                image.rawBinaryFormat.orientation = artery_font::ORIENTATION_BOTTOM_UP;
                image.data = artery_font::StdByteArray(N*sizeof(T)*atlas.width*atlas.height);
                memcpy((byte *) image.data, atlas.pixels, N*sizeof(T)*atlas.width*atlas.height);
                break;
            default:
                return false;
        }
        arfont.images.vector.push_back((artery_font::StdImage &&) image);
    }

    return artery_font::writeFile(arfont, filename);
}

template bool exportArteryFont<float>(msdfgen::FontHandle *font, const GlyphGeometry *glyphs, int glyphCount, double fontSize, double pxRange, const msdfgen::BitmapConstRef<byte, 1> &atlas, ImageType imageType, ImageFormat imageFormat, const char *filename);
template bool exportArteryFont<float>(msdfgen::FontHandle *font, const GlyphGeometry *glyphs, int glyphCount, double fontSize, double pxRange, const msdfgen::BitmapConstRef<byte, 3> &atlas, ImageType imageType, ImageFormat imageFormat, const char *filename);
template bool exportArteryFont<float>(msdfgen::FontHandle *font, const GlyphGeometry *glyphs, int glyphCount, double fontSize, double pxRange, const msdfgen::BitmapConstRef<byte, 4> &atlas, ImageType imageType, ImageFormat imageFormat, const char *filename);
template bool exportArteryFont<float>(msdfgen::FontHandle *font, const GlyphGeometry *glyphs, int glyphCount, double fontSize, double pxRange, const msdfgen::BitmapConstRef<float, 1> &atlas, ImageType imageType, ImageFormat imageFormat, const char *filename);
template bool exportArteryFont<float>(msdfgen::FontHandle *font, const GlyphGeometry *glyphs, int glyphCount, double fontSize, double pxRange, const msdfgen::BitmapConstRef<float, 3> &atlas, ImageType imageType, ImageFormat imageFormat, const char *filename);
template bool exportArteryFont<float>(msdfgen::FontHandle *font, const GlyphGeometry *glyphs, int glyphCount, double fontSize, double pxRange, const msdfgen::BitmapConstRef<float, 4> &atlas, ImageType imageType, ImageFormat imageFormat, const char *filename);

}
