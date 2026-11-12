// Pygmalion - the skeleton and posing, ported from MakeHuman (shared/skeleton.py, shared/bvh.py; AGPL-3.0): the
// .mhskel's bones placed on the shaped body (joints are vertex groups, each bone's roll from a plane through three
// joints), a pose from a .bvh (MakeHuman's own poses are BVH), and linear blend skinning with the .mhw weights.
#pragma once
#include <array>
#include <map>
#include <string>
#include <vector>

namespace Skel {

using Mat4 = std::array<float, 16>;   // column major, as MakeHuman's numpy matrices transposed

struct Bone {
    std::string name, headJoint, tailJoint, plane;
    int parent = -1;
    float head[3] = {0, 0, 0}, tail[3] = {0, 0, 0};
    Mat4 restGlobal{}, restRelative{}, pose{}, poseGlobal{}, poseVerts{};
};

struct Skeleton {
    std::string name, description;
    std::vector<Bone> bones;                          // parents before children
    std::map<std::string, int> byName;
    std::map<std::string, std::vector<int>> joints;   // joint name -> vertices
    std::map<std::string, std::array<std::string, 3>> planes;
    std::vector<std::vector<std::pair<int, float>>> weights;   // per bone: (vertex, weight)
    bool empty() const { return bones.empty(); }
};

// a pose: a rotation (and for the root a translation) per bone, in MakeHuman's global axes
struct Pose {
    std::string name;
    std::map<std::string, Mat4> bones;
    bool empty() const { return bones.empty(); }
};

bool load(Skeleton& s, const std::string& mhskel, std::string& err);   // and its weights file beside it
void build(Skeleton& s, const std::vector<float>& restPos);           // on the shaped body (not posed)
void setPose(Skeleton& s, const Pose* p);                              // nullptr: the rest pose
void skin(const Skeleton& s, const std::vector<float>& rest, std::vector<float>& out);

// a .bvh: its frames as poses (MakeHuman's poses use frame 0; face-poseunits.bvh holds one unit per frame)
bool loadBvh(const std::string& path, std::vector<Pose>& frames, std::string& err);
// poses mixed by weight (an expression: face pose units each at some strength)
Pose blend(const std::vector<std::pair<const Pose*, float>>& parts);

}
