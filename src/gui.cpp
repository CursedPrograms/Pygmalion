// Pygmalion - make people in 3D with sliders, on your own PC, in C++. Part of Atelier.
// MakeHuman's body, its 1,280 shapes and every one of its sliders (gender, age, muscle, weight, height, proportions,
// ethnicity, then the face, the torso, the arms and legs, measurements and body shapes); words that set the sliders;
// a random person; MakeHuman's own .mhm files; GLB, OBJ and STL out.
//
//   Pygmalion [model.mhm]
//   Pygmalion --capture out.bmp [--describe "words"] [--tab N]      a screenshot, then quit
#include "human.h"
#include "model.h"
#include "splash.h"
#include "viewer.h"

#include <SDL.h>
#include <SDL_opengl.h>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl2.h>
#include <stb_image_write.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <functional>
#include <mutex>
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
static bool dirty = true;
static int tab = 0, forcedTab = -1;
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

// the shaped body as a mesh for the exports: the body's visible triangles and the eyes
static Mesh toMesh() {
    Mesh m;
    m.mats.push_back(Material{"skin"});
    for (size_t t = 0; t < body.tri.size(); ++t) {
        const int v = body.tri[t], u = body.triUv[t];
        m.pos.insert(m.pos.end(), &body.pos[(size_t)v * 3], &body.pos[(size_t)v * 3] + 3);
        if (u >= 0 && (size_t)u * 2 + 1 < body.uv.size()) m.uv.insert(m.uv.end(), {body.uv[(size_t)u * 2], body.uv[(size_t)u * 2 + 1]}); else m.uv.insert(m.uv.end(), {0, 0});
        if (t % 3 == 2) m.matOf.push_back(0);
    }
    if (!body.proxies.empty()) {
        const Human::Proxy& p = body.proxies[0];
        std::vector<float> fit; Human::fitProxy(body, p, fit);
        Material eye{"eyes"}; eye.texturePath = p.texture;
        m.mats.push_back(eye);
        for (size_t c = 0; c < p.tri.size(); ++c) {
            m.pos.insert(m.pos.end(), &fit[(size_t)p.tri[c] * 3], &fit[(size_t)p.tri[c] * 3] + 3);
            m.uv.insert(m.uv.end(), {p.uv[c * 2], p.uv[c * 2 + 1]});
            if (c % 3 == 2) m.matOf.push_back(1);
        }
    }
    computeNormals(m);
    return m;
}

// ---------- the sliders ----------
static void tip(const std::string& s) { if (!s.empty() && ImGui::IsItemHovered()) { ImGui::BeginTooltip(); ImGui::PushTextWrapPos(380); ImGui::TextUnformatted(s.c_str()); ImGui::PopTextWrapPos(); ImGui::EndTooltip(); } }

static bool modSlider(int i, float w) {
    Human::Modifier& m = body.mods[i];
    float v = m.value;
    ImGui::PushID(i);
    ImGui::SetNextItemWidth(w);
    char fmt[160]; snprintf(fmt, sizeof fmt, "%s: %s", m.label.c_str(), m.min() < 0 ? "%+.2f" : "%.2f");
    static bool held = false;
    const bool ch = ImGui::SliderFloat("##m", &v, m.min(), 1.0f, fmt);
    if (ImGui::IsItemActivated()) { remember(m.label); held = true; }
    if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) { remember(m.label); Human::setModifier(body, i, m.defaultValue); dirty = true; }
    tip(m.description + (m.description.empty() ? "" : "\n") + "Right-click: back to the start; Ctrl+click: type a value");
    if (ch) { Human::setModifier(body, i, v); dirty = true; }
    ImGui::PopID();
    return ch;
}

static void macroPanel(float w) {
    auto macro = [&](const char* var, const char* fmt, std::function<std::string(float)> shown, const char* help) {
        auto it = body.modByName.end();
        for (auto i = body.modByName.begin(); i != body.modByName.end(); ++i) if (body.mods[i->second].macro && body.mods[i->second].variable == var) { it = i; break; }
        if (it == body.modByName.end()) return;
        Human::Modifier& m = body.mods[it->second];
        float v = m.value;
        ImGui::PushID(var);
        ImGui::SetNextItemWidth(w);
        std::string label = shown ? shown(v) : std::string(fmt);
        for (size_t at = 0; (at = label.find('%', at)) != std::string::npos; at += 2) label.insert(at, "%");   // the label is a format: % shown as %
        const bool ch = ImGui::SliderFloat("##v", &v, 0, 1, label.c_str());
        if (ImGui::IsItemActivated()) remember(var);
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) { remember(var); Human::setMacro(body, var, m.defaultValue); dirty = true; }
        tip(help);
        if (ch) { Human::setMacro(body, var, v); dirty = true; }
        ImGui::PopID();
    };
    char b[96];
    macro("Gender", nullptr, [&](float v) { snprintf(b, sizeof b, "Gender: %.0f%% %s", std::fabs(v - 0.5f) * 200, v < 0.5f ? "female" : v > 0.5f ? "male" : "(in between)"); return std::string(b); }, "Female to male");
    macro("Age", nullptr, [&](float) { snprintf(b, sizeof b, "Age: %.0f years", Human::ageYears(body)); return std::string(b); }, "1 to 90 years");
    macro("Muscle", nullptr, [&](float v) { snprintf(b, sizeof b, "Muscle: %.0f%%", v * 100); return std::string(b); }, "Soft to muscular");
    macro("Weight", nullptr, [&](float v) { snprintf(b, sizeof b, "Weight: %.0f%%", 50 + (v - 0.5f) * 100); return std::string(b); }, "Thin to heavy");
    macro("Height", nullptr, [&](float) { snprintf(b, sizeof b, "Height: %.0f cm", Human::heightCm(body)); return std::string(b); }, "Short to tall (the height shown is measured on the body)");
    macro("BodyProportions", nullptr, [&](float v) { snprintf(b, sizeof b, "Proportions: %.0f%% %s", v * 100, v > 0.6f ? "ideal" : v < 0.4f ? "uncommon" : "regular"); return std::string(b); }, "Uncommon, regular or ideal proportions");
    ImGui::TextDisabled("Where from (the three add up to 100%)");
    macro("African", nullptr, [&](float v) { snprintf(b, sizeof b, "African: %.0f%%", v * 100); return std::string(b); }, "");
    macro("Asian", nullptr, [&](float v) { snprintf(b, sizeof b, "Asian: %.0f%%", v * 100); return std::string(b); }, "");
    macro("Caucasian", nullptr, [&](float v) { snprintf(b, sizeof b, "Caucasian: %.0f%%", v * 100); return std::string(b); }, "");
    ImGui::TextDisabled("Breasts");
    macro("BreastSize", nullptr, [&](float v) { snprintf(b, sizeof b, "Breast size: %.0f%%", v * 100); return std::string(b); }, "");
    macro("BreastFirmness", nullptr, [&](float v) { snprintf(b, sizeof b, "Breast firmness: %.0f%%", v * 100); return std::string(b); }, "");
}

static void tabPanel(const Human::Tab& t, float w) {
    const std::string q = search;
    for (const auto& g : t.groups) {
        if (q.empty()) {
            if (!ImGui::CollapsingHeader(g.name.c_str())) continue;
        } else {
            bool any = false;
            for (int i : g.mods) { std::string l = body.mods[i].label; std::transform(l.begin(), l.end(), l.begin(), ::tolower); std::string qq = q; std::transform(qq.begin(), qq.end(), qq.begin(), ::tolower); if (l.find(qq) != std::string::npos) any = true; }
            if (!any) continue;
            ImGui::TextColored(col4(C_PINK), "%s", g.name.c_str());
        }
        for (int i : g.mods) {
            if (!q.empty()) { std::string l = body.mods[i].label; std::transform(l.begin(), l.end(), l.begin(), ::tolower); std::string qq = q; std::transform(qq.begin(), qq.end(), qq.begin(), ::tolower); if (l.find(qq) == std::string::npos) continue; }
            if (body.mods[i].macro) continue;
            modSlider(i, w);
        }
    }
}

static void leftPanel(float w) {
    ImGui::TextColored(col4(C_PINK), "DESCRIBE");
    ImGui::SetNextItemWidth(w);
    const bool enter = ImGui::InputTextWithHint("##prompt", "a tall, muscular old woman with a big nose", prompt, sizeof prompt, ImGuiInputTextFlags_EnterReturnsTrue);
    tip("Words like: woman, man, 34 years old, child, old, tall, short, thin, fat, muscular, athletic, african, asian, caucasian, ideal, big nose, round face, square jaw, long neck, broad shoulders, wide hips, pregnant. very / slightly change how much.");
    if (ImGui::Button("Make them", ImVec2((w - 7) / 2, 0)) || enter) {
        remember("Describe");
        understood = Human::describe(body, prompt);
        if (understood.empty()) understood = "(no words it knows: hover the box for a list)";
        dirty = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Random", ImVec2((w - 7) / 2, 0))) { remember("Random"); Human::randomize(body, randomAmount, randMacros, randFace, randBody, (unsigned)std::random_device{}()); dirty = true; }
    if (!understood.empty()) { ImGui::PushTextWrapPos(w + 14); ImGui::TextDisabled("%s", understood.c_str()); ImGui::PopTextWrapPos(); }
    if (ImGui::CollapsingHeader("Random settings")) {
        ImGui::SetNextItemWidth(w); ImGui::SliderFloat("##ra", &randomAmount, 0.1f, 1.0f, "how far: %.2f");
        ImGui::Checkbox("Body type", &randMacros); ImGui::SameLine(); ImGui::Checkbox("Face", &randFace); ImGui::SameLine(); ImGui::Checkbox("Body", &randBody);
    }
    ImGui::Spacing();
    ImGui::TextColored(col4(C_PINK), "MAIN");
    macroPanel(w);
    ImGui::Spacing();
    ImGui::Checkbox("Symmetry (left moves right too)", &body.symmetry);
    if (ImGui::Button("Undo", ImVec2((w - 7) / 2, 0)) && !undo.empty()) { restore(undo.back().first); say("Undid " + undo.back().second); undo.pop_back(); }
    ImGui::SameLine();
    if (ImGui::Button("Start again", ImVec2((w - 7) / 2, 0))) { remember("Start again"); Human::resetAll(body); dirty = true; }
}

static void frame() {
    ImGuiIO& io = ImGui::GetIO();
    const float W = io.DisplaySize.x, H = io.DisplaySize.y;
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z) && !undo.empty() && !io.WantTextInput) { restore(undo.back().first); undo.pop_back(); }
    float top = 0;
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New person")) { remember("New"); Human::resetAll(body); dirty = true; }
            if (ImGui::MenuItem("Open (MakeHuman .mhm)...")) {
                const std::string p = fileDialog(false, L"MakeHuman model\0*.mhm\0\0", L"mhm", homeDir() + "\\");
                std::string err;
                if (!p.empty()) { remember("Open"); if (Human::loadMhm(body, p, err)) { dirty = true; say("Opened " + fs::u8path(p).filename().u8string()); } else say(err); }
            }
            if (ImGui::MenuItem("Save (MakeHuman .mhm)...")) {
                const std::string p = fileDialog(true, L"MakeHuman model\0*.mhm\0\0", L"mhm", homeDir() + "\\person.mhm");
                std::string err;
                if (!p.empty()) say(Human::saveMhm(body, p, fs::u8path(p).stem().u8string(), err) ? "Saved (MakeHuman opens it too)" : err);
            }
            ImGui::Separator();
            auto exportAs = [&](const wchar_t* filter, const wchar_t* ext, const char* name, int kind) {
                const std::string p = fileDialog(true, filter, ext, homeDir() + "\\" + name);
                if (p.empty()) return;
                std::string err;
                const Mesh m = toMesh();
                const bool ok = kind == 0 ? saveGlb(m, p, exportScale, false, err) : kind == 1 ? saveObj(m, p, exportScale, false, err) : saveStl(m, p, exportScale, false, err);
                say(ok ? "Saved " + fs::u8path(p).filename().u8string() : err);
            };
            if (ImGui::MenuItem("Export GLB (any 3D app)...")) exportAs(L"GLB\0*.glb\0\0", L"glb", "person.glb", 0);
            if (ImGui::MenuItem("Export OBJ...")) exportAs(L"OBJ\0*.obj\0\0", L"obj", "person.obj", 1);
            if (ImGui::MenuItem("Export STL (3D printing)...")) exportAs(L"STL\0*.stl\0\0", L"stl", "person.stl", 2);
            if (ImGui::BeginMenu("Export size")) {
                if (ImGui::MenuItem("Metres (Blender, game engines)", nullptr, exportScale == 0.1f)) exportScale = 0.1f;
                if (ImGui::MenuItem("Decimetres (MakeHuman's own)", nullptr, exportScale == 1.0f)) exportScale = 1.0f;
                if (ImGui::MenuItem("Centimetres", nullptr, exportScale == 10.0f)) exportScale = 10.0f;
                if (ImGui::MenuItem("Millimetres (3D printing)", nullptr, exportScale == 100.0f)) exportScale = 100.0f;
                ImGui::EndMenu();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Quit")) quit = true;
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            Viewer::Look& lk = Viewer::look();
            if (ImGui::MenuItem("Front")) Viewer::camera(Viewer::CAM_FRONT);
            if (ImGui::MenuItem("Side")) Viewer::camera(Viewer::CAM_SIDE);
            if (ImGui::MenuItem("Back")) Viewer::camera(Viewer::CAM_BACK);
            if (ImGui::MenuItem("Face")) Viewer::camera(Viewer::CAM_FACE);
            if (ImGui::MenuItem("Face, side")) Viewer::camera(Viewer::CAM_FACE_SIDE);
            ImGui::Separator();
            ImGui::MenuItem("Skin (else clay)", nullptr, nullptr);
            if (ImGui::MenuItem("Clay", nullptr, lk.shading == 1)) lk.shading = lk.shading == 1 ? 0 : 1;
            ImGui::MenuItem("Wireframe", nullptr, &lk.wire);
            ImGui::MenuItem("Floor", nullptr, &lk.grid);
            ImGui::EndMenu();
        }
        top = ImGui::GetWindowSize().y;
        ImGui::EndMainMenuBar();
    }
    const float statusH = 26, leftW = 360, rightW = 400;
    const ImGuiWindowFlags fixed = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus;
    ImGui::SetNextWindowPos(ImVec2(0, top)); ImGui::SetNextWindowSize(ImVec2(leftW, H - top - statusH));
    ImGui::Begin("left", nullptr, fixed);
    if (!loadErr.empty()) { ImGui::PushTextWrapPos(leftW - 20); ImGui::TextColored(col4(C_PINK), "%s", loadErr.c_str()); ImGui::PopTextWrapPos(); }
    else leftPanel(leftW - 28);
    ImGui::End();

    // the sliders by MakeHuman's tabs, on the right
    ImGui::SetNextWindowPos(ImVec2(W - rightW, top)); ImGui::SetNextWindowSize(ImVec2(rightW, H - top - statusH));
    ImGui::Begin("right", nullptr, fixed);
    ImGui::SetNextItemWidth(rightW - 28);
    ImGui::InputTextWithHint("##search", "find a slider (nose, ear, hip...)", search, sizeof search);
    if (ImGui::BeginTabBar("tabs", ImGuiTabBarFlags_FittingPolicyScroll)) {
        for (int k = 0; k < (int)body.tabs.size(); ++k) {
            const auto& t = body.tabs[k];
            if (t.name == "Macro modelling") continue;   // those are the MAIN sliders on the left
            if (ImGui::BeginTabItem(t.name.c_str(), nullptr, forcedTab == k ? ImGuiTabItemFlags_SetSelected : 0)) {
                tab = k;
                ImGui::BeginChild("sliders");
                tabPanel(t, rightW - 40);
                ImGui::EndChild();
                ImGui::EndTabItem();
            }
        }
        forcedTab = -1;
        ImGui::EndTabBar();
    }
    ImGui::End();

    // the body in the middle
    ImGui::SetNextWindowPos(ImVec2(leftW, top)); ImGui::SetNextWindowSize(ImVec2(W - leftW - rightW, H - top - statusH));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("view", nullptr, fixed);
    if (dirty && loadErr.empty()) { Human::apply(body); Viewer::update(body); dirty = false; }
    const ImVec2 a = ImGui::GetCursorScreenPos(), size = ImGui::GetContentRegionAvail();
    ImGui::InvisibleButton("v", size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);
    const unsigned tex = Viewer::render((int)size.x, (int)size.y, ImGui::IsItemHovered() || ImGui::IsItemActive());
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddImage((ImTextureID)(intptr_t)tex, a, ImVec2(a.x + size.x, a.y + size.y), ImVec2(0, 1), ImVec2(1, 0));
    // quick views along the top
    ImGui::SetCursorScreenPos(ImVec2(a.x + 10, a.y + 10));
    const char* views[] = {"Front", "Side", "Back", "Face", "Face side"};
    for (int k = 0; k < 5; ++k) { if (k) ImGui::SameLine(0, 4); if (ImGui::SmallButton(views[k])) Viewer::camera(k); }
    char info[128]; snprintf(info, sizeof info, "%.0f cm, %.0f years", Human::heightCm(body), Human::ageYears(body));
    dl->AddText(ImVec2(a.x + 12, a.y + size.y - 26), C_DIM, info);
    ImGui::End();
    ImGui::PopStyleVar();

    ImGui::SetNextWindowPos(ImVec2(0, H - statusH)); ImGui::SetNextWindowSize(ImVec2(W, statusH));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 4));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, col4(C_BG));
    ImGui::Begin("status", nullptr, fixed);
    if (now() < toastUntil) ImGui::TextColored(col4(C_PINK), "%s", toast.c_str());
    else ImGui::TextDisabled("Drag to turn, right-drag to move up and down, wheel to zoom. Right-click a slider to reset it.");
    ImGui::End();
    ImGui::PopStyleColor(); ImGui::PopStyleVar();
}

static void progressCb(float, const char*);
static std::atomic<float> loadFrac{0};
static std::string loadWhat = "Starting...";
static std::mutex loadMtx;
static void progressCb(float f, const char* what) { loadFrac = f; std::lock_guard<std::mutex> g(loadMtx); loadWhat = what; }

int main(int argc, char** argv) {
    std::string capture, file, describeWords;
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
    if (!file.empty()) { std::string err; if (!Human::loadMhm(body, file, err)) say(err); }
    if (!describeWords.empty()) { snprintf(prompt, sizeof prompt, "%s", describeWords.c_str()); understood = Human::describe(body, describeWords); }
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
