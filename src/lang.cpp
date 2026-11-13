// Pygmalion - language packs (see lang.h)
#include "lang.h"
#include "json.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_map>

namespace fs = std::filesystem;

namespace Lang {

// my eleven (everything translated) and MakeHuman's other languages (its own words translated, Pygmalion's in English)
static const std::vector<Info> langs = {
    {"en", "English", "English", nullptr, 0, nullptr, true},
    {"ja", "Japanese", "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E", "C:/Windows/Fonts/YuGothM.ttc", 1, "japanese_generic", true},
    {"zh", "Chinese", "\xE4\xB8\xAD\xE6\x96\x87", "C:/Windows/Fonts/msyh.ttc", 2, "chinese_china", true},
    {"es", "Spanish", "Espa\xC3\xB1ol", nullptr, 0, "spanish_generic", true},
    {"ko", "Korean", "\xED\x95\x9C\xEA\xB5\xAD\xEC\x96\xB4", "C:/Windows/Fonts/malgun.ttf", 3, "korean", true},
    {"fr", "French", "Fran\xC3\xA7" "ais", nullptr, 0, "french_generic", true},
    {"de", "German", "Deutsch", nullptr, 0, "german_generic", true},
    {"ru", "Russian", "\xD0\xA0\xD1\x83\xD1\x81\xD1\x81\xD0\xBA\xD0\xB8\xD0\xB9", nullptr, 0, "russian_generic", true},
    {"sr", "Serbian", "\xD0\xA1\xD1\x80\xD0\xBF\xD1\x81\xD0\xBA\xD0\xB8", nullptr, 0, nullptr, true},
    {"af", "Afrikaans", "Afrikaans", nullptr, 0, nullptr, true},
    {"yua", "Yucatec Maya", "Maaya t'aan", nullptr, 0, nullptr, true},
    {"pdc", "Pennsylvania Dutch", "Deitsch", nullptr, 0, nullptr, true},
    // MakeHuman's own other translations (Arabic left out: this interface can't join its letters or run right to left)
    {"cs", "Czech", "\xC4\x8C" "e\xC5\xA1tina", nullptr, 0, "czech_generic", false},
    {"nl", "Dutch", "Nederlands", nullptr, 0, "dutch_generic", false},
    {"et", "Estonian", "Eesti", nullptr, 0, "estonian_generic", false},
    {"hu", "Hungarian", "Magyar", nullptr, 0, "hungarian_generic", false},
    {"it", "Italian", "Italiano", nullptr, 0, "italian", false},
    {"pl", "Polish", "Polski", nullptr, 0, "polish_generic", false},
    {"pt", "Portuguese", "Portugu\xC3\xAAs", nullptr, 0, "portuguese_generic", false},
    {"ro", "Romanian", "Rom\xC3\xA2n\xC4\x83", nullptr, 0, "romanian_romania", false},
    {"sv", "Swedish", "Svenska", nullptr, 0, "swedish_sweden", false},
    {"tr", "Turkish", "T\xC3\xBCrk\xC3\xA7" "e", nullptr, 0, "turkish_generic", false},
    {"uk", "Ukrainian", "\xD0\xA3\xD0\xBA\xD1\x80\xD0\xB0\xD1\x97\xD0\xBD\xD1\x81\xD1\x8C\xD0\xBA\xD0\xB0", nullptr, 0, "ukrainian", false},
};

static size_t cur = 0;
static std::unordered_map<std::string, std::string> table;      // Pygmalion's words
static std::unordered_map<std::string, std::string> mhTable;    // MakeHuman's words
static std::unordered_map<std::string, std::string> composed;   // "label##id" and TID results, kept so the pointers last

const std::vector<Info>& all() { return langs; }
const Info& current() { return langs[cur]; }

static std::string trim(std::string s) {
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r' || s.back() == '\n')) s.pop_back();
    size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
    return s.substr(i);
}
static std::string unescape(const std::string& s) {   // "\n" in a pack is a new line
    std::string o;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size() && s[i + 1] == 'n') { o += '\n'; ++i; }
        else o += s[i];
    }
    return o;
}
static std::string readFile(const fs::path& p) {
    std::ifstream in(p, std::ios::binary); std::ostringstream ss; ss << in.rdbuf();
    std::string t = ss.str();
    if (t.size() >= 3 && (unsigned char)t[0] == 0xEF && (unsigned char)t[1] == 0xBB && (unsigned char)t[2] == 0xBF) t.erase(0, 3);
    return t;
}

void load(const std::string& data, const std::string& code) {
    table.clear(); mhTable.clear(); composed.clear();
    cur = 0;
    for (size_t i = 0; i < langs.size(); ++i) if (code == langs[i].code) cur = i;
    if (cur == 0) return;
    const Info& L = langs[cur];
    // Pygmalion's own pack
    const std::string text = readFile(fs::u8path(data) / "lang" / (std::string(L.code) + ".txt"));
    size_t pos = 0;
    while (pos < text.size()) {
        size_t end = text.find('\n', pos); if (end == std::string::npos) end = text.size();
        const std::string t = trim(text.substr(pos, end - pos)); pos = end + 1;
        if (t.empty() || t[0] == '#') continue;
        const size_t eq = t.find(" = "); if (eq == std::string::npos) continue;
        const std::string key = unescape(trim(t.substr(0, eq))), value = unescape(trim(t.substr(eq + 3)));
        if (!key.empty() && !value.empty()) table[key] = value;
    }
    // MakeHuman's own
    if (L.makehuman) {
        const std::string js = readFile(fs::u8path(data) / "languages" / (std::string(L.makehuman) + ".json"));
        Json::Value v; size_t at = 0;
        if (Json::parse(js, at, v) && v.type == Json::Value::OBJ)
            for (const auto& [k, val] : v.o) if (val.type == Json::Value::STR && !val.s.empty()) mhTable[k] = val.s;
    }
}

static const char* plain(const char* english) {
    auto it = table.find(english);
    if (it != table.end()) return it->second.c_str();
    auto m = mhTable.find(english);
    if (m != mhTable.end()) return m->second.c_str();
    return nullptr;
}

const char* T(const char* english) {
    if (cur == 0 || !english || !*english) return english;
    if (const char* t = plain(english)) return t;
    // "label##id": the label translated, the id kept
    const char* hash = strstr(english, "##");
    if (hash && hash != english) {
        const std::string label(english, hash);
        const char* t = plain(label.c_str());
        if (!t) return english;
        auto& c = composed[english];
        if (c.empty()) c = std::string(t) + hash;
        return c.c_str();
    }
    return english;
}

const char* TID(const char* english) {
    if (cur == 0 || !english) return english;
    const char* t = plain(english);
    if (!t) return english;
    auto& c = composed[std::string("###") + english];
    if (c.empty()) c = std::string(t) + "###" + english;
    return c.c_str();
}

}  // namespace Lang
