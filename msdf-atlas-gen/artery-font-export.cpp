
#include "artery-font-export.h"

#ifndef MSDF_ATLAS_NO_ARTERY_FONT

#include <artery-font/std-artery-font.h>
#include <artery-font/stdio-serialization.h>
#include "GlyphGeometry.h"
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

static artery_font::CodepointType convertCodepointType(GlyphIdentifierType glyphIdentifierType) {
    switch (glyphIdentifierType) {
        case GlyphIdentifierType::GLYPH_INDEX:
            return artery_font::CP_INDEXED;
        case GlyphIdentifierType::UNICODE_CODEPOINT:
            return artery_font::CP_UNICODE;
    }
    return artery_font::CP_UNSPECIFIED;
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
bool exportArteryFont(const FontGeometry *fonts, int fontCount, const msdfgen::BitmapConstRef<T, N> &atlas, const char *filename, const ArteryFontExportProperties &properties) {
    artery_font::StdArteryFont<REAL> arfont = { };
    arfont.metadataFormat = artery_font::METADATA_NONE;

    arfont.variants = artery_font::StdList<typename artery_font::StdArteryFont<REAL>::Variant>(fontCount);
    for (int i = 0; i < fontCount; ++i) {
        const FontGeometry &font = fonts[i];
        GlyphIdentifierType identifierType = font.getPreferredIdentifierType();
        const msdfgen::FontMetrics &fontMetrics = font.getMetrics();
        typename artery_font::StdArteryFont<REAL>::Variant &fontVariant = arfont.variants[i] = typename artery_font::StdArteryFont<REAL>::Variant();
        fontVariant.codepointType = convertCodepointType(identifierType);
        fontVariant.imageType = convertImageType(properties.imageType);
        fontVariant.metrics.fontSize = REAL(properties.fontSize*fontMetrics.emSize);
        if (properties.imageType != ImageType::HARD_MASK) {
            fontVariant.metrics.distanceRange = REAL(properties.pxRange.upper-properties.pxRange.lower);
            fontVariant.metrics.distanceRangeMiddle = REAL(.5*(properties.pxRange.lower+properties.pxRange.upper));
        }
        fontVariant.metrics.emSize = REAL(fontMetrics.emSize);
        fontVariant.metrics.ascender = REAL(fontMetrics.ascenderY);
        fontVariant.metrics.descender = REAL(fontMetrics.descenderY);
        fontVariant.metrics.lineHeight = REAL(fontMetrics.lineHeight);
        fontVariant.metrics.underlineY = REAL(fontMetrics.underlineY);
        fontVariant.metrics.underlineThickness = REAL(fontMetrics.underlineThickness);
        const char *name = font.getName();
        if (name)
            (std::string &) fontVariant.name = name;
        fontVariant.glyphs = artery_font::StdList<artery_font::Glyph<REAL> >(font.getGlyphs().size());
        int j = 0;
        for (const GlyphGeometry &glyphGeom : font.getGlyphs()) {
            artery_font::Glyph<REAL> &glyph = fontVariant.glyphs[j++];
            glyph.codepoint = glyphGeom.getIdentifier(identifierType);
            glyph.image = 0;
            double l, b, r, t;
            glyphGeom.getQuadPlaneBounds(l, b, r, t);
            glyph.planeBounds.l = REAL(l);
            glyph.planeBounds.b = REAL(b);
            glyph.planeBounds.r = REAL(r);
            glyph.planeBounds.t = REAL(t);
            glyphGeom.getQuadAtlasBounds(l, b, r, t);
            glyph.imageBounds.l = REAL(l);
            glyph.imageBounds.b = REAL(b);
            glyph.imageBounds.r = REAL(r);
            glyph.imageBounds.t = REAL(t);
            glyph.advance.h = REAL(glyphGeom.getAdvance());
            glyph.advance.v = REAL(0);
        }
        switch (identifierType) {
            case GlyphIdentifierType::GLYPH_INDEX:
                for (const std::pair<std::pair<int, int>, double> &elem : font.getKerning()) {
                    artery_font::KernPair<REAL> kernPair = { };
                    kernPair.codepoint1 = elem.first.first;
                    kernPair.codepoint2 = elem.first.second;
                    kernPair.advance.h = REAL(elem.second);
                    ((std::vector<artery_font::KernPair<REAL> > &) fontVariant.kernPairs).push_back((artery_font::KernPair<REAL> &&) kernPair);
                }
                break;
            case GlyphIdentifierType::UNICODE_CODEPOINT:
                for (const std::pair<std::pair<int, int>, double> &elem : font.getKerning()) {
                    const GlyphGeometry *glyph1 = font.getGlyph(msdfgen::GlyphIndex(elem.first.first));
                    const GlyphGeometry *glyph2 = font.getGlyph(msdfgen::GlyphIndex(elem.first.second));
                    if (glyph1 && glyph2 && glyph1->getCodepoint() && glyph2->getCodepoint()) {
                        artery_font::KernPair<REAL> kernPair = { };
                        kernPair.codepoint1 = glyph1->getCodepoint();
                        kernPair.codepoint2 = glyph2->getCodepoint();
                        kernPair.advance.h = REAL(elem.second);
                        ((std::vector<artery_font::KernPair<REAL> > &) fontVariant.kernPairs).push_back((artery_font::KernPair<REAL> &&) kernPair);
                    }
                }
                break;
        }
    }

    arfont.images = artery_font::StdList<typename artery_font::StdArteryFont<REAL>::Image>(1);
    {
        typename artery_font::StdArteryFont<REAL>::Image &image = arfont.images[0] = typename artery_font::StdArteryFont<REAL>::Image();
        image.width = atlas.width;
        image.height = atlas.height;
        image.channels = N;
        image.imageType = convertImageType(properties.imageType);
        switch (properties.imageFormat) {
        #ifndef MSDFGEN_DISABLE_PNG
            case ImageFormat::PNG:
                image.encoding = artery_font::IMAGE_PNG;
                image.pixelFormat = artery_font::PIXEL_UNSIGNED8;
                if (!encodePng((std::vector<byte> &) image.data, atlas))
                    return false;
                break;
        #endif
            case ImageFormat::TIFF:
                image.encoding = artery_font::IMAGE_TIFF;
                image.pixelFormat = artery_font::PIXEL_FLOAT32;
                if (!encodeTiff((std::vector<byte> &) image.data, atlas))
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
                image.data = artery_font::StdByteArray(N*sizeof(T)*atlas.width*atlas.height);
                switch (properties.yDirection) {
                    case YDirection::BOTTOM_UP:
                        image.rawBinaryFormat.orientation = artery_font::ORIENTATION_BOTTOM_UP;
                        memcpy((byte *) image.data, atlas.pixels, N*sizeof(T)*atlas.width*atlas.height);
                        break;
                    case YDirection::TOP_DOWN: {
                        image.rawBinaryFormat.orientation = artery_font::ORIENTATION_TOP_DOWN;
                        byte *imageData = (byte *) image.data;
                        for (int y = atlas.height-1; y >= 0; --y) {
                            memcpy(imageData, atlas.pixels+N*atlas.width*y, N*sizeof(T)*atlas.width);
                            imageData += N*sizeof(T)*atlas.width;
                        }
                        break;
                    }
                }
                break;
            default:
                return false;
        }
    }

    return artery_font::writeFile(arfont, filename);
}

template bool exportArteryFont<float>(const FontGeometry *fonts, int fontCount, const msdfgen::BitmapConstRef<byte, 1> &atlas, const char *filename, const ArteryFontExportProperties &properties);
template bool exportArteryFont<float>(const FontGeometry *fonts, int fontCount, const msdfgen::BitmapConstRef<byte, 3> &atlas, const char *filename, const ArteryFontExportProperties &properties);
template bool exportArteryFont<float>(const FontGeometry *fonts, int fontCount, const msdfgen::BitmapConstRef<byte, 4> &atlas, const char *filename, const ArteryFontExportProperties &properties);
template bool exportArteryFont<float>(const FontGeometry *fonts, int fontCount, const msdfgen::BitmapConstRef<float, 1> &atlas, const char *filename, const ArteryFontExportProperties &properties);
template bool exportArteryFont<float>(const FontGeometry *fonts, int fontCount, const msdfgen::BitmapConstRef<float, 3> &atlas, const char *filename, const ArteryFontExportProperties &properties);
template bool exportArteryFont<float>(const FontGeometry *fonts, int fontCount, const msdfgen::BitmapConstRef<float, 4> &atlas, const char *filename, const ArteryFontExportProperties &properties);

}

#endif
