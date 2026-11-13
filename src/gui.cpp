// Pygmalion - make people in 3D with sliders, on your own PC, in C++. Part of Atelier.
// MakeHuman's body, its 1,280 shapes and every one of its sliders (gender, age, muscle, weight, height, proportions,
// ethnicity, then the face, the torso, the arms and legs, measurements and body shapes); words that set the sliders;
// a random person; MakeHuman's own .mhm files; GLB, OBJ and STL out.
//
//   Pygmalion [model.mhm]
//   Pygmalion --capture out.bmp [--describe "words"] [--tab N]      a screenshot, then quit
#include "profile.h"
#include "human.h"
#include "skeleton.h"
#include "model.h"
#include "splash.h"
#include "viewer.h"
#include "export.h"
#include "mhscene.h"
#include "profile.h"
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

static ImU32 C_BG = IM_COL32(0x1a, 0x1a, 0x1a, 255), C_PANEL = IM_COL32(0x24, 0x24, 0x24, 255),
                   C_LINE = IM_COL32(0x3a, 0x3a, 0x3a, 255), C_TEXT = IM_COL32(0xe6, 0xe6, 0xe6, 255),
                   C_DIM = IM_COL32(0x9a, 0x9a, 0x9a, 255), C_PINK = IM_COL32(0xe0, 0x40, 0x8a, 255),
                   C_DEEP = IM_COL32(0xc2, 0x18, 0x6b, 255), C_WELL = IM_COL32(0x14, 0x14, 0x14, 255);
static ImVec4 col4(ImU32 c) { return ImGui::ColorConvertU32ToFloat4(c); }

// the themes (Settings > General > Theme): Pygmalion's own, MakeHuman's (data/themes/makehuman.qss, its gradients
// made solid), and the native look (Dear ImGui's own)
static int theme = 0;
static void themeColours(int t) {
    if (t == 1) {   // MakeHuman: #323232 grey, white text, orange
        C_BG = IM_COL32(0x24, 0x24, 0x24, 255); C_PANEL = IM_COL32(0x32, 0x32, 0x32, 255); C_LINE = IM_COL32(0x52, 0x52, 0x52, 255);
        C_TEXT = IM_COL32(0xff, 0xff, 0xff, 255); C_DIM = IM_COL32(0xb1, 0xb1, 0xb1, 255); C_PINK = IM_COL32(0xff, 0xa0, 0x2f, 255);
        C_DEEP = IM_COL32(0xd7, 0x80, 0x1a, 255); C_WELL = IM_COL32(0x21, 0x21, 0x21, 255);
    } else {        // Pygmalion (and the native look's accents)
        C_BG = IM_COL32(0x1a, 0x1a, 0x1a, 255); C_PANEL = IM_COL32(0x24, 0x24, 0x24, 255); C_LINE = IM_COL32(0x3a, 0x3a, 0x3a, 255);
        C_TEXT = IM_COL32(0xe6, 0xe6, 0xe6, 255); C_DIM = IM_COL32(0x9a, 0x9a, 0x9a, 255); C_PINK = IM_COL32(0xe0, 0x40, 0x8a, 255);
        C_DEEP = IM_COL32(0xc2, 0x18, 0x6b, 255); C_WELL = IM_COL32(0x14, 0x14, 0x14, 255);
    }
}
static void applyStyle() {
    themeColours(theme);
    ImGuiStyle& s = ImGui::GetStyle();
    if (theme == 2) {   // the native look
        s = ImGuiStyle(); ImGui::StyleColorsDark(&s);
        C_PINK = ImGui::ColorConvertFloat4ToU32(s.Colors[ImGuiCol_CheckMark]); C_DEEP = ImGui::ColorConvertFloat4ToU32(s.Colors[ImGuiCol_ButtonActive]);
        C_PANEL = ImGui::ColorConvertFloat4ToU32(s.Colors[ImGuiCol_WindowBg]); C_TEXT = ImGui::ColorConvertFloat4ToU32(s.Colors[ImGuiCol_Text]);
        C_DIM = ImGui::ColorConvertFloat4ToU32(s.Colors[ImGuiCol_TextDisabled]);
        return;
    }
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
static std::string folderDialog() {
    std::string out;
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);   // (once is enough; again is harmless)
    IFileOpenDialog* d = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&d)))) {
        DWORD o = 0; d->GetOptions(&o); d->SetOptions(o | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
        if (SUCCEEDED(d->Show(nullptr))) { IShellItem* it = nullptr; if (SUCCEEDED(d->GetResult(&it))) { PWSTR p = nullptr; if (SUCCEEDED(it->GetDisplayName(SIGDN_FILESYSPATH, &p))) { out = fs::path(p).u8string(); CoTaskMemFree(p); } it->Release(); } }
        d->Release();
    }
    return out;
}
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
static std::string configFile() {   // %APPDATA%\Pygmalion\pygmalion.conf: one line, the home folder (MakeHuman's makehuman.conf)
    PWSTR p = nullptr; std::string f;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &p))) { f = (fs::path(p) / "Pygmalion" / "pygmalion.conf").u8string(); CoTaskMemFree(p); }
    return f;
}
static std::string homeDir() {
    {   // a home folder of the user's own choosing
        static std::string chosen; static bool read = false;
        if (!read) { read = true; std::ifstream in(fs::u8path(configFile())); std::string l; if (std::getline(in, l)) { while (!l.empty() && (l.back() == '\r' || l.back() == ' ')) l.pop_back(); std::error_code ec; if (!l.empty() && fs::is_directory(fs::u8path(l), ec)) chosen = l; } }
        if (!chosen.empty()) return chosen;
    }
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
static float randomAmount = 1.0f; static bool randMacros = true, randFace = true, randBody = true;
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
// each line's level (MakeHuman's log levels: Debug, Message, Notice, Warning, Error), told by what it says
static std::vector<int> logLevels;
static std::vector<char> logPicked;
static int logLevelOf(const std::string& s) {
    std::string l = s; for (auto& c : l) c = (char)tolower((unsigned char)c);
    for (const char* w : {"couldn't", "failed", "error", "can't", "cannot", "not valid", "doesn't fit", "not found", "unknown"}) if (l.find(w) != std::string::npos) return 4;
    for (const char* w : {"no ", "not ", "isn't", "too ", "already", "warning", "missing"}) if (l.find(w) != std::string::npos) return 3;
    for (const char* w : {"saved", "loaded", "exported", "installed", "made", "done", "rendered"}) if (l.find(w) != std::string::npos) return 2;
    return 1;
}
static void say(const std::string& s) {
    toast = s; toastUntil = now() + 6;
    char t[16]; const Uint32 ms = SDL_GetTicks(); snprintf(t, sizeof t, "%02u:%02u ", ms / 60000 % 60, ms / 1000 % 60);
    logs.push_back(t + s); logLevels.push_back(logLevelOf(s)); logPicked.push_back(0);
    if (logs.size() > 500) { logs.erase(logs.begin()); logLevels.erase(logLevels.begin()); logPicked.erase(logPicked.begin()); }
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
    // MakeHuman's Settings > General
    bool realWeight = false;       // the weight in kg (else MakeHuman's relative %)
    int tagMode = 0;               // the tag filter: OR, AND, NOT OR, NOT AND
    int tagCount = 5;              // tags a model can have (Files > Save)
    bool nameTags = false;         // the choosers show each file's own name (its "name" line)
    bool preload = true;           // macro targets loaded at the start (Pygmalion loads every target then)
    bool restoreWindow = false;    // the window opens where it was
    bool hdpi = false;             // the interface scaled to the screen's DPI
    bool noSampleBuffers = false;  // no anti-aliasing in renders
    bool sliderEdit = false;       // a number box beside each slider (MakeHuman's "Slider's edit widget")
    int win[4] = {0, 0, 0, 0};     // the window's place and size, kept when restoreWindow is on
};
static Prefs prefs;
// Settings > Plugins and User Plugins: the parts left out ("Category/Task", as at the start) and the scripts run at the start
static std::set<std::string> excludedTasks, excludedAtStart;
static std::vector<std::string> activeUserPlugins;
static std::set<std::string>& startExclusions() { return excludedAtStart; }
static std::string settingsFile() { return (fs::u8path(homeDir()) / "settings.ini").u8string(); }
static void saveSettings() {
    std::ofstream o(fs::u8path(settingsFile()));
    if (!o) return;
    const Viewer::Look& lk = Viewer::look();
    o << "sliderImages=" << prefs.sliderImages << "\nshowValues=" << prefs.showValues << "\ncamFollows=" << prefs.camFollows
      << "\nrealtime=" << prefs.realtime << "\nmetric=" << prefs.metric
      << "\nturnSpeed=" << lk.turnSpeed << "\nmoveSpeed=" << lk.moveSpeed << "\nzoomSpeed=" << lk.zoomSpeed
      << "\ninvertTurn=" << lk.invertTurn << "\ninvertZoom=" << lk.invertZoom
      << "\nmhShaders=" << lk.mhShaders << "\nbgImage=" << lk.bgImage << "\nbgOpacity=" << lk.bgOpacity << "\n";
    o << "realtimeNormals=" << lk.normalsWhileDragging << "\nrealtimeFitting=" << lk.fitWhileDragging << "\nrealWeight=" << prefs.realWeight
      << "\ntagMode=" << prefs.tagMode << "\ntagCount=" << prefs.tagCount << "\nnameTags=" << prefs.nameTags << "\npreload=" << prefs.preload
      << "\nrestoreWindow=" << prefs.restoreWindow << "\nhdpi=" << prefs.hdpi << "\nnoSampleBuffers=" << prefs.noSampleBuffers << "\nsliderEdit=" << prefs.sliderEdit
      << "\nexcluded=" << [] { std::string s; for (const auto& e : excludedTasks) s += (s.empty() ? "" : "|") + e; return s; }()
      << "\nuserPlugins=" << [] { std::string s; for (const auto& e : activeUserPlugins) s += (s.empty() ? "" : "|") + e; return s; }()
      << "\ntheme=" << theme << "\nwindow=" << prefs.win[0] << " " << prefs.win[1] << " " << prefs.win[2] << " " << prefs.win[3] << "\n";
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
        else if (k == "mhShaders") lk.mhShaders = b;
        else if (k == "realtimeNormals") lk.normalsWhileDragging = b; else if (k == "realtimeFitting") lk.fitWhileDragging = b;
        else if (k == "realWeight") prefs.realWeight = b; else if (k == "tagMode") prefs.tagMode = std::clamp(atoi(v.c_str()), 0, 3);
        else if (k == "tagCount") prefs.tagCount = std::clamp(atoi(v.c_str()), 1, 20); else if (k == "nameTags") prefs.nameTags = b;
        else if (k == "preload") prefs.preload = b; else if (k == "restoreWindow") prefs.restoreWindow = b; else if (k == "hdpi") prefs.hdpi = b;
        else if (k == "noSampleBuffers") prefs.noSampleBuffers = b; else if (k == "sliderEdit") prefs.sliderEdit = b;
        else if (k == "theme") theme = std::clamp(atoi(v.c_str()), 0, 2);
        else if (k == "excluded") { excludedTasks.clear(); std::istringstream es(v); std::string one; while (std::getline(es, one, '|')) if (!one.empty()) excludedTasks.insert(one); excludedAtStart = excludedTasks; }
        else if (k == "userPlugins") { activeUserPlugins.clear(); std::istringstream es(v); std::string one; while (std::getline(es, one, '|')) if (!one.empty()) activeUserPlugins.push_back(one); }
        else if (k == "window") { std::istringstream ws(v); for (int& c : prefs.win) ws >> c; }
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
    if (prefs.sliderEdit) {   // MakeHuman's edit widget: the value typed in
        float e = m.value;
        ImGui::SetNextItemWidth(sw);
        if (ImGui::InputFloat("##e", &e, 0, 0, "%.6f", ImGuiInputTextFlags_EnterReturnsTrue)) { remember(m.label); Human::setModifier(body, i, std::clamp(e, m.min(), 1.0f)); dirty = true; }
    }
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
// MakeHuman's foot posing (plugins/2_foot_posing.py): clothes with "special_pose foot <name>" (shoes with heels) put
// that BVH's foot and toe bones over whatever pose is on; taking them off takes it away
static const char* FOOT_BONES[] = {"foot", "toe1-1", "toe1-2", "toe2-1", "toe2-2", "toe2-3", "toe3-1", "toe3-2", "toe3-3", "toe4-1", "toe4-2", "toe4-3", "toe5-1", "toe5-2", "toe5-3"};
static std::string footPoseFile;   // the foot pose in use ("" none), for the Pose tab
static const Skel::Pose* footPose() {
    static std::map<std::string, Skel::Pose> cache;
    std::string want;
    for (const auto& p : body.proxies) {
        if (p.kind != "clothes") continue;
        auto it = p.specialPose.find("foot"); if (it == p.specialPose.end()) continue;
        // thoroughFindFile over the user's and the system's special_poses/foot, and beside the clothes
        const std::string folder = fs::u8path(p.file).parent_path().u8string();
        for (const std::string& n : {it->second, it->second + ".bvh"}) {
            for (const std::string& sub : {std::string("special_poses/foot/") + n, n}) { want = Human::resolveAsset(sub, folder); if (!want.empty()) break; }
            if (!want.empty()) break;
        }
        static std::string told; if (want.empty() && told != p.file) { told = p.file; say("No foot pose " + it->second + " for " + p.name); }
    }
    footPoseFile = want;
    if (want.empty()) return nullptr;
    auto c = cache.find(want);
    if (c == cache.end()) {
        std::vector<Skel::Pose> frames; std::string err;
        if (!Skel::loadBvh(want, frames, err, nullptr, &rig) || frames.empty()) { say("Foot pose: " + err); cache[want] = Skel::Pose(); }
        else cache[want] = frames[0];
        c = cache.find(want);
    }
    return c->second.empty() ? nullptr : &c->second;
}
static void repose() {
    PROF("repose (pose + expression)");
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
    if (const Skel::Pose* foot = footPose())   // animation.mixPoses: the foot pose's foot and toe bones replace the pose's
        for (const char* side : {".L", ".R"}) for (const char* b : FOOT_BONES) { auto it = foot->bones.find(std::string(b) + side); if (it != foot->bones.end()) p.bones[it->first] = it->second; }
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
// MakeHuman's file chooser around the pictures: a search box, the tag filter (its mode in Settings > General), sorting
struct ChooserState { char search[64] = ""; std::set<std::string> tags; int sort = 0; };
static std::map<std::string, ChooserState> chooserStates;
static std::string fileLabel(const std::string& f) {   // the file's own name with Use Name Tags on, else its file name
    if (prefs.nameTags) { const std::string t = Human::fileTitle(f); if (!t.empty()) { std::string l = t; std::replace(l.begin(), l.end(), '_', ' '); if (!l.empty()) l[0] = (char)toupper((unsigned char)l[0]); return l; } }
    return niceName(f);
}
static std::vector<std::string> chooserFilter(const std::string& id, const std::vector<std::string>& files, float w) {
    ChooserState& st = chooserStates[id];
    ImGui::SetNextItemWidth(w); ImGui::InputTextWithHint("##search", "Search", st.search, sizeof st.search);
    std::set<std::string> all;
    for (const auto& f : files) for (const auto& t : Human::fileTags(f)) all.insert(t);
    static const char* MODES[] = {"OR", "AND", "NOT OR", "NOT AND"};
    if (!all.empty() && ImGui::TreeNode("tf", "Tag Filter [Mode : %s]", MODES[prefs.tagMode])) {
        for (const auto& t : all) {
            std::string title = t; bool up = true; for (auto& c : title) { if (up) c = (char)toupper((unsigned char)c); up = c == ' '; }
            bool on = st.tags.count(t) > 0;
            if (ImGui::Checkbox(title.c_str(), &on)) { if (on) st.tags.insert(t); else st.tags.erase(t); }
        }
        if (!st.tags.empty() && ImGui::SmallButton("Clear")) st.tags.clear();
        ImGui::TreePop();
    }
    static const char* SORTS[] = {"By name", "By created", "By modified", "By size"};
    ImGui::SetNextItemWidth(w); ImGui::Combo("##sort", &st.sort, SORTS, 4);
    std::string q = st.search; for (auto& c : q) c = (char)tolower((unsigned char)c);
    std::vector<std::string> out;
    for (const auto& f : files) {
        if (!q.empty()) { std::string l = fileLabel(f) + " " + fs::u8path(f).stem().u8string(); for (auto& c : l) c = (char)tolower((unsigned char)c); if (l.find(q) == std::string::npos) continue; }
        if (!st.tags.empty()) {   // TagFilter.filter
            const auto& ft = Human::fileTags(f);
            size_t hit = 0; for (const auto& t : st.tags) if (std::find(ft.begin(), ft.end(), t) != ft.end()) ++hit;
            const bool keep = prefs.tagMode == 0 ? hit > 0 : prefs.tagMode == 1 ? hit == st.tags.size() : prefs.tagMode == 2 ? hit == 0 : hit != st.tags.size();
            if (!keep) continue;
        }
        out.push_back(f);
    }
    std::error_code ec;
    auto key = [&](const std::string& f) -> double {
        if (st.sort == 1) return (double)fs::last_write_time(fs::u8path(f), ec).time_since_epoch().count();   // (Windows keeps no other date here)
        if (st.sort == 2) return (double)fs::last_write_time(fs::u8path(f), ec).time_since_epoch().count();
        return (double)fs::file_size(fs::u8path(f), ec);
    };
    if (st.sort == 0) std::sort(out.begin(), out.end(), [&](const std::string& a, const std::string& b) { std::string x = fileLabel(a), y = fileLabel(b); for (auto& c : x) c = (char)tolower((unsigned char)c); for (auto& c : y) c = (char)tolower((unsigned char)c); return x < y; });
    else std::stable_sort(out.begin(), out.end(), [&](const std::string& a, const std::string& b) { return key(a) < key(b); });
    return out;
}
static bool chooser(const char* id, const std::vector<std::string>& allFiles, const std::string& current, std::string& picked, float w, bool none = true, const char* noneLabel = "None") {
    bool ch = false;
    ImGui::PushID(id);
    const std::vector<std::string> files = chooserFilter(id, allFiles, w);
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
    for (const auto& f : files) cell(f, fileLabel(f));
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
        ImGui::PushID("clothes");
        const std::vector<std::string> shown = chooserFilter("clothes", files, lw);   // search, tag filter, sort
        ImGui::PopID();
        for (const auto& f : shown) {
            if (n % cols) ImGui::SameLine();
            ImGui::BeginGroup(); ImGui::PushID(n);
            const GLuint th = picture(fs::u8path(f).replace_extension(".thumb").u8string());
            const bool on = Human::isWorn(body, f);
            if (on) ImGui::PushStyleColor(ImGuiCol_Button, col4(C_DEEP));
            const bool click = th ? ImGui::ImageButton("b", (ImTextureID)(intptr_t)th, ImVec2(88, 88)) : ImGui::Button("?", ImVec2(96, 96));
            if (on) ImGui::PopStyleColor();
            std::string l = fileLabel(f); if (l.size() > 14) l = l.substr(0, 13) + ".";
            ImGui::TextDisabled("%s", l.c_str());
            ImGui::PopID(); ImGui::EndGroup();
            tip(fileLabel(f));
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

static std::vector<std::string> tasksOfAll(int c);
static std::set<std::string>& startExclusions();   // (with the settings)
static std::vector<std::string> tasksOf(int c) {
    std::vector<std::string> v = tasksOfAll(c);
    v.erase(std::remove_if(v.begin(), v.end(), [&](const std::string& t) { return startExclusions().count(std::string(CAT_NAMES[c]) + "/" + t) > 0; }), v.end());
    return v;
}
static std::vector<std::string> tasksOfAll(int c) {
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
    case CAT_SETTINGS: return {"Sliders", "General", "Background", "Mouse", "Shortcuts", "Plugins", "User Plugins"};
    case CAT_UTILITIES: return {"Describe", "Symmetry", "Statistics", "Material Editor", "Scene Editor", "Expression mixer", "Targets", "Save targets", "Mass produce", "Shell", "Scripting", "Download", "Socket", "Profile", "Example", "Data", "Logs"};
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
    macro("Weight", [&](float v) {
        if (!prefs.realWeight) snprintf(b, sizeof b, "Weight: %.2f%%", 50 + (150 - 50) * v);
        else { const float kg = Human::weightKg(body); if (prefs.metric) snprintf(b, sizeof b, "Weight: %.1f kg", kg); else snprintf(b, sizeof b, "Weight: %.1f lb", kg * 2.20462f); }
        return std::string(b); }, "");
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
    // MakeHuman's brassiere sizes (its syncBraSizes, its box switched off there): band from the underbust, cup from the difference
    const float bust = Human::measureCm(body, "measure/measure-bust-circ-decr|incr"), under = Human::measureCm(body, "measure/measure-underbust-circ-decr|incr");
    if (under > 0 && bust > 0) {
        groupBox("Brassiere size");
        static const char* EU[] = {"AA", "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K"};
        static const char* JP[] = {"AAA", "AA", "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K"};
        static const char* US[] = {"AA", "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K"};
        static const char* UK[] = {"AA", "A", "B", "C", "D", "DD", "E", "F", "FF", "G", "GG", "H"};
        const int mod = (int)under % 5;
        const float band = mod < 2.5f ? under - mod : under - mod + 5;
        int cup = std::clamp((int)std::lround((bust - under - 10) / 2), 0, 11);
        ImGui::Text("EU: %d%s", (int)band, EU[cup]);
        ImGui::Text("JP: %d%s", (int)band, JP[std::clamp((int)std::lround((bust - under - 5) / 2.5f), 0, 12)]);
        float usBand = under * 0.393700787f; usBand = ((int)usBand % 2) ? usBand + 5 : usBand + 4;
        ImGui::Text("US: %d%s", (int)usBand, US[cup]);
        ImGui::Text("UK: %d%s", (int)usBand, UK[cup]);
    }
}

// Random, as MakeHuman's: what to randomise, how symmetric, and back to the start
static bool rMacro = true, rFace = true, rBody = true, rHeight = false; static float rSym = 0.7f;
// Modelling > Random: MakeHuman's randomize() (plugins/0_modeling_8_random.py): the chosen groups' modifiers in a
// random order, each round its default with a spread by group (a mirror pair kept within "Symmetry" of each other),
// the pregnancy target only for a young woman (MakeHuman's own test), all at once and undoable
static float gaussIn(std::mt19937& rng, float lo, float hi, float mid, float sigmaFactor) {
    const float sigma = sigmaFactor * std::fabs(hi - lo);
    float v = sigma > 0 ? std::normal_distribution<float>(mid, sigma)(rng) : mid;
    if (v < lo) v = lo + std::fabs(v - lo); else if (v > hi) v = hi - std::fabs(v - hi);
    return std::clamp(v, lo, hi);
}
static std::string mirrorName(const Human::Modifier& m) {   // getSymmetricOpposite: an l- / r- word swapped
    std::vector<std::string> parts; std::stringstream ss(m.name); std::string p; bool found = false;
    while (std::getline(ss, p, '-')) { if (p == "l") { p = "r"; found = true; } else if (p == "r") { p = "l"; found = true; } parts.push_back(p); }
    if (!found) return "";
    std::string n; for (size_t k = 0; k < parts.size(); ++k) n += (k ? "-" : "") + parts[k];
    return m.group + "/" + n;
}
static void randomizeMh(float symmetry, bool macro, bool height, bool face, bool bodyParts, float amount) {
    std::vector<std::string> groups;
    if (macro) groups.insert(groups.end(), {"macrodetails", "macrodetails-universal", "macrodetails-proportions"});
    if (height) groups.push_back("macrodetails-height");
    if (face) groups.insert(groups.end(), {"eyebrows", "eyes", "chin", "forehead", "head", "mouth", "nose", "neck", "ears", "cheek"});
    if (bodyParts) groups.insert(groups.end(), {"pelvis", "hip", "armslegs", "stomach", "breast", "buttocks", "torso"});
    std::vector<int> mods;
    for (int i = 0; i < (int)body.mods.size(); ++i) if (std::find(groups.begin(), groups.end(), body.mods[i].group) != groups.end()) mods.push_back(i);
    static std::mt19937 rng{std::random_device{}()};
    std::shuffle(mods.begin(), mods.end(), rng);
    std::map<std::string, float> vals;
    static const std::set<std::string> transInOut = {"hip/hip-trans-in|out", "torso/torso-trans-in|out", "neck/neck-trans-in|out", "head/head-trans-in|out", "nose/nose-trans-in|out", "mouth/mouth-trans-in|out"};
    static const std::set<std::string> faceGroups = {"head", "forehead", "eyebrows", "neck", "eyes", "nose", "ears", "chin", "cheek", "mouth"};
    for (int i : mods) {
        const auto& m = body.mods[i];
        const std::string fn = m.fullName();
        if (vals.count(fn)) continue;
        const float lo = m.min(), hi = 1.0f, def = m.defaultValue;
        float sigma = 0.1f; bool have = false; float v = 0;
        if (fn == "forehead/forehead-nubian-less|more" || fn == "forehead/forehead-scale-vert-less|more") sigma = 0.02f;
        else if (transInOut.count(fn)) {
            if (symmetry == 1) v = def;
            else { const float w = std::fabs(hi - lo) * (1 - symmetry); v = gaussIn(rng, std::max(lo, def - w / 2), std::min(hi, def + w / 2), def, 0.1f * amount); }
            have = true;
        } else if (faceGroups.count(m.group)) sigma = 0.1f;
        else if (m.group == "macrodetails") sigma = 0.3f;
        if (!have) v = gaussIn(rng, lo, hi, def, sigma * amount);
        vals[fn] = v;
        const std::string sym = mirrorName(m);
        auto it = body.modByName.find(sym);
        if (!sym.empty() && it != body.modByName.end() && !vals.count(sym)) {
            if (symmetry == 1) vals[sym] = v;
            else {
                const auto& m2 = body.mods[it->second];
                const float dev = (1 - symmetry) * std::fabs(1.0f - m2.min()) / 2;
                const float sLo = std::max(m2.min(), std::min(v - dev, 1.0f)), sHi = std::max(m2.min(), std::min(v + dev, 1.0f));
                vals[sym] = gaussIn(rng, sLo, sHi, v, sigma * amount);
            }
        }
    }
    // (MakeHuman's own condition, kept as it is: in effect most people aren't pregnant)
    auto get = [&](const char* k, float d) { auto it = vals.find(k); return it == vals.end() ? d : it->second; };
    if (get("macrodetails/Gender", 0) > 0.5f || get("macrodetails/Age", 0.5f) < 0.2f || get("macrodetails/Age", 0.7f) < 0.75f)
        if (vals.count("stomach/stomach-pregnant-decr|incr")) vals["stomach/stomach-pregnant-decr|incr"] = 0;
    const bool sym = body.symmetry; body.symmetry = false;
    for (const auto& [fn, v] : vals) { auto it = body.modByName.find(fn); if (it != body.modByName.end()) Human::setModifier(body, it->second, v); }
    body.symmetry = sym;
}
static void randomLeft(float w) {
    groupBox("Randomize settings");
    ImGui::Checkbox("Macro", &rMacro); ImGui::Checkbox("Face", &rFace); ImGui::Checkbox("Body", &rBody); ImGui::Checkbox("Height", &rHeight);
    ImGui::SetNextItemWidth(w); ImGui::SliderFloat("##sym", &rSym, 0, 1, "Symmetry: %.2f");
    ImGui::SetNextItemWidth(w); ImGui::SliderFloat("##amt", &randomAmount, 0.1f, 2.0f, "Amount: %.2f");
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Pygmalion's own: the spread MakeHuman uses, times this (1 is MakeHuman's).");
    if (ImGui::Button("Randomize", ImVec2(w, 0))) { remember("Randomize"); redo.clear(); randomizeMh(rSym, rMacro, rHeight, rFace, rBody, randomAmount); dirty = true; }
    groupBox("Restore settings");
    if (ImGui::Button("Restore to defaults", ImVec2(w, 0))) { rMacro = rFace = rBody = true; rHeight = false; rSym = 0.7f; randomAmount = 1.0f; }
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
static char saveName[128] = "person";   // the file name (and the model's name when it has none of its own)
static bool saveModel(const std::string& path, const std::string& name, std::string& err) {
    {   // MakeHuman's camera line: rotation x, y in degrees, translation, zoom
        const Viewer::Look& lk = Viewer::look();
        body.camera[0] = lk.pitch * 57.29578f; body.camera[1] = -lk.yaw * 57.29578f; body.camera[2] = body.camera[3] = body.camera[4] = 0;
        body.camera[5] = lk.dist > 0 ? Viewer::bodyHeight() * 1.7f / lk.dist : 1;
        body.subdivide = lk.smooth;
    }
    if (!Human::saveMhm(body, path, name, err)) return false;
    std::ofstream o(fs::u8path(path), std::ios::app);
    if (!skeletonFile.empty()) o << "skeleton " << fs::u8path(skeletonFile).filename().u8string() << "\n";
    if (!poseFile.empty()) o << "pose " << relData(poseFile) << "\n";
    if (!expressionFile.empty()) o << "expression " << relData(expressionFile) << "\n";
    o << "subdivide " << (body.subdivide ? "True" : "False") << "\n";   // MakeHuman's last line
    return true;
}
static bool loadModel(const std::string& path, std::string& err) {
    if (!Human::loadMhm(body, path, err)) return false;
    Viewer::look().smooth = body.subdivide;
    if (body.hasCamera) { Viewer::Look& lk = Viewer::look(); lk.pitch = std::clamp(body.camera[0] / 57.29578f, -1.4f, 1.4f); lk.yaw = -body.camera[1] / 57.29578f; }
    if (!body.name.empty()) snprintf(saveName, sizeof saveName, "%s", body.name.c_str());
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
static int exportFormat = 0; static int exportUnits = 1; static bool exportFeet = true, exportPosed = true, exportAnim = false;
// MakeHuman's exporter options: Collada's orientation and bone axes, helper geometry, facial pose units, FBX/STL binary, OBJ normals
static int exportOrient = 0, exportBoneAxis = 0; static bool exportHelpers = false, exportFaceUnits = false, exportFbxBinary = true, exportStlBinary = false, exportObjNormals = false;
enum { EX_DAE, EX_FBX, EX_GLB, EX_OBJ, EX_OGRE, EX_STL, EX_BVH, EX_UV, EX_LIGHT, EX_COUNT };
static const char* EXPORT_NAMES[] = {"Collada (dae)", "Filmbox (fbx)", "glTF binary (glb)", "Wavefront obj", "Ogre3D", "Stereolithography (stl)", "Biovision Hierarchy (bvh)", "UV map (png)", "Lightmap (png)"};
static const char* EXPORT_EXT[] = {".dae", ".fbx", ".glb", ".obj", ".mesh.xml", ".stl", ".bvh", ".png", ".png"};
static const char* UNIT_NAMES[] = {"decimeter", "meter", "centimeter", "millimeter", "inch"};
static const float UNIT_SCALE[] = {1, 0.1f, 10, 100, 3.93700787f};
static const float UNIT_CM[] = {10, 100, 1, 0.1f, 2.54f};
static Mesh toMesh();
static bool exportTo(const std::string& p, int fmt, std::string& err) {
    PROF("export");
    const float sc = UNIT_SCALE[exportUnits];
    if (fmt == EX_GLB) { const Mesh m = toMesh(); return saveGlb(m, p, sc, false, err); }
    if (fmt == EX_STL) {   // MakeHuman's STL: binary or ASCII, feet on ground
        Export::Options o; o.scale = sc; o.feetOnGround = exportFeet; o.binary = exportStlBinary;
        return Export::saveStl(Export::parts(body, nullptr, exportPosed, exportHelpers), p, o, err);
    }
    if (fmt == EX_OBJ) {   // MakeHuman's OBJ: shared vertices and its quads, as posed now
        Export::Options o; o.scale = sc; o.cmPerUnit = UNIT_CM[exportUnits]; o.feetOnGround = exportFeet; o.posed = exportPosed; o.normals = exportObjNormals;
        return Export::saveObj(Export::withTexturesBeside(Export::parts(body, nullptr, exportPosed, exportHelpers), p), p, o, err);
    }
    if (fmt == EX_UV) return Export::saveUvMap(body, p, 2048, err);
    if (fmt == EX_LIGHT) return Export::saveLightmap(body, p, 1024, err);
    if (fmt == EX_BVH) {
        const Skel::Skeleton& sk = chosen.empty() ? rig : chosen;
        if (sk.empty()) { err = "No skeleton to export"; return false; }
        std::vector<Skel::Pose> frames;
        if (exportAnim && !animFrames.empty()) frames = animFrames;
        else if (!currentPose.empty()) frames.push_back(currentPose);
        float ground = 0; { float g[3]; if (exportFeet && Human::jointPos(body, "joint-ground", g)) ground = -g[1]; }
        const bool ok = Skel::saveBvh(sk, frames, p, sc, animFrameTime, err, &sk != &rig && Skel::usesReferences(sk) ? &rig : nullptr, ground);
        repose();   // (saving walked the skeleton through the frames)
        return ok;
    }
    Export::Options o; o.scale = sc; o.cmPerUnit = UNIT_CM[exportUnits]; o.feetOnGround = exportFeet; o.posed = exportPosed;
    o.binary = exportFbxBinary;
    if (fmt == EX_DAE) { o.orient = exportOrient; o.boneAxis = exportBoneAxis; if (exportFaceUnits) o.poseUnits = &faceUnits; }
    const Skel::Skeleton* sk = chosen.empty() ? nullptr : &chosen;
    const auto parts = Export::withTexturesBeside(Export::parts(body, sk, exportPosed && fmt != EX_FBX, exportHelpers), p);
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
        {
            std::string picked;
            if (chooser("models", modelFiles(), "", picked, w, false) && !picked.empty()) { std::string err; remember("Load"); if (loadModel(picked, err)) { dirty = true; say("Loaded " + fs::u8path(picked).filename().u8string()); } else say(err); }
        }
    } else if (task[CAT_FILES] == "Save") {
        groupBox("Save the model");
        ImGui::TextUnformatted("File Name:");
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
        // MakeHuman's metadata: the person's name, a uuid, tags (Settings > General > Tags Count of them)
        groupBox("Human Name");
        { static char nb[128]; static std::string was = "\x01"; if (was != body.name) { snprintf(nb, sizeof nb, "%s", body.name.c_str()); was = body.name; }
          ImGui::SetNextItemWidth(w); if (ImGui::InputText("##hname", nb, sizeof nb)) { body.name = nb; was = body.name; } }
        groupBox("UUID");
        ImGui::PushTextWrapPos(w); ImGui::TextDisabled("%s", body.uuid.empty() ? "(none)" : body.uuid.c_str()); ImGui::PopTextWrapPos();
        if (ImGui::Button("Generate UUID", ImVec2(w, 0))) {
            std::random_device rd; std::mt19937_64 g(((uint64_t)rd() << 32) ^ rd()); std::uniform_int_distribution<int> d(0, 15);
            const char* hex = "0123456789abcdef"; std::string u;
            for (int k = 0; k < 36; ++k) u += (k == 8 || k == 13 || k == 18 || k == 23) ? '-' : k == 14 ? '4' : k == 19 ? hex[8 + d(g) % 4] : hex[d(g)];
            body.uuid = u;
        }
        groupBox("Tags");
        {
            std::vector<std::string> t = body.tags;   // (sorted when saved, as MakeHuman does)
            t.resize((size_t)prefs.tagCount);
            bool ch = false;
            for (int k = 0; k < prefs.tagCount; ++k) {
                char tb[26]; snprintf(tb, sizeof tb, "%s", t[(size_t)k].c_str());
                ImGui::PushID(k); ImGui::SetNextItemWidth(w);
                if (ImGui::InputText("##tag", tb, sizeof tb)) { std::string v = tb; for (auto& c : v) c = (char)tolower((unsigned char)c); t[(size_t)k] = v; ch = true; }
                ImGui::PopID();
            }
            if (ch) { body.tags.clear(); for (const auto& x : t) if (!x.empty() && std::find(body.tags.begin(), body.tags.end(), x) == body.tags.end()) body.tags.push_back(x); }
        }
        if (ImGui::Button("Reset Metadata", ImVec2(w, 0))) { body.name.clear(); body.uuid.clear(); body.tags.clear(); }
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
        else if (exportFormat == EX_LIGHT) ImGui::TextDisabled("MakeHuman's lightmap: one light, from the front left and above, baked into the body's UV layout (1024 x 1024).");
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
    if (exportFormat != EX_UV && exportFormat != EX_LIGHT && exportFormat != EX_GLB) ImGui::Checkbox("Feet on ground", &exportFeet);
    if (exportFormat == EX_DAE || exportFormat == EX_FBX || exportFormat == EX_OGRE || exportFormat == EX_OBJ || exportFormat == EX_STL) ImGui::Checkbox("In the current pose", &exportPosed);
    if (exportFormat == EX_DAE || exportFormat == EX_FBX || exportFormat == EX_OBJ || exportFormat == EX_STL) ImGui::Checkbox("Helper geometry", &exportHelpers);
    if (exportFormat == EX_OBJ) ImGui::Checkbox("Normals", &exportObjNormals);
    if (exportFormat == EX_FBX) ImGui::Checkbox("Binary FBX", &exportFbxBinary);
    if (exportFormat == EX_STL) ImGui::Checkbox("Binary STL", &exportStlBinary);
    if (exportFormat == EX_DAE) {
        ImGui::Checkbox("Facial pose-units", &exportFaceUnits);
        ImGui::TextUnformatted("Orientation");
        ImGui::RadioButton("Y up, face Z", &exportOrient, 0); ImGui::RadioButton("Y up, face X", &exportOrient, 1);
        ImGui::RadioButton("Z up, face -Y", &exportOrient, 2); ImGui::RadioButton("Z up, face X", &exportOrient, 3);
        ImGui::TextUnformatted("Bone orientation");
        ImGui::RadioButton("Along local Y", &exportBoneAxis, 0); ImGui::RadioButton("Along local X", &exportBoneAxis, 1); ImGui::RadioButton("Local = Global", &exportBoneAxis, 2);
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
    const bool ok = Viewer::renderImage(w, h, prefs.noSampleBuffers ? 1 : renderSS, rendered);
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
// MakeHuman's lights are point lights at a position, looking at a focus, in the view's axes: the direction is the one
// from the focus to the light. Turning a light (yaw/pitch) moves its position round the focus at the same distance.
static void lightFromMh(Viewer::Look::Light& L) {
    float d[3] = {L.pos[0] - L.focus[0], L.pos[1] - L.focus[1], L.pos[2] - L.focus[2]};
    const float len = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    if (len < 1e-6f) return;
    for (float& c : d) c /= len;
    L.yaw = std::atan2(d[0], d[2]); L.pitch = std::asin(std::clamp(d[1], -1.0f, 1.0f));
}
static void lightToMh(Viewer::Look::Light& L) {
    float len = std::sqrt((L.pos[0] - L.focus[0]) * (L.pos[0] - L.focus[0]) + (L.pos[1] - L.focus[1]) * (L.pos[1] - L.focus[1]) + (L.pos[2] - L.focus[2]) * (L.pos[2] - L.focus[2]));
    if (len < 1e-3f) len = 31.0f;
    const float cp = std::cos(L.pitch);
    L.pos[0] = L.focus[0] + len * cp * std::sin(L.yaw); L.pos[1] = L.focus[1] + len * std::sin(L.pitch); L.pos[2] = L.focus[2] + len * cp * std::cos(L.yaw);
}
static std::string sceneFile;          // the scene file in use ("" : a new scene), as MakeHuman's scene.file
static bool sceneModified = false;
static MhScene::Scene sceneRead;       // the numbers as read (unchanged ones are written back exactly)
static void fromMhScene(const MhScene::Scene& s) {
    sceneRead = s;
    Viewer::Look& lk = Viewer::look();
    for (int c = 0; c < 3; ++c) lk.ambient[c] = (float)s.ambience[c];
    lk.skybox = s.skybox;
    lk.lights.clear();
    for (const auto& m : s.lights) {
        if (lk.lights.size() == 8) break;
        Viewer::Look::Light L; L.on = true; L.withView = true; L.strength = 1;
        for (int c = 0; c < 3; ++c) { L.col[c] = (float)m.color[c]; L.spec[c] = (float)m.specular[c]; L.pos[c] = (float)m.position[c]; L.focus[c] = (float)m.focus[c]; }
        L.fov = (float)m.fov; L.attenuation = (float)m.attenuation; L.areaLights = m.areaLights; L.areaLightSize = (float)m.areaLightSize;
        lightFromMh(L);
        lk.lights.push_back(L);
    }
}
static MhScene::Scene toMhScene() {
    const Viewer::Look& lk = Viewer::look();
    MhScene::Scene s;
    using MhScene::keep;
    for (int c = 0; c < 3; ++c) s.ambience[c] = keep(lk.ambient[c], sceneRead.ambience[c]);
    s.skybox = lk.skybox;
    s.lights.clear();
    for (auto L : lk.lights) {
        if (!L.on) continue;
        const MhScene::Light was = s.lights.size() < sceneRead.lights.size() ? sceneRead.lights[s.lights.size()] : MhScene::Light();
        MhScene::Light m;
        for (int c = 0; c < 3; ++c) {
            m.color[c] = keep(L.col[c] * L.strength, was.color[c]); m.specular[c] = keep(L.spec[c], was.specular[c]);
            m.position[c] = keep(L.pos[c], was.position[c]); m.focus[c] = keep(L.focus[c], was.focus[c]);
        }
        m.fov = keep(L.fov, was.fov); m.attenuation = keep(L.attenuation, was.attenuation); m.areaLights = L.areaLights; m.areaLightSize = keep(L.areaLightSize, was.areaLightSize);
        s.lights.push_back(m);
    }
    return s;
}
static bool isMhScene(const std::string& p) { std::string e = fs::u8path(p).extension().u8string(); for (char& c : e) c = (char)tolower((unsigned char)c); return e == ".mhscene"; }
static bool saveScene(const std::string& path) {
    if (isMhScene(path)) { std::string err; if (!MhScene::save(toMhScene(), path, err)) return false; sceneFile = path; sceneModified = false; return true; }
    const Viewer::Look& lk = Viewer::look();
    std::ofstream o(fs::u8path(path));
    o.precision(9);   // enough to come back as the same float
    if (!o) return false;
    o << "{\n  \"ambient\": [" << lk.ambient[0] << ", " << lk.ambient[1] << ", " << lk.ambient[2] << "],\n  \"shine\": " << lk.shine
      << ",\n  \"shininess\": " << lk.shininess << ",\n  \"brightness\": " << lk.light << ",\n  \"skinLit\": " << (lk.sceneSkin ? "true" : "false")
      << ",\n  \"background\": [" << lk.bg[0] << ", " << lk.bg[1] << ", " << lk.bg[2] << "],\n  \"lights\": [";
    for (size_t i = 0; i < lk.lights.size(); ++i) {
        const auto& L = lk.lights[i];
        o << (i ? ",\n" : "\n") << "    {\"on\": " << (L.on ? "true" : "false") << ", \"turn\": " << L.yaw / DEG << ", \"height\": " << L.pitch / DEG
          << ", \"colour\": [" << L.col[0] << ", " << L.col[1] << ", " << L.col[2] << "], \"strength\": " << L.strength
          << ", \"withView\": " << (L.withView ? "true" : "false")
          << ", \"specular\": [" << L.spec[0] << ", " << L.spec[1] << ", " << L.spec[2] << "], \"position\": [" << L.pos[0] << ", " << L.pos[1] << ", " << L.pos[2]
          << "], \"focus\": [" << L.focus[0] << ", " << L.focus[1] << ", " << L.focus[2] << "], \"fov\": " << L.fov << ", \"attenuation\": " << L.attenuation
          << ", \"areaLights\": " << L.areaLights << ", \"areaLightSize\": " << L.areaLightSize << "}";
    }
    o << "\n  ]";
    if (!lk.skybox.empty()) o << ",\n  \"skybox\": \"" << lk.skybox << "\"";
    o << "\n}\n";
    if (o) { sceneFile = path; sceneModified = false; }
    return (bool)o;
}
static bool loadScene(const std::string& path, std::string& err) {
    if (isMhScene(path)) { MhScene::Scene s; if (!MhScene::load(path, s, err)) return false; fromMhScene(s); sceneFile = path; sceneModified = false; return true; }
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
    lk.skybox = v.str("skybox");
    sceneFile = path; sceneModified = false;
    if (const Json::Value* ls = v.get("lights")) {
        lk.lights.clear();
        for (const auto& l : ls->a) {
            if (lk.lights.size() == 8) break;
            Viewer::Look::Light L;
            L.on = flag(l.get("on"), true); L.yaw = (float)l.num("turn") * DEG; L.pitch = (float)l.num("height") * DEG;
            three(l.get("colour"), L.col); L.strength = (float)l.num("strength", 1); L.withView = flag(l.get("withView"), false);
            three(l.get("specular"), L.spec); three(l.get("position"), L.pos); three(l.get("focus"), L.focus);
            L.fov = (float)l.num("fov", 180); L.attenuation = (float)l.num("attenuation", 0); L.areaLights = (int)l.num("areaLights", 1); L.areaLightSize = (float)l.num("areaLightSize", 4);
            if (!l.get("position")) lightToMh(L);
            lk.lights.push_back(L);
        }
    }
    return true;
}

// Utilities > Scene Editor: MakeHuman's (plugins/7_scene_editor.py): the scene file, its items (the environment and
// each light) and the chosen item's properties - every number a slider
static int sceneItem = 0;              // 0 the environment, 1.. a light, -1 "Add scene item"
static void sceneEditorLeft(float lw) {
    Viewer::Look& lk = Viewer::look();
    groupBox("Scene");
    ImGui::TextUnformatted((sceneFile.empty() ? std::string("<New scene>") : fs::u8path(sceneFile).filename().u8string() + (sceneModified ? "*" : "")).c_str());
    const float hw = lw / 2 - 4;
    auto saveAs = [&] {
        const std::string p = fileDialog(true, L"MakeHuman scene\0*.mhscene\0Pygmalion scene (keeps every setting)\0*.pygscene\0\0", L"mhscene",
                                         (fs::u8path(userDataDir()) / "scenes" / "scene.mhscene").u8string());
        if (!p.empty()) { std::error_code ec; fs::create_directories(fs::u8path(p).parent_path(), ec); say(saveScene(p) ? "Saved " + p : "Couldn't save " + p); }
    };
    if (ImGui::Button("Save", ImVec2(hw, 0))) { if (sceneFile.empty()) saveAs(); else say(saveScene(sceneFile) ? "Saved " + sceneFile : "Couldn't save " + sceneFile); }
    ImGui::SameLine();
    if (ImGui::Button("Load ...", ImVec2(hw, 0))) {
        const std::string p = fileDialog(false, L"MakeHuman scene\0*.mhscene\0Pygmalion scene\0*.pygscene\0All files\0*.*\0\0", L"mhscene", (fs::u8path(userDataDir()) / "scenes").u8string() + "\\");
        if (!p.empty()) { std::string err; say(loadScene(p, err) ? "Scene " + fs::u8path(p).stem().u8string() : err); sceneItem = 0; }
    }
    if (ImGui::Button("Save As...", ImVec2(hw, 0))) saveAs();
    ImGui::SameLine();
    if (ImGui::Button("Close", ImVec2(hw, 0))) {   // MakeHuman's reset: its default light and ambience
        MhScene::Scene s; fromMhScene(s); sceneFile.clear(); sceneModified = false; sceneItem = 0; say("A new scene");
    }
    groupBox("Items");
    ImGui::BeginChild("items", ImVec2(lw, std::min(200.0f, 30.0f + 22.0f * (float)(lk.lights.size() + 1))), true);
    if (ImGui::Selectable("Environment", sceneItem == 0)) sceneItem = 0;
    for (size_t i = 0; i < lk.lights.size(); ++i) if (ImGui::Selectable(("Light " + std::to_string(i)).c_str(), sceneItem == (int)i + 1)) sceneItem = (int)i + 1;
    ImGui::EndChild();
    if (ImGui::Button("Add...", ImVec2(lw, 0))) sceneItem = -1;
    bool changed = false;
    auto slider3 = [&](const char* id, float* v, float lo, float hi, const char* a, const char* b, const char* c) {
        ImGui::PushID(id); const char* n[] = {a, b, c}; bool ch = false;
        for (int k = 0; k < 3; ++k) { ImGui::PushID(k); ImGui::SetNextItemWidth(lw); ch |= ImGui::SliderFloat("##v", &v[k], lo, hi, n[k]); ImGui::PopID(); }
        ImGui::PopID(); return ch;
    };
    if (sceneItem == -1) {
        groupBox("Add scene item");
        if (lk.lights.size() < 8 && ImGui::Button("Add light", ImVec2(lw, 0))) {
            Viewer::Look::Light L; L.withView = true; L.strength = 1; lightFromMh(L);   // MakeHuman's new light: (-10.99, 20, 20) at the centre
            lk.lights.push_back(L); sceneItem = (int)lk.lights.size(); changed = true;
        }
        if (lk.lights.size() >= 8) ImGui::TextDisabled("Eight lights is MakeHuman's limit.");
    } else if (sceneItem == 0) {
        groupBox("Environment properties");
        ImGui::TextUnformatted("Ambience");
        changed |= slider3("amb", lk.ambient, 0, 1, "Red %.3f", "Green %.3f", "Blue %.3f");
        if (!lk.skybox.empty()) ImGui::TextDisabled("Skybox: %s", lk.skybox.c_str());
    } else if (sceneItem <= (int)lk.lights.size()) {
        auto& L = lk.lights[(size_t)sceneItem - 1];
        groupBox(("Light " + std::to_string(sceneItem - 1) + " properties").c_str());
        ImGui::TextUnformatted("Position");
        if (slider3("pos", L.pos, -60, 60, "X %.2f", "Y %.2f", "Z %.2f")) { lightFromMh(L); L.withView = true; changed = true; }
        ImGui::TextUnformatted("Focus");
        if (slider3("foc", L.focus, -30, 30, "X %.2f", "Y %.2f", "Z %.2f")) { lightFromMh(L); L.withView = true; changed = true; }
        ImGui::TextUnformatted("Color");
        changed |= slider3("col", L.col, 0, 1.5f, "Red %.3f", "Green %.3f", "Blue %.3f");
        ImGui::TextUnformatted("Specular");
        changed |= slider3("spe", L.spec, 0, 1.5f, "Red %.3f", "Green %.3f", "Blue %.3f");
        ImGui::SetNextItemWidth(lw); changed |= ImGui::SliderFloat("##fov", &L.fov, 1, 180, "Spot angle %.1f");
        ImGui::SetNextItemWidth(lw); changed |= ImGui::SliderFloat("##att", &L.attenuation, 0, 1, "Attenuation %.3f");
        bool soft = L.areaLights > 1;
        if (ImGui::Checkbox("Soft light", &soft)) { L.areaLights = soft ? std::max(L.areaLights, 2) : 1; changed = true; }
        ImGui::SetNextItemWidth(lw); changed |= ImGui::SliderFloat("##size", &L.areaLightSize, 0, 20, "Softness %.2f");
        ImGui::SetNextItemWidth(lw); changed |= ImGui::SliderInt("##samp", &L.areaLights, 1, 16, "Samples %d");
        ImGui::TextDisabled("Spot angle, attenuation and softness are kept in the scene file for renderers; the view lights by position and colour, as MakeHuman's does.");
        if (ImGui::Button(("Remove light " + std::to_string(sceneItem - 1)).c_str(), ImVec2(lw, 0))) { lk.lights.erase(lk.lights.begin() + (sceneItem - 1)); sceneItem = 0; changed = true; }
    } else sceneItem = 0;
    if (changed) sceneModified = true;
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
        if (pre >= 0) { scenePreset(pre); sceneFile.clear(); for (auto& L : lk.lights) lightToMh(L); say(std::string(SCENE_PRESETS[pre]) + " scene"); }
        // MakeHuman's scenes: the .mhscene files in its scenes folders (and Pygmalion's own .pygscene)
        groupBox("Scene files");
        {
            std::vector<std::string> files = filesIn("scenes", ".mhscene");
            for (const auto& f : filesIn("scenes", ".pygscene")) files.push_back(f);
            ImGui::BeginChild("scenes", ImVec2(lw, std::min(160.0f, 26.0f + 22.0f * (float)files.size())), true);
            for (const auto& f : files) {
                const bool on = fs::u8path(f) == fs::u8path(sceneFile);
                if (ImGui::Selectable(fs::u8path(f).stem().u8string().c_str(), on)) { std::string err; say(loadScene(f, err) ? "Scene " + fs::u8path(f).stem().u8string() : err); }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", f.c_str());
            }
            ImGui::EndChild();
        }
        if (ImGui::Button("Load...", ImVec2(lw / 2 - 4, 0))) {
            const std::string p = fileDialog(false, L"Scene (*.mhscene, *.pygscene)\0*.mhscene;*.pygscene\0\0", L"mhscene", homeDir() + "\\");
            if (!p.empty()) { std::string err; say(loadScene(p, err) ? "Scene " + fs::u8path(p).stem().u8string() : err); }
        }
        ImGui::SameLine();
        if (ImGui::Button("Save...", ImVec2(lw / 2 - 4, 0))) {
            const std::string p = fileDialog(true, L"MakeHuman scene\0*.mhscene\0Pygmalion scene (keeps every setting)\0*.pygscene\0\0", L"mhscene", homeDir() + "\\scene.mhscene");
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
            ImGui::SetNextItemWidth(lw); if (ImGui::SliderFloat("##y", &deg[0], -180, 180, L.withView ? "From the left/right %.0f deg" : "Round the person %.0f deg")) { L.yaw = deg[0] * DEG; lightToMh(L); sceneModified = true; }
            ImGui::SetNextItemWidth(lw); if (ImGui::SliderFloat("##p", &deg[1], -89, 89, "Height %.0f deg")) { L.pitch = deg[1] * DEG; lightToMh(L); sceneModified = true; }
            ImGui::SetNextItemWidth(lw); ImGui::SliderFloat("##s", &L.strength, 0, 3, "Strength %.2f");
            rgbSliders("lc", L.col, 1.5f, lw);
            if (ImGui::TreeNode("Highlight colour")) { rgbSliders("ls", L.spec, 1.5f, lw - 20); ImGui::TreePop(); }
            if (lk.lights.size() > 1 && ImGui::SmallButton("Remove this light")) drop = (int)i;
            ImGui::PopID();
        }
        if (drop >= 0) lk.lights.erase(lk.lights.begin() + drop);
        if (lk.lights.size() < 8 && ImGui::Button("Add a light", ImVec2(lw, 0))) { lk.lights.push_back(Viewer::Look::Light{true, 45 * DEG, 30 * DEG, {1, 1, 1}, 0.5f, false}); lightToMh(lk.lights.back()); }
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
// Utilities > Material Editor: MakeHuman's (plugins/7_material_editor.py) - the object, its shader, the shader's
// config, custom defines and parameters, every material setting (sliders), a description, and the .mhmat file
static void materialEditor(float lw) {
    groupBox("Object");
    std::vector<std::string> names = {"Skin"};
    for (const auto& p : body.proxies) names.push_back(niceName(p.file) + " (" + p.kind + ")");
    matPick = std::clamp(matPick, 0, (int)names.size() - 1);
    for (int k = 0; k < (int)names.size(); ++k) if (ImGui::RadioButton(names[k].c_str(), matPick == k)) matPick = k;
    Human::Proxy* px = matPick == 0 ? nullptr : &body.proxies[matPick - 1];
    Mat::Material& m = px ? px->mat : body.skinMat;
    bool changed = false;
    // ---- Shader: [None] and MakeHuman's six
    groupBox("Shader");
    if (ImGui::RadioButton("[None]", m.shader.empty())) { m.shader.clear(); changed = true; }
    for (int k = 0; k < Mat::SHADER_COUNT; ++k) if (ImGui::RadioButton(Mat::SHADERS[k], m.shader == Mat::SHADERS[k])) { m.shader = Mat::SHADERS[k]; changed = true; }
    // ---- Shader config: on only where the material has the texture and the shader takes it
    groupBox("Shader config");
    {
        const auto& defs = m.shader.empty() ? std::vector<std::string>() : Mat::defineables(m.shader);
        auto takes = [&](const char* d, const char* u) {
            if (m.shader.empty()) return true;
            if (std::find(defs.begin(), defs.end(), d) != defs.end()) return true;
            for (const auto& x : Mat::uniforms(m.shader)) if (x.name == u) return true;
            return false;
        };
        struct C { const char* name; bool* v; bool on; };
        const C cs[] = {{"diffuse", &m.cfgDiffuse, m.supportsDiffuse() && takes("DIFFUSE", "diffuseTexture")},
                        {"bump", &m.cfgBump, m.supportsBump() && takes("BUMPMAP", "bumpmapTexture")},
                        {"normal", &m.cfgNormal, !m.shader.empty() && m.supportsNormal() && takes("NORMALMAP", "normalmapTexture")},
                        {"displacement", &m.cfgDisplacement, !m.shader.empty() && m.supportsDisplacement() && takes("DISPLACEMENT", "displacementmapTexture")},
                        {"spec", &m.cfgSpec, !m.shader.empty() && m.supportsSpecular() && takes("SPECULARMAP", "specularmapTexture")},
                        {"vertexColors", &m.cfgVertexColors, m.shader.empty() || std::find(defs.begin(), defs.end(), "VERTEX_COLOR") != defs.end()},
                        {"transparency", &m.cfgTransparency, !m.shader.empty() && m.supportsTransparency() && takes("TRANSPARENCYMAP", "transparencymapTexture")},
                        {"ambientOcclusion", &m.cfgAo, !m.shader.empty() && m.supportsAo() && takes("AOMAP", "aomapTexture")}};
        for (const auto& c : cs) { ImGui::BeginDisabled(!c.on); changed |= ImGui::Checkbox(c.name, c.v); ImGui::EndDisabled(); }
    }
    // ---- Custom shader defines: the shader's own, besides the config's
    if (!m.shader.empty()) {
        std::vector<std::string> custom;
        for (const auto& d : Mat::defineables(m.shader)) { bool cfg = false; for (int k = 0; k < 8; ++k) if (d == Mat::CONFIG_DEFINES[k]) cfg = true; if (!cfg) custom.push_back(d); }
        if (!custom.empty()) {
            groupBox("Custom shader defines");
            for (const auto& d : custom) {
                bool on = std::find(m.shaderDefines.begin(), m.shaderDefines.end(), d) != m.shaderDefines.end();
                if (ImGui::Checkbox(d.c_str(), &on)) {
                    if (on) m.shaderDefines.push_back(d); else m.shaderDefines.erase(std::remove(m.shaderDefines.begin(), m.shaderDefines.end(), d), m.shaderDefines.end());
                    std::sort(m.shaderDefines.begin(), m.shaderDefines.end()); changed = true;
                }
            }
        }
    }
    // ---- Shader parameters: the shader's uniforms the material doesn't set itself
    if (!m.shader.empty()) {
        groupBox("Shader parameters");
        int shown = 0;
        for (const auto& u : Mat::uniforms(m.shader)) {
            if (Mat::isMaterialParam(u.name)) continue;
            ImGui::PushID(u.name.c_str()); ++shown;
            std::string v = m.param(u.name, u.def);
            if (u.type == "sampler2D") {
                ImGui::TextUnformatted(u.name.c_str());
                ImGui::PushTextWrapPos(lw); ImGui::TextDisabled("%s", v.empty() ? "(none)" : fs::u8path(v).filename().u8string().c_str()); ImGui::PopTextWrapPos();
                if (ImGui::Button("Browse...", ImVec2(lw / 2 - 4, 0))) {
                    const std::string f = fileDialog(false, L"Pictures\0*.png;*.jpg;*.jpeg;*.bmp;*.tga\0\0", L"png", (fs::u8path(dataDir) / "litspheres").u8string() + "\\");
                    if (!f.empty()) { m.setParam(u.name, f); changed = true; }
                }
                ImGui::SameLine(); if (ImGui::Button("Clear", ImVec2(lw / 2 - 4, 0))) { m.shaderParams.erase(std::remove_if(m.shaderParams.begin(), m.shaderParams.end(), [&](const auto& q) { return q.first == u.name; }), m.shaderParams.end()); changed = true; }
            } else if (u.type == "float" || u.type == "int") {
                float f = (float)atof(v.c_str());
                const float hi = std::max(1.0f, std::fabs((float)atof(u.def.c_str())) * 4);
                ImGui::SetNextItemWidth(lw);
                if (ImGui::SliderFloat("##v", &f, 0, hi, (u.name + " %.3f").c_str())) { char b2[32]; snprintf(b2, sizeof b2, "%g", f); m.setParam(u.name, b2); changed = true; }
            } else if (u.type == "vec3" || u.type == "vec4" || u.type == "vec2") {
                const int n = u.type == "vec2" ? 2 : u.type == "vec3" ? 3 : 4;
                float x[4] = {0, 0, 0, 0}; { std::istringstream vs(v); for (int k = 0; k < n; ++k) vs >> x[k]; }
                ImGui::TextUnformatted(u.name.c_str());
                bool ch = false;
                for (int k = 0; k < n; ++k) { ImGui::PushID(k); ImGui::SetNextItemWidth(lw); ch |= ImGui::SliderFloat("##c", &x[k], 0, 1, "%.3f"); ImGui::PopID(); }
                if (ch) { std::string t; for (int k = 0; k < n; ++k) { char b2[32]; snprintf(b2, sizeof b2, "%g", x[k]); t += (k ? " " : "") + std::string(b2); } m.setParam(u.name, t); changed = true; }
            } else ImGui::TextDisabled("%s (%s)", u.name.c_str(), u.type.c_str());
            ImGui::PopID();
        }
        if (!shown) ImGui::TextDisabled("This shader has none of its own.");
    }
    // ---- Material settings
    groupBox("Material settings");
    auto colour = [&](const char* label, float* c, bool enabled) {
        ImGui::BeginDisabled(!enabled); ImGui::TextUnformatted(label);
        ImGui::PushID(label);
        static const char* n[] = {"Red %.3f", "Green %.3f", "Blue %.3f"};
        for (int k = 0; k < 3; ++k) { ImGui::PushID(k); ImGui::SetNextItemWidth(lw); changed |= ImGui::SliderFloat("##c", &c[k], 0, 1, n[k]); ImGui::PopID(); }
        ImGui::PopID(); ImGui::EndDisabled();
    };
    auto scalar = [&](const char* label, float* v, bool enabled = true) { ImGui::BeginDisabled(!enabled); ImGui::SetNextItemWidth(lw); changed |= ImGui::SliderFloat(label, v, 0, 1, (std::string(label).substr(2) + " %.3f").c_str()); ImGui::EndDisabled(); };
    auto truth = [&](const char* label, bool* v) { changed |= ImGui::Checkbox(label, v); };
    auto image = [&](const char* label, std::string& t, bool enabled) {
        ImGui::BeginDisabled(!enabled);
        ImGui::TextUnformatted(label);
        ImGui::PushID(label);
        ImGui::PushTextWrapPos(lw); ImGui::TextDisabled("%s", t.empty() ? "(none)" : fs::u8path(t).filename().u8string().c_str()); ImGui::PopTextWrapPos();
        if (ImGui::Button("Browse...", ImVec2(lw / 2 - 4, 0))) {
            const std::string f = fileDialog(false, L"Pictures\0*.png;*.jpg;*.jpeg;*.bmp;*.tga\0\0", L"png", (t.empty() ? homeDir() : fs::u8path(t).parent_path().u8string()) + "\\");
            if (!f.empty()) { t = f; changed = true; }
        }
        ImGui::SameLine(); if (ImGui::Button("Clear", ImVec2(lw / 2 - 4, 0))) { t.clear(); changed = true; }
        ImGui::PopID(); ImGui::EndDisabled();
    };
    const bool sh = !m.shader.empty();
    auto uni = [&](const char* u) { if (!sh) return true; for (const auto& x : Mat::uniforms(m.shader)) if (x.name == u) return true; return false; };
    auto def = [&](const char* d) { if (!sh) return false; const auto& ds = Mat::defineables(m.shader); return std::find(ds.begin(), ds.end(), d) != ds.end(); };
    // which settings the chosen shader reads (MakeHuman greys out the rest; the fixed-function [None] reads the colours)
    const bool fixed = !sh || m.shader == "skin" || m.shader == "normalmap";
    colour("Diffuse", m.diffuse, (uni("diffuse") || fixed) && !m.autoBlendSkin);
    image("Diffuse texture", m.diffuseTexture, !sh || uni("diffuseTexture") || def("DIFFUSE"));
    colour("Ambient", m.ambient, uni("ambient") || fixed);
    colour("Specular", m.specular, uni("specular") || fixed);
    scalar("##Specular shininess", &m.shininess, uni("specular") || fixed);
    colour("Emissive", m.emissive, uni("emissive") || fixed);
    scalar("##Opacity", &m.opacity, uni("diffuse") || fixed);
    scalar("##Translucency", &m.translucency);
    truth("Shadeless", &m.shadeless); truth("Wireframe", &m.wireframe); truth("Transparent", &m.transparent);
    truth("Alpha to Coverage", &m.alphaToCoverage); truth("Backface culling", &m.backfaceCull); truth("Depthless", &m.depthless);
    truth("Auto ethnic skin", &m.autoBlendSkin);
    image("Transparency map texture", m.transparencyMapTexture, sh && (uni("transparencymapTexture") || def("TRANSPARENCYMAP")));
    scalar("##Transparency (map) intensity", &m.transparencyMapIntensity, sh && (uni("transparencymapTexture") || def("TRANSPARENCYMAP")));
    image("Bump map texture", m.bumpMapTexture, sh && (uni("bumpmapTexture") || def("BUMPMAP")));
    scalar("##Bump map intensity", &m.bumpMapIntensity, sh && (uni("bumpmapTexture") || def("BUMPMAP")));
    image("Normal map texture", m.normalMapTexture, sh && (uni("normalmapTexture") || def("NORMALMAP")));
    scalar("##Normal map intensity", &m.normalMapIntensity, sh && (uni("normalmapTexture") || def("NORMALMAP")));
    image("Displacement map texture", m.displacementMapTexture, sh && (uni("displacementmapTexture") || def("DISPLACEMENT")));
    scalar("##Displacement map intensity", &m.displacementMapIntensity, sh && (uni("displacementmapTexture") || def("DISPLACEMENT")));
    image("Specular map texture", m.specularMapTexture, sh && (uni("specularmapTexture") || def("SPECULARMAP")));
    scalar("##Specular map intensity", &m.specularMapIntensity, sh && (uni("specularmapTexture") || def("SPECULARMAP")));
    image("Ambient Occlusion texture", m.aoMapTexture, sh && (uni("aomapTexture") || def("AOMAP")));
    scalar("##AO map intensity", &m.aoMapIntensity, sh && (uni("aomapTexture") || def("AOMAP")));
    {   // the uv map: one of MakeHuman's uv sets (data/uvs), or the mesh's own
        ImGui::TextUnformatted("UV map");
        std::vector<std::string> uvs = filesIn("uvs", ".mhuv"); for (const auto& f : filesIn("uvs", ".obj")) uvs.push_back(f);
        const std::string cur = m.uvMap.empty() ? "default" : fs::u8path(m.uvMap).stem().u8string();
        ImGui::SetNextItemWidth(lw);
        if (ImGui::BeginCombo("##uv", cur.c_str())) {
            if (ImGui::Selectable("default", m.uvMap.empty())) { m.uvMap.clear(); changed = true; }
            for (const auto& f : uvs) { const std::string n = fs::u8path(f).stem().u8string(); if (n == "default") continue; if (ImGui::Selectable(n.c_str(), m.uvMap == f)) { m.uvMap = f; changed = true; } }
            ImGui::EndCombo();
        }
    }
    {
        static char nameBuf[128]; static const Mat::Material* nameOf = nullptr;
        if (nameOf != &m) { snprintf(nameBuf, sizeof nameBuf, "%s", m.name.c_str()); nameOf = &m; }
        ImGui::TextUnformatted("Material name");
        ImGui::SetNextItemWidth(lw); if (ImGui::InputText("##mname", nameBuf, sizeof nameBuf)) m.name = nameBuf;
    }
    // ---- Description: the object, its size and its licence
    groupBox("Description");
    {
        size_t nv, nf; int vpf; std::string author = "unknown", license = "unknown", file = "internal", nm;
        if (px) {
            nv = px->pos.size() / 3; nf = px->tri.size() / 3; if (px->vertsPerFace == 4) nf = (nf + 1) / 2; vpf = px->vertsPerFace;
            author = px->author; license = px->license; file = px->file; nm = px->name;
        } else { nv = body.base.size() / 3; nf = body.faces4.size() / 4; vpf = 4; nm = "base.obj"; }
        ImGui::PushTextWrapPos(lw);
        ImGui::TextWrapped("%s:\n%zu vertices\n%zu faces\n%d vertices per face.\nAuthor: %s\nLicense: %s\n\nFile: %s", nm.c_str(), nv, nf, vpf, author.c_str(), license.c_str(), file.c_str());
        ImGui::PopTextWrapPos();
    }
    // ---- Material file
    groupBox("Material file");
    if (ImGui::Button("Load", ImVec2(lw / 2 - 4, 0))) {
        const std::string dir = m.filename.empty() ? homeDir() : fs::u8path(m.filename).parent_path().u8string();
        const std::string f = fileDialog(false, L"MakeHuman Material (*.mhmat)\0*.mhmat\0\0", L"mhmat", dir + "\\");
        if (!f.empty()) { remember("Material"); if (px) Human::setProxyMaterial(body, px->kind, f); else Human::setSkin(body, f); dirty = true; }
    }
    ImGui::SameLine();
    if (ImGui::Button("Save", ImVec2(lw / 2 - 4, 0))) {
        const std::string dir = m.filename.empty() ? (fs::u8path(userDataDir()) / "skins").u8string() : fs::u8path(m.filename).parent_path().u8string();
        std::string f = fileDialog(true, L"MakeHuman Material (*.mhmat)\0*.mhmat\0\0", L"mhmat", dir + "\\" + (m.name.empty() ? std::string("material") : m.name) + ".mhmat");
        if (!f.empty()) {
            if (fs::u8path(f).extension().empty()) f += ".mhmat";
            std::string err; say(Mat::toFile(m, f, err) ? "Saved " + f : err);
        }
    }
    if (changed) {
        // the old fields the rest of Pygmalion reads, kept in step
        if (px) { px->texture = m.diffuseTexture; px->transparent = m.transparent; } else body.skinTexture = m.diffuseTexture;
        if (!px) { std::string err; if (!Human::applyUvMap(body, err)) { say(err); m.uvMap.clear(); Human::applyUvMap(body, err); } }
        dirty = true;
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

static void customFolder(std::string& dir) { dir = homeDir().empty() ? std::string() : (fs::u8path(homeDir()) / "custom").u8string(); std::error_code ec; if (!dir.empty()) fs::create_directories(fs::u8path(dir), ec); }
// Modelling > Custom (MakeHuman's 0_modeling_9_custom_targets): every .target under the custom folder, by folder
static std::map<int, std::string> customFolderOf;   // a custom modifier -> its folder (relative, "" the top)
static std::string customShown;                     // the folder whose sliders show
static bool keepCustomValues = true;
static void loadCustomTargets() {
    std::string dir; customFolder(dir);
    std::error_code ec;
    for (const auto& e : fs::recursive_directory_iterator(fs::u8path(dir), ec)) {
        if (e.path().extension() != ".target") continue;
        const fs::path rel = fs::relative(e.path(), fs::u8path(dir), ec);
        std::string name = rel.parent_path().empty() ? rel.stem().u8string() : (rel.parent_path() / rel.stem()).generic_u8string();
        const int m = Human::addCustomTarget(body, e.path().u8string(), name);
        if (m >= 0) { customFolderOf[m] = rel.parent_path().generic_u8string(); std::string l = rel.stem().u8string(); std::replace(l.begin(), l.end(), '-', ' '); if (!l.empty()) l[0] = (char)toupper((unsigned char)l[0]); body.mods[m].label = l; }
    }
}
// Utilities > Save targets (MakeHuman's 7_save_targets): the model as one target (base targets stripped or not), and a
// diff target from a base model to this one (Set Base first); Alt+T saves quickly
static char targetName[128] = "full_target.target", diffName[128] = "diff_target.target";
static bool stripBase = true;
static std::vector<float> diffBase;   // "Set Base": the base model's shape (MakeHuman keeps it in a cache file)
static std::string targetsDir() { std::string d; customFolder(d); return d; }
static void saveFullTarget(const std::string& p) {
    std::string err;
    if (Human::saveTarget(body, p, err, stripBase)) { say("Saving target to " + p); const int m = Human::addCustomTarget(body, p); if (m >= 0) customFolderOf[m] = ""; }
    else say(err);
}
static void saveTargetsLeft(float lw) {
    groupBox("Save Model as Target");
    ImGui::TextUnformatted("Filename:");
    ImGui::SetNextItemWidth(lw); ImGui::InputText("##tn", targetName, sizeof targetName);
    ImGui::Checkbox("Strip Base Targets", &stripBase);
    if (ImGui::Button("Save", ImVec2(lw, 0))) {
        std::string n = targetName; if (fs::u8path(n).extension() != ".target") n += ".target";
        saveFullTarget((fs::u8path(targetsDir()) / n).u8string());
    }
    if (ImGui::Button("Save As ...", ImVec2(lw, 0))) {
        const std::string p = fileDialog(true, L"MakeHuman Target (*.target)\0*.target\0All files (*.*)\0*.*\0\0", L"target", (fs::u8path(targetsDir()) / targetName).u8string());
        if (!p.empty()) {
            if (fs::u8path(p).extension() != ".target") say("Cannot save target to file: " + p + " - expected a path to a .target file");
            else { saveFullTarget(p); snprintf(targetName, sizeof targetName, "%s", fs::u8path(p).filename().u8string().c_str()); }
        }
    }
    groupBox("Save Diff Target");
    ImGui::SetNextItemWidth(lw); ImGui::InputText("##dn", diffName, sizeof diffName);
    if (ImGui::Button("Set Base", ImVec2(lw, 0))) { diffBase = body.pos; say("The base model is set: change or load the second model, then Save"); }
    auto saveDiff = [&](const std::string& p) {
        if (diffBase.empty()) { say("No Base Model defined. Press \"Set Base\""); return; }
        std::string err; if (Human::saveTarget(body, p, err, false, &diffBase)) say("Saving target to " + p); else say(err);
    };
    if (ImGui::Button("Save##diff", ImVec2(lw, 0))) { std::string n = diffName; if (fs::u8path(n).extension() != ".target") n += ".target"; saveDiff((fs::u8path(targetsDir()) / n).u8string()); }
    if (ImGui::Button("Save As ...##diff", ImVec2(lw, 0))) {
        const std::string p = fileDialog(true, L"MakeHuman Target (*.target)\0*.target\0All files (*.*)\0*.*\0\0", L"target", (fs::u8path(targetsDir()) / diffName).u8string());
        if (!p.empty()) saveDiff(p);
    }
    if (ImGui::Button("Clear Cache", ImVec2(lw, 0))) { diffBase.clear(); say("The base model is forgotten"); }
    groupBox("Info");
    notYet("Save target saves the current state of all targets applied to the default start up mesh as one global target. The strip option will "
           "remove all targets that constitute the default mesh from the global target. This is mandatory when reusing the global target inside "
           "MakeHuman (or Pygmalion: it appears in Modelling > Custom). For usage on the base mesh in MakeTarget uncheck the strip option.\n\n"
           "Save diff target saves the data difference of two models as a global target. First create or load a model and press \"Set Base\". "
           "Then create or load a second model. Pressing \"Save\" will create a global target, which can transform the first model into the "
           "second one. The resulting global diff target is absolutely specific to the first model and will not work on any other model.\n\n"
           "The default license of the saved global targets is AGPL3.");
}

static void customLeft(float lw) {
    groupBox("Custom targets");
    std::string dir; customFolder(dir);
    if (ImGui::Button("Rescan targets' folder", ImVec2(lw / 2 - 4, 0))) {
        // Keep Values: the sliders keep what they were set to, else every custom one goes back to 0
        if (!keepCustomValues) for (auto& [m, f] : customFolderOf) Human::setModifier(body, m, 0);
        loadCustomTargets(); dirty = true;
    }
    ImGui::SameLine();
#ifdef _WIN32
    if (ImGui::Button("Open the folder", ImVec2(lw / 2 - 4, 0))) ShellExecuteW(nullptr, L"open", fs::u8path(dir).wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#endif
    ImGui::Checkbox("Keep Values", &keepCustomValues);
    // the folders that have targets, as MakeHuman's folder buttons
    std::map<std::string, int> folders;
    for (const auto& [m, f] : customFolderOf) ++folders[f];
    if (folders.size() > 1) {
        groupBox("Folders");
        if (!folders.count(customShown)) customShown = folders.begin()->first;
        for (const auto& [f, c] : folders) if (ImGui::RadioButton((f.empty() ? std::string("Targets") : f).c_str(), customShown == f)) customShown = f;
    } else if (!folders.empty()) customShown = folders.begin()->first;
    groupBox("Targets");
    int n = 0;
    for (const auto& [m, f] : customFolderOf) if (f == customShown && m < (int)body.mods.size()) { modSlider(m, lw, "frontView"); ++n; }
    if (!n) notYet(("Put your own .target files (from Blender's MakeTarget, or Utilities > Save targets) in " + dir + " and they appear here as sliders.").c_str());
}

// Mass produce: many random people, saved (and exported) one after another
// ---------- Utilities > Mass produce: MakeHuman's (plugins/9_massproduce) ----------
// Each person: the macros at random within their ranges (gender and ethnicity whole or mixed), the chosen modifier
// groups varied round their values now (a normal spread, at most "max deviation"), then a skin, hair, eyebrows,
// eyelashes and clothes from the allowed tables for that gender (and ethnicity, for skins); saved as .mhm in the models
// folder and exported, named <base><4 digits>. When it's done the person goes back to how they were.
struct MassSettings {
    bool randAge = true, randWeight = true, randHeight = true, randMuscle = true, gender = true, genderAbs = true, ethnicity = true, ethnicityAbs = true;
    float ageMin = 0.45f, ageMax = 0.95f, weightMin = 0.1f, weightMax = 0.9f, heightMin = 0.2f, heightMax = 0.9f, muscleMin = 0.3f, muscleMax = 0.8f;
    std::map<std::string, bool> groups;   // modifier group -> randomize
    float maxdev = 0.3f;
    bool hair = true, eyelashes = true, eyebrows = true, fullClothes = true, upperClothes = false, lowerClothes = false, shoes = true, skins = true;
    // allowed: name -> {mixed, female, male} (skins: female/male tables of {mixed, african, asian, caucasian})
    std::map<std::string, std::array<bool, 3>> hairT, browsT, lashesT, fullT, upperT, lowerT, shoesT;
    std::map<std::string, std::array<bool, 4>> femaleSkins, maleSkins;
    std::map<std::string, std::string> path;   // name -> file
    char base[260] = "mass"; int format = -1; int count = 5;
    bool built = false;
};
static MassSettings mass;
static int massLeft = 0, massDone = 0, massTotal = 0;
static State massStart;
static std::string massName(const std::string& f) {   // MakeHuman's: the file name, lower case, _ as spaces
    std::string n = fs::u8path(f).stem().u8string(); for (auto& c : n) c = c == '_' ? ' ' : (char)tolower((unsigned char)c); return n;
}
// MakeHuman's modifier groups for detail: every group but the macros, the genitals and armslegs (that one split into
// arms, hands, legs and feet by name), right-hand ones left out (they follow the left)
static std::map<std::string, std::vector<int>> massGroups() {
    std::map<std::string, std::vector<int>> g;
    for (int i = 0; i < (int)body.mods.size(); ++i) {
        const auto& m = body.mods[i];
        if (m.macro || m.group.find("macro") != std::string::npos || m.group.find("genitals") != std::string::npos || m.name.rfind("r-", 0) == 0) continue;
        if (m.group == "armslegs") {
            if (m.name.find("hand") != std::string::npos) g["hands"].push_back(i);
            if (m.name.find("foot") != std::string::npos || m.name.find("feet") != std::string::npos) g["feet"].push_back(i);
            if (m.name.find("arm") != std::string::npos) g["arms"].push_back(i);
            if (m.name.find("leg") != std::string::npos) g["legs"].push_back(i);
        } else g[m.group].push_back(i);
    }
    for (const char* k : {"arms", "hands", "legs", "feet"}) g[k];
    return g;
}
static void massBuild() {
    if (mass.built) return;
    mass.built = true;
    for (const auto& [n, v] : massGroups()) { const std::string k = n; mass.groups[k] = !(k == "arms" || k == "hands" || k == "legs" || k == "feet"); }
    auto has = [](const std::string& n, const char* w) { return n.find(w) != std::string::npos; };
    for (const auto& f : filesIn("hair", ".mhclo")) {
        const std::string n = massName(f); mass.path["hair:" + n] = f;
        const bool femaleOnly = n == "bob01" || n == "bob02" || n == "long01" || n == "braid01" || n == "ponytail01", maleOnly = n == "short02" || n == "short04";
        mass.hairT[n] = {true, !maleOnly, !femaleOnly};
    }
    for (const auto& f : filesIn("eyebrows", ".mhclo")) { const std::string n = massName(f); mass.path["brows:" + n] = f; mass.browsT[n] = {true, true, true}; }
    for (const auto& f : filesIn("eyelashes", ".mhclo")) { const std::string n = massName(f); mass.path["lashes:" + n] = f; mass.lashesT[n] = {true, true, true}; }
    static const std::set<std::string> femaleFullExplicit = {"female elegantsuit01", "female casualsuit02", "female casualsuit01", "female sportsuit01"};
    static const std::set<std::string> maleFullExplicit = {"male worksuit01", "male elegantsuit01", "male casualsuit06", "male casualsuit05", "male casualsuit04", "male casualsuit02", "male casualsuit01", "male casualsuit03"};
    for (const auto& f : filesIn("clothes", ".mhclo")) {
        const std::string n = massName(f); mass.path["clothes:" + n] = f;
        auto any = [&](std::initializer_list<const char*> ws) { for (const char* w : ws) if (has(n, w)) return true; return false; };
        const bool notFemale = !has(n, "female");
        const bool mFull = notFemale && (maleFullExplicit.count(n) || any({"wetsuit"})), fFull = femaleFullExplicit.count(n) || any({"gown", "dress", "swimsuit"});
        const bool mUp = notFemale && any({"top", "shirt", "sweater"}), fUp = any({"bra", "top", "shirt", "sweater"});
        const bool mLow = notFemale && any({"jeans", "shorts", "pants", "trunk"}), fLow = any({"skirt", "jeans", "string", "shorts", "pants"});
        const bool mSh = notFemale && any({"boot", "shoe", "sneaker", "sock"}), fSh = any({"boot", "shoe", "sneaker", "sock"});
        mass.fullT[n] = {fFull || mFull, fFull, mFull}; mass.upperT[n] = {fUp || mUp, fUp, mUp};
        mass.lowerT[n] = {fLow || mLow, fLow, mLow}; mass.shoesT[n] = {fSh || mSh, fSh, mSh};
    }
    for (const auto& f : filesIn("skins", ".mhmat")) {
        const std::string n = massName(f); mass.path["skin:" + n] = f;
        std::array<bool, 4> fe{}, ma{};
        if (has(n, "female") && !has(n, "special")) fe = {true, has(n, "african"), has(n, "asian") && !has(n, "caucasian"), has(n, "caucasian")};
        if (has(n, "male") && !has(n, "female") && !has(n, "special")) ma = {true, has(n, "african"), has(n, "asian") && !has(n, "caucasian"), has(n, "caucasian")};
        mass.femaleSkins[n] = fe; mass.maleSkins[n] = ma;
    }
}
static void massOne(std::mt19937& rng) {
    auto uni = [&](float a, float b) { if (a > b) std::swap(a, b); return a + std::uniform_real_distribution<float>(0, 1)(rng) * (b - a); };
    auto gauss = [&](float lo, float hi, float mid) {   // getNormalRandomValue: reflected back inside the range
        const float sigma = 0.2f * std::fabs(hi - lo);
        float v = sigma > 0 ? std::normal_distribution<float>(mid, sigma)(rng) : mid;
        if (v < lo) v = lo + std::fabs(v - lo); else if (v > hi) v = hi - std::fabs(v - hi);
        return std::clamp(v, lo, hi);
    };
    // the macros
    if (mass.randAge) Human::setMacro(body, "Age", uni(mass.ageMin, mass.ageMax));
    if (mass.randWeight) Human::setMacro(body, "Weight", uni(mass.weightMin, mass.weightMax));
    if (mass.randHeight) Human::setMacro(body, "Height", uni(mass.heightMin, mass.heightMax));
    if (mass.randMuscle) Human::setMacro(body, "Muscle", uni(mass.muscleMin, mass.muscleMax));
    if (mass.ethnicity) {
        if (mass.ethnicityAbs) { const int k = (int)(rng() % 3); Human::setMacro(body, "African", k == 0 ? 1.0f : 0.0f); Human::setMacro(body, "Asian", k == 1 ? 1.0f : 0.0f); Human::setMacro(body, "Caucasian", k == 2 ? 1.0f : 0.0f); }
        else { Human::setMacro(body, "African", uni(0, 1)); Human::setMacro(body, "Asian", uni(0, 1)); Human::setMacro(body, "Caucasian", uni(0, 1)); }
    }
    if (mass.gender) Human::setMacro(body, "Gender", mass.genderAbs ? (float)(rng() % 2) : uni(0, 1));
    const std::string gender = body.gender < 0.3f ? "female" : body.gender > 0.7f ? "male" : "mixed";
    const int gcol = gender == "mixed" ? 0 : gender == "female" ? 1 : 2;
    std::string ethn = "mixed";
    if (body.african > 0.9f) ethn = "african"; else if (body.asian > 0.9f) ethn = "asian"; else if (body.caucasian > 0.9f) ethn = "caucasian";
    // the details: each chosen group's modifiers round their values at the start
    const auto groups = massGroups();
    for (const auto& [g, mods] : groups) {
        auto it = mass.groups.find(g); if (it == mass.groups.end() || !it->second) continue;
        if (g == "breast" && gender != "female") continue;
        for (int i : mods) {
            const auto& m = body.mods[i];
            const float def = i < (int)massStart.mods.size() ? massStart.mods[i] : m.defaultValue;
            float v = gauss(def - mass.maxdev, def + mass.maxdev, def);
            v = std::clamp(v, m.min() < -0.05f ? -1.0f : 0.0f, 1.0f);
            Human::setModifier(body, i, v);   // (with symmetry on, the right side follows the left, as MakeHuman sets both)
        }
    }
    // the skin, for the gender and ethnicity
    std::string err;
    if (mass.skins) {
        auto& table = gender == "female" ? mass.femaleSkins : mass.maleSkins;
        const int col = ethn == "mixed" ? 0 : ethn == "african" ? 1 : ethn == "asian" ? 2 : 3;
        std::vector<std::string> ok; for (const auto& [n, a] : table) if (a[col]) ok.push_back(n);
        if (!ok.empty()) Human::setSkin(body, mass.path["skin:" + ok[rng() % ok.size()]]);
    }
    auto pick = [&](const std::map<std::string, std::array<bool, 3>>& t, const std::string& kind) -> std::string {
        std::vector<std::string> ok; for (const auto& [n, a] : t) if (a[gcol]) ok.push_back(n);
        return ok.empty() ? std::string() : mass.path[kind + ":" + ok[rng() % ok.size()]];
    };
    auto put = [&](const std::string& kind, const std::string& f) { if (f.empty()) return; Human::takeOff(body, kind); Human::wear(body, f, err); };
    if (mass.hair) put("hair", pick(mass.hairT, "hair"));
    if (mass.eyebrows) put("eyebrows", pick(mass.browsT, "brows"));
    if (mass.eyelashes) put("eyelashes", pick(mass.lashesT, "lashes"));
    if (mass.fullClothes || mass.upperClothes || mass.lowerClothes || mass.shoes) {
        Human::takeOff(body, "clothes");
        if (mass.fullClothes) { const std::string f = pick(mass.fullT, "clothes"); if (!f.empty()) Human::wear(body, f, err); }
        if (mass.upperClothes) { const std::string f = pick(mass.upperT, "clothes"); if (!f.empty() && !Human::isWorn(body, f)) Human::wear(body, f, err); }
        if (mass.lowerClothes) { const std::string f = pick(mass.lowerT, "clothes"); if (!f.empty() && !Human::isWorn(body, f)) Human::wear(body, f, err); }
        if (mass.shoes) { const std::string f = pick(mass.shoesT, "clothes"); if (!f.empty() && !Human::isWorn(body, f)) Human::wear(body, f, err); }
    }
}
static void massStep() {
    if (massLeft <= 0) return;
    static std::mt19937 rng{std::random_device{}()};
    massOne(rng);
    Human::apply(body);
    if (!rig.empty()) Skel::build(rig, body.pos);
    if (!chosen.empty()) Skel::build(chosen, body.pos);
    repose();
    char nb[96]; snprintf(nb, sizeof nb, "%s%04d", mass.base, massLeft);   // MakeHuman counts down
    std::string err;
    if (mass.format < 0) saveModel((fs::u8path(homeDir()) / (std::string(nb) + ".mhm")).u8string(), nb, err);
    else exportTo((fs::u8path(homeDir()) / (std::string(nb) + EXPORT_EXT[mass.format])).u8string(), mass.format, err);
    if (!err.empty()) say(err);
    ++massDone; --massLeft;
    if (!massLeft) { restore(massStart); say("Done! Made " + std::to_string(massDone) + " in " + homeDir()); }
    dirty = true;
}
static void massTable3(const char* title, const char* first, std::map<std::string, std::array<bool, 3>>& t, float lw) {
    if (!ImGui::TreeNode(title)) return;
    if (ImGui::BeginTable(title, 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersInnerV, ImVec2(lw, std::min(250.0f, 30.0f + 24.0f * (float)t.size())))) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn(first); for (const char* c : {"Mixed", "Female", "Male"}) ImGui::TableSetupColumn(c, ImGuiTableColumnFlags_WidthFixed, 46);
        ImGui::TableHeadersRow();
        for (auto& [n, a] : t) {
            ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::TextUnformatted(n.c_str());
            for (int k = 0; k < 3; ++k) { ImGui::TableNextColumn(); ImGui::PushID((n + std::to_string(k)).c_str()); ImGui::Checkbox("##c", &a[k]); ImGui::PopID(); }
        }
        ImGui::EndTable();
    }
    ImGui::TreePop();
}
static void massSkinTable(const char* title, std::map<std::string, std::array<bool, 4>>& t, float lw) {
    if (!ImGui::TreeNode(title)) return;
    if (ImGui::BeginTable(title, 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersInnerV, ImVec2(lw, std::min(250.0f, 30.0f + 24.0f * (float)t.size())))) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Skin"); for (const char* c : {"Mixed", "African", "Asian", "Caucasian"}) ImGui::TableSetupColumn(c, ImGuiTableColumnFlags_WidthFixed, 40);
        ImGui::TableHeadersRow();
        for (auto& [n, a] : t) {
            ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::TextUnformatted(n.c_str());
            for (int k = 0; k < 4; ++k) { ImGui::TableNextColumn(); ImGui::PushID((n + std::to_string(k)).c_str()); ImGui::Checkbox("##c", &a[k]); ImGui::PopID(); }
        }
        ImGui::EndTable();
    }
    ImGui::TreePop();
}
static void massLeftPanel(float lw) {
    massBuild();
    auto range = [&](const char* label, bool* on, float* lo, float* hi) {
        ImGui::Checkbox((std::string("Randomize ") + label).c_str(), on);
        ImGui::BeginDisabled(!*on);
        ImGui::PushID(label);
        ImGui::SetNextItemWidth(lw); ImGui::SliderFloat("##lo", lo, 0, 1, (std::string("Minimum ") + label + " %.2f").c_str());
        ImGui::SetNextItemWidth(lw); ImGui::SliderFloat("##hi", hi, 0, 1, (std::string("Maximum ") + label + " %.2f").c_str());
        ImGui::PopID(); ImGui::EndDisabled();
    };
    groupBox("Macro settings");
    range("age", &mass.randAge, &mass.ageMin, &mass.ageMax);
    range("weight", &mass.randWeight, &mass.weightMin, &mass.weightMax);
    range("height", &mass.randHeight, &mass.heightMin, &mass.heightMax);
    range("muscle", &mass.randMuscle, &mass.muscleMin, &mass.muscleMax);
    ImGui::Checkbox("Randomize gender", &mass.gender); ImGui::Checkbox("Absolute gender", &mass.genderAbs);
    ImGui::Checkbox("Randomize ethnicity", &mass.ethnicity); ImGui::Checkbox("Absolute ethnicity", &mass.ethnicityAbs);
    groupBox("Modeling settings");
    for (auto& [g, on] : mass.groups) ImGui::Checkbox(("Randomize " + (g == "breast" ? std::string("breasts (if fem)") : g)).c_str(), &on);
    ImGui::SetNextItemWidth(lw); ImGui::SliderFloat("##md", &mass.maxdev, 0, 1, "Max deviation from default %.2f");
    groupBox("Randomize clothes and body parts");
    ImGui::Checkbox("Randomize hair", &mass.hair); ImGui::Checkbox("Randomize eyelashes", &mass.eyelashes); ImGui::Checkbox("Randomize eyebrows", &mass.eyebrows);
    ImGui::Checkbox("Randomize full body clothes", &mass.fullClothes); ImGui::Checkbox("Randomize upper body clothes", &mass.upperClothes);
    ImGui::Checkbox("Randomize lower body clothes", &mass.lowerClothes); ImGui::Checkbox("Randomize shoes", &mass.shoes);
    groupBox("Randomize materials");
    ImGui::Checkbox("Randomize skins", &mass.skins);
    groupBox("Allowed");
    massSkinTable("Allowed female skins", mass.femaleSkins, lw);
    massSkinTable("Allowed male skins", mass.maleSkins, lw);
    massTable3("Allowed hair", "Hair", mass.hairT, lw);
    massTable3("Allowed eyebrows", "Eyebrows", mass.browsT, lw);
    massTable3("Allowed eyelashes", "Eyelashes", mass.lashesT, lw);
    massTable3("Allowed full body clothes", "Clothes", mass.fullT, lw);
    massTable3("Allowed upper body clothes", "Clothes", mass.upperT, lw);
    massTable3("Allowed lower body clothes", "Clothes", mass.lowerT, lw);
    massTable3("Allowed shoes", "Clothes", mass.shoesT, lw);
    groupBox("Export settings");
    ImGui::TextUnformatted("File name base"); ImGui::SetNextItemWidth(lw); ImGui::InputText("##fnb", mass.base, sizeof mass.base);
    ImGui::TextUnformatted("File format");
    std::vector<const char*> f = {"MHM"}; for (int k = 0; k < EX_COUNT; ++k) f.push_back(EXPORT_NAMES[k]);
    int sel = mass.format + 1; ImGui::SetNextItemWidth(lw); if (ImGui::Combo("##mf", &sel, f.data(), (int)f.size())) mass.format = sel - 1;
    ImGui::TextDisabled("Files end up in %s, named with the file name base plus four digits plus file extension.", homeDir().c_str());
    groupBox("Produce");
    ImGui::SetNextItemWidth(lw); ImGui::SliderInt("##mc", &mass.count, 1, 500, "Number of characters: %d");
    if (massLeft > 0) {
        ImGui::ProgressBar((float)massDone / std::max(1, massTotal), ImVec2(lw, 0), (std::to_string(massDone) + " / " + std::to_string(massTotal)).c_str());
        if (ImGui::Button("Stop", ImVec2(lw, 0))) { massLeft = 0; restore(massStart); dirty = true; }
    } else if (ImGui::Button("Produce", ImVec2(lw, 34))) { remember("Mass produce"); massStart = snapshot(); massLeft = massTotal = mass.count; massDone = 0; }
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
    PROF("script command");
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
    else if (cmd == "camera") { lookFor(rest == "face" ? "faceCamera" : rest); Viewer::update(body); }   // a MakeHuman view: faceCamera, frontView, leftView...
    else if (cmd == "massproduce") {   // massproduce N [base]: Mass produce's settings, N people now
        massBuild(); std::istringstream ms(rest); int n = 5; std::string base; ms >> n >> base;
        if (!base.empty()) snprintf(mass.base, sizeof mass.base, "%s", base.c_str());
        massStart = snapshot(); massLeft = massTotal = std::max(1, n); massDone = 0;
        while (massLeft > 0) massStep();
        out("made " + std::to_string(massDone));
    }
    else if (cmd == "exportopt") {   // exportopt orient 0..3 | bones 0..2 | helpers | faceunits | fbxbinary | stlbinary | normals | feet | posed  (0/1)
        const int iv = (int)v; const bool bv = v != 0;
        if (a == "orient") exportOrient = std::clamp(iv, 0, 3); else if (a == "bones") exportBoneAxis = std::clamp(iv, 0, 2);
        else if (a == "helpers") exportHelpers = bv; else if (a == "faceunits") exportFaceUnits = bv; else if (a == "fbxbinary") exportFbxBinary = bv;
        else if (a == "stlbinary") exportStlBinary = bv; else if (a == "normals") exportObjNormals = bv; else if (a == "feet") exportFeet = bv; else if (a == "posed") exportPosed = bv;
        else { ok = false; err = "exportopt: orient, bones, helpers, faceunits, fbxbinary, stlbinary, normals, feet, posed"; }
    }
    else if (cmd == "randomize") randomizeMh(rest.empty() ? rSym : (float)atof(rest.c_str()), rMacro, rHeight, rFace, rBody, randomAmount);   // Modelling > Random, MakeHuman's way (randomize 0.7: the symmetry)
    else if (cmd == "savetarget") { Human::apply(body); std::istringstream ts(rest); std::string f, st; ts >> f >> st; ok = Human::saveTarget(body, f, err, st == "strip"); }   // savetarget file.target [strip]
    else if (cmd == "shaders") Viewer::look().mhShaders = rest != "off";   // MakeHuman's own shaders (on) or Pygmalion's
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
    else if (cmd == "lightmap" || cmd == "uvmap") { Human::apply(body); if (!rig.empty()) Skel::build(rig, body.pos); repose(); ok = exportTo(rest, cmd == "lightmap" ? EX_LIGHT : EX_UV, err); }
    else if (cmd == "export" || cmd == "render") {
        Human::apply(body); if (!rig.empty()) Skel::build(rig, body.pos); if (!chosen.empty()) Skel::build(chosen, body.pos); repose(); Viewer::update(body);
        if (cmd == "export") {
            int fmt = -1;
            for (int k = 0; k < EX_COUNT; ++k) { const std::string e = EXPORT_EXT[k]; if (rest.size() > e.size() && rest.compare(rest.size() - e.size(), e.size(), e) == 0) fmt = k; }
            if (fmt == EX_UV || fmt == EX_LIGHT) fmt = -1;
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
    else if (cmd == "material") {   // material skin|<kind> save file.mhmat | material skin|<kind> file.mhmat
        std::istringstream ms(rest); std::string who, verb, file; ms >> who >> verb; std::getline(ms >> std::ws, file);
        if (verb != "save") { file = verb + (file.empty() ? "" : " " + file); verb = "load"; }
        Mat::Material* m = who == "skin" ? &body.skinMat : nullptr;
        for (auto& px : body.proxies) if (!m && px.kind == who) m = &px.mat;
        if (!m) { ok = false; err = "nothing worn of the kind " + who; }
        else if (verb == "save") ok = Mat::toFile(*m, file, err, {"Written by Pygmalion"});
        else { const std::string f = Human::absPath(body, file).empty() ? file : Human::absPath(body, file); ok = who == "skin" ? Human::setSkin(body, f) : Human::setProxyMaterial(body, who, f); if (!ok) err = "couldn't load " + file; }
    }
    else if (cmd == "help") out(COMMANDS);
    else { ok = false; err = "unknown command " + cmd + " (help lists them)"; }
    return ok;
}
static void runScript(const std::string& text);
#ifdef _WIN32
static bool runQuiet(const std::wstring& cmd);
#endif

// ---------- Settings > Plugins and User Plugins (MakeHuman's plugin list, and 5_settings_userplugins) ----------
// Pygmalion's own parts stand where MakeHuman's plugins do: one off is gone after a restart. User plugins are Pygmalion
// scripts (.pygscript, the Scripting tab's commands) in the plugins folder; the checked ones run at the start.
static std::string pluginsDir() { const fs::path d = fs::u8path(homeDir()) / "plugins"; std::error_code ec; fs::create_directories(d, ec); return d.u8string(); }
static std::vector<std::string> userPlugins() {
    std::vector<std::string> v; std::error_code ec;
    for (const auto& e : fs::directory_iterator(fs::u8path(pluginsDir()), ec)) {
        const std::string n = e.path().filename().u8string();
        if (n.empty() || n[0] == '_') continue;
        if (e.is_directory(ec) && fs::exists(e.path() / "main.pygscript", ec)) v.push_back(n);
        else if (e.path().extension() == ".pygscript") v.push_back(e.path().stem().u8string());
    }
    std::sort(v.begin(), v.end());
    return v;
}
static std::string pluginFile(const std::string& name) {
    const fs::path d = fs::u8path(pluginsDir()); std::error_code ec;
    if (fs::exists(d / name / "main.pygscript", ec)) return (d / name / "main.pygscript").u8string();
    return (d / (name + ".pygscript")).u8string();
}
static void runPlugin(const std::string& name) {
    std::ifstream in(fs::u8path(pluginFile(name))); std::ostringstream ss; ss << in.rdbuf();
    runScript(ss.str()); say("Plugin " + name + " ran");
}
static void pluginsLeft(float lw) {
    groupBox("Plugins");
    ImGui::TextWrapped("Untick a part to leave it out; it's gone after a restart (MakeHuman's plugins work the same way).");
    for (int c = 0; c < CAT_COUNT; ++c) {
        if (!ImGui::TreeNode(CAT_NAMES[c])) continue;
        for (const auto& t : tasksOfAll(c)) {
            const std::string key = std::string(CAT_NAMES[c]) + "/" + t;
            if (key == "Settings/Plugins") continue;
            bool on = !excludedTasks.count(key);
            if (ImGui::Checkbox(t.c_str(), &on)) { if (on) excludedTasks.erase(key); else excludedTasks.insert(key); }
        }
        ImGui::TreePop();
    }
}
static void userPluginsLeft(float lw) {
    groupBox("User Plugins");
    const auto list = userPlugins();
    if (list.empty()) ImGui::TextDisabled("None in %s", pluginsDir().c_str());
    for (const auto& n : list) {
        bool on = std::find(activeUserPlugins.begin(), activeUserPlugins.end(), n) != activeUserPlugins.end();
        if (ImGui::Checkbox(n.c_str(), &on)) { if (on) activeUserPlugins.push_back(n); else activeUserPlugins.erase(std::remove(activeUserPlugins.begin(), activeUserPlugins.end(), n), activeUserPlugins.end()); }
    }
    ImGui::Spacing();
    if (ImGui::Button("Install Plugin File", ImVec2(lw, 0))) {
        const std::string f = fileDialog(false, L"Pygmalion script (*.pygscript)\0*.pygscript\0All files\0*.*\0\0", L"pygscript", homeDir() + "\\");
        if (!f.empty()) {
            std::error_code ec; fs::copy_file(fs::u8path(f), fs::u8path(pluginsDir()) / fs::u8path(f).filename(), fs::copy_options::overwrite_existing, ec);
            say(ec ? "Failed to copy " + f + " to the user plugins folder" : "The plugin copied successfully. To activate, check the plugin in the list and press \"Activate Plugins\" or restart Pygmalion.");
        }
    }
    if (ImGui::Button("Install Zipped Plugin", ImVec2(lw, 0))) {
        const std::string f = fileDialog(false, L"Zip files\0*.zip\0All files\0*.*\0\0", L"zip", homeDir() + "\\");
        if (!f.empty()) say(runQuiet(L"tar.exe -xf \"" + fs::u8path(f).wstring() + L"\" -C \"" + fs::u8path(pluginsDir()).wstring() + L"\"")
                            ? "The plugin copied successfully. To activate, check the plugin in the list and press \"Activate Plugins\" or restart Pygmalion." : "Not a zip file " + f);
    }
    if (ImGui::Button("Reload Plugins Folder", ImVec2(lw, 0))) say("Plugins folder read again");
    if (ImGui::Button("Activate Plugins", ImVec2(lw, 0))) for (const auto& n : activeUserPlugins) runPlugin(n);
    if (ImGui::Button("Open the plugins folder", ImVec2(lw, 0))) ShellExecuteW(nullptr, L"open", fs::u8path(pluginsDir()).wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    groupBox("Info");
    ImGui::TextWrapped("Install new plugins by either copying to the user plugins folder or using the built in installer. A plugin is a Pygmalion script "
                       "(.pygscript: the Scripting tab's commands, one a line), or a folder with a main.pygscript in it.\n\nTo activate a plugin it must be "
                       "checked in the list. Then click the \"Activate Plugins\" button or restart Pygmalion. MakeHuman's own Python plugins can't run here.");
}

// ---------- Utilities > Profile (MakeHuman's 7_profile): the main steps' timings, sortable, saved as text ----------
static int profKey = 0, profDir = 0;
static void profileLeft(float lw) {
    static const char* KEYS[] = {"Total calls", "Total time", "Total per call", "Cumulative time", "Cumulative per call", "Function"};
    groupBox("Sort");
    for (int k = 0; k < 6; ++k) ImGui::RadioButton(KEYS[k], &profKey, k);
    groupBox("Sort ");
    ImGui::RadioButton("Descending", &profDir, 0); ImGui::RadioButton("Ascending", &profDir, 1);
    groupBox("Profile");
    ImGui::Checkbox("Timing on", &Prof::on);
    if (ImGui::Button("Clear", ImVec2(lw, 0))) Prof::reset();
    groupBox("Save");
    if (ImGui::Button("Save...", ImVec2(lw, 0))) {
        const std::string f = fileDialog(true, L"Text\0*.txt\0\0", L"txt", homeDir() + "\\profile.txt");
        if (!f.empty()) {
            std::ofstream o(fs::u8path(f));
            o << "Calls\tTotal\tTotal/Call\tCumul.\tCumul/Call\tFunction\n";
            for (const auto& [n, s] : Prof::stats()) o << s.calls << "\t" << s.total << "\t" << (s.calls ? s.total / s.calls : 0) << "\t" << s.cumul << "\t" << (s.calls ? s.cumul / s.calls : 0) << "\t" << n << "\n";
            say("Saved " + f);
        }
    }
    groupBox("Statistics");
    void profileTable(float w, float h);
    profileTable(lw, 420);
}
void profileTable(float w, float h) {
    std::vector<std::pair<std::string, Prof::Stat>> rows(Prof::stats().begin(), Prof::stats().end());
    auto val = [&](const std::pair<std::string, Prof::Stat>& r) -> double {
        const auto& s = r.second;
        switch (profKey) { case 0: return (double)s.calls; case 1: return s.total; case 2: return s.calls ? s.total / s.calls : 0; case 3: return s.cumul; case 4: return s.calls ? s.cumul / s.calls : 0; default: return 0; }
    };
    std::sort(rows.begin(), rows.end(), [&](const auto& a, const auto& b) {
        const bool less = profKey == 5 ? a.first < b.first : val(a) < val(b);
        const bool more = profKey == 5 ? b.first < a.first : val(b) < val(a);
        return profDir == 1 ? less : more;
    });
    if (ImGui::BeginTable("prof", 6, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY | ImGuiTableFlags_ScrollX | ImGuiTableFlags_SizingFixedFit, ImVec2(w, h))) {
        ImGui::TableSetupScrollFreeze(0, 1);
        for (const char* c : {"Calls", "Total", "Total/Call", "Cumul.", "Cumul/Call"}) ImGui::TableSetupColumn(c, ImGuiTableColumnFlags_WidthFixed, 62);
        ImGui::TableSetupColumn("Function", ImGuiTableColumnFlags_WidthFixed, 200);
        ImGui::TableHeadersRow();
        for (const auto& [n, s] : rows) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("%lld", s.calls);
            ImGui::TableNextColumn(); ImGui::Text("%.3f", s.total);
            ImGui::TableNextColumn(); ImGui::Text("%.3f", s.calls ? s.total / s.calls : 0);
            ImGui::TableNextColumn(); ImGui::Text("%.3f", s.cumul);
            ImGui::TableNextColumn(); ImGui::Text("%.3f", s.calls ? s.cumul / s.calls : 0);
            ImGui::TableNextColumn(); ImGui::TextUnformatted(n.c_str());
        }
        ImGui::EndTable();
    }
}

// ---------- Utilities > Example (MakeHuman's 7_example): every kind of widget, and a slider that pushes the mesh ----------
static int exPushed = 0, exRadio = 0; static bool exToggle = false; static float exSlider = 0.5f; static char exText[128] = "Some text";
static void exampleLeft(float lw) {
    groupBox("Example");
    if (ImGui::Button("Button", ImVec2(lw, 0))) ++exPushed;
    ImGui::Text("Pushed %d times", exPushed);
    ImGui::Checkbox("ToggleButton", &exToggle);
    ImGui::TextUnformatted(exToggle ? "Selected" : "Not selected");
    ImGui::RadioButton("RadioButton1", &exRadio, 0); ImGui::RadioButton("RadioButton2", &exRadio, 1);
    ImGui::TextUnformatted(exRadio == 0 ? "Button 1 is selected" : "Button 2 is selected");
    ImGui::SetNextItemWidth(lw); ImGui::SliderFloat("##exs", &exSlider, 0, 1, "Slider %.2f");
    ImGui::Text("Value is %f", exSlider);
    ImGui::ProgressBar(exSlider, ImVec2(lw, 0));
    ImGui::SetNextItemWidth(lw); ImGui::InputText("##ext", exText, sizeof exText);
    // MakeHuman's "Mesh distort": the body pushed out along its normals (in the view only: the shape itself is untouched)
    ImGui::SetNextItemWidth(lw);
    if (ImGui::SliderFloat("##exm", &Viewer::look().distort, -0.5f, 0.5f, "Mesh distort %0.2f")) dirty = true;
    ImGui::TextDisabled("This is an example plugin; see plugins/7_example.py in MakeHuman.");
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
// Utilities > Logs (MakeHuman's 7_logging): the level shown and up, lines picked (click; Ctrl+click for more) and copied
static int logLevel = 0;
static void logsView() {
    groupBox("Level");
    static const char* LEVELS[] = {"Debug", "Message", "Notice", "Warning", "Error"};
    for (int k = 0; k < 5; ++k) ImGui::RadioButton(LEVELS[k], &logLevel, k);
    if (ImGui::Button("Copy to Clipboard")) {
        std::string text;
        for (size_t i = 0; i < logs.size(); ++i) if (i < logPicked.size() && logPicked[i]) text += logs[i] + "\n";
        if (text.empty()) say("No log items selected"); else ImGui::SetClipboardText(text.c_str());
    }
    ImGui::SameLine(); if (ImGui::Button("Clear")) { logs.clear(); logLevels.clear(); logPicked.clear(); }
    groupBox("Logs");
    ImGui::BeginChild("lg");
    static const ImU32 COLS[] = {IM_COL32(0x9a, 0x9a, 0x9a, 255), IM_COL32(0xe6, 0xe6, 0xe6, 255), IM_COL32(0x9c, 0xd7, 0xff, 255), IM_COL32(0xff, 0xc8, 0x5a, 255), IM_COL32(0xff, 0x6b, 0x6b, 255)};
    for (size_t i = 0; i < logs.size(); ++i) {
        const int lv = i < logLevels.size() ? logLevels[i] : 1;
        if (lv < logLevel) continue;
        ImGui::PushID((int)i); ImGui::PushStyleColor(ImGuiCol_Text, COLS[std::clamp(lv, 0, 4)]);
        if (ImGui::Selectable(logs[i].c_str(), logPicked[i] != 0)) {
            if (!ImGui::GetIO().KeyCtrl) std::fill(logPicked.begin(), logPicked.end(), 0);
            logPicked[i] = !logPicked[i];
        }
        ImGui::PopStyleColor(); ImGui::PopID();
    }
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
            ImGui::Checkbox("Update normals real-time", &Viewer::look().normalsWhileDragging);
            ImGui::Checkbox("Fit objects real-time", &Viewer::look().fitWhileDragging);
            ImGui::Checkbox("Camera follows the slider", &prefs.camFollows);
            groupBox("Slider appearance");
            ImGui::Checkbox("Show slider values", &prefs.showValues);
            ImGui::Checkbox("Show slider images", &prefs.sliderImages);
        } else if (t == "General") {
            groupBox("Units");
            int u = prefs.metric ? 0 : 1;
            if (ImGui::RadioButton("Metric (cm)", &u, 0)) prefs.metric = true;
            if (ImGui::RadioButton("Imperial (in)", &u, 1)) prefs.metric = false;
            groupBox("Weight");
            { int wv = prefs.realWeight ? 1 : 0; if (ImGui::RadioButton("Relative Weight", &wv, 0)) prefs.realWeight = false; if (ImGui::RadioButton("Real Weight", &wv, 1)) prefs.realWeight = true; }
            groupBox(("Tag Filter Mode: " + std::string(prefs.tagMode == 0 ? "OR" : prefs.tagMode == 1 ? "AND" : prefs.tagMode == 2 ? "NOT OR" : "NOT AND")).c_str());
            ImGui::RadioButton("OR", &prefs.tagMode, 0); ImGui::SameLine(lw / 2); ImGui::RadioButton("AND", &prefs.tagMode, 1);
            ImGui::RadioButton("NOT OR", &prefs.tagMode, 2); ImGui::SameLine(lw / 2); ImGui::RadioButton("NOT AND", &prefs.tagMode, 3);
            groupBox("Tags Count");
            ImGui::SetNextItemWidth(lw); ImGui::SliderInt("##tc", &prefs.tagCount, 1, 20, "%d Tags");
            groupBox("Name Tags");
            ImGui::Checkbox("Use Name Tags", &prefs.nameTags);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("The choosers show each file's own name (its \"name\" line) instead of its file name.");
            groupBox("Startup");
            ImGui::Checkbox("Preload macro targets", &prefs.preload);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Pygmalion loads every target at the start either way: there's nothing left to load later.");
            ImGui::Checkbox("Restore window size", &prefs.restoreWindow);
            if (ImGui::Checkbox("Use HDPI", &prefs.hdpi)) say("You need to restart for these changes to be applied.");
            { bool noSh = !Viewer::look().mhShaders; if (ImGui::Checkbox("No shaders", &noSh)) Viewer::look().mhShaders = !noSh; }
            ImGui::Checkbox("No sample buffers", &prefs.noSampleBuffers);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Renders without anti-aliasing (Rendering > Render).");
            groupBox("Theme");
            { int th = theme; bool c1 = ImGui::RadioButton("Pygmalion", &th, 0); bool c2 = ImGui::RadioButton("MakeHuman", &th, 1); bool c3 = ImGui::RadioButton("Native look", &th, 2); if (c1 || c2 || c3) { theme = th; applyStyle(); } }
            groupBox("Slider's Edit Widget Visibility");
            { int ev = prefs.sliderEdit ? 0 : 1; if (ImGui::RadioButton("Visible", &ev, 0)) prefs.sliderEdit = true; if (ImGui::RadioButton("Default", &ev, 1)) prefs.sliderEdit = false; }
            groupBox("Shading");
            ImGui::Checkbox("MakeHuman's shaders", &Viewer::look().mhShaders);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Each object drawn with its material's own shader (litsphere, phong, toon, xray...), as MakeHuman draws it.\nOff: Pygmalion's own shading, lit by every light in the scene.");
            groupBox("Configure Home Folder");
            {
                std::error_code ec; const bool has = fs::exists(fs::u8path(configFile()), ec);
                if (ImGui::Button(has ? "Delete Config File" : "Create Config File", ImVec2(lw, 0))) {
                    if (has) { fs::remove(fs::u8path(configFile()), ec); say("Home Folder Location: Default (after a restart)"); }
                    else {
                        const std::string d = folderDialog();
                        if (!d.empty()) { fs::create_directories(fs::u8path(configFile()).parent_path(), ec); std::ofstream o(fs::u8path(configFile())); o << d << "\n"; say("Home Folder Location: " + d + " (after a restart)"); }
                    }
                }
            }
            groupBox("Models folder");
            ImGui::TextWrapped("%s", homeDir().c_str());
            groupBox("Settings");
            notYet(("Kept in " + settingsFile() + " when Pygmalion closes.").c_str());
            if (ImGui::Button("Restore the defaults", ImVec2(lw, 0))) {
                prefs = Prefs(); theme = 0; applyStyle();
                Viewer::Look& lk = Viewer::look(); lk.normalsWhileDragging = lk.fitWhileDragging = lk.mhShaders = true; lk.turnSpeed = lk.moveSpeed = lk.zoomSpeed = 1; lk.invertTurn = lk.invertZoom = false; lk.bgImage.clear(); lk.bgOpacity = 1;
                say("Settings back to their defaults");
            }
        } else if (t == "Background") backgroundLeft(lw);
        else if (t == "Plugins") pluginsLeft(lw);
        else if (t == "User Plugins") userPluginsLeft(lw);
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
        else if (t == "Scene Editor") sceneEditorLeft(lw);
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
        else if (t == "Profile") profileLeft(lw);
        else if (t == "Example") exampleLeft(lw);
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
    Viewer::look().dragging = ImGui::IsMouseDown(ImGuiMouseButton_Left) && ImGui::GetIO().WantCaptureMouse && ImGui::IsAnyItemActive();
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
    Human::setAssetRoots(body.roots);
    Mat::setShaderFolder((fs::u8path(dataDir) / "shaders" / "glsl").u8string());
    Human::setSkin(body, "");   // the default skin's material (skins/default.mhmat)
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
    for (const auto& n : activeUserPlugins) if (fs::exists(fs::u8path(pluginFile(n)))) runPlugin(n);   // Settings > User Plugins
    if (!startScript.empty()) {   // a script file run at the start (and its output in script-log.txt beside it)
        std::ifstream in(fs::u8path(startScript)); std::ostringstream ss; ss << in.rdbuf();
        runScript(ss.str());
        std::ofstream lo(fs::u8path(startScript).replace_extension(".log")); lo << scriptLog;
    }
    dirty = true;
#ifdef _WIN32
    if ((sockCfg.accept || socketArg) && capture.empty()) socketOpen();   // as MakeHuman: the server comes back on if it was on
#endif
    applyStyle();
    if (prefs.hdpi) { float dpi = 96; if (SDL_GetDisplayDPI(0, &dpi, nullptr, nullptr) == 0 && dpi > 96) ImGui::GetIO().FontGlobalScale = dpi / 96; }
    {   // 1_convert_model_tags: old "a,,b" tags in the saved models, once, the files kept as .mhm~
        std::vector<std::string> log; Human::convertModelTags(homeDir(), log);
        for (const auto& l : log) say(l);
    }
    if (capture.empty()) {
        SDL_ShowWindow(window);
        if (prefs.restoreWindow && prefs.win[2] > 300 && prefs.win[3] > 200) { SDL_SetWindowPosition(window, prefs.win[0], prefs.win[1]); SDL_SetWindowSize(window, prefs.win[2], prefs.win[3]); }
        else SDL_MaximizeWindow(window);
        SDL_RaiseWindow(window);
    }
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
    if (capture.empty() && !(SDL_GetWindowFlags(window) & (SDL_WINDOW_MAXIMIZED | SDL_WINDOW_MINIMIZED))) { SDL_GetWindowPosition(window, &prefs.win[0], &prefs.win[1]); SDL_GetWindowSize(window, &prefs.win[2], &prefs.win[3]); }
    if (capture.empty()) saveSettings();
    ImGui_ImplOpenGL3_Shutdown(); ImGui_ImplSDL2_Shutdown(); ImGui::DestroyContext();
    SDL_GL_DeleteContext(gl); SDL_DestroyWindow(window); SDL_Quit();
    return 0;
}
