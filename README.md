
# Multi-channel signed distance field atlas generator

This is a utility for generating compact font atlases using [MSDFgen](https://github.com/Chlumsky/msdfgen).

The atlas generator loads a subset of glyphs from a TTF or OTF font file, generates a distance field for each of them, and tightly packs them into an atlas bitmap (example below). The finished atlas and/or its layout metadata can be exported as an [Artery Font](https://github.com/Chlumsky/artery-font-format) file, a plain image file, a CSV sheet or a structured JSON file.

![Atlas example](https://user-images.githubusercontent.com/18639794/76163889-811f2e80-614a-11ea-9b28-1eed54dbb899.png)

A font atlas is typically stored in texture memory and used to draw text in real-time rendering contexts such as video games.

- See what's new in the [changelog](CHANGELOG.md).

## Atlas types

The atlas generator can generate the following six types of atlases.

| |Hard mask|Soft mask|SDF|PSDF|MSDF|MTSDF|
|-|-|-|-|-|-|-|
| |![Hard mask](https://user-images.githubusercontent.com/18639794/76163903-9eec9380-614a-11ea-92cb-d49485bbad31.png)|![Soft mask](https://user-images.githubusercontent.com/18639794/76163904-a1e78400-614a-11ea-912a-b220fed081cb.png)|![SDF](https://user-images.githubusercontent.com/18639794/76163905-a4e27480-614a-11ea-93eb-c80819a44e6e.png)|![PSDF](https://user-images.githubusercontent.com/18639794/76163907-a6ac3800-614a-11ea-8d97-dafc1db6711d.png)|![MSDF](https://user-images.githubusercontent.com/18639794/76163909-a9a72880-614a-11ea-9726-e825ee0dde94.png)|![MTSDF](https://user-images.githubusercontent.com/18639794/76163910-ac098280-614a-11ea-8b6b-811d864cd584.png)|
|Channels:|1 (1-bit)|1|1|1|3|4|
|Anti-aliasing:|-|Yes|Yes|Yes|Yes|Yes|
|Scalability:|-|-|Yes|Yes|Yes|Yes|
|Sharp corners:|-|-|-|-|Yes|Yes|
|Soft effects:|-|-|Yes|-|-|Yes|
|Hard effects:|-|-|-|Yes|Yes|Yes|

Notes:
- *Sharp corners* refers to preservation of corner sharpness when upscaled.
- *Soft effects* refers to the support of effects that use true distance, such as glows, rounded borders, or simplified shadows.
- *Hard effects* refers to the support of effects that use pseudo-distance, such as mitered borders or thickness adjustment.

## Getting started

This project can be used either as a library or as a standalone console program.
Examples of how to use it as a library are available at the [bottom of the page](#library-usage-examples).
To start using the program immediately, there is a Windows binary available for download in the ["Releases" section](https://github.com/Chlumsky/msdf-atlas-gen/releases).
To build the project from source, you may use the included [CMake script](CMakeLists.txt).
In its default configuration, it requires [vcpkg](https://vcpkg.io/) as the provider for third-party library dependencies.
If you set the environment variable `VCPKG_ROOT` to the vcpkg directory, the CMake configuration will take care of fetching all required packages from vcpkg.

## Command line arguments

Use the following command line arguments for the standalone version of the atlas generator.

### Input

- `-font <fontfile.ttf/otf>` (required) &ndash; sets the input font file.
  - Alternatively, use `-varfont <fontfile.ttf/otf?var0=value0&var1=value1>` to configure a variable font.
- `-charset <charset.txt>` &ndash; sets the character set. The ASCII charset will be used if not specified. See [the syntax specification](#character-set-specification-syntax) of `charset.txt`.
- `-glyphset <glyphset.txt>` &ndash; sets the set of input glyphs using their indices within the font file. See [the syntax specification](#glyph-set-specification).
- `-fontscale <scale>` &ndash; applies a scaling transformation to the font's glyphs. Mainly to be used to generate multiple sizes in a single atlas, otherwise use [`-size`](#glyph-configuration).
- `-fontname <name>` &ndash; sets a name for the font that will be stored in certain output files as metadata.
- `-and` &ndash; separates multiple inputs to be combined into a single atlas.

### Bitmap atlas type

`-type <type>` &ndash; see [Atlas types](#atlas-types)

`<type>` can be one of:

- `hardmask` &ndash; a non-anti-aliased binary image
- `softmask` &ndash; an anti-aliased image
- `sdf` &ndash; a true signed distance field (SDF)
- `psdf` &ndash; a pseudo-distance field
- `msdf` (default) &ndash; a multi-channel signed distance field (MSDF)
- `mtsdf` &ndash; a combination of MSDF and true SDF in the alpha channel

### Atlas image format

`-format <format>`

`<format>` can be one of:

- `png` &ndash; a compressed PNG image
- `bmp` &ndash; an uncompressed BMP image
- `tiff` &ndash; an uncompressed floating-point TIFF image
- `text` &ndash; a sequence of pixel values in plain text
- `textfloat` &ndash; a sequence of floating-point pixel values in plain text
- `bin` &ndash; a sequence of pixel values encoded as raw bytes of data
- `binfloat` &ndash; a sequence of pixel values encoded as raw 32-bit floating-point values

### Atlas dimensions

`-dimensions <width> <height>` &ndash; sets fixed atlas dimensions

Alternativelly, the minimum possible dimensions may be selected automatically if a dimensions constraint is set instead:

- `-pots` &ndash; a power-of-two square
- `-potr` &ndash; a power-of-two square or rectangle (2:1)
- `-square` &ndash; any square dimensions
- `-square2` &ndash; square with even side length
- `-square4` (default) &ndash; square with side length divisible by four

### Outputs

Any non-empty subset of the following may be specified:

- `-imageout <filename.*>` &ndash; saves the atlas bitmap as a plain image file. Format matches `-format`
- `-json <filename.json>` &ndash; writes the atlas's layout data as well as other metrics into a structured JSON file
- `-csv <filename.csv>` &ndash; writes the glyph layout data into a simple CSV file
- `-arfont <filename.arfont>` &ndash; saves the atlas and its layout data as an [Artery Font](https://github.com/Chlumsky/artery-font-format) file
- `-shadronpreview <filename.shadron> <sample text>` &ndash; generates a [Shadron script](https://www.arteryengine.com/shadron/) that uses the generated atlas to draw a sample text as a preview

### Glyph configuration

- `-size <EM size>` &ndash; sets the size of the glyphs in the atlas in pixels per EM
- `-minsize <EM size>` &ndash; sets the minimum size. The largest possible size that fits the same atlas dimensions will be used
- `-emrange <EM range>` &ndash; sets the distance field range in EM's
- `-pxrange <pixel range>` (default = 2) &ndash; sets the distance field range in output pixels

### Distance field generator settings

- `-angle <angle>` &ndash; sets the minimum angle between adjacent edges to be considered a corner. Append D for degrees (`msdf` / `mtsdf` only)
- `-coloringstrategy <simple / inktrap / distance>` &ndash; selects the edge coloring heuristic (`msdf` / `mtsdf` only)
- `-errorcorrection <mode>` &ndash; selects the error correction algorithm. Use `help` as mode for more information (`msdf` / `mtsdf` only)
- `-miterlimit <value>` &ndash; sets the miter limit that limits the extension of each glyph's bounding box due to very sharp corners (`psdf` / `msdf` / `mtsdf` only)
- `-overlap` &ndash; switches to distance field generator with support for overlapping contours
- `-nopreprocess` &ndash; disables path preprocessing which resolves self-intersections and overlapping contours
- `-scanline` &ndash; performs an additional scanline pass to fix the signs of the distances
- `-seed <N>` &ndash; sets the initial seed for the edge coloring heuristic
- `-threads <N>` &ndash; sets the number of threads for the parallel computation (0 = auto)

Use `-help` for an exhaustive list of options.

## Character set specification syntax

The character set file is a text file with UTF-8 or ASCII encoding.
The characters can be denoted in the following ways:

- Single character: `'A'` (UTF-8 encoded), `65` (decimal Unicode), `0x41` (hexadecimal Unicode)
- Range of characters: `['A', 'Z']`, `[65, 90]`, `[0x41, 0x5a]`
- String of characters: `"ABCDEFGHIJKLMNOPQRSTUVWXYZ"` (UTF-8 encoded)

The entries should be separated by commas or whitespace.
In between quotation marks, backslash is used as the escape character (e.g. `'\''`, `'\\'`, `"!\"#"`).
The order in which characters appear is not taken into consideration.

Additionally, the include directive can be used to include other charset files and combine character sets in a hierarchical way.
It must be written on a separate line:

`@include "base-charset.txt"`

### Glyph set specification

The syntax of the glyph set specification is mostly the same as that of a character set, but only numeric values (decimal and hexadecimal) are allowed.

## Library usage examples

Here are commented snippets of code that demonstrate how the project can be used as a library.

### Generating whole atlas at once

```c++
#include <msdf-atlas-gen/msdf-atlas-gen.h>

using namespace msdf_atlas;

bool generateAtlas(const char *fontFilename) {
    bool success = false;
    // Initialize instance of FreeType library
    if (msdfgen::FreetypeHandle *ft = msdfgen::initializeFreetype()) {
        // Load font file
        if (msdfgen::FontHandle *font = msdfgen::loadFont(ft, fontFilename)) {
            // Storage for glyph geometry and their coordinates in the atlas
            std::vector<GlyphGeometry> glyphs;
            // FontGeometry is a helper class that loads a set of glyphs from a single font.
            // It can also be used to get additional font metrics, kerning information, etc.
            FontGeometry fontGeometry(&glyphs);
            // Load a set of character glyphs:
            // The second argument can be ignored unless you mix different font sizes in one atlas.
            // In the last argument, you can specify a charset other than ASCII.
            // To load specific glyph indices, use loadGlyphs instead.
            fontGeometry.loadCharset(font, 1.0, Charset::ASCII);
            // Apply MSDF edge coloring. See edge-coloring.h for other coloring strategies.
            const double maxCornerAngle = 3.0;
            for (GlyphGeometry &glyph : glyphs)
                glyph.edgeColoring(&msdfgen::edgeColoringInkTrap, maxCornerAngle, 0);
            // TightAtlasPacker class computes the layout of the atlas.
            TightAtlasPacker packer;
            // Set atlas parameters:
            // setDimensions or setDimensionsConstraint to find the best value
            packer.setDimensionsConstraint(TightAtlasPacker::DimensionsConstraint::SQUARE);
            // setScale for a fixed size or setMinimumScale to use the largest that fits
            packer.setMinimumScale(24.0);
            // setPixelRange or setUnitRange
            packer.setPixelRange(2.0);
            packer.setMiterLimit(1.0);
            // Compute atlas layout - pack glyphs
            packer.pack(glyphs.data(), glyphs.size());
            // Get final atlas dimensions
            int width = 0, height = 0;
            packer.getDimensions(width, height);
            // The ImmediateAtlasGenerator class facilitates the generation of the atlas bitmap.
            ImmediateAtlasGenerator<
                float, // pixel type of buffer for individual glyphs depends on generator function
                3, // number of atlas color channels
                &msdfGenerator, // function to generate bitmaps for individual glyphs
                BitmapAtlasStorage<byte, 3> // class that stores the atlas bitmap
                // For example, a custom atlas storage class that stores it in VRAM can be used.
            > generator(width, height);
            // GeneratorAttributes can be modified to change the generator's default settings.
            GeneratorAttributes attributes;
            generator.setAttributes(attributes);
            generator.setThreadCount(4);
            // Generate atlas bitmap
            generator.generate(glyphs.data(), glyphs.size());
            // The atlas bitmap can now be retrieved via atlasStorage as a BitmapConstRef.
            // The glyphs array (or fontGeometry) contains positioning data for typesetting text.
            success = myProject::submitAtlasBitmapAndLayout(generator.atlasStorage(), glyphs);
            // Cleanup
            msdfgen::destroyFont(font);
        }
        msdfgen::deinitializeFreetype(ft);
    }
    return success;
}
```

### Dynamic atlas

The `DynamicAtlas` class allows you to add glyphs to the atlas "on-the-fly" as they are needed. In this example, the `ImmediateAtlasGenerator` is used as the underlying atlas generator, which however isn't ideal for this purpose because it may launch new threads every time. In practice, you would typically define your own atlas generator class that properly handles your specific performance and synchronization requirements.

Acquiring the `GlyphGeometry` objects can be adapted from the previous example.

```c++
#include <msdf-atlas-gen/msdf-atlas-gen.h>

using namespace msdf_atlas;

using MyDynamicAtlas = DynamicAtlas<ImmediateAtlasGenerator<float, 3, &msdfGenerator, BitmapAtlasStorage<byte, 3>>>;

const double pixelRange = 2.0;
const double glyphScale = 32.0;
const double miterLimit = 1.0;
const double maxCornerAngle = 3.0;

MyDynamicAtlas atlas;

void addGlyphsToAtlas(GlyphGeometry *glyphs, int count) {
    for (int i = 0; i < count; ++i) {
        // Apply MSDF edge coloring. See edge-coloring.h for other coloring strategies.
        glyphs[i].edgeColoring(&msdfgen::edgeColoringInkTrap, maxCornerAngle, 0);
        // Finalize glyph box size based on the parameters
        glyphs[i].wrapBox(glyphScale, pixelRange/glyphScale, miterLimit);
    }
    // Add glyphs to atlas - invokes the underlying atlas generator
    // Adding multiple glyphs at once may improve packing efficiency.
    MyDynamicAtlas::ChangeFlags change = atlas.add(glyphs, count);
    if (change&MyDynamicAtlas::RESIZED) {
        // Atlas has been enlarged - can be handled here or directly in custom generator class
    }
    // Glyph positioning data is now stored in glyphs.
}
```

The atlas storage (and its bitmap) can be accessed as `dynamicAtlas.atlasGenerator().atlasStorage()`.
