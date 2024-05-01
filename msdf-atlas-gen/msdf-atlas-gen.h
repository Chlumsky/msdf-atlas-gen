
#pragma once

/*
 * MULTI-CHANNEL SIGNED DISTANCE FIELD ATLAS GENERATOR
 * ---------------------------------------------------
 * A utility by Viktor Chlumsky, (c) 2020 - 2024
 * Generates compact bitmap font atlases using MSDFgen
 */

#include <msdfgen.h>
#include <msdfgen-ext.h>

#include "types.h"
#include "utf8.h"
#include "Rectangle.h"
#include "Padding.h"
#include "Charset.h"
#include "GlyphBox.h"
#include "GlyphGeometry.h"
#include "FontGeometry.h"
#include "RectanglePacker.h"
#include "rectangle-packing.h"
#include "Workload.h"
#include "size-selectors.h"
#include "bitmap-blit.h"
#include "AtlasStorage.h"
#include "BitmapAtlasStorage.h"
#include "TightAtlasPacker.h"
#include "GridAtlasPacker.h"
#include "AtlasGenerator.h"
#include "ImmediateAtlasGenerator.h"
#include "DynamicAtlas.h"
#include "glyph-generators.h"
#include "image-encode.h"
#include "image-save.h"
#include "artery-font-export.h"
#include "csv-export.h"
#include "json-export.h"
#include "shadron-preview-generator.h"
