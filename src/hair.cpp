// Pygmalion - strand hair: the curves, the surface they grow on, the tool stack and its evaluation.
// The tools are Blender 4.0's procedural hair node groups (GPL-2.0-or-later), ported node graph by node graph; each
// one's sliders come from hair_tools.inc (generated from Blender's own asset file).
#include "hair.h"
#include "human.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <cstdio>
#include "json.h"

namespace Hair {

// ---------- Blender's hash (BLI_hash.h: Bob Jenkins' lookup3 final mix) ----------
static inline uint32_t rot(uint32_t x, int k) { return (x << k) | (x >> (32 - k)); }
static inline void finalMix(uint32_t& a, uint32_t& b, uint32_t& c) {
    c ^= b; c -= rot(b, 14);
    a ^= c; a -= rot(c, 11);
    b ^= a; b -= rot(a, 25);
    c ^= b; c -= rot(b, 16);
    a ^= c; a -= rot(c, 4);
    b ^= a; b -= rot(a, 14);
    c ^= b; c -= rot(b, 24);
}
uint32_t hash(uint32_t kx) {
    uint32_t a, b, c; a = b = c = 0xdeadbeef + (1 << 2) + 13;
    a += kx; finalMix(a, b, c); return c;
}
uint32_t hash(uint32_t kx, uint32_t ky) {
    uint32_t a, b, c; a = b = c = 0xdeadbeef + (2 << 2) + 13;
    b += ky; a += kx; finalMix(a, b, c); return c;
}
uint32_t hash(uint32_t kx, uint32_t ky, uint32_t kz) {
    uint32_t a, b, c; a = b = c = 0xdeadbeef + (3 << 2) + 13;
    c += kz; b += ky; a += kx; finalMix(a, b, c); return c;
}
float hashFloat(uint32_t kx, uint32_t ky, uint32_t kz) { return (float)hash(kx, ky, kz) / (float)0xFFFFFFFFu; }
float hashFloat2(uint32_t kx, uint32_t ky) { return (float)hash(kx, ky) / (float)0xFFFFFFFFu; }

// ---------- curves and surface ----------
void Curves::clear() { *this = Curves(); }

int Curves::addCurve(int n) {
    const int c = curveCount();
    offsets.push_back(offsets.back() + n);
    pos.resize(pos.size() + (size_t)n * 3, 0.0f);
    radius.resize(radius.size() + n, 0.005f);
    surfaceUv.resize(surfaceUv.size() + 2, 0.0f);
    rootTri.push_back(-1);
    rootBary.resize(rootBary.size() + 3, 0.0f);
    id.push_back(c);
    guideIndex.push_back(-1);
    guideMask.push_back(0);
    return c;
}

void Surface::at(int t, const float b[3], float P[3], float N[3], float U[2]) const {
    const int* v = &tri[t * 3];
    for (int k = 0; k < 3; ++k) {
        P[k] = b[0] * pos[v[0] * 3 + k] + b[1] * pos[v[1] * 3 + k] + b[2] * pos[v[2] * 3 + k];
        N[k] = b[0] * nrm[v[0] * 3 + k] + b[1] * nrm[v[1] * 3 + k] + b[2] * nrm[v[2] * 3 + k];
    }
    const float l = std::sqrt(N[0] * N[0] + N[1] * N[1] + N[2] * N[2]);
    if (l > 0) for (int k = 0; k < 3; ++k) N[k] /= l;
    if (U) {
        if (uv.empty()) { U[0] = U[1] = 0; return; }
        for (int k = 0; k < 2; ++k) U[k] = b[0] * uv[v[0] * 2 + k] + b[1] * uv[v[1] * 2 + k] + b[2] * uv[v[2] * 2 + k];
    }
}

// ---------- the tools and their sliders ----------
struct ParamDef { const char* name; Param::Kind kind; float v0, v1, v2, min, max; const char* tip; };
struct ToolDef { const char* type; const char* description; std::vector<ParamDef> params; };
static const std::vector<ToolDef>& toolDefs() {
    static const std::vector<ToolDef> defs = {
#include "hair_tools.inc"
    };
    return defs;
}

const std::vector<std::string>& toolTypes() {
    // Blender's own order in its Add > Hair menu: Deformation, Generation, Guides, Utility (the info-only groups,
    // which output fields rather than change the hair, are left out)
    static const std::vector<std::string> order = {
        "Blend Hair Curves", "Braid Hair Curves", "Clump Hair Curves", "Curl Hair Curves", "Displace Hair Curves",
        "Frizz Hair Curves", "Hair Curves Noise", "Roll Hair Curves", "Rotate Hair Curves", "Shrinkwrap Hair Curves",
        "Smooth Hair Curves", "Straighten Hair Curves", "Trim Hair Curves",
        "Duplicate Hair Curves", "Generate Hair Curves", "Interpolate Hair Curves",
        "Create Guide Index Map",
        "Attach Hair Curves to Surface", "Redistribute Curve Points", "Restore Curve Segment Length", "Set Hair Curve Profile"};
    return order;
}

Tool makeTool(const std::string& type) {
    Tool t; t.type = type;
    for (const auto& d : toolDefs()) {
        if (type != d.type) continue;
        for (const auto& p : d.params) {
            Param q; q.name = p.name; q.kind = p.kind; q.v[0] = p.v0; q.v[1] = p.v1; q.v[2] = p.v2;
            q.min = p.min; q.max = p.max; q.tip = p.tip;
            // the slider ends somewhere sensible when Blender leaves the range open (typing still goes further)
            q.softMax = p.max < 1e6f ? p.max : std::max(1.0f, std::fabs(p.v0) * 10);
            t.params.push_back(q);
        }
    }
    return t;
}

float Tool::get(const char* name) const {
    for (const auto& p : params) if (p.name == name) return p.v[0];
    return 0;
}
const float* Tool::getv(const char* name) const {
    static const float zero[3] = {0, 0, 0};
    for (const auto& p : params) if (p.name == name) return p.v;
    return zero;
}
void Tool::setv(const char* name, const float v[3]) {
    for (auto& p : params) if (p.name == name) { std::memcpy(p.v, v, sizeof p.v); return; }
}
void Tool::set(const char* name, float v) {
    for (auto& p : params) if (p.name == name) { p.v[0] = v; return; }
}

// ---------- the tools ----------

// Distribute Points on Faces (Blender's node, geometry_nodes/node_geo_distribute_points_on_faces.cc): random points
// on the surface's triangles, density per unit area (times the mask), and for Poisson a minimum distance between
// them (Blender scatters at Density Max, then removes points closer than Distance Min).
struct Scatter { int tri; float bary[3]; };
static std::vector<Scatter> distribute(const Surface& s, float density, bool poisson, float distMin, float densityFactor, int seed) {
    std::vector<Scatter> out;
    const int nt = (int)s.tri.size() / 3;
    for (int t = 0; t < nt; ++t) {
        const int* v = &s.tri[t * 3];
        const float* a = &s.pos[v[0] * 3]; const float* b = &s.pos[v[1] * 3]; const float* c = &s.pos[v[2] * 3];
        float e1[3], e2[3], cr[3];
        for (int k = 0; k < 3; ++k) { e1[k] = b[k] - a[k]; e2[k] = c[k] - a[k]; }
        cr[0] = e1[1] * e2[2] - e1[2] * e2[1]; cr[1] = e1[2] * e2[0] - e1[0] * e2[2]; cr[2] = e1[0] * e2[1] - e1[1] * e2[0];
        const float area = 0.5f * std::sqrt(cr[0] * cr[0] + cr[1] * cr[1] + cr[2] * cr[2]);
        float m = 1;
        if (!s.mask.empty()) m = (s.mask[v[0]] + s.mask[v[1]] + s.mask[v[2]]) / 3.0f;
        if (m <= 0) continue;
        // Blender: the expected count per face, its fraction decided by a hash of the face and seed
        const float expected = area * density * m;
        int n = (int)expected;
        if (hashFloat((uint32_t)t, (uint32_t)seed, 1) < expected - n) ++n;
        for (int i = 0; i < n; ++i) {
            float r1 = hashFloat((uint32_t)t, (uint32_t)i, (uint32_t)seed * 2 + 0);
            float r2 = hashFloat((uint32_t)t, (uint32_t)i, (uint32_t)seed * 2 + 1);
            if (r1 + r2 > 1) { r1 = 1 - r1; r2 = 1 - r2; }
            Scatter sc; sc.tri = t; sc.bary[0] = 1 - r1 - r2; sc.bary[1] = r1; sc.bary[2] = r2;
            out.push_back(sc);
        }
    }
    if (!poisson || distMin <= 0) return out;
    // Poisson: keep points in a random order, dropping any closer than distMin to one kept (a grid of distMin cells),
    // then thin to Density Factor
    std::vector<int> order(out.size());
    for (size_t i = 0; i < order.size(); ++i) order[i] = (int)i;
    std::sort(order.begin(), order.end(), [&](int x, int y) { return hash((uint32_t)x, (uint32_t)seed) < hash((uint32_t)y, (uint32_t)seed); });
    struct Key { long long x, y, z; bool operator==(const Key& o) const { return x == o.x && y == o.y && z == o.z; } };
    struct KeyHash { size_t operator()(const Key& k) const { return (size_t)(k.x * 73856093 ^ k.y * 19349663 ^ k.z * 83492791); } };
    std::unordered_map<Key, std::vector<int>, KeyHash> grid;
    std::vector<Scatter> kept;
    std::vector<float> keptPos;
    const float d2 = distMin * distMin;
    for (int i : order) {
        float P[3], N[3];
        s.at(out[i].tri, out[i].bary, P, N, nullptr);
        const Key k{(long long)std::floor(P[0] / distMin), (long long)std::floor(P[1] / distMin), (long long)std::floor(P[2] / distMin)};
        bool near = false;
        for (long long dx = -1; dx <= 1 && !near; ++dx)
            for (long long dy = -1; dy <= 1 && !near; ++dy)
                for (long long dz = -1; dz <= 1 && !near; ++dz) {
                    auto it = grid.find(Key{k.x + dx, k.y + dy, k.z + dz});
                    if (it == grid.end()) continue;
                    for (int j : it->second) {
                        const float* q = &keptPos[j * 3];
                        const float ex = q[0] - P[0], ey = q[1] - P[1], ez = q[2] - P[2];
                        if (ex * ex + ey * ey + ez * ez < d2) { near = true; break; }
                    }
                }
        if (near) continue;
        grid[k].push_back((int)kept.size());
        kept.push_back(out[i]);
        keptPos.insert(keptPos.end(), P, P + 3);
    }
    if (densityFactor < 1) {
        std::vector<Scatter> thin;
        for (size_t i = 0; i < kept.size(); ++i)
            if (hashFloat((uint32_t)i, (uint32_t)seed, 7) < densityFactor) thin.push_back(kept[i]);
        kept.swap(thin);
    }
    return kept;
}

// Generate Hair Curves: points scattered over the surface, each grown into a straight curve along the surface
// normal (Blender interpolates one guide, a line along +Z, onto each point with the normal as its up), Hair Length
// long with Control Points points; the root's uv stored as surface_uv_coordinate.
static void generate(const Tool& t, Curves& c, const Surface& s, bool viewport) {
    const float density = t.get("Density"), length = t.get("Hair Length");
    const int points = std::max(2, t.geti("Control Points"));
    const bool poisson = t.getb("Poisson Disk Distribution");
    const float viewportAmount = viewport ? std::clamp(t.get("Viewport Amount"), 0.0f, 1.0f) : 1.0f;
    const float maskFactor = t.get("Density Mask");
    const int seed = t.geti("Seed");
    std::vector<Scatter> pts;
    if (poisson) {
        // Blender: Distance Min = sqrt(0.3 / Density), Density Max = Density * 2 * viewport, Density Factor = mask
        const float distMin = density > 0 ? std::sqrt(0.3f / density) : 0;
        pts = distribute(s, density * 2 * viewportAmount, true, distMin, maskFactor, seed);
    } else {
        pts = distribute(s, density * viewportAmount * maskFactor, false, 0, 1, seed);
    }
    for (const auto& sc : pts) {
        float P[3], N[3], U[2];
        s.at(sc.tri, sc.bary, P, N, U);
        const int ci = c.addCurve(points);
        c.rootTri[ci] = sc.tri;
        std::memcpy(&c.rootBary[ci * 3], sc.bary, sizeof(sc.bary));
        c.surfaceUv[ci * 2] = U[0]; c.surfaceUv[ci * 2 + 1] = U[1];
        c.id[ci] = (int)hash((uint32_t)sc.tri, (uint32_t)(sc.bary[1] * 1e6f), (uint32_t)(sc.bary[2] * 1e6f)) & 0x3fffffff;
        const int f = c.first(ci);
        for (int i = 0; i < points; ++i) {
            const float u = (float)i / (points - 1) * length;
            for (int k = 0; k < 3; ++k) c.pos[(f + i) * 3 + k] = P[k] + N[k] * u;
        }
    }
}

// ---------- Blender's helpers ----------
float shapeRange(float value, float min, float max, float shape, float base) {
    if (std::fabs(shape) <= 1e-5f) return 1;                                // Shape 0: constant
    const float x = (max != min) ? (value - min) / (max - min) : 0;         // Map Range (unclamped)
    const float k = std::min(std::fabs(shape), 0.999999f);
    const float b = std::max(base, 1.00001f);
    float pp = std::fmod(k, 1.0f); if (pp > 0.5f) pp = 1.0f - pp;           // ping-pong with 0.5
    const float m = std::max(1.0f - pp * (1.0f - 1.0f / b) * 2.0f, 1e-5f);
    const float e = -std::log(m) / std::log(std::max(b, 1.000001f));
    const bool flip = k > 0.5f ? !(shape < 0) : (shape < 0);
    const float y = flip ? 1.0f - x : x;
    const float p = std::pow(std::max(y, 0.0f), e);
    const float r = k > 0.5f ? 1.0f - p : p;
    return std::clamp(min + r * (max - min), std::min(min, max), std::max(min, max));
}

float randomFloat(int id, int seed, float min, float max) { return hashFloat2((uint32_t)seed, (uint32_t)id) * (max - min) + min; }
void randomVector(int id, int seed, const float min[3], const float max[3], float out[3]) {
    for (int k = 0; k < 3; ++k) out[k] = hashFloat((uint32_t)seed, (uint32_t)id, (uint32_t)k) * (max[k] - min[k]) + min[k];
}
int randomInt(int id, int seed, int min, int max) {
    // Blender 4.0's Random Value (Integer): interpf(max, min, hash_to_float(id, seed)), rounded, all in float
    const float v = hashFloat2((uint32_t)id, (uint32_t)seed);
    // as Blender 4.0 computes it: min + (max - min) * v, each step a float (checked against Blender for 8 id/seed pairs)
    volatile float range = (float)max - (float)min;
    volatile float scaled = range * v;
    const float x = (float)min + scaled;
    return (int)std::floor(x + 0.5f);
}

static inline void norm3(float* v) { const float l = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]); if (l > 0) for (int k = 0; k < 3; ++k) v[k] /= l; }
static inline void cross3(const float* a, const float* b, float* o) { o[0] = a[1] * b[2] - a[2] * b[1]; o[1] = a[2] * b[0] - a[0] * b[2]; o[2] = a[0] * b[1] - a[1] * b[0]; }
static inline float dot3(const float* a, const float* b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
static inline float len3(const float* a) { return std::sqrt(dot3(a, a)); }
// v rotated round a unit axis by angle (Rodrigues)
static void rotateAround(const float* v, const float* axis, float angle, float* o) {
    const float c = std::cos(angle), s = std::sin(angle), d = dot3(axis, v);
    float x[3]; cross3(axis, v, x);
    for (int k = 0; k < 3; ++k) o[k] = v[k] * c + x[k] * s + axis[k] * d * (1 - c);
}

void curveFrames(const float* P, int n, std::vector<float>& T, std::vector<float>& N) {
    T.assign((size_t)n * 3, 0.0f); N.assign((size_t)n * 3, 0.0f);
    if (n == 0) return;
    if (n == 1) { T[2] = 1; N[0] = 1; return; }
    auto deltaDir = [&](int a, int b, float* out) { float d[3] = {P[b * 3] - P[a * 3], P[b * 3 + 1] - P[a * 3 + 1], P[b * 3 + 2] - P[a * 3 + 2]}; const float l = len3(d); if (l < 1e-9f) return false; for (int k = 0; k < 3; ++k) out[k] = d[k] / l; return true; };
    int first = -1;
    for (int i = 0; i + 1 < n; ++i) if (deltaDir(i, i + 1, &T[i * 3])) { first = i; break; }
    if (first < 0) { for (int i = 0; i < n; ++i) T[i * 3 + 2] = 1; }
    else {
        for (int i = 0; i < first; ++i) std::memcpy(&T[i * 3], &T[first * 3], 12);
        float prev[3]; std::memcpy(prev, &T[first * 3], 12); bool prevEqual = false;
        for (int i = first + 1; i < n - 1; ++i) {   // direction_bisect
            float nd[3] = {P[(i + 1) * 3] - P[i * 3], P[(i + 1) * 3 + 1] - P[i * 3 + 1], P[(i + 1) * 3 + 2] - P[i * 3 + 2]};
            const float nl = len3(nd); const bool wasEqual = prevEqual; prevEqual = nl < 1e-9f;
            float* t = &T[i * 3];
            if (prevEqual) { std::memcpy(t, prev, 12); continue; }
            float pd[3]; std::memcpy(pd, prev, 12);
            for (int k = 0; k < 3; ++k) prev[k] = nd[k] / nl;
            if (wasEqual) { std::memcpy(t, prev, 12); continue; }
            float sum[3] = {pd[0] + prev[0], pd[1] + prev[1], pd[2] + prev[2]};
            const float sl = len3(sum);
            if (sl < 0.6627619f) {
                if (sl < 2e-7f) { std::memcpy(t, prev, 12); continue; }
                float bn[3], nm[3] = {prev[0] - pd[0], prev[1] - pd[1], prev[2] - pd[2]};
                cross3(prev, pd, bn); cross3(bn, nm, t); norm3(t); continue;
            }
            for (int k = 0; k < 3; ++k) t[k] = sum[k] / sl;
        }
        if (!deltaDir(n - 2, n - 1, &T[(n - 1) * 3])) std::memcpy(&T[(n - 1) * 3], prev, 12);
    }
    // minimum twist normals
    const float* t0 = &T[0];
    if (std::fabs(t0[0]) + std::fabs(t0[1]) < 1e-4f) { N[0] = 1; N[1] = 0; N[2] = 0; }
    else { N[0] = t0[1]; N[1] = -t0[0]; N[2] = 0; norm3(&N[0]); }
    for (int i = 1; i < n; ++i) {
        const float* a = &T[(i - 1) * 3]; const float* b = &T[i * 3];
        std::memcpy(&N[i * 3], &N[(i - 1) * 3], 12);
        if (len3(a) == 0 || len3(b) == 0) continue;
        const float ang = std::acos(std::clamp(dot3(a, b), -1.0f, 1.0f));
        if (ang == 0) continue;
        float ax[3]; cross3(a, b, ax); if (len3(ax) == 0) continue; norm3(ax);
        rotateAround(&N[(i - 1) * 3], ax, ang, &N[i * 3]); norm3(&N[i * 3]);
    }
}

// a curve's spline parameter (Factor): its length so far over its whole length
static void splineFactor(const float* P, int n, std::vector<float>& f) {
    f.assign(n, 0.0f); float total = 0;
    for (int i = 1; i < n; ++i) { const float d[3] = {P[i * 3] - P[(i - 1) * 3], P[i * 3 + 1] - P[(i - 1) * 3 + 1], P[i * 3 + 2] - P[(i - 1) * 3 + 2]}; total += len3(d); f[i] = total; }
    if (total > 0) for (float& x : f) x /= total;
}

void restoreSegmentLength(Curves& c, const std::vector<float>& ref, float factor, float pin, const std::vector<char>* sel) {
    if (factor <= 0) return;
    for (int ci = 0; ci < c.curveCount(); ++ci) {
        const int a = c.first(ci), n = c.count(ci);
        std::vector<float> cur(c.pos.begin() + a * 3, c.pos.begin() + (a + n) * 3), out = cur;
        for (int i = 1; i < n; ++i) {
            if (sel && !(*sel)[a + i]) { for (int k = 0; k < 3; ++k) out[i * 3 + k] = out[(i - 1) * 3 + k] + (cur[i * 3 + k] - cur[(i - 1) * 3 + k]); continue; }
            float d[3] = {cur[i * 3] - cur[(i - 1) * 3], cur[i * 3 + 1] - cur[(i - 1) * 3 + 1], cur[i * 3 + 2] - cur[(i - 1) * 3 + 2]};
            const float l = len3(d); if (l > 0) for (float& x : d) x /= l;
            const float* r0 = &ref[(a + i - 1) * 3]; const float* r1 = &ref[(a + i) * 3];
            const float rd[3] = {r1[0] - r0[0], r1[1] - r0[1], r1[2] - r0[2]};
            const float L = l + (len3(rd) - l) * std::clamp(factor, 0.0f, 1.0f);
            for (int k = 0; k < 3; ++k) out[i * 3 + k] = out[(i - 1) * 3 + k] + d[k] * L;
        }
        if (pin > 0) {   // keep the point at parameter pin where the reference had it
            std::vector<float> fr, fo; splineFactor(&ref[a * 3], n, fr); splineFactor(out.data(), n, fo);
            auto sample = [&](const float* P, const std::vector<float>& f, float t, float* o) {
                int j = 1; while (j < n - 1 && f[j] < t) ++j;
                const float span = f[j] - f[j - 1], u = span > 0 ? (t - f[j - 1]) / span : 0;
                for (int k = 0; k < 3; ++k) o[k] = P[(j - 1) * 3 + k] + (P[j * 3 + k] - P[(j - 1) * 3 + k]) * u;
            };
            float pr[3], po[3]; sample(&ref[a * 3], fr, pin, pr); sample(out.data(), fo, pin, po);
            for (int i = 0; i < n; ++i) for (int k = 0; k < 3; ++k) out[i * 3 + k] += pr[k] - po[k];
        }
        std::copy(out.begin(), out.end(), c.pos.begin() + a * 3);
    }
}

static float cumLengths(const float* P, int n, std::vector<float>& L);
// ---------- Blender's Perlin noise (blenlib noise.cc) and its Noise Texture (3D, fBM) ----------
static inline uint32_t floatBits(float f) { uint32_t u; std::memcpy(&u, &f, 4); return u; }
static inline float fadeP(float t) { return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); }
static inline float negateIf(float v, uint32_t c) { uint32_t b; std::memcpy(&b, &v, 4); b ^= (uint32_t)(c != 0u) << 31; float o; std::memcpy(&o, &b, 4); return o; }
static inline float gradP(uint32_t h, float x, float y, float z) {
    h &= 15u;
    const float u = h < 8u ? x : y, vt = (h & 13u) == 12u ? x : z, v = h < 4u ? y : vt;
    return negateIf(u, h & 1u) + negateIf(v, h & 2u);
}
static inline float mix8(float v0, float v1, float v2, float v3, float v4, float v5, float v6, float v7, float x, float y, float z) {
    const float x1 = 1 - x, y1 = 1 - y, z1 = 1 - z;
    return z1 * (y1 * (v0 * x1 + v1 * x) + y * (v2 * x1 + v3 * x)) + z * (y1 * (v4 * x1 + v5 * x) + y * (v6 * x1 + v7 * x));
}
static float perlinNoise3(float px, float py, float pz) {
    const int X = (int)std::floor(px), Y = (int)std::floor(py), Z = (int)std::floor(pz);
    const float fx = px - (float)X, fy = py - (float)Y, fz = pz - (float)Z;
    const float u = fadeP(fx), v = fadeP(fy), w = fadeP(fz);
    auto H = [](int a, int b, int c) { return hash((uint32_t)a, (uint32_t)b, (uint32_t)c); };
    return mix8(gradP(H(X, Y, Z), fx, fy, fz), gradP(H(X + 1, Y, Z), fx - 1, fy, fz), gradP(H(X, Y + 1, Z), fx, fy - 1, fz),
                gradP(H(X + 1, Y + 1, Z), fx - 1, fy - 1, fz), gradP(H(X, Y, Z + 1), fx, fy, fz - 1), gradP(H(X + 1, Y, Z + 1), fx - 1, fy, fz - 1),
                gradP(H(X, Y + 1, Z + 1), fx, fy - 1, fz - 1), gradP(H(X + 1, Y + 1, Z + 1), fx - 1, fy - 1, fz - 1), u, v, w);
}
static inline float precisionCorrection(float x) { return std::fmod(x, 100000.0f) + 0.5f * (std::fabs(x) >= 1000000.0f ? 1.0f : 0.0f); }
static float perlinSigned3(const float p[3]) {
    // Blender's math::mod is fmod with the sign of x (C's fmodf)
    return perlinNoise3(precisionCorrection(p[0]), precisionCorrection(p[1]), precisionCorrection(p[2])) * 0.9820f;
}
static float perlinFbm3(const float p[3], float detail, float roughness, float lacunarity, bool normalize) {
    float fscale = 1, amp = 1, maxamp = 0, sum = 0;
    for (int i = 0; i <= (int)detail; ++i) {
        const float q[3] = {fscale * p[0], fscale * p[1], fscale * p[2]};
        sum += perlinSigned3(q) * amp; maxamp += amp; amp *= roughness; fscale *= lacunarity;
    }
    const float rmd = detail - std::floor(detail);
    if (rmd != 0) {
        const float q[3] = {fscale * p[0], fscale * p[1], fscale * p[2]};
        const float sum2 = sum + perlinSigned3(q) * amp;
        return normalize ? (0.5f * sum / maxamp + 0.5f) * (1 - rmd) + (0.5f * sum2 / (maxamp + amp) + 0.5f) * rmd : sum * (1 - rmd) + sum2 * rmd;
    }
    return normalize ? 0.5f * sum / maxamp + 0.5f : sum;
}
static void randomFloat3Offset(float seed, float o[3]) {
    for (int k = 0; k < 3; ++k) o[k] = 100.0f + (float)hash(floatBits(seed), floatBits((float)k)) / (float)0xFFFFFFFFu * 100.0f;
}
// the Noise Texture node's Color: three fBM noises, the 2nd and 3rd at Blender's fixed offsets (seeds 3 and 4)
static void noiseTextureColor(const float vec[3], float scale, float detail, float roughness, float lacunarity, bool normalize, float out[3]) {
    const float p[3] = {vec[0] * scale, vec[1] * scale, vec[2] * scale};
    float o3[3], o4[3]; randomFloat3Offset(3.0f, o3); randomFloat3Offset(4.0f, o4);
    const float p3[3] = {p[0] + o3[0], p[1] + o3[1], p[2] + o3[2]}, p4[3] = {p[0] + o4[0], p[1] + o4[1], p[2] + o4[2]};
    out[0] = perlinFbm3(p, detail, roughness, lacunarity, normalize);
    out[1] = perlinFbm3(p3, detail, roughness, lacunarity, normalize);
    out[2] = perlinFbm3(p4, detail, roughness, lacunarity, normalize);
}

// Hair Curves Noise: each point pushed by 3D noise in its curve's frame; the noise read at the curve's root (times
// Scale, shifted by the seed) and along the curve (Scale along Curve, plus Offset per Curve at random); added up along
// the curve or not, from the root, faded by Shape, Preserve Length after.
static void curvesNoise(const Tool& t, Curves& c, const Surface&, bool) {
    const float factor = t.get("Factor"), dist = t.get("Distance"), shape = t.get("Shape");
    if (factor == 0) return;
    const float scale = t.get("Scale"), along = t.get("Scale along Curve"), perCurve = t.get("Offset per Curve");
    const bool cumulative = t.getb("Cumulative Offset"), preserve = t.getb("Preserve Length");
    const int seed = t.geti("Seed");
    const int hseed = randomInt(seed, 368741, -1073741823, 1073741823);
    const float lo[3] = {-1, -1, -1}, hi[3] = {1, 1, 1};
    float shift[3]; randomVector(seed, hseed, lo, hi, shift);
    for (float& s : shift) s *= 100.0f;
    const std::vector<float> before = c.pos;
    std::vector<float> T, N, F, L, off;
    for (int ci = 0; ci < c.curveCount(); ++ci) {
        const int a = c.first(ci), n = c.count(ci);
        curveFrames(&before[a * 3], n, T, N);
        splineFactor(&before[a * 3], n, F);
        cumLengths(&before[a * 3], n, L);
        const float* root = &before[a * 3];
        const float curveOff = randomFloat(a, hseed, -1, 1) * perCurve;
        off.assign((size_t)n * 3, 0.0f);
        for (int i = 0; i < n; ++i) {
            const float v[3] = {root[0] * scale + shift[0], root[1] * scale + shift[1], root[2] * scale + shift[2] + L[i] * along + curveOff};
            float col[3]; noiseTextureColor(v, 5.0f, 0, 0, 2, true, col);
            const float r[3] = {(col[0] - 0.5f) * 2, (col[1] - 0.5f) * 2, (col[2] - 0.5f) * 2};
            const float* tg = &T[i * 3]; const float* nm = &N[i * 3]; float bi[3]; cross3(tg, nm, bi);
            for (int k = 0; k < 3; ++k) off[i * 3 + k] = (tg[k] * r[0] + bi[k] * r[1] + nm[k] * r[2]) * (factor * dist);
        }
        float lead[3] = {0, 0, 0};
        for (int i = 0; i < n; ++i) {
            float v[3];
            if (cumulative) {
                float trail[3] = {lead[0], lead[1], lead[2]};
                for (int k = 0; k < 3; ++k) lead[k] += off[i * 3 + k];
                for (int k = 0; k < 3; ++k) v[k] = 0.5f * (lead[k] + trail[k] - off[k]);
            } else for (int k = 0; k < 3; ++k) v[k] = off[i * 3 + k] - off[k];
            const float s = shapeRange(F[i], 0, 1, shape, 2);
            for (int k = 0; k < 3; ++k) c.pos[(a + i) * 3 + k] += v[k] * s;
        }
    }
    if (preserve) restoreSegmentLength(c, before, 1.0f);
}

// Frizz Hair Curves: each point pushed a random distance in its curve's frame (along, across, out), the offsets added
// up along the curve (Cumulative Offset) or not, measured from the root so it stays, faded by Shape; Preserve Length
// then gives each segment its old length back.
static void frizz(const Tool& t, Curves& c, const Surface&, bool) {
    const float factor = t.get("Factor"), dist = t.get("Distance"), shape = t.get("Shape");
    if (factor == 0) return;
    const bool cumulative = t.getb("Cumulative Offset"), preserve = t.getb("Preserve Length");
    const int hseed = randomInt(t.geti("Seed"), 64785, -1073741823, 1073741823);   // Blender's "Hash Seed"
    const std::vector<float> before = c.pos;
    const float lo[3] = {-1, -1, -1}, hi[3] = {1, 1, 1};
    std::vector<float> T, N, F, off;
    for (int ci = 0; ci < c.curveCount(); ++ci) {
        const int a = c.first(ci), n = c.count(ci);
        curveFrames(&before[a * 3], n, T, N);
        splineFactor(&before[a * 3], n, F);
        off.assign((size_t)n * 3, 0.0f);
        for (int i = 0; i < n; ++i) {
            float r[3]; randomVector(a + i, hseed, lo, hi, r);
            const float* tg = &T[i * 3]; const float* nm = &N[i * 3]; float bi[3]; cross3(tg, nm, bi);
            for (int k = 0; k < 3; ++k) off[i * 3 + k] = (tg[k] * r[0] + bi[k] * r[1] + nm[k] * r[2]) * (dist * factor);
        }
        float lead[3] = {0, 0, 0};
        for (int i = 0; i < n; ++i) {
            float v[3];
            if (cumulative) {   // (leading + trailing - the root's own) / 2
                float trail[3] = {lead[0], lead[1], lead[2]};
                for (int k = 0; k < 3; ++k) lead[k] += off[i * 3 + k];
                for (int k = 0; k < 3; ++k) v[k] = 0.5f * (lead[k] + trail[k] - off[k]);
            } else for (int k = 0; k < 3; ++k) v[k] = off[i * 3 + k] - off[k];
            const float s = shapeRange(F[i], 0, 1, shape, 2);
            for (int k = 0; k < 3; ++k) c.pos[(a + i) * 3 + k] += v[k] * s;
        }
    }
    if (preserve) restoreSegmentLength(c, before, 1.0f);
}

// Blender's Blur Attribute on curves: each point mixed with its neighbours along the curve, the neighbours weighted by
// the point's own weight, iterations times
static void blurOnCurves(const Curves& c, std::vector<float>& v, const std::vector<float>& w, int iterations) {
    std::vector<float> src;
    for (int it = 0; it < iterations; ++it) {
        src = v;
        for (int ci = 0; ci < c.curveCount(); ++ci) {
            const int a = c.first(ci), n = c.count(ci);
            if (n < 2) continue;
            for (int i = 0; i < n; ++i) {
                const int p = a + i; const float wi = w[p];
                float sum[3] = {src[p * 3], src[p * 3 + 1], src[p * 3 + 2]}; float tot = 1;
                if (i > 0) { for (int k = 0; k < 3; ++k) sum[k] += src[(p - 1) * 3 + k] * wi; tot += wi; }
                if (i < n - 1) { for (int k = 0; k < 3; ++k) sum[k] += src[(p + 1) * 3 + k] * wi; tot += wi; }
                for (int k = 0; k < 3; ++k) v[p * 3 + k] = tot > 0 ? sum[k] / tot : src[p * 3 + k];
            }
        }
    }
}

// Smooth Hair Curves: the points blurred along each curve (Blur Attribute), weighted by Weight and Shape (base e),
// never the root, the tip only without Lock Tips; mixed in by Amount; Preserve Length gives the lengths back.
static void smooth(const Tool& t, Curves& c, const Surface&, bool) {
    const float amount = t.get("Amount"), weight = t.get("Weight"), shape = t.get("Shape");
    const int iterations = std::max(0, t.geti("Iterations"));
    const bool lockTips = t.getb("Lock Tips"), preserve = t.getb("Preserve Length");
    const bool active = amount != 0 && weight > 0;
    if (!active || iterations == 0) return;
    const std::vector<float> before = c.pos;
    std::vector<float> w(c.pointCount(), 0.0f), F; std::vector<char> sel(c.pointCount(), 0);
    for (int ci = 0; ci < c.curveCount(); ++ci) {
        const int a = c.first(ci), n = c.count(ci);
        splineFactor(&before[a * 3], n, F);
        for (int i = 0; i < n; ++i) {
            const bool s = !(i == n - 1 && lockTips);
            sel[a + i] = s;
            w[a + i] = weight * (s ? 1.0f : 0.0f) * shapeRange(F[i], 0, 1, shape, 2.7182817f) * (i == 0 ? 0.0f : 1.0f);
        }
    }
    std::vector<float> blurred = before;
    blurOnCurves(c, blurred, w, iterations);
    for (int ci = 0; ci < c.curveCount(); ++ci) {
        const int a = c.first(ci), n = c.count(ci);
        float rootFix[3]; for (int k = 0; k < 3; ++k) rootFix[k] = before[a * 3 + k] - blurred[a * 3 + k];
        for (int i = 0; i < n; ++i) {
            const int p = a + i; if (!sel[p]) continue;
            for (int k = 0; k < 3; ++k) c.pos[p * 3 + k] += ((blurred[p * 3 + k] - rootFix[k]) - before[p * 3 + k]) * amount;
        }
    }
    if (preserve) restoreSegmentLength(c, before, 1.0f);
}

// Straighten Hair Curves: each point pulled toward the straight line from root to tip, at its place along the curve,
// by Amount times Shape (base e), at most all the way; Preserve Length gives the lengths back. Not checked against
// Blender: its 4.0.2 asset mixes the root with the root (both Mix inputs read Curve Root), so it collapses every curve
// onto its root at the defaults and does nothing at Amount 0.5; this is what the tool says it does.
static void straighten(const Tool& t, Curves& c, const Surface&, bool) {
    const float amount = t.get("Amount"), shape = t.get("Shape");
    if (amount == 0) return;
    const std::vector<float> before = c.pos; std::vector<float> F;
    for (int ci = 0; ci < c.curveCount(); ++ci) {
        const int a = c.first(ci), n = c.count(ci);
        if (n < 2) continue;
        splineFactor(&before[a * 3], n, F);
        const float* r = &before[a * 3]; const float* tip = &before[(a + n - 1) * 3];
        for (int i = 0; i < n; ++i) {
            const float m = std::min(amount * shapeRange(F[i], 0, 1, shape, 2.7182817f), 1.0f);
            if (m == 0) continue;
            for (int k = 0; k < 3; ++k) {
                const float target = r[k] + (tip[k] - r[k]) * F[i];
                c.pos[(a + i) * 3 + k] = before[(a + i) * 3 + k] + (target - before[(a + i) * 3 + k]) * m;
            }
        }
    }
    if (t.getb("Preserve Length")) restoreSegmentLength(c, before, 1.0f);
}

// a poly curve's cumulative lengths; and the point at a length along it (clamped to the curve, Sample Curve)
static float cumLengths(const float* P, int n, std::vector<float>& L) {
    L.assign(n, 0.0f);
    for (int i = 1; i < n; ++i) { const float d[3] = {P[i * 3] - P[(i - 1) * 3], P[i * 3 + 1] - P[(i - 1) * 3 + 1], P[i * 3 + 2] - P[(i - 1) * 3 + 2]}; L[i] = L[i - 1] + len3(d); }
    return n ? L[n - 1] : 0;
}
static void sampleAtLength(const float* P, int n, const std::vector<float>& L, float len, float* o) {
    if (n == 1 || L[n - 1] <= 0) { std::memcpy(o, P, 12); return; }
    len = std::clamp(len, 0.0f, L[n - 1]);
    int j = 1; while (j < n - 1 && L[j] < len) ++j;
    const float span = L[j] - L[j - 1], u = span > 0 ? (len - L[j - 1]) / span : 0;
    for (int k = 0; k < 3; ++k) o[k] = P[(j - 1) * 3 + k] + (P[j * 3 + k] - P[(j - 1) * 3 + k]) * u;
}

// Blender's hair curves are Catmull-Rom splines (resolution 12): what Sample Curve, Spline Length, Spline Parameter
// and Curve Tangent see is the evaluated curve. Control point i is evaluated point i * res.
struct Evaluated {
    int res = 12; std::vector<float> E, L, T, N; float total = 0;
    void build(const float* P, int n, int resolution = 12) {
        res = std::max(1, resolution);
        if (n < 2) { E.assign(P, P + n * 3); L.assign(n, 0.0f); total = 0; curveFrames(E.data(), n, T, N); return; }
        E.assign((size_t)((n - 1) * res + 1) * 3, 0.0f);
        auto pt = [&](int i) { return &P[std::clamp(i, 0, n - 1) * 3]; };
        for (int s = 0; s < n - 1; ++s) {
            const float* a = pt(s - 1); const float* b = pt(s); const float* c = pt(s + 1); const float* d = pt(s + 2);
            for (int k = 0; k < res; ++k) {
                const float t = (float)k / res, u = 1 - t;
                const float w0 = -t * u * u, w1 = 2 + t * t * (3 * t - 5), w2 = 2 + u * u * (3 * u - 5), w3 = -u * t * t;
                float* o = &E[(size_t)(s * res + k) * 3];
                if (k == 0) { std::memcpy(o, b, 12); continue; }
                for (int j = 0; j < 3; ++j) o[j] = 0.5f * (a[j] * w0 + b[j] * w1 + c[j] * w2 + d[j] * w3);
            }
        }
        std::memcpy(&E[(size_t)((n - 1) * res) * 3], pt(n - 1), 12);
        total = cumLengths(E.data(), (int)E.size() / 3, L);
        curveFrames(E.data(), (int)E.size() / 3, T, N);
    }
    int count() const { return (int)E.size() / 3; }
    float param(int controlPoint) const { return total > 0 ? L[(size_t)controlPoint * res] / total : 0; }
    const float* tangent(int controlPoint) const { return &T[(size_t)controlPoint * res * 3]; }
    void at(float length, float* o) const { sampleAtLength(E.data(), count(), L, length, o); }
};

// Rotate Hair Curves: each curve turned round its root, by Factor x (Angle + its random offset), about Axis (or the
// curve's root-to-tip direction with Lock Ends; the root's direction if the axis is zero)
static void rotateCurves(const Tool& t, Curves& c, const Surface&, bool) {
    const float factor = t.get("Factor"), angle = t.get("Angle"), rnd = t.get("Random Offset");
    const float* axisIn = t.getv("Axis"); const bool lockEnds = t.getb("Lock Ends");
    const int hseed = randomInt(t.geti("Seed"), -87614, -1073741823, 1073741823);
    std::vector<float> T, N;
    for (int ci = 0; ci < c.curveCount(); ++ci) {
        const int a = c.first(ci), n = c.count(ci);
        const float* root = &c.pos[a * 3];
        float ax[3];
        if (lockEnds) { const float* tip = &c.pos[(a + n - 1) * 3]; for (int k = 0; k < 3; ++k) ax[k] = tip[k] - root[k]; norm3(ax); }
        else std::memcpy(ax, axisIn, 12);
        if (ax[0] == 0 && ax[1] == 0 && ax[2] == 0) { curveFrames(root, n, T, N); std::memcpy(ax, &T[0], 12); }
        norm3(ax);
        const float ang = factor * (angle + randomFloat(a, hseed, -0.5f, 0.5f) * rnd);   // Curve ID: the root point's id (its index)
        if (ang == 0 || len3(ax) == 0) continue;
        const float r[3] = {root[0], root[1], root[2]};
        for (int i = a; i < a + n; ++i) {
            float v[3] = {c.pos[i * 3] - r[0], c.pos[i * 3 + 1] - r[1], c.pos[i * 3 + 2] - r[2]}, o[3];
            rotateAround(v, ax, ang, o);
            for (int k = 0; k < 3; ++k) c.pos[i * 3 + k] = r[k] + o[k];
        }
    }
}

// Redistribute Curve Points: the inner points moved along their curve toward even spacing by Factor (with Feature
// Awareness, toward a spacing blurred by how straight each point is, so bends keep more points)
static void redistributeImpl(Curves& c, float factor, bool features) {
    if (factor <= 0) return;
    const std::vector<float> before = c.pos;
    std::vector<float> target; Evaluated ev;
    for (int ci = 0; ci < c.curveCount(); ++ci) {
        const int a = c.first(ci), n = c.count(ci);
        if (n < 3) continue;
        ev.build(&before[a * 3], n);
        const float total = ev.total;
        target.assign(n, 0.0f);
        if (!features) for (int i = 0; i < n; ++i) target[i] = (float)i / (n - 1);
        else {
            std::vector<float> w(n, 0.0f), f(n);
            for (int i = 0; i < n; ++i) {
                f[i] = ev.param(i);
                if (i == 0 || i == n - 1) continue;
                float sd[3] = {before[(a + i) * 3] - before[(a + i - 1) * 3], before[(a + i) * 3 + 1] - before[(a + i - 1) * 3 + 1], before[(a + i) * 3 + 2] - before[(a + i - 1) * 3 + 2]};
                norm3(sd);
                const float x = std::clamp(std::fabs(dot3(ev.tangent(i), sd)), 0.0f, 1.0f);
                w[i] = x * x * (3 - 2 * x);   // Map Range, smooth step
            }
            for (int it = 0; it < 10; ++it) {   // Blur Attribute on the spline parameter
                std::vector<float> src = f;
                for (int i = 0; i < n; ++i) {
                    float s = src[i], tot = 1;
                    if (i > 0) { s += src[i - 1] * w[i]; tot += w[i]; }
                    if (i < n - 1) { s += src[i + 1] * w[i]; tot += w[i]; }
                    f[i] = s / tot;
                }
            }
            target = f;
        }
        for (int i = 1; i < n - 1; ++i) {
            const float fa = ev.param(i);
            const float u = fa + (target[i] - fa) * std::clamp(factor, 0.0f, 1.0f);
            ev.at(u * total, &c.pos[(a + i) * 3]);
        }
    }
}
static void redistribute(const Tool& t, Curves& c, const Surface&, bool) { redistributeImpl(c, t.get("Factor"), t.getb("Feature Awareness")); }

// Trim Hair Curves: each curve given a new length - Length (Replace Length) or its own, times Length Factor, plus a
// random part of Random Offset - blended in by Mask, around the Pin at Parameter point. Without Scale Uniform the curve
// is resampled over its new span (the ends carried on along their direction when it grows) and its points evened out;
// with it, the whole curve is scaled about the pinned point.
static void trim(const Tool& t, Curves& c, const Surface&, bool) {
    const bool uniform = t.getb("Scale Uniform"), replace = t.getb("Replace Length");
    const float lf = t.get("Length Factor"), lenIn = t.get("Length"), mask = std::clamp(t.get("Mask"), 0.0f, 1.0f);
    const float rnd = t.get("Random Offset"), pin = t.get("Pin at Parameter");
    const int hseed = randomInt(t.geti("Seed"), 14699, -1073741823, 1073741823);
    const std::vector<float> before = c.pos; Evaluated ev;
    for (int ci = 0; ci < c.curveCount(); ++ci) {
        const int a = c.first(ci), n = c.count(ci);
        const float* P = &before[a * 3];
        ev.build(P, n);
        const float cur = ev.total;
        const float want = std::max((replace ? lenIn : cur) * lf + randomFloat(a, hseed, -1, 0) * rnd, 1e-4f);
        const float Ln = cur + (want - cur) * mask;
        if (uniform) {
            float pp[3]; ev.at(std::clamp(pin, 0.0f, 1.0f) * cur, pp);
            const float s = cur > 0 ? Ln / cur : 1;
            for (int i = 0; i < n; ++i) for (int k = 0; k < 3; ++k) c.pos[(a + i) * 3 + k] = pp[k] + (P[i * 3 + k] - pp[k]) * s;
            continue;
        }
        const float start = pin * cur - pin * Ln, end = Ln * (1 - pin) + pin * cur;
        const float* rootDir = ev.tangent(0); const float* tipDir = &ev.T[(size_t)(ev.count() - 1) * 3];
        for (int i = 0; i < n; ++i) {
            const float u = n > 1 ? (float)i / (n - 1) : 0;
            const float len = std::max(start, 0.0f) + (std::min(end, cur) - std::max(start, 0.0f)) * u;
            float* o = &c.pos[(a + i) * 3];
            ev.at(len, o);
            if (i == 0 && start < 0) for (int k = 0; k < 3; ++k) o[k] += rootDir[k] * start;               // the root carried back
            if (i == n - 1 && end > cur) for (int k = 0; k < 3; ++k) o[k] += tipDir[k] * (end - cur);     // the tip carried on
        }
    }
    if (!uniform) redistributeImpl(c, 1.0f, false);
}

// ---------- tests against Blender's own results ----------
static bool readJson(const std::string& path, Json::Value& v) {
    v = Json::Value();
    std::ifstream in(path, std::ios::binary); if (!in) return false;
    std::stringstream ss; ss << in.rdbuf(); const std::string t = ss.str(); size_t i = 0;
    return Json::parse(t, i, v);
}
int selfTest(const std::string& folder) {
    int fails = 0, checks = 0;
    auto report = [&](const std::string& what, double err, double tol) { ++checks; const bool ok = err <= tol; if (!ok) ++fails; std::printf("%s  %-48s max error %.2g\n", ok ? "PASS" : "FAIL", what.c_str(), err); };
    Json::Value v;
    if (readJson(folder + "/shape_range.json", v) && v.type == Json::Value::ARR) {
        for (const auto& r : v.a) {
            const float shape = (float)r.num("shape"), base = (float)r.num("base"); double err = 0;
            const Json::Value* vals = r.get("v"); if (!vals) continue;
            for (size_t i = 0; i < vals->a.size(); ++i) err = std::max(err, std::fabs(vals->a[i].n - shapeRange(i / 100.0f, 0, 1, shape, base)));
            char nm[64]; std::snprintf(nm, sizeof nm, "shape_range shape %.2f base %.0f", shape, base); report(nm, err, 1e-4);
        }
    } else { std::printf("FAIL  %s/shape_range.json missing\n", folder.c_str()); ++fails; }
    const char* tools[][2] = {{"frizz.json", "Frizz Hair Curves"}, {"smooth.json", "Smooth Hair Curves"}, {"rotate.json", "Rotate Hair Curves"}, {"trim.json", "Trim Hair Curves"}, {"redistribute.json", "Redistribute Curve Points"}, {"noise.json", "Hair Curves Noise"}};
    for (auto& tl : tools) {
        if (!readJson(folder + "/" + tl[0], v) || v.type != Json::Value::OBJ) { std::printf("FAIL  %s missing\n", tl[0]); ++fails; continue; }
        Curves probe;
        for (const auto& curve : v.get("probe")->a) {
            const int ci = probe.addCurve((int)curve.a.size());
            for (size_t i = 0; i < curve.a.size(); ++i) for (int k = 0; k < 3; ++k) probe.pos[(probe.first(ci) + i) * 3 + k] = (float)curve.a[i].a[k].n;
        }
        for (const auto& res : v.get("results")->a) {
            Tool t = makeTool(tl[1]);
            for (const auto& kv : res.get("inputs")->o) {
                if (kv.second.type == Json::Value::ARR) { float v3[3] = {0, 0, 0}; for (size_t k = 0; k < 3 && k < kv.second.a.size(); ++k) v3[k] = (float)kv.second.a[k].n; t.setv(kv.first.c_str(), v3); continue; }
                const float x = kv.second.type == Json::Value::BOOL ? (kv.second.b ? 1.0f : 0.0f) : (float)kv.second.n;
                t.set(kv.first.c_str(), x);
            }
            Groom g; g.base = probe; g.stack.push_back(t);
            evaluate(g, Surface(), false);
            const Json::Value* pos = res.get("pos"); double err = 0;
            if ((int)pos->a.size() != g.result.pointCount()) err = 1e9;
            else for (size_t i = 0; i < pos->a.size(); ++i) for (int k = 0; k < 3; ++k) err = std::max(err, std::fabs(pos->a[i].a[k].n - g.result.pos[i * 3 + k]));
            report(std::string(tl[1]) + ": " + res.str("name"), err, 1e-4);
            if (err > 1e-4 && err < 1e8) {   // which curves, and where
                for (int ci = 0; ci < g.result.curveCount(); ++ci) {
                    double e = 0; int worst = 0;
                    for (int i = g.result.first(ci); i < g.result.first(ci) + g.result.count(ci); ++i)
                        for (int k = 0; k < 3; ++k) { const double d = std::fabs(pos->a[i].a[k].n - g.result.pos[i * 3 + k]); if (d > e) { e = d; worst = i - g.result.first(ci); } }
                    if (e > 1e-4) std::printf("      curve %d: max error %.3g at point %d of %d\n", ci, e, worst, g.result.count(ci));
                }
            }
        }
    }
    std::printf("%d checks, %d failed\n", checks, fails);
    return fails;
}

using ToolFn = void (*)(const Tool&, Curves&, const Surface&, bool);
static ToolFn toolFn(const std::string& type) {
    if (type == "Generate Hair Curves") return generate;
    if (type == "Frizz Hair Curves") return frizz;
    if (type == "Hair Curves Noise") return curvesNoise;
    if (type == "Smooth Hair Curves") return smooth;
    if (type == "Rotate Hair Curves") return rotateCurves;
    if (type == "Trim Hair Curves") return trim;
    if (type == "Redistribute Curve Points") return redistribute;
    if (type == "Straighten Hair Curves") return straighten;
    return nullptr;   // not ported yet: passes the hair through unchanged
}

bool ported(const std::string& type) { return toolFn(type) != nullptr; }

void evaluate(Groom& g, const Surface& s, bool viewport) {
    g.result = g.base;
    for (const auto& t : g.stack) {
        if (!t.on) continue;
        if (ToolFn f = toolFn(t.type)) f(t, g.result, s, viewport);
    }
}

void surfaceFromBody(const Human::Body& b, int growOn, Surface& s) {
    const std::vector<float>& P = b.shown.empty() ? b.pos : b.shown;
    const int nv = (int)P.size() / 3;
    s.pos = P;
    s.tri = b.tri;
    // per vertex normals (area weighted) and uvs (a vertex's first corner)
    s.nrm.assign(P.size(), 0.0f);
    for (size_t t = 0; t + 2 < s.tri.size(); t += 3) {
        const float* a = &P[s.tri[t] * 3]; const float* bb = &P[s.tri[t + 1] * 3]; const float* c = &P[s.tri[t + 2] * 3];
        const float e1[3] = {bb[0] - a[0], bb[1] - a[1], bb[2] - a[2]}, e2[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
        const float n[3] = {e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0]};
        for (int k = 0; k < 3; ++k) for (int d = 0; d < 3; ++d) s.nrm[s.tri[t + k] * 3 + d] += n[d];
    }
    for (int v = 0; v < nv; ++v) {
        float* n = &s.nrm[v * 3]; const float l = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
        if (l > 0) for (int d = 0; d < 3; ++d) n[d] /= l;
    }
    s.uv.assign((size_t)nv * 2, 0.0f);
    if (b.triUv.size() == b.tri.size() && !b.uv.empty()) {
        std::vector<char> done(nv, 0);
        for (size_t i = 0; i < b.tri.size(); ++i) {
            const int v = b.tri[i], u = b.triUv[i];
            if (done[v] || u < 0 || u * 2 + 1 >= (int)b.uv.size()) continue;
            s.uv[v * 2] = b.uv[u * 2]; s.uv[v * 2 + 1] = b.uv[u * 2 + 1]; done[v] = 1;
        }
    }
    s.mask.clear();
    if (growOn == 2) return;
    s.mask.assign(nv, 0.0f);
    if (growOn == 1) {   // the body vertices the worn MakeHuman hairstyle is fitted to
        for (const auto& p : b.proxies) {
            if (p.kind != "hair") continue;
            for (size_t i = 0; i < p.ref.size(); ++i) if (p.ref[i] >= 0 && p.ref[i] < nv && p.w[i] > 0.05f) s.mask[p.ref[i]] = 1;
        }
        return;
    }
    // the scalp, from MakeHuman's joint markers: above the brows at the front, above the ears at the sides and back,
    // within the head; a soft edge (the hairline) so the density thins out rather than stops
    auto centre = [&](const char* name, float out[3]) {
        out[0] = out[1] = out[2] = 0;
        auto it = b.joints.find(name);
        if (it == b.joints.end() || it->second.empty()) return false;
        for (int v : it->second) for (int d = 0; d < 3; ++d) out[d] += P[v * 3 + d];
        for (int d = 0; d < 3; ++d) out[d] /= (float)it->second.size();
        return true;
    };
    float head[3], eyeL[3], eyeR[3], neck[3];
    if (!centre("joint-head", head) || !centre("joint-l-eye", eyeL) || !centre("joint-r-eye", eyeR) || !centre("joint-neck", neck)) return;
    float top = -1e30f;
    float eyeY = (eyeL[1] + eyeR[1]) / 2, eyeZ = (eyeL[2] + eyeR[2]) / 2;
    const float halfWidth = std::fabs(eyeL[0] - eyeR[0]);   // about half the head's width
    for (int v = 0; v < nv; ++v) {
        const float* q = &P[v * 3];
        if (q[1] > neck[1] && std::fabs(q[0] - head[0]) < halfWidth * 2.2f && std::fabs(q[2] - head[2]) < halfWidth * 2.6f) top = std::max(top, q[1]);
    }
    if (top < eyeY) return;
    const float H = top - eyeY;
    auto smooth = [](float e0, float e1, float x) { const float t = std::clamp((x - e0) / (e1 - e0), 0.0f, 1.0f); return t * t * (3 - 2 * t); };
    for (int v = 0; v < nv; ++v) {
        const float* q = &P[v * 3]; const float* n = &s.nrm[v * 3];
        if (q[1] < neck[1] || std::fabs(q[0] - head[0]) > halfWidth * 2.2f || q[2] - head[2] > halfWidth * 2.6f || head[2] - q[2] > halfWidth * 2.6f) continue;
        // the hairline: high on the forehead, down to the ears at the sides, lower still at the nape
        const float front = std::clamp((q[2] - head[2]) / std::max(eyeZ - head[2], 1e-3f), 0.0f, 1.0f);   // 1 at the face
        const float side = std::clamp(std::fabs(q[0] - head[0]) / halfWidth - 0.6f, 0.0f, 1.0f);
        const float line = eyeY + H * (0.42f * front * (1 - side) - 0.12f * side - 0.38f * (1 - front) * (q[2] < head[2] ? 1.0f : 0.0f));
        float m = smooth(line, line + H * 0.08f, q[1]);
        if (n[2] > 0.6f && q[1] < eyeY + H * 0.4f) m = 0;   // never the face
        s.mask[v] = m;
    }
}

void followSurface(Curves& c, const Surface& before, const Surface& after) {
    for (int ci = 0; ci < c.curveCount(); ++ci) {
        const int t = c.rootTri[ci];
        if (t < 0 || t * 3 + 2 >= (int)after.tri.size()) continue;
        float P0[3], N0[3], P1[3], N1[3];
        before.at(t, &c.rootBary[ci * 3], P0, N0, nullptr);
        after.at(t, &c.rootBary[ci * 3], P1, N1, nullptr);
        // move the curve with its root, turned by the change in the surface normal (Rodrigues)
        float ax[3] = {N0[1] * N1[2] - N0[2] * N1[1], N0[2] * N1[0] - N0[0] * N1[2], N0[0] * N1[1] - N0[1] * N1[0]};
        const float sn = std::sqrt(ax[0] * ax[0] + ax[1] * ax[1] + ax[2] * ax[2]);
        const float cs = N0[0] * N1[0] + N0[1] * N1[1] + N0[2] * N1[2];
        if (sn > 1e-6f) for (float& a : ax) a /= sn;
        for (int i = c.first(ci); i < c.first(ci) + c.count(ci); ++i) {
            float v[3] = {c.pos[i * 3] - P0[0], c.pos[i * 3 + 1] - P0[1], c.pos[i * 3 + 2] - P0[2]};
            if (sn > 1e-6f) {
                const float d = ax[0] * v[0] + ax[1] * v[1] + ax[2] * v[2];
                const float x[3] = {ax[1] * v[2] - ax[2] * v[1], ax[2] * v[0] - ax[0] * v[2], ax[0] * v[1] - ax[1] * v[0]};
                for (int k = 0; k < 3; ++k) v[k] = v[k] * cs + x[k] * sn + ax[k] * d * (1 - cs);
            }
            for (int k = 0; k < 3; ++k) c.pos[i * 3 + k] = P1[k] + v[k];
        }
    }
}

}  // namespace Hair
