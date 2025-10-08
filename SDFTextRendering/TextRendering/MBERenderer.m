//
//  MBERenderer.m
//  TextRendering
//
//  Created by Warren Moore on 12/15/14.
//  Copyright (c) 2014 Metal By Example. All rights reserved.
//

@import Metal;
#import "MBERenderer.h"
#import "MBEMathUtilities.h"
#import "MBETypes.h"
#import "MBEFontAtlas.h"
#import "MBETextMesh.h"

#define MBE_FORCE_REGENERATE_FONT_ATLAS 0

static NSString *const MBEFontName = @"HoeflerText-Regular";
static float MBEFontDisplaySize = 72;
static NSString *const MBESampleText = @"It was the best of times, it was the worst of times, "
                                        "it was the age of wisdom, it was the age of foolishness...\n\n"
                                        "Все счастливые семьи похожи друг на друга, "
                                        "каждая несчастливая семья несчастлива по-своему.";
static vector_float4 MBETextColor = { 0.1, 0.1, 0.1, 1 };
static MTLClearColor MBEClearColor = { 1, 1, 1, 1 };
static float MBEFontAtlasSize = 2048;

@interface MBERenderer ()
@property (nonatomic, strong) CAMetalLayer *layer;
// Long-lived Metal objects
@property (nonatomic, strong) id<MTLDevice> device;
@property (nonatomic, strong) id<MTLCommandQueue> commandQueue;
@property (nonatomic, strong) id<MTLRenderPipelineState> pipelineState;
@property (nonatomic, strong) id<MTLSamplerState> sampler;
// Resources
@property (nonatomic, strong) id<MTLTexture> depthTexture;
@property (nonatomic, strong) MBEFontAtlas *fontAtlas;
@property (nonatomic, strong) MBETextMesh *textMesh;
@property (nonatomic, strong) id<MTLBuffer> uniformBuffer;
@property (nonatomic, strong) id<MTLTexture> fontTexture;
@end

@implementation MBERenderer

- (instancetype)initWithLayer:(CAMetalLayer *)layer
{
    if ((self = [super init]))
    {
        _layer = layer;
        [self buildMetal];
        [self buildResources];

        _textScale = 1.0;
        _textTranslation = CGPointMake(0, 0);
    }
    return self;
}

- (void)buildMetal
{
    _device = MTLCreateSystemDefaultDevice();
    _layer.device = _device;
    _layer.pixelFormat = MTLPixelFormatBGRA8Unorm;

    _commandQueue = [_device newCommandQueue];

    MTLSamplerDescriptor *samplerDescriptor = [MTLSamplerDescriptor new];
    samplerDescriptor.minFilter = MTLSamplerMinMagFilterNearest;
    samplerDescriptor.magFilter = MTLSamplerMinMagFilterLinear;
    samplerDescriptor.sAddressMode = MTLSamplerAddressModeClampToZero;
    samplerDescriptor.tAddressMode = MTLSamplerAddressModeClampToZero;
    _sampler = [_device newSamplerStateWithDescriptor:samplerDescriptor];

    id<MTLLibrary> library = [_device newDefaultLibrary];

    MTLRenderPipelineDescriptor *pipelineDescriptor = [MTLRenderPipelineDescriptor new];

    pipelineDescriptor.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
    pipelineDescriptor.colorAttachments[0].blendingEnabled = YES;
    pipelineDescriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
    pipelineDescriptor.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    pipelineDescriptor.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
    pipelineDescriptor.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorSourceAlpha;
    pipelineDescriptor.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    pipelineDescriptor.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;

    pipelineDescriptor.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
    pipelineDescriptor.vertexFunction = [library newFunctionWithName:@"vertex_shade"];
    pipelineDescriptor.fragmentFunction = [library newFunctionWithName:@"fragment_shade"];
    pipelineDescriptor.vertexDescriptor = [self newVertexDescriptor];

    NSError *error = nil;
    _pipelineState = [_device newRenderPipelineStateWithDescriptor:pipelineDescriptor error:&error];
    if (!_pipelineState)
    {
        NSLog(@"Error occurred when compiling pipeline state: %@", error);
    }
}

- (MTLVertexDescriptor *)newVertexDescriptor
{
    MTLVertexDescriptor *vertexDescriptor = [MTLVertexDescriptor new];

    // Position
    vertexDescriptor.attributes[0].format = MTLVertexFormatFloat4;
    vertexDescriptor.attributes[0].offset = 0;
    vertexDescriptor.attributes[0].bufferIndex = 0;

    // Texture coordinates
    vertexDescriptor.attributes[1].format = MTLVertexFormatFloat2;
    vertexDescriptor.attributes[1].offset = sizeof(vector_float4);
    vertexDescriptor.attributes[1].bufferIndex = 0;

    vertexDescriptor.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;
    vertexDescriptor.layouts[0].stride = sizeof(MBEVertex);

    return vertexDescriptor;
}

- (NSURL *)documentsURL
{
    NSArray *candidates = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
    NSString *documentsPath = [candidates firstObject];
    return [NSURL fileURLWithPath:documentsPath isDirectory:YES];
}

- (void)buildResources
{
    [self buildFontAtlas];
    [self buildTextMesh];
    [self buildUniformBuffer];
}

- (void)buildFontAtlas
{
    NSURL *fontURL = [[self.documentsURL URLByAppendingPathComponent:MBEFontName] URLByAppendingPathExtension:@"sdff"];

#if !MBE_FORCE_REGENERATE_FONT_ATLAS
    _fontAtlas = [NSKeyedUnarchiver unarchiveObjectWithFile:fontURL.path];
#endif

    // Cache miss: if we don't have a serialized version of the font atlas, build it now
    if (!_fontAtlas)
    {
        UIFont *font = [UIFont fontWithName:MBEFontName size:32];
        _fontAtlas = [[MBEFontAtlas alloc] initWithFont:font textureSize:MBEFontAtlasSize];
        [NSKeyedArchiver archiveRootObject:_fontAtlas toFile:fontURL.path];
    }

    MTLTextureDescriptor *textureDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR8Unorm
                                                                                           width:MBEFontAtlasSize
                                                                                          height:MBEFontAtlasSize
                                                                                       mipmapped:NO];
    MTLRegion region = MTLRegionMake2D(0, 0, MBEFontAtlasSize, MBEFontAtlasSize);
    _fontTexture = [_device newTextureWithDescriptor:textureDesc];
    [_fontTexture setLabel:@"Font Atlas"];
    [_fontTexture replaceRegion:region mipmapLevel:0 withBytes:_fontAtlas.textureData.bytes bytesPerRow:MBEFontAtlasSize];
}

- (void)buildTextMesh
{
    CGRect textRect = CGRectInset([UIScreen mainScreen].nativeBounds, 10, 10);

    _textMesh = [[MBETextMesh alloc] initWithString:MBESampleText
                                             inRect:textRect
                                      withFontAtlas:_fontAtlas
                                             atSize:MBEFontDisplaySize
                                             device:_device];
}

- (void)buildUniformBuffer
{
    _uniformBuffer = [_device newBufferWithLength:sizeof(MBEUniforms)
                                          options:MTLResourceOptionCPUCacheModeDefault];
    [_uniformBuffer setLabel:@"Uniform Buffer"];
}

- (void)buildDepthTexture
{
    CGSize drawableSize = self.layer.drawableSize;
    MTLTextureDescriptor *descriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float
                                                                                          width:drawableSize.width
                                                                                         height:drawableSize.height
                                                                                      mipmapped:NO];
    descriptor.storageMode = MTLStorageModeMemoryless;
    descriptor.usage = MTLTextureUsageRenderTarget;
    self.depthTexture = [self.device newTextureWithDescriptor:descriptor];
    [self.depthTexture setLabel:@"Depth Texture"];
}


- (MTLRenderPassDescriptor *)newRenderPassWithColorAttachmentTexture:(id<MTLTexture>)texture
{
    MTLRenderPassDescriptor *renderPass = [MTLRenderPassDescriptor new];

    renderPass.colorAttachments[0].texture = texture;
    renderPass.colorAttachments[0].loadAction = MTLLoadActionClear;
    renderPass.colorAttachments[0].storeAction = MTLStoreActionStore;
    renderPass.colorAttachments[0].clearColor = MBEClearColor;

    renderPass.depthAttachment.texture = self.depthTexture;
    renderPass.depthAttachment.loadAction = MTLLoadActionClear;
    renderPass.depthAttachment.storeAction = MTLStoreActionDontCare;
    renderPass.depthAttachment.clearDepth = 1.0;

    return renderPass;
}

- (void)updateUniforms
{
    CGSize drawableSize = self.layer.drawableSize;

    MBEUniforms uniforms;

    vector_float3 translation = { self.textTranslation.x, self.textTranslation.y, 0 };
    vector_float3 scale = { self.textScale, self.textScale, 1 };
    matrix_float4x4 modelMatrix = matrix_multiply(matrix_translation(translation), matrix_scale(scale));
    uniforms.modelMatrix = modelMatrix;

    matrix_float4x4 projectionMatrix = matrix_orthographic_projection(0, drawableSize.width, 0, drawableSize.height);
    uniforms.viewProjectionMatrix = projectionMatrix;

    uniforms.foregroundColor = MBETextColor;

    memcpy([self.uniformBuffer contents], &uniforms, sizeof(MBEUniforms));
}

- (void)draw
{
    id<CAMetalDrawable> drawable = [self.layer nextDrawable];

    if (drawable)
    {
        CGSize drawableSize = self.layer.drawableSize;

        if ([self.depthTexture width] != drawableSize.width || [self.depthTexture height] != drawableSize.height)
        {
            [self buildDepthTexture];
        }

        [self updateUniforms];

        MTLRenderPassDescriptor *renderPass = [self newRenderPassWithColorAttachmentTexture:[drawable texture]];

        id<MTLCommandBuffer> commandBuffer = [self.commandQueue commandBuffer];

        id<MTLRenderCommandEncoder> commandEncoder = [commandBuffer renderCommandEncoderWithDescriptor:renderPass];
        [commandEncoder setFrontFacingWinding:MTLWindingCounterClockwise];
        [commandEncoder setCullMode:MTLCullModeNone];
        [commandEncoder setRenderPipelineState:self.pipelineState];

        [commandEncoder setVertexBuffer:self.textMesh.vertexBuffer offset:0 atIndex:0];
        [commandEncoder setVertexBuffer:self.uniformBuffer offset:0 atIndex:1];

        [commandEncoder setFragmentBuffer:self.uniformBuffer offset:0 atIndex:0];
        [commandEncoder setFragmentTexture:self.fontTexture atIndex:0];
        [commandEncoder setFragmentSamplerState:self.sampler atIndex:0];

        [commandEncoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                                   indexCount:[self.textMesh.indexBuffer length] / sizeof(MBEIndexType)
                                    indexType:MTLIndexTypeUInt16
                                  indexBuffer:self.textMesh.indexBuffer
                            indexBufferOffset:0];

        [commandEncoder endEncoding];

        [commandBuffer presentDrawable:drawable];
        [commandBuffer commit];
    }
}

@end
