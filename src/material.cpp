// Pygmalion - MakeHuman's materials (see material.h)
#include "material.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>

namespace fs = std::filesystem;

namespace Mat {

const char* SHADERS[] = {"litsphere", "normalmap", "phong", "skin", "toon", "xray"};
const int SHADER_COUNT = 6;
const char* CONFIG_DEFINES[] = {"DIFFUSE", "BUMPMAP", "NORMALMAP", "DISPLACEMENT", "SPECULARMAP", "VERTEX_COLOR", "TRANSPARENCYMAP", "AOMAP"};
static const char* MATERIAL_PARAMS[] = {"diffuse", "ambient", "specular", "emissive", "diffuseTexture", "bumpmapTexture", "bumpmapIntensity", "normalmapTexture",
                                        "normalmapIntensity", "displacementmapTexture", "specularmapTexture", "specularmapIntensity", "transparencymapTexture",
                                        "transparencymapIntensity", "aomapTexture", "aomapIntensity"};
bool isMaterialParam(const std::string& u) { for (const char* p : MATERIAL_PARAMS) if (u == p) return true; return false; }

static std::string shaderFolder;
void setShaderFolder(const std::string& f) { shaderFolder = f; }

// a shader's own files: the #ifdef names and the uniforms (with their defaults), as shader.py reads them
struct ShaderInfo { std::vector<std::string> defs; std::vector<Uniform> unis; };
static const ShaderInfo& info(const std::string& shader) {
    static std::map<std::string, ShaderInfo> cache;
    auto it = cache.find(shader);
    if (it != cache.end()) return it->second;
    ShaderInfo si;
    for (const char* kind : {"vertex", "fragment"}) {
        std::ifstream in(fs::u8path(shaderFolder) / (shader + "_" + kind + "_shader.txt"));
        std::string line;
        static const std::regex def(R"(^\s*#ifn?def\s+(\w+))"), uni(R"(^\s*uniform\s+(\w+)\s+(\w+)\s*(?:=\s*([^;]+))?;)");
        while (std::getline(in, line)) {
            std::smatch m;
            if (std::regex_search(line, m, def)) { if (std::find(si.defs.begin(), si.defs.end(), m[1].str()) == si.defs.end()) si.defs.push_back(m[1]); }
            else if (std::regex_search(line, m, uni)) {
                bool seen = false; for (const auto& u : si.unis) if (u.name == m[2].str()) seen = true;
                if (!seen) si.unis.push_back({m[2], m[1], m[3].matched ? m[3].str() : std::string()});
            }
        }
    }
    return cache[shader] = si;
}
const std::vector<std::string>& defineables(const std::string& shader) { return info(shader).defs; }
const std::vector<Uniform>& uniforms(const std::string& shader) { return info(shader).unis; }

std::set<std::string> Material::defines() const {
    std::set<std::string> d(shaderDefines.begin(), shaderDefines.end());
    if (shader.empty()) return d;
    auto can = [&](const char* def, const char* uni) {   // material.py's supportsX with a shader: the shader takes it
        const auto& ds = defineables(shader); const auto& us = uniforms(shader);
        if (std::find(ds.begin(), ds.end(), def) != ds.end()) return true;
        for (const auto& u : us) if (u.name == uni) return true;
        return false;
    };
    if (cfgVertexColors) d.insert("VERTEX_COLOR");
    if (cfgDiffuse && supportsDiffuse() && can("DIFFUSE", "diffuseTexture")) d.insert("DIFFUSE");
    const bool bump = cfgBump && supportsBump() && can("BUMPMAP", "bumpmapTexture"), normal = cfgNormal && supportsNormal() && can("NORMALMAP", "normalmapTexture");
    if (bump && !normal) d.insert("BUMPMAP");
    if (normal) d.insert("NORMALMAP");
    if (cfgDisplacement && supportsDisplacement() && can("DISPLACEMENT", "displacementmapTexture")) d.insert("DISPLACEMENT");
    if (cfgSpec && supportsSpecular() && can("SPECULARMAP", "specularmapTexture")) d.insert("SPECULARMAP");
    if (cfgTransparency && supportsTransparency() && can("TRANSPARENCYMAP", "transparencymapTexture")) d.insert("TRANSPARENCYMAP");
    if (cfgAo && supportsAo() && can("AOMAP", "aomapTexture")) d.insert("AOMAP");
    return d;
}
std::string Material::param(const std::string& n, const std::string& def) const { for (const auto& p : shaderParams) if (p.first == n) return p.second; return def; }
void Material::setParam(const std::string& n, const std::string& v) {
    for (auto& p : shaderParams) if (p.first == n) { p.second = v; return; }
    shaderParams.push_back({n, v});
}

static bool readBool(const std::string& v) { std::string l = v; for (auto& c : l) c = (char)tolower((unsigned char)c); return l == "yes" || l == "enabled" || l == "true"; }
static float clamp01(float v) { return std::max(0.0f, std::min(1.0f, v)); }
static bool numeric(const std::string& s) { char* e = nullptr; strtod(s.c_str(), &e); return !s.empty() && e && *e == 0; }

bool fromFile(const std::string& path, Material& m, const Resolver& resolve) {
    std::ifstream in(fs::u8path(path));
    if (!in) return false;
    m = Material();
    m.filename = fs::u8path(path).lexically_normal().u8string();
    const std::string folder = fs::u8path(path).parent_path().u8string();
    // the shader config: what the file sets (unset ones keep their defaults)
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::istringstream ls(line);
        std::vector<std::string> w; std::string x; while (ls >> x) w.push_back(x);
        if (w.empty() || w[0] == "#" || w[0] == "//" || w[0][0] == '#') continue;
        if (w.size() < 2) continue;
        auto rest = [&](size_t from) { std::string s; for (size_t k = from; k < w.size(); ++k) s += (k > from ? " " : "") + w[k]; return s; };
        auto col = [&](float* c) { for (int k = 0; k < 3 && k + 1 < (int)w.size(); ++k) c[k] = (float)atof(w[k + 1].c_str()); };
        auto tex = [&](std::string& t) { t = resolve(w[1], folder); if (t.empty()) t = w[1]; };   // not found: kept as written (thoroughFindFile)
        const std::string& k = w[0];
        const float f = (float)atof(w[1].c_str());
        if (k == "name") m.name = w[1];
        else if (k == "tag") { std::string t = rest(1); for (auto& c : t) c = (char)tolower((unsigned char)c); m.tags.push_back(t); }
        else if (k == "description") m.description = rest(1);
        else if (k == "ambientColor") col(m.ambient);
        else if (k == "diffuseColor") col(m.diffuse);
        else if (k == "specularColor") col(m.specular);
        else if (k == "shininess") m.shininess = clamp01(f);
        else if (k == "emissiveColor") col(m.emissive);
        else if (k == "viewPortColor") { col(m.viewPortColor); m.hasViewPortColor = true; }
        else if (k == "viewPortAlpha") { m.viewPortAlpha = clamp01(f); m.hasViewPortColor = true; }
        else if (k == "opacity") m.opacity = clamp01(f);
        else if (k == "translucency") m.translucency = clamp01(f);
        else if (k == "shadeless") m.shadeless = readBool(w[1]);
        else if (k == "wireframe") m.wireframe = readBool(w[1]);
        else if (k == "transparent") m.transparent = readBool(w[1]);
        else if (k == "alphaToCoverage") m.alphaToCoverage = readBool(w[1]);
        else if (k == "backfaceCull") m.backfaceCull = readBool(w[1]);
        else if (k == "depthless") m.depthless = readBool(w[1]);
        else if (k == "castShadows") m.castShadows = readBool(w[1]);
        else if (k == "receiveShadows") m.receiveShadows = readBool(w[1]);
        else if (k == "autoBlendSkin") m.autoBlendSkin = readBool(w[1]);
        else if (k == "diffuseTexture") tex(m.diffuseTexture);
        else if (k == "bumpmapTexture") tex(m.bumpMapTexture);
        else if (k == "bumpmapIntensity") m.bumpMapIntensity = clamp01(f);
        else if (k == "normalmapTexture") tex(m.normalMapTexture);
        else if (k == "normalmapIntensity") m.normalMapIntensity = clamp01(f);
        else if (k == "displacementmapTexture") tex(m.displacementMapTexture);
        else if (k == "displacementmapIntensity") m.displacementMapIntensity = clamp01(f);
        else if (k == "specularmapTexture") tex(m.specularMapTexture);
        else if (k == "specularmapIntensity") m.specularMapIntensity = clamp01(f);
        else if (k == "transparencymapTexture") tex(m.transparencyMapTexture);
        else if (k == "transparencymapIntensity") m.transparencyMapIntensity = clamp01(f);
        else if (k == "aomapTexture") tex(m.aoMapTexture);
        else if (k == "aomapIntensity") m.aoMapIntensity = clamp01(f);
        else if (k == "sssEnabled") m.sssEnabled = readBool(w[1]);
        else if (k == "sssRScale") m.sssRScale = std::max(0.0f, f);
        else if (k == "sssGScale") m.sssGScale = std::max(0.0f, f);
        else if (k == "sssBScale") m.sssBScale = std::max(0.0f, f);
        else if (k == "shader") m.shader = fs::u8path(w[1]).filename().u8string();
        else if (k == "uvMap") { m.uvMap = resolve(w[1], folder); if (fs::u8path(m.uvMap).filename() == "default.mhuv") m.uvMap.clear(); }
        else if (k == "shaderParam" && w.size() >= 3) {
            if (isMaterialParam(w[1])) continue;   // protected: set through the material's own settings
            std::string v = rest(2);
            if (w.size() == 3 && !numeric(w[2])) { const std::string p = resolve(w[2], folder); if (!p.empty()) v = p; }   // a path
            m.setParam(w[1], v);
        }
        else if (k == "shaderDefine") { bool cfg = false; for (const char* d : CONFIG_DEFINES) if (w[1] == d) cfg = true; if (!cfg && std::find(m.shaderDefines.begin(), m.shaderDefines.end(), w[1]) == m.shaderDefines.end()) m.shaderDefines.push_back(w[1]); }
        else if (k == "shaderConfig" && w.size() >= 3) {
            const bool b = readBool(w[2]); const std::string& o = w[1];
            if (o == "diffuse") m.cfgDiffuse = b; else if (o == "bump") m.cfgBump = b; else if (o == "normal") m.cfgNormal = b;
            else if (o == "displacement") m.cfgDisplacement = b; else if (o == "spec") m.cfgSpec = b; else if (o == "vertexColors") m.cfgVertexColors = b;
            else if (o == "transparency") m.cfgTransparency = b; else if (o == "ambientOcclusion") m.cfgAo = b;
        }
    }
    std::sort(m.shaderDefines.begin(), m.shaderDefines.end());
    return true;
}

// numbers as Python's str() writes them: the shortest form that reads back, and always a decimal point
static std::string num(float f) {
    char b[48];
    for (int p = 1; p <= 9; ++p) { snprintf(b, sizeof b, "%.*g", p, (double)f); if ((float)strtod(b, nullptr) == f) break; }
    std::string s = b;
    if (s.find_first_of(".eEn") == std::string::npos) s += ".0";
    return s;
}
static std::string col(const float* c) { return num(c[0]) + " " + num(c[1]) + " " + num(c[2]); }
static std::string tf(bool b) { return b ? "True" : "False"; }
// a path for the file: relative to its folder when under it (getJailedPath), else as it is with forward slashes
static std::string rel(const std::string& p, const fs::path& folder) {
    if (!fs::u8path(p).is_absolute()) return fs::u8path(p).generic_u8string();   // a name that wasn't found: as it was written
    std::error_code ec;
    const fs::path a = fs::absolute(fs::u8path(p), ec).lexically_normal(), f = fs::absolute(folder, ec).lexically_normal();
    const fs::path r = a.lexically_relative(f);
    if (!r.empty() && r.native()[0] != '.') return r.generic_u8string();
    return a.generic_u8string();
}

bool toFile(const Material& m, const std::string& path, std::string& err, const std::vector<std::string>& comments) {
    std::ofstream f(fs::u8path(path));
    if (!f) { err = "Couldn't write " + path; return false; }
    const fs::path dir = fs::u8path(path).parent_path();
    f << "# Material definition for " << m.name << "\n";
    for (const auto& c : comments) f << ((c.rfind("//", 0) == 0 || c.rfind("#", 0) == 0) ? c : "# " + c) << "\n";
    f << "\n";
    f << "name " << m.name << "\n";
    f << "description " << m.description << "\n";
    f << "ambientColor " << col(m.ambient) << "\n";
    f << "diffuseColor " << col(m.diffuse) << "\n";
    f << "specularColor " << col(m.specular) << "\n";
    f << "shininess " << num(m.shininess) << "\n";
    f << "emissiveColor " << col(m.emissive) << "\n";
    f << "opacity " << num(m.opacity) << "\n";
    f << "translucency " << num(m.translucency) << "\n\n";
    f << "shadeless " << tf(m.shadeless) << "\n";
    f << "wireframe " << tf(m.wireframe) << "\n";
    f << "transparent " << tf(m.transparent) << "\n";
    f << "alphaToCoverage " << tf(m.alphaToCoverage) << "\n";
    f << "backfaceCull " << tf(m.backfaceCull) << "\n";
    f << "depthless " << tf(m.depthless) << "\n\n";
    f << "castShadows " << tf(m.castShadows) << "\n\n";
    f << "receiveShadows " << tf(m.receiveShadows) << "\n\n";
    bool hasTex = false;
    auto texLine = [&](const char* key, const std::string& t, const char* ikey, float inten) {
        if (t.empty()) return;
        f << key << " " << rel(t, dir) << "\n"; if (ikey) f << ikey << " " << num(inten) << "\n"; hasTex = true;
    };
    texLine("diffuseTexture", m.diffuseTexture, nullptr, 0);
    texLine("bumpmapTexture", m.bumpMapTexture, "bumpmapIntensity", m.bumpMapIntensity);
    texLine("normalmapTexture", m.normalMapTexture, "normalmapIntensity", m.normalMapIntensity);
    texLine("displacementmapTexture", m.displacementMapTexture, "displacementmapIntensity", m.displacementMapIntensity);
    texLine("specularmapTexture", m.specularMapTexture, "specularmapIntensity", m.specularMapIntensity);
    texLine("transparencymapTexture", m.transparencyMapTexture, "transparencymapIntensity", m.transparencyMapIntensity);
    texLine("aomapTexture", m.aoMapTexture, "aomapIntensity", m.aoMapIntensity);
    if (hasTex) f << "\n";
    if (m.sssEnabled) {
        f << "# Sub-surface scattering parameters\n";
        f << "sssEnabled " << tf(m.sssEnabled) << "\n" << "sssRScale " << num(m.sssRScale) << "\n" << "sssGScale " << num(m.sssGScale) << "\n" << "sssBScale " << num(m.sssBScale) << "\n\n";
    }
    if (!m.uvMap.empty()) f << "uvMap " << rel(m.uvMap, dir) << "\n\n";
    if (!m.shader.empty()) f << "shader data/shaders/glsl/" << m.shader << "\n\n";
    bool any = false;
    for (const auto& [n, v] : m.shaderParams) {
        if (isMaterialParam(n)) continue;
        any = true;
        std::istringstream vs(v); std::vector<std::string> parts; std::string p; while (vs >> p) parts.push_back(p);
        if (parts.size() == 1 && !numeric(parts[0])) f << "shaderParam " << n << " " << rel(v, dir) << "\n";
        else f << "shaderParam " << n << " " << v << "\n";
    }
    if (any) f << "\n";
    any = false;
    for (const auto& d : m.shaderDefines) { any = true; f << "shaderDefine " << d << "\n"; }
    if (any) f << "\n";
    f << "shaderConfig diffuse " << tf(m.cfgDiffuse) << "\nshaderConfig bump " << tf(m.cfgBump) << "\nshaderConfig normal " << tf(m.cfgNormal)
      << "\nshaderConfig displacement " << tf(m.cfgDisplacement) << "\nshaderConfig spec " << tf(m.cfgSpec) << "\nshaderConfig vertexColors " << tf(m.cfgVertexColors)
      << "\nshaderConfig transparency " << tf(m.cfgTransparency) << "\nshaderConfig ambientOcclusion " << tf(m.cfgAo) << "\n";
    return (bool)f;
}

}
