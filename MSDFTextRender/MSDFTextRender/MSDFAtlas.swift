import CoreGraphics
import Foundation

struct MSDFAtlas: Decodable {
    private enum CodingKeys: String, CodingKey {
        case atlas
        case metrics
        case glyphs
    }

    struct AtlasInfo: Decodable {
        let distanceRange: Float
        let distanceRangeMiddle: Float
        let size: Float
        let width: Int
        let height: Int
    }

    struct GlyphBounds: Decodable {
        let left: Float
        let bottom: Float
        let right: Float
        let top: Float
    }

    struct GlyphDescriptor: Decodable {
        let index: UInt32
        let advance: Float
        let planeBounds: GlyphBounds?
        let atlasBounds: GlyphBounds?
    }

    struct Metrics: Decodable {
        let emSize: Float
        let lineHeight: Float
        let ascender: Float
        let descender: Float
    }

    let atlas: AtlasInfo
    let metrics: Metrics
    let glyphs: [GlyphDescriptor]

    private var glyphMap: [CGGlyph: GlyphDescriptor] = [:]

    var textureSize: CGSize {
        CGSize(width: atlas.width, height: atlas.height)
    }

    var pxRange: SIMD2<Float> {
        SIMD2<Float>(
            repeating: atlas.distanceRange,
        )
    }

    func descriptor(for glyph: CGGlyph) -> GlyphDescriptor? {
        glyphMap[glyph]
    }

    mutating func buildGlyphMap() {
        glyphMap.removeAll(keepingCapacity: true)
        glyphs.forEach { glyphMap[CGGlyph($0.index)] = $0 }
    }

    static func load(from url: URL) throws -> MSDFAtlas {
        let data = try Data(contentsOf: url)
        var atlas = try JSONDecoder().decode(MSDFAtlas.self, from: data)
        atlas.buildGlyphMap()
        return atlas
    }
}
