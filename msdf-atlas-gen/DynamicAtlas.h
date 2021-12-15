
#pragma once

#include <vector>
#include "RectanglePacker.h"
#include "AtlasGenerator.h"

namespace msdf_atlas {

/**
 * This class can be used to produce a dynamic atlas to which more glyphs are added over time.
 * It takes care of laying out and enlarging the atlas as necessary and delegates the actual work
 * to the specified AtlasGenerator, which may e.g. do the work asynchronously.
 */
template <class AtlasGenerator>
class DynamicAtlas {

public:
    enum ChangeFlag {
        NO_CHANGE = 0x00,
        RESIZED = 0x01,
        REARRANGED = 0x02
    };
    typedef int ChangeFlags;

    DynamicAtlas();
    /// Creates with a configured generator. The generator must not contain any prior glyphs!
    explicit DynamicAtlas(AtlasGenerator &&generator);
    /// Adds a batch of glyphs. Adding more than one glyph at a time may improve packing efficiency
    ChangeFlags add(GlyphGeometry *glyphs, int count, bool allowRearrange = false);
    /// Allows access to generator. Do not add glyphs to the generator directly!
    AtlasGenerator & atlasGenerator();
    const AtlasGenerator & atlasGenerator() const;

private:
    AtlasGenerator generator;
    RectanglePacker packer;
    int glyphCount;
    int side;
    std::vector<Rectangle> rectangles;
    std::vector<Remap> remapBuffer;
    int totalArea;
    int padding;

};

}

#include "DynamicAtlas.hpp"
