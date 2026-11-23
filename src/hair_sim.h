// Pygmalion - strand hair physics: AMD TressFX's simulation (TressFXSimulation.hlsl, MIT, (c) Advanced Micro Devices)
// run on the CPU over all cores, so it works on any OpenGL 3.3 PC; and TressFX's .tfx / .tfxbone files.
#pragma once
#include "hair.h"

#include <string>
#include <vector>

namespace Human { struct Body; }

namespace HairSim {

// TressFX's simulation settings (its g_Shape, g_GravTimeTip, g_SimInts, g_VSP, g_Wind), in Pygmalion's units
// (decimetres): gravity 98.2 is the Earth's.
struct Settings {
    float damping = 0.035f;            // velocity damping (TressFX: exp(-damping * dt * 60))
    float localStiffness = 0.75f;      // keeps each segment's groomed bend
    float globalStiffness = 0.05f;     // pulls toward the groomed shape...
    float globalRange = 0.3f;          // ...over this part of the strand from the root
    float gravity = 98.2f;             // down (negative y)
    float vsp = 0.75f;                 // velocity shock propagation: how much root movement carries down the strand
    float vspAccelThreshold = 1.2f;    // above this jerk, all of it does
    int lengthIterations = 3;
    int localIterations = 2;
    float wind[3] = {0, 0, 0};         // direction times strength
    float windStrength = 0;
    float clampDelta = 2.0f;           // largest move in one step (decimetres)
    bool collide = true;               // the head, neck and shoulders push hair out
    int substeps = 2;                  // steps a frame (at 60 frames a second)
};

struct Capsule { float p0[3], r0, p1[3], r1; };

// The running simulation of a head of hair.
class Simulation {
public:
    // Starts from the groomed curves (their rest shape) on the surface they grow on.
    void reset(const Hair::Curves& groomed, const Hair::Surface& s);
    bool empty() const { return pos.empty(); }
    // One frame: the roots follow the surface as it is now (shaped, posed, animated), then the strands move.
    void step(const Hair::Surface& s, const Settings& set, float dt);
    // Lets the hair fall and come to rest (frames at 60 a second, with the surface still).
    void settle(const Hair::Surface& s, const Settings& set, int frames = 180);
    const std::vector<float>& positions() const { return pos; }
    const std::vector<int>& offsets() const { return offs; }
    std::vector<Capsule> capsules;     // set from the body (bodyCapsules)

private:
    std::vector<int> offs;
    std::vector<float> rest, pos, prev, prevprev, restLen;
    std::vector<int> tri; std::vector<float> bary;      // each strand's root on the surface
    std::vector<float> restFrame;                        // per strand: the root triangle's frame at rest (9 floats)
    std::vector<float> invMass;                          // per point: 0 for the first two (TressFX's immovable)
};

// Collision capsules for a body: the head (from its joint markers), the neck and the shoulders.
std::vector<Capsule> bodyCapsules(const Human::Body& b);

// TressFX's files. Every strand is resampled to verticesPerStrand points (4..64, a power of two) along its
// Catmull-Rom curve; scale multiplies positions (Pygmalion's decimetres: 10 for centimetres, 0.1 for metres);
// zUp swaps y and z (Unreal). The .tfxbone file names the skeleton's bones and gives each strand its root's four
// strongest bone weights (pass empty to skip it).
bool exportTfx(const Hair::Curves& c, const std::string& path, int verticesPerStrand, float scale, bool zUp, std::string& err);
bool exportTfxBone(const Hair::Curves& c, const std::string& path, const std::vector<std::string>& boneNames,
                   const std::vector<std::vector<std::pair<int, float>>>& vertexWeights, const Hair::Surface& s, std::string& err);

}  // namespace HairSim
