//
//  MBETextMesh.m
//  TextRendering
//
//  Created by Warren Moore on 2/9/15.
//  Copyright (c) 2015 Metal By Example. All rights reserved.
//

#import "MBETextMesh.h"
#import "MBETypes.h"
@import CoreText;

typedef void (^MBEGlyphPositionEnumerationBlock)(CGGlyph glyph,
                                                 NSInteger glyphIndex,
                                                 CGRect glyphBounds);

@implementation MBETextMesh

@synthesize vertexBuffer=_vertexBuffer;
@synthesize indexBuffer=_indexBuffer;

- (instancetype)initWithString:(NSString *)string
                        inRect:(CGRect)rect
                      withFontAtlas:(MBEFontAtlas *)fontAtlas
                        atSize:(CGFloat)fontSize
                        device:(id<MTLDevice>)device
{
    if ((self = [super init]))
    {
        [self buildMeshWithString:string inRect:rect withFont:fontAtlas atSize:fontSize device:device];
    }
    return self;
}

- (void)buildMeshWithString:(NSString *)string
                     inRect:(CGRect)rect
                   withFont:(MBEFontAtlas *)fontAtlas
                     atSize:(CGFloat)fontSize
                     device:(id<MTLDevice>)device
{
    UIFont *font = [fontAtlas.parentFont fontWithSize:fontSize];
    NSDictionary *attributes = @{ NSFontAttributeName : font };
    NSAttributedString *attrString = [[NSAttributedString alloc] initWithString:string attributes:attributes];
    CFRange stringRange = CFRangeMake(0, attrString.length);
    CGPathRef rectPath = CGPathCreateWithRect(rect, NULL);
    CTFramesetterRef framesetter = CTFramesetterCreateWithAttributedString((__bridge CFAttributedStringRef)attrString);
    CTFrameRef frame = CTFramesetterCreateFrame(framesetter, stringRange, rectPath, NULL);

    __block CFIndex frameGlyphCount = 0;
    NSArray *lines = (__bridge id)CTFrameGetLines(frame);
    [lines enumerateObjectsUsingBlock:^(id lineObject, NSUInteger lineIndex, BOOL *stop) {
        frameGlyphCount += CTLineGetGlyphCount((__bridge CTLineRef)lineObject);
    }];

    const size_t vertexCount = frameGlyphCount * 4;
    const size_t indexCount = frameGlyphCount * 6;
    MBEVertex *vertices = malloc(vertexCount * sizeof(MBEVertex));
    MBEIndexType *indices = malloc(indexCount * sizeof(MBEIndexType));

    __block MBEIndexType v = 0, i = 0;
    [self enumerateGlyphsInFrame:frame block:^(CGGlyph glyph, NSInteger glyphIndex, CGRect glyphBounds) {
        if (glyph >= fontAtlas.glyphDescriptors.count)
        {
            NSLog(@"Font atlas has no entry corresponding to glyph #%d; Skipping...", glyph);
            return;
        }
        MBEGlyphDescriptor *glyphInfo = fontAtlas.glyphDescriptors[glyph];
        float minX = CGRectGetMinX(glyphBounds);
        float maxX = CGRectGetMaxX(glyphBounds);
        float minY = CGRectGetMinY(glyphBounds);
        float maxY = CGRectGetMaxY(glyphBounds);
        float minS = glyphInfo.topLeftTexCoord.x;
        float maxS = glyphInfo.bottomRightTexCoord.x;
        float minT = glyphInfo.topLeftTexCoord.y;
        float maxT = glyphInfo.bottomRightTexCoord.y;
        vertices[v++] = (MBEVertex){ { minX, maxY, 0, 1 }, { minS, maxT } };
        vertices[v++] = (MBEVertex){ { minX, minY, 0, 1 }, { minS, minT } };
        vertices[v++] = (MBEVertex){ { maxX, minY, 0, 1 }, { maxS, minT } };
        vertices[v++] = (MBEVertex){ { maxX, maxY, 0, 1 }, { maxS, maxT } };
        indices[i++] = glyphIndex * 4;
        indices[i++] = glyphIndex * 4 + 1;
        indices[i++] = glyphIndex * 4 + 2;
        indices[i++] = glyphIndex * 4 + 2;
        indices[i++] = glyphIndex * 4 + 3;
        indices[i++] = glyphIndex * 4;
    }];

    _vertexBuffer = [device newBufferWithBytes:vertices
                                        length:vertexCount * sizeof(MBEVertex)
                                       options:MTLResourceOptionCPUCacheModeDefault];
    [_vertexBuffer setLabel:@"Text Mesh Vertices"];
    _indexBuffer = [device newBufferWithBytes:indices
                                       length:indexCount * sizeof(MBEIndexType)
                                      options:MTLResourceOptionCPUCacheModeDefault];
    [_indexBuffer setLabel:@"Text Mesh Indices"];

    free(indices);
    free(vertices);
    CFRelease(frame);
    CFRelease(framesetter);
    CFRelease(rectPath);
}

- (void)enumerateGlyphsInFrame:(CTFrameRef)frame
                         block:(MBEGlyphPositionEnumerationBlock)block
{
    if (!block)
        return;

    CFRange entire = CFRangeMake(0, 0);

    CGPathRef framePath = CTFrameGetPath(frame);
    CGRect frameBoundingRect = CGPathGetPathBoundingBox(framePath);

    NSArray *lines = (__bridge id)CTFrameGetLines(frame);

    CGPoint *lineOriginBuffer = malloc(lines.count * sizeof(CGPoint));
    CTFrameGetLineOrigins(frame, entire, lineOriginBuffer);

    __block CFIndex glyphIndexInFrame = 0;

    UIGraphicsBeginImageContext(CGSizeMake(1, 1));
    CGContextRef context = UIGraphicsGetCurrentContext();

    [lines enumerateObjectsUsingBlock:^(id lineObject, NSUInteger lineIndex, BOOL *stop) {
        CTLineRef line = (__bridge CTLineRef)lineObject;
        CGPoint lineOrigin = lineOriginBuffer[lineIndex];

        NSArray *runs = (__bridge id)CTLineGetGlyphRuns(line);
        [runs enumerateObjectsUsingBlock:^(id runObject, NSUInteger rangeIndex, BOOL *stop) {
            CTRunRef run = (__bridge CTRunRef)runObject;

            size_t glyphCount = CTRunGetGlyphCount(run);

            CGGlyph *glyphBuffer = malloc(glyphCount * sizeof(CGGlyph));
            CTRunGetGlyphs(run, entire, glyphBuffer);

            CGPoint *positionBuffer = malloc(glyphCount * sizeof(CGPoint));
            CTRunGetPositions(run, entire, positionBuffer);

            for (size_t glyphIndex = 0; glyphIndex < glyphCount; ++glyphIndex)
            {
                CGGlyph glyph = glyphBuffer[glyphIndex];
                CGPoint glyphOrigin = positionBuffer[glyphIndex];
                CGRect glyphRect = CTRunGetImageBounds(run, context, CFRangeMake(glyphIndex, 1));
                CGFloat boundsTransX = frameBoundingRect.origin.x + lineOrigin.x;
                CGFloat boundsTransY = CGRectGetHeight(frameBoundingRect) + frameBoundingRect.origin.y - lineOrigin.y + glyphOrigin.y;
                CGAffineTransform pathTransform = CGAffineTransformMake(1, 0, 0, -1, boundsTransX, boundsTransY);
                glyphRect = CGRectApplyAffineTransform(glyphRect, pathTransform);
                block(glyph, glyphIndexInFrame, glyphRect);

                ++glyphIndexInFrame;
            }

            free(positionBuffer);
            free(glyphBuffer);
        }];
    }];
    
    UIGraphicsEndImageContext();
}

@end
