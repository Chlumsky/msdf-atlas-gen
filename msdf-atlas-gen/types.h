
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
    /// Signed pseudo-distance field
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
    TEXT,
    TEXT_FLOAT,
    BINARY,
    BINARY_FLOAT,
    BINARY_FLOAT_BE
};

}
