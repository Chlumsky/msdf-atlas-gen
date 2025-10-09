//
//  MSDFTextMesh.swift
//  MSDFTextRender
//
//  Created by Codex on 10/8/25.
//

import CoreText
import Metal
import simd
import UIKit

struct MSDFGlyphVertex {
    var position: SIMD3<Float>
    var texCoord: SIMD2<Float>
}

struct MSDFTextMesh {
    let vertexBuffer: MTLBuffer
    let indexBuffer: MTLBuffer
    let indexCount: Int
    let bounds: CGSize
}

final class MSDFTextMeshBuilder {
    private let device: MTLDevice
    private let atlas: MSDFAtlas
    private var font: CTFont

    init(device: MTLDevice, atlas: MSDFAtlas, font: CTFont) {
        self.device = device
        self.atlas = atlas
        self.font = font
    }
    
    func updateFont(_ font: CTFont) {
        self.font = font
    }

    func buildMesh(for text: String,
                   in frameSize: CGSize,
                   margin: CGFloat,
                   scale: CGFloat) -> MSDFTextMesh? {
        guard frameSize.width > 0, frameSize.height > 0 else { return nil }
        let scaleValue = max(scale, 0.0001)
        let layoutWidth = max(frameSize.width - margin * 2.0, 1.0)
        let layoutHeight = max(frameSize.height - margin * 2.0, 1.0)
        let layoutRect = CGRect(origin: .zero, size: CGSize(width: layoutWidth, height: layoutHeight))

        let attrString = NSMutableAttributedString(string: text)
        let fullRange = NSRange(location: 0, length: attrString.length)
        attrString.addAttribute(NSAttributedString.Key(kCTFontAttributeName as String),
                                value: font,
                                range: fullRange)

        let paragraphStyle = NSMutableParagraphStyle()
        paragraphStyle.alignment = .left
        paragraphStyle.lineBreakMode = .byWordWrapping
        attrString.addAttribute(.paragraphStyle, value: paragraphStyle, range: fullRange)

        let framesetter = CTFramesetterCreateWithAttributedString(attrString)
        let path = CGPath(rect: layoutRect, transform: nil)
        let frame = CTFramesetterCreateFrame(framesetter, CFRangeMake(0, attrString.length), path, nil)

        let framePath = CTFrameGetPath(frame)
        let frameBoundingRect = framePath.boundingBox

        guard let lines = CTFrameGetLines(frame) as? [CTLine], !lines.isEmpty else {
            return nil
        }

        let lineOrigins = UnsafeMutablePointer<CGPoint>.allocate(capacity: lines.count)
        defer { lineOrigins.deallocate() }
        CTFrameGetLineOrigins(frame, CFRange(location: 0, length: 0), lineOrigins)

        UIGraphicsBeginImageContext(CGSize(width: 1, height: 1))
        guard let context = UIGraphicsGetCurrentContext() else {
            UIGraphicsEndImageContext()
            return nil
        }
        defer { UIGraphicsEndImageContext() }

        var vertices: [MSDFGlyphVertex] = []
        var indices: [UInt32] = []

        var minPosition = SIMD2<Float>(repeating: Float.greatestFiniteMagnitude)
        var maxPosition = SIMD2<Float>(repeating: -Float.greatestFiniteMagnitude)

        let atlasWidth = Float(atlas.textureSize.width)
        let atlasHeight = Float(atlas.textureSize.height)

        for (lineIndex, line) in lines.enumerated() {
            let lineOrigin = lineOrigins[lineIndex]
            guard let runs = CTLineGetGlyphRuns(line) as? [CTRun] else {
                continue
            }

            for run in runs {
                let glyphCount = CTRunGetGlyphCount(run)
                guard glyphCount > 0 else { continue }

                var glyphBuffer = [CGGlyph](repeating: 0, count: glyphCount)
                var positionBuffer = [CGPoint](repeating: .zero, count: glyphCount)

                CTRunGetGlyphs(run, CFRange(location: 0, length: 0), &glyphBuffer)
                CTRunGetPositions(run, CFRange(location: 0, length: 0), &positionBuffer)

                for glyphIndex in 0..<glyphCount {
                    let glyph = glyphBuffer[glyphIndex]

                    guard let descriptor = atlas.descriptor(for: glyph),
                          let atlasBounds = descriptor.atlasBounds else {
                        continue
                    }

                    var glyphRect = CTRunGetImageBounds(run, context, CFRange(location: glyphIndex, length: 1))

                    if glyphRect.isNull || glyphRect.isEmpty {
                        continue
                    }

                    let glyphOrigin = positionBuffer[glyphIndex]
                    let boundsTransX = frameBoundingRect.origin.x + lineOrigin.x
                    let boundsTransY = frameBoundingRect.origin.y + frameBoundingRect.height - lineOrigin.y + glyphOrigin.y
                    let transform = CGAffineTransform(a: 1, b: 0, c: 0, d: -1, tx: boundsTransX, ty: boundsTransY)
                    glyphRect = glyphRect.applying(transform)

                    let minX = Float(glyphRect.minX)
                    let maxX = Float(glyphRect.maxX)
                    let minY = Float(glyphRect.minY)
                    let maxY = Float(glyphRect.maxY)

                    minPosition.x = min(minPosition.x, minX)
                    minPosition.y = min(minPosition.y, minY)
                    maxPosition.x = max(maxPosition.x, maxX)
                    maxPosition.y = max(maxPosition.y, maxY)

                    let u0 = Float(atlasBounds.left) / atlasWidth
                    let u1 = Float(atlasBounds.right) / atlasWidth
                    let v0 = 1.0 - Float(atlasBounds.top) / atlasHeight
                    let v1 = 1.0 - Float(atlasBounds.bottom) / atlasHeight

                    let baseIndex = UInt32(vertices.count)
                    vertices.append(MSDFGlyphVertex(position: SIMD3<Float>(minX, maxY, 0),
                                                    texCoord: SIMD2<Float>(u0, v1)))
                    vertices.append(MSDFGlyphVertex(position: SIMD3<Float>(minX, minY, 0),
                                                    texCoord: SIMD2<Float>(u0, v0)))
                    vertices.append(MSDFGlyphVertex(position: SIMD3<Float>(maxX, minY, 0),
                                                    texCoord: SIMD2<Float>(u1, v0)))
                    vertices.append(MSDFGlyphVertex(position: SIMD3<Float>(maxX, maxY, 0),
                                                    texCoord: SIMD2<Float>(u1, v1)))

                    indices.append(contentsOf: [
                        baseIndex,
                        baseIndex + 1,
                        baseIndex + 2,
                        baseIndex,
                        baseIndex + 2,
                        baseIndex + 3,
                    ])
                }
            }
        }

        guard !vertices.isEmpty, !indices.isEmpty else {
            return nil
        }

        let translate = SIMD2<Float>(Float(margin) - minPosition.x, Float(margin) - minPosition.y)
        for vertexIndex in vertices.indices {
            vertices[vertexIndex].position.x += translate.x
            vertices[vertexIndex].position.y += translate.y
        }

        let layoutWidthFinal = (maxPosition.x - minPosition.x) + Float(margin * 2.0)
        let layoutHeightFinal = (maxPosition.y - minPosition.y) + Float(margin * 2.0)

        let scaleFactor = Float(scaleValue)
        if scaleFactor != 1.0 {
            for vertexIndex in vertices.indices {
                vertices[vertexIndex].position.x *= scaleFactor
                vertices[vertexIndex].position.y *= scaleFactor
            }
        }

        let finalWidth = CGFloat(layoutWidthFinal) * scaleValue
        let finalHeight = CGFloat(layoutHeightFinal) * scaleValue

        guard let vertexBuffer = device.makeBuffer(bytes: vertices,
                                                   length: vertices.count * MemoryLayout<MSDFGlyphVertex>.stride,
                                                   options: .storageModeShared),
              let indexBuffer = device.makeBuffer(bytes: indices,
                                                  length: indices.count * MemoryLayout<UInt32>.stride,
                                                  options: .storageModeShared) else {
            return nil
        }

        vertexBuffer.label = "MSDF Text Vertices"
        indexBuffer.label = "MSDF Text Indices"

        return MSDFTextMesh(vertexBuffer: vertexBuffer,
                            indexBuffer: indexBuffer,
                            indexCount: indices.count,
                            bounds: CGSize(width: finalWidth,
                                           height: finalHeight))
    }
}
