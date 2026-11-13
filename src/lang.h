// Pygmalion - language packs. English is the main language, written straight into the code. Each other language is
// data/lang/<code>.txt, one "English text = translation" per line (Pygmalion's own words), and MakeHuman's own
// translation of its words (data/languages/<name>.json: slider labels, groups, tabs) where MakeHuman has that language.
// T("text") gives the translation, or the English when there's none; "label##id" keeps its id. CJK languages say which
// Windows font to merge in (only for that language).
#pragma once
#include <string>
#include <vector>

namespace Lang {

struct Info {
    const char* code;      // data/lang/<code>.txt ("en": none)
    const char* english;   // "Japanese"
    const char* native;    // "日本語" (UTF-8)
    const char* font;      // a CJK font to merge in, or nullptr
    int glyphs;            // which glyph range that font needs: 0 none, 1 Japanese, 2 Chinese, 3 Korean
    const char* makehuman; // MakeHuman's translation file (data/languages/<this>.json), or nullptr
    bool full;             // Pygmalion's own words translated too (else only MakeHuman's)
};

const std::vector<Info>& all();
const Info& current();
// loads the language (an unknown code or missing files: English); data: the data folder
void load(const std::string& data, const std::string& code);

const char* T(const char* english);
inline const char* T(const std::string& english) { return T(english.c_str()); }
// a label whose id stays the same whatever the language ("Translated###english"), for tabs and other remembered items
const char* TID(const char* english);

}  // namespace Lang

using Lang::T;
using Lang::TID;
