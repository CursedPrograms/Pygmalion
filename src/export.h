// Pygmalion - MakeHuman's exports with the skeleton: Collada (.dae), Filmbox (.fbx, binary 7.4), Ogre3D (.mesh.xml,
// .skeleton.xml, .material), Biovision (.bvh) and the UV map (.png). Ported in spirit from MakeHuman's plugins
// 9_export_collada, 9_export_fbx, 9_export_ogre, 9_export_bvh and 9_export_uvmap (AGPL-3.0).
//
// What's written: the person in the rest pose with the skeleton bound to it (inverse bind matrices from the rest),
// and the skeleton's joints in the current pose, so any 3D app shows them posed and can pose them again.
#pragma once
#include "human.h"
#include "skeleton.h"

#include <string>
#include <vector>

namespace Export {

struct Part {                                   // one mesh with one material: the body, or a proxy
    std::string name, texture;                  // texture: a PNG beside the export ("" none)
    std::vector<float> pos, nrm, uv;            // per vertex (vertices split where their uvs differ)
    std::vector<int> tri;                       // 3 a triangle
    std::vector<int> faces, faceSize;           // the same surface as MakeHuman's faces: quads where its files have them
    std::vector<std::vector<std::pair<int, float>>> weights;   // per vertex: (bone, weight), at most 4, adding to 1
};

struct Options {
    float scale = 1;                            // MakeHuman's units are decimetres: 0.1 makes metres
    float cmPerUnit = 10;                       // what one unit is, in cm (for FBX's UnitScaleFactor)
    bool feetOnGround = true;                   // the lowest point at 0
    bool posed = true;                          // the joints in the current pose (else the rest pose)
    // MakeHuman's exporter options
    int orient = 0;                             // Collada's orientation: Y up face Z, Y up face X, Z up face -Y, Z up face X
    int boneAxis = 0;                           // Collada's bone orientation: along local Y, along local X, local = global
    bool binary = true;                         // FBX and STL: binary (else ASCII)
    bool normals = true;                        // OBJ: the normals written
    const std::vector<Skel::Pose>* poseUnits = nullptr;   // Collada's "Facial pose-units": the face's units as an animation
};

// the person as parts, for the skeleton s (built on b.pos; s's pose is the one written when posed)
// posedMesh: the vertices as posed now (Collada and Ogre bind the skeleton in the pose; FBX binds the rest)
// helpers: MakeHuman's "Helper geometry" - every face of the base mesh (helpers, joint markers, under clothes) kept
std::vector<Part> parts(const Human::Body& b, const Skel::Skeleton* s, bool posedMesh = false, bool helpers = false);
// STL as MakeHuman writes it (binary or ASCII), the parts in the file's units
bool saveStl(const std::vector<Part>& parts, const std::string& path, const Options& o, std::string& err);

bool saveCollada(const std::vector<Part>& parts, const Skel::Skeleton* s, const std::string& path, const Options& o, std::string& err);
bool saveFbx(const std::vector<Part>& parts, const Skel::Skeleton* s, const std::string& path, const Options& o, std::string& err);
// Wavefront OBJ as MakeHuman writes it: shared vertices, quads, uvs, normals, a .mtl and the textures beside it
bool saveObj(const std::vector<Part>& parts, const std::string& path, const Options& o, std::string& err);
bool saveOgre(const std::vector<Part>& parts, const Skel::Skeleton* s, const std::string& path, const Options& o, std::string& err);
// the body's uv layout as lines on a square picture (size x size)
bool saveUvMap(const Human::Body& b, const std::string& path, int size, std::string& err);
// MakeHuman's Lightmap export (9_export_light, projection.mapLighting): one light at (-10.99, 20, 20) baked into the
// body's uv layout - each vertex's N.L, filled over the faces, the seams grown by a pixel - size x size
bool saveLightmap(const Human::Body& b, const std::string& path, int size, std::string& err);
// MakeHuman's "Lightmap SSS" (mh2opengl.Render): every light baked into the uv layout (projection.mapSceneLighting),
// the ambience mixed in, red blurred by sssR and green by sssG (light scattered under the skin), times the skin's
// texture (if it has one). lights: position xyz then colour rgb, in the body's own space. Out: rgba, w x h.
struct WorldLight { float pos[3], col[3]; };
bool sssTexture(const Human::Body& b, const std::vector<WorldLight>& lights, const float ambience[3], const float diffuse[3], float sssR, float sssG,
                const std::string& diffuseTexture, int size, std::vector<unsigned char>& rgba, int& w, int& h);
// the textures beside the export: copies each part's texture next to path, and returns the parts pointing there
std::vector<Part> withTexturesBeside(const std::vector<Part>& parts, const std::string& path);

}
