// Pygmalion - make people in 3D with sliders, on your own PC, in C++. Part of Atelier.
// MakeHuman's body, its 1,280 shapes and every one of its sliders (gender, age, muscle, weight, height, proportions,
// ethnicity, then the face, the torso, the arms and legs, measurements and body shapes); words that set the sliders;
// a random person; MakeHuman's own .mhm files; GLB, OBJ and STL out.
//
//   Pygmalion [model.mhm]
//   Pygmalion --capture out.bmp [--describe "words"] [--tab N]      a screenshot, then quit
#include "human.h"
#include "skeleton.h"
#include "model.h"
#include "splash.h"
#include "viewer.h"

#include <SDL.h>
#include <SDL_opengl.h>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl2.h>
#include <stb_image.h>
#include <stb_image_write.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <functional>
#include <map>
#include <mutex>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include "json.h"
#include <random>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#define NOMINMAX
#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "shell32.lib")
#endif

namespace fs = std::filesystem;

#ifndef PYGMALION_VERSION
#define PYGMALION_VERSION "1.0.0"
#endif

static const ImU32 C_BG = IM_COL32(0x1a, 0x1a, 0x1a, 255), C_PANEL = IM_COL32(0x24, 0x24, 0x24, 255),
                   C_LINE = IM_COL32(0x3a, 0x3a, 0x3a, 255), C_TEXT = IM_COL32(0xe6, 0xe6, 0xe6, 255),
                   C_DIM = IM_COL32(0x9a, 0x9a, 0x9a, 255), C_PINK = IM_COL32(0xe0, 0x40, 0x8a, 255),
                   C_DEEP = IM_COL32(0xc2, 0x18, 0x6b, 255), C_WELL = IM_COL32(0x14, 0x14, 0x14, 255);
static ImVec4 col4(ImU32 c) { return ImGui::ColorConvertU32ToFloat4(c); }

static void applyStyle() {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding = 0; s.FrameRounding = 2; s.GrabRounding = 2; s.PopupRounding = 2; s.ScrollbarRounding = 2; s.TabRounding = 2;
    s.WindowBorderSize = 0; s.FrameBorderSize = 0; s.PopupBorderSize = 1;
    s.WindowPadding = ImVec2(14, 12); s.FramePadding = ImVec2(8, 4); s.ItemSpacing = ImVec2(7, 6);
    ImVec4* c = s.Colors;
    c[ImGuiCol_Text] = col4(C_TEXT); c[ImGuiCol_TextDisabled] = col4(C_DIM);
    c[ImGuiCol_WindowBg] = col4(C_PANEL); c[ImGuiCol_ChildBg] = col4(C_PANEL); c[ImGuiCol_PopupBg] = col4(IM_COL32(0x21, 0x21, 0x21, 255));
    c[ImGuiCol_Border] = col4(C_LINE); c[ImGuiCol_Separator] = col4(C_LINE);
    c[ImGuiCol_FrameBg] = col4(C_WELL); c[ImGuiCol_FrameBgHovered] = col4(IM_COL32(0x2e, 0x2e, 0x2e, 255)); c[ImGuiCol_FrameBgActive] = col4(IM_COL32(0x33, 0x33, 0x33, 255));
    c[ImGuiCol_MenuBarBg] = col4(C_BG);
    c[ImGuiCol_CheckMark] = col4(C_PINK); c[ImGuiCol_SliderGrab] = col4(C_PINK); c[ImGuiCol_SliderGrabActive] = col4(C_DEEP);
    c[ImGuiCol_Button] = col4(IM_COL32(0x33, 0x33, 0x33, 255)); c[ImGuiCol_ButtonHovered] = col4(C_DEEP); c[ImGuiCol_ButtonActive] = col4(C_PINK);
    c[ImGuiCol_Header] = col4(IM_COL32(0x33, 0x33, 0x33, 255)); c[ImGuiCol_HeaderHovered] = col4(C_DEEP); c[ImGuiCol_HeaderActive] = col4(C_PINK);
    c[ImGuiCol_Tab] = col4(C_PANEL); c[ImGuiCol_TabActive] = col4(C_DEEP); c[ImGuiCol_TabHovered] = col4(C_PINK);
    c[ImGuiCol_TabUnfocusedActive] = col4(C_DEEP); c[ImGuiCol_TabUnfocused] = col4(C_PANEL);
    c[ImGuiCol_ModalWindowDimBg] = ImVec4(0, 0, 0, 0.5f);
}

static bool fileExists(const char* f) { if (FILE* t = fopen(f, "rb")) { fclose(t); return true; } return false; }

static void loadFont() {
    ImGuiIO& io = ImGui::GetIO();
    const char* f = "C:/Windows/Fonts/segoeui.ttf";
    if (!fileExists(f)) { io.Fonts->AddFontDefault(); return; }
    static const ImWchar ranges[] = {0x0020, 0x00FF, 0x2013, 0x2026, 0x2190, 0x21FF, 0};
    auto add = [&](const char* file, float size) { return io.Fonts->AddFontFromFileTTF(file, size, nullptr, ranges); };
    add(f, 16.0f);
    const char* light = fileExists("C:/Windows/Fonts/segoeuil.ttf") ? "C:/Windows/Fonts/segoeuil.ttf" : f;
    Splash::setFonts(add(light, 50.0f), add(f, 13.0f));
}

#ifdef _WIN32
static std::string fileDialog(bool save, const wchar_t* filter, const wchar_t* ext, const std::string& start) {
    std::vector<wchar_t> buf(4096, 0);
    const std::wstring s = fs::u8path(start).wstring();
    wcsncpy(buf.data(), s.c_str(), buf.size() - 1);
    OPENFILENAMEW o{};
    o.lStructSize = sizeof o; o.lpstrFilter = filter; o.lpstrFile = buf.data(); o.nMaxFile = (DWORD)buf.size(); o.lpstrDefExt = ext;
    o.Flags = save ? (OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR) : (OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR);
    if (!(save ? GetSaveFileNameW(&o) : GetOpenFileNameW(&o))) return "";
    return fs::path(buf.data()).u8string();
}
static std::string homeDir() {
    // Documents\Pygmalion unless Windows guards Documents: then Pygmalion in the user's own folder
    for (const KNOWNFOLDERID& id : {FOLDERID_Documents, FOLDERID_Profile}) {
        PWSTR p = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(id, 0, nullptr, &p))) {
            const fs::path d = fs::path(p) / "Pygmalion"; CoTaskMemFree(p);
            std::error_code ec; fs::create_directories(d, ec);
            const fs::path t = d / ".write-test";
            if (FILE* f = _wfopen(t.wstring().c_str(), L"wb")) { fclose(f); fs::remove(t, ec); return d.u8string(); }
        }
    }
    return "";
}
#else
static std::string fileDialog(bool, const wchar_t*, const wchar_t*, const std::string&) { return ""; }
static std::string homeDir() { return "."; }
#endif

// ---------- state ----------
static SDL_Window* window = nullptr;
static bool quit = false;
static Human::Body body;
static std::string dataDir, loadErr, toast; static double toastUntil = 0;
// MakeHuman's asset pack (clothes, hair, skins, poses, expressions...): assets\ beside data\ when it's there
static std::string assetsDir() {
    std::error_code ec;
    // beside data, or up from it (a build folder's data is a copy; the pack stays in the source folder)
    static std::string found; static bool looked = false;
    if (looked) return found;
    looked = true;
    fs::path d = fs::absolute(fs::u8path(dataDir), ec).lexically_normal().parent_path();
    for (int up = 0; up < 4 && !d.empty(); ++up, d = d.parent_path())
        if (fs::exists(d / "assets-mh", ec)) return found = (d / "assets-mh").u8string();
    return found;
}
static bool dirty = true;
static int tab = 0, forcedTab = 1;   // MakeHuman opens on Modelling
static std::string forcedTask;
static char prompt[512] = "", search[64] = "";
static std::string understood;
static float randomAmount = 0.6f; static bool randMacros = true, randFace = true, randBody = true;
static std::vector<std::pair<std::vector<float>, std::string>> undo;   // values of every modifier, and what changed
static float exportScale = 1.0f;   // MakeHuman's units are decimetres: 0.1 makes metres
static double now() { return SDL_GetTicks() / 1000.0; }
static void say(const std::string& s) { toast = s; toastUntil = now() + 6; }

static std::vector<float> snapshot() {
    std::vector<float> v;
    for (const auto& m : body.mods) v.push_back(m.value);
    v.insert(v.end(), {body.gender, body.age, body.muscle, body.weight, body.height, body.proportions, body.african, body.asian, body.caucasian, body.breastSize, body.breastFirmness});
    return v;
}
static void restore(const std::vector<float>& v) {
    for (size_t i = 0; i < body.mods.size() && i < v.size(); ++i) body.mods[i].value = v[i];
    const size_t k = body.mods.size();
    if (v.size() >= k + 11) {
        float* f[] = {&body.gender, &body.age, &body.muscle, &body.weight, &body.height, &body.proportions, &body.african, &body.asian, &body.caucasian, &body.breastSize, &body.breastFirmness};
        for (int i = 0; i < 11; ++i) *f[i] = v[k + i];
    }
    dirty = true;
}
static void remember(const std::string& what) { undo.push_back({snapshot(), what}); if (undo.size() > 60) undo.erase(undo.begin()); }

// the shaped body as a mesh for the exports: the body's visible triangles (less those under clothes), then the gear
static Mesh toMesh() {
    Mesh m;
    const std::vector<float>& pos = body.shown.empty() ? body.pos : body.shown;
    const std::vector<char> hid = Human::hiddenVerts(body);
    Material skin{"skin"}; skin.texturePath = body.skinTexture;
    m.mats.push_back(skin);
    for (size_t t = 0; t + 2 < body.tri.size(); t += 3) {
        if (hid[body.tri[t]] || hid[body.tri[t + 1]] || hid[body.tri[t + 2]]) continue;
        for (size_t c = t; c < t + 3; ++c) {
            const int v = body.tri[c], u = body.triUv[c];
            m.pos.insert(m.pos.end(), &pos[(size_t)v * 3], &pos[(size_t)v * 3] + 3);
            if (u >= 0 && (size_t)u * 2 + 1 < body.uv.size()) m.uv.insert(m.uv.end(), {body.uv[(size_t)u * 2], body.uv[(size_t)u * 2 + 1]}); else m.uv.insert(m.uv.end(), {0, 0});
        }
        m.matOf.push_back(0);
    }
    for (const auto& p : body.proxies) {
        std::vector<float> fit; Human::fitProxy(body, p, fit);
        Material mat{p.name.empty() ? p.kind : p.name}; mat.texturePath = p.kind == "proxymeshes" ? body.skinTexture : p.texture;
        const int mi = (int)m.mats.size();
        m.mats.push_back(mat);
        for (size_t c = 0; c < p.tri.size(); ++c) {
            m.pos.insert(m.pos.end(), &fit[(size_t)p.tri[c] * 3], &fit[(size_t)p.tri[c] * 3] + 3);
            m.uv.insert(m.uv.end(), {p.uv[c * 2], p.uv[c * 2 + 1]});
            if (c % 3 == 2) m.matOf.push_back(mi);
        }
    }
    computeNormals(m);
    return m;
}

// ---------- settings (Settings > Sliders and General, as MakeHuman has them) ----------
struct Prefs {
    bool sliderImages = true;      // the shape's picture beside each slider
    bool showValues = true;        // the number on the slider
    bool camFollows = true;        // the view turns to what a slider changes
    bool realtime = true;          // the body follows the slider as it moves (else when let go)
    bool metric = true;            // cm (else inches)
    bool invertMouse = false;
};
static Prefs prefs;
static std::vector<std::pair<std::vector<float>, std::string>> redo;

static std::string units(float cm) {
    char b[48];
    if (prefs.metric) snprintf(b, sizeof b, "%.1f cm", cm); else snprintf(b, sizeof b, "%.1f in", cm * 0.393700787f);
    return b;
}

// the slider pictures, loaded once
static std::map<std::string, GLuint> pics;
static GLuint picture(const std::string& path) {
    if (path.empty()) return 0;
    auto it = pics.find(path);
    if (it != pics.end()) return it->second;
    GLuint t = 0;
    int w, h, n;
    stbi_set_flip_vertically_on_load(0);
    if (unsigned char* px = stbi_load(path.c_str(), &w, &h, &n, 4)) {
        glGenTextures(1, &t); glBindTexture(GL_TEXTURE_2D, t);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
        stbi_image_free(px);
    }
    pics[path] = t;
    return t;
}

// MakeHuman's slider cameras: a part of the body (a joint marker), seen from a side
static void lookFor(const std::string& cam) {
    if (!prefs.camFollows || cam.empty()) return;
    const float H = Viewer::bodyHeight();
    const float PI = 3.14159265f;
    auto at = [&](const char* joint, float dist, float yaw, float pitch) {
        float p[3];
        if (Human::jointPos(body, joint, p)) Viewer::focus(p, dist, yaw, pitch);
    };
    if (cam == "frontView" || cam == "globalCamera") Viewer::focusBody(0, 0.05f);
    else if (cam == "backView") Viewer::focusBody(PI, 0.05f);
    else if (cam == "leftView") Viewer::focusBody(PI / 2, 0.05f);
    else if (cam == "rightView") Viewer::focusBody(-PI / 2, 0.05f);
    else if (cam == "topView") Viewer::focusBody(0, 1.5f);
    else if (cam == "bottomView") Viewer::focusBody(0, -1.5f);
    else if (cam == "faceCamera") at("joint-head", H * 0.22f, 0, 0.02f);
    else {
        // e.g. leftArmTopCamera, rightFootRightCamera: the part, then the side
        const bool left = cam.rfind("left", 0) == 0;
        const std::string s = left ? "l" : "r";
        const std::string rest = cam.substr(left ? 4 : 5);
        const char* joint = nullptr; float dist = H * 0.45f;
        if (rest.rfind("Arm", 0) == 0) { joint = "elbow"; dist = H * 0.5f; }
        else if (rest.rfind("Hand", 0) == 0) { joint = "hand"; dist = H * 0.2f; }
        else if (rest.rfind("Leg", 0) == 0) { joint = "knee"; dist = H * 0.6f; }
        else if (rest.rfind("Foot", 0) == 0) { joint = "ankle"; dist = H * 0.2f; }
        if (!joint) return;
        float yaw = 0, pitch = 0.05f;
        if (rest.find("Top") != std::string::npos) pitch = 1.45f;
        else if (rest.find("Left") != std::string::npos) yaw = PI / 2;
        else if (rest.find("Right") != std::string::npos) yaw = -PI / 2;
        at(("joint-" + s + "-" + joint).c_str(), dist, yaw, pitch);
    }
}

static void tip(const std::string& s) { if (!s.empty() && ImGui::IsItemHovered()) { ImGui::BeginTooltip(); ImGui::PushTextWrapPos(380); ImGui::TextUnformatted(s.c_str()); ImGui::PopTextWrapPos(); ImGui::EndTooltip(); } }

// one slider, MakeHuman's way: its picture beside it, its value on it, right-click resets, the view follows
static void modSlider(int i, float w, const std::string& tabCam) {
    Human::Modifier& m = body.mods[i];
    ImGui::PushID(i);
    const GLuint pic = prefs.sliderImages ? picture(m.image) : 0;
    float sw = w;
    if (pic) { ImGui::Image((ImTextureID)(intptr_t)pic, ImVec2(46, 46)); ImGui::SameLine(); sw = w - 53; ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 12); }
    float v = m.value;
    std::string fmt = m.label;
    // Measure's sliders say the measurement itself
    const float cm = m.group == "measure" ? Human::measureCm(body, m.fullName()) : -1;
    if (prefs.showValues) fmt += cm >= 0 ? ": " + units(cm) : (m.min() < 0 ? ": %+.2f" : ": %.2f");
    for (size_t at = 0; cm >= 0 && (at = fmt.find('%', at)) != std::string::npos; at += 2) fmt.insert(at, "%");
    ImGui::SetNextItemWidth(sw);
    const bool ch = ImGui::SliderFloat("##m", &v, m.min(), 1.0f, fmt.c_str());
    if (ImGui::IsItemActivated()) { remember(m.label); redo.clear(); lookFor(m.cam.empty() ? tabCam : m.cam); }
    if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) { remember(m.label); Human::setModifier(body, i, m.defaultValue); dirty = true; }
    tip(m.description.empty() ? m.label : m.description);
    if (ch) { Human::setModifier(body, i, v); if (prefs.realtime) dirty = true; }
    if (ImGui::IsItemDeactivatedAfterEdit()) dirty = true;
    ImGui::PopID();
}

static void groupBox(const char* title) {
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, col4(C_PINK)); ImGui::TextUnformatted(title); ImGui::PopStyleColor();
    ImGui::Separator();
}

// ---------- posing: MakeHuman's default skeleton moves the body; the chosen skeleton is the one exported ----------
static Skel::Skeleton rig;                      // the default skeleton, for posing
static std::string skeletonFile;                // the chosen skeleton ("" none), as MakeHuman's Skeleton tab
static Skel::Pose bodyPose; static std::string poseFile;
static std::vector<Skel::Pose> faceUnits; static std::vector<std::string> unitNames; static std::vector<float> unitW;
static std::string expressionFile;
static bool showBones = false;

static std::vector<std::string> filesIn(const std::string& sub, const char* ext) {
    std::vector<std::string> v;
    std::error_code ec;
    for (const std::string& root : {dataDir, assetsDir()})
        if (!root.empty())
            for (const auto& e : fs::recursive_directory_iterator(fs::u8path(root) / sub, ec))
                if (e.is_regular_file() && e.path().extension() == ext) v.push_back(e.path().u8string());
    std::sort(v.begin(), v.end(), [](const std::string& a, const std::string& b) { return fs::u8path(a).stem() < fs::u8path(b).stem(); });
    return v;
}

static void loadFaceUnits() {
    std::string err, t;
    Skel::loadBvh((fs::u8path(dataDir) / "poseunits" / "face-poseunits.bvh").u8string(), faceUnits, err);
    std::ifstream in(fs::u8path(dataDir) / "poseunits" / "face-poseunits.json");
    std::ostringstream ss; ss << in.rdbuf(); t = ss.str();
    Json::Value v; size_t i = 0;
    if (Json::parse(t, i, v)) if (const Json::Value* fm = v.get("framemapping")) for (const auto& n : fm->a) unitNames.push_back(n.s);
    unitW.assign(unitNames.size(), 0.0f);
}

// the pose: the body's pose, the face's expression over it (mixPoses: the expression's bones win)
static void repose() {
    if (rig.empty()) { body.shown.clear(); return; }
    bool anyUnit = false;
    for (size_t k = 1; k < unitW.size(); ++k) if (unitW[k] > 0) anyUnit = true;
    if (bodyPose.empty() && !anyUnit) { body.shown.clear(); return; }
    Skel::Pose p = bodyPose;
    if (anyUnit) {
        std::vector<std::pair<const Skel::Pose*, float>> parts;
        for (size_t k = 1; k < unitW.size() && k < faceUnits.size(); ++k) if (unitW[k] > 0) parts.push_back({&faceUnits[k], unitW[k]});
        const Skel::Pose face = Skel::blend(parts);
        for (const auto& kv : face.bones) {
            const auto& m = kv.second;
            const bool moved = std::fabs(m[0] - 1) + std::fabs(m[5] - 1) + std::fabs(m[10] - 1) > 1e-6f;
            if (moved) p.bones[kv.first] = m;
        }
    }
    Skel::setPose(rig, &p);
    Skel::skin(rig, body.pos, body.shown);
}

static void setPoseFile(const std::string& f) {
    poseFile = f;
    bodyPose = Skel::Pose();
    if (!f.empty()) {
        std::vector<Skel::Pose> fr; std::string err;
        if (Skel::loadBvh(f, fr, err)) bodyPose = fr[0]; else say(err);
    }
    dirty = true;
}

// an .mhpose expression: its pose units and their weights (MakeHuman's expressions/*.mhpose)
static void setExpressionFile(const std::string& f) {
    expressionFile = f;
    std::fill(unitW.begin(), unitW.end(), 0.0f);
    if (!f.empty()) {
        std::ifstream in(fs::u8path(f)); std::ostringstream ss; ss << in.rdbuf();
        Json::Value v; size_t i = 0; const std::string t = ss.str();
        if (Json::parse(t, i, v)) if (const Json::Value* u = v.get("unit_poses"))
            for (const auto& kv : u->o) for (size_t k = 0; k < unitNames.size(); ++k) if (unitNames[k] == kv.first) unitW[k] = (float)kv.second.n;
    }
    dirty = true;
}

static std::string niceName(const std::string& path) {
    std::string n = fs::u8path(path).stem().u8string();
    for (auto& c : n) if (c == '_' || c == '-') c = ' ';
    if (!n.empty()) n[0] = (char)toupper((unsigned char)n[0]);
    return n;
}

// a MakeHuman file chooser: a "None" choice, then each file with its .thumb
static bool chooser(const char* id, const std::vector<std::string>& files, const std::string& current, std::string& picked, float w, bool none = true, const char* noneLabel = "None") {
    bool ch = false;
    ImGui::PushID(id);
    const int cols = std::max(1, (int)(w / 104));
    int n = 0;
    auto cell = [&](const std::string& f, const std::string& label) {
        if (n % cols) ImGui::SameLine();
        ImGui::BeginGroup();
        const GLuint th = f.empty() ? 0 : picture(fs::u8path(f).replace_extension(".thumb").u8string());
        const bool on = f == current;
        if (on) ImGui::PushStyleColor(ImGuiCol_Button, col4(C_DEEP));
        ImGui::PushID(n);
        bool click = th ? ImGui::ImageButton("b", (ImTextureID)(intptr_t)th, ImVec2(88, 88)) : ImGui::Button(f.empty() ? noneLabel : "?", ImVec2(96, 96));
        ImGui::PopID();
        if (on) ImGui::PopStyleColor();
        std::string l = label; if (l.size() > 14) l = l.substr(0, 13) + ".";
        ImGui::TextDisabled("%s", l.c_str());
        ImGui::EndGroup();
        tip(label);
        if (click) { picked = f; ch = true; }
        ++n;
    };
    if (none) cell("", noneLabel);
    for (const auto& f : files) cell(f, niceName(f));
    ImGui::PopID();
    return ch;
}

static void notYet(const char* what);
// ---------- Geometries and Materials: MakeHuman's proxy and material libraries ----------
static const char* kindOf(const std::string& tab) {
    if (tab == "Clothes") return "clothes";
    if (tab == "Eyes") return "eyes";
    if (tab == "Hair") return "hair";
    if (tab == "Teeth") return "teeth";
    if (tab == "Topologies") return "proxymeshes";
    if (tab == "Eyebrows") return "eyebrows";
    if (tab == "Eyelashes") return "eyelashes";
    return "tongue";
}
static std::map<std::string, std::vector<std::string>> libCache;
static const std::vector<std::string>& library(const std::string& sub, const char* ext) {
    const std::string key = sub + ext;
    auto it = libCache.find(key);
    if (it != libCache.end()) return it->second;
    auto v = filesIn(sub, ext);
    // the asset pack's own copy wins over data's (the same file twice shows once)
    std::vector<std::string> out; std::set<std::string> seen;
    for (const auto& f : v) if (seen.insert(fs::u8path(f).filename().u8string()).second) out.push_back(f);
    return libCache[key] = out;
}
static void putOn(const std::string& f, const std::string& kind) {
    remember("Geometry");
    std::string err;
    if (f.empty()) { Human::takeOff(body, kind); dirty = true; return; }
    if (Human::wear(body, f, err)) { dirty = true; say((Human::isWorn(body, f) ? "On: " : "Off: ") + niceName(f)); } else say(err);
}
static void geometriesLeft(const std::string& t, float lw) {
    const std::string kind = kindOf(t);
    groupBox(t.c_str());
    if (assetsDir().empty() && kind != "eyes") { notYet("MakeHuman's clothes, hair, eyebrows, eyelashes, teeth, tongue, topologies and skins are its asset pack (CC0). Put its \"system\" folder's contents in assets-mh beside data."); return; }
    const auto& files = kind == "proxymeshes" ? library("proxymeshes", ".proxy") : library(kind, ".mhclo");
    std::string picked;
    if (kind == "clothes") {
        // several at once: a click puts one on, a second click takes it off; the worn ones are pink
        ImGui::Checkbox("Hide faces under clothes", &body.hideUnderClothes);
        if (ImGui::IsItemEdited()) dirty = true;
        if (ImGui::Button("Take everything off", ImVec2(lw, 0))) { remember("Geometry"); Human::takeOff(body, "clothes"); dirty = true; }
        const int cols = std::max(1, (int)(lw / 104));
        int n = 0;
        for (const auto& f : files) {
            if (n % cols) ImGui::SameLine();
            ImGui::BeginGroup(); ImGui::PushID(n);
            const GLuint th = picture(fs::u8path(f).replace_extension(".thumb").u8string());
            const bool on = Human::isWorn(body, f);
            if (on) ImGui::PushStyleColor(ImGuiCol_Button, col4(C_DEEP));
            const bool click = th ? ImGui::ImageButton("b", (ImTextureID)(intptr_t)th, ImVec2(88, 88)) : ImGui::Button("?", ImVec2(96, 96));
            if (on) ImGui::PopStyleColor();
            std::string l = niceName(f); if (l.size() > 14) l = l.substr(0, 13) + ".";
            ImGui::TextDisabled("%s", l.c_str());
            ImGui::PopID(); ImGui::EndGroup();
            tip(niceName(f));
            if (click) putOn(f, kind);
            ++n;
        }
        return;
    }
    const Human::Proxy* cur = Human::worn(body, kind);
    if (chooser(kind.c_str(), files, cur ? cur->file : std::string(), picked, lw, kind != "eyes")) putOn(picked, kind);
}
static void materialsLeft(float lw) {
    // MakeHuman's Skin/Material: the skins, then each worn proxy's own materials (eye colours, hair colours...)
    std::string picked;
    groupBox("Skin");
    std::vector<std::string> skins = library("skins", ".mhmat");
    if (chooser("skin", skins, body.skinMaterial, picked, lw, true, "Default")) { remember("Skin"); Human::setSkin(body, picked); dirty = true; say(picked.empty() ? "The default skin" : niceName(picked)); }
    for (const auto& p : body.proxies) {
        if (p.kind == "proxymeshes") continue;
        std::vector<std::string> mats;
        std::error_code ec;
        const fs::path dir = fs::u8path(p.file).parent_path();
        for (const fs::path d : {dir, dir.parent_path() / "materials"})
            for (const auto& e : fs::directory_iterator(d, ec)) if (e.path().extension() == ".mhmat") mats.push_back(e.path().u8string());
        if (mats.size() < 2) continue;
        std::sort(mats.begin(), mats.end());
        groupBox((niceName(p.file) + " material").c_str());
        if (chooser(("m" + p.file).c_str(), mats, p.material, picked, lw, false)) { remember("Material"); Human::setProxyMaterial(body, p.kind, picked); dirty = true; }
    }
}

// ---------- the tasks: MakeHuman's categories and their tabs ----------
enum Cat { CAT_FILES, CAT_MODELLING, CAT_GEOMETRIES, CAT_MATERIALS, CAT_POSE, CAT_RENDERING, CAT_SETTINGS, CAT_UTILITIES, CAT_HELP, CAT_COUNT };
static const char* CAT_NAMES[] = {"Files", "Modelling", "Geometries", "Materials", "Pose/Animate", "Rendering", "Settings", "Utilities", "Help"};
static int cat = CAT_MODELLING;
static std::string task[CAT_COUNT] = {"Load", "Main", "Clothes", "Skin/Material", "Skeleton", "Render", "Sliders", "Describe", "About"};
static std::map<std::string, int> category;   // a modelling tab's chosen Category (the slider group shown)

static std::vector<std::string> tasksOf(int c) {
    switch (c) {
    case CAT_FILES: return {"Load", "Save", "Export"};
    case CAT_MODELLING: {
        std::vector<std::string> v = {"Main"};
        for (const char* n : {"Gender", "Face", "Torso", "Arms and Legs", "Body shapes"}) for (const auto& t : body.tabs) if (t.name == n) v.push_back(n);
        v.push_back("Custom");
        for (const auto& t : body.tabs) if (t.name == "Measure") v.push_back("Measure");
        v.push_back("Random");
        return v;
    }
    case CAT_GEOMETRIES: return {"Clothes", "Eyes", "Hair", "Teeth", "Topologies", "Eyebrows", "Eyelashes", "Tongue"};
    case CAT_MATERIALS: return {"Skin/Material"};
    case CAT_POSE: return {"Skeleton", "Pose", "Expression"};
    case CAT_RENDERING: return {"Render", "Scene", "Viewer"};
    case CAT_SETTINGS: return {"Sliders", "General", "Mouse", "Shortcuts"};
    case CAT_UTILITIES: return {"Describe", "Symmetry", "Statistics"};
    default: return {"About", "Licence"};
    }
}

static const Human::Tab* tabNamed(const std::string& n) { for (const auto& t : body.tabs) if (t.name == n) return &t; return nullptr; }

// Main: the macro sliders, with MakeHuman's own readings (years, cm, percentages)
static void macroSliders(float w) {
    auto macro = [&](const char* var, std::function<std::string(float)> shown, const char* help) {
        for (auto& m : body.mods) {
            if (!m.macro || m.variable != var) continue;
            float v = m.value;
            ImGui::PushID(var);
            ImGui::SetNextItemWidth(w);
            std::string label = prefs.showValues ? shown(v) : std::string(var);
            for (size_t at = 0; (at = label.find('%', at)) != std::string::npos; at += 2) label.insert(at, "%");
            const bool ch = ImGui::SliderFloat("##v", &v, 0, 1, label.c_str());
            if (ImGui::IsItemActivated()) { remember(var); redo.clear(); lookFor("frontView"); }
            if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) { remember(var); Human::setMacro(body, var, m.defaultValue); dirty = true; }
            tip(m.description.empty() ? help : m.description);
            if (ch) { Human::setMacro(body, var, v); if (prefs.realtime) dirty = true; }
            if (ImGui::IsItemDeactivatedAfterEdit()) dirty = true;
            ImGui::PopID();
            return;
        }
    };
    char b[96];
    macro("Gender", [&](float v) { snprintf(b, sizeof b, "Gender: %.2f%%", v * 100); return std::string(b); }, "Female to male");
    macro("Age", [&](float) { snprintf(b, sizeof b, "Age: %.1f years", Human::ageYears(body)); return std::string(b); }, "1 to 90 years");
    macro("Muscle", [&](float v) { snprintf(b, sizeof b, "Muscle: %.2f%%", v * 100); return std::string(b); }, "");
    macro("Weight", [&](float v) { snprintf(b, sizeof b, "Weight: %.2f%%", 50 + (150 - 50) * v); return std::string(b); }, "");
    macro("Height", [&](float) { return "Height: " + units(Human::heightCm(body)); }, "");
    macro("BodyProportions", [&](float v) { snprintf(b, sizeof b, "Proportions: %.2f%%", v * 100); return std::string(b); }, "Uncommon to ideal proportions");
    macro("African", [&](float v) { snprintf(b, sizeof b, "African: %.2f%%", v * 100); return std::string(b); }, "");
    macro("Asian", [&](float v) { snprintf(b, sizeof b, "Asian: %.2f%%", v * 100); return std::string(b); }, "");
    macro("Caucasian", [&](float v) { snprintf(b, sizeof b, "Caucasian: %.2f%%", v * 100); return std::string(b); }, "");
}

// a modelling tab: the chosen Category's sliders on the left, the Category list on the right
static void modTabLeft(const Human::Tab& t, float w) {
    int& c = category[t.name];
    if (c >= (int)t.groups.size()) c = 0;
    const auto& g = t.groups[c];
    groupBox(g.name.c_str());
    const std::string tabCam = t.name == "Face" ? "faceCamera" : "";
    for (int i : g.mods) if (!body.mods[i].macro) modSlider(i, w, tabCam);
    // the Gender tab's breast sliders are macros (size, firmness)
    if (t.name == "Gender" && g.name == "Breast") {
        char b[64];
        for (const char* var : {"BreastSize", "BreastFirmness"})
            for (auto& m : body.mods) if (m.macro && m.variable == var) {
                float v = m.value;
                snprintf(b, sizeof b, "%s: %%.2f%%%%", std::string(var) == "BreastSize" ? "Breast size" : "Breast firmness");
                ImGui::PushID(var); ImGui::SetNextItemWidth(w);
                if (ImGui::SliderFloat("##b", &v, 0, 1, prefs.showValues ? b : var)) { Human::setMacro(body, var, v); dirty = true; }
                if (ImGui::IsItemActivated()) remember(var);
                ImGui::PopID();
            }
    }
}
static void modTabRight(const Human::Tab& t) {
    groupBox("Category");
    int& c = category[t.name];
    for (int k = 0; k < (int)t.groups.size(); ++k) if (ImGui::RadioButton(t.groups[k].name.c_str(), &c, k)) lookFor(t.name == "Face" ? "faceCamera" : "");
}

static void statistics() {
    groupBox("Statistics");
    ImGui::Text("Height: %s", units(Human::heightCm(body)).c_str());
    ImGui::Text("Chest: %s", units(Human::measureCm(body, "measure/measure-bust-circ-decr|incr")).c_str());
    ImGui::Text("Waist: %s", units(Human::measureCm(body, "measure/measure-waist-circ-decr|incr")).c_str());
    ImGui::Text("Hips: %s", units(Human::measureCm(body, "measure/measure-hips-circ-decr|incr")).c_str());
}

// Random, as MakeHuman's: what to randomise, how symmetric, and back to the start
static bool rMacro = true, rFace = true, rBody = true, rHeight = false; static float rSym = 0.7f;
static void randomLeft(float w) {
    groupBox("Randomize settings");
    ImGui::Checkbox("Macro", &rMacro); ImGui::Checkbox("Face", &rFace); ImGui::Checkbox("Body", &rBody); ImGui::Checkbox("Height", &rHeight);
    ImGui::SetNextItemWidth(w); ImGui::SliderFloat("##sym", &rSym, 0, 1, "Symmetry: %.2f");
    ImGui::SetNextItemWidth(w); ImGui::SliderFloat("##amt", &randomAmount, 0.1f, 1.0f, "Amount: %.2f");
    if (ImGui::Button("Randomize", ImVec2(w, 0))) {
        remember("Randomize"); redo.clear();
        const float keepH = body.height;
        const bool sym = body.symmetry;
        body.symmetry = std::uniform_real_distribution<float>(0, 1)(*new std::mt19937(std::random_device{}())) < rSym;
        Human::randomize(body, randomAmount, rMacro, rFace, rBody, (unsigned)std::random_device{}());
        body.symmetry = sym;
        if (!rHeight) Human::setMacro(body, "Height", keepH);
        dirty = true;
    }
    groupBox("Restore settings");
    if (ImGui::Button("Restore to defaults", ImVec2(w, 0))) { remember("Restore"); Human::resetAll(body); dirty = true; }
}

// Files
static std::vector<std::string> modelFiles() {
    std::vector<std::string> v;
    std::error_code ec;
    for (const auto& e : fs::directory_iterator(fs::u8path(homeDir()), ec)) if (e.path().extension() == ".mhm") v.push_back(e.path().u8string());
    std::sort(v.begin(), v.end());
    return v;
}
static char saveName[128] = "person";
static int exportFormat = 0; static int exportUnits = 1;
static const char* EXPORT_NAMES[] = {"glTF binary (glb)", "Wavefront obj", "Stereolithography (stl)"};
static const char* UNIT_NAMES[] = {"decimeter", "meter", "centimeter", "millimeter", "inch"};
static const float UNIT_SCALE[] = {1, 0.1f, 10, 100, 3.93700787f};
static bool wantThumb = false; static std::string thumbPath;

static void filesLeft(float w) {
    if (task[CAT_FILES] == "Load") {
        groupBox("Load a model");
        ImGui::TextDisabled("Models in %s", homeDir().c_str());
        if (ImGui::Button("Load from another folder...", ImVec2(w, 0))) {
            const std::string p = fileDialog(false, L"MakeHuman model\0*.mhm\0\0", L"mhm", homeDir() + "\\");
            std::string err;
            if (!p.empty()) { remember("Load"); if (Human::loadMhm(body, p, err)) { dirty = true; say("Loaded " + fs::u8path(p).filename().u8string()); } else say(err); }
        }
        for (const auto& f : modelFiles()) {
            const std::string name = fs::u8path(f).stem().u8string();
            const GLuint th = picture(fs::u8path(f).replace_extension(".thumb").u8string());
            ImGui::PushID(f.c_str());
            if (th) { if (ImGui::ImageButton("t", (ImTextureID)(intptr_t)th, ImVec2(64, 64))) { std::string err; remember("Load"); if (Human::loadMhm(body, f, err)) dirty = true; else say(err); } ImGui::SameLine(); }
            if (ImGui::Button(name.c_str(), ImVec2(th ? w - 80 : w, th ? 72 : 0))) { std::string err; remember("Load"); if (Human::loadMhm(body, f, err)) dirty = true; else say(err); }
            ImGui::PopID();
        }
    } else if (task[CAT_FILES] == "Save") {
        groupBox("Save the model");
        ImGui::SetNextItemWidth(w); ImGui::InputText("##name", saveName, sizeof saveName);
        if (ImGui::Button("Save", ImVec2(w, 0))) {
            const std::string p = (fs::u8path(homeDir()) / (std::string(saveName) + ".mhm")).u8string();
            std::string err;
            if (Human::saveMhm(body, p, saveName, err)) { say("Saved " + p + " (MakeHuman opens it too)"); wantThumb = true; thumbPath = fs::u8path(p).replace_extension(".thumb").u8string(); }
            else say(err);
        }
        if (ImGui::Button("Save as...", ImVec2(w, 0))) {
            const std::string p = fileDialog(true, L"MakeHuman model\0*.mhm\0\0", L"mhm", homeDir() + "\\" + saveName + ".mhm");
            std::string err;
            if (!p.empty()) { if (Human::saveMhm(body, p, fs::u8path(p).stem().u8string(), err)) { say("Saved"); wantThumb = true; thumbPath = fs::u8path(p).replace_extension(".thumb").u8string(); } else say(err); }
        }
        ImGui::TextDisabled("A picture of the view is saved beside it (.thumb), as MakeHuman does.");
    } else {
        groupBox("Export");
        ImGui::SetNextItemWidth(w); ImGui::InputText("##ename", saveName, sizeof saveName);
        if (ImGui::Button("Export...", ImVec2(w, 0))) {
            static const wchar_t* filters[] = {L"GLB\0*.glb\0\0", L"OBJ\0*.obj\0\0", L"STL\0*.stl\0\0"};
            static const wchar_t* exts[] = {L"glb", L"obj", L"stl"};
            static const char* dots[] = {".glb", ".obj", ".stl"};
            const std::string p = fileDialog(true, filters[exportFormat], exts[exportFormat], homeDir() + "\\" + saveName + dots[exportFormat]);
            if (!p.empty()) {
                std::string err;
                const Mesh m = toMesh();
                const float sc = UNIT_SCALE[exportUnits];
                const bool ok = exportFormat == 0 ? saveGlb(m, p, sc, false, err) : exportFormat == 1 ? saveObj(m, p, sc, false, err) : saveStl(m, p, sc, false, err);
                say(ok ? "Exported " + fs::u8path(p).filename().u8string() : err);
            }
        }
    }
}
static void filesRight() {
    if (task[CAT_FILES] != "Export") return;
    groupBox("Mesh format");
    for (int k = 0; k < 3; ++k) ImGui::RadioButton(EXPORT_NAMES[k], &exportFormat, k);
    groupBox("Options");
    ImGui::TextUnformatted("Scale units");
    ImGui::SetNextItemWidth(-1); ImGui::Combo("##units", &exportUnits, UNIT_NAMES, 5);
}

// ---------- the toolbar: MakeHuman's edit, view, symmetry and camera buttons ----------
static void symmetry(bool rightToLeft) {
    remember(rightToLeft ? "Symmetry R>L" : "Symmetry L>R"); redo.clear();
    for (size_t i = 0; i < body.mods.size(); ++i) {
        const auto& m = body.mods[i];
        std::vector<std::string> parts; std::stringstream ss(m.name); std::string p; bool from = false;
        while (std::getline(ss, p, '-')) { if (p == (rightToLeft ? "r" : "l")) { from = true; p = rightToLeft ? "l" : "r"; } parts.push_back(p); }
        if (!from) continue;
        std::string n; for (size_t k = 0; k < parts.size(); ++k) n += (k ? "-" : "") + parts[k];
        auto it = body.modByName.find(m.group + "/" + n);
        if (it != body.modByName.end()) body.mods[it->second].value = m.value;
    }
    dirty = true;
}

static void toolbar(float x, float y, float w) {
    ImGui::SetNextWindowPos(ImVec2(x, y)); ImGui::SetNextWindowSize(ImVec2(w, 34));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 5));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, col4(C_BG));
    ImGui::Begin("toolbar", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus);
    auto btn = [&](const char* l, const char* t, bool on = false) { if (on) ImGui::PushStyleColor(ImGuiCol_Button, col4(C_DEEP)); const bool r = ImGui::SmallButton(l); if (on) ImGui::PopStyleColor(); tip(t); ImGui::SameLine(); return r; };
    auto gap = [] { ImGui::SameLine(0, 12); };
    if (btn("Undo", "Undo (Ctrl+Z)") && !undo.empty()) { redo.push_back({snapshot(), undo.back().second}); restore(undo.back().first); undo.pop_back(); }
    if (btn("Redo", "Redo (Ctrl+Y)") && !redo.empty()) { undo.push_back({snapshot(), redo.back().second}); restore(redo.back().first); redo.pop_back(); }
    if (btn("Reset", "Reset the human to the start")) { remember("Reset"); Human::resetAll(body); Human::defaultGear(body); dirty = true; }
    gap();
    Viewer::Look& lk = Viewer::look();
    if (btn("Wire", "Show the mesh's lines", lk.wire)) lk.wire = !lk.wire;
    if (btn("Clay", "Plain grey instead of skin", lk.shading == 1)) lk.shading = lk.shading == 1 ? 0 : 1;
    if (btn("Floor", "The grid under the feet", lk.grid)) lk.grid = !lk.grid;
    gap();
    if (btn("R>L", "Copy the right side's shapes to the left")) symmetry(true);
    if (btn("L>R", "Copy the left side's shapes to the right")) symmetry(false);
    if (btn("Symm", "Moving a left slider moves the right one too", body.symmetry)) body.symmetry = !body.symmetry;
    gap();
    const float PI = 3.14159265f;
    if (btn("Front", "Front view")) Viewer::focusBody(0, 0.05f);
    if (btn("Back", "Back view")) Viewer::focusBody(PI, 0.05f);
    if (btn("Left", "Left side")) Viewer::focusBody(PI / 2, 0.05f);
    if (btn("Right", "Right side")) Viewer::focusBody(-PI / 2, 0.05f);
    if (btn("Top", "From above")) Viewer::focusBody(0, 1.5f);
    if (btn("Under", "From below")) Viewer::focusBody(0, -1.5f);
    if (btn("Face", "The face, close")) lookFor("faceCamera");
    if (btn("Home", "The whole person, from the front")) Viewer::focusBody(0, 0.05f);
    gap();
    if (btn("Shot", "Save a picture of the view")) {
        const std::string p = fileDialog(true, L"PNG\0*.png\0\0", L"png", homeDir() + "\\screenshot.png");
        if (!p.empty()) { wantThumb = true; thumbPath = p; }
    }
    ImGui::End();
    ImGui::PopStyleColor(); ImGui::PopStyleVar();
}

// ---------- the categories without their own code yet show what's coming, honestly ----------
static void notYet(const char* what) {
    ImGui::PushTextWrapPos(0);
    ImGui::TextDisabled("%s", what);
    ImGui::PopTextWrapPos();
}

static void frame() {
    ImGuiIO& io = ImGui::GetIO();
    const float W = io.DisplaySize.x, H = io.DisplaySize.y;
    if (!io.WantTextInput) {
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z) && !undo.empty()) { redo.push_back({snapshot(), undo.back().second}); restore(undo.back().first); undo.pop_back(); }
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y) && !redo.empty()) { undo.push_back({snapshot(), redo.back().second}); restore(redo.back().first); redo.pop_back(); }
    }
    // the category tabs along the top, then the category's own tabs
    const ImGuiWindowFlags fixed = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus;
    ImGui::SetNextWindowPos(ImVec2(0, 0)); ImGui::SetNextWindowSize(ImVec2(W, 66));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 4));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, col4(C_BG));
    ImGui::Begin("cats", nullptr, fixed);
    if (ImGui::BeginTabBar("catbar")) {
        for (int c = 0; c < CAT_COUNT; ++c) if (ImGui::BeginTabItem(CAT_NAMES[c], nullptr, forcedTab == c ? ImGuiTabItemFlags_SetSelected : 0)) { cat = c; ImGui::EndTabItem(); }
        forcedTab = -1;
        ImGui::EndTabBar();
    }
    if (ImGui::BeginTabBar("taskbar")) {
        for (const auto& t : tasksOf(cat)) if (ImGui::BeginTabItem(t.c_str(), nullptr, forcedTask == t ? ImGuiTabItemFlags_SetSelected : 0)) {
            if (task[cat] != t) { task[cat] = t; if (cat == CAT_MODELLING && t == "Face") lookFor("faceCamera"); }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
        if (!forcedTask.empty() && task[cat] == forcedTask) forcedTask.clear();
    }
    ImGui::End();
    ImGui::PopStyleColor(); ImGui::PopStyleVar();
    const float top = 66, statusH = 26, sideW = 340;
    toolbar(sideW, top, W - 2 * sideW);

    // the left and right panels
    ImGui::SetNextWindowPos(ImVec2(0, top)); ImGui::SetNextWindowSize(ImVec2(sideW, H - top - statusH));
    ImGui::Begin("left", nullptr, fixed);
    const float lw = sideW - 28;
    const std::string& t = task[cat];
    if (!loadErr.empty()) { ImGui::PushTextWrapPos(lw); ImGui::TextColored(col4(C_PINK), "%s", loadErr.c_str()); ImGui::PopTextWrapPos(); }
    else if (cat == CAT_MODELLING) {
        if (t == "Main") { groupBox("Macros"); macroSliders(lw); }
        else if (t == "Random") randomLeft(lw);
        else if (t == "Custom") { groupBox("Custom targets"); notYet(("Put your own .target files in " + homeDir() + "\\custom and they appear here as sliders.").c_str()); }
        else if (const Human::Tab* tb = tabNamed(t)) modTabLeft(*tb, lw);
    } else if (cat == CAT_FILES) filesLeft(lw);
    else if (cat == CAT_SETTINGS) {
        if (t == "Sliders") {
            groupBox("Slider behaviour");
            ImGui::Checkbox("Update in real time", &prefs.realtime);
            ImGui::Checkbox("Camera follows the slider", &prefs.camFollows);
            groupBox("Slider appearance");
            ImGui::Checkbox("Show slider values", &prefs.showValues);
            ImGui::Checkbox("Show slider images", &prefs.sliderImages);
        } else if (t == "General") {
            groupBox("Units");
            int u = prefs.metric ? 0 : 1;
            if (ImGui::RadioButton("Metric (cm)", &u, 0)) prefs.metric = true;
            if (ImGui::RadioButton("Imperial (in)", &u, 1)) prefs.metric = false;
            groupBox("Models folder");
            ImGui::TextWrapped("%s", homeDir().c_str());
        } else if (t == "Mouse") {
            groupBox("Camera");
            ImGui::TextWrapped("Left drag: turn. Right or middle drag: move. Wheel: zoom.");
        } else {
            groupBox("Shortcuts");
            ImGui::TextUnformatted("Ctrl+Z  Undo\nCtrl+Y  Redo");
        }
    } else if (cat == CAT_UTILITIES) {
        if (t == "Describe") {
            groupBox("Describe the person");
            ImGui::SetNextItemWidth(lw);
            const bool enter = ImGui::InputTextWithHint("##prompt", "a tall, muscular old woman with a big nose", prompt, sizeof prompt, ImGuiInputTextFlags_EnterReturnsTrue);
            if (ImGui::Button("Make them", ImVec2(lw, 0)) || enter) { remember("Describe"); redo.clear(); understood = Human::describe(body, prompt); if (understood.empty()) understood = "(no words it knows)"; dirty = true; }
            if (!understood.empty()) { ImGui::PushTextWrapPos(lw); ImGui::TextDisabled("%s", understood.c_str()); ImGui::PopTextWrapPos(); }
        } else if (t == "Symmetry") {
            groupBox("Symmetry");
            if (ImGui::Button("Right to left", ImVec2(lw, 0))) symmetry(true);
            if (ImGui::Button("Left to right", ImVec2(lw, 0))) symmetry(false);
            ImGui::Checkbox("Symmetric modelling", &body.symmetry);
        } else statistics();
    } else if (cat == CAT_HELP) {
        if (t == "About") { groupBox("Pygmalion " PYGMALION_VERSION); ImGui::TextWrapped("People in 3D with sliders: MakeHuman, remade in C++ by Cursed Entertainment. Farica\xe2\x98\x86Kimora \xc2\xa9 Cursed Entertainment."); }
        else { groupBox("Licence"); ImGui::TextWrapped("The code is AGPL-3.0 (it ports MakeHuman's code). The body, its shapes and the assets are MakeHuman's, CC0."); }
    } else if (cat == CAT_GEOMETRIES) geometriesLeft(t, lw);
    else if (cat == CAT_MATERIALS) materialsLeft(lw);
    else if (cat == CAT_POSE) {
        std::string picked;
        if (t == "Skeleton") {
            groupBox("Skeleton");
            if (chooser("sk", filesIn("rigs", ".mhskel"), skeletonFile, picked, lw)) { skeletonFile = picked; say(picked.empty() ? "No skeleton" : niceName(picked) + " skeleton (exported with the model)"); }
            ImGui::Checkbox("Show the bones", &showBones);
        } else if (t == "Pose") {
            groupBox("Pose");
            if (chooser("po", filesIn("poses", ".bvh"), poseFile, picked, lw)) setPoseFile(picked);
        } else {
            groupBox("Expression");
            if (chooser("ex", filesIn("expressions", ".mhpose"), expressionFile, picked, lw)) setExpressionFile(picked);
            if (filesIn("expressions", ".mhpose").empty()) notYet("MakeHuman's ready-made expressions come with its asset pack; the mixer on the right makes any expression now.");
        }
    }
    else notYet("Rendering: this tab is being built.");
    ImGui::End();

    ImGui::SetNextWindowPos(ImVec2(W - sideW, top)); ImGui::SetNextWindowSize(ImVec2(sideW, H - top - statusH));
    ImGui::Begin("right", nullptr, fixed);
    if (loadErr.empty()) {
        if (cat == CAT_MODELLING) {
            if (t == "Measure") { if (const Human::Tab* tb = tabNamed(t)) modTabRight(*tb); statistics(); }
            else if (t != "Main" && t != "Random" && t != "Custom") { if (const Human::Tab* tb = tabNamed(t)) modTabRight(*tb); }
            else if (t == "Main") statistics();
        } else if (cat == CAT_FILES) filesRight();
        else if (cat == CAT_POSE && t == "Expression") {
            // the Expression mixer: every face pose unit is a slider
            groupBox("Expression mixer");
            if (ImGui::Button("Neutral", ImVec2(-1, 0))) { std::fill(unitW.begin(), unitW.end(), 0.0f); expressionFile.clear(); dirty = true; }
            ImGui::BeginChild("units");
            for (size_t k = 1; k < unitNames.size(); ++k) {
                ImGui::PushID((int)k);
                ImGui::SetNextItemWidth(-1);
                if (ImGui::SliderFloat("##u", &unitW[k], 0, 1, (unitNames[k] + ": %.2f").c_str())) { if (prefs.realtime) dirty = true; }
                if (ImGui::IsItemActivated()) lookFor("faceCamera");
                if (ImGui::IsItemDeactivatedAfterEdit()) dirty = true;
                if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) { unitW[k] = 0; dirty = true; }
                ImGui::PopID();
            }
            ImGui::EndChild();
        }
    }
    ImGui::End();

    // the person in the middle
    ImGui::SetNextWindowPos(ImVec2(sideW, top + 34)); ImGui::SetNextWindowSize(ImVec2(W - 2 * sideW, H - top - 34 - statusH));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("view", nullptr, fixed);
    if (dirty && loadErr.empty()) {
        Human::apply(body);
        if (!rig.empty()) Skel::build(rig, body.pos);
        repose();
        Viewer::update(body);
        dirty = false;
    }
    const ImVec2 a = ImGui::GetCursorScreenPos(), size = ImGui::GetContentRegionAvail();
    ImGui::InvisibleButton("v", size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);
    const unsigned tex = Viewer::render((int)size.x, (int)size.y, ImGui::IsItemHovered() || ImGui::IsItemActive());
    ImGui::GetWindowDrawList()->AddImage((ImTextureID)(intptr_t)tex, a, ImVec2(a.x + size.x, a.y + size.y), ImVec2(0, 1), ImVec2(1, 0));
    if (wantThumb) {   // a picture of the view: the .thumb beside a saved model, or a screenshot
        std::vector<unsigned char> px; int w, h; Viewer::readPixels(px, w, h);
        if (!px.empty()) stbi_write_png(fs::u8path(thumbPath).string().c_str(), w, h, 4, px.data(), w * 4);
        pics.erase(thumbPath);
        wantThumb = false;
    }
    ImGui::End();
    ImGui::PopStyleVar();

    ImGui::SetNextWindowPos(ImVec2(0, H - statusH)); ImGui::SetNextWindowSize(ImVec2(W, statusH));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 4));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, col4(C_BG));
    ImGui::Begin("status", nullptr, fixed);
    if (now() < toastUntil) ImGui::TextColored(col4(C_PINK), "%s", toast.c_str());
    else ImGui::TextDisabled("%s, %.0f years. Drag to turn, right-drag to move, wheel to zoom; right-click a slider to reset it.", units(Human::heightCm(body)).c_str(), Human::ageYears(body));
    ImGui::End();
    ImGui::PopStyleColor(); ImGui::PopStyleVar();
}

static void progressCb(float, const char*);
static std::atomic<float> loadFrac{0};
static std::string loadWhat = "Starting...";
static std::mutex loadMtx;
static void progressCb(float f, const char* what) { loadFrac = f; std::lock_guard<std::mutex> g(loadMtx); loadWhat = what; }

int main(int argc, char** argv) {
    std::string capture, file, describeWords, startPose;
    std::vector<std::string> startUnits;   // "Name=weight"
    float startYaw = -1000;   // degrees, for a screenshot from a side
    std::vector<std::string> args;
#ifdef _WIN32
    { int n; LPWSTR* wa = CommandLineToArgvW(GetCommandLineW(), &n); if (wa) { for (int i = 1; i < n; ++i) args.push_back(fs::path(wa[i]).u8string()); LocalFree(wa); } }
#else
    for (int i = 1; i < argc; ++i) args.push_back(argv[i]);
#endif
    (void)argc; (void)argv;
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--capture" && i + 1 < args.size()) capture = args[++i];
        else if (args[i] == "--describe" && i + 1 < args.size()) describeWords = args[++i];
        else if (args[i] == "--tab" && i + 1 < args.size()) forcedTab = atoi(args[++i].c_str());
        else if (args[i] == "--task" && i + 1 < args.size()) forcedTask = args[++i];
        else if (args[i] == "--pose" && i + 1 < args.size()) startPose = args[++i];
        else if (args[i] == "--yaw" && i + 1 < args.size()) startYaw = (float)atof(args[++i].c_str());
        else if (args[i] == "--unit" && i + 1 < args.size()) startUnits.push_back(args[++i]);
        else file = args[i];
    }
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) return 1;
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    window = SDL_CreateWindow("Pygmalion", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1500, 900, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_HIDDEN);
    if (!window) return 1;
    SDL_SetWindowMinimumSize(window, 1150, 650);
    SDL_GLContext gl = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl);
    SDL_GL_SetSwapInterval(1);
    char* bp = SDL_GetBasePath();
    const std::string base = bp ? bp : "";
    if (bp) SDL_free(bp);
    SDL_Surface* icon = SDL_LoadBMP((base + "assets/icon.bmp").c_str());
    if (icon) SDL_SetWindowIcon(window, icon);
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    loadFont();
    applyStyle();
    SDL_Window* splashWin = nullptr;
    if (capture.empty()) {
        splashWin = SDL_CreateWindow("Pygmalion", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, (int)Splash::W, (int)Splash::H, SDL_WINDOW_OPENGL | SDL_WINDOW_BORDERLESS);
        if (splashWin && SDL_GL_MakeCurrent(splashWin, gl) != 0) { SDL_DestroyWindow(splashWin); splashWin = nullptr; SDL_GL_MakeCurrent(window, gl); }
        if (splashWin && icon) SDL_SetWindowIcon(splashWin, icon);
    }
    if (icon) SDL_FreeSurface(icon);
    ImGui_ImplSDL2_InitForOpenGL(splashWin ? splashWin : window, gl);
    ImGui_ImplOpenGL3_Init("#version 330");

    // the body and its 1,280 shapes load while the splash shows
    dataDir = (fs::u8path(base) / "data").u8string();
    { std::error_code ec; if (!fs::exists(fs::u8path(dataDir) / "3dobjs", ec)) dataDir = (fs::u8path(base) / ".." / ".." / "data").u8string(); }
    static std::atomic<bool> ready{false};
    std::thread([] { std::string err; if (!Human::load(body, dataDir, err, progressCb)) loadErr = "The body didn't load: " + err; ready = true; }).detach();
    if (splashWin) {
        const Uint32 start = SDL_GetTicks();
        for (;;) {
            SDL_Event e;
            while (SDL_PollEvent(&e)) { ImGui_ImplSDL2_ProcessEvent(&e); if (e.type == SDL_QUIT) quit = true; }
            const Uint32 t = SDL_GetTicks() - start;
            if (quit || (ready && t >= 1200)) break;
            ImGui_ImplOpenGL3_NewFrame(); ImGui_ImplSDL2_NewFrame(); ImGui::NewFrame();
            std::string what; { std::lock_guard<std::mutex> g(loadMtx); what = loadWhat; }
            Splash::draw(ImGui::GetBackgroundDrawList(), 0, 0, ready ? "Ready" : what.c_str(), ready ? 1.0f : (float)loadFrac);
            ImGui::Render();
            int sw, sh; SDL_GL_GetDrawableSize(splashWin, &sw, &sh);
            glViewport(0, 0, sw, sh); glClearColor(0.1f, 0.1f, 0.1f, 1); glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            SDL_GL_SwapWindow(splashWin);
        }
        ImGui_ImplSDL2_Shutdown();
        SDL_DestroyWindow(splashWin);
        SDL_GL_MakeCurrent(window, gl);
        ImGui_ImplSDL2_InitForOpenGL(window, gl);
    } else while (!ready) SDL_Delay(10);
    Viewer::init(dataDir);
    body.roots.clear();
    if (!assetsDir().empty()) body.roots.push_back(assetsDir());
    body.roots.push_back(dataDir);
    Human::defaultGear(body);
    { std::string err; if (!Skel::load(rig, (fs::u8path(dataDir) / "rigs" / "default.mhskel").u8string(), err)) say("No skeleton: " + err); }
    loadFaceUnits();
    category.clear();
    if (!file.empty()) { std::string err; if (!Human::loadMhm(body, file, err)) say(err); }
    if (!describeWords.empty()) { snprintf(prompt, sizeof prompt, "%s", describeWords.c_str()); understood = Human::describe(body, describeWords); }
    if (startYaw > -999) { Human::apply(body); Viewer::update(body); Viewer::focusBody(startYaw * 3.14159265f / 180, 0.05f); }
    if (!startPose.empty()) setPoseFile((fs::u8path(dataDir) / "poses" / (startPose + ".bvh")).u8string());
    for (const auto& u : startUnits) {
        const size_t eq = u.find('=');
        for (size_t k = 0; k < unitNames.size(); ++k) if (unitNames[k] == u.substr(0, eq)) unitW[k] = eq == std::string::npos ? 1.0f : (float)atof(u.c_str() + eq + 1);
    }
    dirty = true;
    if (capture.empty()) { SDL_ShowWindow(window); SDL_MaximizeWindow(window); SDL_RaiseWindow(window); }
    int frames = 0;
    while (!quit) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            ImGui_ImplSDL2_ProcessEvent(&e);
            if (e.type == SDL_QUIT) quit = true;
            if (e.type == SDL_DROPFILE) { std::string err; remember("Open"); if (Human::loadMhm(body, e.drop.file, err)) dirty = true; else say(err); SDL_free(e.drop.file); }
        }
        ImGui_ImplOpenGL3_NewFrame(); ImGui_ImplSDL2_NewFrame(); ImGui::NewFrame();
        frame();
        ImGui::Render();
        int w, h; SDL_GL_GetDrawableSize(window, &w, &h);
        glViewport(0, 0, w, h); glClearColor(0.1f, 0.1f, 0.1f, 1); glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        if (!capture.empty() && ++frames >= 20) {
            std::vector<unsigned char> px((size_t)w * h * 4);
            glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
            std::vector<unsigned char> flip(px.size());
            for (int y = 0; y < h; ++y) memcpy(&flip[(size_t)y * w * 4], &px[(size_t)(h - 1 - y) * w * 4], (size_t)w * 4);
            SDL_Surface* s = SDL_CreateRGBSurfaceWithFormatFrom(flip.data(), w, h, 32, w * 4, SDL_PIXELFORMAT_ABGR8888);
            SDL_SaveBMP(s, capture.c_str());
            SDL_FreeSurface(s);
            break;
        }
        SDL_GL_SwapWindow(window);
    }
    ImGui_ImplOpenGL3_Shutdown(); ImGui_ImplSDL2_Shutdown(); ImGui::DestroyContext();
    SDL_GL_DeleteContext(gl); SDL_DestroyWindow(window); SDL_Quit();
    return 0;
}
