// Pygmalion - strand hair: the curves, the surface they grow on, the tool stack and its evaluation.
// The tools are Blender 4.0's procedural hair node groups (GPL-2.0-or-later), ported node graph by node graph; each
// one's sliders come from hair_tools.inc (generated from Blender's own asset file).
#include "hair.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <unordered_map>

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

using ToolFn = void (*)(const Tool&, Curves&, const Surface&, bool);
static ToolFn toolFn(const std::string& type) {
    if (type == "Generate Hair Curves") return generate;
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
