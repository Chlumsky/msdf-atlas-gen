//
//  GameViewController.swift
//  MSDFTextRender
//
//  Created by Sihao Lu on 10/8/25.
//

import UIKit
import MetalKit

// Our iOS specific view controller
class GameViewController: UIViewController {

    var renderer: Renderer!
    var mtkView: MTKView!
    
    private var zoomScale: CGFloat = 1.0
    private let minZoomScale: CGFloat = 0.5
    private let maxZoomScale: CGFloat = 3.0

    override func viewDidLoad() {
        super.viewDidLoad()

        guard let mtkView = view as? MTKView else {
            print("View of Gameview controller is not an MTKView")
            return
        }

        // Select the device to render with.  We choose the default device
        guard let defaultDevice = MTLCreateSystemDefaultDevice() else {
            print("Metal is not supported")
            return
        }
        
        mtkView.device = defaultDevice
        mtkView.backgroundColor = UIColor.black

        guard let newRenderer = Renderer(metalKitView: mtkView) else {
            print("Renderer cannot be initialized")
            return
        }

        renderer = newRenderer
        self.mtkView = mtkView

        renderer.mtkView(mtkView, drawableSizeWillChange: mtkView.drawableSize)

        mtkView.delegate = renderer
        mtkView.isMultipleTouchEnabled = true
        
        configureGestureRecognizers(for: mtkView)
        applyViewport(scale: zoomScale)
    }
    
    override func viewDidLayoutSubviews() {
        super.viewDidLayoutSubviews()
        guard let mtkView = mtkView else { return }
        renderer?.mtkView(mtkView, drawableSizeWillChange: mtkView.drawableSize)
    }
    
    private func configureGestureRecognizers(for view: MTKView) {
        let pinchRecognizer = UIPinchGestureRecognizer(target: self,
                                                       action: #selector(handlePinch(_:)))
        view.addGestureRecognizer(pinchRecognizer)
    }
    
    private func applyViewport(scale: CGFloat) {
        guard let renderer = renderer else { return }
        let clampedScale = max(min(scale, maxZoomScale), minZoomScale)
        zoomScale = clampedScale
        renderer.updateZoom(zoomScale: zoomScale)
        renderer.rebuildTextMeshForCurrentView()
    }
    
    @objc private func handlePinch(_ recognizer: UIPinchGestureRecognizer) {
        switch recognizer.state {
        case .changed, .ended:
            var targetScale = zoomScale * recognizer.scale
            targetScale = max(min(targetScale, maxZoomScale), minZoomScale)
            applyViewport(scale: targetScale)
            recognizer.scale = 1.0
        case .cancelled, .failed:
            applyViewport(scale: zoomScale)
        default:
            break
        }
    }
}
