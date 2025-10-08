//
//  MBETextureLoader.h
//  TextRendering
//
//  Created by Warren Moore on 11/7/14.
//  Copyright (c) 2014 Metal By Example. All rights reserved.
//

@import UIKit;
@import Metal;

@interface MBETextureLoader : NSObject

+ (instancetype)sharedTextureLoader;

- (id<MTLTexture>)texture2DWithImage:(UIImage *)image
                           mipmapped:(BOOL)mipmapped
                              device:(id<MTLDevice>)device;

@end
