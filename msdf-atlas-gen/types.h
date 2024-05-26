
#pragma once

#include <cstdint>

namespace msdf_atlas {

typedef unsigned char byte;
typedef uint32_t unicode_t;

/// Type of atlas image contents
enum class ImageType {
    /// Rendered glyphs without anti-aliasing (two colors only)
    HARD_MASK,
    /// Rendered glyphs with anti-aliasing
    SOFT_MASK,
    /// Signed (true) distance field
    SDF,
    /// Signed perpendicular distance field
    PSDF,
    /// Multi-channel signed distance field
    MSDF,
    /// Multi-channel & true signed distance field
    MTSDF
};

/// Atlas image encoding
enum class ImageFormat {
    UNSPECIFIED,
    PNG,
    BMP,
    TIFF,
    RGBA,
    FL32,
    TEXT,
    TEXT_FLOAT,
    BINARY,
    BINARY_FLOAT,
    BINARY_FLOAT_BE
};

/// Glyph identification
enum class GlyphIdentifierType {
    GLYPH_INDEX,
    UNICODE_CODEPOINT
};

/// Direction of the Y-axis
enum class YDirection {
    BOTTOM_UP,
    TOP_DOWN
};

/// The method of computing the layout of the atlas
enum class PackingStyle {
    TIGHT,
    GRID
};

/// Constraints for the atlas's dimensions - see size selectors for more info
enum class DimensionsConstraint {
    NONE,
    SQUARE,
    EVEN_SQUARE,
    MULTIPLE_OF_FOUR_SQUARE,
    POWER_OF_TWO_RECTANGLE,
    POWER_OF_TWO_SQUARE
};

}
