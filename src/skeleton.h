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
    std::vector<std::string> reference;      // the default skeleton's bones it stands for (its weights come from them)
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
    bool hasWeights = false;
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
// target: the skeleton other skeletons' BVHs are turned onto (MakeHuman's old animations, CMU or Mixamo motion capture)
bool loadBvh(const std::string& path, std::vector<Pose>& frames, std::string& err, float* frameTime = nullptr, const Skeleton* target = nullptr);
// a skeleton without its own weights takes them from the default one, through each bone's reference bones
void inheritWeights(Skeleton& s, const Skeleton& reference);
// a skeleton posed like the default one: each bone turns with its first reference bone (bones without one follow
// their parent), so a game-engine or CMU skeleton takes the default skeleton's pose
void retarget(Skeleton& s, const Skeleton& posedDefault);
bool usesReferences(const Skeleton& s);
// the posed bones as line segments (head, tail: 6 floats each), for drawing
void boneLines(const Skeleton& s, std::vector<float>& lines);
// a .bvh of the skeleton (Y up, as MakeHuman exports it): its rest pose, then a frame per pose given (none: the rest)
bool saveBvh(const Skeleton& s, const std::vector<Pose>& frames, const std::string& path, float scale, float frameTime, std::string& err,
             const Skeleton* posedThrough = nullptr);   // frames posed on this one (the default) and retargeted to s
// the global pose matrix's rotation per bone, from a pose applied to s (for exports of the posed skeleton)
Mat4 identity();
Mat4 multiply(const Mat4& a, const Mat4& b);
Mat4 inverseRigid(const Mat4& m);
// poses mixed by weight (an expression: face pose units each at some strength)
Pose blend(const std::vector<std::pair<const Pose*, float>>& parts);

}
