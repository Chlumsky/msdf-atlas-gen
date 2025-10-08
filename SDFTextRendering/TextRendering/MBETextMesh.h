//
//  MBETextMesh.h
//  TextRendering
//
//  Created by Warren Moore on 2/9/15.
//  Copyright (c) 2015 Metal By Example. All rights reserved.
//

@import Metal;
#import "MBEMesh.h"
#import "MBEFontAtlas.h"

@interface MBETextMesh : MBEMesh

- (instancetype)initWithString:(NSString *)string
                        inRect:(CGRect)rect
                      withFontAtlas:(MBEFontAtlas *)fontAtlas
                        atSize:(CGFloat)fontSize
                        device:(id<MTLDevice>)device;

@end
