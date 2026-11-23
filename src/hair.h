// Pygmalion - strand hair: Blender's hair system ported to C++ (Blender 4.0's procedural hair node assets and its
// curves sculpt tools, GPL-2.0-or-later, here under Pygmalion's AGPL-3.0), simulated and drawn the TressFX way
// (AMD TressFX, MIT) and exported as TressFX .tfx files.
//
// A head of hair is Blender's hair object: curves (strands) rooted on the body's surface, then a stack of tools
// (Blender's hair node groups: Generate, Interpolate, Clump, Curl, Frizz...) applied in order, each with its sliders.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace Human { struct Body; }

namespace Hair {

// The curves: Blender's CurvesGeometry for hair. Strand c's points are [offsets[c], offsets[c + 1]).
struct Curves {
    std::vector<int> offsets{0};
    std::vector<float> pos;               // 3 floats a point
    std::vector<float> radius;            // per point
    // per curve: where it's rooted on the surface (Blender's surface_uv_coordinate, here also the triangle and its
    // barycentric weights, so the root follows the body as it's shaped and posed)
    std::vector<float> surfaceUv;         // 2 floats a curve
    std::vector<int> rootTri;             // the body triangle (-1: not attached)
    std::vector<float> rootBary;          // 3 floats a curve
    std::vector<int> id;                  // per curve: a stable random id (Blender's "id" attribute), for seeds
    std::vector<int> guideIndex;          // per curve: its guide (Create Guide Index Map; -1 none)
    std::vector<char> guideMask;          // per curve: 1 if it is a guide

    int curveCount() const { return (int)offsets.size() - 1; }
    int pointCount() const { return (int)pos.size() / 3; }
    int first(int c) const { return offsets[c]; }
    int count(int c) const { return offsets[c + 1] - offsets[c]; }
    float* p(int i) { return &pos[i * 3]; }
    const float* p(int i) const { return &pos[i * 3]; }
    void clear();
    // a new curve of n points (its points, radius and per-curve data appended, zeroed); returns its index
    int addCurve(int n);
};

// The surface hair grows on: triangles in the body's space, with uvs, normals, and a mask (0..1 per vertex: where
// hair may grow, e.g. the scalp).
struct Surface {
    std::vector<float> pos, nrm;          // 3 floats a vertex
    std::vector<float> uv;                // 2 floats a vertex
    std::vector<int> tri;                 // 3 vertices a triangle
    std::vector<float> mask;              // per vertex, 0..1 (empty: everywhere)
    // a point on triangle t at barycentric b
    void at(int t, const float b[3], float outPos[3], float outNrm[3], float outUv[2]) const;
};

// A slider of a tool: Blender's node group input, with its name, range and default.
struct Param {
    std::string name;                     // as Blender names it ("Hair Length")
    enum Kind { FLOAT, INT, BOOL, VECTOR } kind = FLOAT;
    float v[3] = {0, 0, 0};               // its value (INT/BOOL in v[0])
    float min = 0, max = 1, softMax = 1;  // the slider's range (softMax: where the slider ends, typing goes further)
    std::string tip;                      // Blender's description
};

// One tool in the stack: one of Blender's hair node groups.
struct Tool {
    std::string type;                     // "Generate Hair Curves", "Curl Hair Curves"...
    bool on = true;
    std::vector<Param> params;
    float get(const char* name) const;
    int geti(const char* name) const { return (int)get(name); }
    bool getb(const char* name) const { return get(name) != 0; }
    const float* getv(const char* name) const;
    void set(const char* name, float v);
};

// The tools there are, in Blender's order, and a new one with Blender's defaults.
const std::vector<std::string>& toolTypes();
Tool makeTool(const std::string& type);
bool ported(const std::string& type);         // false: not ported yet (the tool passes the hair through)

// A head of hair: the hand-made curves (Blender's sculpted guides; may be empty) and the tool stack.
struct Groom {
    std::string name = "Hair";
    Curves base;                          // what the sculpt brushes edit (Blender's original curves)
    std::vector<Tool> stack;              // applied to base, top to bottom
    Curves result;                        // base after the stack: what's drawn, simulated and exported
    float rootColor[3] = {0.08f, 0.05f, 0.035f}, tipColor[3] = {0.25f, 0.17f, 0.11f};
    float thickness = 1;                  // multiplies every radius (Blender's Set Hair Curve Profile does the shape)
    bool shown = true;
    // where it grows on the body: 0 the scalp (worked out from the head's joint markers), 1 where the MakeHuman
    // hairstyle being worn is fitted, 2 everywhere
    int growOn = 0;
    float shift = 0.1f, gloss = 80, shine = 0.4f;   // the look: highlight shift along the strand, sharpness, strength
};

// The body as a surface to grow on (posed if posed), with the mask for growOn.
void surfaceFromBody(const Human::Body& b, int growOn, Surface& s);

// Runs the stack on g.base over the surface into g.result. viewport: Blender's Is Viewport (the Viewport Amount
// sliders thin the hair while editing).
void evaluate(Groom& g, const Surface& s, bool viewport = true);

// Keeps attached roots on the surface as it changes (shaped, posed): moves each curve rigidly with its root.
void followSurface(Curves& c, const Surface& before, const Surface& after);

// Blender's shape_range node group: a 0..1 falloff along a curve (Shape 0: constant 1, 0.5: linear, toward -1/1:
// pushed to the root or the tip), Base its steepness.
float shapeRange(float value, float min, float max, float shape, float base);
// Blender's curve frames on a poly curve (curve_poly.cc): tangents bisecting the segments, minimum-twist normals.
void curveFrames(const float* pos, int n, std::vector<float>& tangents, std::vector<float>& normals);
// Blender's Random Value node: a float, vector or int from an id and a seed (noise::hash_to_float...).
float randomFloat(int id, int seed, float min, float max);
void randomVector(int id, int seed, const float min[3], const float max[3], float out[3]);
int randomInt(int id, int seed, int min, int max);
// Restore Curve Segment Length: each segment's length mixed toward the reference's by factor, directions kept,
// from the root out (pin: the point at that parameter along the curve stays where the reference had it).
void restoreSegmentLength(Curves& c, const std::vector<float>& reference, float factor, float pin = 0, const std::vector<char>* selection = nullptr);
// Runs the ported tools against Blender's own results (tests/hair/*.json); prints each check, returns failures.
int selfTest(const std::string& folder);

// Blender's random hash (noise::hash, Bob Jenkins' lookup3), so seeds behave alike.
uint32_t hash(uint32_t kx);
uint32_t hash(uint32_t kx, uint32_t ky);
uint32_t hash(uint32_t kx, uint32_t ky, uint32_t kz);
float hashFloat(uint32_t kx, uint32_t ky = 0, uint32_t kz = 0);   // 0..1, the 3D hash
float hashFloat2(uint32_t kx, uint32_t ky);                         // 0..1, the 2D hash

}  // namespace Hair
