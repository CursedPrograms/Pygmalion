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
#include "export.h"
#ifdef _WIN32
#include "http.h"
#include "socket.h"
#endif

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
// MakeHuman's own user data folder: anything put there (or installed by MakeHuman) shows in Pygmalion too
static std::string makehumanDataDir() {
    static std::string found; static bool looked = false;
    if (looked) return found;
    looked = true;
    PWSTR p = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, 0, nullptr, &p))) {
        const fs::path docs(p); CoTaskMemFree(p);
        std::error_code ec;
        for (const char* v : {"makehuman/v1py3/data", "makehuman/v1/data", "makehuman/v2/data"}) {
            const fs::path d = docs / fs::u8path(v);
            if (fs::is_directory(d, ec)) { found = d.u8string(); break; }
        }
    }
    return found;
}
#else
static std::string fileDialog(bool, const wchar_t*, const wchar_t*, const std::string&) { return ""; }
static std::string homeDir() { return "."; }
static std::string makehumanDataDir() { return ""; }
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
struct State {
    std::vector<float> mods, macros;
    std::vector<std::pair<std::string, std::string>> gear;   // each proxy's file and material
    std::string skin, pose, expression; bool hide = true;
    std::vector<float> units;
};
static std::vector<std::pair<State, std::string>> undo;   // the state before each change, and what changed
static void restorePose(const std::string& pose, const std::string& expression, const std::vector<float>& units);
static void getPose(std::string& pose, std::string& expression, std::vector<float>& units);
static float exportScale = 1.0f;   // MakeHuman's units are decimetres: 0.1 makes metres
static double now() { return SDL_GetTicks() / 1000.0; }
static std::vector<std::string> logs;   // Utilities > Logs: everything said in the status line
static void say(const std::string& s) {
    toast = s; toastUntil = now() + 6;
    char t[16]; const Uint32 ms = SDL_GetTicks(); snprintf(t, sizeof t, "%02u:%02u ", ms / 60000 % 60, ms / 1000 % 60);
    logs.push_back(t + s); if (logs.size() > 500) logs.erase(logs.begin());
}

static State snapshot() {
    State st;
    for (const auto& m : body.mods) st.mods.push_back(m.value);
    st.macros = {body.gender, body.age, body.muscle, body.weight, body.height, body.proportions, body.african, body.asian, body.caucasian, body.breastSize, body.breastFirmness};
    for (const auto& p : body.proxies) st.gear.push_back({p.file, p.material});
    st.skin = body.skinMaterial; st.hide = body.hideUnderClothes;
    getPose(st.pose, st.expression, st.units);
    return st;
}
static void restore(const State& st) {
    for (size_t i = 0; i < body.mods.size() && i < st.mods.size(); ++i) body.mods[i].value = st.mods[i];
    float* f[] = {&body.gender, &body.age, &body.muscle, &body.weight, &body.height, &body.proportions, &body.african, &body.asian, &body.caucasian, &body.breastSize, &body.breastFirmness};
    for (size_t i = 0; i < 11 && i < st.macros.size(); ++i) *f[i] = st.macros[i];
    // the gear only when it changed (loading proxies takes a moment)
    bool same = st.gear.size() == body.proxies.size();
    for (size_t i = 0; same && i < st.gear.size(); ++i) same = st.gear[i].first == body.proxies[i].file && st.gear[i].second == body.proxies[i].material;
    if (!same) {
        body.proxies.clear();
        std::string err;
        for (const auto& [file, mat] : st.gear) {
            if (!Human::wear(body, file, err)) continue;
            for (auto& p : body.proxies) if (p.file == file && !mat.empty() && p.material != mat) { p.material = mat; Human::loadMaterial(mat, p.texture, p.transparent); }
        }
    }
    if (st.skin != body.skinMaterial) Human::setSkin(body, st.skin);
    body.hideUnderClothes = st.hide;
    restorePose(st.pose, st.expression, st.units);
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
// the socket server's settings (Utilities > Socket), kept with the rest
struct SockCfg { bool accept = false, advanced = false; std::string host = "127.0.0.1"; int port = 12345; };
static SockCfg sockCfg;
static char sockHost[64] = "127.0.0.1", sockPort[16] = "12345";
static bool socketArg = false;
struct Prefs {
    bool sliderImages = true;      // the shape's picture beside each slider
    bool showValues = true;        // the number on the slider
    bool camFollows = true;        // the view turns to what a slider changes
    bool realtime = true;          // the body follows the slider as it moves (else when let go)
    bool metric = true;            // cm (else inches)
    bool invertMouse = false;
};
static Prefs prefs;
static std::string settingsFile() { return (fs::u8path(homeDir()) / "settings.ini").u8string(); }
static void saveSettings() {
    std::ofstream o(fs::u8path(settingsFile()));
    if (!o) return;
    const Viewer::Look& lk = Viewer::look();
    o << "sliderImages=" << prefs.sliderImages << "\nshowValues=" << prefs.showValues << "\ncamFollows=" << prefs.camFollows
      << "\nrealtime=" << prefs.realtime << "\nmetric=" << prefs.metric
      << "\nturnSpeed=" << lk.turnSpeed << "\nmoveSpeed=" << lk.moveSpeed << "\nzoomSpeed=" << lk.zoomSpeed
      << "\ninvertTurn=" << lk.invertTurn << "\ninvertZoom=" << lk.invertZoom
      << "\nbgImage=" << lk.bgImage << "\nbgOpacity=" << lk.bgOpacity << "\n";
    o << "socketAccept=" << sockCfg.accept << "\nsocketAdvanced=" << sockCfg.advanced << "\nsocketHost=" << sockCfg.host << "\nsocketPort=" << sockCfg.port << "\n";
}
static void loadSettings() {
    std::ifstream in(fs::u8path(settingsFile())); std::string l;
    Viewer::Look& lk = Viewer::look();
    while (std::getline(in, l)) {
        if (!l.empty() && l.back() == '\r') l.pop_back();
        const size_t eq = l.find('='); if (eq == std::string::npos) continue;
        const std::string k = l.substr(0, eq), v = l.substr(eq + 1); const float f = (float)atof(v.c_str()); const bool b = v == "1";
        if (k == "sliderImages") prefs.sliderImages = b; else if (k == "showValues") prefs.showValues = b;
        else if (k == "camFollows") prefs.camFollows = b; else if (k == "realtime") prefs.realtime = b; else if (k == "metric") prefs.metric = b;
        else if (k == "turnSpeed") lk.turnSpeed = std::clamp(f, 0.1f, 5.0f); else if (k == "moveSpeed") lk.moveSpeed = std::clamp(f, 0.1f, 5.0f);
        else if (k == "zoomSpeed") lk.zoomSpeed = std::clamp(f, 0.1f, 5.0f);
        else if (k == "invertTurn") lk.invertTurn = b; else if (k == "invertZoom") lk.invertZoom = b;
        else if (k == "bgImage") { std::error_code ec; if (!v.empty() && fs::exists(fs::u8path(v), ec)) lk.bgImage = v; }
        else if (k == "bgOpacity") lk.bgOpacity = std::clamp(f, 0.0f, 1.0f);
        else if (k == "socketAccept") sockCfg.accept = b; else if (k == "socketAdvanced") sockCfg.advanced = b;
        else if (k == "socketHost" && !v.empty()) { sockCfg.host = v; snprintf(sockHost, sizeof sockHost, "%s", v.c_str()); }
        else if (k == "socketPort") { sockCfg.port = std::clamp(atoi(v.c_str()), 1, 65535); snprintf(sockPort, sizeof sockPort, "%d", sockCfg.port); }
    }
}
static std::vector<std::pair<State, std::string>> redo;

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
    // MakeHuman's condition: the genital sliders work on a topology that has genitals
    const bool off = m.enabledCondition == "hasGenitals" && !Human::hasGenitals(body);
    ImGui::PushID(i);
    if (off) ImGui::BeginDisabled();
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
    if (off) { ImGui::EndDisabled(); if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("Wear a topology with genitals first (Geometries > Topologies): MakeHuman's community has them to download."); }
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

static std::string userDataDir() {   // MakeHuman's "data" in the user's own folder: downloaded packs go here
    const std::string h = homeDir();
    if (h.empty()) return "";
    std::error_code ec; fs::create_directories(fs::u8path(h) / "data", ec);
    return (fs::u8path(h) / "data").u8string();
}
static std::vector<std::string> filesIn(const std::string& sub, const char* ext) {
    std::vector<std::string> v;
    std::error_code ec;
    for (const std::string& root : {dataDir, assetsDir(), userDataDir(), makehumanDataDir()})
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
static Skel::Skeleton chosen;                   // the chosen skeleton: exported, and shown with Show the bones
static Skel::Pose currentPose;                  // what the body is posed with now (empty: the rest)
static void repose() {
    if (rig.empty()) { body.shown.clear(); return; }
    bool anyUnit = false;
    for (size_t k = 1; k < unitW.size(); ++k) if (unitW[k] > 0) anyUnit = true;
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
    currentPose = p;
    const bool posed = !p.empty();
    Skel::setPose(rig, posed ? &p : nullptr);
    if (!chosen.empty()) { if (Skel::usesReferences(chosen)) Skel::retarget(chosen, rig); else Skel::setPose(chosen, posed ? &p : nullptr); }
    if (posed) Skel::skin(rig, body.pos, body.shown); else body.shown.clear();
    std::vector<float> lines;
    Skel::boneLines(chosen.empty() ? rig : chosen, lines);
    Viewer::setBones(lines);
}
static void setSkeletonFile(const std::string& f) {
    skeletonFile = f;
    chosen = Skel::Skeleton();
    if (!f.empty()) {
        std::string err;
        if (Skel::load(chosen, f, err)) Skel::inheritWeights(chosen, rig);
        else { say(err); skeletonFile.clear(); chosen = Skel::Skeleton(); }
    }
    dirty = true;
}

static void setPoseFile(const std::string& f) {
    poseFile = f;
    bodyPose = Skel::Pose();
    if (!f.empty()) {
        std::vector<Skel::Pose> fr; std::string err;
        if (!rig.empty()) { Human::apply(body); Skel::build(rig, body.pos); }
        if (Skel::loadBvh(f, fr, err, nullptr, rig.empty() ? nullptr : &rig)) bodyPose = fr[0]; else say(err);
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

static void getPose(std::string& pose, std::string& expression, std::vector<float>& units) { pose = poseFile; expression = expressionFile; units = unitW; }
static void restorePose(const std::string& pose, const std::string& expression, const std::vector<float>& units) {
    if (pose != poseFile) setPoseFile(pose);
    expressionFile = expression;
    if (units.size() == unitW.size()) unitW = units;
}

// ---------- Pose/Animate > Animations: MakeHuman's BVH animations played on the body ----------
static std::vector<Skel::Pose> animFrames; static std::string animFile;
static float animFrameTime = 1 / 24.0f, animFrame = 0, animSpeed = 1; static bool animPlaying = false;
static void setAnimation(const std::string& f) {
    animFile = f; animFrames.clear(); animPlaying = false; animFrame = 0;
    if (f.empty()) { bodyPose = Skel::Pose(); poseFile.clear(); dirty = true; return; }
    std::string err;
    if (!rig.empty()) { Human::apply(body); Skel::build(rig, body.pos); }
    if (!Skel::loadBvh(f, animFrames, err, &animFrameTime, rig.empty() ? nullptr : &rig)) { say(err); animFile.clear(); return; }
    bodyPose = animFrames[0]; poseFile.clear(); animPlaying = true; dirty = true;
}
static void animate(float dt) {
    if (!animPlaying || animFrames.empty()) return;
    animFrame += dt * animSpeed / std::max(animFrameTime, 1e-3f);
    while (animFrame >= animFrames.size()) animFrame -= (float)animFrames.size();
    bodyPose = animFrames[(size_t)animFrame];
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
    case CAT_POSE: return {"Skeleton", "Pose", "Expression", "Animations"};
    case CAT_RENDERING: return {"Render", "Scene", "Viewer"};
    case CAT_SETTINGS: return {"Sliders", "General", "Background", "Mouse", "Shortcuts"};
    case CAT_UTILITIES: return {"Describe", "Symmetry", "Statistics", "Material Editor", "Expression mixer", "Targets", "Save targets", "Mass produce", "Shell", "Scripting", "Download", "Socket", "Data", "Logs"};
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
static std::string relData(const std::string& f) { return f.empty() ? std::string() : Human::relPath(body, f); }
static std::string findData(const std::string& rel, const char* sub) {
    if (rel.empty()) return "";
    { const size_t k = rel.find('/'); const std::string tail = k == std::string::npos ? rel : rel.substr(k + 1); if (fs::u8path(tail).is_absolute()) return tail; }
    std::string f = Human::absPath(body, rel);
    if (f.empty()) for (const auto& c : filesIn(sub, fs::u8path(rel).extension().u8string().c_str())) if (fs::u8path(c).filename() == fs::u8path(rel).filename()) return c;
    return f;
}
static bool saveModel(const std::string& path, const std::string& name, std::string& err) {
    if (!Human::saveMhm(body, path, name, err)) return false;
    std::ofstream o(fs::u8path(path), std::ios::app);
    if (!skeletonFile.empty()) o << "skeleton " << fs::u8path(skeletonFile).filename().u8string() << "\n";
    if (!poseFile.empty()) o << "pose " << relData(poseFile) << "\n";
    if (!expressionFile.empty()) o << "expression " << relData(expressionFile) << "\n";
    return true;
}
static bool loadModel(const std::string& path, std::string& err) {
    if (!Human::loadMhm(body, path, err)) return false;
    std::ifstream in(fs::u8path(path)); std::string line;
    std::string sk, po, ex;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::istringstream ls(line); std::string k, v; ls >> k; std::getline(ls >> std::ws, v);
        if (k == "skeleton") sk = v; else if (k == "pose") po = v; else if (k == "expression") ex = v;
    }
    setSkeletonFile(sk.empty() ? std::string() : findData("rigs/" + sk, "rigs"));
    setPoseFile(findData(po, "poses"));
    if (!ex.empty()) setExpressionFile(findData(ex, "expressions")); else { expressionFile.clear(); std::fill(unitW.begin(), unitW.end(), 0.0f); }
    animFrames.clear(); animFile.clear(); animPlaying = false;
    return true;
}
static char saveName[128] = "person";
static int exportFormat = 0; static int exportUnits = 1; static bool exportFeet = true, exportPosed = true, exportAnim = false;
enum { EX_DAE, EX_FBX, EX_GLB, EX_OBJ, EX_OGRE, EX_STL, EX_BVH, EX_UV, EX_COUNT };
static const char* EXPORT_NAMES[] = {"Collada (dae)", "Filmbox (fbx)", "glTF binary (glb)", "Wavefront obj", "Ogre3D", "Stereolithography (stl)", "Biovision Hierarchy (bvh)", "UV map (png)"};
static const char* EXPORT_EXT[] = {".dae", ".fbx", ".glb", ".obj", ".mesh.xml", ".stl", ".bvh", ".png"};
static const char* UNIT_NAMES[] = {"decimeter", "meter", "centimeter", "millimeter", "inch"};
static const float UNIT_SCALE[] = {1, 0.1f, 10, 100, 3.93700787f};
static const float UNIT_CM[] = {10, 100, 1, 0.1f, 2.54f};
static Mesh toMesh();
static bool exportTo(const std::string& p, int fmt, std::string& err) {
    const float sc = UNIT_SCALE[exportUnits];
    if (fmt == EX_GLB || fmt == EX_STL) {
        const Mesh m = toMesh();
        return fmt == EX_GLB ? saveGlb(m, p, sc, false, err) : saveStl(m, p, sc, false, err);
    }
    if (fmt == EX_OBJ) {   // MakeHuman's OBJ: shared vertices and its quads, as posed now
        Export::Options o; o.scale = sc; o.cmPerUnit = UNIT_CM[exportUnits]; o.feetOnGround = exportFeet; o.posed = exportPosed;
        return Export::saveObj(Export::withTexturesBeside(Export::parts(body, nullptr, exportPosed), p), p, o, err);
    }
    if (fmt == EX_UV) return Export::saveUvMap(body, p, 2048, err);
    if (fmt == EX_BVH) {
        const Skel::Skeleton& sk = chosen.empty() ? rig : chosen;
        if (sk.empty()) { err = "No skeleton to export"; return false; }
        std::vector<Skel::Pose> frames;
        if (exportAnim && !animFrames.empty()) frames = animFrames;
        else if (!currentPose.empty()) frames.push_back(currentPose);
        const bool ok = Skel::saveBvh(sk, frames, p, sc, animFrameTime, err, &sk != &rig && Skel::usesReferences(sk) ? &rig : nullptr);
        repose();   // (saving walked the skeleton through the frames)
        return ok;
    }
    Export::Options o; o.scale = sc; o.cmPerUnit = UNIT_CM[exportUnits]; o.feetOnGround = exportFeet; o.posed = exportPosed;
    const Skel::Skeleton* sk = chosen.empty() ? nullptr : &chosen;
    const auto parts = Export::withTexturesBeside(Export::parts(body, sk, exportPosed && fmt != EX_FBX), p);
    return fmt == EX_DAE ? Export::saveCollada(parts, sk, p, o, err) : fmt == EX_FBX ? Export::saveFbx(parts, sk, p, o, err) : Export::saveOgre(parts, sk, p, o, err);
}
static bool wantThumb = false; static std::string thumbPath;

static void filesLeft(float w) {
    if (task[CAT_FILES] == "Load") {
        groupBox("Load a model");
        ImGui::TextDisabled("Models in %s", homeDir().c_str());
        if (ImGui::Button("Load from another folder...", ImVec2(w, 0))) {
            const std::string p = fileDialog(false, L"MakeHuman model\0*.mhm\0\0", L"mhm", homeDir() + "\\");
            std::string err;
            if (!p.empty()) { remember("Load"); if (loadModel(p, err)) { dirty = true; say("Loaded " + fs::u8path(p).filename().u8string()); } else say(err); }
        }
        for (const auto& f : modelFiles()) {
            const std::string name = fs::u8path(f).stem().u8string();
            const GLuint th = picture(fs::u8path(f).replace_extension(".thumb").u8string());
            ImGui::PushID(f.c_str());
            if (th) { if (ImGui::ImageButton("t", (ImTextureID)(intptr_t)th, ImVec2(64, 64))) { std::string err; remember("Load"); if (loadModel(f, err)) dirty = true; else say(err); } ImGui::SameLine(); }
            if (ImGui::Button(name.c_str(), ImVec2(th ? w - 80 : w, th ? 72 : 0))) { std::string err; remember("Load"); if (loadModel(f, err)) dirty = true; else say(err); }
            ImGui::PopID();
        }
    } else if (task[CAT_FILES] == "Save") {
        groupBox("Save the model");
        ImGui::SetNextItemWidth(w); ImGui::InputText("##name", saveName, sizeof saveName);
        if (ImGui::Button("Save", ImVec2(w, 0))) {
            const std::string p = (fs::u8path(homeDir()) / (std::string(saveName) + ".mhm")).u8string();
            std::string err;
            if (saveModel(p, saveName, err)) { say("Saved " + p + " (MakeHuman opens it too)"); wantThumb = true; thumbPath = fs::u8path(p).replace_extension(".thumb").u8string(); }
            else say(err);
        }
        if (ImGui::Button("Save as...", ImVec2(w, 0))) {
            const std::string p = fileDialog(true, L"MakeHuman model\0*.mhm\0\0", L"mhm", homeDir() + "\\" + saveName + ".mhm");
            std::string err;
            if (!p.empty()) { if (saveModel(p, fs::u8path(p).stem().u8string(), err)) { say("Saved"); wantThumb = true; thumbPath = fs::u8path(p).replace_extension(".thumb").u8string(); } else say(err); }
        }
        ImGui::TextDisabled("A picture of the view is saved beside it (.thumb), as MakeHuman does.");
    } else {
        groupBox("Export");
        ImGui::SetNextItemWidth(w); ImGui::InputText("##ename", saveName, sizeof saveName);
        if (ImGui::Button("Export...", ImVec2(w, 0))) {
            std::wstring filter = L"File\0*", ext;
            const std::string dot = EXPORT_EXT[exportFormat];
            for (char c : dot) filter += (wchar_t)c;
            filter += L"\0\0";
            for (char c : dot.substr(dot.rfind('.') + 1)) ext += (wchar_t)c;
            const std::string p = fileDialog(true, filter.c_str(), ext.c_str(), homeDir() + "\\" + saveName + dot);
            if (!p.empty()) { std::string err; say(exportTo(p, exportFormat, err) ? "Exported " + fs::u8path(p).filename().u8string() : err); }
        }
        ImGui::PushTextWrapPos(w);
        if (exportFormat == EX_DAE || exportFormat == EX_FBX || exportFormat == EX_OGRE)
            ImGui::TextDisabled(chosen.empty() ? "No skeleton is chosen (Pose/Animate > Skeleton): the mesh goes without one." : "With the %s skeleton and its weights; textures are copied beside it.", chosen.name.c_str());
        else if (exportFormat == EX_BVH) ImGui::TextDisabled("The %s skeleton and the current pose (or the animation).", chosen.empty() ? "default" : chosen.name.c_str());
        else if (exportFormat == EX_UV) ImGui::TextDisabled("The body's UV layout as a 2048 x 2048 picture, for painting skins.");
        ImGui::PopTextWrapPos();
    }
}
static void filesRight() {
    if (task[CAT_FILES] != "Export") return;
    groupBox("Mesh format");
    for (int k = 0; k < EX_COUNT; ++k) ImGui::RadioButton(EXPORT_NAMES[k], &exportFormat, k);
    groupBox("Options");
    ImGui::TextUnformatted("Scale units");
    ImGui::SetNextItemWidth(-1); ImGui::Combo("##units", &exportUnits, UNIT_NAMES, 5);
    if (exportFormat == EX_DAE || exportFormat == EX_FBX || exportFormat == EX_OGRE) {
        ImGui::Checkbox("Feet on ground", &exportFeet);
        ImGui::Checkbox("In the current pose", &exportPosed);
    }
    if (exportFormat == EX_BVH) ImGui::Checkbox("The whole animation", &exportAnim);
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

static float toolbarH = 34;   // one row of buttons, or more when the window is narrow
static void toolbar(float x, float y, float w) {
    ImGui::SetNextWindowPos(ImVec2(x, y)); ImGui::SetNextWindowSize(ImVec2(w, toolbarH));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 5));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, col4(C_BG));
    ImGui::Begin("toolbar", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus);
    auto btn = [&](const char* l, const char* t, bool on = false) {
        // a row full: the next one below (the toolbar grows to fit)
        const float bw = ImGui::CalcTextSize(l).x + ImGui::GetStyle().FramePadding.x * 2;
        if (ImGui::GetCursorPosX() + bw > ImGui::GetWindowContentRegionMax().x) ImGui::NewLine();
        if (on) ImGui::PushStyleColor(ImGuiCol_Button, col4(C_DEEP)); const bool r = ImGui::SmallButton(l); if (on) ImGui::PopStyleColor(); tip(t); ImGui::SameLine(); return r; };
    auto gap = [] { ImGui::SameLine(0, 12); };
    if (btn("Undo", "Undo (Ctrl+Z)") && !undo.empty()) { redo.push_back({snapshot(), undo.back().second}); restore(undo.back().first); undo.pop_back(); }
    if (btn("Redo", "Redo (Ctrl+Y)") && !redo.empty()) { undo.push_back({snapshot(), redo.back().second}); restore(redo.back().first); redo.pop_back(); }
    if (btn("Reset", "Reset the human to the start")) { remember("Reset"); Human::resetAll(body); Human::defaultGear(body); dirty = true; }
    gap();
    Viewer::Look& lk = Viewer::look();
    if (btn("Wire", "Show the mesh's lines", lk.wire)) lk.wire = !lk.wire;
    if (btn("Clay", "Plain grey instead of skin", lk.shading == 1)) lk.shading = lk.shading == 1 ? 0 : 1;
    if (btn("Floor", "The grid under the feet", lk.grid)) lk.grid = !lk.grid;
    if (btn("Bones", "Show the skeleton", lk.bones)) { lk.bones = !lk.bones; showBones = lk.bones; }
    if (btn("Ortho", "Orthographic view (no perspective)", lk.ortho)) lk.ortho = !lk.ortho;
    if (btn("Smooth", "Smooth the mesh (subdivided once, as MakeHuman's Smooth)", lk.smooth)) { lk.smooth = !lk.smooth; dirty = true; }
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
    if (btn("Shot", "Save a picture of the view (F12)") || (!ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_F12, false))) {
        const std::string p = fileDialog(true, L"PNG\0*.png\0\0", L"png", homeDir() + "\\screenshot.png");
        if (!p.empty()) { wantThumb = true; thumbPath = p; }
    }
    ImGui::NewLine();
    toolbarH = std::max(34.0f, ImGui::GetCursorPosY() + 2);
    ImGui::End();
    ImGui::PopStyleColor(); ImGui::PopStyleVar();
}

// ---------- Rendering: Render, Scene, Viewer ----------
static int renderW = 1920, renderH = 1080, renderSS = 2; static bool renderTransparent = false;
static std::vector<unsigned char> rendered; static int renderedW = 0, renderedH = 0; static GLuint renderedTex = 0;
static bool renderNow(int w, int h) {
    Viewer::Look& lk = Viewer::look();
    const bool was = lk.transparent;
    lk.transparent = renderTransparent;
    const bool ok = Viewer::renderImage(w, h, renderSS, rendered);
    lk.transparent = was;
    if (!ok) return false;
    renderedW = w; renderedH = h;
    if (!renderedTex) glGenTextures(1, &renderedTex);
    glBindTexture(GL_TEXTURE_2D, renderedTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rendered.data());
    return true;
}
static bool saveRendered(const std::string& p) {
    return !rendered.empty() && stbi_write_png(fs::u8path(p).string().c_str(), renderedW, renderedH, 4, rendered.data(), renderedW * 4) != 0;
}
// ---------- MakeHuman's scenes: ambient light, shine and lights, kept as .pygscene (JSON) ----------
static const float DEG = 3.14159265f / 180;
static void scenePreset(int k) {
    Viewer::Look& lk = Viewer::look();
    using L = Viewer::Look::Light;
    lk.lights.clear();
    lk.sceneSkin = k != 0;   // MakeHuman's own scene keeps its painted skin; the others light it
    switch (k) {
    case 0:   // MakeHuman's default: a light from the eye
        lk.ambient[0] = lk.ambient[1] = lk.ambient[2] = 0.38f; lk.shine = 0.04f; lk.shininess = 24;
        lk.lights.push_back(L{true, 0, 0, {1, 1, 1}, 0.72f, true}); break;
    case 1:   // studio: key, fill and rim, fixed round the person
        lk.ambient[0] = lk.ambient[1] = lk.ambient[2] = 0.18f; lk.shine = 0.12f; lk.shininess = 30;
        lk.lights.push_back(L{true, -40 * DEG, 30 * DEG, {1, 0.97f, 0.92f}, 0.85f, false});
        lk.lights.push_back(L{true, 50 * DEG, 5 * DEG, {0.85f, 0.9f, 1}, 0.35f, false});
        lk.lights.push_back(L{true, 160 * DEG, 25 * DEG, {1, 1, 1}, 0.6f, false}); break;
    case 2:   // soft: lots of ambient, two gentle lights
        lk.ambient[0] = lk.ambient[1] = lk.ambient[2] = 0.5f; lk.shine = 0.02f; lk.shininess = 12;
        lk.lights.push_back(L{true, -30 * DEG, 20 * DEG, {1, 1, 1}, 0.4f, true});
        lk.lights.push_back(L{true, 30 * DEG, 20 * DEG, {1, 1, 1}, 0.3f, true}); break;
    case 3:   // dramatic: one hard light from the side, rim from behind
        lk.ambient[0] = lk.ambient[1] = lk.ambient[2] = 0.06f; lk.shine = 0.2f; lk.shininess = 40;
        lk.lights.push_back(L{true, -80 * DEG, 15 * DEG, {1, 0.93f, 0.85f}, 1.1f, false});
        lk.lights.push_back(L{true, 150 * DEG, 30 * DEG, {0.7f, 0.8f, 1}, 0.7f, false}); break;
    default:  // sunset: warm low sun, cool sky
        lk.ambient[0] = 0.2f; lk.ambient[1] = 0.22f; lk.ambient[2] = 0.32f; lk.shine = 0.08f; lk.shininess = 20;
        lk.lights.push_back(L{true, -60 * DEG, 8 * DEG, {1.2f, 0.75f, 0.45f}, 0.9f, false});
        lk.lights.push_back(L{true, 90 * DEG, 60 * DEG, {0.5f, 0.6f, 0.9f}, 0.35f, false}); break;
    }
}
static void rgbSliders(const char* id, float* c, float maxV, float lw) {   // a colour as three sliders
    static const char* n[] = {"Red %.2f", "Green %.2f", "Blue %.2f"};
    ImGui::PushID(id);
    for (int k = 0; k < 3; ++k) { ImGui::PushID(k); ImGui::SetNextItemWidth(lw); ImGui::SliderFloat("##c", &c[k], 0, maxV, n[k]); ImGui::PopID(); }
    ImGui::PopID();
}
static const char* SCENE_PRESETS[] = {"MakeHuman", "Studio", "Soft", "Dramatic", "Sunset"};
static bool saveScene(const std::string& path) {
    const Viewer::Look& lk = Viewer::look();
    std::ofstream o(fs::u8path(path));
    if (!o) return false;
    o << "{\n  \"ambient\": [" << lk.ambient[0] << ", " << lk.ambient[1] << ", " << lk.ambient[2] << "],\n  \"shine\": " << lk.shine
      << ",\n  \"shininess\": " << lk.shininess << ",\n  \"brightness\": " << lk.light << ",\n  \"skinLit\": " << (lk.sceneSkin ? "true" : "false")
      << ",\n  \"background\": [" << lk.bg[0] << ", " << lk.bg[1] << ", " << lk.bg[2] << "],\n  \"lights\": [";
    for (size_t i = 0; i < lk.lights.size(); ++i) {
        const auto& L = lk.lights[i];
        o << (i ? ",\n" : "\n") << "    {\"on\": " << (L.on ? "true" : "false") << ", \"turn\": " << L.yaw / DEG << ", \"height\": " << L.pitch / DEG
          << ", \"colour\": [" << L.col[0] << ", " << L.col[1] << ", " << L.col[2] << "], \"strength\": " << L.strength
          << ", \"withView\": " << (L.withView ? "true" : "false") << "}";
    }
    o << "\n  ]\n}\n";
    return (bool)o;
}
static bool loadScene(const std::string& path, std::string& err) {
    std::ifstream in(fs::u8path(path)); if (!in) { err = "Couldn't open " + path; return false; }
    std::ostringstream ss; ss << in.rdbuf(); const std::string t = ss.str();
    Json::Value v; size_t i = 0;
    if (!Json::parse(t, i, v) || v.type != Json::Value::OBJ) { err = "Not a scene: " + path; return false; }
    Viewer::Look& lk = Viewer::look();
    auto three = [](const Json::Value* a, float* out) { if (a && a->type == Json::Value::ARR && a->a.size() == 3) for (int c = 0; c < 3; ++c) out[c] = (float)a->a[(size_t)c].n; };
    auto flag = [](const Json::Value* b, bool def) { return b && b->type == Json::Value::BOOL ? b->b : def; };
    three(v.get("ambient"), lk.ambient); three(v.get("background"), lk.bg);
    lk.shine = (float)v.num("shine", lk.shine); lk.shininess = (float)v.num("shininess", lk.shininess); lk.light = (float)v.num("brightness", lk.light);
    lk.sceneSkin = flag(v.get("skinLit"), false);
    if (const Json::Value* ls = v.get("lights")) {
        lk.lights.clear();
        for (const auto& l : ls->a) {
            if (lk.lights.size() == 4) break;
            Viewer::Look::Light L;
            L.on = flag(l.get("on"), true); L.yaw = (float)l.num("turn") * DEG; L.pitch = (float)l.num("height") * DEG;
            three(l.get("colour"), L.col); L.strength = (float)l.num("strength", 1); L.withView = flag(l.get("withView"), false);
            lk.lights.push_back(L);
        }
    }
    return true;
}

static void renderingLeft(const std::string& t, float lw) {
    Viewer::Look& lk = Viewer::look();
    if (t == "Render") {
        groupBox("Resolution");
        ImGui::SetNextItemWidth(lw); ImGui::SliderInt("##rw", &renderW, 64, 8192, "Width %d px");
        ImGui::SetNextItemWidth(lw); ImGui::SliderInt("##rh", &renderH, 64, 8192, "Height %d px");
        const struct { const char* n; int w, h; } presets[] = {{"HD", 1280, 720}, {"Full HD", 1920, 1080}, {"4K", 3840, 2160}, {"Square", 2048, 2048}, {"Portrait", 1080, 1920}};
        for (int k = 0; k < 5; ++k) { if (k) ImGui::SameLine(); if (ImGui::SmallButton(presets[k].n)) { renderW = presets[k].w; renderH = presets[k].h; } }
        groupBox("Quality");
        ImGui::SetNextItemWidth(lw); ImGui::SliderInt("##ss", &renderSS, 1, 4, "Anti-aliasing %dx");
        ImGui::Checkbox("Transparent background", &renderTransparent);
        if (ImGui::Button("Render", ImVec2(lw, 34))) {
            if (renderNow(renderW, renderH)) { say("Rendered " + std::to_string(renderW) + " x " + std::to_string(renderH) + " (Rendering > Viewer)"); forcedTask = "Viewer"; }
            else say("That size is too big for this graphics card");
        }
        ImGui::TextDisabled("The view as it is now, at this size.");
    } else if (t == "Scene") {
        groupBox("Scene");
        int pre = -1;
        ImGui::SetNextItemWidth(lw);
        if (ImGui::BeginCombo("##pre", "A ready-made scene...")) {
            for (int k = 0; k < 5; ++k) if (ImGui::Selectable(SCENE_PRESETS[k])) pre = k;
            ImGui::EndCombo();
        }
        if (pre >= 0) { scenePreset(pre); say(std::string(SCENE_PRESETS[pre]) + " scene"); }
        if (ImGui::Button("Load...", ImVec2(lw / 2 - 4, 0))) {
            const std::string p = fileDialog(false, L"Pygmalion scene\0*.pygscene\0\0", L"pygscene", homeDir() + "\\");
            if (!p.empty()) { std::string err; say(loadScene(p, err) ? "Scene " + fs::u8path(p).stem().u8string() : err); }
        }
        ImGui::SameLine();
        if (ImGui::Button("Save...", ImVec2(lw / 2 - 4, 0))) {
            const std::string p = fileDialog(true, L"Pygmalion scene\0*.pygscene\0\0", L"pygscene", homeDir() + "\\scene.pygscene");
            if (!p.empty()) say(saveScene(p) ? "Saved " + p : "Couldn't save " + p);
        }
        groupBox("Ambient light");
        rgbSliders("amb", lk.ambient, 1.5f, lw);
        ImGui::SetNextItemWidth(lw); ImGui::SliderFloat("##sh", &lk.shine, 0, 1, "Shine %.2f");
        ImGui::SetNextItemWidth(lw); ImGui::SliderFloat("##shn", &lk.shininess, 2, 128, "Sharpness %.0f", ImGuiSliderFlags_Logarithmic);
        ImGui::Checkbox("Light the skin by the scene", &lk.sceneSkin);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("MakeHuman's skin has its light painted in (a litsphere). On, the scene's lights shade it instead.");
        int drop = -1;
        for (size_t i = 0; i < lk.lights.size(); ++i) {
            auto& L = lk.lights[i];
            ImGui::PushID(100 + (int)i);
            groupBox(("Light " + std::to_string(i + 1)).c_str());
            ImGui::Checkbox("On", &L.on); ImGui::SameLine(); ImGui::Checkbox("Turns with the view", &L.withView);
            float deg[2] = {L.yaw / DEG, L.pitch / DEG};
            ImGui::SetNextItemWidth(lw); if (ImGui::SliderFloat("##y", &deg[0], -180, 180, L.withView ? "From the left/right %.0f deg" : "Round the person %.0f deg")) L.yaw = deg[0] * DEG;
            ImGui::SetNextItemWidth(lw); if (ImGui::SliderFloat("##p", &deg[1], -89, 89, "Height %.0f deg")) L.pitch = deg[1] * DEG;
            ImGui::SetNextItemWidth(lw); ImGui::SliderFloat("##s", &L.strength, 0, 3, "Strength %.2f");
            rgbSliders("lc", L.col, 1.5f, lw);
            if (lk.lights.size() > 1 && ImGui::SmallButton("Remove this light")) drop = (int)i;
            ImGui::PopID();
        }
        if (drop >= 0) lk.lights.erase(lk.lights.begin() + drop);
        if (lk.lights.size() < 4 && ImGui::Button("Add a light", ImVec2(lw, 0))) lk.lights.push_back(Viewer::Look::Light{true, 45 * DEG, 30 * DEG, {1, 1, 1}, 0.5f, false});
        groupBox("Overall");
        ImGui::SetNextItemWidth(lw); ImGui::SliderFloat("##li", &lk.light, 0.1f, 2.5f, "Brightness %.2f");
        for (int c = 0; c < 3; ++c) { static const char* n[] = {"Red %.2f", "Green %.2f", "Blue %.2f"}; ImGui::PushID(c); ImGui::SetNextItemWidth(lw); ImGui::SliderFloat("##lc", &lk.lightCol[c], 0, 1.5f, n[c]); ImGui::PopID(); }
        ImGui::TextDisabled("Tint");
        if (ImGui::SmallButton("White")) lk.lightCol[0] = lk.lightCol[1] = lk.lightCol[2] = 1;
        ImGui::SameLine(); if (ImGui::SmallButton("Warm")) { lk.lightCol[0] = 1.1f; lk.lightCol[1] = 0.98f; lk.lightCol[2] = 0.85f; }
        ImGui::SameLine(); if (ImGui::SmallButton("Cold")) { lk.lightCol[0] = 0.88f; lk.lightCol[1] = 0.96f; lk.lightCol[2] = 1.12f; }
        groupBox("Background");
        for (int c = 0; c < 3; ++c) { static const char* n[] = {"Red %.2f", "Green %.2f", "Blue %.2f"}; ImGui::PushID(10 + c); ImGui::SetNextItemWidth(lw); ImGui::SliderFloat("##bg", &lk.bg[c], 0, 1, n[c]); ImGui::PopID(); }
        groupBox("Camera");
        ImGui::Checkbox("Orthographic", &lk.ortho);
    } else {
        groupBox("Viewer");
        if (rendered.empty()) { notYet("Nothing rendered yet: Rendering > Render makes a picture, shown here."); return; }
        ImGui::Text("%d x %d", renderedW, renderedH);
        if (ImGui::Button("Save as...", ImVec2(lw, 0))) {
            const std::string p = fileDialog(true, L"PNG\0*.png\0\0", L"png", homeDir() + "\\render.png");
            if (!p.empty()) say(saveRendered(p) ? "Saved " + p : "Couldn't save " + p);
        }
    }
}

// ---------- Settings > Background: a picture behind the person ----------
static void backgroundLeft(float lw) {
    Viewer::Look& lk = Viewer::look();
    groupBox("Background picture");
    if (ImGui::Button("Choose a picture...", ImVec2(lw, 0))) {
        const std::string p = fileDialog(false, L"Pictures\0*.png;*.jpg;*.jpeg;*.bmp\0\0", L"png", homeDir() + "\\");
        if (!p.empty()) lk.bgImage = p;
    }
    if (!lk.bgImage.empty()) {
        ImGui::PushTextWrapPos(lw); ImGui::TextDisabled("%s", lk.bgImage.c_str()); ImGui::PopTextWrapPos();
        ImGui::SetNextItemWidth(lw); ImGui::SliderFloat("##bo", &lk.bgOpacity, 0, 1, "Opacity %.2f");
        if (ImGui::Button("Remove", ImVec2(lw, 0))) lk.bgImage.clear();
    }
    notYet("A photo or drawing behind the person to model against, like MakeHuman's background images.");
}

// ---------- Utilities ----------
static int matPick = 0;   // 0: the skin; k: proxy k-1
static void materialEditor(float lw) {
    groupBox("Object");
    std::vector<std::string> names = {"Skin"};
    for (const auto& p : body.proxies) names.push_back(niceName(p.file) + " (" + p.kind + ")");
    matPick = std::clamp(matPick, 0, (int)names.size() - 1);
    std::vector<const char*> cn; for (const auto& n : names) cn.push_back(n.c_str());
    ImGui::SetNextItemWidth(lw); ImGui::Combo("##obj", &matPick, cn.data(), (int)cn.size());
    float* tint = matPick == 0 ? body.skinTint : body.proxies[matPick - 1].tint;
    std::string& tex = matPick == 0 ? body.skinTexture : body.proxies[matPick - 1].texture;
    groupBox("Colour");
    for (int c = 0; c < 3; ++c) { static const char* n[] = {"Red %.2f", "Green %.2f", "Blue %.2f"}; ImGui::PushID(c); ImGui::SetNextItemWidth(lw); if (ImGui::SliderFloat("##t", &tint[c], 0, 2, n[c])) dirty = true; ImGui::PopID(); }
    if (ImGui::Button("Plain", ImVec2(lw, 0))) { tint[0] = tint[1] = tint[2] = 1; dirty = true; }
    groupBox("Texture");
    ImGui::PushTextWrapPos(lw); ImGui::TextDisabled("%s", tex.empty() ? "(none)" : tex.c_str()); ImGui::PopTextWrapPos();
    if (ImGui::Button("Change texture...", ImVec2(lw, 0))) {
        const std::string p = fileDialog(false, L"Pictures\0*.png;*.jpg;*.jpeg\0\0", L"png", homeDir() + "\\");
        if (!p.empty()) { tex = p; dirty = true; }
    }
    if (matPick > 0) { if (ImGui::Checkbox("See-through (alpha)", &body.proxies[matPick - 1].transparent)) dirty = true; }
    groupBox("Material file");
    if (ImGui::Button("Load a material (.mhmat)...", ImVec2(lw, 0))) {
        const std::string p = fileDialog(false, L"MakeHuman material\0*.mhmat\0\0", L"mhmat", homeDir() + "\\");
        if (!p.empty()) { remember("Material"); if (matPick == 0) Human::setSkin(body, p); else Human::setProxyMaterial(body, body.proxies[matPick - 1].kind, p); dirty = true; }
    }
    if (ImGui::Button("Save as a material...", ImVec2(lw, 0))) {
        const std::string p = fileDialog(true, L"MakeHuman material\0*.mhmat\0\0", L"mhmat", homeDir() + "\\material.mhmat");
        if (!p.empty()) {
            std::ofstream o(fs::u8path(p));
            char b[128];
            o << "# Written by Pygmalion\nname " << fs::u8path(p).stem().u8string() << "\n";
            snprintf(b, sizeof b, "diffuseColor %.4f %.4f %.4f\n", std::min(tint[0], 1.0f), std::min(tint[1], 1.0f), std::min(tint[2], 1.0f)); o << b;
            if (!tex.empty()) {
                const fs::path src = fs::u8path(tex), dst = fs::u8path(p).parent_path() / src.filename();
                std::error_code ec; if (!fs::exists(dst, ec)) fs::copy_file(src, dst, ec);
                o << "diffuseTexture " << src.filename().u8string() << "\n";
            }
            if (matPick > 0 && body.proxies[matPick - 1].transparent) o << "transparent True\n";
            say("Saved " + p);
        }
    }
}

static char targetFilter[64] = "";
static void targetsView(float lw) {
    groupBox("Targets in use");
    ImGui::SetNextItemWidth(lw); ImGui::InputTextWithHint("##tf", "filter", targetFilter, sizeof targetFilter);
    int n = 0;
    ImGui::BeginChild("tl", ImVec2(lw, 0));
    for (const auto& t : body.targets) {
        if (std::fabs(t.weight) < 1e-4f) continue;
        if (targetFilter[0] && t.path.find(targetFilter) == std::string::npos) continue;
        ImGui::TextDisabled("%.3f", t.weight); ImGui::SameLine(); ImGui::TextUnformatted(t.path.c_str());
        ++n;
    }
    if (!n) ImGui::TextDisabled("(none)");
    ImGui::EndChild();
}

static char targetName[64] = "my-shape";
static void customFolder(std::string& dir) { dir = homeDir().empty() ? std::string() : (fs::u8path(homeDir()) / "custom").u8string(); std::error_code ec; if (!dir.empty()) fs::create_directories(fs::u8path(dir), ec); }
static void loadCustomTargets() {
    std::string dir; customFolder(dir);
    std::error_code ec;
    for (const auto& e : fs::directory_iterator(fs::u8path(dir), ec)) if (e.path().extension() == ".target") Human::addCustomTarget(body, e.path().u8string());
}
static void saveTargetsLeft(float lw) {
    groupBox("Save the shape as a target");
    ImGui::SetNextItemWidth(lw); ImGui::InputText("##tn", targetName, sizeof targetName);
    if (ImGui::Button("Save", ImVec2(lw, 0))) {
        std::string dir; customFolder(dir);
        const std::string p = (fs::u8path(dir) / (std::string(targetName) + ".target")).u8string();
        std::string err;
        if (Human::saveTarget(body, p, err)) { say("Saved " + p + " (Modelling > Custom)"); Human::addCustomTarget(body, p); } else say(err);
    }
    notYet("The whole shape as it is now, against MakeHuman's base mesh. It's saved in your custom folder and appears as a slider in Modelling > Custom.");
}

static void customLeft(float lw) {
    groupBox("Custom targets");
    std::string dir; customFolder(dir);
    if (ImGui::Button("Look again", ImVec2(lw / 2 - 4, 0))) loadCustomTargets();
    ImGui::SameLine();
#ifdef _WIN32
    if (ImGui::Button("Open the folder", ImVec2(lw / 2 - 4, 0))) ShellExecuteW(nullptr, L"open", fs::u8path(dir).wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#endif
    int n = 0;
    for (size_t i = 0; i < body.mods.size(); ++i) if (body.mods[i].group == "custom") { modSlider((int)i, lw, "frontView"); ++n; }
    if (!n) notYet(("Put your own .target files (from Blender's MakeTarget, or Utilities > Save targets) in " + dir + " and they appear here as sliders.").c_str());
}

// Mass produce: many random people, saved (and exported) one after another
static int massCount = 10, massLeft = 0, massDone = 0, massFormat = -1; static float massAmount = 0.5f; static bool massRandGear = true;
static void massStep() {
    if (massLeft <= 0) return;
    std::random_device rd;
    Human::randomize(body, massAmount, true, true, true, rd());
    if (massRandGear) {   // clothes, hair, skin at random too
        auto pick = [&](const std::vector<std::string>& v) { return v.empty() ? std::string() : v[rd() % v.size()]; };
        std::string err;
        Human::takeOff(body, "hair"); Human::takeOff(body, "clothes");
        const std::string h = pick(filesIn("hair", ".mhclo")); if (!h.empty()) Human::wear(body, h, err);
        const std::string c = pick(filesIn("clothes", ".mhclo")); if (!c.empty()) Human::wear(body, c, err);
        Human::setSkin(body, pick(filesIn("skins", ".mhmat")));
    }
    Human::apply(body);
    if (!rig.empty()) Skel::build(rig, body.pos);
    if (!chosen.empty()) Skel::build(chosen, body.pos);
    repose();
    std::string dir = (fs::u8path(homeDir()) / "massproduce").u8string();
    std::error_code ec; fs::create_directories(fs::u8path(dir), ec);
    char name[32]; snprintf(name, sizeof name, "person%03d", massDone + 1);
    std::string err;
    saveModel((fs::u8path(dir) / (std::string(name) + ".mhm")).u8string(), name, err);
    if (massFormat >= 0) exportTo((fs::u8path(dir) / (std::string(name) + EXPORT_EXT[massFormat])).u8string(), massFormat, err);
    ++massDone; --massLeft;
    dirty = true;
    if (!massLeft) say("Made " + std::to_string(massDone) + " people in " + dir);
}
static void massLeftPanel(float lw) {
    groupBox("Mass produce");
    ImGui::SetNextItemWidth(lw); ImGui::SliderInt("##mc", &massCount, 1, 500, "%d people");
    ImGui::SetNextItemWidth(lw); ImGui::SliderFloat("##ma", &massAmount, 0.05f, 1, "Variation %.2f");
    ImGui::Checkbox("Random hair, clothes and skin", &massRandGear);
    std::vector<const char*> f = {"Models only (.mhm)"}; for (int k = 0; k < EX_COUNT; ++k) f.push_back(EXPORT_NAMES[k]);
    int sel = massFormat + 1;
    ImGui::SetNextItemWidth(lw); if (ImGui::Combo("##mf", &sel, f.data(), (int)f.size())) massFormat = sel - 1;
    if (massLeft > 0) {
        ImGui::ProgressBar((float)massDone / std::max(1, massDone + massLeft), ImVec2(lw, 0));
        if (ImGui::Button("Stop", ImVec2(lw, 0))) massLeft = 0;
    } else if (ImGui::Button("Make them", ImVec2(lw, 34))) { remember("Mass produce"); massLeft = massCount; massDone = 0; }
    notYet(("They're saved in " + (fs::u8path(homeDir()) / "massproduce").u8string() + ".").c_str());
}

static bool installPack(const std::string& pack, std::string& err);
static bool installAsset(const std::string& src, std::string kind, std::string& said, std::string& err);
static std::string kindOfProxy(const std::string& mhclo);
extern const char* KINDS[13];
// Scripting: Pygmalion's own commands, a line each (MakeHuman's is Python; this does the same jobs)
static char script[8192] = "# a line each; # starts a note\nreset\nmacro Gender 0\nmacro Age 0.4\nmodifier nose/nose-scale-horiz-decr|incr 0.3\nwear hair/bob01/bob01.mhclo\nskin skins/young_caucasian_female/young_caucasian_female.mhmat\nexpression smile01\n";
static std::string scriptLog;
static const char* MACRO_NAMES[] = {"Gender", "Age", "Muscle", "Weight", "Height", "BodyProportions", "African", "Asian", "Caucasian", "BreastSize", "BreastFirmness"};
static const char* COMMANDS = "reset | random 0.5 | describe words\nmacro Gender 0..1 (Age, Muscle, Weight,\n  Height, BodyProportions, African, Asian,\n  Caucasian, BreastSize, BreastFirmness)\nmodifier group/name value\nwear clothes/x/x.mhclo | takeoff hair\nskin skins/x/x.mhmat | skin default\nskeleton game_engine | none\npose standing01 | none\nanimation walk1 | none\nexpression smile01 | none\nbones on | off\nsymmetry on | off\nunits meter (decimeter, centimeter...)\nload file.mhm | save file.mhm\nexport file.fbx (.dae .glb .obj ...)\nrender file.png 1920 1080\nsmooth on | off\nscene studio (makehuman, soft,\n  dramatic, sunset) | scene file.pygscene\n  | scene save file.pygscene\ndownload hair01 (an asset pack)\ninstall file.zip | folder [clothes, hair...]\nget Gender | get group/name (a value)\nlist macros | modifiers | worn | poses\ninfo (the size of everything)";
// one command; anything it prints goes to out
static bool runCommand(const std::string& line, std::string& err, const std::function<void(const std::string&)>& out) {
    std::istringstream ls(line); std::string cmd; ls >> cmd;
    if (cmd.empty() || cmd[0] == '#') return true;
    std::string rest; std::getline(ls >> std::ws, rest);
    std::string a; float v = 0;
    std::istringstream rs(rest); rs >> a >> v;
    bool ok = true;
    if (cmd == "reset") { Human::resetAll(body); Human::defaultGear(body); setPoseFile(""); setExpressionFile(""); }
    else if (cmd == "modifier") { auto it = body.modByName.find(a); if (it == body.modByName.end()) { ok = false; err = "no modifier " + a; } else Human::setModifier(body, it->second, v); }
    else if (cmd == "macro") Human::setMacro(body, a, v);
    else if (cmd == "wear") { const std::string f = Human::absPath(body, rest); ok = !f.empty() && Human::wear(body, f, err); if (f.empty()) err = "no " + rest; }
    else if (cmd == "takeoff") Human::takeOff(body, rest);
    else if (cmd == "skin") { const std::string f = rest == "default" ? std::string() : Human::absPath(body, rest); ok = Human::setSkin(body, f); }
    else if (cmd == "skeleton" || cmd == "pose" || cmd == "expression") {
        const std::string sub = cmd == "skeleton" ? "rigs" : cmd == "pose" ? "poses" : "expressions";
        const std::string ext = cmd == "skeleton" ? ".mhskel" : cmd == "pose" ? ".bvh" : ".mhpose";
        std::string f;
        if (rest != "none") {
            f = findData(sub + "/" + rest + (rest.find('.') == std::string::npos ? ext : ""), sub.c_str());
            if (f.empty()) for (const auto& c : filesIn(sub, ext.c_str())) if (fs::u8path(c).stem().u8string() == rest) f = c;   // in a folder of its own
            if (f.empty()) { ok = false; err = "no " + cmd + " " + rest; }
        }
        if (ok) { if (cmd == "skeleton") setSkeletonFile(f); else if (cmd == "pose") setPoseFile(f); else setExpressionFile(f); }
    }
    else if (cmd == "random") { std::random_device rd; Human::randomize(body, rest.empty() ? 0.5f : (float)atof(rest.c_str()), true, true, true, rd()); }
    else if (cmd == "describe") Human::describe(body, rest);
    else if (cmd == "info") {   // the size of everything (for checking scripts)
        Human::apply(body); if (!rig.empty()) Skel::build(rig, body.pos); if (!chosen.empty()) Skel::build(chosen, body.pos); repose();
        auto box = [&](const char* what, const std::vector<float>& v, const std::vector<int>* idx) {
            float lo[3] = {1e9f, 1e9f, 1e9f}, hi[3] = {-1e9f, -1e9f, -1e9f};
            auto take = [&](size_t i) { for (int c = 0; c < 3; ++c) { lo[c] = std::min(lo[c], v[i * 3 + c]); hi[c] = std::max(hi[c], v[i * 3 + c]); } };
            if (idx) for (int i : *idx) take((size_t)i); else for (size_t i = 0; i < v.size() / 3; ++i) take(i);
            char b2[200]; snprintf(b2, sizeof b2, "%s: x %.2f..%.2f y %.2f..%.2f z %.2f..%.2f", what, lo[0], hi[0], lo[1], hi[1], lo[2], hi[2]); out(b2);
        };
        box("body", body.shown.empty() ? body.pos : body.shown, &body.tri);
        box("body rest", body.pos, &body.tri);
        { std::vector<int> h; for (int v = 13380; v < (int)(body.pos.size() / 3); ++v) h.push_back(v); box("helpers", body.shown.empty() ? body.pos : body.shown, &h); box("helpers rest", body.pos, &h); }
        for (const auto& px : body.proxies) { std::vector<float> f; Human::fitProxy(body, px, f); box(px.name.c_str(), f, nullptr); std::vector<float> r; Human::fitProxy(body, px, r, true); box((px.name + " rest").c_str(), r, nullptr); }
    }
    else if (cmd == "symmetry") body.symmetry = rest != "off";
    else if (cmd == "bones") { showBones = rest != "off"; Viewer::look().bones = showBones; }
    else if (cmd == "smooth") Viewer::look().smooth = rest != "off";
    else if (cmd == "download") ok = installPack(rest, err);
    else if (cmd == "kindof") out(kindOfProxy(rest));   // what Install would call a .mhclo
    else if (cmd == "install") {
        std::string path = rest, kind;
        for (const char* k : KINDS) { const std::string kk = std::string(" ") + k; if (path.size() > kk.size() && path.compare(path.size() - kk.size(), kk.size(), kk) == 0) { kind = k; path.resize(path.size() - kk.size()); } }
        std::string said; ok = installAsset(path, kind, said, err); if (ok) out(said);
    }
    else if (cmd == "animation") {
        if (rest == "none") setAnimation("");
        else { std::string f; for (const auto& c : filesIn("animations", ".bvh")) if (fs::u8path(c).stem().u8string() == rest) f = c; if (f.empty()) { ok = false; err = "no animation " + rest; } else setAnimation(f); }
    }
    else if (cmd == "units") { ok = false; for (int k = 0; k < 5; ++k) if (rest == UNIT_NAMES[k]) { exportUnits = k; ok = true; } if (!ok) err = "units: decimeter, meter, centimeter, millimeter or inch"; }
    else if (cmd == "load") ok = loadModel(rest, err);
    else if (cmd == "save") ok = saveModel(rest, fs::u8path(rest).stem().u8string(), err);
    else if (cmd == "export" || cmd == "render") {
        Human::apply(body); if (!rig.empty()) Skel::build(rig, body.pos); if (!chosen.empty()) Skel::build(chosen, body.pos); repose(); Viewer::update(body);
        if (cmd == "export") {
            int fmt = -1;
            for (int k = 0; k < EX_COUNT; ++k) { const std::string e = EXPORT_EXT[k]; if (rest.size() > e.size() && rest.compare(rest.size() - e.size(), e.size(), e) == 0) fmt = k; }
            if (fmt == EX_UV) fmt = -1;
            if (fmt < 0) { ok = false; err = "export: name the file with .dae .fbx .glb .obj .mesh.xml .stl or .bvh"; } else ok = exportTo(rest, fmt, err);
        } else {
            std::string path; int w = renderW, h = renderH; std::istringstream r2(rest); r2 >> path >> w >> h;
            ok = renderNow(w, h) && saveRendered(path); if (!ok) err = "couldn't render " + path;
        }
    }
    else if (cmd == "get") {
        char b2[160];
        auto it = body.modByName.find(a);
        if (it != body.modByName.end()) { snprintf(b2, sizeof b2, "%s = %.3f", a.c_str(), body.mods[(size_t)it->second].value); out(b2); }
        else { bool found = false; const std::vector<float> mv = snapshot().macros; for (size_t i = 0; i < 11; ++i) if (a == MACRO_NAMES[i]) { snprintf(b2, sizeof b2, "%s = %.3f", MACRO_NAMES[i], mv[i]); out(b2); found = true; }
               if (!found) { ok = false; err = "no modifier or macro " + a; } }
    }
    else if (cmd == "list") {
        if (a == "macros") for (const char* m : MACRO_NAMES) out(m);
        else if (a == "modifiers") for (const auto& kv : body.modByName) out(kv.first);
        else if (a == "worn") for (const auto& px : body.proxies) out(px.kind + ": " + px.name);
        else if (a == "poses") for (const auto& f : filesIn("poses", ".bvh")) out(fs::u8path(f).stem().u8string());
        else { ok = false; err = "list macros, modifiers, worn or poses"; }
    }
    else if (cmd == "scene") {
        int k = -1; for (int j = 0; j < 5; ++j) { std::string n = SCENE_PRESETS[j]; for (char& c : n) c = (char)tolower((unsigned char)c); if (rest == n) k = j; }
        if (k >= 0) scenePreset(k);
        else if (a == "save") { const std::string f = rest.size() > 5 ? rest.substr(5) : ""; ok = !f.empty() && saveScene(f); if (!ok) err = "scene save file.pygscene"; }
        else ok = loadScene(rest, err);
    }
    else if (cmd == "help") out(COMMANDS);
    else { ok = false; err = "unknown command " + cmd + " (help lists them)"; }
    return ok;
}
static void runScript(const std::string& text) {
    scriptLog.clear();
    std::istringstream in(text); std::string line; int n = 0;
    auto out = [&](const std::string& m) { scriptLog += m + "\n"; };
    while (std::getline(in, line)) {
        ++n;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::string err;
        const bool ok = runCommand(line, err, out);
        { std::istringstream ls(line); std::string c; ls >> c; if (c.empty() || c[0] == '#') continue; }
        out((ok ? "ok    " : "line " + std::to_string(n) + ": ") + (ok ? line : err));
    }
    dirty = true;
}
#ifdef _WIN32
#include "sockops.inc"
#endif

static void scriptingLeft(float lw) {
    groupBox("Script");
    ImGui::InputTextMultiline("##sc", script, sizeof script, ImVec2(lw, 300));
    if (ImGui::Button("Run", ImVec2(lw, 30))) { remember("Script"); runScript(script); say("The script ran"); }
    if (ImGui::Button("Load...", ImVec2(lw / 2 - 4, 0))) {
        const std::string p = fileDialog(false, L"Pygmalion script\0*.pyg;*.txt\0\0", L"pyg", homeDir() + "\\");
        if (!p.empty()) { std::ifstream in(fs::u8path(p)); std::ostringstream ss; ss << in.rdbuf(); snprintf(script, sizeof script, "%s", ss.str().c_str()); }
    }
    ImGui::SameLine();
    if (ImGui::Button("Save...", ImVec2(lw / 2 - 4, 0))) {
        const std::string p = fileDialog(true, L"Pygmalion script\0*.pyg\0\0", L"pyg", homeDir() + "\\script.pyg");
        if (!p.empty()) { std::ofstream o(fs::u8path(p)); o << script; }
    }
}
static std::vector<std::string> shellLines = {"Pygmalion's shell: a command a line (help lists them)."}, shellHistory;
static char shellIn[512] = ""; static int shellBack = -1; static bool shellFocus = true;
static void shellRun(const std::string& line) {
    if (line.empty()) return;
    shellLines.push_back("> " + line);
    shellHistory.push_back(line); shellBack = -1;
    if (line == "clear") { shellLines.clear(); return; }
    std::string err;
    remember("Shell");
    const bool ok = runCommand(line, err, [](const std::string& m) { std::istringstream ss(m); std::string l; while (std::getline(ss, l)) shellLines.push_back(l); });
    if (!ok) shellLines.push_back(err);
    dirty = true;
}
static char mixName[64] = "my expression", mixDesc[256] = "";
static void mixerLeft(float lw) {
    groupBox("Expression mixer");
    notYet("Every face pose unit is a slider on the right. Mix them, then save the mix: it joins Pose/Animate > Expression.");
    ImGui::SetNextItemWidth(lw); ImGui::InputTextWithHint("##mn", "name", mixName, sizeof mixName);
    ImGui::SetNextItemWidth(lw); ImGui::InputTextWithHint("##md", "description", mixDesc, sizeof mixDesc);
    auto json = [](const std::string& t) { std::string o; for (char c : t) { if (c == '"' || c == '\\') o += '\\'; o += c; } return o; };
    if (ImGui::Button("Save the expression", ImVec2(lw, 0))) {
        std::string name = mixName; for (char& c : name) if (strchr("\\/:*?\"<>|", c)) c = '_';
        if (name.empty()) name = "expression";
        std::error_code ec; const fs::path dir = fs::u8path(userDataDir()) / "expressions"; fs::create_directories(dir, ec);
        const fs::path f = dir / fs::u8path(name + ".mhpose");
        std::ofstream o(f);
        o << "{\n    \"name\": \"" << json(mixName) << "\",\n    \"description\": \"" << json(mixDesc) << "\",\n    \"tags\": [],\n    \"unit_poses\": {";
        bool first = true;
        for (size_t k = 1; k < unitW.size() && k < unitNames.size(); ++k) if (unitW[k] > 0) { o << (first ? "\n" : ",\n") << "        \"" << json(unitNames[k]) << "\": " << unitW[k]; first = false; }
        o << "\n    }\n}\n";
        if (o) { expressionFile = f.u8string(); say("Saved " + f.u8string()); } else say("Couldn't write " + f.u8string());
    }
    if (ImGui::Button("Load an expression...", ImVec2(lw, 0))) {
        const std::string p = fileDialog(false, L"MakeHuman expression\0*.mhpose\0\0", L"mhpose", (fs::u8path(userDataDir()) / "expressions").u8string() + "\\");
        if (!p.empty()) { remember("Expression"); setExpressionFile(p); snprintf(mixName, sizeof mixName, "%s", fs::u8path(p).stem().u8string().c_str()); }
    }
    if (ImGui::Button("Neutral", ImVec2(lw, 0))) { remember("Expression"); std::fill(unitW.begin(), unitW.end(), 0.0f); expressionFile.clear(); dirty = true; }
    int used = 0; for (size_t k = 1; k < unitW.size(); ++k) if (unitW[k] > 0) ++used;
    ImGui::TextDisabled("%d of %d pose units in the mix", used, (int)unitNames.size() - 1);
}
static void shellView() {
    groupBox("Shell");
    const float inH = ImGui::GetFrameHeightWithSpacing();
    ImGui::BeginChild("sh", ImVec2(0, -inH), true);
    for (const auto& l : shellLines) {
        if (l.rfind("> ", 0) == 0) ImGui::TextColored(ImVec4(0.88f, 0.25f, 0.54f, 1), "%s", l.c_str()); else ImGui::TextUnformatted(l.c_str());
    }
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) ImGui::SetScrollHereY(1);
    ImGui::EndChild();
    ImGui::SetNextItemWidth(-1);
    if (shellFocus) { ImGui::SetKeyboardFocusHere(); shellFocus = false; }
    auto cb = [](ImGuiInputTextCallbackData* d) -> int {   // up and down go through what was typed before
        if (d->EventFlag != ImGuiInputTextFlags_CallbackHistory || shellHistory.empty()) return 0;
        if (d->EventKey == ImGuiKey_UpArrow) shellBack = shellBack < 0 ? (int)shellHistory.size() - 1 : std::max(0, shellBack - 1);
        else if (shellBack >= 0 && ++shellBack >= (int)shellHistory.size()) shellBack = -1;
        d->DeleteChars(0, d->BufTextLen);
        if (shellBack >= 0) d->InsertChars(0, shellHistory[(size_t)shellBack].c_str());
        return 0;
    };
    if (ImGui::InputTextWithHint("##shin", "macro Age 0.8", shellIn, sizeof shellIn, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackHistory, cb)) {
        shellRun(shellIn); shellIn[0] = 0; shellFocus = true;
    }
}
static void scriptingRight() {
    groupBox("Commands");
    ImGui::TextDisabled("%s", COMMANDS);
    if (!scriptLog.empty()) { groupBox("Output"); ImGui::BeginChild("so"); ImGui::TextUnformatted(scriptLog.c_str()); ImGui::EndChild(); }
}

// Download: MakeHuman's asset packs (CC0) from its mirror, into your own data folder
static const char* MIRROR = "https://mirror1.makehuman.net/asset_packs/";
static std::vector<std::string> packs; static std::set<std::string> packsHave;
static std::atomic<bool> dlBusy{false}, dlStop{false}; static std::atomic<int64_t> dlDone{0}, dlTotal{0};
static std::string dlName, dlResult; static std::mutex dlMtx; static bool dlFinished = false;
static void loadPacksHave() {
    packsHave.clear();
    std::ifstream in(fs::u8path(userDataDir()) / "packs-installed.txt"); std::string l;
    while (std::getline(in, l)) if (!l.empty()) packsHave.insert(l);
}
static bool installPack(const std::string& pack, std::string& err) {
#ifdef _WIN32
    // the pack's own page names its zip: _cc0, _ccby or _cc-by (the licence: CC-BY packs want their authors credited)
    std::string zipName = pack + "_cc0.zip";
    {
        const Http::Response r = Http::get(std::string(MIRROR) + pack + "/");
        size_t at = 0;
        while ((at = r.body.find("href=\"", at)) != std::string::npos) {
            at += 6; const size_t e = r.body.find('"', at); if (e == std::string::npos) break;
            const std::string h = r.body.substr(at, e - at);
            if (h.size() > 4 && h.compare(h.size() - 4, 4, ".zip") == 0 && h.find('/') == std::string::npos) { zipName = h; break; }
        }
    }
    const std::string url = std::string(MIRROR) + pack + "/" + zipName;
    const fs::path zip = fs::temp_directory_path() / ("pygmalion-" + pack + ".zip");
    if (!Http::download(url, zip.u8string(), [](int64_t d, int64_t t) { dlDone = d; dlTotal = t; }, &dlStop, err)) { if (err.empty()) err = "Download stopped"; return false; }
    // Windows' own tar opens zips
    std::wstring cmd = L"tar.exe -xf \"" + zip.wstring() + L"\" -C \"" + fs::u8path(userDataDir()).wstring() + L"\"";
    STARTUPINFOW si{}; si.cb = sizeof si; PROCESS_INFORMATION pi{};
    std::vector<wchar_t> buf(cmd.begin(), cmd.end()); buf.push_back(0);
    DWORD code = 1;
    if (CreateProcessW(nullptr, buf.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, INFINITE); GetExitCodeProcess(pi.hProcess, &code); CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    }
    std::error_code ec; fs::remove(zip, ec);
    if (code != 0) { err = "Couldn't unpack " + pack; return false; }
    std::ofstream o(fs::u8path(userDataDir()) / "packs-installed.txt", std::ios::app); o << pack << "\n";
    libCache.clear();
    return true;
#else
    err = "Downloads need Windows for now"; (void)pack; return false;
#endif
}
static void startDownload(const std::string& pack, bool listOnly) {
#ifdef _WIN32
    if (dlBusy) return;
    dlBusy = true; dlStop = false; dlDone = 0; dlTotal = 0; dlName = pack;
    std::thread([pack, listOnly] {
        std::string result;
        if (listOnly) {
            const Http::Response r = Http::get(MIRROR);
            std::vector<std::string> found;
            size_t i = 0;
            while ((i = r.body.find("data-sort=\"*", i)) != std::string::npos) { i += 12; const size_t e = r.body.find('"', i); found.push_back(r.body.substr(i, e - i)); }
            std::lock_guard<std::mutex> g(dlMtx);
            if (found.empty()) result = "Couldn't reach MakeHuman's mirror"; else { packs = found; result = std::to_string(found.size()) + " packs"; }
        } else {
            std::string err;
            result = installPack(pack, err) ? "Installed " + pack : err;
        }
        std::lock_guard<std::mutex> g(dlMtx);
        dlResult = result; dlFinished = true; dlBusy = false;
    }).detach();
#else
    (void)pack; (void)listOnly;
#endif
}
// what kind of asset a folder holds, from its files (MakeHuman's folder names are its kinds)
const char* KINDS[13] = {"clothes", "hair", "eyebrows", "eyelashes", "teeth", "tongue", "eyes", "proxymeshes", "skins", "poses", "expressions", "rigs", "custom"};
// what a .mhclo is: its name if that says, else where it sits on the body (MakeHuman's files don't record it)
static std::string kindOfProxy(const std::string& mhclo) {
    std::string nm = fs::u8path(mhclo).stem().u8string(); for (char& c : nm) c = (char)tolower((unsigned char)c);
    for (const auto& [word, kind] : std::vector<std::pair<const char*, const char*>>{{"eyebrow", "eyebrows"}, {"brow", "eyebrows"}, {"eyelash", "eyelashes"}, {"lash", "eyelashes"},
                                                                                      {"teeth", "teeth"}, {"tooth", "teeth"}, {"tongue", "tongue"}, {"hair", "hair"}})
        if (nm.find(word) != std::string::npos) return kind;
    // its reference vertices, and its material's see-through flag
    std::ifstream in(fs::u8path(mhclo)); std::string l, mat; bool verts = false; std::vector<int> refs;
    while (std::getline(in, l)) {
        if (!l.empty() && l.back() == '\r') l.pop_back();
        if (l.rfind("material ", 0) == 0) mat = l.substr(9);
        if (l.rfind("verts", 0) == 0) { verts = true; continue; }
        if (l.rfind("delete_verts", 0) == 0) verts = false;
        if (verts) {   // "a b c  wa wb wc  dx dy dz" or "a" (a material line can sit among them)
            const size_t k = l.find_first_not_of(" 	");
            if (k == std::string::npos || !isdigit((unsigned char)l[k])) continue;
            std::istringstream ls(l); int v; if (ls >> v) refs.push_back(v);
        }
    }
    bool seeThrough = false;
    if (!mat.empty()) { std::ifstream m(fs::u8path(mhclo).parent_path() / fs::u8path(mat)); std::string ml; while (std::getline(m, ml)) { for (char& c : ml) c = (char)tolower((unsigned char)c); if (ml.rfind("transparent true", 0) == 0) seeThrough = true; } }
    const std::vector<float>& B = body.base;
    if (refs.empty() || B.empty()) return "clothes";
    float y0 = 1e9f, y1 = -1e9f; for (size_t i = 1; i < B.size(); i += 3) { y0 = std::min(y0, B[i]); y1 = std::max(y1, B[i]); }
    const float H = y1 - y0;
    float lo = 1e9f, hi = -1e9f, cy = 0, cz = 0; int n = 0;
    for (int r : refs) if (r >= 0 && (size_t)r * 3 + 2 < B.size()) { const float y = B[(size_t)r * 3 + 1]; lo = std::min(lo, y); hi = std::max(hi, y); cy += y; cz += B[(size_t)r * 3 + 2]; ++n; }
    if (!n) return "clothes";
    cy /= n; cz /= n;
    float eyeY = y0 + 0.93f * H;   // the eyes' height, from their joint markers if there
    { auto it = body.joints.find("joint-l-eye"); if (it != body.joints.end() && !it->second.empty()) { float sy = 0; for (int v : it->second) sy += B[(size_t)v * 3 + 1]; eyeY = sy / it->second.size(); } }
    const bool head = lo > y0 + 0.84f * H, tiny = hi - lo < 0.045f * H;
    if (head && tiny) {   // something small on the face: by its height against the eyes, and how far forward
        if (seeThrough) return cy > eyeY + 0.0035f * H ? "eyebrows" : "eyelashes";
        if (cy > eyeY - 0.012f * H) return cy > eyeY + 0.01f * H ? "clothes" : "eyes";
        return cz < 1.22f ? "tongue" : "teeth";
    }
    // see-through, up to the top of the head: hair (long hair hangs down the back)
    if (seeThrough && hi > y0 + 0.97f * H && cy > y0 + 0.75f * H) return "hair";
    return "clothes";
}
static std::string guessKind(const fs::path& dir) {
    std::error_code ec;
    std::string mhclo, ext;
    for (const auto& e : fs::recursive_directory_iterator(dir, ec)) {
        if (!e.is_regular_file()) continue;
        const std::string x = e.path().extension().u8string();
        if (x == ".mhclo" && mhclo.empty()) mhclo = e.path().u8string();
        else if (x == ".proxy") return "proxymeshes";
        else if (x == ".mhskel") return "rigs";
        else if (x == ".target") ext = "custom";
        else if (x == ".bvh" && ext.empty()) ext = "poses";
        else if (x == ".mhpose" && ext.empty()) ext = "expressions";
        else if (x == ".mhmat" && ext.empty()) ext = "skins";
    }
    if (!mhclo.empty()) return kindOfProxy(mhclo);
    return ext.empty() ? "clothes" : ext;
}
#ifdef _WIN32
static bool runQuiet(const std::wstring& cmd) {
    STARTUPINFOW si{}; si.cb = sizeof si; PROCESS_INFORMATION pi{};
    std::vector<wchar_t> buf(cmd.begin(), cmd.end()); buf.push_back(0);
    DWORD code = 1;
    if (CreateProcessW(nullptr, buf.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, INFINITE); GetExitCodeProcess(pi.hProcess, &code); CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    }
    return code == 0;
}
#endif
// a downloaded asset (a .zip, or a folder) into your data folder, where its kind goes. kind "" = work it out.
// A zip with MakeHuman's kind folders at the top (clothes/, hair/...) goes in as it is.
static bool installAsset(const std::string& src, std::string kind, std::string& said, std::string& err) {
    std::error_code ec;
    const fs::path in = fs::u8path(src);
    fs::path dir = in;
    fs::path tmp;
    if (fs::is_regular_file(in, ec)) {
#ifdef _WIN32
        tmp = fs::temp_directory_path() / ("pygmalion-asset-" + std::to_string(GetTickCount64()));
        fs::create_directories(tmp, ec);
        if (!runQuiet(L"tar.exe -xf \"" + in.wstring() + L"\" -C \"" + tmp.wstring() + L"\"")) { fs::remove_all(tmp, ec); err = "Couldn't unpack " + in.filename().u8string(); return false; }
        dir = tmp;
#else
        err = "Unzip it first, then install the folder"; return false;
#endif
    }
    if (!fs::is_directory(dir, ec)) { err = "Not a zip or a folder: " + src; return false; }
    // a single folder inside (as most downloads are): use it
    auto only = [&](const fs::path& d) { fs::path one; int n = 0; for (const auto& e : fs::directory_iterator(d, ec)) { ++n; one = e.path(); } return n == 1 && fs::is_directory(one, ec) ? one : fs::path(); };
    const fs::path data = fs::u8path(userDataDir());
    // kind folders at the top: copy as they are
    bool kindsTop = false;
    for (const char* k : KINDS) if (fs::is_directory(dir / k, ec)) kindsTop = true;
    if (!kindsTop) { const fs::path o = only(dir); if (!o.empty()) { bool kt = false; for (const char* k : KINDS) if (fs::is_directory(o / k, ec)) kt = true; if (kt) { dir = o; kindsTop = true; } } }
    if (kindsTop && kind.empty()) {
        fs::copy(dir, data, fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
        said = "Installed into " + data.u8string();
    } else {
        fs::path item = dir; { const fs::path o = only(dir); if (!o.empty()) item = o; }
        if (kind.empty()) kind = guessKind(item);
        std::string name = item.filename().u8string();
        if (item == tmp || name.empty()) name = in.stem().u8string();
        const fs::path to = data / kind / fs::u8path(name);
        fs::create_directories(to, ec);
        fs::copy(item, to, fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
        said = "Installed " + name + " as " + kind;
    }
    if (!tmp.empty()) fs::remove_all(tmp, ec);
    if (ec) { err = "Couldn't copy: " + ec.message(); return false; }
    libCache.clear();
    return true;
}
static int installKind = 0;   // 0: work it out, else KINDS[k - 1]
static void installLeft(float lw) {
    groupBox("Install a downloaded asset");
    notYet("Clothes, hair, skins, poses... downloaded from MakeHuman's community site (or anywhere) as a zip or a folder. They show in their tab straight away.");
    std::vector<const char*> k = {"Work out what it is"}; for (const char* n : KINDS) k.push_back(n);
    ImGui::SetNextItemWidth(lw); ImGui::Combo("##ik", &installKind, k.data(), (int)k.size());
    auto go = [&](const std::string& p) {
        if (p.empty()) return;
        std::string said, err;
        if (installAsset(p, installKind ? KINDS[installKind - 1] : "", said, err)) say(said); else say(err);
    };
    if (ImGui::Button("Install a zip...", ImVec2(lw / 2 - 4, 0))) go(fileDialog(false, L"Downloaded asset\0*.zip\0\0", L"zip", homeDir() + "\\"));
    ImGui::SameLine();
    if (ImGui::Button("Install a folder...", ImVec2(lw / 2 - 4, 0))) {
#ifdef _WIN32
        // a folder: pick any file inside it
        const std::string f = fileDialog(false, L"Any file in the asset's folder\0*.mhclo;*.mhmat;*.obj;*.bvh;*.mhpose;*.target;*.proxy;*.png\0\0", L"mhclo", homeDir() + "\\");
        if (!f.empty()) go(fs::u8path(f).parent_path().u8string());
#endif
    }
    if (ImGui::Button("Open your data folder", ImVec2(lw, 0))) {
#ifdef _WIN32
        ShellExecuteW(nullptr, L"open", fs::u8path(userDataDir()).wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#endif
    }
    ImGui::TextDisabled("Dropping a .zip on the window installs it too.");
}

static void downloadLeft(float lw) {
    installLeft(lw);
    groupBox("MakeHuman's asset packs");
    notYet("Clothes, hair, shoes, hats, poses, skins and more from MakeHuman's community (all CC0), into your own data folder.");
    if (dlBusy) {
        ImGui::Text("%s", dlName.empty() ? "Getting the list..." : ("Downloading " + dlName).c_str());
        ImGui::ProgressBar(dlTotal > 0 ? (float)dlDone / dlTotal : 0.0f, ImVec2(lw, 0));
        if (ImGui::Button("Stop", ImVec2(lw, 0))) dlStop = true;
        return;
    }
    if (packs.empty()) { if (ImGui::Button("Get the list", ImVec2(lw, 30))) { loadPacksHave(); dlName.clear(); startDownload("", true); } return; }
    ImGui::BeginChild("packs");
    std::lock_guard<std::mutex> g(dlMtx);
    for (const auto& p : packs) {
        ImGui::PushID(p.c_str());
        const bool have = packsHave.count(p) > 0;
        if (have) ImGui::PushStyleColor(ImGuiCol_Button, col4(C_DEEP));
        if (ImGui::Button(have ? "Again" : "Get", ImVec2(60, 0))) startDownload(p, false);
        if (have) ImGui::PopStyleColor();
        ImGui::SameLine(); ImGui::TextUnformatted(p.c_str());
        ImGui::PopID();
    }
    ImGui::EndChild();
}

static void dataView() {
    groupBox("Data");
    int shown = 0; for (size_t t = 0; t + 2 < body.tri.size(); t += 3) ++shown;
    ImGui::Text("Base mesh: %d vertices, %d triangles", (int)(body.base.size() / 3), shown);
    ImGui::Text("Targets: %d, modifiers: %d", (int)body.targets.size(), (int)body.mods.size());
    ImGui::Text("Proxies worn: %d", (int)body.proxies.size());
    for (const auto& p : body.proxies) ImGui::BulletText("%s (%s): %d vertices", p.name.c_str(), p.kind.c_str(), (int)(p.pos.size() / 3));
    ImGui::Text("Skeleton for posing: %d bones", (int)rig.bones.size());
    if (!chosen.empty()) ImGui::Text("Chosen skeleton: %s, %d bones", chosen.name.c_str(), (int)chosen.bones.size());
    groupBox("Folders");
    ImGui::TextWrapped("Program data: %s", dataDir.c_str());
    ImGui::TextWrapped("MakeHuman assets: %s", assetsDir().empty() ? "(none)" : assetsDir().c_str());
    ImGui::TextWrapped("Your data: %s", userDataDir().c_str());
    ImGui::TextWrapped("MakeHuman's own data: %s", makehumanDataDir().empty() ? "(MakeHuman isn't installed)" : makehumanDataDir().c_str());
}
static void logsView() {
    groupBox("Logs");
    if (ImGui::Button("Clear")) logs.clear();
    ImGui::BeginChild("lg");
    for (const auto& l : logs) ImGui::TextUnformatted(l.c_str());
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) ImGui::SetScrollHereY(1);
    ImGui::EndChild();
}

// ---------- the categories without their own code yet show what's coming, honestly ----------
static void notYet(const char* what) {
    ImGui::PushTextWrapPos(0);
    ImGui::TextDisabled("%s", what);
    ImGui::PopTextWrapPos();
}

static const char* SHORTCUTS[][2] = {
    {"Ctrl+Z", "Undo"}, {"Ctrl+Y", "Redo"}, {"Ctrl+M", "Modelling"}, {"Ctrl+S", "Save"}, {"Ctrl+L", "Load"}, {"Ctrl+E", "Export"},
    {"Ctrl+R", "Rendering"}, {"Ctrl+H", "Help"}, {"Ctrl+W", "Wireframe"}, {"Ctrl+Alt+S", "Smooth"}, {"Ctrl+B", "Show the bones"},
    {"Ctrl+Q", "Quit"}, {"1 / Ctrl+1", "Front / back view"}, {"3 / Ctrl+3", "Right / left side"}, {"7 / Ctrl+7", "Top / bottom view"},
    {"Home", "The whole person, from the front"}, {"F12", "A picture of the view"}};
static void goTo(int c, const char* t) { forcedTab = c; forcedTask = t; }
static void shortcuts() {
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantTextInput) return;
    auto key = [&](ImGuiKey k) { return ImGui::IsKeyPressed(k, false); };
    if (io.KeyCtrl && !io.KeyAlt) {
        if (key(ImGuiKey_M)) goTo(CAT_MODELLING, "Main");
        if (key(ImGuiKey_S)) goTo(CAT_FILES, "Save");
        if (key(ImGuiKey_L)) goTo(CAT_FILES, "Load");
        if (key(ImGuiKey_E)) goTo(CAT_FILES, "Export");
        if (key(ImGuiKey_R)) goTo(CAT_RENDERING, "Render");
        if (key(ImGuiKey_H)) goTo(CAT_HELP, "About");
        if (key(ImGuiKey_W)) Viewer::look().wire = !Viewer::look().wire;
        if (key(ImGuiKey_B)) { showBones = !showBones; Viewer::look().bones = showBones; }
        if (key(ImGuiKey_Q)) quit = true;
    }
    if (io.KeyCtrl && io.KeyAlt && key(ImGuiKey_S)) Viewer::look().smooth = !Viewer::look().smooth;
    if (!io.KeyAlt) {
        const bool back = io.KeyCtrl;
        if (key(ImGuiKey_1) || key(ImGuiKey_Keypad1)) Viewer::focusBody(back ? 3.14159265f : 0, 0.05f);
        if (key(ImGuiKey_3) || key(ImGuiKey_Keypad3)) Viewer::focusBody(back ? -1.5707963f : 1.5707963f, 0.05f);
        if (key(ImGuiKey_7) || key(ImGuiKey_Keypad7)) Viewer::focusBody(0, back ? -1.4f : 1.4f);
        if (key(ImGuiKey_Home)) Viewer::focusBody(0, 0.05f);
    }
}
static void frame() {
    ImGuiIO& io = ImGui::GetIO();
    animate(io.DeltaTime);
    massStep();
    shortcuts();
    { std::lock_guard<std::mutex> g(dlMtx); if (dlFinished) { dlFinished = false; say(dlResult); loadPacksHave(); libCache.clear(); } }
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
        else if (t == "Custom") customLeft(lw);
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
            groupBox("Settings");
            notYet(("Kept in " + settingsFile() + " when Pygmalion closes.").c_str());
            if (ImGui::Button("Restore the defaults", ImVec2(lw, 0))) {
                prefs = Prefs();
                Viewer::Look& lk = Viewer::look(); lk.turnSpeed = lk.moveSpeed = lk.zoomSpeed = 1; lk.invertTurn = lk.invertZoom = false; lk.bgImage.clear(); lk.bgOpacity = 1;
                say("Settings back to their defaults");
            }
        } else if (t == "Background") backgroundLeft(lw);
        else if (t == "Mouse") {
            Viewer::Look& lk = Viewer::look();
            groupBox("Camera");
            ImGui::TextWrapped("Left drag: turn. Right or middle drag: move. Wheel: zoom.");
            ImGui::SetNextItemWidth(lw); ImGui::SliderFloat("##ts", &lk.turnSpeed, 0.1f, 5, "Turn speed: %.1f");
            ImGui::SetNextItemWidth(lw); ImGui::SliderFloat("##ms", &lk.moveSpeed, 0.1f, 5, "Move speed: %.1f");
            ImGui::SetNextItemWidth(lw); ImGui::SliderFloat("##zs", &lk.zoomSpeed, 0.1f, 5, "Zoom speed: %.1f");
            ImGui::Checkbox("Invert turning", &lk.invertTurn);
            ImGui::Checkbox("Invert the wheel", &lk.invertZoom);
            if (ImGui::Button("Defaults", ImVec2(lw, 0))) { lk.turnSpeed = lk.moveSpeed = lk.zoomSpeed = 1; lk.invertTurn = lk.invertZoom = false; }
        } else {
            groupBox("Shortcuts");
            if (ImGui::BeginTable("keys", 2, ImGuiTableFlags_RowBg)) {
                for (const auto& k : SHORTCUTS) { ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::TextUnformatted(k[0]); ImGui::TableNextColumn(); ImGui::TextDisabled("%s", k[1]); }
                ImGui::EndTable();
            }
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
        } else if (t == "Statistics") statistics();
        else if (t == "Material Editor") materialEditor(lw);
        else if (t == "Expression mixer") mixerLeft(lw);
        else if (t == "Targets") targetsView(lw);
        else if (t == "Save targets") saveTargetsLeft(lw);
        else if (t == "Mass produce") massLeftPanel(lw);
        else if (t == "Scripting") scriptingLeft(lw);
        else if (t == "Shell") { groupBox("Shell"); notYet("MakeHuman's shell runs Python; Pygmalion's runs its own commands, the same as Scripting. Up and down bring back what you typed; clear empties it."); }
        else if (t == "Download") downloadLeft(lw);
#ifdef _WIN32
        else if (t == "Socket") socketLeft(lw);
#endif
        else if (t == "Data") dataView();
        else logsView();
    } else if (cat == CAT_HELP) {
        if (t == "About") { groupBox("Pygmalion " PYGMALION_VERSION); ImGui::TextWrapped("People in 3D with sliders: MakeHuman, remade in C++ by Cursed Entertainment. Farica\xe2\x98\x86Kimora \xc2\xa9 Cursed Entertainment."); }
        else { groupBox("Licence"); ImGui::TextWrapped("The code is AGPL-3.0 (it ports MakeHuman's code). The body, its shapes and the assets are MakeHuman's, CC0."); }
    } else if (cat == CAT_GEOMETRIES) geometriesLeft(t, lw);
    else if (cat == CAT_MATERIALS) materialsLeft(lw);
    else if (cat == CAT_POSE) {
        std::string picked;
        if (t == "Skeleton") {
            groupBox("Skeleton");
            if (chooser("sk", filesIn("rigs", ".mhskel"), skeletonFile, picked, lw)) { remember("Skeleton"); setSkeletonFile(picked); say(picked.empty() ? "No skeleton" : niceName(picked) + " skeleton (exported with the model)"); }
            if (ImGui::Checkbox("Show the bones", &showBones)) Viewer::look().bones = showBones;
            if (!chosen.empty()) { ImGui::TextDisabled("%s: %d bones", chosen.name.c_str(), (int)chosen.bones.size()); if (!chosen.description.empty()) { ImGui::PushTextWrapPos(lw); ImGui::TextDisabled("%s", chosen.description.c_str()); ImGui::PopTextWrapPos(); } }
        } else if (t == "Pose") {
            groupBox("Pose");
            if (chooser("po", filesIn("poses", ".bvh"), poseFile, picked, lw)) setPoseFile(picked);
        } else if (t == "Animations") {
            groupBox("Animations");
            if (chooser("an", filesIn("animations", ".bvh"), animFile, picked, lw)) setAnimation(picked);
            if (!animFrames.empty()) {
                groupBox("Playback");
                if (ImGui::Button(animPlaying ? "Pause" : "Play", ImVec2(lw / 2 - 4, 0))) animPlaying = !animPlaying;
                ImGui::SameLine();
                if (ImGui::Button("Stop", ImVec2(lw / 2 - 4, 0))) { animPlaying = false; animFrame = 0; bodyPose = animFrames[0]; dirty = true; }
                int fr = (int)animFrame;
                ImGui::SetNextItemWidth(lw);
                if (ImGui::SliderInt("##frame", &fr, 0, (int)animFrames.size() - 1, "Frame %d")) { animFrame = (float)fr; bodyPose = animFrames[fr]; dirty = true; }
                ImGui::SetNextItemWidth(lw);
                ImGui::SliderFloat("##speed", &animSpeed, 0.1f, 3, "Speed %.2fx");
                ImGui::TextDisabled("%d frames at %.0f a second", (int)animFrames.size(), 1 / std::max(animFrameTime, 1e-3f));
            }
        } else {
            groupBox("Expression");
            if (chooser("ex", filesIn("expressions", ".mhpose"), expressionFile, picked, lw)) setExpressionFile(picked);
            if (filesIn("expressions", ".mhpose").empty()) notYet("MakeHuman's ready-made expressions come with its asset pack; the mixer on the right makes any expression now.");
        }
    }
    else if (cat == CAT_RENDERING) renderingLeft(t, lw);
    ImGui::End();

    ImGui::SetNextWindowPos(ImVec2(W - sideW, top)); ImGui::SetNextWindowSize(ImVec2(sideW, H - top - statusH));
    ImGui::Begin("right", nullptr, fixed);
    if (loadErr.empty()) {
        if (cat == CAT_MODELLING) {
            if (t == "Measure") { if (const Human::Tab* tb = tabNamed(t)) modTabRight(*tb); statistics(); }
            else if (t != "Main" && t != "Random" && t != "Custom") { if (const Human::Tab* tb = tabNamed(t)) modTabRight(*tb); }
            else if (t == "Main") statistics();
        } else if (cat == CAT_FILES) filesRight();
        else if (cat == CAT_UTILITIES && t == "Scripting") scriptingRight();
        else if (cat == CAT_UTILITIES && t == "Shell") shellView();
        else if ((cat == CAT_POSE && t == "Expression") || (cat == CAT_UTILITIES && t == "Expression mixer")) {
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
    ImGui::SetNextWindowPos(ImVec2(sideW, top + toolbarH)); ImGui::SetNextWindowSize(ImVec2(W - 2 * sideW, H - top - toolbarH - statusH));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("view", nullptr, fixed);
    if (dirty && loadErr.empty()) {
        Human::apply(body);
        if (!rig.empty()) Skel::build(rig, body.pos);
        if (!chosen.empty()) Skel::build(chosen, body.pos);
        repose();
        Viewer::update(body);
        dirty = false;
    }
    const ImVec2 a = ImGui::GetCursorScreenPos(), size = ImGui::GetContentRegionAvail();
    ImGui::InvisibleButton("v", size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);
    const unsigned tex = Viewer::render((int)size.x, (int)size.y, ImGui::IsItemHovered() || ImGui::IsItemActive());
    ImGui::GetWindowDrawList()->AddImage((ImTextureID)(intptr_t)tex, a, ImVec2(a.x + size.x, a.y + size.y), ImVec2(0, 1), ImVec2(1, 0));
    if (cat == CAT_RENDERING && task[CAT_RENDERING] == "Viewer" && renderedTex) {   // the last render, fitted
        const float k = std::min(size.x / renderedW, size.y / renderedH);
        const ImVec2 sz(renderedW * k, renderedH * k), at(a.x + (size.x - sz.x) / 2, a.y + (size.y - sz.y) / 2);
        ImGui::GetWindowDrawList()->AddRectFilled(a, ImVec2(a.x + size.x, a.y + size.y), C_BG);
        ImGui::GetWindowDrawList()->AddImage((ImTextureID)(intptr_t)renderedTex, at, ImVec2(at.x + sz.x, at.y + sz.y));
    }
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
    std::string capture, file, describeWords, startPose, startScript;
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
        else if (args[i] == "--script" && i + 1 < args.size()) startScript = args[++i];
        else if (args[i] == "--socket") socketArg = true;   // the socket server on for this run (Utilities > Socket)
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
    if (!userDataDir().empty()) body.roots.push_back(userDataDir());
    if (!makehumanDataDir().empty()) body.roots.push_back(makehumanDataDir());
    body.roots.push_back(dataDir);
    loadSettings();
    loadCustomTargets();
    Human::defaultGear(body);
    { std::string err; if (!Skel::load(rig, (fs::u8path(dataDir) / "rigs" / "default.mhskel").u8string(), err)) say("No skeleton: " + err); }
    loadFaceUnits();
    category.clear();
    if (!file.empty()) { std::string err; if (!loadModel(file, err)) say(err); }
    if (!describeWords.empty()) { snprintf(prompt, sizeof prompt, "%s", describeWords.c_str()); understood = Human::describe(body, describeWords); }
    if (startYaw > -999) { Human::apply(body); Viewer::update(body); Viewer::focusBody(startYaw * 3.14159265f / 180, 0.05f); }
    if (!startPose.empty()) setPoseFile((fs::u8path(dataDir) / "poses" / (startPose + ".bvh")).u8string());
    for (const auto& u : startUnits) {
        const size_t eq = u.find('=');
        for (size_t k = 0; k < unitNames.size(); ++k) if (unitNames[k] == u.substr(0, eq)) unitW[k] = eq == std::string::npos ? 1.0f : (float)atof(u.c_str() + eq + 1);
    }
    if (!startScript.empty()) {   // a script file run at the start (and its output in script-log.txt beside it)
        std::ifstream in(fs::u8path(startScript)); std::ostringstream ss; ss << in.rdbuf();
        runScript(ss.str());
        std::ofstream lo(fs::u8path(startScript).replace_extension(".log")); lo << scriptLog;
    }
    dirty = true;
#ifdef _WIN32
    if ((sockCfg.accept || socketArg) && capture.empty()) socketOpen();   // as MakeHuman: the server comes back on if it was on
#endif
    if (capture.empty()) { SDL_ShowWindow(window); SDL_MaximizeWindow(window); SDL_RaiseWindow(window); }
    int frames = 0;
    while (!quit) {
#ifdef _WIN32
        socketPoll();
#endif
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            ImGui_ImplSDL2_ProcessEvent(&e);
            if (e.type == SDL_QUIT) quit = true;
            if (e.type == SDL_DROPFILE) {
                const std::string f = e.drop.file; std::string err;
                std::string x = fs::u8path(f).extension().u8string(); for (char& c : x) c = (char)tolower((unsigned char)c);
                std::error_code ec;
                if (x == ".zip" || fs::is_directory(fs::u8path(f), ec)) { std::string said; if (installAsset(f, "", said, err)) say(said); else say(err); }
                else { remember("Open"); if (loadModel(f, err)) dirty = true; else say(err); }
                SDL_free(e.drop.file);
            }
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
#ifdef _WIN32
    Sock::stop();
#endif
    if (capture.empty()) saveSettings();
    ImGui_ImplOpenGL3_Shutdown(); ImGui_ImplSDL2_Shutdown(); ImGui::DestroyContext();
    SDL_GL_DeleteContext(gl); SDL_DestroyWindow(window); SDL_Quit();
    return 0;
}
