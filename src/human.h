// Pygmalion - the human: MakeHuman's base mesh, shaped by its targets through its modifiers. Ported from MakeHuman
// (apps/human.py, apps/humanmodifier.py, lib/targets.py; AGPL-3.0); the mesh and targets are MakeHuman's (CC0).
//
// How a shape is made: every .target file holds offsets for some of the base mesh's vertices. Its file path gives
// it a group ("nose-nose-trans-up") and the macro variables it depends on ("female", "young", "maxweight"...).
// Its weight is the product of those variables' current values and its group's factor (a slider's value), and the
// mesh is the base plus every target's offsets times its weight.
#pragma once
#include "material.h"

#include <map>
#include <string>
#include <vector>

namespace Human {

struct Target {
    std::string path;                    // relative to data/targets
    std::string group;                   // "nose-nose-trans-up"
    std::vector<int> vars;               // macro values it depends on (indexes into Body::val)
    std::vector<int> idx;                // vertices
    std::vector<float> d;                // their offsets, 3 floats each
    float weight = 0;                    // as applied to the mesh now
};

struct Modifier {
    std::string group, name;             // "nose", "nose-trans-down|up": shown as group/name, MakeHuman's own
    std::string label, description;      // for the window (MakeHuman's own label rule, or the sliders file's)
    std::string image;                   // its picture (targets/*/images), "" if none
    std::string cam;                     // the view MakeHuman turns to while it's moved (frontView, leftView, faceCamera...)
    std::string enabledCondition;        // MakeHuman's: "hasGenitals" (only with a topology that has them)
    bool macro = false;                  // a macro variable (Gender, Age...) rather than targets of its own
    std::string variable;                // the macro's variable: "Gender", "Age", "African"...
    std::string left, right, center;     // target groups at -1, +1 and 0 (left/center empty: 0..1 only)
    float value = 0, defaultValue = 0;
    float min() const { return macro || left.empty() ? 0.0f : -1.0f; }
    std::string fullName() const { return group + "/" + name; }
};

struct SliderGroup { std::string name; std::vector<int> mods; };
struct Tab { std::string name; float order = 0; std::vector<SliderGroup> groups; };

struct Proxy {                           // something fitted to the body (MakeHuman's .mhclo): eyes, clothes, hair...
    std::string name, uuid, file, kind;  // kind: eyes, clothes, hair, eyebrows, eyelashes, teeth, tongue, proxymeshes
    std::vector<float> pos, uv;          // own vertices; uv per corner
    std::vector<int> tri;                // corners
    std::vector<int> ref;                // per vertex: 3 body vertices
    std::vector<float> w, off;           // per vertex: 3 weights, 3 offsets
    int scaleRef[3][2] = {{0, 0}, {0, 0}, {0, 0}};
    float scaleDiv[3] = {1, 1, 1};
    std::string material, texture;       // its .mhmat and that's diffuse PNG
    bool transparent = false;            // its texture has see-through parts (hair, eyebrows, lashes)
    int zDepth = 50;                     // drawing order (MakeHuman's z_depth: lower first)
    std::vector<int> deleteVerts;        // the body's vertices it covers (hidden under it)
    float tint[3] = {1, 1, 1};           // Material Editor: its colour multiplied in
    std::vector<std::string> tags;       // its "tag" lines (a topology tagged "genitals" has them)
    Mat::Material mat;                   // its whole material (MakeHuman's Material Editor), read from material
    std::string author = "MakeHuman Team", license = "CC0", homepage, copyright;   // "# author: ..." comments (MakeHuman's LicenseInfo)
    int vertsPerFace = 4;                // its obj's faces (MakeHuman's vertsPerFaceForExport)
    std::map<std::string, std::string> specialPose;   // "special_pose foot high_heels": poses it wants on the person
};
bool loadProxyFile(const std::string& mhclo, Proxy& p, std::string& err);
// a material: its diffuse texture (absolute path) and whether it's see-through
bool loadMaterial(const std::string& mhmat, std::string& texture, bool& transparent);

struct Body {
    // the base mesh
    std::vector<float> base, pos;        // 3 floats a vertex: as made, as shaped now
    std::vector<float> shown;            // as posed (empty: the same as pos)
    std::vector<float> uv;               // 2 floats a uv
    std::vector<int> tri, triUv;         // the visible body's triangles (helpers left out), and their uvs
    std::map<std::string, std::vector<int>> joints;   // the joint markers ("joint-l-hand"...): their vertices
    // every face of base.obj as MakeHuman holds it (fvert/fuvs: 4 a face, a triangle repeats its last corner), and
    // its face groups as runs of faces [start, stop) - for the socket (Blender's MPFB reads the whole seed mesh)
    std::vector<int> faces4, faces4Uv;
    std::vector<float> uvOwn; std::vector<int> triUvOwn, faces4UvOwn;   // the mesh's own uvs, while a uv map is used
    std::string uvMapIn;                                                // the uv map in use ("" the mesh's own)
    std::vector<std::pair<std::string, std::vector<std::pair<int, int>>>> faceGroups;
    // targets and modifiers
    std::vector<Target> targets;
    std::map<std::string, std::vector<int>> groupTargets;
    std::vector<Modifier> mods;
    std::map<std::string, int> modByName;
    std::vector<Tab> tabs;
    std::vector<Proxy> proxies;
    // the macro values: maleVal, femaleVal, babyVal... (see VAR_NAMES)
    std::vector<float> val;
    // macro settings, 0..1 (MakeHuman's own scale)
    float gender = 0.5f, age = 0.5f, muscle = 0.5f, weight = 0.5f, height = 0.5f, proportions = 0.5f;
    float african = 1 / 3.0f, asian = 1 / 3.0f, caucasian = 1 / 3.0f, breastSize = 0.5f, breastFirmness = 0.5f;
    bool symmetry = true;                // a left slider moves the right one too
    // what it wears (MakeHuman's Geometries and Materials): proxies above, its skin here
    std::vector<std::string> roots;      // where assets are looked for (data, MakeHuman's asset pack)
    std::string skinMaterial, skinTexture;   // "" : the litsphere skin by ethnicity
    bool hideUnderClothes = true;        // clothes hide the body's faces under them
    float skinTint[3] = {1, 1, 1};       // Material Editor: the skin's colour multiplied in
    Mat::Material skinMat;               // the skin's whole material (skins/default.mhmat when none is chosen)
    // the model's metadata (Files > Save): its name, uuid and tags, the camera and Smooth, as .mhm files keep them
    std::string name, uuid;
    std::vector<std::string> tags;
    float camera[6] = {0, 0, 0, 0, 0, 1};   // MakeHuman's: rotation x, y (degrees), translation x, y, z, zoom
    bool subdivide = false, hasCamera = false;
};
// a file's tags (MakeHuman's "tag" lines, a model's "tags a;b", a pose's .meta) and its "name" line, for the choosers
const std::vector<std::string>& fileTags(const std::string& path);
std::string fileTitle(const std::string& path);
// MakeHuman's real weight: the body's surface area (Mosteller) turned into kilograms for its height
float weightKg(const Body& b);
// 1_convert_model_tags: old "tags a,,b c" lines in the models under folder become "tags a b;c", the file kept as .mhm~
int convertModelTags(const std::string& folder, std::vector<std::string>& log);
// where assets are looked for when a file names one (materials' textures, litspheres, shaders): set once the roots are
void setAssetRoots(const std::vector<std::string>& roots);
// a path as a file writes it ("data/litspheres/x.png", "x.png" beside it...) -> absolute, "" if it isn't anywhere
std::string resolveAsset(const std::string& written, const std::string& folder);
// the skin material's uv map (MakeHuman's data/uvs: an OBJ of uvs and faces) on the body, or its own uvs back
bool applyUvMap(Body& b, std::string& err);

extern const char* VAR_NAMES[];          // the macro value names, in val's order
int varIndex(const std::string& v);

// data: the folder holding 3dobjs/, targets/, modifiers/ and eyes/. progress: 0..1 as it loads (may be null)
bool load(Body& b, const std::string& data, std::string& err, void (*progress)(float, const char*) = nullptr);

// MakeHuman's hasGenitals: the topology worn has the tag "genitals" (their convention)
bool hasGenitals(const Body& b);
void setModifier(Body& b, int m, float value);    // and its mirror, with symmetry on
void setMacro(Body& b, const std::string& variable, float value);
void resetAll(Body& b);
void apply(Body& b);                     // brings pos up to date (only the targets whose weight changed)
void fitProxy(const Body& b, const Proxy& p, std::vector<float>& out, bool rest = false);   // to what's shown (posed if posed), or the rest

// the gear: MakeHuman's proxy libraries. kind: clothes (several), the rest one at a time ("proxymeshes": a topology)
bool wear(Body& b, const std::string& file, std::string& err);      // clothes: on, or off if already worn
void takeOff(Body& b, const std::string& kindOrFile);                // every proxy of a kind, or that file
const Proxy* worn(const Body& b, const std::string& kind);           // the first of a kind (null if none)
bool isWorn(const Body& b, const std::string& file);
bool setSkin(Body& b, const std::string& mhmat);                    // "" : the default (litspheres)
bool setProxyMaterial(Body& b, const std::string& kind, const std::string& mhmat);
void defaultGear(Body& b);                                          // MakeHuman's start: eyes, eyebrow001, eyelashes01
std::vector<char> hiddenVerts(const Body& b);                        // body vertices under clothes (or all, with a topology)
std::string relPath(const Body& b, const std::string& abs);         // as MakeHuman writes paths: from the asset root
std::string absPath(const Body& b, const std::string& rel);

// Modelling > Custom: a .target of your own as a 0..1 slider ("custom/<name>"); -1 if it couldn't be read
int addCustomTarget(Body& b, const std::string& targetFile);
// Utilities > Save targets: the shape as it is now (against the base mesh) as a .target file
bool saveTarget(const Body& b, const std::string& path, std::string& err);

float ageYears(const Body& b);
float heightCm(const Body& b);
bool jointPos(const Body& b, const std::string& joint, float out[3]);   // the middle of a joint marker, as shaped now
// a MakeHuman ruler ("measure/measure-waist-circ-decr|incr"...) in cm; -1 if there's no such ruler
float measureCm(const Body& b, const std::string& name);           // measured on the shaped mesh

// MakeHuman's own .mhm files ("modifier macrodetails/Gender 0.500000"), so models go both ways
bool saveMhm(const Body& b, const std::string& path, const std::string& name, std::string& err);
bool loadMhm(Body& b, const std::string& path, std::string& err);

// words to sliders: "a tall, muscular old woman" sets the macros (and some face and body sliders)
std::string describe(Body& b, const std::string& prompt);   // returns what it understood
void randomize(Body& b, float amount, bool macros, bool face, bool body, unsigned seed);

}
