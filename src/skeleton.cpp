// Pygmalion - the skeleton and posing (see skeleton.h). The maths follows MakeHuman's shared/skeleton.py (getMatrix,
// Bone.build, Skeleton.setPose, skinMesh), shared/bvh.py (BVHJoint.calculateFrames) and shared/animation.py
// (PoseUnit.getBlendedPose).
#include "skeleton.h"
#include "json.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <set>
#include <sstream>

namespace fs = std::filesystem;

namespace Skel {

static Mat4 ident() { Mat4 m{}; m[0] = m[5] = m[10] = m[15] = 1; return m; }
static Mat4 mul(const Mat4& a, const Mat4& b) {
    Mat4 r{};
    for (int c = 0; c < 4; ++c) for (int rr = 0; rr < 4; ++rr) { float s = 0; for (int k = 0; k < 4; ++k) s += a[k * 4 + rr] * b[c * 4 + k]; r[c * 4 + rr] = s; }
    return r;
}
// a rigid transform's inverse (rotation transposed, translation turned round)
static Mat4 inv(const Mat4& m) {
    Mat4 r = ident();
    for (int i = 0; i < 3; ++i) for (int j = 0; j < 3; ++j) r[j * 4 + i] = m[i * 4 + j];
    for (int i = 0; i < 3; ++i) r[12 + i] = -(r[0 * 4 + i] * m[12] + r[1 * 4 + i] * m[13] + r[2 * 4 + i] * m[14]);
    return r;
}
static void norm3(float* v) { const float l = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]); if (l > 1e-12f) for (int i = 0; i < 3; ++i) v[i] /= l; }
static void cross(const float* a, const float* b, float* o) { o[0] = a[1] * b[2] - a[2] * b[1]; o[1] = a[2] * b[0] - a[0] * b[2]; o[2] = a[0] * b[1] - a[1] * b[0]; }

// MakeHuman's getMatrix: Y along the bone, X close to the normal, Z = X x Y, from the head
static Mat4 boneMatrix(const float* head, const float* tail, const float* normal0) {
    float y[3] = {tail[0] - head[0], tail[1] - head[1], tail[2] - head[2]}; norm3(y);
    float n[3] = {normal0[0], normal0[1], normal0[2]}; norm3(n);
    float z[3]; cross(n, y, z); norm3(z);
    float x[3]; cross(y, z, x); norm3(x);
    Mat4 m = ident();
    for (int i = 0; i < 3; ++i) { m[i] = x[i]; m[4 + i] = y[i]; m[8 + i] = z[i]; m[12 + i] = head[i]; }
    return m;
}

static bool readAll(const std::string& p, std::string& out) {
    std::ifstream in(fs::u8path(p), std::ios::binary);
    if (!in) return false;
    std::ostringstream ss; ss << in.rdbuf(); out = ss.str();
    return true;
}

bool load(Skeleton& s, const std::string& path, std::string& err) {
    s = Skeleton();
    std::string t;
    if (!readAll(path, t)) { err = "Couldn't read " + path; return false; }
    Json::Value root; size_t i = 0;
    if (!Json::parse(t, i, root)) { err = "The skeleton file is damaged"; return false; }
    s.name = root.str("name"); s.description = root.str("description");
    if (const Json::Value* j = root.get("joints"))
        for (const auto& kv : j->o) { std::vector<int> v; for (const auto& x : kv.second.a) v.push_back((int)x.n); s.joints[kv.first] = v; }
    if (const Json::Value* p = root.get("planes"))
        for (const auto& kv : p->o) if (kv.second.a.size() == 3) s.planes[kv.first] = {kv.second.a[0].s, kv.second.a[1].s, kv.second.a[2].s};
    // the bones, parents first
    struct Raw { std::string name, parent, head, tail, plane; };
    std::vector<Raw> raw;
    if (const Json::Value* b = root.get("bones"))
        for (const auto& kv : b->o) raw.push_back({kv.first, kv.second.str("parent"), kv.second.str("head"), kv.second.str("tail"), kv.second.str("rotation_plane")});
    std::set<std::string> placed;
    while (s.bones.size() < raw.size()) {
        bool any = false;
        for (const auto& r : raw) {
            if (placed.count(r.name) || (!r.parent.empty() && !placed.count(r.parent))) continue;
            Bone bn; bn.name = r.name; bn.headJoint = r.head; bn.tailJoint = r.tail; bn.plane = r.plane;
            bn.parent = r.parent.empty() ? -1 : s.byName[r.parent];
            s.byName[r.name] = (int)s.bones.size();
            s.bones.push_back(bn); placed.insert(r.name); any = true;
        }
        if (!any) { err = "The skeleton's bones don't connect"; return false; }
    }
    // the weights
    s.weights.assign(s.bones.size(), {});
    const std::string wf = root.str("weights_file");
    std::string w;
    if (!wf.empty() && readAll((fs::u8path(path).parent_path() / wf).u8string(), w)) {
        Json::Value wr; size_t j = 0;
        if (Json::parse(w, j, wr))
            if (const Json::Value* ws = wr.get("weights"))
                for (const auto& kv : ws->o) {
                    auto it = s.byName.find(kv.first);
                    if (it == s.byName.end()) continue;
                    for (const auto& pr : kv.second.a) if (pr.a.size() == 2) s.weights[it->second].push_back({(int)pr.a[0].n, (float)pr.a[1].n});
                }
    }
    for (auto& b : s.bones) b.pose = ident();
    return true;
}

static bool jointPos(const Skeleton& s, const std::vector<float>& pos, const std::string& joint, float out[3]) {
    auto it = s.joints.find(joint);
    if (it == s.joints.end() || it->second.empty()) return false;
    out[0] = out[1] = out[2] = 0;
    for (int v : it->second) for (int c = 0; c < 3; ++c) out[c] += pos[(size_t)v * 3 + c];
    for (int c = 0; c < 3; ++c) out[c] /= it->second.size();
    return true;
}

void build(Skeleton& s, const std::vector<float>& pos) {
    for (auto& b : s.bones) {
        jointPos(s, pos, b.headJoint, b.head);
        jointPos(s, pos, b.tailJoint, b.tail);
        // the normal of the plane through three joints (counter-clockwise), else +Y (skeleton.py get_normal)
        float n[3] = {0, 1, 0};
        auto pl = s.planes.find(b.plane);
        if (pl != s.planes.end()) {
            float p1[3], p2[3], p3[3];
            if (jointPos(s, pos, pl->second[0], p1) && jointPos(s, pos, pl->second[1], p2) && jointPos(s, pos, pl->second[2], p3)) {
                float pv[3] = {p2[0] - p1[0], p2[1] - p1[1], p2[2] - p1[2]}, yv[3] = {p3[0] - p2[0], p3[1] - p2[1], p3[2] - p2[2]};
                norm3(pv); norm3(yv);
                float c[3]; cross(yv, pv, c);
                if (std::fabs(c[0]) + std::fabs(c[1]) + std::fabs(c[2]) > 1e-5f) { norm3(c); n[0] = c[0]; n[1] = c[1]; n[2] = c[2]; }
            }
        }
        b.restGlobal = boneMatrix(b.head, b.tail, n);
        b.restRelative = b.parent >= 0 ? mul(inv(s.bones[b.parent].restGlobal), b.restGlobal) : b.restGlobal;
    }
    setPose(s, nullptr);
}

void setPose(Skeleton& s, const Pose* p) {
    for (auto& b : s.bones) {
        b.pose = ident();
        if (p) {
            auto it = p->bones.find(b.name);
            if (it != p->bones.end()) {
                // the pose's rotation is in global axes: into the bone's own (Skeleton.setPose)
                Mat4 r = ident();
                for (int i = 0; i < 3; ++i) for (int j = 0; j < 3; ++j) r[j * 4 + i] = it->second[j * 4 + i];
                const Mat4 ir = inv(b.restGlobal);
                b.pose = mul(mul(ir, r), b.restGlobal);
                b.pose[12] = b.pose[13] = b.pose[14] = 0;
                if (b.parent < 0) {   // the root may move too
                    const float t[3] = {it->second[12], it->second[13], it->second[14]};
                    for (int i = 0; i < 3; ++i) b.pose[12 + i] = ir[0 * 4 + i] * t[0] + ir[1 * 4 + i] * t[1] + ir[2 * 4 + i] * t[2];
                }
            }
        }
        b.poseGlobal = b.parent >= 0 ? mul(s.bones[b.parent].poseGlobal, mul(b.restRelative, b.pose)) : mul(b.restRelative, b.pose);
        b.poseVerts = mul(b.poseGlobal, inv(b.restGlobal));
    }
}

void skin(const Skeleton& s, const std::vector<float>& rest, std::vector<float>& out) {
    out.assign(rest.size(), 0.0f);
    std::vector<float> wsum(rest.size() / 3, 0.0f);
    for (size_t bi = 0; bi < s.bones.size(); ++bi) {
        const Mat4& m = s.bones[bi].poseVerts;
        for (const auto& [v, w] : s.weights[bi]) {
            if ((size_t)v * 3 + 2 >= rest.size()) continue;
            const float* p = &rest[(size_t)v * 3];
            float* o = &out[(size_t)v * 3];
            for (int i = 0; i < 3; ++i) o[i] += w * (m[i] * p[0] + m[4 + i] * p[1] + m[8 + i] * p[2] + m[12 + i]);
            wsum[v] += w;
        }
    }
    // vertices no bone holds (the helpers) stay where they are
    for (size_t v = 0; v < wsum.size(); ++v) if (wsum[v] <= 0) for (int i = 0; i < 3; ++i) out[v * 3 + i] = rest[v * 3 + i];
}

// ---------- BVH ----------
static Mat4 axisRot(char axis, float a) {
    Mat4 m = ident();
    const float c = std::cos(a), s = std::sin(a);
    if (axis == 'x') { m[5] = c; m[6] = s; m[9] = -s; m[10] = c; }
    else if (axis == 'y') { m[0] = c; m[2] = -s; m[8] = s; m[10] = c; }
    else { m[0] = c; m[1] = s; m[4] = -s; m[5] = c; }
    return m;
}

bool loadBvh(const std::string& path, std::vector<Pose>& frames, std::string& err) {
    frames.clear();
    std::string t;
    if (!readAll(path, t)) { err = "Couldn't read " + path; return false; }
    std::istringstream in(t);
    struct J { std::string name; std::vector<std::string> ch; float off[3] = {0, 0, 0}; int parent = -1; bool end = false; };
    std::vector<J> js;
    std::vector<int> stack;
    std::string w;
    in >> w;
    if (w != "HIERARCHY") { err = "Not a BVH file"; return false; }
    while (in >> w) {
        if (w == "ROOT" || w == "JOINT") { J j; in >> j.name; j.parent = stack.empty() ? -1 : stack.back(); js.push_back(j); }
        else if (w == "End") { J j; in >> w; j.name = "_end"; j.end = true; j.parent = stack.back(); js.push_back(j); }
        else if (w == "{") stack.push_back((int)js.size() - 1);
        else if (w == "}") { if (!stack.empty()) stack.pop_back(); }
        else if (w == "OFFSET") in >> js.back().off[0] >> js.back().off[1] >> js.back().off[2];
        else if (w == "CHANNELS") { int n; in >> n; for (int k = 0; k < n; ++k) { in >> w; js.back().ch.push_back(w); } }
        else if (w == "MOTION") break;
    }
    int nFrames = 0; float frameTime = 0;
    in >> w >> nFrames >> w >> w >> frameTime;   // Frames: N  Frame Time: T
    // Z up or Y up (bvh.py _autoGuessCoordinateSystem): the longest offsets go up
    float sum[3] = {0, 0, 0};
    for (const auto& j : js) for (int c = 0; c < 3; ++c) sum[c] += std::fabs(j.off[c]);
    const bool zUp = sum[2] > sum[1] && sum[2] > sum[0];
    const float D = 3.14159265f / 180;
    for (int f = 0; f < nFrames; ++f) {
        Pose p; p.name = fs::u8path(path).stem().u8string();
        for (const auto& j : js) {
            if (j.end) continue;
            float v[6] = {0, 0, 0, 0, 0, 0};
            std::vector<std::pair<char, float>> rots;
            float tr[3] = {0, 0, 0}; bool hasT = false;
            for (const auto& c : j.ch) {
                float x; in >> x;
                if (c == "Xposition") { tr[0] = x; hasT = true; }
                else if (c == "Yposition") { if (zUp) tr[2] = -x; else tr[1] = x; hasT = true; }
                else if (c == "Zposition") { if (zUp) tr[1] = x; else tr[2] = x; hasT = true; }
                else if (c == "Xrotation") rots.push_back({'x', D * x});
                else if (c == "Yrotation") rots.push_back(zUp ? std::make_pair('z', -D * x) : std::make_pair('y', D * x));
                else if (c == "Zrotation") rots.push_back(zUp ? std::make_pair('y', D * x) : std::make_pair('z', D * x));
            }
            (void)v;
            // the channels' rotations, the first channel outermost (R = R1 R2 R3), as BVH has it
            Mat4 m = ident();
            if (rots.size() >= 3) for (const auto& r : rots) m = mul(m, axisRot(r.first, r.second));
            if (hasT && j.parent < 0) { m[12] = tr[0]; m[13] = tr[1]; m[14] = tr[2]; }
            p.bones[j.name] = m;
        }
        frames.push_back(std::move(p));
    }
    (void)frameTime;
    if (frames.empty()) { err = "The BVH has no frames"; return false; }
    return true;
}

// ---------- blending (getBlendedPose: each unit slerped from rest by its weight, then multiplied in) ----------
struct Quat { float w, x, y, z; };
static Quat fromMat(const Mat4& m) {
    const float tr = m[0] + m[5] + m[10];
    Quat q;
    if (tr > 0) { const float s = std::sqrt(tr + 1) * 2; q = {0.25f * s, (m[6] - m[9]) / s, (m[8] - m[2]) / s, (m[1] - m[4]) / s}; }
    else if (m[0] > m[5] && m[0] > m[10]) { const float s = std::sqrt(1 + m[0] - m[5] - m[10]) * 2; q = {(m[6] - m[9]) / s, 0.25f * s, (m[4] + m[1]) / s, (m[8] + m[2]) / s}; }
    else if (m[5] > m[10]) { const float s = std::sqrt(1 + m[5] - m[0] - m[10]) * 2; q = {(m[8] - m[2]) / s, (m[4] + m[1]) / s, 0.25f * s, (m[9] + m[6]) / s}; }
    else { const float s = std::sqrt(1 + m[10] - m[0] - m[5]) * 2; q = {(m[1] - m[4]) / s, (m[8] + m[2]) / s, (m[9] + m[6]) / s, 0.25f * s}; }
    return q;
}
static Mat4 toMat(const Quat& q) {
    Mat4 m = ident();
    const float w = q.w, x = q.x, y = q.y, z = q.z;
    m[0] = 1 - 2 * (y * y + z * z); m[4] = 2 * (x * y - z * w); m[8] = 2 * (x * z + y * w);
    m[1] = 2 * (x * y + z * w); m[5] = 1 - 2 * (x * x + z * z); m[9] = 2 * (y * z - x * w);
    m[2] = 2 * (x * z - y * w); m[6] = 2 * (y * z + x * w); m[10] = 1 - 2 * (x * x + y * y);
    return m;
}
static Quat slerpFromRest(Quat q, float t) {
    float d = q.w;   // dot with (1, 0, 0, 0)
    if (d < 0) { q = {-q.w, -q.x, -q.y, -q.z}; d = -d; }
    if (d > 0.9995f) { Quat r = {1 + t * (q.w - 1), t * q.x, t * q.y, t * q.z}; const float l = std::sqrt(r.w * r.w + r.x * r.x + r.y * r.y + r.z * r.z); return {r.w / l, r.x / l, r.y / l, r.z / l}; }
    const float th = std::acos(d), s = std::sin(th), a = std::sin((1 - t) * th) / s, b = std::sin(t * th) / s;
    return {a + b * q.w, b * q.x, b * q.y, b * q.z};
}
static Quat qmul(const Quat& a, const Quat& b) {
    return {a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z, a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
            a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x, a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w};
}

Pose blend(const std::vector<std::pair<const Pose*, float>>& parts) {
    Pose out;
    if (parts.empty()) return out;
    std::set<std::string> names;
    for (const auto& [p, w] : parts) for (const auto& kv : p->bones) names.insert(kv.first);
    for (const auto& n : names) {
        Quat acc = {1, 0, 0, 0};
        for (const auto& [p, w] : parts) {
            auto it = p->bones.find(n);
            if (it == p->bones.end() || w == 0) continue;
            acc = qmul(slerpFromRest(fromMat(it->second), w), acc);
        }
        out.bones[n] = toMat(acc);
    }
    return out;
}

}
