// Pygmalion - the 3D view (see viewer.h). MakeHuman's litsphere shading: the normal in view space picks a colour from
// a lit sphere picture; the three skin tones' spheres are blended by the ethnicity sliders. OpenGL 3.3 core.
#include "viewer.h"

#include <SDL.h>
#include <SDL_opengl.h>
#include <imgui.h>
#include <stb_image.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <map>
#include <vector>

namespace fs = std::filesystem;

#ifndef APIENTRY
#define APIENTRY
#endif
typedef char GLchar;
typedef ptrdiff_t GLsizeiptr;
#define GLF(ret, name, args) typedef ret(APIENTRY* PFN_##name) args; static PFN_##name name##_ = nullptr;
GLF(GLuint, glCreateShader, (GLenum))
GLF(void, glShaderSource, (GLuint, GLsizei, const GLchar* const*, const GLint*))
GLF(void, glCompileShader, (GLuint))
GLF(GLuint, glCreateProgram, (void))
GLF(void, glAttachShader, (GLuint, GLuint))
GLF(void, glLinkProgram, (GLuint))
GLF(void, glUseProgram, (GLuint))
GLF(GLint, glGetUniformLocation, (GLuint, const GLchar*))
GLF(void, glUniformMatrix4fv, (GLint, GLsizei, GLboolean, const GLfloat*))
GLF(void, glUniform1i, (GLint, GLint))
GLF(void, glUniform1f, (GLint, GLfloat))
GLF(void, glUniform3f, (GLint, GLfloat, GLfloat, GLfloat))
GLF(void, glGenVertexArrays, (GLsizei, GLuint*))
GLF(void, glBindVertexArray, (GLuint))
GLF(void, glGenBuffers, (GLsizei, GLuint*))
GLF(void, glBindBuffer, (GLenum, GLuint))
GLF(void, glBufferData, (GLenum, GLsizeiptr, const void*, GLenum))
GLF(void, glEnableVertexAttribArray, (GLuint))
GLF(void, glVertexAttribPointer, (GLuint, GLint, GLenum, GLboolean, GLsizei, const void*))
GLF(void, glGenFramebuffers, (GLsizei, GLuint*))
GLF(void, glBindFramebuffer, (GLenum, GLuint))
GLF(void, glFramebufferTexture2D, (GLenum, GLenum, GLenum, GLuint, GLint))
GLF(void, glGenRenderbuffers, (GLsizei, GLuint*))
GLF(void, glBindRenderbuffer, (GLenum, GLuint))
GLF(void, glRenderbufferStorage, (GLenum, GLenum, GLsizei, GLsizei))
GLF(void, glFramebufferRenderbuffer, (GLenum, GLenum, GLenum, GLuint))
GLF(void, glGenerateMipmap, (GLenum))
GLF(void, glActiveTexture, (GLenum))
#undef GLF
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER 0x8B31
#define GL_ARRAY_BUFFER 0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_STATIC_DRAW 0x88E4
#define GL_DYNAMIC_DRAW 0x88E8
#define GL_FRAMEBUFFER 0x8D40
#define GL_RENDERBUFFER 0x8D41
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_DEPTH_ATTACHMENT 0x8D00
#define GL_DEPTH_COMPONENT24 0x81A6
#define GL_TEXTURE0 0x84C0
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif
#ifndef GL_LINEAR_MIPMAP_LINEAR
#define GL_LINEAR_MIPMAP_LINEAR 0x2703
#endif

namespace Viewer {

static Look lk;
Look& look() { return lk; }
static GLuint prog = 0, progLine = 0;
// a mesh drawn corner by corner (uvs are per corner): the body, and each proxy (eyes, clothes, hair...)
struct Part { GLuint vao = 0, pos = 0, nrm = 0, uv = 0, tex = 0; int count = 0, mode = 3; bool cutout = false; };
static Part bodyPart;
static std::vector<Part> parts;
static std::map<std::string, GLuint> texCache;
static GLuint gridVao = 0, gridVbo = 0; static int gridCount = 0;
static GLuint lit[3] = {0, 0, 0}, litEye = 0;
static GLuint fbo = 0, fboTex = 0, fboDepth = 0; static int fbW = 0, fbH = 0;
static float eth[3] = {1 / 3.0f, 1 / 3.0f, 1 / 3.0f};
static float target[3] = {0, 8.5f, 0}, height = 17, bodyMid = 8.5f;
static bool framed = false;

static GLuint textureLoad(const std::string& path);
static GLuint texture(const std::string& path) {
    if (path.empty()) return 0;
    auto it = texCache.find(path);
    if (it != texCache.end()) return it->second;
    return texCache[path] = textureLoad(path);
}
static GLuint textureLoad(const std::string& path) {
    int w, h, n;
    stbi_set_flip_vertically_on_load(1);
    unsigned char* px = stbi_load(path.c_str(), &w, &h, &n, 4);
    if (!px) return 0;
    GLuint t; glGenTextures(1, &t); glBindTexture(GL_TEXTURE_2D, t);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    glGenerateMipmap_(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    stbi_image_free(px);
    return t;
}

bool init(const std::string& data) {
#define L(name) name##_ = (PFN_##name)SDL_GL_GetProcAddress(#name); if (!name##_) return false;
    L(glCreateShader) L(glShaderSource) L(glCompileShader) L(glCreateProgram) L(glAttachShader) L(glLinkProgram) L(glUseProgram)
    L(glGetUniformLocation) L(glUniformMatrix4fv) L(glUniform1i) L(glUniform1f) L(glUniform3f) L(glGenVertexArrays) L(glBindVertexArray)
    L(glGenBuffers) L(glBindBuffer) L(glBufferData) L(glEnableVertexAttribArray) L(glVertexAttribPointer) L(glGenFramebuffers)
    L(glBindFramebuffer) L(glFramebufferTexture2D) L(glGenRenderbuffers) L(glBindRenderbuffer) L(glRenderbufferStorage)
    L(glFramebufferRenderbuffer) L(glGenerateMipmap) L(glActiveTexture)
#undef L
    auto sh = [](GLenum t, const char* src) { GLuint s = glCreateShader_(t); glShaderSource_(s, 1, &src, nullptr); glCompileShader_(s); return s; };
    auto make = [&](const char* vs, const char* fs) { GLuint p = glCreateProgram_(); glAttachShader_(p, sh(GL_VERTEX_SHADER, vs)); glAttachShader_(p, sh(GL_FRAGMENT_SHADER, fs)); glLinkProgram_(p); return p; };
    prog = make(R"(#version 330
layout(location=0) in vec3 p; layout(location=1) in vec3 n; layout(location=2) in vec2 t;
uniform mat4 mvp, view; out vec3 N; out vec2 T;
void main() { N = mat3(view) * n; T = t; gl_Position = mvp * vec4(p, 1); })",
                R"(#version 330
in vec3 N; in vec2 T; out vec4 o;
uniform sampler2D litA, litB, litC, tex; uniform vec3 eth; uniform int mode, hasTex, cutout; uniform float light;
void main() {
  vec3 n = normalize(N); if (!gl_FrontFacing) n = -n;
  vec2 uv = n.xy * 0.495 + 0.5;                          // MakeHuman's litsphere lookup
  vec4 tc = hasTex == 1 ? texture(tex, T) : vec4(0.62, 0.62, 0.64, 1);
  if (cutout == 1 && tc.a < 0.4) discard;                // hair, eyebrows, lashes: their see-through parts
  float d = max(n.z, 0.0);
  if (mode == 3) { o = vec4(tc.rgb * (0.38 + 0.72 * d) + vec3(0.04) * pow(d, 24.0), 1); return; }
  if (mode == 4) { o = vec4(tc.rgb * (0.42 + 0.68 * d) * vec3(1.0, 0.97, 0.95), 1); return; }   // a skin texture
  if (mode == 1) {                                       // the eyes: their picture; the cornea's clear parts let the iris show
    vec4 c = tc;
    if (c.a < 0.35) discard;
    vec3 shine = texture(litA, uv).rgb;                  // MakeHuman's eye litsphere, added at half strength
    o = vec4(c.rgb * 0.8 + shine * 0.5 * 0.35, 1);
    return;
  }
  if (mode == 2) { float d = max(n.z, 0.0); o = vec4(vec3(0.78, 0.76, 0.74) * (0.25 + 0.8 * d), 1); return; }   // clay
  vec3 skin = texture(litA, uv).rgb * eth.x + texture(litB, uv).rgb * eth.y + texture(litC, uv).rgb * eth.z;
  o = vec4(mix(vec3(dot(skin, vec3(0.33))), skin, 1.0) * mix(1.0, 1.0, light), 1);
})");
    progLine = make(R"(#version 330
layout(location=0) in vec3 p; uniform mat4 mvp; void main() { gl_Position = mvp * vec4(p, 1); })",
                    R"(#version 330
out vec4 o; uniform vec3 col; void main() { o = vec4(col, 1); })");
    const fs::path ls = fs::u8path(data) / "litspheres";
    lit[0] = texture((ls / "skinmat_african.png").u8string());
    lit[1] = texture((ls / "skinmat_asian.png").u8string());
    lit[2] = texture((ls / "skinmat_caucasian.png").u8string());
    litEye = texture((ls / "skinmat_eye.png").u8string());

    glGenVertexArrays_(1, &gridVao); glGenBuffers_(1, &gridVbo);
    return prog != 0;
}

static void upload(GLuint vao, GLuint buf, int attr, int comps, const std::vector<float>& d) {
    glBindVertexArray_(vao);
    glBindBuffer_(GL_ARRAY_BUFFER, buf);
    glBufferData_(GL_ARRAY_BUFFER, (GLsizeiptr)(d.size() * 4), d.data(), GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray_(attr);
    glVertexAttribPointer_(attr, comps, GL_FLOAT, GL_FALSE, 0, nullptr);
}

static void normals(const std::vector<float>& pos, const std::vector<int>& tri, std::vector<float>& n) {
    n.assign(pos.size(), 0);
    for (size_t t = 0; t + 2 < tri.size(); t += 3) {
        const float* a = &pos[(size_t)tri[t] * 3]; const float* b = &pos[(size_t)tri[t + 1] * 3]; const float* c = &pos[(size_t)tri[t + 2] * 3];
        const float u[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]}, v[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
        const float f[3] = {u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]};
        for (int k = 0; k < 3; ++k) for (int q = 0; q < 3; ++q) n[(size_t)tri[t + k] * 3 + q] += f[q];
    }
    for (size_t i = 0; i < n.size(); i += 3) {
        const float l = std::sqrt(n[i] * n[i] + n[i + 1] * n[i + 1] + n[i + 2] * n[i + 2]);
        if (l > 0) for (int q = 0; q < 3; ++q) n[i + q] /= l;
    }
}

static void fill(Part& pt, const std::vector<float>& P, const std::vector<float>& N, const std::vector<float>& U) {
    if (!pt.vao) { glGenVertexArrays_(1, &pt.vao); glGenBuffers_(1, &pt.pos); glGenBuffers_(1, &pt.nrm); glGenBuffers_(1, &pt.uv); }
    upload(pt.vao, pt.pos, 0, 3, P); upload(pt.vao, pt.nrm, 1, 3, N); upload(pt.vao, pt.uv, 2, 2, U);
    pt.count = (int)P.size() / 3;
}

void update(const Human::Body& b) {
    eth[0] = b.african; eth[1] = b.asian; eth[2] = b.caucasian;
    const std::vector<float>& pos = b.shown.empty() ? b.pos : b.shown;
    // the body: its visible triangles, less those under clothes (all of them under a topology)
    {
        std::vector<float> n; normals(pos, b.tri, n);
        const std::vector<char> hid = Human::hiddenVerts(b);
        std::vector<float> P, N, U;
        P.reserve(b.tri.size() * 3); N.reserve(b.tri.size() * 3); U.reserve(b.tri.size() * 2);
        for (size_t t = 0; t + 2 < b.tri.size(); t += 3) {
            if (hid[b.tri[t]] || hid[b.tri[t + 1]] || hid[b.tri[t + 2]]) continue;
            for (size_t c = t; c < t + 3; ++c) {
                const size_t v = (size_t)b.tri[c]; const int u = b.triUv[c];
                P.insert(P.end(), &pos[v * 3], &pos[v * 3] + 3); N.insert(N.end(), &n[v * 3], &n[v * 3] + 3);
                if (u >= 0 && (size_t)u * 2 + 1 < b.uv.size()) U.insert(U.end(), {b.uv[(size_t)u * 2], b.uv[(size_t)u * 2 + 1]}); else U.insert(U.end(), {0, 0});
            }
        }
        fill(bodyPart, P, N, U);
        bodyPart.tex = texture(b.skinTexture);
        bodyPart.mode = bodyPart.tex ? 4 : 0;
    }
    // the proxies, fitted to the body as it is now, in MakeHuman's z_depth order
    parts.resize(std::max(parts.size(), b.proxies.size()));
    for (size_t i = 0; i < parts.size(); ++i) {
        Part& pt = parts[i];
        if (i >= b.proxies.size()) { pt.count = 0; continue; }
        const Human::Proxy& p = b.proxies[i];
        std::vector<float> fit; Human::fitProxy(b, p, fit);
        std::vector<float> pn; normals(fit, p.tri, pn);
        std::vector<float> P, N;
        P.reserve(p.tri.size() * 3); N.reserve(p.tri.size() * 3);
        for (int v : p.tri) { P.insert(P.end(), &fit[(size_t)v * 3], &fit[(size_t)v * 3] + 3); N.insert(N.end(), &pn[(size_t)v * 3], &pn[(size_t)v * 3] + 3); }
        fill(pt, P, N, p.uv);
        const bool topology = p.kind == "proxymeshes";
        pt.tex = texture(topology ? b.skinTexture : p.texture);
        pt.mode = p.kind == "eyes" ? 1 : topology ? (pt.tex ? 4 : 0) : 3;
        pt.cutout = p.transparent || p.kind == "hair" || p.kind == "eyebrows" || p.kind == "eyelashes";
    }
    // the height (for framing): MakeHuman's units are decimetres
    float lo = 1e30f, hi = -1e30f;
    for (int v : b.tri) { lo = std::min(lo, pos[(size_t)v * 3 + 1]); hi = std::max(hi, pos[(size_t)v * 3 + 1]); }
    height = hi - lo; bodyMid = (hi + lo) / 2;
    if (!framed) { target[1] = bodyMid; lk.dist = height * 1.7f; framed = true; }
    glBindVertexArray_(0);
}

void focus(const float at[3], float dist, float yaw, float pitch) {
    for (int c = 0; c < 3; ++c) target[c] = at[c];
    lk.dist = dist; lk.yaw = yaw; lk.pitch = pitch;
}
void focusBody(float yaw, float pitch) {
    target[0] = 0; target[1] = bodyMid; target[2] = 0;
    lk.dist = height * 1.7f; lk.yaw = yaw; lk.pitch = pitch;
}
float bodyHeight() { return height; }

static void mul(const float* a, const float* b, float* o) {
    float r[16];
    for (int c = 0; c < 4; ++c) for (int rr = 0; rr < 4; ++rr) { float s = 0; for (int k = 0; k < 4; ++k) s += a[k * 4 + rr] * b[c * 4 + k]; r[c * 4 + rr] = s; }
    memcpy(o, r, sizeof r);
}

unsigned render(int w, int h, bool hovered) {
    if (w < 8 || h < 8) return fboTex;
    ImGuiIO& io = ImGui::GetIO();
    if (hovered) {
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) { lk.yaw -= io.MouseDelta.x * 0.008f; lk.pitch = std::clamp(lk.pitch + io.MouseDelta.y * 0.006f, -1.4f, 1.4f); }
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Right) || ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
            const float k = lk.dist * 0.0015f, cy = std::cos(lk.yaw), sy = std::sin(lk.yaw);
            target[0] -= io.MouseDelta.x * k * cy; target[2] += io.MouseDelta.x * k * sy; target[1] += io.MouseDelta.y * k;
        }
        if (io.MouseWheel != 0) lk.dist = std::clamp(lk.dist * std::pow(0.88f, io.MouseWheel), height * 0.12f, height * 6);
    }
    const float ty = target[1];
    if (w != fbW || h != fbH) {
        if (!fbo) { glGenFramebuffers_(1, &fbo); glGenTextures(1, &fboTex); glGenRenderbuffers_(1, &fboDepth); }
        glBindTexture(GL_TEXTURE_2D, fboTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindRenderbuffer_(GL_RENDERBUFFER, fboDepth);
        glRenderbufferStorage_(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);
        glBindFramebuffer_(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D_(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboTex, 0);
        glFramebufferRenderbuffer_(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, fboDepth);
        fbW = w; fbH = h;
    }
    glBindFramebuffer_(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, w, h);
    glClearColor(0x14 / 255.0f, 0x14 / 255.0f, 0x14 / 255.0f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    // the view: orbit round the body's middle
    const float cp = std::cos(lk.pitch), sp = std::sin(lk.pitch), cy = std::cos(lk.yaw), sy = std::sin(lk.yaw);
    const float e[3] = {target[0] + lk.dist * cp * sy, ty + lk.dist * sp, target[2] + lk.dist * cp * cy};
    float f[3] = {target[0] - e[0], ty - e[1], target[2] - e[2]};
    const float fl = std::sqrt(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]); for (float& v : f) v /= fl;
    float s[3] = {-f[2], 0, f[0]}; const float sl = std::sqrt(s[0] * s[0] + s[2] * s[2]); s[0] /= sl; s[2] /= sl;
    const float u[3] = {s[1] * f[2] - s[2] * f[1], s[2] * f[0] - s[0] * f[2], s[0] * f[1] - s[1] * f[0]};
    const float V[16] = {s[0], u[0], -f[0], 0, s[1], u[1], -f[1], 0, s[2], u[2], -f[2], 0,
                         -(s[0] * e[0] + s[1] * e[1] + s[2] * e[2]), -(u[0] * e[0] + u[1] * e[1] + u[2] * e[2]), f[0] * e[0] + f[1] * e[1] + f[2] * e[2], 1};
    const float n = 0.05f, fr = 400, t = 1.0f / std::tan(0.38f), a = (float)w / h;
    const float P[16] = {t / a, 0, 0, 0, 0, t, 0, 0, 0, 0, -(fr + n) / (fr - n), -1, 0, 0, -2 * fr * n / (fr - n), 0};
    float mvp[16]; mul(P, V, mvp);
    // the floor
    if (lk.grid) {
        std::vector<float> g;
        const float y = bodyMid - height / 2;
        for (int i = -10; i <= 10; ++i) g.insert(g.end(), {i * 1.0f, y, -10, i * 1.0f, y, 10, -10, y, i * 1.0f, 10, y, i * 1.0f});
        glBindVertexArray_(gridVao); glBindBuffer_(GL_ARRAY_BUFFER, gridVbo);
        glBufferData_(GL_ARRAY_BUFFER, (GLsizeiptr)(g.size() * 4), g.data(), GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray_(0); glVertexAttribPointer_(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
        gridCount = (int)g.size() / 3;
        glUseProgram_(progLine);
        glUniformMatrix4fv_(glGetUniformLocation_(progLine, "mvp"), 1, GL_FALSE, mvp);
        glUniform3f_(glGetUniformLocation_(progLine, "col"), 0.23f, 0.23f, 0.23f);
        glDrawArrays(GL_LINES, 0, gridCount);
    }
    glUseProgram_(prog);
    glUniformMatrix4fv_(glGetUniformLocation_(prog, "mvp"), 1, GL_FALSE, mvp);
    glUniformMatrix4fv_(glGetUniformLocation_(prog, "view"), 1, GL_FALSE, V);
    glUniform3f_(glGetUniformLocation_(prog, "eth"), eth[0], eth[1], eth[2]);
    glUniform1f_(glGetUniformLocation_(prog, "light"), lk.light);
    glUniform1i_(glGetUniformLocation_(prog, "litA"), 0); glUniform1i_(glGetUniformLocation_(prog, "litB"), 1);
    glUniform1i_(glGetUniformLocation_(prog, "litC"), 2); glUniform1i_(glGetUniformLocation_(prog, "tex"), 3);
    const GLint uMode = glGetUniformLocation_(prog, "mode"), uHas = glGetUniformLocation_(prog, "hasTex"), uCut = glGetUniformLocation_(prog, "cutout");
    auto draw = [&](const Part& pt, bool lines) {
        if (!pt.count) return;
        for (int k = 0; k < 3; ++k) { glActiveTexture_(GL_TEXTURE0 + k); glBindTexture(GL_TEXTURE_2D, pt.mode == 1 && k == 0 ? litEye : lit[k]); }
        glActiveTexture_(GL_TEXTURE0 + 3); glBindTexture(GL_TEXTURE_2D, pt.tex);
        const bool clay = lk.shading == 1 && pt.mode != 1;
        glUniform1i_(uMode, clay ? 2 : pt.mode);
        glUniform1i_(uHas, pt.tex ? 1 : 0);
        glUniform1i_(uCut, pt.cutout && pt.tex ? 1 : 0);
        glBindVertexArray_(pt.vao);
        glDrawArrays(GL_TRIANGLES, 0, pt.count);
        if (lines) {
            glUseProgram_(progLine);
            glUniformMatrix4fv_(glGetUniformLocation_(progLine, "mvp"), 1, GL_FALSE, mvp);
            glUniform3f_(glGetUniformLocation_(progLine, "col"), 0.88f, 0.25f, 0.54f);
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); glEnable(GL_POLYGON_OFFSET_LINE); glPolygonOffset(-1, -1);
            glDrawArrays(GL_TRIANGLES, 0, pt.count);
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); glDisable(GL_POLYGON_OFFSET_LINE);
            glUseProgram_(prog);
        }
    };
    draw(bodyPart, lk.wire);
    for (const Part& pt : parts) draw(pt, lk.wire && pt.mode != 1 && !pt.cutout);
    glBindVertexArray_(0); glUseProgram_(0);
    glDisable(GL_DEPTH_TEST);
    glBindFramebuffer_(GL_FRAMEBUFFER, 0);
    glActiveTexture_(GL_TEXTURE0);
    return fboTex;
}

void readPixels(std::vector<unsigned char>& rgba, int& w, int& h) {
    w = fbW; h = fbH;
    std::vector<unsigned char> raw((size_t)w * h * 4);
    rgba.resize(raw.size());
    glBindFramebuffer_(GL_FRAMEBUFFER, fbo);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, raw.data());
    glBindFramebuffer_(GL_FRAMEBUFFER, 0);
    for (int y = 0; y < h; ++y) memcpy(&rgba[(size_t)y * w * 4], &raw[(size_t)(h - 1 - y) * w * 4], (size_t)w * 4);
}

}
