// Pygmalion - the exports with the skeleton (see export.h).
#include "export.h"

#include <stb_image_write.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <sstream>

namespace fs = std::filesystem;
using Skel::Mat4;

namespace Export {

// ---------- the parts ----------
static void normalsOf(const std::vector<float>& pos, const std::vector<int>& tri, std::vector<float>& n) {
    n.assign(pos.size(), 0);
    for (size_t t = 0; t + 2 < tri.size(); t += 3) {
        const float* a = &pos[(size_t)tri[t] * 3]; const float* b = &pos[(size_t)tri[t + 1] * 3]; const float* c = &pos[(size_t)tri[t + 2] * 3];
        const float u[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]}, v[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
        const float f[3] = {u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]};
        for (int k = 0; k < 3; ++k) for (int q = 0; q < 3; ++q) n[(size_t)tri[t + k] * 3 + q] += f[q];
    }
    for (size_t i = 0; i < n.size(); i += 3) {
        const float l = std::sqrt(n[i] * n[i] + n[i + 1] * n[i + 1] + n[i + 2] * n[i + 2]);
        if (l > 0) for (int q = 0; q < 3; ++q) n[i + q] /= l; else n[i + 1] = 1;
    }
}

// the strongest four, adding up to 1
static std::vector<std::pair<int, float>> topFour(std::map<int, float>& acc) {
    std::vector<std::pair<int, float>> v(acc.begin(), acc.end());
    std::sort(v.begin(), v.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
    if (v.size() > 4) v.resize(4);
    float s = 0; for (auto& p : v) s += p.second;
    if (s > 0) for (auto& p : v) p.second /= s;
    v.erase(std::remove_if(v.begin(), v.end(), [](const auto& p) { return p.second < 1e-4f; }), v.end());
    return v;
}

std::vector<Part> parts(const Human::Body& b, const Skel::Skeleton* s, bool posedMesh) {
    std::vector<Part> out;
    // the skeleton's weights per body vertex
    std::vector<std::map<int, float>> bw;
    if (s) {
        bw.resize(b.pos.size() / 3);
        for (size_t bi = 0; bi < s->bones.size() && bi < s->weights.size(); ++bi)
            for (const auto& [v, w] : s->weights[bi]) if ((size_t)v < bw.size()) bw[v][(int)bi] += w;
    }
    // one mesh: vertices split by (vertex, uv) pair
    auto build = [&](const std::string& name, const std::string& tex, const std::vector<float>& pos, const std::vector<int>& tri,
                     const std::function<void(size_t, float*)>& uvOf, const std::function<std::map<int, float>(int)>& weightOf,
                     const std::vector<char>* hidden) {
        Part p; p.name = name; p.texture = tex;
        std::vector<float> n; normalsOf(pos, tri, n);
        std::map<std::pair<int, std::pair<float, float>>, int> seen;
        for (size_t t = 0; t + 2 < tri.size(); t += 3) {
            if (hidden && ((*hidden)[tri[t]] || (*hidden)[tri[t + 1]] || (*hidden)[tri[t + 2]])) continue;
            for (size_t c = t; c < t + 3; ++c) {
                float uv[2]; uvOf(c, uv);
                const int v = tri[c];
                const auto key = std::make_pair(v, std::make_pair(uv[0], uv[1]));
                auto it = seen.find(key);
                if (it == seen.end()) {
                    it = seen.emplace(key, (int)(p.pos.size() / 3)).first;
                    p.pos.insert(p.pos.end(), &pos[(size_t)v * 3], &pos[(size_t)v * 3] + 3);
                    p.nrm.insert(p.nrm.end(), &n[(size_t)v * 3], &n[(size_t)v * 3] + 3);
                    p.uv.insert(p.uv.end(), {uv[0], uv[1]});
                    if (s) { auto acc = weightOf(v); p.weights.push_back(topFour(acc)); }
                }
                p.tri.push_back(it->second);
            }
        }
        if (!p.tri.empty()) out.push_back(std::move(p));
    };
    const std::vector<char> hid = Human::hiddenVerts(b);
    const std::vector<float>& bodyPos = posedMesh && !b.shown.empty() ? b.shown : b.pos;
    build("body", b.skinTexture, bodyPos, b.tri,
          [&](size_t c, float* uv) { const int u = b.triUv[c]; if (u >= 0 && (size_t)u * 2 + 1 < b.uv.size()) { uv[0] = b.uv[(size_t)u * 2]; uv[1] = b.uv[(size_t)u * 2 + 1]; } else uv[0] = uv[1] = 0; },
          [&](int v) { return bw[v]; }, &hid);
    for (const auto& px : b.proxies) {
        std::vector<float> fit; Human::fitProxy(b, px, fit, !posedMesh);
        std::vector<int> corners(px.tri.begin(), px.tri.end());
        build(px.name.empty() ? px.kind : px.name, px.kind == "proxymeshes" ? b.skinTexture : px.texture, fit, corners,
              [&](size_t c, float* uv) { uv[0] = px.uv[c * 2]; uv[1] = px.uv[c * 2 + 1]; },
              [&](int v) {   // a proxy vertex moves with the body vertices it's fitted to
                  std::map<int, float> acc;
                  for (int k = 0; k < 3; ++k) {
                      const int r = px.ref[(size_t)v * 3 + k]; const float w = px.w[(size_t)v * 3 + k];
                      if (w == 0 || (size_t)r >= bw.size()) continue;
                      for (const auto& [bone, bwt] : bw[r]) acc[bone] += w * bwt;
                  }
                  return acc;
              }, nullptr);
    }
    return out;
}

std::vector<Part> withTexturesBeside(const std::vector<Part>& in, const std::string& path) {
    std::vector<Part> out = in;
    const fs::path dir = fs::u8path(path).parent_path();
    std::error_code ec;
    for (auto& p : out) {
        if (p.texture.empty()) continue;
        const fs::path src = fs::u8path(p.texture);
        if (!fs::exists(src, ec)) { p.texture.clear(); continue; }
        const fs::path dst = dir / src.filename();
        if (!fs::exists(dst, ec)) fs::copy_file(src, dst, ec);
        p.texture = src.filename().u8string();
    }
    return out;
}

// ---------- shared: the transform to the file's units ----------
struct Xf {
    float scale = 1, yOff = 0;
    void pt(const float* p, float* o) const { o[0] = p[0] * scale; o[1] = (p[1] + yOff) * scale; o[2] = p[2] * scale; }
    Mat4 m(const Mat4& a) const { Mat4 r = a; r[12] = a[12] * scale; r[13] = (a[13] + yOff) * scale; r[14] = a[14] * scale; return r; }
};
static Xf xfOf(const std::vector<Part>& parts, const Options& o) {
    Xf x; x.scale = o.scale;
    if (o.feetOnGround) {
        float lo = 1e30f;
        for (const auto& p : parts) for (size_t i = 1; i < p.pos.size(); i += 3) lo = std::min(lo, p.pos[i]);
        if (lo < 1e29f) x.yOff = -lo;
    }
    return x;
}
// each bone's global transform in the file (bind: the rest; scene: posed or rest)
static void boneMats(const Skel::Skeleton& s, const Xf& x, bool posed, std::vector<Mat4>& bind, std::vector<Mat4>& scene, std::vector<Mat4>& local) {
    bind.clear(); scene.clear(); local.clear();
    for (const auto& b : s.bones) { bind.push_back(x.m(b.restGlobal)); scene.push_back(x.m(posed ? b.poseGlobal : b.restGlobal)); }
    for (size_t i = 0; i < s.bones.size(); ++i) {
        const int p = s.bones[i].parent;
        local.push_back(p >= 0 ? Skel::multiply(Skel::inverseRigid(scene[p]), scene[i]) : scene[i]);
    }
}
static std::string safeId(const std::string& n) {
    std::string o;
    for (char c : n) o += (isalnum((unsigned char)c) || c == '_' || c == '-') ? c : '_';
    if (o.empty() || isdigit((unsigned char)o[0])) o = "_" + o;
    return o;
}
static std::string xmlEsc(const std::string& s) {
    std::string o;
    for (char c : s) { if (c == '&') o += "&amp;"; else if (c == '<') o += "&lt;"; else if (c == '>') o += "&gt;"; else if (c == '"') o += "&quot;"; else o += c; }
    return o;
}

// ---------- Collada ----------
static void rowMajor(std::ostream& o, const Mat4& m) {
    char b[64];
    for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c) { snprintf(b, sizeof b, "%g ", m[c * 4 + r]); o << b; }
}

bool saveCollada(const std::vector<Part>& parts, const Skel::Skeleton* s, const std::string& path, const Options& op, std::string& err) {
    std::ofstream o(fs::u8path(path));
    if (!o) { err = "Couldn't write " + path; return false; }
    const Xf x = xfOf(parts, op);
    char ts[32]; { std::time_t t = std::time(nullptr); std::strftime(ts, sizeof ts, "%Y-%m-%dT%H:%M:%S", std::gmtime(&t)); }
    o << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<COLLADA xmlns=\"http://www.collada.org/2005/11/COLLADASchema\" version=\"1.4.1\">\n";
    o << "  <asset><contributor><authoring_tool>Pygmalion</authoring_tool></contributor><created>" << ts << "</created><modified>" << ts
      << "</modified><unit name=\"unit\" meter=\"" << op.cmPerUnit / 100 << "\"/><up_axis>Y_UP</up_axis></asset>\n";
    o << "  <library_images>\n";
    for (size_t i = 0; i < parts.size(); ++i) if (!parts[i].texture.empty()) o << "    <image id=\"img" << i << "\" name=\"img" << i << "\"><init_from>" << xmlEsc(parts[i].texture) << "</init_from></image>\n";
    o << "  </library_images>\n  <library_effects>\n";
    for (size_t i = 0; i < parts.size(); ++i) {
        o << "    <effect id=\"fx" << i << "\"><profile_COMMON>";
        if (!parts[i].texture.empty())
            o << "<newparam sid=\"s" << i << "-surface\"><surface type=\"2D\"><init_from>img" << i << "</init_from></surface></newparam><newparam sid=\"s" << i
              << "-sampler\"><sampler2D><source>s" << i << "-surface</source></sampler2D></newparam>";
        o << "<technique sid=\"common\"><phong><diffuse>";
        if (!parts[i].texture.empty()) o << "<texture texture=\"s" << i << "-sampler\" texcoord=\"UVMap\"/>"; else o << "<color>0.8 0.8 0.8 1</color>";
        o << "</diffuse></phong></technique></profile_COMMON></effect>\n";
    }
    o << "  </library_effects>\n  <library_materials>\n";
    for (size_t i = 0; i < parts.size(); ++i) o << "    <material id=\"mat" << i << "\" name=\"" << xmlEsc(parts[i].name) << "\"><instance_effect url=\"#fx" << i << "\"/></material>\n";
    o << "  </library_materials>\n  <library_geometries>\n";
    char b[96];
    for (size_t i = 0; i < parts.size(); ++i) {
        const Part& p = parts[i];
        const size_t n = p.pos.size() / 3;
        const std::string g = "geo" + std::to_string(i);
        o << "    <geometry id=\"" << g << "\" name=\"" << xmlEsc(p.name) << "\"><mesh>\n";
        o << "      <source id=\"" << g << "-pos\"><float_array id=\"" << g << "-pos-a\" count=\"" << n * 3 << "\">";
        for (size_t v = 0; v < n; ++v) { float q[3]; x.pt(&p.pos[v * 3], q); snprintf(b, sizeof b, "%g %g %g ", q[0], q[1], q[2]); o << b; }
        o << "</float_array><technique_common><accessor source=\"#" << g << "-pos-a\" count=\"" << n << "\" stride=\"3\"><param name=\"X\" type=\"float\"/><param name=\"Y\" type=\"float\"/><param name=\"Z\" type=\"float\"/></accessor></technique_common></source>\n";
        o << "      <source id=\"" << g << "-nrm\"><float_array id=\"" << g << "-nrm-a\" count=\"" << n * 3 << "\">";
        for (size_t v = 0; v < n; ++v) { snprintf(b, sizeof b, "%.4f %.4f %.4f ", p.nrm[v * 3], p.nrm[v * 3 + 1], p.nrm[v * 3 + 2]); o << b; }
        o << "</float_array><technique_common><accessor source=\"#" << g << "-nrm-a\" count=\"" << n << "\" stride=\"3\"><param name=\"X\" type=\"float\"/><param name=\"Y\" type=\"float\"/><param name=\"Z\" type=\"float\"/></accessor></technique_common></source>\n";
        o << "      <source id=\"" << g << "-uv\"><float_array id=\"" << g << "-uv-a\" count=\"" << n * 2 << "\">";
        for (size_t v = 0; v < n; ++v) { snprintf(b, sizeof b, "%.5f %.5f ", p.uv[v * 2], p.uv[v * 2 + 1]); o << b; }
        o << "</float_array><technique_common><accessor source=\"#" << g << "-uv-a\" count=\"" << n << "\" stride=\"2\"><param name=\"S\" type=\"float\"/><param name=\"T\" type=\"float\"/></accessor></technique_common></source>\n";
        o << "      <vertices id=\"" << g << "-v\"><input semantic=\"POSITION\" source=\"#" << g << "-pos\"/></vertices>\n";
        o << "      <triangles material=\"mat" << i << "\" count=\"" << p.tri.size() / 3 << "\"><input semantic=\"VERTEX\" source=\"#" << g << "-v\" offset=\"0\"/><input semantic=\"NORMAL\" source=\"#" << g
          << "-nrm\" offset=\"0\"/><input semantic=\"TEXCOORD\" source=\"#" << g << "-uv\" offset=\"0\" set=\"0\"/><p>";
        for (int t : p.tri) o << t << ' ';
        o << "</p></triangles>\n    </mesh></geometry>\n";
    }
    o << "  </library_geometries>\n";
    std::vector<Mat4> bind, scene, local;
    if (s) {
        boneMats(*s, x, op.posed, bind, scene, local);
        if (op.posed) bind = scene;   // the mesh is posed: bound as it stands
        o << "  <library_controllers>\n";
        for (size_t i = 0; i < parts.size(); ++i) {
            const Part& p = parts[i];
            const std::string c = "ctl" + std::to_string(i);
            const size_t nb = s->bones.size();
            o << "    <controller id=\"" << c << "\"><skin source=\"#geo" << i << "\"><bind_shape_matrix>1 0 0 0 0 1 0 0 0 0 1 0 0 0 0 1</bind_shape_matrix>\n";
            o << "      <source id=\"" << c << "-joints\"><Name_array id=\"" << c << "-joints-a\" count=\"" << nb << "\">";
            for (const auto& bn : s->bones) o << safeId(bn.name) << ' ';
            o << "</Name_array><technique_common><accessor source=\"#" << c << "-joints-a\" count=\"" << nb << "\" stride=\"1\"><param name=\"JOINT\" type=\"name\"/></accessor></technique_common></source>\n";
            o << "      <source id=\"" << c << "-bind\"><float_array id=\"" << c << "-bind-a\" count=\"" << nb * 16 << "\">";
            for (size_t k = 0; k < nb; ++k) rowMajor(o, Skel::inverseRigid(bind[k]));
            o << "</float_array><technique_common><accessor source=\"#" << c << "-bind-a\" count=\"" << nb << "\" stride=\"16\"><param name=\"TRANSFORM\" type=\"float4x4\"/></accessor></technique_common></source>\n";
            std::vector<float> ws; std::ostringstream vc, vv;
            for (const auto& wl : p.weights) { vc << wl.size() << ' '; for (const auto& [bone, w] : wl) { vv << bone << ' ' << ws.size() << ' '; ws.push_back(w); } }
            o << "      <source id=\"" << c << "-w\"><float_array id=\"" << c << "-w-a\" count=\"" << ws.size() << "\">";
            for (float w : ws) { snprintf(b, sizeof b, "%.5f ", w); o << b; }
            o << "</float_array><technique_common><accessor source=\"#" << c << "-w-a\" count=\"" << ws.size() << "\" stride=\"1\"><param name=\"WEIGHT\" type=\"float\"/></accessor></technique_common></source>\n";
            o << "      <joints><input semantic=\"JOINT\" source=\"#" << c << "-joints\"/><input semantic=\"INV_BIND_MATRIX\" source=\"#" << c << "-bind\"/></joints>\n";
            o << "      <vertex_weights count=\"" << p.weights.size() << "\"><input semantic=\"JOINT\" source=\"#" << c << "-joints\" offset=\"0\"/><input semantic=\"WEIGHT\" source=\"#" << c
              << "-w\" offset=\"1\"/><vcount>" << vc.str() << "</vcount><v>" << vv.str() << "</v></vertex_weights>\n";
            o << "    </skin></controller>\n";
        }
        o << "  </library_controllers>\n";
    }
    o << "  <library_visual_scenes><visual_scene id=\"Scene\" name=\"Scene\">\n";
    std::string root;
    if (s) {
        std::function<void(int, int)> node = [&](int i, int d) {
            const std::string pad(4 + d * 2, ' ');
            const std::string id = safeId(s->bones[i].name);
            if (root.empty()) root = id;
            o << pad << "<node id=\"" << id << "\" name=\"" << xmlEsc(s->bones[i].name) << "\" sid=\"" << id << "\" type=\"JOINT\"><matrix sid=\"transform\">";
            rowMajor(o, local[i]);
            o << "</matrix>\n";
            for (size_t k = 0; k < s->bones.size(); ++k) if (s->bones[k].parent == i) node((int)k, d + 1);
            o << pad << "</node>\n";
        };
        o << "    <node id=\"Armature\" name=\"Armature\" type=\"NODE\">\n";
        for (size_t i = 0; i < s->bones.size(); ++i) if (s->bones[i].parent < 0) node((int)i, 1);
        o << "    </node>\n";
    }
    for (size_t i = 0; i < parts.size(); ++i) {
        o << "    <node id=\"obj" << i << "\" name=\"" << xmlEsc(parts[i].name) << "\" type=\"NODE\">";
        const std::string bm = "<bind_material><technique_common><instance_material symbol=\"mat" + std::to_string(i) + "\" target=\"#mat" + std::to_string(i) +
                               "\"><bind_vertex_input semantic=\"UVMap\" input_semantic=\"TEXCOORD\" input_set=\"0\"/></instance_material></technique_common></bind_material>";
        if (s) o << "<instance_controller url=\"#ctl" << i << "\"><skeleton>#" << root << "</skeleton>" << bm << "</instance_controller>";
        else o << "<instance_geometry url=\"#geo" << i << "\">" << bm << "</instance_geometry>";
        o << "</node>\n";
    }
    o << "  </visual_scene></library_visual_scenes>\n  <scene><instance_visual_scene url=\"#Scene\"/></scene>\n</COLLADA>\n";
    if (!o) { err = "Couldn't finish " + path; return false; }
    return true;
}

// ---------- FBX (binary 7.4, as MakeHuman's encode_bin.py writes it) ----------
namespace {
struct Prop {
    char type;
    std::string data;   // little-endian bytes (arrays: the elements)
    uint32_t count = 0;
};
struct Node {
    std::string name;
    std::vector<Prop> props;
    std::vector<Node> kids;
    Node(std::string n = "") : name(std::move(n)) {}
    template <class T> static std::string raw(const T& v) { return std::string((const char*)&v, sizeof v); }
    Node& I(int32_t v) { props.push_back({'I', raw(v)}); return *this; }
    Node& L(int64_t v) { props.push_back({'L', raw(v)}); return *this; }
    Node& D(double v) { props.push_back({'D', raw(v)}); return *this; }
    Node& C(bool v) { props.push_back({'C', std::string(1, v ? 'T' : 'F')}); return *this; }   // (Blender reads any non-zero byte)
    Node& S(const std::string& v) { props.push_back({'S', v}); return *this; }
    Node& R(const std::string& v) { props.push_back({'R', v}); return *this; }
    Node& Ad(const std::vector<double>& v) { Prop p{'d', std::string((const char*)v.data(), v.size() * 8), (uint32_t)v.size()}; props.push_back(p); return *this; }
    Node& Ai(const std::vector<int32_t>& v) { Prop p{'i', std::string((const char*)v.data(), v.size() * 4), (uint32_t)v.size()}; props.push_back(p); return *this; }
    Node& kid(const Node& n) { kids.push_back(n); return kids.back(); }
    Node& add(const std::string& n) { kids.emplace_back(n); return kids.back(); }
};
void put32(std::string& o, uint32_t v) { o.append((const char*)&v, 4); }
void writeNode(std::string& o, const Node& n) {
    const size_t start = o.size();
    put32(o, 0); put32(o, (uint32_t)n.props.size()); put32(o, 0);
    o += (char)n.name.size(); o += n.name;
    const size_t ps = o.size();
    for (const auto& p : n.props) {
        o += p.type;
        if (p.type == 'S' || p.type == 'R') { put32(o, (uint32_t)p.data.size()); o += p.data; }
        else if (p.type == 'd' || p.type == 'i') { put32(o, p.count); put32(o, 0); put32(o, (uint32_t)p.data.size()); o += p.data; }
        else o += p.data;
    }
    const uint32_t plen = (uint32_t)(o.size() - ps);
    memcpy(&o[start + 8], &plen, 4);
    if (!n.kids.empty() || n.props.empty()) {
        for (const auto& k : n.kids) writeNode(o, k);
        o.append(13, '\0');
    }
    const uint32_t end = (uint32_t)o.size();
    memcpy(&o[start], &end, 4);
}
// a Properties70 "P" entry
Node P(const std::string& name, const std::string& type, const std::string& label, const std::string& flags) { Node p("P"); p.S(name).S(type).S(label).S(flags); return p; }
std::string fbxName(const std::string& n, const char* cls) { return n + std::string("\x00\x01", 2) + cls; }
// R = Rz Ry Rx (FBX's XYZ order), in degrees
void eulerXYZ(const Mat4& m, double& x, double& y, double& z) {
    const double r20 = m[0 * 4 + 2];
    y = std::asin(std::clamp(-r20, -1.0, 1.0));
    if (std::fabs(r20) < 0.9999) { x = std::atan2(m[1 * 4 + 2], m[2 * 4 + 2]); z = std::atan2(m[0 * 4 + 1], m[0 * 4 + 0]); }
    else { x = std::atan2(-m[2 * 4 + 1], m[1 * 4 + 1]); z = 0; }
    const double D = 180 / 3.14159265358979;
    x *= D; y *= D; z *= D;
}
std::vector<double> m16(const Mat4& m) { return std::vector<double>(m.begin(), m.end()); }
}

bool saveFbx(const std::vector<Part>& parts, const Skel::Skeleton* s, const std::string& path, const Options& op, std::string& err) {
    const Xf x = xfOf(parts, op);
    int64_t nextId = 1000000;
    auto id = [&]() { return nextId++; };
    Node objects("Objects"), conns("Connections");
    auto OO = [&](int64_t c, int64_t p) { conns.add("C").S("OO").L(c).L(p); };
    auto OP = [&](int64_t c, int64_t p, const std::string& prop) { conns.add("C").S("OP").L(c).L(p).S(prop); };
    int nGeo = 0, nModel = 0, nMat = 0, nTex = 0, nAttr = 0, nDef = 0, nPose = 0;
    std::vector<Mat4> bind, scene, local;
    std::vector<int64_t> boneIds;
    if (s) {
        boneMats(*s, x, op.posed, bind, scene, local);
        for (size_t i = 0; i < s->bones.size(); ++i) {
            const int64_t attr = id(), model = id();
            boneIds.push_back(model);
            Node& a = objects.add("NodeAttribute"); a.L(attr).S(fbxName(s->bones[i].name, "NodeAttribute")).S("LimbNode");
            a.add("Properties70").kid(P("Size", "double", "Number", "").D(1.0 * op.scale * 10));
            a.add("TypeFlags").S("Skeleton");
            ++nAttr;
            Node& m = objects.add("Model"); m.L(model).S(fbxName(s->bones[i].name, "Model")).S("LimbNode");
            m.add("Version").I(232);
            Node& pr = m.add("Properties70");
            double rx, ry, rz; eulerXYZ(local[i], rx, ry, rz);
            pr.kid(P("Lcl Translation", "Lcl Translation", "", "A").D(local[i][12]).D(local[i][13]).D(local[i][14]));
            pr.kid(P("Lcl Rotation", "Lcl Rotation", "", "A").D(rx).D(ry).D(rz));
            pr.kid(P("Lcl Scaling", "Lcl Scaling", "", "A").D(1).D(1).D(1));
            m.add("Shading").C(true);
            m.add("Culling").S("CullingOff");
            ++nModel;
            OO(attr, model);
        }
        for (size_t i = 0; i < s->bones.size(); ++i) OO(boneIds[i], s->bones[i].parent >= 0 ? boneIds[s->bones[i].parent] : 0);
    }
    std::vector<int64_t> meshModels;
    const fs::path dir = fs::u8path(path).parent_path();
    for (const auto& p : parts) {
        const size_t n = p.pos.size() / 3;
        const int64_t geo = id(), model = id(), mat = id();
        Node& g = objects.add("Geometry"); g.L(geo).S(fbxName(p.name, "Geometry")).S("Mesh");
        std::vector<double> v(n * 3); for (size_t k = 0; k < n; ++k) { float q[3]; x.pt(&p.pos[k * 3], q); v[k * 3] = q[0]; v[k * 3 + 1] = q[1]; v[k * 3 + 2] = q[2]; }
        g.add("Vertices").Ad(v);
        std::vector<int32_t> pvi(p.tri.begin(), p.tri.end());
        for (size_t k = 2; k < pvi.size(); k += 3) pvi[k] = -pvi[k] - 1;
        g.add("PolygonVertexIndex").Ai(pvi);
        g.add("GeometryVersion").I(124);
        std::vector<double> nr; for (int t : p.tri) for (int c = 0; c < 3; ++c) nr.push_back(p.nrm[(size_t)t * 3 + c]);
        Node& ln = g.add("LayerElementNormal"); ln.I(0);
        ln.add("Version").I(101); ln.add("Name").S(""); ln.add("MappingInformationType").S("ByPolygonVertex"); ln.add("ReferenceInformationType").S("Direct"); ln.add("Normals").Ad(nr);
        Node& lu = g.add("LayerElementUV"); lu.I(0);
        lu.add("Version").I(101); lu.add("Name").S("UVMap"); lu.add("MappingInformationType").S("ByPolygonVertex"); lu.add("ReferenceInformationType").S("IndexToDirect");
        lu.add("UV").Ad(std::vector<double>(p.uv.begin(), p.uv.end())); lu.add("UVIndex").Ai(std::vector<int32_t>(p.tri.begin(), p.tri.end()));
        Node& lm = g.add("LayerElementMaterial"); lm.I(0);
        lm.add("Version").I(101); lm.add("Name").S(""); lm.add("MappingInformationType").S("AllSame"); lm.add("ReferenceInformationType").S("IndexToDirect"); lm.add("Materials").Ai({0});
        Node& ly = g.add("Layer"); ly.I(0); ly.add("Version").I(100);
        for (const char* t : {"LayerElementNormal", "LayerElementUV", "LayerElementMaterial"}) { Node& le = ly.add("LayerElement"); le.add("Type").S(t); le.add("TypedIndex").I(0); }
        ++nGeo;
        Node& m = objects.add("Model"); m.L(model).S(fbxName(p.name, "Model")).S("Mesh");
        m.add("Version").I(232);
        m.add("Properties70").kid(P("Lcl Scaling", "Lcl Scaling", "", "A").D(1).D(1).D(1));
        m.add("Shading").C(true); m.add("Culling").S("CullingOff");
        ++nModel;
        meshModels.push_back(model);
        Node& mt = objects.add("Material"); mt.L(mat).S(fbxName(p.name, "Material")).S("");
        mt.add("Version").I(102); mt.add("ShadingModel").S("phong"); mt.add("MultiLayer").I(0);
        Node& mp = mt.add("Properties70");
        mp.kid(P("DiffuseColor", "Color", "", "A").D(0.8).D(0.8).D(0.8));
        mp.kid(P("Diffuse", "Vector3D", "Vector", "").D(0.8).D(0.8).D(0.8));
        ++nMat;
        OO(geo, model); OO(model, 0); OO(mat, model);
        if (!p.texture.empty()) {
            const int64_t tex = id(), vid = id();
            const std::string abs = (dir / fs::u8path(p.texture)).u8string();
            Node& t = objects.add("Texture"); t.L(tex).S(fbxName(p.name, "Texture")).S("");
            t.add("Type").S("TextureVideoClip"); t.add("Version").I(202); t.add("TextureName").S(fbxName(p.name, "Texture"));
            t.add("Media").S(fbxName(p.name, "Video")); t.add("FileName").S(abs); t.add("RelativeFilename").S(p.texture);
            Node& vd = objects.add("Video"); vd.L(vid).S(fbxName(p.name, "Video")).S("Clip");
            vd.add("Type").S("Clip");
            vd.add("Properties70").kid(P("Path", "KString", "XRefUrl", "").S(abs));
            vd.add("UseMipMap").I(0); vd.add("Filename").S(abs); vd.add("RelativeFilename").S(p.texture);
            nTex += 2;
            OP(tex, mat, "DiffuseColor"); OO(vid, tex);
        }
        if (s) {
            const int64_t skin = id();
            Node& sk = objects.add("Deformer"); sk.L(skin).S(fbxName(p.name, "Deformer")).S("Skin");
            sk.add("Version").I(101); sk.add("Link_DeformAcuracy").D(50);
            ++nDef;
            OO(skin, geo);
            std::vector<std::vector<int32_t>> idx(s->bones.size());
            std::vector<std::vector<double>> wt(s->bones.size());
            for (size_t k = 0; k < p.weights.size(); ++k) for (const auto& [bone, w] : p.weights[k]) { idx[bone].push_back((int32_t)k); wt[bone].push_back(w); }
            for (size_t bi = 0; bi < s->bones.size(); ++bi) {
                if (idx[bi].empty()) continue;
                const int64_t cl = id();
                Node& c = objects.add("Deformer"); c.L(cl).S(fbxName(s->bones[bi].name, "SubDeformer")).S("Cluster");
                c.add("Version").I(100);
                c.add("UserData").S("").S("");
                c.add("Indexes").Ai(idx[bi]); c.add("Weights").Ad(wt[bi]);
                c.add("Transform").Ad(m16(Skel::inverseRigid(bind[bi])));
                c.add("TransformLink").Ad(m16(bind[bi]));
                ++nDef;
                OO(cl, skin); OO(boneIds[bi], cl);
            }
        }
    }
    if (s) {   // the bind pose: every mesh and bone where it was bound
        const int64_t pose = id();
        Node& bp = objects.add("Pose"); bp.L(pose).S(fbxName("BindPose", "Pose")).S("BindPose");
        bp.add("Type").S("BindPose"); bp.add("Version").I(100); bp.add("NbPoseNodes").I((int)(meshModels.size() + boneIds.size()));
        for (int64_t mm : meshModels) { Node& pn = bp.add("PoseNode"); pn.add("Node").L(mm); pn.add("Matrix").Ad(m16(Skel::identity())); }
        for (size_t i = 0; i < boneIds.size(); ++i) { Node& pn = bp.add("PoseNode"); pn.add("Node").L(boneIds[i]); pn.add("Matrix").Ad(m16(bind[i])); }
        ++nPose;
    }
    // the file
    Node root;
    Node& hx = root.add("FBXHeaderExtension");
    hx.add("FBXHeaderVersion").I(1003); hx.add("FBXVersion").I(7400);
    Node& cts = hx.add("CreationTimeStamp");
    cts.add("Version").I(1000); cts.add("Year").I(1970); cts.add("Month").I(1); cts.add("Day").I(1); cts.add("Hour").I(10); cts.add("Minute").I(0); cts.add("Second").I(0); cts.add("Millisecond").I(0);
    hx.add("Creator").S("Pygmalion");
    root.add("FileId").R(std::string("\x28\xb3\x2a\xeb\xb6\x24\xcc\xc2\xbf\xc8\xb0\x2a\xa9\x2b\xfc\xf1", 16));
    root.add("CreationTime").S("1970-01-01 10:00:00:000");
    root.add("Creator").S("Pygmalion");
    Node& gs = root.add("GlobalSettings");
    gs.add("Version").I(1000);
    Node& gp = gs.add("Properties70");
    gp.kid(P("UpAxis", "int", "Integer", "").I(1)); gp.kid(P("UpAxisSign", "int", "Integer", "").I(1));
    gp.kid(P("FrontAxis", "int", "Integer", "").I(2)); gp.kid(P("FrontAxisSign", "int", "Integer", "").I(1));
    gp.kid(P("CoordAxis", "int", "Integer", "").I(0)); gp.kid(P("CoordAxisSign", "int", "Integer", "").I(1));
    gp.kid(P("OriginalUpAxis", "int", "Integer", "").I(1)); gp.kid(P("OriginalUpAxisSign", "int", "Integer", "").I(1));
    gp.kid(P("UnitScaleFactor", "double", "Number", "").D(op.cmPerUnit)); gp.kid(P("OriginalUnitScaleFactor", "double", "Number", "").D(op.cmPerUnit));
    Node& docs = root.add("Documents"); docs.add("Count").I(1);
    Node& doc = docs.add("Document"); doc.L(id()).S("Scene").S("Scene");
    doc.add("Properties70").kid(P("SourceObject", "object", "", ""));
    doc.add("RootNode").L(0);
    root.add("References");
    Node& defs = root.add("Definitions");
    defs.add("Version").I(100);
    const std::pair<const char*, int> types[] = {{"GlobalSettings", 1}, {"Model", nModel}, {"NodeAttribute", nAttr}, {"Geometry", nGeo}, {"Material", nMat},
                                                 {"Texture", nTex / 2}, {"Video", nTex / 2}, {"Deformer", nDef}, {"Pose", nPose}};
    int total = 0; for (const auto& t : types) total += t.second;
    defs.add("Count").I(total);
    for (const auto& t : types) if (t.second) { Node& ot = defs.add("ObjectType"); ot.S(t.first); ot.add("Count").I(t.second); }
    root.kids.push_back(objects);
    root.kids.push_back(conns);
    Node& takes = root.add("Takes"); takes.add("Current").S("");

    std::string o = std::string("Kaydara FBX Binary  \x00\x1a\x00", 23);
    put32(o, 7400);
    for (const auto& k : root.kids) writeNode(o, k);
    o.append(13, '\0');
    o += std::string("\xfa\xbc\xab\x09\xd0\xc8\xd4\x66\xb1\x76\xfb\x83\x1c\xf7\x26\x7e", 16);
    o.append(4, '\0');
    size_t pad = ((o.size() + 15) & ~(size_t)15) - o.size();
    if (pad == 0) pad = 16;
    o.append(pad, '\0');
    put32(o, 7400);
    o.append(120, '\0');
    o += std::string("\xf8\x5a\x8c\x6a\xde\xf5\xd9\x7e\xec\xe9\x0c\xe3\x75\x8f\x29\x0b", 16);
    std::ofstream f(fs::u8path(path), std::ios::binary);
    if (!f || !f.write(o.data(), (std::streamsize)o.size())) { err = "Couldn't write " + path; return false; }
    return true;
}

// ---------- Ogre3D ----------
static void axisAngle(const Mat4& m, float& angle, float axis[3]) {
    const float tr = m[0] + m[5] + m[10];
    angle = std::acos(std::clamp((tr - 1) / 2, -1.0f, 1.0f));
    axis[0] = m[1 * 4 + 2] - m[2 * 4 + 1]; axis[1] = m[2 * 4 + 0] - m[0 * 4 + 2]; axis[2] = m[0 * 4 + 1] - m[1 * 4 + 0];
    const float l = std::sqrt(axis[0] * axis[0] + axis[1] * axis[1] + axis[2] * axis[2]);
    if (l < 1e-6f) {
        if (angle < 0.5f) { angle = 0; axis[0] = 0; axis[1] = 1; axis[2] = 0; return; }
        // 180 degrees: the axis from the diagonal
        const float xx = std::sqrt(std::max(0.0f, (m[0] + 1) / 2)), yy = std::sqrt(std::max(0.0f, (m[5] + 1) / 2)), zz = std::sqrt(std::max(0.0f, (m[10] + 1) / 2));
        axis[0] = xx; axis[1] = m[4] + m[1] < 0 ? -yy : yy; axis[2] = m[8] + m[2] < 0 ? -zz : zz;
        return;
    }
    for (int i = 0; i < 3; ++i) axis[i] /= l;
}

bool saveOgre(const std::vector<Part>& parts, const Skel::Skeleton* s, const std::string& path, const Options& op, std::string& err) {
    // path: name.mesh.xml; beside it name.skeleton.xml and name.material
    std::string base = path;
    for (const char* e : {".mesh.xml", ".xml", ".mesh"}) if (base.size() > strlen(e) && base.compare(base.size() - strlen(e), strlen(e), e) == 0) { base.resize(base.size() - strlen(e)); break; }
    const std::string name = fs::u8path(base).filename().u8string();
    const Xf x = xfOf(parts, op);
    char b[160];
    {
        std::ofstream m(fs::u8path(base + ".material"));
        if (!m) { err = "Couldn't write " + base + ".material"; return false; }
        for (const auto& p : parts) {
            m << "material " << name << "_" << safeId(p.name) << "\n{\n    technique\n    {\n        pass\n        {\n            diffuse 0.8 0.8 0.8\n";
            if (!p.texture.empty()) m << "            texture_unit\n            {\n                texture " << p.texture << "\n            }\n";
            m << "        }\n    }\n}\n\n";
        }
    }
    std::ofstream o(fs::u8path(base + ".mesh.xml"));
    if (!o) { err = "Couldn't write " + base + ".mesh.xml"; return false; }
    o << "<mesh>\n    <submeshes>\n";
    for (const auto& p : parts) {
        const size_t n = p.pos.size() / 3;
        o << "        <submesh material=\"" << name << "_" << safeId(p.name) << "\" usesharedvertices=\"false\" use32bitindexes=\"true\" operationtype=\"triangle_list\">\n";
        o << "            <faces count=\"" << p.tri.size() / 3 << "\">\n";
        for (size_t t = 0; t + 2 < p.tri.size(); t += 3) o << "                <face v1=\"" << p.tri[t] << "\" v2=\"" << p.tri[t + 1] << "\" v3=\"" << p.tri[t + 2] << "\" />\n";
        o << "            </faces>\n            <geometry vertexcount=\"" << n << "\">\n";
        o << "                <vertexbuffer positions=\"true\" normals=\"true\" texture_coords=\"1\" texture_coord_dimensions_0=\"2\">\n";
        for (size_t v = 0; v < n; ++v) {
            float q[3]; x.pt(&p.pos[v * 3], q);
            snprintf(b, sizeof b, "                    <vertex>\n                        <position x=\"%g\" y=\"%g\" z=\"%g\" />\n", q[0], q[1], q[2]); o << b;
            snprintf(b, sizeof b, "                        <normal x=\"%.4f\" y=\"%.4f\" z=\"%.4f\" />\n", p.nrm[v * 3], p.nrm[v * 3 + 1], p.nrm[v * 3 + 2]); o << b;
            snprintf(b, sizeof b, "                        <texcoord u=\"%.5f\" v=\"%.5f\" />\n                    </vertex>\n", p.uv[v * 2], 1 - p.uv[v * 2 + 1]); o << b;
        }
        o << "                </vertexbuffer>\n            </geometry>\n";
        if (s) {
            o << "            <boneassignments>\n";
            for (size_t v = 0; v < p.weights.size(); ++v)
                for (const auto& [bone, w] : p.weights[v]) { snprintf(b, sizeof b, "                <vertexboneassignment vertexindex=\"%zu\" boneindex=\"%d\" weight=\"%.5f\" />\n", v, bone, w); o << b; }
            o << "            </boneassignments>\n";
        }
        o << "        </submesh>\n";
    }
    o << "    </submeshes>\n";
    if (s) o << "    <skeletonlink name=\"" << name << ".skeleton\" />\n";
    o << "</mesh>\n";
    if (s) {
        std::vector<Mat4> bind, scene, local;
        boneMats(*s, x, op.posed, bind, scene, local);   // Ogre's skeleton is the binding pose: the mesh's
        std::ofstream k(fs::u8path(base + ".skeleton.xml"));
        if (!k) { err = "Couldn't write " + base + ".skeleton.xml"; return false; }
        k << "<skeleton>\n    <bones>\n";
        for (size_t i = 0; i < s->bones.size(); ++i) {
            float ang, ax[3]; axisAngle(local[i], ang, ax);
            k << "        <bone id=\"" << i << "\" name=\"" << xmlEsc(s->bones[i].name) << "\">\n";
            snprintf(b, sizeof b, "            <position x=\"%g\" y=\"%g\" z=\"%g\" />\n", local[i][12], local[i][13], local[i][14]); k << b;
            snprintf(b, sizeof b, "            <rotation angle=\"%.6f\">\n                <axis x=\"%.6f\" y=\"%.6f\" z=\"%.6f\" />\n            </rotation>\n", ang, ax[0], ax[1], ax[2]); k << b;
            k << "        </bone>\n";
        }
        k << "    </bones>\n    <bonehierarchy>\n";
        for (const auto& bn : s->bones) if (bn.parent >= 0) k << "        <boneparent bone=\"" << xmlEsc(bn.name) << "\" parent=\"" << xmlEsc(s->bones[bn.parent].name) << "\" />\n";
        k << "    </bonehierarchy>\n</skeleton>\n";
    }
    return true;
}

// ---------- the UV map ----------
bool saveUvMap(const Human::Body& b, const std::string& path, int size, std::string& err) {
    std::vector<unsigned char> px((size_t)size * size * 4);
    for (size_t i = 0; i < px.size(); i += 4) { px[i] = px[i + 1] = px[i + 2] = 0x24; px[i + 3] = 255; }
    auto plot = [&](int x, int y) { if (x < 0 || y < 0 || x >= size || y >= size) return; unsigned char* p = &px[((size_t)(size - 1 - y) * size + x) * 4]; p[0] = 0xe6; p[1] = 0xe6; p[2] = 0xe6; };
    auto line = [&](float u0, float v0, float u1, float v1) {
        int x0 = (int)(u0 * (size - 1)), y0 = (int)(v0 * (size - 1)), x1 = (int)(u1 * (size - 1)), y1 = (int)(v1 * (size - 1));
        const int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1, dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
        int e = dx + dy;
        for (int guard = 0; guard < size * 4; ++guard) {
            plot(x0, y0);
            if (x0 == x1 && y0 == y1) break;
            const int e2 = 2 * e;
            if (e2 >= dy) { e += dy; x0 += sx; }
            if (e2 <= dx) { e += dx; y0 += sy; }
        }
    };
    for (size_t t = 0; t + 2 < b.triUv.size(); t += 3)
        for (int k = 0; k < 3; ++k) {
            const int a = b.triUv[t + k], c = b.triUv[t + (k + 1) % 3];
            if (a < 0 || c < 0) continue;
            line(b.uv[(size_t)a * 2], b.uv[(size_t)a * 2 + 1], b.uv[(size_t)c * 2], b.uv[(size_t)c * 2 + 1]);
        }
    if (!stbi_write_png(path.c_str(), size, size, 4, px.data(), size * 4)) { err = "Couldn't write " + path; return false; }
    return true;
}

}
