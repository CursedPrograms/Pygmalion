// Pygmalion - the human (see human.h). The weighting follows MakeHuman's own code line for line: the macro values
// (human.py's _set*Vals), the targets' groups and variables (targets.py) and the modifiers' factors
// (humanmodifier.py).
#include "human.h"
#include "json.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <regex>
#include <set>
#include <sstream>

namespace fs = std::filesystem;

namespace Human {

// MakeHuman's categories and their values (targets.py _cat_data): a target's file path names its values
const char* VAR_NAMES[] = {
    "male", "female",
    "baby", "child", "young", "old",
    "caucasian", "asian", "african",
    "maxmuscle", "averagemuscle", "minmuscle",
    "minweight", "averageweight", "maxweight",
    "minheight", "averageheight", "maxheight",
    "mincup", "averagecup", "maxcup",
    "minfirmness", "averagefirmness", "maxfirmness",
    "uncommonproportions", "regularproportions", "idealproportions",
    nullptr};
enum { V_MALE, V_FEMALE, V_BABY, V_CHILD, V_YOUNG, V_OLD, V_CAUC, V_ASIAN, V_AFRICAN, V_MAXMUS, V_AVGMUS, V_MINMUS,
       V_MINW, V_AVGW, V_MAXW, V_MINH, V_AVGH, V_MAXH, V_MINCUP, V_AVGCUP, V_MAXCUP, V_MINF, V_AVGF, V_MAXF,
       V_UNCOMMON, V_REGULAR, V_IDEAL, V_COUNT };

int varIndex(const std::string& v) {
    for (int i = 0; VAR_NAMES[i]; ++i) if (v == VAR_NAMES[i]) return i;
    return -1;
}

static bool readAll(const fs::path& p, std::string& out) {
    std::ifstream in(p, std::ios::binary);
    if (!in) return false;
    std::ostringstream ss; ss << in.rdbuf(); out = ss.str();
    return true;
}

// ---------- the base mesh ----------
static bool loadBase(Body& b, const fs::path& obj, std::string& err) {
    std::string t;
    if (!readAll(obj, t)) { err = "Couldn't read " + obj.u8string(); return false; }
    std::istringstream in(t);
    std::string line, group;
    bool visible = true;
    while (std::getline(in, line)) {
        if (line.size() < 2) continue;
        if (line[0] == 'v' && line[1] == ' ') { float x, y, z; if (sscanf(line.c_str() + 2, "%f %f %f", &x, &y, &z) == 3) { b.base.push_back(x); b.base.push_back(y); b.base.push_back(z); } }
        else if (line[0] == 'v' && line[1] == 't') { float u, v; if (sscanf(line.c_str() + 3, "%f %f", &u, &v) == 2) { b.uv.push_back(u); b.uv.push_back(v); } }
        else if (line[0] == 'g' && line[1] == ' ') {
            group = line.substr(2); while (!group.empty() && (group.back() == '\r' || group.back() == ' ')) group.pop_back();
            // the helpers (tights, skirt, hair and joint markers) are there for fitting clothes and bones: not shown
            visible = group.rfind("helper-", 0) != 0 && group.rfind("joint-", 0) != 0;
        } else if (line[0] == 'f' && line[1] == ' ' && group.rfind("joint-", 0) == 0) {
            std::istringstream ls(line.substr(2)); std::string c;
            while (ls >> c) { const int a = atoi(c.c_str()) - 1; if (a >= 0) b.joints[group].push_back(a); }
        } else if (line[0] == 'f' && line[1] == ' ' && visible) {
            std::vector<int> v, vt;
            std::istringstream ls(line.substr(2));
            std::string c;
            while (ls >> c) {
                int a = 0, u = 0;
                if (sscanf(c.c_str(), "%d/%d", &a, &u) >= 1) { v.push_back(a - 1); vt.push_back(u - 1); }
            }
            for (size_t k = 1; k + 1 < v.size(); ++k) {
                b.tri.insert(b.tri.end(), {v[0], v[k], v[k + 1]});
                b.triUv.insert(b.triUv.end(), {vt[0], vt[k], vt[k + 1]});
            }
        }
    }
    if (b.base.empty() || b.tri.empty()) { err = "The base mesh is empty"; return false; }
    b.pos = b.base;
    return true;
}

// ---------- the targets: every .target under targets/, its group and variables from its path ----------
static void addTarget(Body& b, const fs::path& file, const fs::path& root) {
    Target t;
    t.path = fs::relative(file, root).generic_u8string();
    std::vector<std::string> key;
    std::set<int> vars;
    // each folder and the file name, split on - (and _ and .), as targets.py does
    std::vector<std::string> parts;
    for (const auto& comp : fs::relative(file, root)) {
        std::string n = comp.u8string();
        for (auto& c : n) if (c == '_' || c == '.') c = '-';
        std::stringstream ss(n); std::string p;
        while (std::getline(ss, p, '-')) parts.push_back(p);
    }
    for (const auto& p : parts) {
        const int v = varIndex(p);
        if (v >= 0) vars.insert(v);
        else if (p != "target" && !p.empty()) key.push_back(p);
    }
    for (size_t i = 0; i < key.size(); ++i) t.group += (i ? "-" : "") + key[i];
    t.vars.assign(vars.begin(), vars.end());
    std::string txt;
    if (!readAll(file, txt)) return;
    const char* s = txt.c_str();
    const char* end = s + txt.size();
    while (s < end) {
        const char* eol = (const char*)memchr(s, '\n', end - s);
        if (!eol) eol = end;
        if (*s != '#') {
            char* q;
            const long i = strtol(s, &q, 10);
            if (q != s) {
                const float dx = strtof(q, &q), dy = strtof(q, &q), dz = strtof(q, &q);
                t.idx.push_back((int)i); t.d.insert(t.d.end(), {dx, dy, dz});
            }
        }
        s = eol + 1;
    }
    b.groupTargets[t.group].push_back((int)b.targets.size());
    b.targets.push_back(std::move(t));
}

// ---------- the modifiers (modifiers/*_modifiers.json) and their sliders (*_sliders.json) ----------
// MakeHuman's own rule (modifierslider.py): the name's words, without the last if it holds the "|" choices and
// without the group's name when it starts with it, each capitalised
static std::string niceLabel(const std::string& group, const std::string& name) {
    std::vector<std::string> w; std::stringstream ss(name); std::string p;
    while (std::getline(ss, p, '-')) w.push_back(p);
    if (!w.empty() && w.back().find('|') != std::string::npos) w.pop_back();
    if (w.size() > 1 && w[0] == group) w.erase(w.begin());
    std::string r;
    for (auto& x : w) { if (x.empty()) continue; x[0] = (char)toupper((unsigned char)x[0]); r += (r.empty() ? "" : " ") + x; }
    return r;
}

static void loadModifiers(Body& b, const fs::path& file) {
    std::string t;
    if (!readAll(file, t)) return;
    Json::Value root; size_t i = 0;
    if (!Json::parse(t, i, root)) return;
    std::map<std::string, std::string> desc;
    {
        std::string d; fs::path df = file; df.replace_filename(file.stem().u8string() + "_desc.json");
        Json::Value dr; size_t j = 0;
        if (readAll(df, d) && Json::parse(d, j, dr)) for (const auto& kv : dr.o) desc[kv.first] = kv.second.s;
    }
    for (const auto& g : root.a) {
        const std::string group = g.str("group");
        const Json::Value* ms = g.get("modifiers");
        if (!ms) continue;
        for (const auto& m : ms->a) {
            Modifier mod;
            mod.group = group;
            if (m.get("macrovar")) {
                mod.macro = true; mod.variable = m.str("macrovar"); mod.name = mod.variable;
                mod.defaultValue = (mod.variable == "African" || mod.variable == "Asian" || mod.variable == "Caucasian") ? 1 / 3.0f : 0.5f;
            } else {
                const std::string target = m.str("target"), lo = m.str("min"), hi = m.str("max"), mid = m.str("mid");
                const std::string base = group + "-" + target;
                if (!lo.empty() && !hi.empty()) {
                    mod.left = base + "-" + lo; mod.right = base + "-" + hi;
                    if (!mid.empty()) { mod.center = base + "-" + mid; mod.name = target + "-" + lo + "|" + mid + "|" + hi; }
                    else mod.name = target + "-" + lo + "|" + hi;
                } else { mod.right = base; mod.name = target; }
            }
            if (const Json::Value* dv = m.get("defaultValue")) mod.defaultValue = (float)dv->n;
            mod.value = mod.defaultValue;
            mod.label = mod.macro ? mod.variable : niceLabel(group, mod.name);
            auto it = desc.find(mod.fullName());
            if (it != desc.end()) mod.description = it->second;
            b.modByName[mod.fullName()] = (int)b.mods.size();
            b.mods.push_back(mod);
        }
    }
}

static void loadSliders(Body& b, const fs::path& file) {
    std::string t;
    if (!readAll(file, t)) return;
    Json::Value root; size_t i = 0;
    if (!Json::parse(t, i, root)) return;
    for (const auto& tab : root.o) {
        Tab tb; tb.name = tab.first; tb.order = (float)tab.second.num("sortOrder", 0.5);
        const Json::Value* groups = tab.second.get("modifiers");
        if (!groups) continue;
        for (const auto& g : groups->o) {
            SliderGroup sg; sg.name = g.first;
            if (!sg.name.empty()) sg.name[0] = (char)toupper((unsigned char)sg.name[0]);
            for (const auto& s : g.second.a) {
                auto it = b.modByName.find(s.str("mod"));
                if (it == b.modByName.end()) continue;
                if (s.get("label")) b.mods[it->second].label = s.str("label");
                if (s.get("cam")) b.mods[it->second].cam = s.str("cam");
                sg.mods.push_back(it->second);
            }
            if (!sg.mods.empty()) tb.groups.push_back(sg);
        }
        if (!tb.groups.empty()) b.tabs.push_back(tb);
    }
}

// ---------- proxies (.mhclo): everything fitted to the body ----------
bool loadMaterial(const std::string& mhmat, std::string& texture, bool& transparent) {
    std::string mt;
    const fs::path mp = fs::u8path(mhmat);
    if (!readAll(mp, mt)) return false;
    std::istringstream mi(mt);
    std::string line;
    texture.clear(); transparent = false;
    while (std::getline(mi, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::istringstream ls(line); std::string k, v; ls >> k >> v;
        if (k == "diffuseTexture") texture = (mp.parent_path() / fs::u8path(v)).lexically_normal().u8string();
        else if ((k == "transparent" && (v == "True" || v == "true")) || (k == "shaderConfig" && v == "transparency" && line.find("True") != std::string::npos)) transparent = true;
    }
    return true;
}

bool loadProxyFile(const std::string& path, Proxy& p, std::string& err) {
    p = Proxy();
    const fs::path mhclo = fs::u8path(path);
    p.file = path;
    p.kind = mhclo.parent_path().parent_path().filename().u8string();
    if (p.kind == "high-poly" || p.kind == "low-poly") p.kind = "eyes";
    std::string t;
    if (!readAll(mhclo, t)) { err = "Couldn't read " + path; return false; }
    std::istringstream in(t);
    std::string line, objFile, material;
    enum { NONE, VERTS, DELETE } mode = NONE;
    bool sawVerts = false;   // (keywords like material may sit between "verts" and its numbers)
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        std::istringstream ls(line);
        const bool numeric = line.find_first_not_of(" \t0123456789.-+eE") == std::string::npos;
        if (mode != DELETE && sawVerts && numeric) {
            std::vector<double> v; double x;
            while (ls >> x) v.push_back(x);
            if (v.size() == 9) {
                for (int k = 0; k < 3; ++k) { p.ref.push_back((int)v[k]); p.w.push_back((float)v[3 + k]); p.off.push_back((float)v[6 + k]); }
            } else if (v.size() == 1) {   // exactly on a body vertex
                p.ref.insert(p.ref.end(), {(int)v[0], (int)v[0], (int)v[0]}); p.w.insert(p.w.end(), {1, 0, 0}); p.off.insert(p.off.end(), {0, 0, 0});
            }
            continue;
        }
        if (mode == DELETE && numeric) {
            // "a - b" ranges and single vertices
            std::vector<std::string> tok; std::string w;
            while (ls >> w) tok.push_back(w);
            for (size_t i = 0; i < tok.size(); ++i) {
                if (i + 2 < tok.size() && tok[i + 1] == "-") { const int a = atoi(tok[i].c_str()), z = atoi(tok[i + 2].c_str()); for (int v = a; v <= z; ++v) p.deleteVerts.push_back(v); i += 2; }
                else p.deleteVerts.push_back(atoi(tok[i].c_str()));
            }
            continue;
        }
        mode = NONE;
        std::string k; ls >> k;
        if (k == "verts") { mode = VERTS; sawVerts = true; }
        else if (k == "delete_verts") mode = DELETE;
        else if (k == "name") ls >> p.name;
        else if (k == "uuid") ls >> p.uuid;
        else if (k == "obj_file") ls >> objFile;
        else if (k == "material") { std::getline(ls >> std::ws, material); }
        else if (k == "z_depth") ls >> p.zDepth;
        else if (k == "x_scale" || k == "y_scale" || k == "z_scale") {
            const int a = k[0] - 'x';
            ls >> p.scaleRef[a][0] >> p.scaleRef[a][1] >> p.scaleDiv[a];
        }
    }
    // its own mesh: vertices, uvs, triangles
    std::string o;
    if (!readAll(mhclo.parent_path() / fs::u8path(objFile), o)) { err = "Couldn't read " + objFile; return false; }
    std::vector<float> vt;
    const char* s = o.c_str(); const char* end = s + o.size();
    while (s < end) {
        const char* eol = (const char*)memchr(s, '\n', end - s);
        if (!eol) eol = end;
        char* q = nullptr;
        if (s[0] == 'v' && s[1] == ' ') { q = (char*)s + 2; for (int k = 0; k < 3; ++k) p.pos.push_back(strtof(q, &q)); }
        else if (s[0] == 'v' && s[1] == 't') { q = (char*)s + 3; for (int k = 0; k < 2; ++k) vt.push_back(strtof(q, &q)); }
        else if (s[0] == 'f' && s[1] == ' ') {
            std::vector<int> v, u;
            const char* c = s + 2;
            while (c < eol) {
                while (c < eol && (*c == ' ' || *c == '\t' || *c == '\r')) ++c;
                if (c >= eol) break;
                char* e2;
                const int a = (int)strtol(c, &e2, 10); int w = 0;
                c = e2;
                if (*c == '/') { ++c; if (*c != '/') { w = (int)strtol(c, &e2, 10); c = e2; } if (*c == '/') { ++c; strtol(c, &e2, 10); c = e2; } }
                while (c < eol && *c != ' ' && *c != '\t') ++c;
                v.push_back(a - 1); u.push_back(w - 1);
            }
            for (size_t k = 1; k + 1 < v.size(); ++k)
                for (size_t qq : {(size_t)0, k, k + 1}) {
                    p.tri.push_back(v[qq]);
                    const int ui = u[qq];
                    p.uv.push_back(ui >= 0 && (size_t)ui * 2 + 1 < vt.size() ? vt[ui * 2] : 0);
                    p.uv.push_back(ui >= 0 && (size_t)ui * 2 + 1 < vt.size() ? vt[ui * 2 + 1] : 0);
                }
        }
        s = eol + 1;
    }
    if (p.ref.size() / 3 != p.pos.size() / 3) { err = p.name + ": its fitting doesn't match its mesh"; return false; }
    if (!material.empty()) {
        p.material = (mhclo.parent_path() / fs::u8path(material)).lexically_normal().u8string();
        loadMaterial(p.material, p.texture, p.transparent);
    }
    return true;
}

// ---------- the gear ----------
std::string relPath(const Body& b, const std::string& abs) {
    const fs::path a = fs::u8path(abs).lexically_normal();
    for (const auto& r : b.roots) {
        const fs::path rel = a.lexically_relative(fs::u8path(r).lexically_normal());
        if (!rel.empty() && rel.native()[0] != '.') return rel.generic_u8string();
    }
    return a.generic_u8string();
}
std::string absPath(const Body& b, const std::string& rel) {
    std::error_code ec;
    if (fs::u8path(rel).is_absolute()) return rel;
    for (const auto& r : b.roots) { const fs::path f = fs::u8path(r) / fs::u8path(rel); if (fs::exists(f, ec)) return f.lexically_normal().u8string(); }
    return std::string();
}

static void sortGear(Body& b) {
    std::stable_sort(b.proxies.begin(), b.proxies.end(), [](const Proxy& x, const Proxy& y) { return x.zDepth < y.zDepth; });
}

bool isWorn(const Body& b, const std::string& file) {
    for (const auto& p : b.proxies) if (fs::u8path(p.file).lexically_normal() == fs::u8path(file).lexically_normal()) return true;
    return false;
}
const Proxy* worn(const Body& b, const std::string& kind) {
    for (const auto& p : b.proxies) if (p.kind == kind) return &p;
    return nullptr;
}
void takeOff(Body& b, const std::string& k) {
    const fs::path f = fs::u8path(k).lexically_normal();
    b.proxies.erase(std::remove_if(b.proxies.begin(), b.proxies.end(), [&](const Proxy& p) { return p.kind == k || fs::u8path(p.file).lexically_normal() == f; }), b.proxies.end());
}
bool wear(Body& b, const std::string& file, std::string& err) {
    if (isWorn(b, file)) {   // clothes come off with a second click; the rest stay
        for (const auto& p : b.proxies) if (p.kind == "clothes" && fs::u8path(p.file).lexically_normal() == fs::u8path(file).lexically_normal()) { takeOff(b, file); break; }
        return true;
    }
    Proxy p;
    if (!loadProxyFile(file, p, err)) return false;
    if (p.kind != "clothes") takeOff(b, p.kind);
    b.proxies.push_back(std::move(p));
    sortGear(b);
    return true;
}
bool setSkin(Body& b, const std::string& mhmat) {
    b.skinMaterial = mhmat; b.skinTexture.clear();
    if (mhmat.empty()) return true;
    bool t;
    if (!loadMaterial(mhmat, b.skinTexture, t)) { b.skinMaterial.clear(); return false; }
    std::error_code ec;
    if (!b.skinTexture.empty() && !fs::exists(fs::u8path(b.skinTexture), ec)) b.skinTexture.clear();
    return true;
}
bool setProxyMaterial(Body& b, const std::string& kind, const std::string& mhmat) {
    for (auto& p : b.proxies) if (p.kind == kind) { p.material = mhmat; return loadMaterial(mhmat, p.texture, p.transparent); }
    return false;
}
std::vector<char> hiddenVerts(const Body& b) {
    std::vector<char> h(b.base.size() / 3, 0);
    for (const auto& p : b.proxies) {
        if (p.kind == "proxymeshes") { std::fill(h.begin(), h.end(), 1); break; }
        if (p.kind == "clothes" && b.hideUnderClothes) for (int v : p.deleteVerts) if (v >= 0 && (size_t)v < h.size()) h[v] = 1;
    }
    return h;
}
static std::string findGear(const Body& b, const std::string& kind, const std::string& name, const std::string& uuid) {
    std::error_code ec;
    std::string byName;
    for (const auto& r : b.roots)
        for (const auto& e : fs::recursive_directory_iterator(fs::u8path(r) / (kind == "proxy" ? "proxymeshes" : kind), ec)) {
            const auto x = e.path().extension();
            if (!e.is_regular_file() || (x != ".mhclo" && x != ".proxy")) continue;
            std::ifstream in(e.path()); std::string line;
            for (int n = 0; n < 60 && std::getline(in, line); ++n) {
                std::istringstream ls(line); std::string k, v; ls >> k >> v;
                if (k == "uuid" && !uuid.empty() && v == uuid) return e.path().u8string();
                if (k == "name" && byName.empty() && v == name) byName = e.path().u8string();
            }
            if (byName.empty() && e.path().stem().u8string() == name) byName = e.path().u8string();
        }
    return byName;
}
void defaultGear(Body& b) {
    // MakeHuman's new human: brown high-poly eyes, eyebrow001, eyelashes01; no clothes, the default skin
    b.proxies.clear();
    std::string err;
    const std::string e = findGear(b, "eyes", "HighPolyEyes", "2c12f43b-1303-432c-b7ce-d78346baf2e6");
    if (!e.empty()) wear(b, e, err);
    for (const char* k : {"eyebrows/eyebrow001/eyebrow001.mhclo", "eyelashes/eyelashes01/eyelashes01.mhclo"}) {
        const std::string f = absPath(b, k);
        if (!f.empty()) wear(b, f, err);
    }
    setSkin(b, "");
}

static bool loadProxy(Body& b, const fs::path& mhclo) {
    Proxy p; std::string err;
    if (!loadProxyFile(mhclo.u8string(), p, err)) return false;
    b.proxies.push_back(std::move(p));
    return true;
}

bool load(Body& b, const std::string& data, std::string& err, void (*progress)(float, const char*)) {
    b = Body();
    const fs::path root = fs::u8path(data);
    if (progress) progress(0.05f, "Loading the body...");
    if (!loadBase(b, root / "3dobjs" / "base.obj", err)) return false;
    std::vector<fs::path> files;
    std::error_code ec;
    for (const auto& e : fs::recursive_directory_iterator(root / "targets", ec))
        if (e.is_regular_file() && e.path().extension() == ".target") files.push_back(e.path());
    std::sort(files.begin(), files.end());
    for (size_t i = 0; i < files.size(); ++i) {
        addTarget(b, files[i], root / "targets");
        if (progress && i % 50 == 0) progress(0.1f + 0.75f * i / files.size(), "Loading the shapes...");
    }
    if (b.targets.empty()) { err = "No shapes found in " + (root / "targets").u8string(); return false; }
    std::map<std::string, std::string> images;
    for (const auto& e : fs::recursive_directory_iterator(root / "targets", ec))
        if (e.is_regular_file() && e.path().extension() == ".png") {
            std::string n = e.path().filename().u8string();
            for (auto& c : n) c = (char)tolower((unsigned char)c);
            images[n] = e.path().u8string();
        }
    if (progress) progress(0.88f, "Loading the sliders...");
    for (const char* f : {"modeling_modifiers.json", "measurement_modifiers.json", "bodyshapes_modifiers.json"}) loadModifiers(b, root / "modifiers" / f);
    for (const char* f : {"modeling_sliders.json", "measurement_sliders.json", "bodyshapes_sliders.json"}) loadSliders(b, root / "modifiers" / f);
    std::stable_sort(b.tabs.begin(), b.tabs.end(), [](const Tab& a, const Tab& c) { return a.order < c.order; });
    for (auto& m : b.mods) {
        std::string n = m.name;
        for (auto& c : n) { if (c == '|') c = '-'; c = (char)tolower((unsigned char)c); }
        auto it = images.find(n + ".png");
        if (it != images.end()) m.image = it->second;
    }
    if (progress) progress(0.94f, "Fitting the eyes...");
    loadProxy(b, root / "eyes" / "high-poly" / "high-poly.mhclo");
    b.val.assign(V_COUNT, 0);
    resetAll(b);
    return true;
}

// ---------- the macro values (human.py) ----------
static void macroVals(Body& b) {
    auto& v = b.val;
    v[V_MALE] = b.gender; v[V_FEMALE] = 1 - b.gender;
    if (b.age < 0.5f) {
        v[V_OLD] = 0;
        v[V_BABY] = std::max(0.0f, 1 - b.age * 5.333f);
        v[V_YOUNG] = std::max(0.0f, (b.age - 0.1875f) * 3.2f);
        v[V_CHILD] = std::max(0.0f, std::min(1.0f, 5.333f * b.age) - v[V_YOUNG]);
    } else {
        v[V_CHILD] = v[V_BABY] = 0;
        v[V_OLD] = std::max(0.0f, b.age * 2 - 1);
        v[V_YOUNG] = 1 - v[V_OLD];
    }
    auto three = [](float x, float& lo, float& mid, float& hi, bool sumStyle) {
        hi = std::max(0.0f, x * 2 - 1); lo = std::max(0.0f, 1 - x * 2);
        mid = sumStyle ? 1 - (hi + lo) : (hi > lo ? 1 - hi : 1 - lo);
    };
    three(b.muscle, v[V_MINMUS], v[V_AVGMUS], v[V_MAXMUS], true);
    three(b.weight, v[V_MINW], v[V_AVGW], v[V_MAXW], true);
    three(b.height, v[V_MINH], v[V_AVGH], v[V_MAXH], false);
    three(b.breastSize, v[V_MINCUP], v[V_AVGCUP], v[V_MAXCUP], false);
    three(b.breastFirmness, v[V_MINF], v[V_AVGF], v[V_MAXF], false);
    three(b.proportions, v[V_UNCOMMON], v[V_REGULAR], v[V_IDEAL], false);
    v[V_AFRICAN] = b.african; v[V_ASIAN] = b.asian; v[V_CAUC] = b.caucasian;
}

static float* macroField(Body& b, const std::string& v) {
    if (v == "Gender") return &b.gender; if (v == "Age") return &b.age; if (v == "Muscle") return &b.muscle; if (v == "Weight") return &b.weight;
    if (v == "Height") return &b.height; if (v == "BodyProportions") return &b.proportions; if (v == "African") return &b.african;
    if (v == "Asian") return &b.asian; if (v == "Caucasian") return &b.caucasian; if (v == "BreastSize") return &b.breastSize;
    if (v == "BreastFirmness") return &b.breastFirmness;
    return nullptr;
}

void setMacro(Body& b, const std::string& variable, float value) {
    value = std::clamp(value, 0.0f, 1.0f);
    float* f = macroField(b, variable);
    if (!f) return;
    *f = value;
    if (variable == "African" || variable == "Asian" || variable == "Caucasian") {
        // the three always add up to 1 (human.py _setEthnicVals): the other two share what's left in their proportion
        float* others[2]; int n = 0;
        for (float* o : {&b.african, &b.asian, &b.caucasian}) if (o != f) others[n++] = o;
        const float rest = 1 - value, sum = *others[0] + *others[1];
        if (sum <= 1e-6f) { *others[0] = *others[1] = rest / 2; }
        else { *others[0] = rest * *others[0] / sum; *others[1] = rest * *others[1] / sum; }
    }
    for (auto& m : b.mods) if (m.macro) if (float* q = macroField(b, m.variable)) m.value = *q;
}

static int mirrorOf(const Body& b, const Modifier& m) {
    // l <-> r in the name's parts (humanmodifier.py getSymmetricOpposite)
    std::vector<std::string> parts; std::stringstream ss(m.name); std::string p; bool side = false;
    while (std::getline(ss, p, '-')) { if (p == "l") { p = "r"; side = true; } else if (p == "r") { p = "l"; side = true; } parts.push_back(p); }
    if (!side) return -1;
    std::string n; for (size_t i = 0; i < parts.size(); ++i) n += (i ? "-" : "") + parts[i];
    auto it = b.modByName.find(m.group + "/" + n);
    return it == b.modByName.end() ? -1 : it->second;
}

void setModifier(Body& b, int i, float value) {
    Modifier& m = b.mods[i];
    if (m.macro) { setMacro(b, m.variable, value); return; }
    m.value = std::clamp(value, m.min(), 1.0f);
    if (b.symmetry) { const int o = mirrorOf(b, m); if (o >= 0) b.mods[o].value = m.value; }
}

void resetAll(Body& b) {
    b.gender = b.age = b.muscle = b.weight = b.height = b.proportions = b.breastSize = b.breastFirmness = 0.5f;
    b.african = b.asian = b.caucasian = 1 / 3.0f;
    for (auto& m : b.mods) m.value = m.defaultValue;
    for (auto& m : b.mods) if (m.macro) if (float* q = macroField(b, m.variable)) *q = m.value;
}

void apply(Body& b) {
    macroVals(b);
    // each group's factor from its modifier (macro groups: 1)
    std::map<std::string, float> factor;
    for (const auto& m : b.mods) {
        if (m.macro) { factor[m.group] = 1; continue; }
        const float v = m.value;
        if (!m.left.empty()) factor[m.left] = -std::min(v, 0.0f);
        if (!m.center.empty()) factor[m.center] = 1 - std::fabs(v);
        factor[m.right] = std::max(0.0f, v);
    }
    for (auto& t : b.targets) {
        auto it = factor.find(t.group);
        float w = it == factor.end() ? 0.0f : it->second;
        for (int v : t.vars) w *= b.val[v];
        const float dw = w - t.weight;
        if (std::fabs(dw) < 1e-7f) continue;
        for (size_t k = 0; k < t.idx.size(); ++k) {
            float* p = &b.pos[(size_t)t.idx[k] * 3];
            p[0] += dw * t.d[k * 3]; p[1] += dw * t.d[k * 3 + 1]; p[2] += dw * t.d[k * 3 + 2];
        }
        t.weight = w;
    }
}

void fitProxy(const Body& b, const Proxy& p, std::vector<float>& out) {
    out.resize(p.pos.size());
    const std::vector<float>& pos = b.shown.empty() ? b.pos : b.shown;
    float scale[3];
    for (int a = 0; a < 3; ++a) {
        const float d = std::fabs(b.pos[(size_t)p.scaleRef[a][0] * 3 + a] - b.pos[(size_t)p.scaleRef[a][1] * 3 + a]);   // (the scale from the unposed body)
        scale[a] = p.scaleDiv[a] != 0 ? d / p.scaleDiv[a] : 1;
    }
    for (size_t i = 0; i < p.pos.size() / 3; ++i)
        for (int a = 0; a < 3; ++a) {
            float s = 0;
            for (int k = 0; k < 3; ++k) s += p.w[i * 3 + k] * pos[(size_t)p.ref[i * 3 + k] * 3 + a];
            out[i * 3 + a] = s + p.off[i * 3 + a] * scale[a];
        }
}

// MakeHuman's rulers (plugins/0_modeling_a_measurement.py): vertex paths measured along the body
static const std::pair<const char*, std::vector<int>> RULERS[] = {
    {"measure/measure-neck-circ-decr|incr", {7514,10358,7631,7496,7488,7489,7474,7475,7531,7537,7543,7549,7555,7561,7743,7722,856,1030,1051,850,844,838,832,826,820,756,755,770,769,777,929,3690,804,800,808,801,799,803,7513,7515,7521,7514}},
    {"measure/measure-neck-height-decr|incr", {853,854,855,856,857,858,1496,1491}},
    {"measure/measure-upperarm-circ-decr|incr", {8383,8393,8392,8391,8390,8394,8395,8399,10455,10516,8396,8397,8398,8388,8387,8386,10431,8385,8384,8389}},
    {"measure/measure-upperarm-length-decr|incr", {8274,10037}},
    {"measure/measure-lowerarm-length-decr|incr", {10040,10548}},
    {"measure/measure-wrist-circ-decr|incr", {10208,10211,10212,10216,10471,10533,10213,10214,10215,10205,10204,10203,10437,10202,10201,10206,10200,10210,10209,10208}},
    {"measure/measure-frontchest-dist-decr|incr", {1437,8125}},
    {"measure/measure-bust-circ-decr|incr", {8439,8455,8462,8446,8478,8494,8557,8510,8526,8542,10720,10601,10603,10602,10612,10611,10610,10613,10604,10605,10606,3942,3941,3940,3950,3947,3948,3949,3938,3939,3937,4065,1870,1854,1838,1885,1822,1806,1774,1790,1783,1767,1799,8471}},
    {"measure/measure-underbust-circ-decr|incr", {10750,10744,10724,10725,10748,10722,10640,10642,10641,10651,10650,10649,10652,10643,10644,10645,10646,10647,10648,3988,3987,3986,3985,3984,3983,3982,3992,3989,3990,3991,3980,3981,3979,4067,4098,4073,4072,4094,4100,4082,4088,4088}},
    {"measure/measure-waist-circ-decr|incr", {4121,10760,10757,10777,10776,10779,10780,10778,10781,10771,10773,10772,10775,10774,10814,10834,10816,10817,10818,10819,10820,10821,4181,4180,4179,4178,4177,4176,4175,4196,4173,4131,4132,4129,4130,4128,4138,4135,4137,4136,4133,4134,4108,4113,4118,4121}},
    {"measure/measure-napetowaist-dist-decr|incr", {1491,4181}},
    {"measure/measure-waisttohip-dist-decr|incr", {4121,4341}},
    {"measure/measure-shoulder-dist-decr|incr", {7478,8274}},
    {"measure/measure-hips-circ-decr|incr", {4341,10968,10969,10971,10970,10967,10928,10927,10925,10926,10923,10924,10868,10875,10861,10862,4228,4227,4226,4242,4234,4294,4293,4296,4295,4297,4298,4342,4345,4346,4344,4343,4361,4341}},
    {"measure/measure-upperleg-height-decr|incr", {10970,11230}},
    {"measure/measure-thigh-circ-decr|incr", {11071,11080,11081,11086,11076,11077,11074,11075,11072,11073,11069,11070,11087,11085,11084,12994,11083,11082,11079,11071}},
    {"measure/measure-lowerleg-height-decr|incr", {11225,12820}},
    {"measure/measure-calf-circ-decr|incr", {11339,11336,11353,11351,11350,13008,11349,11348,11345,11337,11344,11346,11347,11352,11342,11343,11340,11341,11338,11339}},
    {"measure/measure-ankle-circ-decr|incr", {11460,11464,11458,11459,11419,11418,12958,12965,12960,12963,12961,12962,12964,12927,13028,12957,11463,11461,11457,11460}},
    {"measure/measure-knee-circ-decr|incr", {11223,11230,11232,11233,11238,11228,11229,11226,11227,11224,11225,11221,11222,11239,11237,11236,13002,11235,11234,11223}},
};

float measureCm(const Body& b, const std::string& name) {
    for (const auto& r : RULERS) {
        if (name != r.first) continue;
        float m = 0;
        int a = r.second[0];
        for (int v : r.second) {
            const float* p = &b.pos[(size_t)a * 3]; const float* q = &b.pos[(size_t)v * 3];
            m += std::sqrt((p[0] - q[0]) * (p[0] - q[0]) + (p[1] - q[1]) * (p[1] - q[1]) + (p[2] - q[2]) * (p[2] - q[2]));
            a = v;
        }
        return m * 10;   // decimetres to centimetres
    }
    return -1;
}

bool jointPos(const Body& b, const std::string& joint, float out[3]) {
    auto it = b.joints.find(joint);
    if (it == b.joints.end() || it->second.empty()) return false;
    out[0] = out[1] = out[2] = 0;
    for (int v : it->second) for (int c = 0; c < 3; ++c) out[c] += b.pos[(size_t)v * 3 + c];
    for (int c = 0; c < 3; ++c) out[c] /= it->second.size();
    return true;
}

float ageYears(const Body& b) {
    return b.age < 0.5f ? 1 + (25 - 1) * 2 * b.age : 25 + (90 - 25) * 2 * (b.age - 0.5f);
}

float heightCm(const Body& b) {
    // the visible body's height (MakeHuman's units are decimetres)
    float lo = 1e30f, hi = -1e30f;
    for (int v : b.tri) { lo = std::min(lo, b.pos[(size_t)v * 3 + 1]); hi = std::max(hi, b.pos[(size_t)v * 3 + 1]); }
    return (hi - lo) * 10;
}

// ---------- MakeHuman's .mhm ----------
bool saveMhm(const Body& b, const std::string& path, const std::string& name, std::string& err) {
    std::ofstream o(fs::u8path(path));
    if (!o) { err = "Couldn't write " + path; return false; }
    o << "# Written by Pygmalion\nversion v1.1.1\nname " << name << "\ncamera 0.0 0.0 0.0 0.0 0.0 1.0\n";
    char buf[512];
    for (const auto& m : b.mods) {
        if (!m.macro && std::fabs(m.value - m.defaultValue) < 1e-6f) continue;
        snprintf(buf, sizeof buf, "modifier %s %f\n", m.fullName().c_str(), m.value);
        o << buf;
    }
    // the gear, as MakeHuman's libraries write it: "<library> <name> <uuid>", then each one's material
    for (const auto& p : b.proxies) o << (p.kind == "proxymeshes" ? "proxy" : p.kind) << " " << p.name << " " << p.uuid << "\n";
    o << "clothesHideFaces " << (b.hideUnderClothes ? "True" : "False") << "\n";
    o << "skinMaterial " << (b.skinMaterial.empty() ? std::string("skins/default.mhmat") : relPath(b, b.skinMaterial)) << "\n";
    for (const auto& p : b.proxies) if (!p.material.empty()) o << "material " << p.name << " " << p.uuid << " " << relPath(b, p.material) << "\n";
    o << "subdivide False\n";
    return true;
}

bool loadMhm(Body& b, const std::string& path, std::string& err) {
    std::ifstream in(fs::u8path(path));
    if (!in) { err = "Couldn't read " + path; return false; }
    resetAll(b);
    std::string line;
    int n = 0;
    std::vector<std::array<std::string, 3>> wearLater, matLater;
    std::string skinLater;
    float eth[3] = {-1, -1, -1};
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::istringstream ls(line); std::string k, name; float v;
        ls >> k;
        static const std::set<std::string> gear = {"clothes", "hair", "eyebrows", "eyelashes", "teeth", "tongue", "eyes", "proxy"};
        if (gear.count(k)) { std::string nm, id; ls >> nm >> id; wearLater.push_back({k, nm, id}); continue; }
        if (k == "skinMaterial") { std::string f; std::getline(ls >> std::ws, f); skinLater = f; continue; }
        if (k == "clothesHideFaces") { std::string f; ls >> f; b.hideUnderClothes = f != "False"; continue; }
        if (k == "material") { std::string nm, id, f; ls >> nm >> id; std::getline(ls >> std::ws, f); matLater.push_back({nm, id, f}); continue; }
        if (k != "modifier" || !(ls >> name >> v)) continue;
        auto it = b.modByName.find(name);
        if (it == b.modByName.end()) continue;
        Modifier& m = b.mods[it->second];
        if (m.macro) {
            // the three ethnicities are taken as they are (they were saved adding up to 1)
            if (m.variable == "African") eth[0] = v; else if (m.variable == "Asian") eth[1] = v; else if (m.variable == "Caucasian") eth[2] = v;
            else if (float* f = macroField(b, m.variable)) *f = std::clamp(v, 0.0f, 1.0f);
        } else m.value = std::clamp(v, m.min(), 1.0f);
        ++n;
    }
    if (eth[0] >= 0 && eth[1] >= 0 && eth[2] >= 0) { b.african = eth[0]; b.asian = eth[1]; b.caucasian = eth[2]; }
    for (auto& m : b.mods) if (m.macro) if (float* q = macroField(b, m.variable)) m.value = *q;
    if (!b.roots.empty()) {
        b.proxies.erase(std::remove_if(b.proxies.begin(), b.proxies.end(), [&](const Proxy& p) { return !wearLater.empty() || p.kind != "eyes"; }), b.proxies.end());
        std::string e2;
        for (const auto& g : wearLater) { const std::string f = findGear(b, g[0], g[1], g[2]); if (!f.empty()) wear(b, f, e2); }
        if (!worn(b, "eyes")) { const std::string f = findGear(b, "eyes", "HighPolyEyes", "2c12f43b-1303-432c-b7ce-d78346baf2e6"); if (!f.empty()) wear(b, f, e2); }
        for (const auto& m : matLater) {
            const std::string f = absPath(b, m[2]);
            if (f.empty()) continue;
            for (auto& p : b.proxies) if (p.uuid == m[1] || p.name == m[0]) { p.material = f; loadMaterial(f, p.texture, p.transparent); }
        }
        const bool plain = skinLater.empty() || fs::u8path(skinLater).filename() == "default.mhmat";
        setSkin(b, plain ? std::string() : absPath(b, skinLater));
    }
    if (!n) { err = "No MakeHuman settings in that file"; return false; }
    return true;
}

// ---------- words to sliders ----------
std::string describe(Body& b, const std::string& prompt) {
    std::string t = " " + prompt + " ";
    for (auto& c : t) c = (char)tolower((unsigned char)c);
    for (auto& c : t) if (c == ',' || c == '.' || c == ';' || c == '!' || c == '?') c = ' ';
    std::vector<std::string> said;
    auto has = [&](const char* w) { return t.find(std::string(" ") + w + " ") != std::string::npos; };
    auto setAgeYears = [&](float y) {
        y = std::clamp(y, 1.0f, 90.0f);
        setMacro(b, "Age", y < 25 ? (y - 1) / ((25 - 1) * 2.0f) : (y - 25) / ((90 - 25) * 2.0f) + 0.5f);
    };
    auto mod = [&](const char* full, float v) { auto it = b.modByName.find(full); if (it != b.modByName.end()) setModifier(b, it->second, v); };
    // a strength word just before: "very tall", "slightly fat"
    auto strength = [&](const char* w) {
        const size_t at = t.find(std::string(" ") + w + " ");
        const std::string before = at == std::string::npos ? "" : t.substr(at > 14 ? at - 14 : 0, at > 14 ? 14 : at);
        if (before.find("very") != std::string::npos || before.find("extremely") != std::string::npos || before.find("really") != std::string::npos) return 1.0f;
        if (before.find("slightly") != std::string::npos || before.find("a bit") != std::string::npos || before.find("somewhat") != std::string::npos || before.find("a little") != std::string::npos) return 0.35f;
        return 0.6f;
    };
    // "big" alone is a build; "a big nose" is a nose
    auto bigBuild = [&]() {
        for (size_t at = t.find(" big "); at != std::string::npos; at = t.find(" big ", at + 1)) {
            std::string next = t.substr(at + 5, 12);
            bool part = false;
            for (const char* w : {"nose", "eyes", "ears", "lips", "mouth", "chin", "jaw", "head", "breast", "belly", "hands", "feet", "butt", "bum"}) if (next.rfind(w, 0) == 0) part = true;
            if (!part) return true;
        }
        return false;
    };
    // who
    for (const char* w : {"woman", "female", "girl", "lady", "she", "her", "mother", "grandmother", "wife"}) if (has(w)) { setMacro(b, "Gender", 0); said.push_back("female"); break; }
    for (const char* w : {"man", "male", "boy", "guy", "he", "him", "father", "grandfather", "husband", "gentleman"}) if (has(w)) { setMacro(b, "Gender", 1); said.push_back("male"); break; }
    if (has("androgynous")) { setMacro(b, "Gender", 0.5f); said.push_back("androgynous"); }
    // age: "34 years old", "a 34-year-old", or words
    std::smatch m;
    if (std::regex_search(t, m, std::regex(R"((\d{1,2})[ -]*(years?|year-old|yo\b))"))) { setAgeYears((float)atof(m[1].str().c_str())); said.push_back(m[1].str() + " years old"); }
    else {
        struct A { const char* w; float y; };
        for (const A& a : {A{"baby", 1}, A{"toddler", 3}, A{"child", 8}, A{"kid", 8}, A{"girl", 12}, A{"boy", 12}, A{"teen", 16}, A{"teenager", 16}, A{"teenage", 16},
                          A{"young", 22}, A{"adult", 30}, A{"middle-aged", 48}, A{"middle", 48}, A{"mature", 55}, A{"old", 70}, A{"elderly", 75}, A{"ancient", 90},
                          A{"grandmother", 72}, A{"grandfather", 72}})
            if (has(a.w)) { setAgeYears(a.y); said.push_back(std::string(a.w)); break; }
    }
    // build
    for (const char* w : {"thin", "skinny", "slim", "slender", "lean", "petite"}) if (has(w)) { setMacro(b, "Weight", 0.5f - 0.4f * strength(w)); said.push_back(w); break; }
    for (const char* w : {"fat", "heavy", "overweight", "chubby", "plump", "curvy"}) if (has(w)) { setMacro(b, "Weight", 0.5f + 0.4f * strength(w)); said.push_back(w); break; }
    if (bigBuild()) { setMacro(b, "Weight", 0.5f + 0.4f * strength("big")); said.push_back("big"); }
    if (has("obese")) { setMacro(b, "Weight", 1); said.push_back("obese"); }
    for (const char* w : {"muscular", "athletic", "strong", "buff", "ripped", "bodybuilder", "fit"}) if (has(w)) { setMacro(b, "Muscle", 0.5f + 0.5f * strength(w)); said.push_back(w); break; }
    for (const char* w : {"weak", "soft", "frail", "flabby"}) if (has(w)) { setMacro(b, "Muscle", 0.5f - 0.5f * strength(w)); said.push_back(w); break; }
    for (const char* w : {"tall", "giant", "lanky"}) if (has(w)) { setMacro(b, "Height", 0.5f + 0.35f * strength(w)); said.push_back(w); break; }
    for (const char* w : {"short", "small", "tiny"}) if (has(w)) { setMacro(b, "Height", 0.5f - 0.35f * strength(w)); said.push_back(w); break; }
    if (has("ideal") || has("model") || has("perfect")) { setMacro(b, "BodyProportions", 1); said.push_back("ideal proportions"); }
    // where from
    for (const char* w : {"african", "black"}) if (has(w)) { setMacro(b, "African", 1); said.push_back("african"); break; }
    for (const char* w : {"asian", "east-asian", "chinese", "japanese", "korean"}) if (has(w)) { setMacro(b, "Asian", 1); said.push_back("asian"); break; }
    for (const char* w : {"caucasian", "white", "european"}) if (has(w)) { setMacro(b, "Caucasian", 1); said.push_back("caucasian"); break; }
    if (has("mixed")) { setMacro(b, "African", 1 / 3.0f); setMacro(b, "Asian", 0.5f); said.push_back("mixed"); }
    // details
    if (t.find("big breasts") != std::string::npos || t.find("large breasts") != std::string::npos || has("busty")) { setMacro(b, "BreastSize", 0.9f); said.push_back("big breasts"); }
    if (t.find("small breasts") != std::string::npos || has("flat-chested")) { setMacro(b, "BreastSize", 0.15f); said.push_back("small breasts"); }
    if (t.find("big nose") != std::string::npos || t.find("large nose") != std::string::npos) { mod("nose/nose-scale-vert-decr|incr", 0.6f); mod("nose/nose-scale-horiz-decr|incr", 0.5f); said.push_back("big nose"); }
    if (t.find("small nose") != std::string::npos) { mod("nose/nose-scale-vert-decr|incr", -0.5f); mod("nose/nose-scale-horiz-decr|incr", -0.4f); said.push_back("small nose"); }
    if (t.find("round face") != std::string::npos) { mod("head/head-round", 0.8f); said.push_back("round face"); }
    if (t.find("oval face") != std::string::npos) { mod("head/head-oval", 0.8f); said.push_back("oval face"); }
    if (t.find("square jaw") != std::string::npos || t.find("strong jaw") != std::string::npos) { mod("head/head-square", 0.7f); said.push_back("square jaw"); }
    if (t.find("long neck") != std::string::npos) { mod("neck/neck-scale-vert-decr|incr", 0.6f); said.push_back("long neck"); }
    if (t.find("broad shoulders") != std::string::npos || t.find("wide shoulders") != std::string::npos) { mod("torso/torso-scale-horiz-decr|incr", 0.5f); said.push_back("broad shoulders"); }
    if (t.find("wide hips") != std::string::npos) { mod("hip/hip-scale-horiz-decr|incr", 0.6f); said.push_back("wide hips"); }
    if (has("pregnant")) { mod("stomach/stomach-pregnant-decr|incr", 0.8f); said.push_back("pregnant"); }
    if (t.find("big belly") != std::string::npos || has("potbelly")) { mod("stomach/stomach-pregnant-decr|incr", 0.4f); said.push_back("belly"); }
    std::string r;
    for (size_t i = 0; i < said.size(); ++i) r += (i ? ", " : "") + said[i];
    return r;
}

void randomize(Body& b, float amount, bool macros, bool face, bool body, unsigned seed) {
    std::mt19937 rng(seed);
    std::normal_distribution<float> n(0, 1);
    auto around = [&](float base, float lo, float hi, float spread) { return std::clamp(base + n(rng) * spread * amount, lo, hi); };
    if (macros) {
        setMacro(b, "Gender", std::uniform_real_distribution<float>(0, 1)(rng) < 0.5f ? around(0.05f, 0, 1, 0.1f) : around(0.95f, 0, 1, 0.1f));
        setMacro(b, "Age", around(0.55f, 0.25f, 1, 0.2f));
        setMacro(b, "Muscle", around(0.5f, 0, 1, 0.25f));
        setMacro(b, "Weight", around(0.5f, 0, 1, 0.25f));
        setMacro(b, "Height", around(0.5f, 0, 1, 0.25f));
        setMacro(b, "BodyProportions", around(0.5f, 0, 1, 0.3f));
        setMacro(b, "BreastSize", around(0.5f, 0, 1, 0.3f));
        setMacro(b, "BreastFirmness", around(0.5f, 0, 1, 0.3f));
        float e[3] = {std::uniform_real_distribution<float>(0, 1)(rng), std::uniform_real_distribution<float>(0, 1)(rng), std::uniform_real_distribution<float>(0, 1)(rng)};
        const float s = e[0] + e[1] + e[2];
        b.african = e[0] / s; b.asian = e[1] / s; b.caucasian = e[2] / s;
        for (auto& m : b.mods) if (m.macro) if (float* q = macroField(b, m.variable)) m.value = *q;
    }
    const bool sym = b.symmetry;
    for (size_t i = 0; i < b.mods.size(); ++i) {
        Modifier& m = b.mods[i];
        if (m.macro) continue;
        static const std::set<std::string> faceGroups = {"head", "forehead", "eyebrows", "neck", "eyes", "nose", "mouth", "ears", "chin", "cheek"};
        const bool isFace = faceGroups.count(m.group) > 0;
        if ((isFace && !face) || (!isFace && !body) || m.group == "measure" || m.group == "genitals") continue;
        if (sym && m.name.find("l-") != std::string::npos && mirrorOf(b, m) >= 0 && m.name.rfind("l-", 0) == 0) continue;   // the right one sets both
        setModifier(b, (int)i, around(m.defaultValue, m.min(), 1, 0.18f));
    }
}

}
