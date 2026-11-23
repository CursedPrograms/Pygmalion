// Pygmalion - strand hair physics and TressFX files (see hair_sim.h). The simulation is TressFX's compute passes
// (TressFXSimulation.hlsl, MIT, (c) Advanced Micro Devices), one strand per task on the CPU:
//   IntegrationAndGlobalShapeConstraints, CalculateStrandLevelData + VelocityShockPropagation,
//   LocalShapeConstraints, LengthConstriantsWindAndCollision.
// TressFX skins each strand's rest shape with its bones; here the strand's root triangle carries it (TressFX 3's
// GenerateTransforms way), so the hair follows the body however it's shaped, posed or animated.
#include "hair_sim.h"
#include "human.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <thread>

namespace HairSim {

namespace {
struct V3 { float x, y, z; };
inline V3 v3(const float* p) { return {p[0], p[1], p[2]}; }
inline V3 operator+(V3 a, V3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline V3 operator-(V3 a, V3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline V3 operator*(V3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
inline float dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline V3 cross(V3 a, V3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
inline float len(V3 a) { return std::sqrt(dot(a, a)); }
inline V3 norm(V3 a) { const float l = len(a); return l > 0 ? a * (1 / l) : a; }
inline void put(float* p, V3 v) { p[0] = v.x; p[1] = v.y; p[2] = v.z; }

struct Q { float x, y, z, w; };
inline Q qnorm(Q q) { const float n = q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w; if (n < 1e-10f) return {q.x, q.y, q.z, 1}; const float s = 1 / std::sqrt(n); return {q.x * s, q.y * s, q.z * s, q.w * s}; }
inline V3 rotate(Q q, V3 v) {   // TressFX's MultQuaternionAndVector
    const V3 qv = {q.x, q.y, q.z};
    V3 uv = cross(qv, v), uuv = cross(qv, uv);
    return v + uv * (2 * q.w) + uuv * 2;
}
inline Q fromTwoUnit(V3 u, V3 v) {   // TressFX's QuatFromTwoUnitVectors
    float r = 1 + dot(u, v); V3 n;
    if (r < 1e-7f) { r = 0; n = std::fabs(u.x) > std::fabs(u.z) ? V3{-u.y, u.x, 0} : V3{0, -u.z, u.y}; }
    else n = cross(u, v);
    return qnorm({n.x, n.y, n.z, r});
}
// a triangle's frame: tangent, normal, binormal (TressFX's GenerateTransforms)
void triFrame(const Hair::Surface& s, int t, float out[9]) {
    const int* v = &s.tri[t * 3];
    const V3 a = v3(&s.pos[v[0] * 3]), b = v3(&s.pos[v[1] * 3]), c = v3(&s.pos[v[2] * 3]);
    const V3 tg = norm(a - c), nm = norm(cross(tg, b - c)), bn = norm(cross(nm, tg));
    put(out, tg); put(out + 3, nm); put(out + 6, bn);
}
V3 rootAt(const Hair::Surface& s, int t, const float* b) {
    const int* v = &s.tri[t * 3];
    return v3(&s.pos[v[0] * 3]) * b[0] + v3(&s.pos[v[1] * 3]) * b[1] + v3(&s.pos[v[2] * 3]) * b[2];
}

template <class F> void parallel(int n, F f) {
    const int threads = std::max(1, std::min((int)std::thread::hardware_concurrency(), (n + 63) / 64));
    if (threads == 1) { for (int i = 0; i < n; ++i) f(i); return; }
    std::vector<std::thread> pool;
    for (int t = 0; t < threads; ++t) pool.emplace_back([&, t] { for (int i = t; i < n; i += threads) f(i); });
    for (auto& th : pool) th.join();
}
}  // namespace

void Simulation::reset(const Hair::Curves& g, const Hair::Surface& s) {
    offs = g.offsets; rest = g.pos; pos = rest; prev = rest; prevprev = rest;
    const int nc = g.curveCount(), np = g.pointCount();
    restLen.assign(np, 0.0f); invMass.assign(np, 1.0f);
    tri.assign(nc, -1); bary.assign((size_t)nc * 3, 0.0f); restFrame.assign((size_t)nc * 9, 0.0f);
    for (int c = 0; c < nc; ++c) {
        const int a = offs[c], n = offs[c + 1] - a;
        for (int i = 0; i < n; ++i) {
            if (i < 2) invMass[a + i] = 0;   // TressFX: the first two points of a strand never move by themselves
            if (i < n - 1) restLen[a + i] = len(v3(&rest[(a + i + 1) * 3]) - v3(&rest[(a + i) * 3]));
        }
        if (c < (int)g.rootTri.size() && g.rootTri[c] >= 0 && g.rootTri[c] * 3 + 2 < (int)s.tri.size()) {
            tri[c] = g.rootTri[c];
            std::memcpy(&bary[c * 3], &g.rootBary[c * 3], 12);
            triFrame(s, tri[c], &restFrame[c * 9]);
        }
    }
}

void Simulation::step(const Hair::Surface& s, const Settings& set, float frameDt) {
    if (pos.empty()) return;
    const int nc = (int)offs.size() - 1, sub = std::max(1, set.substeps);
    const float dt = frameDt / sub;
    const float decay = std::exp(-set.damping * dt * 60.0f);
    const V3 wind = norm(v3(set.wind)) * set.windStrength;
    const bool windy = set.windStrength != 0 && len(v3(set.wind)) > 0;
    const float localK = 0.5f * std::min(set.localStiffness, 0.95f);
    for (int it = 0; it < sub; ++it) {
        parallel(nc, [&](int c) {
            const int a = offs[c], n = offs[c + 1] - a;
            if (n < 2) return;
            // the root's transform: rest triangle frame to the current one, rest root to current root
            // R maps the rest frame onto the current one: R = Cur^T * Rest (rows of each are the triangle's axes)
            float R[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}; V3 restRoot = v3(&rest[a * 3]), curRoot = restRoot;
            if (tri[c] >= 0) {
                float cf[9]; triFrame(s, tri[c], cf);
                const float* rf = &restFrame[c * 9];
                for (int r = 0; r < 3; ++r) for (int k = 0; k < 3; ++k) R[r][k] = cf[0 + r] * rf[0 + k] + cf[3 + r] * rf[3 + k] + cf[6 + r] * rf[6 + k];
                curRoot = rootAt(s, tri[c], &bary[c * 3]);
            }
            auto turn = [&](V3 v) { return V3{R[0][0] * v.x + R[0][1] * v.y + R[0][2] * v.z, R[1][0] * v.x + R[1][1] * v.y + R[1][2] * v.z, R[2][0] * v.x + R[2][1] * v.y + R[2][2] * v.z}; };
            auto skinned = [&](int i) { return turn(v3(&rest[i * 3]) - restRoot) + curRoot; };
            // 1. integration and global shape constraints
            for (int i = 0; i < n; ++i) {
                const int p = a + i;
                const V3 cur = v3(&pos[p * 3]), old = v3(&prev[p * 3]), init = skinned(p);
                V3 nw;
                if (invMass[p] > 0) nw = cur + (cur - old) * decay + V3{0, -set.gravity, 0} * (dt * dt);
                else nw = init;
                if (set.globalStiffness > 0 && set.globalRange > 0 && invMass[p] > 0 && (float)i < set.globalRange * (float)n)
                    nw = nw + (init - nw) * set.globalStiffness;
                std::memcpy(&prevprev[p * 3], &prev[p * 3], 12); put(&prev[p * 3], cur); put(&pos[p * 3], nw);
            }
            // 2. velocity shock propagation: the root's move carried down the strand
            {
                const V3 u = norm(v3(&prev[(a + 1) * 3]) - v3(&prev[a * 3])), v = norm(v3(&pos[(a + 1) * 3]) - v3(&pos[a * 3]));
                const Q rot = fromTwoUnit(u, v);
                const V3 trans = v3(&pos[a * 3]) - rotate(rot, v3(&prev[a * 3]));
                float coeff = set.vsp;
                const V3 accel = v3(&pos[(a + 1) * 3]) - v3(&prev[(a + 1) * 3]) * 2 + v3(&prevprev[(a + 1) * 3]);
                if (len(accel) > set.vspAccelThreshold) coeff = 1;
                for (int i = 2; i < n; ++i) {
                    const int p = a + i;
                    const V3 pn = v3(&pos[p * 3]), po = v3(&prev[p * 3]);
                    put(&pos[p * 3], pn * (1 - coeff) + (rotate(rot, pn) + trans) * coeff);
                    put(&prev[p * 3], po * (1 - coeff) + (rotate(rot, po) + trans) * coeff);
                }
            }
            // 3. local shape constraints: each segment keeps its groomed bend relative to the one before
            for (int li = 0; li < set.localIterations; ++li)
                for (int i = 1; i < n - 1; ++i) {
                    const int p = a + i;
                    V3 P = v3(&pos[p * 3]), P1 = v3(&pos[(p + 1) * 3]); const V3 P0 = v3(&pos[(p - 1) * 3]);
                    const V3 b = turn(v3(&rest[p * 3])), b1 = turn(v3(&rest[(p + 1) * 3])), b0 = turn(v3(&rest[(p - 1) * 3]));
                    const Q rg = fromTwoUnit(norm(b - b0), norm(P - P0));
                    const V3 target = rotate(rg, b1 - b) + P;
                    const V3 del = (target - P1) * localK;
                    if (invMass[p] > 0) P = P - del;
                    if (invMass[p + 1] > 0) P1 = P1 + del;
                    put(&pos[p * 3], P); put(&pos[(p + 1) * 3], P1);
                }
            // 4. wind, length constraints, collisions
            if (windy)
                for (int i = 2; i < n - 1; ++i) {
                    const int p = a + i;
                    const V3 v = v3(&pos[p * 3]) - v3(&pos[(p + 1) * 3]);
                    const V3 force = cross(cross(v, wind), v) * -1.0f;
                    put(&pos[p * 3], v3(&pos[p * 3]) + force * (dt * dt));
                }
            for (int li = 0; li < set.lengthIterations; ++li)
                for (int parity = 0; parity < 2; ++parity)
                    for (int i = parity; i < n - 1; i += 2) {
                        const int p = a + i;
                        V3 p0 = v3(&pos[p * 3]), p1 = v3(&pos[(p + 1) * 3]);
                        V3 delta = p1 - p0;
                        const float d = std::max(len(delta), 1e-7f);
                        delta = delta * (1 - restLen[p] / d);
                        const bool m0 = invMass[p] > 0, m1 = invMass[p + 1] > 0;
                        const float k0 = m0 ? (m1 ? 0.5f : 1.0f) : 0.0f, k1 = m1 ? (m0 ? 0.5f : 1.0f) : 0.0f;
                        put(&pos[p * 3], p0 + delta * k0); put(&pos[(p + 1) * 3], p1 - delta * k1);
                    }
            for (int i = 0; i < n; ++i) {
                const int p = a + i;
                if (invMass[p] <= 0) continue;
                V3 cur = v3(&pos[p * 3]); const V3 old = v3(&prev[p * 3]);
                bool hit = false;
                if (set.collide)
                    for (const Capsule& cc : capsules) {   // TressFX's CapsuleCollision (friction 0.4)
                        const V3 c0 = v3(cc.p0), c1 = v3(cc.p1), seg = c1 - c0, d0 = cur - c0, d1 = c1 - cur;
                        const float s0 = dot(d0, seg), s1 = dot(d1, seg);
                        if (s0 < 0) { if (dot(d0, d0) < cc.r0 * cc.r0) { cur = norm(d0) * cc.r0 + c0; hit = true; } continue; }
                        if (s1 < 0) { if (dot(d1, d1) < cc.r1 * cc.r1) { cur = norm(d1 * -1.0f) * cc.r1 + c1; hit = true; } continue; }
                        const V3 x = (c1 * s0 + c0 * s1) * (1 / (s0 + s1)), dl = cur - x;
                        const float rx = (s0 * cc.r1 + s1 * cc.r0) / (s0 + s1);
                        if (dot(dl, dl) < rx * rx) {
                            const V3 nn = norm(dl), vec = cur - old, sn = norm(seg), vt = sn * dot(vec, sn), vn = vec - vt;
                            cur = old + vt * 0.4f + (vn + nn * rx - dl);
                            hit = true;
                        }
                    }
                put(&pos[p * 3], cur);
                V3 dlt = cur - old; const float sp = dot(dlt, dlt);
                if (sp > set.clampDelta * set.clampDelta) { dlt = dlt * (set.clampDelta * set.clampDelta / sp); put(&prev[p * 3], cur - dlt); }
                if (hit) put(&prev[p * 3], cur);
            }
        });
    }
}

void Simulation::settle(const Hair::Surface& s, const Settings& set, int frames) {
    for (int f = 0; f < frames; ++f) step(s, set, 1.0f / 60.0f);
    prev = pos; prevprev = pos;   // at rest
}

std::vector<Capsule> bodyCapsules(const Human::Body& b) {
    std::vector<Capsule> out;
    const std::vector<float>& P = b.shown.empty() ? b.pos : b.shown;
    auto centre = [&](const char* name, V3& o) {
        auto it = b.joints.find(name);
        if (it == b.joints.end() || it->second.empty()) return false;
        o = {0, 0, 0};
        for (int v : it->second) o = o + v3(&P[v * 3]);
        o = o * (1.0f / (float)it->second.size());
        return true;
    };
    V3 head, eyeL, eyeR, neck;
    if (!centre("joint-head", head) || !centre("joint-l-eye", eyeL) || !centre("joint-r-eye", eyeR) || !centre("joint-neck", neck)) return out;
    const float ipd = len(eyeL - eyeR);
    const V3 eyes = (eyeL + eyeR) * 0.5f;
    // the skull: a capsule from behind the eyes up to the crown, a little inside the scalp so roots start outside
    const V3 skull0 = {head.x, eyes.y - 0.1f * ipd, head.z - 0.1f * ipd}, skull1 = {head.x, eyes.y + 0.9f * ipd, head.z - 0.2f * ipd};
    Capsule sk; put(sk.p0, skull0); sk.r0 = 1.05f * ipd; put(sk.p1, skull1); sk.r1 = 0.95f * ipd; out.push_back(sk);
    Capsule nk; put(nk.p0, neck); nk.r0 = 0.62f * ipd; put(nk.p1, head); nk.r1 = 0.62f * ipd; out.push_back(nk);
    V3 ls, rs;
    if (centre("joint-l-shoulder", ls) && centre("joint-r-shoulder", rs)) {
        Capsule sh; put(sh.p0, ls); sh.r0 = 0.75f * ipd; put(sh.p1, rs); sh.r1 = 0.75f * ipd; out.push_back(sh);
        V3 spine;
        if (centre("joint-spine-2", spine) || centre("joint-spine-1", spine)) {
            Capsule tb; put(tb.p0, (ls + rs) * 0.5f); tb.r0 = 1.3f * ipd; put(tb.p1, spine); tb.r1 = 1.4f * ipd; out.push_back(tb);
        }
    }
    return out;
}

// ---------- TressFX files ----------
namespace {
struct TfxHeader {   // TressFXFileFormat.h's TressFXTFXFileHeader
    float version; uint32_t numHairStrands, numVerticesPerStrand, offsetVertexPosition, offsetStrandUV, offsetVertexUV, offsetStrandThickness, offsetVertexColor, reserved[32];
};
static_assert(sizeof(TfxHeader) == 160, "TressFX header is 160 bytes");

// a strand resampled to n points evenly along its Catmull-Rom curve (Blender's hair, resolution 12)
void resample(const float* P, int count, int n, std::vector<float>& out) {
    std::vector<float> E;
    const int res = 12;
    if (count < 2) { out.assign((size_t)n * 3, 0.0f); for (int i = 0; i < n; ++i) std::memcpy(&out[i * 3], P, 12); return; }
    auto pt = [&](int i) { return &P[std::clamp(i, 0, count - 1) * 3]; };
    for (int s = 0; s < count - 1; ++s)
        for (int k = 0; k < res; ++k) {
            const float t = (float)k / res, u = 1 - t;
            const float w0 = -t * u * u, w1 = 2 + t * t * (3 * t - 5), w2 = 2 + u * u * (3 * u - 5), w3 = -u * t * t;
            const float* a = pt(s - 1); const float* b = pt(s); const float* c = pt(s + 1); const float* d = pt(s + 2);
            for (int j = 0; j < 3; ++j) E.push_back(k == 0 ? b[j] : 0.5f * (a[j] * w0 + b[j] * w1 + c[j] * w2 + d[j] * w3));
        }
    E.insert(E.end(), pt(count - 1), pt(count - 1) + 3);
    const int m = (int)E.size() / 3;
    std::vector<float> L(m, 0.0f);
    for (int i = 1; i < m; ++i) L[i] = L[i - 1] + len(v3(&E[i * 3]) - v3(&E[(i - 1) * 3]));
    out.assign((size_t)n * 3, 0.0f);
    for (int i = 0; i < n; ++i) {
        const float target = L[m - 1] * (float)i / (n - 1);
        int j = 1; while (j < m - 1 && L[j] < target) ++j;
        const float span = L[j] - L[j - 1], u = span > 0 ? std::clamp((target - L[j - 1]) / span, 0.0f, 1.0f) : 0;
        for (int k = 0; k < 3; ++k) out[i * 3 + k] = E[(j - 1) * 3 + k] + (E[j * 3 + k] - E[(j - 1) * 3 + k]) * u;
    }
}
}  // namespace

bool exportTfx(const Hair::Curves& c, const std::string& path, int vps, float scale, bool zUp, std::string& err) {
    if (vps < 4 || vps > 64 || (vps & (vps - 1))) { err = "Points per strand must be 4, 8, 16, 32 or 64 (TressFX)"; return false; }
    const int nc = c.curveCount();
    if (nc == 0) { err = "No strands to export"; return false; }
    TfxHeader h{}; h.version = 4.0f; h.numHairStrands = (uint32_t)nc; h.numVerticesPerStrand = (uint32_t)vps;
    h.offsetVertexPosition = sizeof(TfxHeader);
    h.offsetStrandUV = h.offsetVertexPosition + (uint32_t)(nc * vps * 16);
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) { err = "Couldn't write " + path; return false; }
    std::fwrite(&h, sizeof h, 1, f);
    std::vector<float> r;
    for (int ci = 0; ci < nc; ++ci) {
        resample(c.p(c.first(ci)), c.count(ci), vps, r);
        for (int i = 0; i < vps; ++i) {
            float p[4] = {r[i * 3] * scale, r[i * 3 + 1] * scale, r[i * 3 + 2] * scale, i < 2 ? 0.0f : 1.0f};   // w: inverse mass, the first two fixed
            if (zUp) std::swap(p[1], p[2]);
            std::fwrite(p, 16, 1, f);
        }
    }
    for (int ci = 0; ci < nc; ++ci) {   // strand uvs: the root's place on the body's uv map
        const float uv[2] = {ci * 2 + 1 < (int)c.surfaceUv.size() ? c.surfaceUv[ci * 2] : 0.0f, ci * 2 + 1 < (int)c.surfaceUv.size() ? c.surfaceUv[ci * 2 + 1] : 0.0f};
        std::fwrite(uv, 8, 1, f);
    }
    const bool ok = std::ferror(f) == 0;
    std::fclose(f);
    if (!ok) err = "Couldn't write " + path;
    return ok;
}

bool exportTfxBone(const Hair::Curves& c, const std::string& path, const std::vector<std::string>& boneNames,
                   const std::vector<std::vector<std::pair<int, float>>>& vw, const Hair::Surface& s, std::string& err) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) { err = "Couldn't write " + path; return false; }
    auto i32 = [&](int32_t v) { std::fwrite(&v, 4, 1, f); };
    i32((int32_t)boneNames.size());
    for (size_t i = 0; i < boneNames.size(); ++i) {   // index, length with the terminating zero, the name
        i32((int32_t)i); i32((int32_t)boneNames[i].size() + 1);
        std::fwrite(boneNames[i].c_str(), 1, boneNames[i].size() + 1, f);
    }
    const int nc = c.curveCount();
    i32(nc);
    for (int ci = 0; ci < nc; ++ci) {
        // the root triangle's three vertices' weights, by the root's barycentric place; the four strongest, summing to 1
        std::vector<std::pair<int, float>> acc;
        const int t = ci < (int)c.rootTri.size() ? c.rootTri[ci] : -1;
        if (t >= 0 && t * 3 + 2 < (int)s.tri.size())
            for (int k = 0; k < 3; ++k) {
                const int v = s.tri[t * 3 + k]; const float b = c.rootBary[ci * 3 + k];
                if (v < (int)vw.size()) for (const auto& bw : vw[v]) {
                    bool found = false;
                    for (auto& e : acc) if (e.first == bw.first) { e.second += bw.second * b; found = true; }
                    if (!found) acc.push_back({bw.first, bw.second * b});
                }
            }
        std::sort(acc.begin(), acc.end(), [](const auto& x, const auto& y) { return x.second > y.second; });
        if (acc.size() > 4) acc.resize(4);
        float sum = 0; for (auto& e : acc) sum += e.second;
        if (acc.empty() || sum <= 0) { acc = {{0, 1.0f}}; sum = 1; }
        i32(ci);
        for (int k = 0; k < 4; ++k) {
            const int bone = k < (int)acc.size() ? acc[k].first : 0;
            const float w = k < (int)acc.size() ? acc[k].second / sum : 0.0f;
            i32(bone); std::fwrite(&w, 4, 1, f);
        }
    }
    const bool ok = std::ferror(f) == 0;
    std::fclose(f);
    if (!ok) err = "Couldn't write " + path;
    return ok;
}

}  // namespace HairSim
