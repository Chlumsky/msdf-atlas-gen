
/*
* MULTI-CHANNEL SIGNED DISTANCE FIELD ATLAS GENERATOR v1.1 (2020-10-18) - standalone console program
* --------------------------------------------------------------------------------------------------
* A utility by Viktor Chlumsky, (c) 2020
*
*/

#ifdef MSDF_ATLAS_STANDALONE

#define _USE_MATH_DEFINES
#include <cstdio>
#include <cmath>
#include <cassert>
#include <algorithm>
#include <thread>

#include "msdf-atlas-gen.h"

using namespace msdf_atlas;

#define DEFAULT_ANGLE_THRESHOLD 3.0
#define DEFAULT_MITER_LIMIT 1.0
#define DEFAULT_EM_SIZE 32.0
#define DEFAULT_PIXEL_RANGE 2.0
#define SDF_ERROR_ESTIMATE_PRECISION 19
#define GLYPH_FILL_RULE msdfgen::FILL_NONZERO
#define MCG_MULTIPLIER 6364136223846793005ull

#ifdef MSDFGEN_USE_SKIA
    #define TITLE_SUFFIX    " & Skia"
    #define EXTRA_UNDERLINE "-------"
#else
    #define TITLE_SUFFIX
    #define EXTRA_UNDERLINE
#endif

static const char * const helpText = R"(
MSDF Atlas Generator by Viktor Chlumsky v)" MSDF_ATLAS_VERSION R"( (with MSDFGEN v)" MSDFGEN_VERSION TITLE_SUFFIX R"()
----------------------------------------------------------------)" EXTRA_UNDERLINE R"(

INPUT SPECIFICATION
  -font <filename.ttf/otf>
      Specifies the input TrueType / OpenType font file. This is required.
  -charset <filename>
      Specifies the input character set. Refer to the documentation for format of charset specification. Defaults to ASCII.
  -glyphset <filename>
      Specifies the set of input glyphs as glyph indices within the font file.

ATLAS CONFIGURATION
  -type <hardmask / softmask / sdf / psdf / msdf / mtsdf>
      Selects the type of atlas to be generated.
  -format <png / bmp / tiff / text / textfloat / bin / binfloat / binfloatbe>
      Selects the format for the atlas image output. Some image formats may be incompatible with embedded output formats.
  -dimensions <width> <height>
      Sets the atlas to have fixed dimensions (width x height).
  -pots / -potr / -square / -square2 / -square4
      Picks the minimum atlas dimensions that fit all glyphs and satisfy the selected constraint:
      power of two square / ... rectangle / any square / square with side divisible by 2 / ... 4

OUTPUT SPECIFICATION - one or more can be specified
  -imageout <filename.*>
      Saves the atlas as an image file with the specified format. Layout data must be stored separately.
  -json <filename.json>
      Writes the atlas's layout data, as well as other metrics into a structured JSON file.
  -csv <filename.csv>
      Writes the layout data of the glyphs into a simple CSV file.
  -arfont <filename.arfont>
      Stores the atlas and its layout data as an Artery Font file. Supported formats: png, bin, binfloat.
  -shadronpreview <filename.shadron> <sample text>
      Generates a Shadron script that uses the generated atlas to draw a sample text as a preview.

GLYPH CONFIGURATION
  -size <EM size>
      Specifies the size of the glyphs in the atlas bitmap in pixels per EM.
  -minsize <EM size>
      Specifies the minimum size. The largest possible size that fits the same atlas dimensions will be used.
  -emrange <EM range>
      Specifies the SDF distance range in EM's.
  -pxrange <pixel range>
      Specifies the SDF distance range in output pixels. The default value is 2.

DISTANCE FIELD GENERATOR SETTINGS
  -angle <angle>
      Specifies the minimum angle between adjacent edges to be considered a corner. Append D for degrees. (msdf / mtsdf only)
  -errorcorrection <threshold>
      Changes the threshold used to detect and correct potential artifacts. 0 disables error correction. (msdf / mtsdf only)
  -miterlimit <value>
      Sets the miter limit that limits the extension of each glyph's bounding box due to very sharp corners. (psdf / msdf / mtsdf only))"
#ifdef MSDFGEN_USE_SKIA
R"(
  -overlap
      Switches to distance field generator with support for overlapping contours.
  -nopreprocess
      Disables path preprocessing which resolves self-intersections and overlapping contours.
  -scanline
      Performs an additional scanline pass to fix the signs of the distances.)"
#else
R"(
  -nooverlap
      Disables resolution of overlapping contours.
  -noscanline
      Disables the scanline pass, which corrects the distance field's signs according to the non-zero fill rule.)"
#endif
R"(
  -seed <N>
      Sets the initial seed for the edge coloring heuristic.
  -threads <N>
      Sets the number of threads for the parallel computation. (0 = auto)
)";

static char toupper(char c) {
    return c >= 'a' && c <= 'z' ? c-'a'+'A' : c;
}

static bool parseUnsigned(unsigned &value, const char *arg) {
    static char c;
    return sscanf(arg, "%u%c", &value, &c) == 1;
}

static bool parseUnsignedLL(unsigned long long &value, const char *arg) {
    static char c;
    return sscanf(arg, "%llu%c", &value, &c) == 1;
}

static bool parseDouble(double &value, const char *arg) {
    static char c;
    return sscanf(arg, "%lf%c", &value, &c) == 1;
}

static bool parseAngle(double &value, const char *arg) {
    char c1, c2;
    int result = sscanf(arg, "%lf%c%c", &value, &c1, &c2);
    if (result == 1)
        return true;
    if (result == 2 && (c1 == 'd' || c1 == 'D')) {
        value *= M_PI/180;
        return true;
    }
    return false;
}

static bool cmpExtension(const char *path, const char *ext) {
    for (const char *a = path+strlen(path)-1, *b = ext+strlen(ext)-1; b >= ext; --a, --b)
        if (a < path || toupper(*a) != toupper(*b))
            return false;
    return true;
}

static void loadGlyphsByIndex(std::vector<GlyphGeometry> &glyphs, msdfgen::FontHandle *font, const Charset &charset, bool preprocessGeometry) {
    glyphs.clear();
    glyphs.reserve(charset.size());
    for (unicode_t cp : charset) {
        GlyphGeometry glyph;
        if (glyph.load(font, msdfgen::GlyphIndex(cp), preprocessGeometry))
            glyphs.push_back((GlyphGeometry &&) glyph);
        else
            printf("Glyph # 0x%X missing\n", cp);
    }
}

static void loadGlyphsByUnicode(std::vector<GlyphGeometry> &glyphs, msdfgen::FontHandle *font, const Charset &charset, bool preprocessGeometry) {
    glyphs.clear();
    glyphs.reserve(charset.size());
    for (unicode_t cp : charset) {
        GlyphGeometry glyph;
        if (glyph.load(font, cp, preprocessGeometry))
            glyphs.push_back((GlyphGeometry &&) glyph);
        else
            printf("Glyph for codepoint 0x%X missing\n", cp);
    }
}

struct Configuration {
    GlyphIdentifierType glyphIdentifierType;
    ImageType imageType;
    ImageFormat imageFormat;
    int width, height;
    double emSize;
    double pxRange;
    double angleThreshold;
    double miterLimit;
    unsigned long long coloringSeed;
    GeneratorAttributes generatorAttributes;
    bool preprocessGeometry;
    int threadCount;
    const char *arteryFontFilename;
    const char *imageFilename;
    const char *jsonFilename;
    const char *csvFilename;
    const char *shadronPreviewFilename;
    const char *shadronPreviewText;
};

template <typename T, typename S, int N, GeneratorFunction<S, N> GEN_FN>
static bool makeAtlas(const std::vector<GlyphGeometry> &glyphs, msdfgen::FontHandle *font, const Configuration &config) {
    ImmediateAtlasGenerator<S, N, GEN_FN, BitmapAtlasStorage<T, N> > generator(config.width, config.height);
    generator.setAttributes(config.generatorAttributes);
    generator.setThreadCount(config.threadCount);
    generator.generate(glyphs.data(), glyphs.size());
    msdfgen::BitmapConstRef<T, N> bitmap = (msdfgen::BitmapConstRef<T, N>) generator.atlasStorage();

    bool success = true;

    if (config.imageFilename) {
        if (saveImage(bitmap, config.imageFormat, config.imageFilename))
            puts("Atlas image file saved.");
        else {
            success = false;
            puts("Failed to save the atlas as an image file.");
        }
    }

    if (config.arteryFontFilename) {
        ArteryFontExportProperties arfontProps;
        arfontProps.glyphIdentifierType = config.glyphIdentifierType;
        arfontProps.fontSize = config.emSize;
        arfontProps.pxRange = config.pxRange;
        arfontProps.imageType = config.imageType;
        arfontProps.imageFormat = config.imageFormat;
        if (exportArteryFont<float>(font, glyphs.data(), glyphs.size(), bitmap, config.arteryFontFilename, arfontProps))
            puts("Artery Font file generated.");
        else {
            success = false;
            puts("Failed to generate Artery Font file.");
        }
    }

    return success;
}

int main(int argc, const char * const *argv) {
    #define ABORT(msg) { puts(msg); return 1; }

    int result = 0;
    Configuration config = { };
    const char *fontFilename = nullptr;
    const char *charsetFilename = nullptr;
    config.glyphIdentifierType = GlyphIdentifierType::UNICODE_CODEPOINT;
    config.imageType = ImageType::MSDF;
    config.imageFormat = ImageFormat::UNSPECIFIED;
    const char *imageFormatName = nullptr;
    int fixedWidth = -1, fixedHeight = -1;
    config.preprocessGeometry = (
        #ifdef MSDFGEN_USE_SKIA
            true
        #else
            false
        #endif
    );
    config.generatorAttributes.overlapSupport = !config.preprocessGeometry;
    config.generatorAttributes.scanlinePass = !config.preprocessGeometry;
    config.generatorAttributes.errorCorrectionThreshold = MSDFGEN_DEFAULT_ERROR_CORRECTION_THRESHOLD;
    double minEmSize = 0;
    enum {
        /// Range specified in EMs
        RANGE_EM,
        /// Range specified in output pixels
        RANGE_PIXEL,
    } rangeMode = RANGE_PIXEL;
    double rangeValue = 0;
    TightAtlasPacker::DimensionsConstraint atlasSizeConstraint = TightAtlasPacker::DimensionsConstraint::MULTIPLE_OF_FOUR_SQUARE;
    config.angleThreshold = DEFAULT_ANGLE_THRESHOLD;
    config.miterLimit = DEFAULT_MITER_LIMIT;
    config.threadCount = 0;

    // Parse command line
    int argPos = 1;
    bool suggestHelp = false;
    while (argPos < argc) {
        const char *arg = argv[argPos];
        #define ARG_CASE(s, p) if (!strcmp(arg, s) && argPos+(p) < argc)

        ARG_CASE("-type", 1) {
            arg = argv[++argPos];
            if (!strcmp(arg, "hardmask"))
                config.imageType = ImageType::HARD_MASK;
            else if (!strcmp(arg, "softmask"))
                config.imageType = ImageType::SOFT_MASK;
            else if (!strcmp(arg, "sdf"))
                config.imageType = ImageType::SDF;
            else if (!strcmp(arg, "psdf"))
                config.imageType = ImageType::PSDF;
            else if (!strcmp(arg, "msdf"))
                config.imageType = ImageType::MSDF;
            else if (!strcmp(arg, "mtsdf"))
                config.imageType = ImageType::MTSDF;
            else
                ABORT("Invalid atlas type. Valid types are: hardmask, softmask, sdf, psdf, msdf, mtsdf");
            ++argPos;
            continue;
        }
        ARG_CASE("-format", 1) {
            arg = argv[++argPos];
            if (!strcmp(arg, "png"))
                config.imageFormat = ImageFormat::PNG;
            else if (!strcmp(arg, "bmp"))
                config.imageFormat = ImageFormat::BMP;
            else if (!strcmp(arg, "tiff"))
                config.imageFormat = ImageFormat::TIFF;
            else if (!strcmp(arg, "text"))
                config.imageFormat = ImageFormat::TEXT;
            else if (!strcmp(arg, "textfloat"))
                config.imageFormat = ImageFormat::TEXT_FLOAT;
            else if (!strcmp(arg, "bin"))
                config.imageFormat = ImageFormat::BINARY;
            else if (!strcmp(arg, "binfloat"))
                config.imageFormat = ImageFormat::BINARY_FLOAT;
            else if (!strcmp(arg, "binfloatbe"))
                config.imageFormat = ImageFormat::BINARY_FLOAT_BE;
            else
                ABORT("Invalid image format. Valid formats are: png, bmp, tiff, text, textfloat, bin, binfloat");
            imageFormatName = arg;
            ++argPos;
            continue;
        }
        ARG_CASE("-font", 1) {
            fontFilename = argv[++argPos];
            ++argPos;
            continue;
        }
        ARG_CASE("-charset", 1) {
            charsetFilename = argv[++argPos];
            config.glyphIdentifierType = GlyphIdentifierType::UNICODE_CODEPOINT;
            ++argPos;
            continue;
        }
        ARG_CASE("-glyphset", 1) {
            charsetFilename = argv[++argPos];
            config.glyphIdentifierType = GlyphIdentifierType::GLYPH_INDEX;
            ++argPos;
            continue;
        }
        ARG_CASE("-arfont", 1) {
            config.arteryFontFilename = argv[++argPos];
            ++argPos;
            continue;
        }
        ARG_CASE("-imageout", 1) {
            config.imageFilename = argv[++argPos];
            ++argPos;
            continue;
        }
        ARG_CASE("-json", 1) {
            config.jsonFilename = argv[++argPos];
            ++argPos;
            continue;
        }
        ARG_CASE("-csv", 1) {
            config.csvFilename = argv[++argPos];
            ++argPos;
            continue;
        }
        ARG_CASE("-shadronpreview", 2) {
            config.shadronPreviewFilename = argv[++argPos];
            config.shadronPreviewText = argv[++argPos];
            ++argPos;
            continue;
        }
        ARG_CASE("-dimensions", 2) {
            unsigned w, h;
            if (!(parseUnsigned(w, argv[argPos+1]) && parseUnsigned(h, argv[argPos+2]) && w && h))
                ABORT("Invalid atlas dimensions. Use -dimensions <width> <height> with two positive integers.");
            fixedWidth = w, fixedHeight = h;
            argPos += 3;
            continue;
        }
        ARG_CASE("-pots", 0) {
            atlasSizeConstraint = TightAtlasPacker::DimensionsConstraint::POWER_OF_TWO_SQUARE;
            fixedWidth = -1, fixedHeight = -1;
            ++argPos;
            continue;
        }
        ARG_CASE("-potr", 0) {
            atlasSizeConstraint = TightAtlasPacker::DimensionsConstraint::POWER_OF_TWO_RECTANGLE;
            fixedWidth = -1, fixedHeight = -1;
            ++argPos;
            continue;
        }
        ARG_CASE("-square", 0) {
            atlasSizeConstraint = TightAtlasPacker::DimensionsConstraint::SQUARE;
            fixedWidth = -1, fixedHeight = -1;
            ++argPos;
            continue;
        }
        ARG_CASE("-square2", 0) {
            atlasSizeConstraint = TightAtlasPacker::DimensionsConstraint::EVEN_SQUARE;
            fixedWidth = -1, fixedHeight = -1;
            ++argPos;
            continue;
        }
        ARG_CASE("-square4", 0) {
            atlasSizeConstraint = TightAtlasPacker::DimensionsConstraint::MULTIPLE_OF_FOUR_SQUARE;
            fixedWidth = -1, fixedHeight = -1;
            ++argPos;
            continue;
        }
        ARG_CASE("-size", 1) {
            double s;
            if (!(parseDouble(s, argv[++argPos]) && s > 0))
                ABORT("Invalid EM size argument. Use -size <EM size> with a positive real number.");
            config.emSize = s;
            ++argPos;
            continue;
        }
        ARG_CASE("-minsize", 1) {
            double s;
            if (!(parseDouble(s, argv[++argPos]) && s > 0))
                ABORT("Invalid minimum EM size argument. Use -minsize <EM size> with a positive real number.");
            minEmSize = s;
            ++argPos;
            continue;
        }
        ARG_CASE("-emrange", 1) {
            double r;
            if (!(parseDouble(r, argv[++argPos]) && r >= 0))
                ABORT("Invalid range argument. Use -emrange <EM range> with a positive real number.");
            rangeMode = RANGE_EM;
            rangeValue = r;
            ++argPos;
            continue;
        }
        ARG_CASE("-pxrange", 1) {
            double r;
            if (!(parseDouble(r, argv[++argPos]) && r >= 0))
                ABORT("Invalid range argument. Use -pxrange <pixel range> with a positive real number.");
            rangeMode = RANGE_PIXEL;
            rangeValue = r;
            ++argPos;
            continue;
        }
        ARG_CASE("-angle", 1) {
            double at;
            if (!parseAngle(at, argv[argPos+1]))
                ABORT("Invalid angle threshold. Use -angle <min angle> with a positive real number less than PI or a value in degrees followed by 'd' below 180d.");
            config.angleThreshold = at;
            argPos += 2;
            continue;
        }
        ARG_CASE("-errorcorrection", 1) {
            double ect;
            if (!parseDouble(ect, argv[argPos+1]) || ect < 0)
                ABORT("Invalid error correction threshold. Use -errorcorrection <threshold> with a real number larger or equal to 1.");
            config.generatorAttributes.errorCorrectionThreshold = ect;
            argPos += 2;
            continue;
        }
        ARG_CASE("-miterlimit", 1) {
            double m;
            if (!(parseDouble(m, argv[++argPos]) && m >= 0))
                ABORT("Invalid miter limit argument. Use -miterlimit <limit> with a positive real number.");
            config.miterLimit = m;
            ++argPos;
            continue;
        }
        ARG_CASE("-nopreprocess", 0) {
            config.preprocessGeometry = false;
            argPos += 1;
            continue;
        }
        ARG_CASE("-preprocess", 0) {
            config.preprocessGeometry = true;
            argPos += 1;
            continue;
        }
        ARG_CASE("-nooverlap", 0) {
            config.generatorAttributes.overlapSupport = false;
            argPos += 1;
            continue;
        }
        ARG_CASE("-overlap", 0) {
            config.generatorAttributes.overlapSupport = true;
            argPos += 1;
            continue;
        }
        ARG_CASE("-noscanline", 0) {
            config.generatorAttributes.scanlinePass = false;
            argPos += 1;
            continue;
        }
        ARG_CASE("-scanline", 0) {
            config.generatorAttributes.scanlinePass = true;
            argPos += 1;
            continue;
        }
        ARG_CASE("-seed", 1) {
            if (!parseUnsignedLL(config.coloringSeed, argv[argPos+1]))
                ABORT("Invalid seed. Use -seed <N> with N being a non-negative integer.");
            argPos += 2;
            continue;
        }
        ARG_CASE("-threads", 1) {
            unsigned tc;
            if (!parseUnsigned(tc, argv[argPos+1]) || (int) tc < 0)
                ABORT("Invalid thread count. Use -threads <N> with N being a non-negative integer.");
            config.threadCount = (int) tc;
            argPos += 2;
            continue;
        }
        ARG_CASE("-help", 0) {
            puts(helpText);
            return 0;
        }
        printf("Unknown setting or insufficient parameters: %s\n", arg);
        suggestHelp = true;
        ++argPos;
    }
    if (suggestHelp)
        printf("Use -help for more information.\n");

    // Nothing to do?
    if (argc == 1) {
        printf(
            "Usage: msdf-atlas-gen"
            #ifdef _WIN32
                ".exe"
            #endif
            " -font <filename.ttf/otf> -charset <charset> <output specification> <options>\n"
            "Use -help for more information.\n"
        );
        return 0;
    }
    if (!fontFilename)
        ABORT("No font specified.");
    if (!(config.arteryFontFilename || config.imageFilename || config.jsonFilename || config.csvFilename || config.shadronPreviewFilename)) {
        puts("No output specified.");
        return 0;
    }
    bool layoutOnly = !(config.arteryFontFilename || config.imageFilename);

    // Fix up configuration based on related values
    if (!(config.imageType == ImageType::PSDF || config.imageType == ImageType::MSDF || config.imageType == ImageType::MTSDF))
        config.miterLimit = 0;
    if (config.emSize > minEmSize)
        minEmSize = config.emSize;
    if (!(fixedWidth > 0 && fixedHeight > 0) && !(minEmSize > 0)) {
        puts("Neither atlas size nor glyph size selected, using default...");
        minEmSize = DEFAULT_EM_SIZE;
    }
    if (!(config.imageType == ImageType::SDF || config.imageType == ImageType::PSDF || config.imageType == ImageType::MSDF || config.imageType == ImageType::MTSDF)) {
        rangeMode = RANGE_PIXEL;
        rangeValue = (double) (config.imageType == ImageType::SOFT_MASK);
    } else if (rangeValue <= 0) {
        rangeMode = RANGE_PIXEL;
        rangeValue = DEFAULT_PIXEL_RANGE;
    }
    if (config.threadCount <= 0)
        config.threadCount = std::max((int) std::thread::hardware_concurrency(), 1);

    // Finalize image format
    ImageFormat imageExtension = ImageFormat::UNSPECIFIED;
    if (config.imageFilename) {
        if (cmpExtension(config.imageFilename, ".png")) imageExtension = ImageFormat::PNG;
        else if (cmpExtension(config.imageFilename, ".bmp")) imageExtension = ImageFormat::BMP;
        else if (cmpExtension(config.imageFilename, ".tif") || cmpExtension(config.imageFilename, ".tiff")) imageExtension = ImageFormat::TIFF;
        else if (cmpExtension(config.imageFilename, ".txt")) imageExtension = ImageFormat::TEXT;
        else if (cmpExtension(config.imageFilename, ".bin")) imageExtension = ImageFormat::BINARY;
    }
    if (config.imageFormat == ImageFormat::UNSPECIFIED) {
        config.imageFormat = ImageFormat::PNG;
        imageFormatName = "png";
        // If image format is not specified and -imageout is the only image output, infer format from its extension
        if (imageExtension != ImageFormat::UNSPECIFIED && !config.arteryFontFilename)
            config.imageFormat = imageExtension;
    }
    if (config.imageType == ImageType::MTSDF && config.imageFormat == ImageFormat::BMP)
        ABORT("Atlas type not compatible with image format. MTSDF requires a format with alpha channel.");
    if (config.arteryFontFilename && !(config.imageFormat == ImageFormat::PNG || config.imageFormat == ImageFormat::BINARY || config.imageFormat == ImageFormat::BINARY_FLOAT)) {
        config.arteryFontFilename = nullptr;
        result = 1;
        puts("Error: Unable to create an Artery Font file with the specified image format!");
        // Recheck whether there is anything else to do
        if (!(config.arteryFontFilename || config.imageFilename || config.jsonFilename || config.csvFilename || config.shadronPreviewFilename))
            return result;
        layoutOnly = !(config.arteryFontFilename || config.imageFilename);
    }
    if (imageExtension != ImageFormat::UNSPECIFIED) {
        // Warn if image format mismatches -imageout extension
        bool mismatch = false;
        switch (config.imageFormat) {
            case ImageFormat::TEXT: case ImageFormat::TEXT_FLOAT:
                mismatch = imageExtension != ImageFormat::TEXT;
                break;
            case ImageFormat::BINARY: case ImageFormat::BINARY_FLOAT: case ImageFormat::BINARY_FLOAT_BE:
                mismatch = imageExtension != ImageFormat::BINARY;
                break;
            default:
                mismatch = imageExtension != config.imageFormat;
        }
        if (mismatch)
            printf("Warning: Output image file extension does not match the image's actual format (%s)!\n", imageFormatName);
    }
    imageFormatName = nullptr; // No longer consistent with imageFormat
    bool floatingPointFormat = (
        config.imageFormat == ImageFormat::TIFF ||
        config.imageFormat == ImageFormat::TEXT_FLOAT ||
        config.imageFormat == ImageFormat::BINARY_FLOAT ||
        config.imageFormat == ImageFormat::BINARY_FLOAT_BE
    );

    // Load font
    class FontHolder {
        msdfgen::FreetypeHandle *ft;
        msdfgen::FontHandle *font;
    public:
        explicit FontHolder(const char *fontFilename) : ft(nullptr), font(nullptr) {
            if ((ft = msdfgen::initializeFreetype()))
                font = msdfgen::loadFont(ft, fontFilename);
        }
        ~FontHolder() {
            if (ft) {
                if (font)
                    msdfgen::destroyFont(font);
                msdfgen::deinitializeFreetype(ft);
            }
        }
        operator msdfgen::FontHandle *() const {
            return font;
        }
    } font(fontFilename);
    if (!font)
        ABORT("Failed to load specified font file.");
    msdfgen::FontMetrics fontMetrics = { };
    msdfgen::getFontMetrics(fontMetrics, font);
    if (fontMetrics.emSize <= 0)
        fontMetrics.emSize = DEFAULT_EM_SIZE;

    // Load character set
    Charset charset;
    if (charsetFilename) {
        if (!charset.load(charsetFilename, config.glyphIdentifierType != GlyphIdentifierType::UNICODE_CODEPOINT))
            ABORT(config.glyphIdentifierType == GlyphIdentifierType::GLYPH_INDEX ? "Failed to load glyph set specification." : "Failed to load character set specification.");
    } else
        charset = Charset::ASCII;

    // Load glyphs
    std::vector<GlyphGeometry> glyphs;
    switch (config.glyphIdentifierType) {
        case GlyphIdentifierType::GLYPH_INDEX:
            loadGlyphsByIndex(glyphs, font, charset, config.preprocessGeometry);
            break;
        case GlyphIdentifierType::UNICODE_CODEPOINT:
            loadGlyphsByUnicode(glyphs, font, charset, config.preprocessGeometry);
            break;
    }
    if (glyphs.empty())
        ABORT("No glyphs loaded.");
    printf("Loaded geometry of %d out of %d %s.\n", (int) glyphs.size(), (int) charset.size(), config.glyphIdentifierType == GlyphIdentifierType::GLYPH_INDEX ? "glyphs" : "characters");

    // Determine final atlas dimensions, scale and range, pack glyphs
    {
        double unitRange = 0, pxRange = 0;
        switch (rangeMode) {
            case RANGE_EM:
                unitRange = rangeValue*fontMetrics.emSize;
                break;
            case RANGE_PIXEL:
                pxRange = rangeValue;
                break;
        }
        bool fixedDimensions = fixedWidth >= 0 && fixedHeight >= 0;
        bool fixedScale = config.emSize > 0;
        TightAtlasPacker atlasPacker;
        if (fixedDimensions)
            atlasPacker.setDimensions(fixedWidth, fixedHeight);
        else
            atlasPacker.setDimensionsConstraint(atlasSizeConstraint);
        atlasPacker.setPadding(config.imageType == ImageType::MSDF || config.imageType == ImageType::MTSDF ? 0 : -1);
        // TODO: In this case (if padding is -1), the border pixels of each glyph are black, but still computed. For floating-point output, this may play a role.
        if (fixedScale)
            atlasPacker.setScale(config.emSize/fontMetrics.emSize);
        else
            atlasPacker.setMinimumScale(minEmSize/fontMetrics.emSize);
        atlasPacker.setPixelRange(pxRange);
        atlasPacker.setUnitRange(unitRange);
        atlasPacker.setMiterLimit(config.miterLimit);
        if (int remaining = atlasPacker.pack(glyphs.data(), glyphs.size())) {
            if (remaining < 0) {
                ABORT("Failed to pack glyphs into atlas.");
            } else {
                printf("Error: Could not fit %d out of %d glyphs into the atlas.\n", remaining, (int) glyphs.size());
                return 1;
            }
        }
        atlasPacker.getDimensions(config.width, config.height);
        if (!(config.width > 0 && config.height > 0))
            ABORT("Unable to determine atlas size.");
        config.emSize = atlasPacker.getScale()*fontMetrics.emSize;
        config.pxRange = atlasPacker.getPixelRange();
        if (!fixedScale)
            printf("Glyph size: %.9g pixels/EM\n", config.emSize);
        if (!fixedDimensions)
            printf("Atlas dimensions: %d x %d\n", config.width, config.height);
    }

    // Generate atlas bitmap
    if (!layoutOnly) {

        // Edge coloring
        if (config.imageType == ImageType::MSDF || config.imageType == ImageType::MTSDF) {
            unsigned long long glyphSeed = config.coloringSeed;
            for (GlyphGeometry &glyph : glyphs) {
                glyphSeed *= MCG_MULTIPLIER;
                glyph.edgeColoring(config.angleThreshold, glyphSeed);
            }
        }

        bool success = false;
        switch (config.imageType) {
            case ImageType::HARD_MASK:
                if (floatingPointFormat)
                    success = makeAtlas<float, float, 1, scanlineGenerator>(glyphs, font, config);
                else
                    success = makeAtlas<byte, float, 1, scanlineGenerator>(glyphs, font, config);
                break;
            case ImageType::SOFT_MASK:
            case ImageType::SDF:
                if (floatingPointFormat)
                    success = makeAtlas<float, float, 1, sdfGenerator>(glyphs, font, config);
                else
                    success = makeAtlas<byte, float, 1, sdfGenerator>(glyphs, font, config);
                break;
            case ImageType::PSDF:
                if (floatingPointFormat)
                    success = makeAtlas<float, float, 1, psdfGenerator>(glyphs, font, config);
                else
                    success = makeAtlas<byte, float, 1, psdfGenerator>(glyphs, font, config);
                break;
            case ImageType::MSDF:
                if (floatingPointFormat)
                    success = makeAtlas<float, float, 3, msdfGenerator>(glyphs, font, config);
                else
                    success = makeAtlas<byte, float, 3, msdfGenerator>(glyphs, font, config);
                break;
            case ImageType::MTSDF:
                if (floatingPointFormat)
                    success = makeAtlas<float, float, 4, mtsdfGenerator>(glyphs, font, config);
                else
                    success = makeAtlas<byte, float, 4, mtsdfGenerator>(glyphs, font, config);
                break;
        }
        if (!success)
            result = 1;
    }

    if (config.csvFilename) {
        if (exportCSV(glyphs.data(), glyphs.size(), config.glyphIdentifierType, fontMetrics.emSize, config.csvFilename))
            puts("Glyph layout written into CSV file.");
        else {
            result = 1;
            puts("Failed to write CSV output file.");
        }
    }
    if (config.jsonFilename) {
        if (exportJSON(font, glyphs.data(), glyphs.size(), config.glyphIdentifierType, config.emSize, config.pxRange, config.width, config.height, config.imageType, config.jsonFilename))
            puts("Glyph layout and metadata written into JSON file.");
        else {
            result = 1;
            puts("Failed to write JSON output file.");
        }
    }

    if (config.shadronPreviewFilename && config.shadronPreviewText) {
        if (config.glyphIdentifierType == GlyphIdentifierType::UNICODE_CODEPOINT) {
            std::vector<unicode_t> previewText;
            utf8Decode(previewText, config.shadronPreviewText);
            previewText.push_back(0);
            if (generateShadronPreview(font, glyphs.data(), glyphs.size(), config.imageType, config.width, config.height, config.pxRange, previewText.data(), config.imageFilename, floatingPointFormat, config.shadronPreviewFilename))
                puts("Shadron preview script generated.");
            else {
                result = 1;
                puts("Failed to generate Shadron preview file.");
            }
        } else {
            result = 1;
            puts("Shadron preview not supported in -glyphset mode.");
        }
    }

    return result;
}

#endif
