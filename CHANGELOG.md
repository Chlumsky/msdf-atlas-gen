
## Version 1.1 (2020-10-18)

- Updated to MSDFgen 1.8.
- Glyph geometry is now preprocessed by Skia to resolve irregularities which were previously unsupported and caused artifacts.
    - The scanline pass and overlapping contour mode is made obsolete by this step and has been disabled by default. The preprocess step can be disabled by the new `-nopreprocess` switch and the former enabled by `-scanline` and `-overlap` respectively.
    - The project can be built without the Skia library, forgoing the geometry preprocessing feature. This is controlled by the macro definition `MSDFGEN_USE_SKIA`.
- Glyphs can now also be loaded by glyph index rather than Unicode values. In the standalone version, a set of glyphs can be passed by `-glyphset` in place of `-charset`.
- Glyphs not present in the font should now be correctly skipped instead of producing a placeholder symbol.
- Added `-threads` argument to set the number of concurrent threads used during distance field generation.

### Version 1.0.1 (2020-03-09)

- Updated to MSDFgen 1.7.1.

## Version 1.0 (2020-03-08)

- Initial release.
