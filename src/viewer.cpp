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
static GLuint bodyVao = 0, bodyPos = 0, bodyNrm = 0, bodyIdx = 0; static int bodyCount = 0;
static GLuint eyeVao = 0, eyePos = 0, eyeNrm = 0, eyeUv = 0; static int eyeCount = 0;
static GLuint gridVao = 0, gridVbo = 0; static int gridCount = 0;
static GLuint lit[3] = {0, 0, 0}, litEye = 0, eyeTex = 0;
static GLuint fbo = 0, fboTex = 0, fboDepth = 0; static int fbW = 0, fbH = 0;
static float eth[3] = {1 / 3.0f, 1 / 3.0f, 1 / 3.0f};
static float target[3] = {0, 8.5f, 0}, height = 17, bodyMid = 8.5f;
static bool framed = false;

static GLuint texture(const std::string& path) {
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
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
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
uniform sampler2D litA, litB, litC, eyeTex; uniform vec3 eth; uniform int mode; uniform float light;
void main() {
  vec3 n = normalize(N); if (!gl_FrontFacing) n = -n;
  vec2 uv = n.xy * 0.495 + 0.5;                          // MakeHuman's litsphere lookup
  if (mode == 1) {                                       // the eyes: their picture; the cornea's clear parts let the iris show
    vec4 c = texture(eyeTex, T);
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
    glGenVertexArrays_(1, &bodyVao); glGenBuffers_(1, &bodyPos); glGenBuffers_(1, &bodyNrm); glGenBuffers_(1, &bodyIdx);
    glGenVertexArrays_(1, &eyeVao); glGenBuffers_(1, &eyePos); glGenBuffers_(1, &eyeNrm); glGenBuffers_(1, &eyeUv);
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

void update(const Human::Body& b) {
    eth[0] = b.african; eth[1] = b.asian; eth[2] = b.caucasian;
    // the body: indexed, the visible triangles only
    std::vector<float> n;
    normals(b.pos, b.tri, n);
    upload(bodyVao, bodyPos, 0, 3, b.pos);
    upload(bodyVao, bodyNrm, 1, 3, n);
    if (!bodyCount) {
        glBindBuffer_(GL_ELEMENT_ARRAY_BUFFER, bodyIdx);
        glBufferData_(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(b.tri.size() * 4), b.tri.data(), GL_STATIC_DRAW);
        bodyCount = (int)b.tri.size();
    }
    // the eyes, fitted to the body as it is now (corners unrolled: their uvs are per corner)
    if (!b.proxies.empty()) {
        const Human::Proxy& p = b.proxies[0];
        std::vector<float> fit; Human::fitProxy(b, p, fit);
        std::vector<int> idx(p.tri.begin(), p.tri.end());
        std::vector<float> pn; normals(fit, idx, pn);
        std::vector<float> P, N;
        for (int v : p.tri) { P.insert(P.end(), &fit[(size_t)v * 3], &fit[(size_t)v * 3] + 3); N.insert(N.end(), &pn[(size_t)v * 3], &pn[(size_t)v * 3] + 3); }
        upload(eyeVao, eyePos, 0, 3, P); upload(eyeVao, eyeNrm, 1, 3, N); upload(eyeVao, eyeUv, 2, 2, p.uv);
        eyeCount = (int)p.tri.size();
        if (!eyeTex && !p.texture.empty()) eyeTex = texture(p.texture);
    }
    // the height (for framing): MakeHuman's units are decimetres
    float lo = 1e30f, hi = -1e30f;
    for (int v : b.tri) { lo = std::min(lo, b.pos[(size_t)v * 3 + 1]); hi = std::max(hi, b.pos[(size_t)v * 3 + 1]); }
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
    for (int k = 0; k < 3; ++k) { glActiveTexture_(GL_TEXTURE0 + k); glBindTexture(GL_TEXTURE_2D, lit[k]); }
    glActiveTexture_(GL_TEXTURE0 + 3); glBindTexture(GL_TEXTURE_2D, eyeTex);
    glUniform1i_(glGetUniformLocation_(prog, "litA"), 0); glUniform1i_(glGetUniformLocation_(prog, "litB"), 1);
    glUniform1i_(glGetUniformLocation_(prog, "litC"), 2); glUniform1i_(glGetUniformLocation_(prog, "eyeTex"), 3);
    glUniform1i_(glGetUniformLocation_(prog, "mode"), lk.shading == 1 ? 2 : 0);
    glBindVertexArray_(bodyVao);
    glBindBuffer_(GL_ELEMENT_ARRAY_BUFFER, bodyIdx);
    glDrawElements(GL_TRIANGLES, bodyCount, GL_UNSIGNED_INT, nullptr);
    if (lk.wire) {
        glUseProgram_(progLine);
        glUniformMatrix4fv_(glGetUniformLocation_(progLine, "mvp"), 1, GL_FALSE, mvp);
        glUniform3f_(glGetUniformLocation_(progLine, "col"), 0.88f, 0.25f, 0.54f);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); glEnable(GL_POLYGON_OFFSET_LINE); glPolygonOffset(-1, -1);
        glDrawElements(GL_TRIANGLES, bodyCount, GL_UNSIGNED_INT, nullptr);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); glDisable(GL_POLYGON_OFFSET_LINE);
        glUseProgram_(prog);
    }
    if (eyeCount) {
        glUniform1i_(glGetUniformLocation_(prog, "mode"), 1);
        glActiveTexture_(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, litEye);
        glBindVertexArray_(eyeVao);
        glDrawArrays(GL_TRIANGLES, 0, eyeCount);
    }
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
