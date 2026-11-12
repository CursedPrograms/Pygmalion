// Pygmalion - the human: MakeHuman's base mesh, shaped by its targets through its modifiers. Ported from MakeHuman
// (apps/human.py, apps/humanmodifier.py, lib/targets.py; AGPL-3.0); the mesh and targets are MakeHuman's (CC0).
//
// How a shape is made: every .target file holds offsets for some of the base mesh's vertices. Its file path gives
// it a group ("nose-nose-trans-up") and the macro variables it depends on ("female", "young", "maxweight"...).
// Its weight is the product of those variables' current values and its group's factor (a slider's value), and the
// mesh is the base plus every target's offsets times its weight.
#pragma once
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
    bool macro = false;                  // a macro variable (Gender, Age...) rather than targets of its own
    std::string variable;                // the macro's variable: "Gender", "Age", "African"...
    std::string left, right, center;     // target groups at -1, +1 and 0 (left/center empty: 0..1 only)
    float value = 0, defaultValue = 0;
    float min() const { return macro || left.empty() ? 0.0f : -1.0f; }
    std::string fullName() const { return group + "/" + name; }
};

struct SliderGroup { std::string name; std::vector<int> mods; };
struct Tab { std::string name; float order = 0; std::vector<SliderGroup> groups; };

struct Proxy {                           // something fitted to the body: the eyes
    std::string name;
    std::vector<float> pos, uv;          // own vertices; uv per corner
    std::vector<int> tri;                // corners
    std::vector<int> ref;                // per vertex: 3 body vertices
    std::vector<float> w, off;           // per vertex: 3 weights, 3 offsets
    int scaleRef[3][2] = {{0, 0}, {0, 0}, {0, 0}};
    float scaleDiv[3] = {1, 1, 1};
    std::string texture;                 // its PNG
};

struct Body {
    // the base mesh
    std::vector<float> base, pos;        // 3 floats a vertex: as made, as shaped now
    std::vector<float> uv;               // 2 floats a uv
    std::vector<int> tri, triUv;         // the visible body's triangles (helpers left out), and their uvs
    std::map<std::string, std::vector<int>> joints;   // the joint markers ("joint-l-hand"...): their vertices
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
};

extern const char* VAR_NAMES[];          // the macro value names, in val's order
int varIndex(const std::string& v);

// data: the folder holding 3dobjs/, targets/, modifiers/ and eyes/. progress: 0..1 as it loads (may be null)
bool load(Body& b, const std::string& data, std::string& err, void (*progress)(float, const char*) = nullptr);

void setModifier(Body& b, int m, float value);    // and its mirror, with symmetry on
void setMacro(Body& b, const std::string& variable, float value);
void resetAll(Body& b);
void apply(Body& b);                     // brings pos up to date (only the targets whose weight changed)
void fitProxy(const Body& b, const Proxy& p, std::vector<float>& out);

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
