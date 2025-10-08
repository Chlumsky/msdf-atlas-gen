//
//  MBEViewController.m
//  TextRendering
//
//  Created by Warren Moore on 2/6/15.
//  Copyright (c) 2015 Metal By Example. All rights reserved.
//

#import "MBEViewController.h"
#import "MBEMetalView.h"
#import "MBERenderer.h"

@interface MBEViewController () <UIGestureRecognizerDelegate>
@property (nonatomic, strong) MBERenderer *renderer;
@property (nonatomic, strong) CADisplayLink *displayLink;
@end

@implementation MBEViewController

- (MBEMetalView *)metalView
{
    return (MBEMetalView *)self.view;
}

- (void)viewDidLoad {
    [super viewDidLoad];

    self.renderer = [[MBERenderer alloc] initWithLayer:self.metalView.metalLayer];

    self.displayLink = [CADisplayLink displayLinkWithTarget:self selector:@selector(displayLinkDidFire:)];
    [self.displayLink addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSRunLoopCommonModes];

    UIPanGestureRecognizer *panGestureRecognizer = [[UIPanGestureRecognizer alloc] initWithTarget:self
                                                                                           action:@selector(panGestureWasRecognized:)];
    panGestureRecognizer.delegate = self;
    [self.view addGestureRecognizer:panGestureRecognizer];

    UIPinchGestureRecognizer *pinchGestureRecognizer = [[UIPinchGestureRecognizer alloc] initWithTarget:self
                                                                                                 action:@selector(pinchGestureWasRecognized:)];
    pinchGestureRecognizer.delegate = self;
    [self.view addGestureRecognizer:pinchGestureRecognizer];
}

- (BOOL)prefersStatusBarHidden
{
    return YES;
}

- (void)displayLinkDidFire:(id)sender
{
    [self redraw];
}

- (void)redraw
{
    [self.renderer draw];
}

- (void)panGestureWasRecognized:(UIPanGestureRecognizer *)sender
{
    CGPoint translation = self.renderer.textTranslation;
    CGPoint deltaTranslation = [sender translationInView:self.view];
    self.renderer.textTranslation = CGPointMake(translation.x + deltaTranslation.x, translation.y + deltaTranslation.y);
    [sender setTranslation:CGPointZero inView:self.view];
}

- (void)pinchGestureWasRecognized:(UIPinchGestureRecognizer *)sender
{
    CGFloat targetScale = self.renderer.textScale * sender.scale;
    targetScale = fmax(0.5, fmin(targetScale, 5));
    self.renderer.textScale = targetScale;
    sender.scale = 1;
}

- (BOOL)gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer shouldRecognizeSimultaneouslyWithGestureRecognizer:(UIGestureRecognizer *)otherGestureRecognizer
{
    return YES;
}

@end
