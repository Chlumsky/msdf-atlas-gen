//
//  MBERenderer.h
//  TextRendering
//
//  Created by Warren Moore on 12/15/14.
//  Copyright (c) 2014 Metal By Example. All rights reserved.
//

@import Foundation;
@import QuartzCore.CAMetalLayer;

@interface MBERenderer : NSObject

@property (nonatomic, assign) CGPoint textTranslation;
@property (nonatomic, assign) CGFloat textScale;

- (instancetype)initWithLayer:(CAMetalLayer *)layer;
- (void)draw;

@end
