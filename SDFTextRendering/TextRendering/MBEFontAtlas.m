//
//  MBEFontAtlas.m
//  TextRendering
//
//  Created by Warren Moore on 2/6/15.
//  Copyright (c) 2015 Metal By Example. All rights reserved.
//

#import "MBEFontAtlas.h"
@import CoreText;

#define MBE_GENERATE_DEBUG_ATLAS_IMAGE 1

// This is the size at which the font atlas will be generated, ideally a large power of two. Even though
// we later downscale the distance field, it's better to render it at as high a resolution as possible in
// order to capture all of the fine details.
static const size_t MBEFontAtlasSize = 4096;

static NSString *const MBEGlyphIndexKey = @"glyphIndex";
static NSString *const MBELeftTexCoordKey = @"leftTexCoord";
static NSString *const MBERightTexCoordKey = @"rightTexCoord";
static NSString *const MBETopTexCoordKey = @"topTexCoord";
static NSString *const MBEBottomTexCoordKey = @"bottomTexCoord";
static NSString *const MBEFontNameKey = @"fontName";
static NSString *const MBEFontSizeKey = @"fontSize";
static NSString *const MBEFontSpreadKey = @"spread";
static NSString *const MBETextureDataKey = @"textureData";
static NSString *const MBETextureWidthKey = @"textureWidth";
static NSString *const MBETextureHeightKey = @"textureHeight";
static NSString *const MBEGlyphDescriptorsKey = @"glyphDescriptors";

@implementation MBEGlyphDescriptor

- (instancetype)initWithCoder:(NSCoder *)aDecoder
{
    if ((self = [super init]))
    {
        _glyphIndex = [aDecoder decodeIntForKey:MBEGlyphIndexKey];
        _topLeftTexCoord.x = [aDecoder decodeFloatForKey:MBELeftTexCoordKey];
        _topLeftTexCoord.y = [aDecoder decodeFloatForKey:MBETopTexCoordKey];
        _bottomRightTexCoord.x = [aDecoder decodeFloatForKey:MBERightTexCoordKey];
        _bottomRightTexCoord.y = [aDecoder decodeFloatForKey:MBEBottomTexCoordKey];
    }

    return self;
}

- (void)encodeWithCoder:(NSCoder *)aCoder
{
    [aCoder encodeInt:self.glyphIndex forKey:MBEGlyphIndexKey];
    [aCoder encodeFloat:self.topLeftTexCoord.x forKey:MBELeftTexCoordKey];
    [aCoder encodeFloat:self.topLeftTexCoord.y forKey:MBETopTexCoordKey];
    [aCoder encodeFloat:self.bottomRightTexCoord.x forKey:MBERightTexCoordKey];
    [aCoder encodeFloat:self.bottomRightTexCoord.y forKey:MBEBottomTexCoordKey];
}

+ (BOOL)supportsSecureCoding
{
    return YES;
}

@end

@implementation MBEFontAtlas

- (instancetype)initWithFont:(UIFont *)font textureSize:(size_t)textureSize
{
    if ((self = [super init]))
    {
        _parentFont = font;
        _fontPointSize = font.pointSize;
        _spread = [self estimatedLineWidthForFont:font] * 0.5;
        _glyphDescriptors = [NSMutableArray array];
        _textureSize = textureSize;
        [self createTextureData];
    }

    return self;
}

- (instancetype)initWithCoder:(NSCoder *)aDecoder
{
    if ((self = [super init]))
    {
        NSString *fontName = [aDecoder decodeObjectForKey:MBEFontNameKey];
        CGFloat fontSize = [aDecoder decodeFloatForKey:MBEFontSizeKey];
        CGFloat spread = [aDecoder decodeFloatForKey:MBEFontSpreadKey];

        if (fontName.length == 0 || fontSize <= 0)
        {
            NSLog(@"Encountered invalid persisted font (invalid font name or size). Aborting...");
            return nil;
        }

        _parentFont = [UIFont fontWithName:fontName size:fontSize];
        _fontPointSize = fontSize;
        _spread = spread;
        _glyphDescriptors = [aDecoder decodeObjectForKey:MBEGlyphDescriptorsKey];

        if (_glyphDescriptors == nil)
        {
            NSLog(@"Encountered invalid persisted font (no glyph metrics). Aborting...");
            return nil;
        }

        size_t width = [aDecoder decodeIntForKey:MBETextureWidthKey];
        size_t height = [aDecoder decodeIntForKey:MBETextureHeightKey];

        if (width != height)
        {
            NSLog(@"Encountered invalid persisted font (non-square textures aren't supported). Aborting...");
            return nil;
        }

        _textureSize = width;

        _textureData = [aDecoder decodeObjectForKey:MBETextureDataKey];

        if (_textureData == nil)
        {
            NSLog(@"Encountered invalid persisted font (texture data is empty). Aborting...");
        }
    }

    return self;
}

- (void)encodeWithCoder:(NSCoder *)aCoder
{
    [aCoder encodeObject:self.parentFont.fontName forKey:MBEFontNameKey];
    [aCoder encodeFloat:self.fontPointSize forKey:MBEFontSizeKey];
    [aCoder encodeFloat:self.spread forKey:MBEFontSpreadKey];
    [aCoder encodeObject:self.textureData forKey:MBETextureDataKey];
    [aCoder encodeInt64:self.textureSize forKey:MBETextureWidthKey];
    [aCoder encodeInt64:self.textureSize forKey:MBETextureHeightKey];
    [aCoder encodeObject:self.glyphDescriptors forKey:MBEGlyphDescriptorsKey];
}

+ (BOOL)supportsSecureCoding
{
    return YES;
}

- (CGSize)estimatedGlyphSizeForFont:(UIFont *)font
{
    NSString *exemplarString = @"{ǺOJMQYZa@jmqyw";
    CGSize exemplarStringSize = [exemplarString sizeWithAttributes:@{ NSFontAttributeName : font }];
    CGFloat averageGlyphWidth = ceilf(exemplarStringSize.width / exemplarString.length);
    CGFloat maxGlyphHeight = ceilf(exemplarStringSize.height);

    return CGSizeMake(averageGlyphWidth, maxGlyphHeight);
}

- (CGFloat)estimatedLineWidthForFont:(UIFont *)font
{
    CGFloat estimatedStrokeWidth = [@"!" sizeWithAttributes:@{ NSFontAttributeName : font }].width;
    return ceilf(estimatedStrokeWidth);
}

- (BOOL)font:(UIFont *)font atSize:(CGFloat)size isLikelyToFitInAtlasRect:(CGRect)rect
{
    const float textureArea = rect.size.width * rect.size.height;
    UIFont *trialFont = [UIFont fontWithName:font.fontName size:size];
    CTFontRef trialCTFont = CTFontCreateWithName((__bridge CFStringRef)font.fontName, size, NULL);
    CFIndex fontGlyphCount = CTFontGetGlyphCount(trialCTFont);
    CGFloat glyphMargin = [self estimatedLineWidthForFont:trialFont];
    CGSize averageGlyphSize = [self estimatedGlyphSizeForFont:trialFont];
    float estimatedGlyphTotalArea = (averageGlyphSize.width + glyphMargin) * (averageGlyphSize.height + glyphMargin) * fontGlyphCount;
    CFRelease(trialCTFont);
    BOOL fits = (estimatedGlyphTotalArea < textureArea);
    return fits;
}

- (CGFloat)pointSizeThatFitsForFont:(UIFont *)font inAtlasRect:(CGRect)rect
{
    CGFloat fittedSize = font.pointSize;

    while ([self font:font atSize:fittedSize isLikelyToFitInAtlasRect:rect])
        ++fittedSize;

    while (![self font:font atSize:fittedSize isLikelyToFitInAtlasRect:rect])
        --fittedSize;

    return fittedSize;
}

- (uint8_t *)createAtlasForFont:(UIFont *)font width:(size_t)width height:(size_t)height
{
    uint8_t *imageData = malloc(width * height);

    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceGray();
    CGBitmapInfo bitmapInfo = (kCGBitmapAlphaInfoMask & kCGImageAlphaNone);
    CGContextRef context = CGBitmapContextCreate(imageData,
                                                 width,
                                                 height,
                                                 8,
                                                 width,
                                                 colorSpace,
                                                 bitmapInfo);

    // Turn off antialiasing so we only get fully-on or fully-off pixels.
    // This implicitly disables subpixel antialiasing and hinting.
    CGContextSetAllowsAntialiasing(context, false);

    // Flip context coordinate space so y increases downward
    CGContextTranslateCTM(context, 0, height);
    CGContextScaleCTM(context, 1, -1);

    // Fill the context with an opaque black color
    CGContextSetRGBFillColor(context, 0, 0, 0, 1);
    CGContextFillRect(context, CGRectMake(0, 0, width, height));

    _fontPointSize = [self pointSizeThatFitsForFont:font inAtlasRect:CGRectMake(0, 0, width, height)];
    CTFontRef ctFont = CTFontCreateWithName((__bridge CFStringRef)font.fontName, _fontPointSize, NULL);
    _parentFont = [UIFont fontWithName:font.fontName size:_fontPointSize];

    CFIndex fontGlyphCount = CTFontGetGlyphCount(ctFont);

    CGFloat glyphMargin = [self estimatedLineWidthForFont:_parentFont];

    // Set fill color so that glyphs are solid white
    CGContextSetRGBFillColor(context, 1, 1, 1, 1);

    NSMutableArray *mutableGlyphs = (NSMutableArray *)self.glyphDescriptors;
    [mutableGlyphs removeAllObjects];

    CGFloat fontAscent = CTFontGetAscent(ctFont);
    CGFloat fontDescent = CTFontGetDescent(ctFont);

    CGPoint origin = CGPointMake(0, fontAscent);
    CGFloat maxYCoordForLine = -1;
    for (CGGlyph glyph = 0; glyph < fontGlyphCount; ++glyph)
    {
        CGRect boundingRect;
        CTFontGetBoundingRectsForGlyphs(ctFont, kCTFontOrientationHorizontal, &glyph, &boundingRect, 1);

        if (origin.x + CGRectGetMaxX(boundingRect) + glyphMargin > width)
        {
            origin.x = 0;
            origin.y = maxYCoordForLine + glyphMargin + fontDescent;
            maxYCoordForLine = -1;
        }

        if (origin.y + CGRectGetMaxY(boundingRect) > maxYCoordForLine)
        {
            maxYCoordForLine = origin.y + CGRectGetMaxY(boundingRect);
        }

        CGFloat glyphOriginX = origin.x - boundingRect.origin.x + (glyphMargin * 0.5);
        CGFloat glyphOriginY = origin.y + (glyphMargin * 0.5);

        CGAffineTransform glyphTransform = CGAffineTransformMake(1, 0, 0, -1, glyphOriginX, glyphOriginY);

        CGPathRef path = CTFontCreatePathForGlyph(ctFont, glyph, &glyphTransform);
        CGContextAddPath(context, path);
        CGContextFillPath(context);

        CGRect glyphPathBoundingRect = CGPathGetPathBoundingBox(path);

        // The null rect (i.e., the bounding rect of an empty path) is problematic
        // because it has its origin at (+inf, +inf); we fix that up here
        if (CGRectEqualToRect(glyphPathBoundingRect, CGRectNull))
        {
            glyphPathBoundingRect = CGRectZero;
        }

        CGFloat texCoordLeft = glyphPathBoundingRect.origin.x / width;
        CGFloat texCoordRight = (glyphPathBoundingRect.origin.x + glyphPathBoundingRect.size.width) / width;
        CGFloat texCoordTop = (glyphPathBoundingRect.origin.y) / height;
        CGFloat texCoordBottom = (glyphPathBoundingRect.origin.y + glyphPathBoundingRect.size.height) / height;

        MBEGlyphDescriptor *descriptor = [MBEGlyphDescriptor new];
        descriptor.glyphIndex = glyph;
        descriptor.topLeftTexCoord = CGPointMake(texCoordLeft, texCoordTop);
        descriptor.bottomRightTexCoord = CGPointMake(texCoordRight, texCoordBottom);
        [mutableGlyphs addObject:descriptor];

        CGPathRelease(path);

        origin.x += CGRectGetWidth(boundingRect) + glyphMargin;
    }

#if MBE_GENERATE_DEBUG_ATLAS_IMAGE
    CGImageRef contextImage = CGBitmapContextCreateImage(context);
    // Break here to view the generated font atlas bitmap
    UIImage *fontImage = [UIImage imageWithCGImage:contextImage];
    fontImage = nil;
    CGImageRelease(contextImage);
#endif

    CFRelease(ctFont);
    CGContextRelease(context);
    CGColorSpaceRelease(colorSpace);

    return imageData;
}

/// Compute signed-distance field for an 8-bpp grayscale image (values greater than 127 are considered "on")
/// For details of this algorithm, see "The 'dead reckoning' signed distance transform" [Grevera 2004]
- (float *)createSignedDistanceFieldForGrayscaleImage:(const uint8_t *)imageData
                                                width:(size_t)width
                                               height:(size_t)height
{
    if (imageData == NULL || width == 0 || height == 0)
        return NULL;

    typedef struct { unsigned short x, y; } intpoint_t;

    float *distanceMap = malloc(width * height * sizeof(float)); // distance to nearest boundary point map
    intpoint_t *boundaryPointMap = malloc(width * height * sizeof(intpoint_t)); // nearest boundary point map

    // Some helpers for manipulating the above arrays
#define image(_x, _y) (imageData[(_y) * width + (_x)] > 0x7f)
#define distance(_x, _y) distanceMap[(_y) * width + (_x)]
#define nearestpt(_x, _y) boundaryPointMap[(_y) * width + (_x)]

    const float maxDist = hypot(width, height);
    const float distUnit = 1;
    const float distDiag = sqrt(2);

    // Initialization phase: set all distances to "infinity"; zero out nearest boundary point map
    for (long y = 0; y < height; ++y)
    {
        for (long x = 0; x < width; ++x)
        {
            distance(x, y) = maxDist;
            nearestpt(x, y) = (intpoint_t){ 0, 0 };
        }
    }

    // Immediate interior/exterior phase: mark all points along the boundary as such
    for (long y = 1; y < height - 1; ++y)
    {
        for (long x = 1; x < width - 1; ++x)
        {
            bool inside = image(x, y);
            if (image(x - 1, y) != inside ||
                image(x + 1, y) != inside ||
                image(x, y - 1) != inside ||
                image(x, y + 1) != inside)
            {
                distance(x, y) = 0;
                nearestpt(x, y) = (intpoint_t){ x, y };
            }
        }
    }

    // Forward dead-reckoning pass
    for (long y = 1; y < height - 2; ++y)
    {
        for (long x = 1; x < width - 2; ++x)
        {
            if (distanceMap[(y - 1) * width + (x - 1)] + distDiag < distance(x, y))
            {
                nearestpt(x, y) = nearestpt(x - 1, y - 1);
                distance(x, y) = hypot(x - nearestpt(x, y).x, y - nearestpt(x, y).y);
            }
            if (distance(x, y - 1) + distUnit < distance(x, y))
            {
                nearestpt(x, y) = nearestpt(x, y - 1);
                distance(x, y) = hypot(x - nearestpt(x, y).x, y - nearestpt(x, y).y);
            }
            if (distance(x + 1, y - 1) + distDiag < distance(x, y))
            {
                nearestpt(x, y) = nearestpt(x + 1, y - 1);
                distance(x, y) = hypot(x - nearestpt(x, y).x, y - nearestpt(x, y).y);
            }
            if (distance(x - 1, y) + distUnit < distance(x, y))
            {
                nearestpt(x, y) = nearestpt(x - 1, y);
                distance(x, y) = hypot(x - nearestpt(x, y).x, y - nearestpt(x, y).y);
            }
        }
    }

    // Backward dead-reckoning pass
    for (long y = height - 2; y >= 1; --y)
    {
        for (long x = width - 2; x >= 1; --x)
        {
            if (distance(x + 1, y) + distUnit < distance(x, y))
            {
                nearestpt(x, y) = nearestpt(x + 1, y);
                distance(x, y) = hypot(x - nearestpt(x, y).x, y - nearestpt(x, y).y);
            }
            if (distance(x - 1, y + 1) + distDiag < distance(x, y))
            {
                nearestpt(x, y) = nearestpt(x - 1, y + 1);
                distance(x, y) = hypot(x - nearestpt(x, y).x, y - nearestpt(x, y).y);
            }
            if (distance(x, y + 1) + distUnit < distance(x, y))
            {
                nearestpt(x, y) = nearestpt(x, y + 1);
                distance(x, y) = hypot(x - nearestpt(x, y).x, y - nearestpt(x, y).y);
            }
            if (distance(x + 1, y + 1) + distDiag < distance(x, y))
            {
                nearestpt(x, y) = nearestpt(x + 1, y + 1);
                distance(x, y) = hypot(x - nearestpt(x, y).x, y - nearestpt(x, y).y);
            }
        }
    }

    // Interior distance negation pass; distances outside the figure are considered negative
    for (long y = 0; y < height; ++y)
    {
        for (long x = 0; x < width; ++x)
        {
            if (!image(x, y))
                distance(x, y) = -distance(x, y);
        }
    }

    free(boundaryPointMap);

    return distanceMap;

#undef image
#undef distance
#undef nearestpt
}

- (float *)createResampledData:(float *)inData
                          width:(size_t)width
                         height:(size_t)height
                    scaleFactor:(size_t)scaleFactor
{
    NSAssert(width % scaleFactor == 0 && height % scaleFactor == 0,
             @"Scale factor does not evenly divide width and height of source distance field");

    size_t scaledWidth = width / scaleFactor;
    size_t scaledHeight = height / scaleFactor;
    float *outData = malloc(scaledWidth * scaledHeight * sizeof(float));

    for (int y = 0; y < height; y += scaleFactor)
    {
        for (int x = 0; x < width; x += scaleFactor)
        {
            float accum = 0;
            for (int ky = 0; ky < scaleFactor; ++ky)
            {
                for (int kx = 0; kx < scaleFactor; ++kx)
                {
                    accum += inData[(y + ky) * width + (x + kx)];
                }
            }
            accum = accum / (scaleFactor * scaleFactor);

            outData[(y / scaleFactor) * scaledWidth + (x / scaleFactor)] = accum;
        }
    }

    return outData;
}

- (uint8_t *)createQuantizedDistanceField:(float *)inData
                                    width:(size_t)width
                                   height:(size_t)height
                      normalizationFactor:(float)normalizationFactor
{
    uint8_t *outData = malloc(width * height);

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            float dist = inData[y * width + x];
            float clampDist = fmax(-normalizationFactor, fmin(dist, normalizationFactor));
            float scaledDist = clampDist / normalizationFactor;
            uint8_t value = ((scaledDist + 1) / 2) * UINT8_MAX;
            outData[y * width + x] = value;
        }
    }

    return outData;
}

- (void)createTextureData
{
    NSAssert(MBEFontAtlasSize >= self.textureSize,
             @"Requested font atlas texture size (%d) must be smaller than intermediate texture size (%d)",
             (int)MBEFontAtlasSize, (int)self.textureSize);

    NSAssert(MBEFontAtlasSize % self.textureSize == 0,
             @"Requested font atlas texture size (%d) does not evenly divide intermediate texture size (%d)",
             (int)MBEFontAtlasSize, (int)self.textureSize);

    // Generate an atlas image for the font, resizing if necessary to fit in the specified size.
    uint8_t *atlasData = [self createAtlasForFont:self.parentFont
                                            width:MBEFontAtlasSize
                                           height:MBEFontAtlasSize];

    size_t scaleFactor = MBEFontAtlasSize / self.textureSize;

    // Create the signed-distance field representation of the font atlas from the rasterized glyph image.
    float *distanceField = [self createSignedDistanceFieldForGrayscaleImage:atlasData
                                                                      width:MBEFontAtlasSize
                                                                     height:MBEFontAtlasSize];

    free(atlasData);

    // Downsample the signed-distance field to the expected texture resolution
    void *scaledField = [self createResampledData:distanceField
                                             width:MBEFontAtlasSize
                                            height:MBEFontAtlasSize
                                       scaleFactor:scaleFactor];

    free(distanceField);

    CGFloat spread = [self estimatedLineWidthForFont:self.parentFont] * 0.5;
    // Quantize the downsampled distance field into an 8-bit grayscale array suitable for use as a texture
    uint8_t *texture = [self createQuantizedDistanceField:scaledField
                                                    width:self.textureSize
                                                   height:self.textureSize
                                      normalizationFactor:spread];

    free(scaledField);

    size_t textureByteCount = self.textureSize * self.textureSize;
    _textureData = [NSData dataWithBytesNoCopy:texture length:textureByteCount freeWhenDone:YES];
}

@end
