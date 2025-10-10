//
//  Renderer.swift
//  MSDFTextRender
//
//  Created by Sihao Lu on 10/8/25.
//

import Metal
import MetalKit
import simd
import CoreText

let alignedUniformsSize = (MemoryLayout<Uniforms>.size + 0xFF) & -0x100
let maxBuffersInFlight = 3

class Renderer: NSObject, MTKViewDelegate {
    
    let device: MTLDevice
    let commandQueue: MTLCommandQueue
    var dynamicUniformBuffer: MTLBuffer
    var pipelineState: MTLRenderPipelineState
    var depthState: MTLDepthStencilState
    var atlasTexture: MTLTexture
    var atlasData: MSDFAtlas
    let textContent: String
    weak var view: MTKView?
    var textMeshBuilder: MSDFTextMeshBuilder?
    var textMesh: MSDFTextMesh?
    
    let inFlightSemaphore = DispatchSemaphore(value: maxBuffersInFlight)
    
    var uniformBufferOffset = 0
    var uniformBufferIndex = 0
    var uniforms: UnsafeMutablePointer<Uniforms>
    
    var projectionMatrix: matrix_float4x4 = matrix_identity_float4x4
    var zoomScale: CGFloat = 1.0
    
    let margin: CGFloat = 24.0
    let baseFontSize: CGFloat = 36.0
    private let baseFont: CTFont
    private var currentFontSize: CGFloat
    var atlasPxRange: SIMD2<Float>
    var atlasUnitRange = SIMD2<Float>(repeating: 0)
    var textColor = SIMD4<Float>(1, 1, 1, 1)

    @MainActor
    init?(metalKitView: MTKView) {
        guard let device = metalKitView.device,
              let queue = device.makeCommandQueue() else {
            return nil
        }
        
        self.device = device
        self.commandQueue = queue
        
        let uniformBufferSize = alignedUniformsSize * maxBuffersInFlight
        guard let buffer = device.makeBuffer(length: uniformBufferSize,
                                             options: [.storageModeShared]) else {
            return nil
        }
        dynamicUniformBuffer = buffer
        dynamicUniformBuffer.label = "UniformBuffer"
        uniforms = UnsafeMutableRawPointer(dynamicUniformBuffer.contents())
            .bindMemory(to: Uniforms.self, capacity: 1)
        
        metalKitView.depthStencilPixelFormat = .invalid
        metalKitView.colorPixelFormat = .bgra8Unorm_srgb
        metalKitView.sampleCount = 1
        
        let vertexDescriptor = Renderer.buildMetalVertexDescriptor()
        
        do {
            pipelineState = try Renderer.buildRenderPipelineWithDevice(device: device,
                                                                       metalKitView: metalKitView,
                                                                       mtlVertexDescriptor: vertexDescriptor)
        } catch {
            print("Unable to compile render pipeline state. Error: \(error)")
            return nil
        }
        
        let depthDescriptor = MTLDepthStencilDescriptor()
        depthDescriptor.depthCompareFunction = .always
        depthDescriptor.isDepthWriteEnabled = false
        guard let depthState = device.makeDepthStencilState(descriptor: depthDescriptor) else {
            return nil
        }
        self.depthState = depthState
        
        guard let atlasJSONURL = Bundle.main.url(forResource: "SF-Pro-Display_msdf", withExtension: "json"),
              let fontURL = Bundle.main.url(forResource: "SF-Pro-Display-Regular", withExtension: "otf") else {
            print("Missing MSDF resources in bundle.")
            return nil
        }
        
        do {
            atlasData = try MSDFAtlas.load(from: atlasJSONURL)
            atlasPxRange = atlasData.pxRange
            atlasTexture = try Renderer.loadTexture(device: device)
            atlasUnitRange = atlasPxRange / SIMD2<Float>(Float(atlasTexture.width), Float(atlasTexture.height))
        } catch {
            print("Unable to load atlas resources. Error: \(error)")
            return nil
        }
        
        guard let ctFont = Renderer.loadFont(at: fontURL, size: baseFontSize) else {
            print("Unable to load SF Pro Display font.")
            return nil
        }
        
        baseFont = ctFont
        currentFontSize = baseFontSize
        textMeshBuilder = MSDFTextMeshBuilder(device: device, atlas: atlasData, font: ctFont)
        
        textContent = Renderer.composeParagraphText()
        super.init()
        
        view = metalKitView
        
        rebuildTextMesh(for: metalKitView)
        updateProjection(for: metalKitView.drawableSize)
    }
    
    class func buildMetalVertexDescriptor() -> MTLVertexDescriptor {
        let descriptor = MTLVertexDescriptor()
        let stride = MemoryLayout<MSDFGlyphVertex>.stride
        
        descriptor.attributes[VertexAttribute.position.rawValue].format = .float3
        descriptor.attributes[VertexAttribute.position.rawValue].offset = 0
        descriptor.attributes[VertexAttribute.position.rawValue].bufferIndex = BufferIndex.meshPositions.rawValue
        
        descriptor.attributes[VertexAttribute.texcoord.rawValue].format = .float2
        descriptor.attributes[VertexAttribute.texcoord.rawValue].offset = MemoryLayout<SIMD3<Float>>.stride
        descriptor.attributes[VertexAttribute.texcoord.rawValue].bufferIndex = BufferIndex.meshPositions.rawValue
        
        descriptor.layouts[BufferIndex.meshPositions.rawValue].stride = stride
        descriptor.layouts[BufferIndex.meshPositions.rawValue].stepRate = 1
        descriptor.layouts[BufferIndex.meshPositions.rawValue].stepFunction = .perVertex
        
        return descriptor
    }
    
    @MainActor
    class func buildRenderPipelineWithDevice(device: MTLDevice,
                                             metalKitView: MTKView,
                                             mtlVertexDescriptor: MTLVertexDescriptor) throws -> MTLRenderPipelineState {
        let library = device.makeDefaultLibrary()
        let vertexFunction = library?.makeFunction(name: "vertexShader")
        let fragmentFunction = library?.makeFunction(name: "fragmentShader")
        
        let pipelineDescriptor = MTLRenderPipelineDescriptor()
        pipelineDescriptor.label = "MSDFTextPipeline"
        pipelineDescriptor.rasterSampleCount = metalKitView.sampleCount
        pipelineDescriptor.vertexFunction = vertexFunction
        pipelineDescriptor.fragmentFunction = fragmentFunction
        pipelineDescriptor.vertexDescriptor = mtlVertexDescriptor
        
        pipelineDescriptor.colorAttachments[0].pixelFormat = metalKitView.colorPixelFormat
        pipelineDescriptor.depthAttachmentPixelFormat = metalKitView.depthStencilPixelFormat
        pipelineDescriptor.stencilAttachmentPixelFormat = metalKitView.depthStencilPixelFormat
        
        if let attachment = pipelineDescriptor.colorAttachments[0] {
            attachment.isBlendingEnabled = true
            attachment.sourceRGBBlendFactor = .sourceAlpha
            attachment.destinationRGBBlendFactor = .oneMinusSourceAlpha
            attachment.rgbBlendOperation = .add
            attachment.sourceAlphaBlendFactor = .one
            attachment.destinationAlphaBlendFactor = .oneMinusSourceAlpha
            attachment.alphaBlendOperation = .add
        }
        
        return try device.makeRenderPipelineState(descriptor: pipelineDescriptor)
    }
    
    class func loadTexture(device: MTLDevice) throws -> MTLTexture {
        let textureLoader = MTKTextureLoader(device: device)
        let options: [MTKTextureLoader.Option: Any] = [
            .SRGB: false,
            .generateMipmaps: false,
            .origin: MTKTextureLoader.Origin.bottomLeft,
            .textureUsage: NSNumber(value: MTLTextureUsage.shaderRead.rawValue),
            .textureStorageMode: NSNumber(value: MTLStorageMode.private.rawValue),
        ]
        return try textureLoader.newTexture(name: "SF-Pro-Display_msdf",
                                            scaleFactor: 1.0,
                                            bundle: .main,
                                            options: options)
    }
    
    private static func loadFont(at url: URL, size: CGFloat) -> CTFont? {
        guard let dataProvider = CGDataProvider(url: url as CFURL),
              let cgFont = CGFont(dataProvider) else {
            return nil
        }
        
        // Use the new API for iOS 18+ and fall back to the deprecated one for older versions
        if #available(iOS 18.0, *) {
            var error: Unmanaged<CFError>?
            if !CTFontManagerRegisterFontsForURL(url as CFURL, .process, &error) {
                if let cfError = error?.takeRetainedValue() {
                    let codeValue = CFErrorGetCode(cfError)
                    if let ctError = CTFontManagerError(rawValue: codeValue),
                       ctError == .alreadyRegistered {
                        // Font already registered; safe to ignore.
                    } else {
                        print("Font registration error: \(cfError)")
                    }
                }
            }
        } else {
            var error: Unmanaged<CFError>?
            if !CTFontManagerRegisterGraphicsFont(cgFont, &error) {
                if let cfError = error?.takeRetainedValue() {
                    let codeValue = CFErrorGetCode(cfError)
                    if let ctError = CTFontManagerError(rawValue: codeValue),
                       ctError == .alreadyRegistered {
                        // Font already registered; safe to ignore.
                    } else {
                        print("Font registration error: \(cfError)")
                    }
                }
            }
        }
        return CTFontCreateWithGraphicsFont(cgFont, size, nil, nil)
    }
    
    private func rebuildTextMesh(for view: MTKView) {
        guard let builder = textMeshBuilder else { return }
        updateFontForCurrentZoom()
        let viewScale = max(CGFloat(view.contentScaleFactor), 0.0001)
        let layoutWidth = max(view.bounds.width, 1.0)
        let layoutHeight = max(view.bounds.height, 1.0)
        textMesh = builder.buildMesh(for: textContent,
                                     in: CGSize(width: layoutWidth, height: layoutHeight),
                                     margin: margin,
                                     scale: viewScale)
    }
    
    private func updateProjection(for drawableSize: CGSize) {
        guard drawableSize.width > 0, drawableSize.height > 0 else { return }
        projectionMatrix = matrix_ortho(width: Float(drawableSize.width),
                                        height: Float(drawableSize.height))
    }
    
    private func updateFontForCurrentZoom() {
        let targetSize = max(baseFontSize * zoomScale, 0.0001)
        guard abs(targetSize - currentFontSize) > 0.0001 else { return }
        let scaledFont = CTFontCreateCopyWithAttributes(baseFont, targetSize, nil, nil)
        textMeshBuilder?.updateFont(scaledFont)
        currentFontSize = targetSize
    }
    
    private func updateDynamicBufferState() {
        uniformBufferIndex = (uniformBufferIndex + 1) % maxBuffersInFlight
        uniformBufferOffset = alignedUniformsSize * uniformBufferIndex
        uniforms = UnsafeMutableRawPointer(dynamicUniformBuffer.contents() + uniformBufferOffset)
            .bindMemory(to: Uniforms.self, capacity: 1)
    }
    
    private func updateUniforms() {
        uniforms[0].projectionMatrix = projectionMatrix
        uniforms[0].modelViewMatrix = matrix_identity_float4x4
        uniforms[0].textColor = textColor
        uniforms[0].unitRange = atlasUnitRange
        uniforms[0].padding = .zero
    }
    
    func draw(in view: MTKView) {
        _ = inFlightSemaphore.wait(timeout: .distantFuture)
        
        guard let textMesh = textMesh else {
            inFlightSemaphore.signal()
            return
        }
        
        guard let commandBuffer = commandQueue.makeCommandBuffer() else {
            inFlightSemaphore.signal()
            return
        }
        
        commandBuffer.label = "TextCommandBuffer"
        commandBuffer.addCompletedHandler { [weak self] _ in
            self?.inFlightSemaphore.signal()
        }
        
        updateDynamicBufferState()
        updateUniforms()
        
        guard let renderPassDescriptor = view.currentRenderPassDescriptor,
              let renderEncoder = commandBuffer.makeRenderCommandEncoder(descriptor: renderPassDescriptor) else {
            commandBuffer.commit()
            return
        }
        
        renderEncoder.label = "MSDF Text Encoder"
        renderEncoder.setCullMode(.none)
        renderEncoder.setRenderPipelineState(pipelineState)
        renderEncoder.setDepthStencilState(depthState)
        
        renderEncoder.setVertexBuffer(textMesh.vertexBuffer,
                                      offset: 0,
                                      index: BufferIndex.meshPositions.rawValue)
        renderEncoder.setVertexBuffer(dynamicUniformBuffer,
                                      offset: uniformBufferOffset,
                                      index: BufferIndex.uniforms.rawValue)
        renderEncoder.setFragmentBuffer(dynamicUniformBuffer,
                                        offset: uniformBufferOffset,
                                        index: BufferIndex.uniforms.rawValue)
        renderEncoder.setFragmentTexture(atlasTexture, index: TextureIndex.color.rawValue)
        
        renderEncoder.drawIndexedPrimitives(type: .triangle,
                                            indexCount: textMesh.indexCount,
                                            indexType: .uint32,
                                            indexBuffer: textMesh.indexBuffer,
                                            indexBufferOffset: 0)
        renderEncoder.endEncoding()
        
        if let drawable = view.currentDrawable {
            commandBuffer.present(drawable)
        }
        commandBuffer.commit()
    }
    
    func mtkView(_ view: MTKView, drawableSizeWillChange size: CGSize) {
        updateProjection(for: size)
        rebuildTextMesh(for: view)
    }
    
    @MainActor
    func rebuildTextMeshForCurrentView() {
        guard let view = view else { return }
        rebuildTextMesh(for: view)
    }
    
    @MainActor
    func updateZoom(zoomScale: CGFloat) {
        self.zoomScale = max(zoomScale, 0.0001)
        updateFontForCurrentZoom()
    }
    
    private static func composeParagraphText() -> String {
        let english = """
        English:
        It was the best of times, it was the worst of times, it was the age of wisdom, it was the age of foolishness, it was the epoch of belief, it was the epoch of incredulity, it was the season of Light, it was the season of Darkness, it was the spring of hope, it was the winter of despair.
        """
        
        let russian = """
        Русский:
        Это было лучшее из времен, это было худшее из времен; это была эпоха мудрости, это была эпоха глупости; это была пора веры, это была пора неверия; это был сезон Света, это был сезон Тьмы; это была весна надежды, это была зима отчаяния.
        """
        
        let greek = """
        Ελληνικά:
        Ήταν οι καλύτερες εποχές, ήταν οι χειρότερες εποχές· ήταν η εποχή της σοφίας, ήταν η εποχή της ανοησίας· ήταν η περίοδος της πίστης, ήταν η περίοδος της απιστίας· ήταν η εποχή του Φωτός, ήταν η εποχή του Σκότους· ήταν η άνοιξη της ελπίδας, ήταν ο χειμώνας της απελπισίας.
        """
        
        return [english, russian, greek].joined(separator: "\n\n")
    }
}

private func matrix_ortho(width: Float, height: Float) -> matrix_float4x4 {
    let sx: Float = width != 0 ? 2.0 / width : 0
    let sy: Float = height != 0 ? -2.0 / height : 0
    return matrix_float4x4(columns: (
        SIMD4<Float>(sx, 0, 0, 0),
        SIMD4<Float>(0, sy, 0, 0),
        SIMD4<Float>(0, 0, 1, 0),
        SIMD4<Float>(-1, 1, 0, 1)
    ))
}
