
#pragma once

#include <cstdlib>
#include <set>
#include "types.h"

#ifndef MSDF_ATLAS_PUBLIC
#define MSDF_ATLAS_PUBLIC
#endif

namespace msdf_atlas {

/// Represents a set of Unicode codepoints (characters)
class Charset {

public:
    /// The set of the 95 printable ASCII characters
    static MSDF_ATLAS_PUBLIC const Charset ASCII;

    /// Adds a codepoint
    void add(unicode_t cp);
    /// Removes a codepoint
    void remove(unicode_t cp);

    size_t size() const;
    bool empty() const;
    std::set<unicode_t>::const_iterator begin() const;
    std::set<unicode_t>::const_iterator end() const;

    /// Load character set from a text file with compliant syntax
    bool load(const char *filename, bool disableCharLiterals = false);
    /// Parse character set from a string with compliant syntax
    bool parse(const char *str, size_t strLength, bool disableCharLiterals = false);

private:
    std::set<unicode_t> codepoints;

};

}
