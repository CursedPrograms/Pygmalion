// Pygmalion - Socket: MakeHuman's socket server (plugins/8_server_socket + MHAPI's mesh, modifier and asset calls;
// AGPL-3.0). What Blender's MPFB asks for, answered the way MakeHuman answers it: JSON {"function","error","params",
// "data"}, or the raw arrays (float32 / uint32, little-endian) for the mesh calls. Included by gui.cpp.

static std::vector<std::string> sockLog;   // (sockCfg, sockHost and sockPort are with the settings in gui.cpp)

namespace SockOps {
// a small JSON writer: numbers rounded to 6 places, as MHApiEncoder does
struct J {
    static std::string num(double v) {
        if (!std::isfinite(v)) return "0";
        char b[64]; snprintf(b, sizeof b, "%.6f", v);
        std::string t = b; t.erase(t.find_last_not_of('0') + 1); if (!t.empty() && t.back() == '.') t += '0';
        return t == "-0.0" ? "0.0" : t;
    }
    static std::string str(const std::string& v) {
        std::string o = "\"";
        for (unsigned char c : v) {
            if (c == '"' || c == '\\') { o += '\\'; o += (char)c; }
            else if (c == '\n') o += "\\n"; else if (c == '\r') o += "\\r"; else if (c == '\t') o += "\\t";
            else if (c < 0x20) { char b[8]; snprintf(b, sizeof b, "\\u%04x", c); o += b; }
            else o += (char)c;
        }
        return o + "\"";
    }
};
static std::string list(const std::vector<double>& v) {
    std::string o = "[";
    for (size_t i = 0; i < v.size(); ++i) { if (i) o += ", "; o += J::num(v[i]); }
    return o + "]";
}
static std::string lower(std::string s) { for (auto& c : s) c = (char)tolower((unsigned char)c); return s; }

static std::string kindName(const std::string& k) {   // MakeHuman's proxy type names
    if (k == "proxymeshes") return "Proxymeshes";
    std::string t = k; if (!t.empty()) t[0] = (char)toupper((unsigned char)t[0]); return t;
}
// the body brought up to date (targets, skeleton, pose), as MakeHuman's human is when a call comes in
static void refresh() { Human::apply(body); if (!rig.empty()) Skel::build(rig, body.pos); if (!chosen.empty()) Skel::build(chosen, body.pos); repose(); }
static const std::vector<float>& shownPos() { return body.shown.empty() ? body.pos : body.shown; }
static std::string bytesOf(const void* p, size_t n) { return std::string((const char*)p, n); }

// MakeHuman's material as a hash: its own settings with their defaults, then any other keys in the file as they are
static std::string materialHash(const std::string& mhmat) {
    std::map<std::string, std::string> fileKeys; std::vector<std::string> order;
    std::string name = fs::u8path(mhmat).stem().u8string(), desc;
    std::ifstream in(fs::u8path(mhmat)); std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const size_t a = line.find_first_not_of(" \t"); if (a == std::string::npos || line[a] == '#' || line[a] == '/') continue;
        std::string k, v; { std::istringstream ls(line.substr(a)); ls >> k; std::getline(ls >> std::ws, v); }
        bool alpha = !k.empty(); for (char c : k) if (!isalpha((unsigned char)c)) alpha = false;
        if (!alpha) continue;
        const std::string lk = lower(k);
        if (!fileKeys.count(lk)) order.push_back(k);
        fileKeys[lk] = v;
        if (lk == "name") name = v; else if (lk == "description") desc = v;
    }
    if (desc.empty()) desc = name + " material";
    auto get = [&](const char* k) -> const std::string* { auto it = fileKeys.find(lower(k)); return it == fileKeys.end() ? nullptr : &it->second; };
    auto color = [&](const char* k, const char* def) { const std::string* v = get(k); std::istringstream ls(v ? *v : def); double r = 0, g = 0, b = 0; ls >> r >> g >> b; return list({r, g, b}); };
    auto real = [&](const char* k, double def) { const std::string* v = get(k); return J::num(v ? atof(v->c_str()) : def); };
    auto flag = [&](const char* k, bool def) { const std::string* v = get(k); bool b = def; if (v) b = lower(*v) == "true" || *v == "1"; return std::string(b ? "true" : "false"); };
    auto tex = [&](const char* k) {
        const std::string* v = get(k); if (!v || v->empty() || *v == "None") return J::str("");
        fs::path p = fs::u8path(*v); std::error_code ec;
        if (!p.is_absolute()) p = fs::u8path(mhmat).parent_path() / p;
        if (!fs::exists(p, ec)) { const std::string a2 = Human::absPath(body, *v); if (!a2.empty()) p = fs::u8path(a2); }
        return J::str(fs::absolute(p, ec).lexically_normal().u8string());
    };
    std::error_code ec; const std::string abs = fs::absolute(fs::u8path(mhmat), ec).lexically_normal().u8string();
    std::string o = "{";
    auto add = [&](const std::string& k, const std::string& v) { if (o.size() > 1) o += ", "; o += J::str(k) + ": " + v; };
    add("name", J::str(name)); add("description", J::str(desc)); add("materialFile", J::str(abs));
    add("ambientColor", color("ambientColor", "1 1 1")); add("diffuseColor", color("diffuseColor", "1 1 1"));
    add("specularColor", color("specularColor", "1 1 1")); add("emissiveColor", color("emissiveColor", "0 0 0"));
    add("diffuseTexture", tex("diffuseTexture")); add("bumpMapTexture", tex("bumpmapTexture")); add("normalMapTexture", tex("normalmapTexture"));
    add("displacementMapTexture", tex("displacementmapTexture")); add("specularMapTexture", tex("specularmapTexture"));
    add("transparencyMapTexture", tex("transparencymapTexture")); add("aoMapTexture", tex("aomapTexture"));
    add("bumpMapIntensity", real("bumpmapIntensity", 1)); add("normalMapIntensity", real("normalmapIntensity", 1));
    add("displacementMapIntensity", real("displacementmapIntensity", 1)); add("specularMapIntensity", real("specularmapIntensity", 1));
    add("transparencyMapIntensity", real("transparencymapIntensity", 1)); add("aoMapIntensity", real("aomapIntensity", 1));
    add("sssEnabled", flag("sssEnabled", false)); add("sssRScale", real("sssRScale", 0)); add("sssGScale", real("sssGScale", 0)); add("sssBScale", real("sssBScale", 0));
    { const std::string* uv = get("uvMap"); add("uvMap", J::str(uv ? *uv : "")); }
    add("shininess", real("shininess", 0.2)); add("opacity", real("opacity", 1)); add("translucency", real("translucency", 0));
    add("shadeless", flag("shadeless", false)); add("wireframe", flag("wireframe", false)); add("transparent", flag("transparent", false));
    add("alphaToCoverage", flag("alphaToCoverage", true)); add("backfaceCull", flag("backfaceCull", true)); add("depthless", flag("depthless", false));
    add("castShadows", flag("castShadows", true)); add("receiveShadows", flag("receiveShadows", true)); add("autoBlendSkin", flag("autoBlendSkin", false));
    if (get("viewPortColor")) { add("viewPortColor", color("viewPortColor", "0 0 0")); add("viewPortAlpha", real("viewPortAlpha", 1)); }
    // the rest, as found ("let the recipient decide")
    std::set<std::string> defined;
    for (const char* k : {"name", "description", "materialfile", "ambientcolor", "diffusecolor", "specularcolor", "emissivecolor", "diffusetexture",
                          "bumpmaptexture", "normalmaptexture", "displacementmaptexture", "specularmaptexture", "transparencymaptexture", "aomaptexture",
                          "bumpmapintensity", "normalmapintensity", "displacementmapintensity", "specularmapintensity", "transparencymapintensity",
                          "aomapintensity", "sssenabled", "sssrscale", "sssgscale", "sssbscale", "uvmap", "shininess", "opacity", "translucency",
                          "shadeless", "wireframe", "transparent", "alphatocoverage", "backfacecull", "depthless", "castshadows", "receiveshadows",
                          "autoblendskin", "viewportcolor", "viewportalpha"}) defined.insert(k);
    for (const auto& k : order) { const std::string lk = lower(k); if (!defined.count(lk)) { add(k, J::str(fileKeys[lk])); defined.insert(lk); } }
    return o + "}";
}
static std::string bodyMaterialFile() {
    if (!body.skinMaterial.empty()) return body.skinMaterial;
    std::error_code ec;
    for (const auto& r : body.roots) { const fs::path p = fs::u8path(r) / "skins" / "default.mhmat"; if (fs::exists(p, ec)) return p.u8string(); }
    return (fs::u8path(dataDir) / "skins" / "default.mhmat").u8string();
}

// runs of true as inclusive [first, last] pairs (MHAPI's _boolsToRunLenghtIdx)
static std::string runs(const std::vector<char>& on) {
    std::string o = "["; bool open = false; int first = 0, n = 0;
    for (int i = 0; i <= (int)on.size(); ++i) {
        const bool v = i < (int)on.size() && on[i];
        if (v && !open) { first = i; open = true; }
        if (!v && open) { o += (n++ ? ", [" : "[") + std::to_string(first) + ", " + std::to_string(i - 1) + "]"; open = false; }
    }
    return o + "]";
}

// a proxy's mesh as MakeHuman holds it: its own vertices fitted to the body, faces of 4 (a triangle repeats its last)
struct PMesh { std::vector<float> coord, texco; std::vector<uint32_t> fvert, fuvs; };
static PMesh proxyMesh(const Human::Proxy& p) {
    PMesh m; Human::fitProxy(body, p, m.coord);
    m.texco = p.uv;
    for (size_t t = 0; t + 2 < p.tri.size(); t += 3) {
        m.fvert.insert(m.fvert.end(), {(uint32_t)p.tri[t], (uint32_t)p.tri[t + 1], (uint32_t)p.tri[t + 2], (uint32_t)p.tri[t + 2]});
        m.fuvs.insert(m.fuvs.end(), {(uint32_t)t, (uint32_t)t + 1, (uint32_t)t + 2, (uint32_t)t + 2});
    }
    return m;
}
static const Human::Proxy* proxyByUuid(const std::string& u) { for (const auto& p : body.proxies) if (p.uuid == u) return &p; return nullptr; }
static std::vector<const Human::Proxy*> proxiesInOrder() {   // MakeHuman's order: the worn proxies, then the topology
    std::vector<const Human::Proxy*> v;
    for (const auto& p : body.proxies) if (p.kind != "proxymeshes") v.push_back(&p);
    for (const auto& p : body.proxies) if (p.kind == "proxymeshes") v.push_back(&p);
    return v;
}

// weights per bone (sorted by name): (vertices, weights), for the body or a proxy
using Weights = std::vector<std::pair<std::string, std::pair<std::vector<uint32_t>, std::vector<float>>>>;
static Weights bodyWeights() {
    Weights w;
    for (size_t bi = 0; bi < chosen.bones.size() && bi < chosen.weights.size(); ++bi) {
        if (chosen.weights[bi].empty()) continue;
        std::pair<std::vector<uint32_t>, std::vector<float>> vw;
        for (const auto& [v, x] : chosen.weights[bi]) { vw.first.push_back((uint32_t)v); vw.second.push_back(x); }
        w.push_back({chosen.bones[bi].name, std::move(vw)});
    }
    std::sort(w.begin(), w.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    return w;
}
static Weights proxyWeights(const Human::Proxy& p) {   // proxy.getVertexWeights: each vertex takes its three body vertices' weights
    std::vector<std::map<int, float>> per(body.base.size() / 3);
    for (size_t bi = 0; bi < chosen.bones.size() && bi < chosen.weights.size(); ++bi)
        for (const auto& [v, x] : chosen.weights[bi]) if ((size_t)v < per.size()) per[v][(int)bi] += x;
    std::map<int, std::pair<std::vector<uint32_t>, std::vector<float>>> byBone;
    const size_t n = p.pos.size() / 3;
    for (size_t i = 0; i < n && i * 3 + 2 < p.ref.size(); ++i) {
        std::map<int, float> acc;
        for (int k = 0; k < 3; ++k) { const int r = p.ref[i * 3 + k]; const float f = p.w[i * 3 + k]; if (r >= 0 && (size_t)r < per.size() && f != 0) for (const auto& [b, x] : per[r]) acc[b] += f * x; }
        for (const auto& [b, x] : acc) if (x > 1e-4f) { byBone[b].first.push_back((uint32_t)i); byBone[b].second.push_back(x); }
    }
    Weights w; for (auto& [b, vw] : byBone) w.push_back({chosen.bones[b].name, std::move(vw)});
    std::sort(w.begin(), w.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    return w;
}
static std::string weightInfo(const Weights& w) {
    size_t sv = 0; std::string l = "[";
    for (size_t i = 0; i < w.size(); ++i) {
        const size_t n = w[i].second.first.size(); sv += n;
        l += (i ? ", " : "") + std::string("{\"bone\": ") + J::str(w[i].first) + ", \"numVertices\": " + std::to_string(n) +
             ", \"vertListBytesWhenPacked\": " + std::to_string(n * 4) + ", \"weightsBytesWhenPacked\": " + std::to_string(n * 4) + "}";
    }
    return "{\"sumVerts\": " + std::to_string(sv) + ", \"sumVertListBytes\": " + std::to_string(sv * 4) + ", \"sumWeightsBytes\": " + std::to_string(sv * 4) + ", \"weights\": " + l + "]}";
}
static std::string weightBytes(const Weights& w, bool verts) {
    std::string o;
    for (const auto& b : w) o += verts ? bytesOf(b.second.first.data(), b.second.first.size() * 4) : bytesOf(b.second.second.data(), b.second.second.size() * 4);
    return o;
}

// a bone's rest matrix as MakeHuman's rows (ours are column major)
static std::array<double, 16> rowsOf(const Skel::Mat4& m) { std::array<double, 16> r{}; for (int i = 0; i < 4; ++i) for (int j = 0; j < 4; ++j) r[i * 4 + j] = m[j * 4 + i]; return r; }
static std::string matrixRows(const std::array<double, 16>& r) {
    std::string o = "[";
    for (int i = 0; i < 4; ++i) o += (i ? ", " : "") + list({r[i * 4], r[i * 4 + 1], r[i * 4 + 2], r[i * 4 + 3]});
    return o + "]";
}
// the bone's roll, as MPFB's import wants it (meshops._boneToHash): the quaternion of the Z-up matrix (rows r0, -r2, r1)
static double rollOf(const std::array<double, 16>& r) {
    const double PI = 3.14159265358979323846;
    double m[3][3];
    for (int j = 0; j < 3; ++j) { m[0][j] = r[0 * 4 + j]; m[1][j] = -r[2 * 4 + j]; m[2][j] = r[1 * 4 + j]; }
    double qw, qy; const double t = m[0][0] + m[1][1] + m[2][2];
    if (t > 0) { const double s = std::sqrt(t + 1) * 2; qw = 0.25 * s; qy = (m[0][2] - m[2][0]) / s; }
    else if (m[0][0] > m[1][1] && m[0][0] > m[2][2]) { const double s = std::sqrt(1 + m[0][0] - m[1][1] - m[2][2]) * 2; qw = (m[2][1] - m[1][2]) / s; qy = (m[0][1] + m[1][0]) / s; }
    else if (m[1][1] > m[2][2]) { const double s = std::sqrt(1 + m[1][1] - m[0][0] - m[2][2]) * 2; qw = (m[0][2] - m[2][0]) / s; qy = 0.25 * s; }
    else { const double s = std::sqrt(1 + m[2][2] - m[0][0] - m[1][1]) * 2; qw = (m[1][0] - m[0][1]) / s; qy = (m[1][2] + m[2][1]) / s; }
    if (qw < 0) { qw = -qw; qy = -qy; }
    double roll = qw < 1e-4 ? 0 : PI - 2 * std::atan2(qy, qw);
    if (roll < -PI) roll += 2 * PI; else if (roll > PI) roll -= 2 * PI;
    return roll;
}
static std::string boneHash(int bi, int depth) {
    const Skel::Bone& b = chosen.bones[bi];
    const auto r = rowsOf(b.restGlobal);
    std::string kids = "[";
    if (depth < 30) { int n = 0; for (size_t c = 0; c < chosen.bones.size(); ++c) if (chosen.bones[c].parent == bi) kids += (n++ ? ", " : "") + boneHash((int)c, depth + 1); }
    return "{\"name\": " + J::str(b.name) + ", \"headPos\": " + list({b.head[0], b.head[1], b.head[2]}) + ", \"tailPos\": " + list({b.tail[0], b.tail[1], b.tail[2]}) +
           ", \"matrix\": " + matrixRows(r) + ", \"roll\": " + J::num(rollOf(r)) + ", \"children\": " + kids + "]}";
}

// one call: data is its answer (JSON text, or bytes when binary)
static void evaluate(const std::string& function, const Json::Value* params, std::string& data, std::string& error, bool& binary) {
    auto param = [&](const char* k) { return params ? params->str(k) : std::string(); };
    binary = false;
    if (function.rfind("get", 0) == 0) refresh();
    if (function == "getUserDir") data = J::str(fs::absolute(fs::u8path(userDataDir())).u8string());
    else if (function == "getSysDir") data = J::str(fs::absolute(fs::u8path(dataDir)).u8string());
    else if (function == "getAvailableModifierNames") {
        std::vector<std::string> n; for (const auto& m : body.mods) n.push_back(m.fullName()); std::sort(n.begin(), n.end());
        data = "["; for (size_t i = 0; i < n.size(); ++i) data += (i ? ", " : "") + J::str(n[i]); data += "]";
    } else if (function == "getAppliedTargets") {
        data = "{"; int k = 0; for (const auto& t : body.targets) if (t.weight != 0) data += (k++ ? ", " : "") + J::str(t.path) + ": " + J::num(t.weight); data += "}";
    } else if (function == "applyModifier") {
        auto it = body.modByName.find(param("modifier"));
        if (it == body.modByName.end()) error = "No such modifier";
        else { Human::setModifier(body, it->second, (float)atof(param("power").c_str())); dirty = true; data = J::str("OK"); }
    } else if (function == "getCoord") {
        const auto& p = shownPos(); data = "[";
        for (size_t i = 0; i + 2 < p.size(); i += 3) data += (i ? ", " : "") + list({p[i], p[i + 1], p[i + 2]});
        data += "]";
    } else if (function == "getBodyVerticesBinary") { binary = true; const auto& p = shownPos(); data = bytesOf(p.data(), p.size() * 4); }
    else if (function == "getBodyFacesBinary") { binary = true; std::vector<uint32_t> f(body.faces4.begin(), body.faces4.end()); data = bytesOf(f.data(), f.size() * 4); }
    else if (function == "getBodyTextureCoordsBinary") { binary = true; data = bytesOf(body.uv.data(), body.uv.size() * 4); }
    else if (function == "getBodyFaceUVMappingsBinary") { binary = true; std::vector<uint32_t> f(body.faces4Uv.begin(), body.faces4Uv.end()); data = bytesOf(f.data(), f.size() * 4); }
    else if (function == "getBodyMaterialInfo") data = materialHash(bodyMaterialFile());
    else if (function == "getBodyMeshInfo") {
        const size_t nv = body.base.size() / 3, nf = body.faces4.size() / 4, nt = body.uv.size() / 2;
        // the faces shown: not a helper or joint marker, and not under clothes
        const std::vector<char> hid = Human::hiddenVerts(body);
        std::vector<char> shown(nf, 1);
        for (const auto& g : body.faceGroups) if (g.first.rfind("helper-", 0) == 0 || g.first.rfind("joint-", 0) == 0) for (const auto& r : g.second) for (int f = r.first; f < r.second; ++f) shown[f] = 0;
        for (size_t f = 0; f < nf; ++f) for (int k = 0; k < 4; ++k) if ((size_t)body.faces4[f * 4 + k] < hid.size() && hid[body.faces4[f * 4 + k]]) shown[f] = 0;
        auto groups = body.faceGroups; std::sort(groups.begin(), groups.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
        std::string fg = "[";
        for (size_t i = 0; i < groups.size(); ++i) {
            fg += (i ? ", " : "") + std::string("{\"name\": ") + J::str(groups[i].first) + ", \"fgStartStops\": [";
            for (size_t r = 0; r < groups[i].second.size(); ++r) fg += (r ? ", [" : "[") + std::to_string(groups[i].second[r].first) + ", " + std::to_string(groups[i].second[r].second) + "]";
            fg += "]}";
        }
        fg += "]";
        // the skin's colour by ethnicity (autoskinblender's three colours)
        const float sum = body.african + body.asian + body.caucasian;
        const double a = sum > 0 ? body.african / sum : 0, s = sum > 0 ? body.asian / sum : 0, c = sum > 0 ? body.caucasian / sum : 1;
        const double skin[3] = {s * 0.721 + a * 0.207 + c * 0.843, s * 0.568 + a * 0.113 + c * 0.639, s * 0.431 + a * 0.066 + c * 0.517};
        const std::string name = saveName[0] ? saveName : "untitled";
        data = "{\"name\": " + J::str(name) + ", \"filename\": " + J::str(name) + ", \"faceMask\": " + runs(shown) +
               ", \"numVertices\": " + std::to_string(nv) + ", \"verticesTypeCode\": \"<f4\", \"verticesShape\": [" + std::to_string(nv) + ", 3], \"verticesBytesWhenPacked\": " + std::to_string(nv * 12) +
               ", \"numFaces\": " + std::to_string(nf) + ", \"facesTypeCode\": \"<u4\", \"facesShape\": [" + std::to_string(nf) + ", 4], \"facesBytesWhenPacked\": " + std::to_string(nf * 16) +
               ", \"faceGroups\": " + fg +
               ", \"numTextureCoords\": " + std::to_string(nt) + ", \"textureCoordsTypeCode\": \"<f4\", \"textureCoordsShape\": [" + std::to_string(nt) + ", 2], \"textureCoordsBytesWhenPacked\": " + std::to_string(nt * 8) +
               ", \"numFaceUVMappings\": " + std::to_string(nf) + ", \"faceUVMappingsTypeCode\": \"<u4\", \"faceUVMappingsShape\": [" + std::to_string(nf) + ", 4], \"faceUVMappingsBytesWhenPacked\": " + std::to_string(nf * 16) +
               ", \"skinColor\": " + list({skin[0], skin[1], skin[2], 1.0}) + "}";
    } else if (function == "getProxiesInfo") {
        data = "["; int k = 0;
        for (const Human::Proxy* p : proxiesInOrder()) {
            const PMesh m = proxyMesh(*p);
            const size_t nv = m.coord.size() / 3, nf = m.fvert.size() / 4, nt = m.texco.size() / 2;
            const std::string mask = p->kind == "proxymeshes" ? runs(std::vector<char>(nf, 1)) : "[]";
            data += (k++ ? ", " : "") + std::string("{\"faceMask\": ") + mask + ", \"type\": " + J::str(kindName(p->kind)) + ", \"uuid\": " + J::str(p->uuid) + ", \"name\": " + J::str(p->name) +
                    ", \"numVertices\": " + std::to_string(nv) + ", \"verticesTypeCode\": \"f\", \"verticesBytesWhenPacked\": " + std::to_string(nv * 12) +
                    ", \"numFaces\": " + std::to_string(nf) + ", \"facesTypeCode\": \"I\", \"facesBytesWhenPacked\": " + std::to_string(nf * 16) +
                    ", \"numTextureCoords\": " + std::to_string(nt) + ", \"textureCoordsTypeCode\": \"f\", \"textureCoordsBytesWhenPacked\": " + std::to_string(nt * 8) +
                    ", \"numFaceUVMappings\": " + std::to_string(nf) + ", \"faceUVMappingsTypeCode\": \"I\", \"faceUVMappingsBytesWhenPacked\": " + std::to_string(nf * 16) + "}";
        }
        data += "]";
    } else if (function.rfind("getProxy", 0) == 0) {
        const Human::Proxy* p = proxyByUuid(param("uuid"));
        if (!p) { error = "No proxy with the uuid " + param("uuid"); return; }
        if (function == "getProxyMaterialInfo") data = materialHash(p->kind == "proxymeshes" ? bodyMaterialFile() : p->material);
        else if (function == "getProxyWeightInfo" || function == "getProxyWeightsVertList" || function == "getProxyWeights") {
            if (chosen.empty()) { error = "No skeleton"; return; }
            const Weights w = proxyWeights(*p);
            if (function == "getProxyWeightInfo") data = weightInfo(w); else { binary = true; data = weightBytes(w, function == "getProxyWeightsVertList"); }
        } else {
            const PMesh m = proxyMesh(*p); binary = true;
            if (function == "getProxyVerticesBinary") data = bytesOf(m.coord.data(), m.coord.size() * 4);
            else if (function == "getProxyFacesBinary") data = bytesOf(m.fvert.data(), m.fvert.size() * 4);
            else if (function == "getProxyTextureCoordsBinary") data = bytesOf(m.texco.data(), m.texco.size() * 4);
            else if (function == "getProxyFaceUVMappingsBinary") data = bytesOf(m.fuvs.data(), m.fuvs.size() * 4);
            else { binary = false; error = "\"" + function + "\" is not valid command"; }
        }
    } else if (function == "getBodyWeightInfo" || function == "getBodyWeightsVertList" || function == "getBodyWeights") {
        if (chosen.empty()) { error = "No skeleton"; return; }
        const Weights w = bodyWeights();
        if (function == "getBodyWeightInfo") data = weightInfo(w); else { binary = true; data = weightBytes(w, function == "getBodyWeightsVertList"); }
    } else if (function == "getSkeleton") {
        float g[3] = {0, 0, 0}; Human::jointPos(body, "joint-ground", g);   // getJointPosition("ground")
        std::string bones = "["; int k = 0;
        if (!chosen.empty()) for (size_t b = 0; b < chosen.bones.size(); ++b) if (chosen.bones[b].parent < 0) bones += (k++ ? ", " : "") + boneHash((int)b, 1);
        bones += "]";
        const std::string nm = chosen.empty() ? "none" : (chosen.name.empty() ? fs::u8path(skeletonFile).stem().u8string() : chosen.name);
        data = "{\"offset\": " + list({0.0, 0.0, -g[1]}) + ", \"name\": " + J::str(nm) + ", \"bones\": " + bones + "}";
    } else if (function == "getPose") {
        const std::string f = param("poseFilename");
        if (!f.empty()) { const std::string ext = fs::u8path(f).extension().u8string(); if (ext == ".mhpose") setExpressionFile(f); else if (ext == ".bvh") setPoseFile(f); refresh(); }
        if (chosen.empty()) { error = "No skeleton"; return; }
        data = "{";
        for (size_t b = 0; b < chosen.bones.size(); ++b) {
            const auto r = rowsOf(chosen.bones[b].restGlobal);
            // zUpFaceNegY: turned 90 degrees round X (rows y and z become -z and y)
            std::array<double, 16> t = r; for (int j = 0; j < 4; ++j) { t[4 + j] = -r[8 + j]; t[8 + j] = r[4 + j]; }
            data += (b ? ", " : "") + J::str(chosen.bones[b].name) + ": " + matrixRows(t);
        }
        data += "}";
    } else error = "\"" + function + "\" is not valid command";
}
}

static void socketAnswer(const std::string& text, Sock::Answer& reply) {
    Json::Value v; size_t i = 0;
    std::string function = "generic", error, data = "null", params = "{}";
    if (!Json::parse(text, i, v) || v.type != Json::Value::OBJ) error = "The call isn't JSON";
    else {
        function = v.str("function", "generic");
        const Json::Value* p = v.get("params");
        // the params go back as they came
        if (p && p->type == Json::Value::OBJ) {
            params = "{";
            for (size_t k = 0; k < p->o.size(); ++k) {
                const Json::Value& x = p->o[k].second;
                params += (k ? ", " : "") + SockOps::J::str(p->o[k].first) + ": " +
                          (x.type == Json::Value::STR ? SockOps::J::str(x.s) : x.type == Json::Value::NUM ? SockOps::J::num(x.n) : x.type == Json::Value::BOOL ? std::string(x.b ? "true" : "false") : std::string("null"));
            }
            params += "}";
        }
        bool binary = false;
        std::string out;
        SockOps::evaluate(function, p && p->type == Json::Value::OBJ ? p : nullptr, out, error, binary);
        if (binary && error.empty()) { reply.bytes = std::move(out); sockLog.push_back(function + ": " + std::to_string(reply.bytes.size()) + " bytes"); return; }
        if (!out.empty()) data = out;
    }
    sockLog.push_back(function + (error.empty() ? ": answered" : ": " + error));
    reply.bytes = "{\"function\": " + SockOps::J::str(function) + ", \"error\": " + SockOps::J::str(error) + ", \"params\": " + params + ", \"data\": " + data + "}";
}
static void socketOpen() {
    std::string err;
    if (!Sock::start(sockCfg.host, sockCfg.port, err)) { say(err); sockCfg.accept = false; }
}
static void socketLeft(float lw) {
    groupBox("Server");
    if (ImGui::Checkbox("Accept connections", &sockCfg.accept)) { if (sockCfg.accept) socketOpen(); else Sock::stop(); }
    ImGui::Checkbox("Advanced settings", &sockCfg.advanced);
    if (sockCfg.advanced) {
        ImGui::TextUnformatted("Host [Default=127.0.0.1]:"); ImGui::SetNextItemWidth(lw); ImGui::InputText("##host", sockHost, sizeof sockHost);
        ImGui::TextUnformatted("Port [Default=12345]:"); ImGui::SetNextItemWidth(lw); ImGui::InputText("##port", sockPort, sizeof sockPort, ImGuiInputTextFlags_CharsDecimal);
        if (ImGui::Button("Change host + port", ImVec2(lw, 0))) {
            sockCfg.host = sockHost; sockCfg.port = std::clamp(atoi(sockPort), 1, 65535);
            say("The host and port must be changed in Blender, too");
            sockCfg.accept = true; socketOpen();
        }
    }
    notYet("Blender's MakeHuman add-on (MPFB) reads the person straight from here: turn this on, then use MPFB's \"Import from MakeHuman\".");
    groupBox("Messages");
    ImGui::BeginChild("socklog", ImVec2(lw, 260), true, ImGuiWindowFlags_HorizontalScrollbar);
    for (const auto& m : sockLog) ImGui::TextUnformatted(m.c_str());
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 4) ImGui::SetScrollHereY(1);
    ImGui::EndChild();
    if (ImGui::Button("Clear", ImVec2(lw, 0))) sockLog.clear();
}
// once a frame: the server's own messages into the log, then the calls waiting
static void socketPoll() {
    for (auto& m : Sock::takeMessages()) sockLog.push_back(std::move(m));
    if (sockLog.size() > 2000) sockLog.erase(sockLog.begin(), sockLog.begin() + (sockLog.size() - 2000));
    if (Sock::running()) Sock::poll(socketAnswer);
}
