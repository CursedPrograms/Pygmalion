// Pygmalion - the 3D view (see viewer.h). MakeHuman's litsphere shading: the normal in view space picks a colour from
// a lit sphere picture; the three skin tones' spheres are blended by the ethnicity sliders. OpenGL 3.3 core.
#include "profile.h"
#include "viewer.h"
#include "material.h"

#include <SDL.h>
#include <SDL_opengl.h>
#include <imgui.h>
#include <stb_image.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <map>
#include <unordered_map>
#include <array>
#include <cstdint>
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
GLF(void, glUniform4f, (GLint, GLfloat, GLfloat, GLfloat, GLfloat))
GLF(void, glUniform3fv, (GLint, GLsizei, const GLfloat*))
GLF(void, glUniform2f, (GLint, GLfloat, GLfloat))
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
static GLuint prog = 0, progLine = 0, progMh = 0;
static float mhPos[24], mhCol[24], mhSpec[24]; static int mhN = 0;   // the scene's lights as MakeHuman's shaders take them   // progMh: MakeHuman's own shaders, by each object's material
// a mesh drawn corner by corner (uvs are per corner): the body, and each proxy (eyes, clothes, hair...)
// what MakeHuman's shader needs of a material: its shader and defines, colours, textures and parameters
struct MatGL {
    int sh = 1; bool blend = false, wire = false; bool dDiffuse = false, dNormal = false, dAo = false, dAlpha = false, autoSkin = false, shadeless = false, backfaceCull = true, depthless = false, xray = false;
    GLuint diffuse = 0, litsphere = 0, normal = 0, ao = 0, alpha = 0;
    float diff[4] = {1, 1, 1, 1}, spec[4] = {1, 1, 1, 0.2f}, amb[3] = {1, 1, 1}, emis[3] = {0, 0, 0};
    float additive = 0, normalInt = 1, silhouette = 0.2f, toonShine = 20, ambI = 0.4f, specI = 0.2f, diffI = 0.4f, specTh = 0.3f, diffTh = 0.5f, edge = 1, xrayI = 0.7f;
};
struct Part { GLuint vao = 0, pos = 0, nrm = 0, uv = 0, tg = 0, tex = 0; int count = 0, mode = 3; bool cutout = false; float tint[3] = {1, 1, 1}; MatGL mat; };
static Part bodyPart;
static std::vector<Part> parts;
static std::map<std::string, GLuint> texCache;
static GLuint gridVao = 0, gridVbo = 0; static int gridCount = 0;
static GLuint progBg = 0, bgVao = 0, bgTex = 0; static std::string bgLoaded;
static std::vector<float> boneSegs;
static GLuint lit[3] = {0, 0, 0}, litEye = 0, lit_standard = 0;
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
    L(glGetUniformLocation) L(glUniformMatrix4fv) L(glUniform1i) L(glUniform1f) L(glUniform3f) L(glUniform4f) L(glUniform3fv) L(glUniform2f) L(glGenVertexArrays) L(glBindVertexArray)
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
uniform sampler2D litA, litB, litC, tex; uniform vec3 eth; uniform int mode, hasTex, cutout; uniform float light; uniform vec3 lcol, tint;
uniform int nl, sceneSkin; uniform vec3 ldir[8], lrgb[8], lspec[8], amb; uniform float shine, shininess;
vec3 lit(vec3 n, vec3 c) {                               // the scene's lights on a colour (view space: the eye looks down -z)
  vec3 d = amb, sp = vec3(0);
  for (int i = 0; i < nl; ++i) {
    float k = max(dot(n, ldir[i]), 0.0);
    d += lrgb[i] * k;
    sp += lspec[i] * pow(max(dot(n, normalize(ldir[i] + vec3(0, 0, 1))), 0.0), shininess) * step(0.0001, k);
  }
  return c * d + sp * shine;
}
void shade();
void main() { shade(); o.rgb *= light * lcol * tint; }
void shade() {
  vec3 n = normalize(N); if (!gl_FrontFacing) n = -n;
  vec2 uv = n.xy * 0.495 + 0.5;                          // MakeHuman's litsphere lookup
  vec4 tc = hasTex == 1 ? texture(tex, T) : vec4(0.62, 0.62, 0.64, 1);
  if (cutout == 1 && tc.a < 0.4) discard;                // hair, eyebrows, lashes: their see-through parts
  float d = max(n.z, 0.0);
  if (mode == 3) { o = vec4(lit(n, tc.rgb), 1); return; }
  if (mode == 4) { o = vec4(lit(n, tc.rgb * 1.05) * vec3(1.0, 0.97, 0.95), 1); return; }   // a skin texture
  if (mode == 1) {                                       // the eyes: their picture; the cornea's clear parts let the iris show
    vec4 c = tc;
    if (c.a < 0.35) discard;
    vec3 shine = texture(litA, uv).rgb;                  // MakeHuman's eye litsphere, added at half strength
    o = vec4(c.rgb * 0.8 + shine * 0.5 * 0.35, 1);
    return;
  }
  if (mode == 2) { o = vec4(lit(n, vec3(0.78, 0.76, 0.74)), 1); return; }   // clay
  vec3 skin = texture(litA, uv).rgb * eth.x + texture(litB, uv).rgb * eth.y + texture(litC, uv).rgb * eth.z;
  if (sceneSkin == 1) {   // the litsphere's colour, lit by the scene instead of its painted light
    vec3 base = vec3(0);
    for (int k = 0; k < 9; ++k) {                          // the sphere's average colour: the skin without its painted light
      vec2 q = vec2(0.5) + 0.3 * vec2(k % 3 - 1, k / 3 - 1);
      base += texture(litA, q).rgb * eth.x + texture(litB, q).rgb * eth.y + texture(litC, q).rgb * eth.z;
    }
    o = vec4(lit(n, base / 9.0 * 1.25), 1); return;
  }
  o = vec4(skin, 1);
})");
    progMh = make(R"(#version 330
layout(location=0) in vec3 p; layout(location=1) in vec3 n; layout(location=2) in vec2 t; layout(location=3) in vec3 tg;
uniform mat4 mvp, view; out vec3 vN, vV, vTg; out vec2 vT;
void main() { vN = mat3(view) * n; vTg = mat3(view) * tg; vV = vec3(view * vec4(p, 1)); vT = t; gl_Position = mvp * vec4(p, 1); })",
                  R"(#version 330
in vec3 vN, vV, vTg; in vec2 vT; out vec4 o;
uniform int sh;                 // 0 none (fixed function), 1 litsphere, 2 normalmap, 3 phong, 4 skin, 5 toon, 6 xray
uniform int dDiffuse, dNormal, dAo, dAlpha, cutout, autoSkin, shadeless, blend;
uniform sampler2D diffuseTexture, litsphereTexture, normalmapTexture, aomapTexture, alphaTexture, litA, litB, litC;
uniform vec3 eth;
uniform vec4 mDiffuse;          // rgb, opacity
uniform vec4 mSpecular;         // rgb, shininess 0..1
uniform vec3 mAmbient, mEmissive;
uniform float AdditiveShading, normalmapIntensity;
uniform float silhouetteThreshold, toonShininess, ambientIntensity, specularIntensity, diffuseIntensity, specular_th, diffuse_th;
uniform float edgefalloff, xrayIntensity;
uniform vec3 sceneAmb, l0pos, l0col, l0spec;
uniform int nl; uniform vec3 lpos[8], lrgb[8], lspec[8];
uniform float light; uniform vec3 lcol, tint;
vec3 viewNormal() {
  vec3 n = normalize(vN); if (!gl_FrontFacing) n = -n;
  if (dNormal == 1) {             // the normal map, tangent to view space
    vec3 t = normalize(vTg - n * dot(n, vTg)); vec3 b = normalize(cross(n, t));
    vec3 m = (2.0 * texture(normalmapTexture, vT).rgb - 1.0) * normalmapIntensity;
    n = normalize(mat3(t, b, n) * m);
  }
  return n;
}
vec4 shadeIt() {
  vec4 tex = dDiffuse == 1 ? texture(diffuseTexture, vT) : vec4(1.0);
  if (shadeless == 1) return vec4(tex.rgb * mDiffuse.rgb, tex.a * mDiffuse.a);
  if (sh == 1) {                 // litsphere
    vec3 n = viewNormal();
    vec2 q = n.xy * 0.495 + 0.5;
    vec3 shading = autoSkin == 1 ? texture(litA, q).rgb * eth.x + texture(litB, q).rgb * eth.y + texture(litC, q).rgb * eth.z : texture(litsphereTexture, q).rgb;
    vec4 c;
    if (dDiffuse == 1) {
      c.rgb = (1.0 - AdditiveShading) * shading * tex.rgb * vec3(2.0 - (shading.r + shading.g + shading.b) / 3.0);
      c.rgb += AdditiveShading * (shading + tex.rgb);
      c.a = tex.a;
    } else { c.rgb = shading; c.a = dAlpha == 1 ? texture(alphaTexture, vT).a : 1.0; }
    if (dAo == 1) c.rgb *= texture(aomapTexture, vT).rgb;
    return c;
  }
  vec3 n = viewNormal();
  vec3 L = normalize(l0pos - vV), E = normalize(-vV);
  vec3 ambientCol = mEmissive + mAmbient * sceneAmb;          // gl_FrontLightModelProduct.sceneColor (the light's own ambient is 0)
  if (sh == 3 || sh == 4) {      // phong; skin (its gradient and AO maps as white)
    if (dAo == 1) tex.rgb *= texture(aomapTexture, vT).rgb;
    vec4 c = vec4(ambientCol * tex.rgb, tex.a);
    float NdotL = max(dot(n, L), 0.0);
    if (NdotL > 0.0) {
      c.rgb += (sh == 3 ? l0col * mDiffuse.rgb : vec3(1.0)) * NdotL * tex.rgb;
      vec3 H = normalize(normalize(l0pos) + vec3(0, 0, 1));
      c.rgb += l0spec * mSpecular.rgb * pow(max(dot(n, H), 0.0), max(mSpecular.a * (sh == 3 ? 255.0 : 128.0), 1.0));
    }
    if (sh == 3) c.rgb += mEmissive;
    return c;
  }
  if (sh == 2) {                 // normalmap: lit in tangent space (its ambient is the light's, which is 0)
    vec3 nn = normalize(vN); if (!gl_FrontFacing) nn = -nn;
    vec3 t = normalize(vTg - nn * dot(nn, vTg)); vec3 b = cross(nn, t);
    vec3 lv = l0pos - vV, ev = -vV;
    vec3 lVec = normalize(vec3(dot(lv, t), dot(lv, b), dot(lv, nn))), vVec = normalize(vec3(dot(ev, t), dot(ev, b), dot(ev, nn)));
    vec4 base = texture(diffuseTexture, vT);
    vec3 bump = normalize(texture(normalmapTexture, vT).xyz * 2.0 - 1.0);
    float d = max(dot(lVec, bump), 0.0);
    float s = pow(clamp(dot(reflect(-lVec, bump), vVec), 0.0, 1.0), max(mSpecular.a * 128.0, 1.0));
    return vec4(l0col * mDiffuse.rgb * d * base.rgb + l0spec * mSpecular.rgb * s, 1.0);
  }
  if (sh == 5) {                 // toon
    tex = texture(diffuseTexture, vT);
    float sil = max(dot(n, E), 0.0);
    if (sil < silhouetteThreshold) return vec4(0.0, 0.0, 0.0, tex.a);
    vec3 H = normalize(L + E);
    float m = ambientIntensity;
    m += specularIntensity * smoothstep(specular_th - 0.01, specular_th, pow(max(dot(n, H), 0.0), toonShininess));
    m += diffuseIntensity * smoothstep(diffuse_th - 0.01, diffuse_th, max(dot(n, L), 0.0));
    return vec4(m * tex.rgb, tex.a);
  }
  if (sh == 6) {                 // xray: see-through at the middle, bright at the edges
    float opac = abs(dot(normalize(-vN), normalize(-vV)));
    opac = (mAmbient.r + mAmbient.g + mAmbient.b) / 3.0 + xrayIntensity * (1.0 - pow(opac, edgefalloff));
    vec4 dc = vec4(1.0); if (dDiffuse == 1) dc += texture(diffuseTexture, vT);
    return vec4(opac * dc.rgb, opac);
  }
  // [None]: OpenGL's fixed-function lighting, every light, the texture multiplied in
  vec3 c = ambientCol;
  for (int i = 0; i < nl; ++i) {
    vec3 Li = normalize(lpos[i] - vV);
    float k = max(dot(n, Li), 0.0);
    c += lrgb[i] * mDiffuse.rgb * k;
    if (k > 0.0) c += lspec[i] * mSpecular.rgb * pow(max(dot(n, normalize(Li + E)), 0.0), max(mSpecular.a * 128.0, 1.0));
  }
  return vec4(c * tex.rgb, mDiffuse.a * tex.a);
}
void main() {
  o = shadeIt();
  if (cutout == 1 && o.a < 0.4) discard;
  if (blend == 0) o.a = 1.0; else if (sh != 6) o.a *= mDiffuse.a;
  o.rgb *= light * lcol * tint;
})");
    progBg = make(R"(#version 330
out vec2 T; void main() { vec2 p = vec2((gl_VertexID & 1) * 2 - 1, (gl_VertexID >> 1) * 2 - 1); T = p * 0.5 + 0.5; gl_Position = vec4(p, 0.999, 1); })",
                  R"(#version 330
in vec2 T; out vec4 o; uniform sampler2D img; uniform float alpha; uniform vec2 fit; uniform vec3 bg;
void main() { vec2 t = (T - 0.5) / fit + 0.5; if (t.x < 0.0 || t.x > 1.0 || t.y < 0.0 || t.y > 1.0) { o = vec4(bg, 1); return; } o = vec4(mix(bg, texture(img, t).rgb, alpha), 1); })");
    glGenVertexArrays_(1, &bgVao);
    progLine = make(R"(#version 330
layout(location=0) in vec3 p; uniform mat4 mvp; void main() { gl_Position = mvp * vec4(p, 1); })",
                    R"(#version 330
out vec4 o; uniform vec3 col; void main() { o = vec4(col, 1); })");
    const fs::path ls = fs::u8path(data) / "litspheres";
    lit[0] = texture((ls / "skinmat_african.png").u8string());
    lit[1] = texture((ls / "skinmat_asian.png").u8string());
    lit[2] = texture((ls / "skinmat_caucasian.png").u8string());
    litEye = texture((ls / "skinmat_eye.png").u8string());
    lit_standard = texture((ls / "lit_standard_skin.png").u8string());
    if (!lit_standard) lit_standard = lit[2];

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

// MakeHuman's Smooth (catmull_clark_subdivision.py), on triangles: each becomes three quads (six triangles). The
// positions are smoothed; the uvs are split linearly per face, so texture seams stay where they are.
static void subdivide(const std::vector<float>& pos, const std::vector<int>& tri, const std::vector<float>& cuv,
                      std::vector<float>& opos, std::vector<int>& otri, std::vector<float>& ouv) {
    const size_t nv = pos.size() / 3, nf = tri.size() / 3;
    std::unordered_map<uint64_t, int> edgeOf;
    std::vector<std::array<int, 2>> ev; std::vector<std::array<int, 2>> ef;   // edge's vertices, faces (-1: none)
    std::vector<std::array<int, 3>> fe(nf);
    for (size_t f = 0; f < nf; ++f)
        for (int k = 0; k < 3; ++k) {
            const int a = tri[f * 3 + k], b = tri[f * 3 + (k + 1) % 3];
            const uint64_t key = ((uint64_t)std::min(a, b) << 32) | (uint32_t)std::max(a, b);
            auto it = edgeOf.find(key);
            if (it == edgeOf.end()) { it = edgeOf.emplace(key, (int)ev.size()).first; ev.push_back({a, b}); ef.push_back({(int)f, -1}); }
            else ef[it->second][1] = (int)f;
            fe[f][k] = it->second;
        }
    const size_t ne = ev.size();
    std::vector<float> F(nf * 3), E(ne * 3);
    for (size_t f = 0; f < nf; ++f) for (int c = 0; c < 3; ++c) F[f * 3 + c] = (pos[(size_t)tri[f * 3] * 3 + c] + pos[(size_t)tri[f * 3 + 1] * 3 + c] + pos[(size_t)tri[f * 3 + 2] * 3 + c]) / 3;
    std::vector<char> boundary(nv, 0);
    for (size_t e = 0; e < ne; ++e) {
        const int a = ev[e][0], b = ev[e][1];
        for (int c = 0; c < 3; ++c) {
            const float m = (pos[(size_t)a * 3 + c] + pos[(size_t)b * 3 + c]) / 2;
            E[e * 3 + c] = ef[e][1] < 0 ? m : (2 * m + F[(size_t)ef[e][0] * 3 + c] + F[(size_t)ef[e][1] * 3 + c]) / 4;
        }
        if (ef[e][1] < 0) boundary[a] = boundary[b] = 1;
    }
    // the old vertices move: (F + 2R + (n - 3)P) / n inside; on an edge of a hole, 3/4 P + 1/8 of its two edge neighbours
    std::vector<float> Fs(nv * 3, 0), Rs(nv * 3, 0), Bs(nv * 3, 0);
    std::vector<int> nF(nv, 0), nR(nv, 0), nB(nv, 0);
    for (size_t f = 0; f < nf; ++f) for (int k = 0; k < 3; ++k) { const int v = tri[f * 3 + k]; for (int c = 0; c < 3; ++c) Fs[(size_t)v * 3 + c] += F[f * 3 + c]; ++nF[v]; }
    for (size_t e = 0; e < ne; ++e) {
        const int a = ev[e][0], b = ev[e][1];
        for (int c = 0; c < 3; ++c) { const float m = (pos[(size_t)a * 3 + c] + pos[(size_t)b * 3 + c]) / 2; Rs[(size_t)a * 3 + c] += m; Rs[(size_t)b * 3 + c] += m; }
        ++nR[a]; ++nR[b];
        if (ef[e][1] < 0) { for (int c = 0; c < 3; ++c) { Bs[(size_t)a * 3 + c] += pos[(size_t)b * 3 + c]; Bs[(size_t)b * 3 + c] += pos[(size_t)a * 3 + c]; } ++nB[a]; ++nB[b]; }
    }
    opos.assign((nv + ne + nf) * 3, 0);
    for (size_t v = 0; v < nv; ++v)
        for (int c = 0; c < 3; ++c) {
            const float P = pos[v * 3 + c];
            if (!nF[v]) opos[v * 3 + c] = P;
            else if (boundary[v] && nB[v] == 2) opos[v * 3 + c] = 0.75f * P + 0.125f * Bs[v * 3 + c];
            else if (boundary[v]) opos[v * 3 + c] = P;
            else { const float n = (float)nF[v]; opos[v * 3 + c] = (Fs[v * 3 + c] / n + 2 * Rs[v * 3 + c] / nR[v] + (n - 3) * P) / n; }
        }
    std::copy(E.begin(), E.end(), opos.begin() + nv * 3);
    std::copy(F.begin(), F.end(), opos.begin() + (nv + ne) * 3);
    otri.clear(); ouv.clear(); otri.reserve(nf * 18); ouv.reserve(nf * 36);
    for (size_t f = 0; f < nf; ++f) {
        const float* u = &cuv[f * 6];
        const float fu[2] = {(u[0] + u[2] + u[4]) / 3, (u[1] + u[3] + u[5]) / 3};
        for (int k = 0; k < 3; ++k) {
            const int kp = (k + 2) % 3;   // the corner before
            const int v = tri[f * 3 + k], eN = (int)nv + fe[f][k], eP = (int)nv + fe[f][kp], fc = (int)(nv + ne + f);
            const float uv[2] = {u[k * 2], u[k * 2 + 1]};
            const float un[2] = {(u[k * 2] + u[((k + 1) % 3) * 2]) / 2, (u[k * 2 + 1] + u[((k + 1) % 3) * 2 + 1]) / 2};
            const float up[2] = {(u[k * 2] + u[kp * 2]) / 2, (u[k * 2 + 1] + u[kp * 2 + 1]) / 2};
            // the quad v, eN, fc, eP as two triangles
            otri.insert(otri.end(), {v, eN, fc, v, fc, eP});
            ouv.insert(ouv.end(), {uv[0], uv[1], un[0], un[1], fu[0], fu[1], uv[0], uv[1], fu[0], fu[1], up[0], up[1]});
        }
    }
}

static void normals(const std::vector<float>& pos, const std::vector<int>& tri, std::vector<float>& n);
static bool keepNormals = false;   // a slider is being dragged with "Update normals real-time" off: the old normals stay
// a mesh corner by corner (smoothed first with Smooth on)
static void unroll(const std::vector<float>& pos, const std::vector<int>& tri, const std::vector<float>& cuv, std::vector<float>& P, std::vector<float>& N, std::vector<float>& U, std::vector<float>& Tg) {
    std::vector<float> sp; std::vector<int> st; std::vector<float> su;
    const std::vector<float>* p = &pos; const std::vector<int>* t = &tri; const std::vector<float>* u = &cuv;
    if (lk.smooth) { subdivide(pos, tri, cuv, sp, st, su); p = &sp; t = &st; u = &su; }
    std::vector<float> n;
    if (keepNormals) n.assign(p->size(), 0); else normals(*p, *t, n);
    P.clear(); N.clear(); P.reserve(t->size() * 3); N.reserve(t->size() * 3);
    for (int v : *t) { P.insert(P.end(), &(*p)[(size_t)v * 3], &(*p)[(size_t)v * 3] + 3); N.insert(N.end(), &n[(size_t)v * 3], &n[(size_t)v * 3] + 3); }
    U = *u;
    // the tangent (the direction u grows in), for normal maps
    std::vector<float> tv(p->size(), 0);
    for (size_t c = 0; c + 2 < t->size(); c += 3) {
        const float* a = &(*p)[(size_t)(*t)[c] * 3]; const float* b = &(*p)[(size_t)(*t)[c + 1] * 3]; const float* d = &(*p)[(size_t)(*t)[c + 2] * 3];
        if (c * 2 + 5 >= u->size()) break;
        const float du1 = (*u)[c * 2 + 2] - (*u)[c * 2], dv1 = (*u)[c * 2 + 3] - (*u)[c * 2 + 1], du2 = (*u)[c * 2 + 4] - (*u)[c * 2], dv2 = (*u)[c * 2 + 5] - (*u)[c * 2 + 1];
        const float det = du1 * dv2 - du2 * dv1; if (std::fabs(det) < 1e-12f) continue;
        const float r = 1 / det;
        for (int k = 0; k < 3; ++k) { const float tk = ((b[k] - a[k]) * dv2 - (d[k] - a[k]) * dv1) * r; for (int q = 0; q < 3; ++q) tv[(size_t)(*t)[c + q] * 3 + k] += tk; }
    }
    Tg.clear(); Tg.reserve(t->size() * 3);
    for (int v : *t) {
        float x = tv[(size_t)v * 3], y = tv[(size_t)v * 3 + 1], z = tv[(size_t)v * 3 + 2]; const float l = std::sqrt(x * x + y * y + z * z);
        if (l > 1e-12f) { x /= l; y /= l; z /= l; } else { x = 1; y = 0; z = 0; }
        Tg.insert(Tg.end(), {x, y, z});
    }
}

static void fill(Part& pt, const std::vector<float>& P, const std::vector<float>& N, const std::vector<float>& U, const std::vector<float>& Tg) {
    if (!pt.vao) { glGenVertexArrays_(1, &pt.vao); glGenBuffers_(1, &pt.pos); glGenBuffers_(1, &pt.nrm); glGenBuffers_(1, &pt.uv); glGenBuffers_(1, &pt.tg); }
    const bool same = (int)P.size() / 3 == pt.count;
    upload(pt.vao, pt.pos, 0, 3, P); if (!(keepNormals && same)) upload(pt.vao, pt.nrm, 1, 3, N); upload(pt.vao, pt.uv, 2, 2, U); if (!(keepNormals && same)) upload(pt.vao, pt.tg, 3, 3, Tg);
    pt.count = (int)P.size() / 3;
}

// a material as MakeHuman's shader takes it (material.py's _updateShaderConfig, and the shader's parameter defaults)
static void matGL(const Mat::Material& m, MatGL& g) {
    g = MatGL();
    const std::string& s = m.shader;
    g.sh = s.empty() ? 0 : s == "litsphere" ? 1 : s == "normalmap" ? 2 : s == "phong" ? 3 : s == "skin" ? 4 : s == "toon" ? 5 : s == "xray" ? 6 : 1;
    const std::set<std::string> d = m.defines();
    // shaders that read a texture without a define (normalmap, skin and toon always do) take it when it's there
    const bool always = g.sh == 2 || g.sh == 4 || g.sh == 5;
    g.dDiffuse = (d.count("DIFFUSE") || ((always || g.sh == 0) && m.supportsDiffuse())) && !m.diffuseTexture.empty();
    g.dNormal = (d.count("NORMALMAP") || g.sh == 2) && !m.normalMapTexture.empty();
    g.dAo = d.count("AOMAP") && !m.aoMapTexture.empty();
    g.dAlpha = d.count("ALPHA_MAP") && !m.transparencyMapTexture.empty();
    g.autoSkin = m.autoBlendSkin;
    g.shadeless = m.shadeless; g.backfaceCull = m.backfaceCull; g.depthless = m.depthless; g.xray = g.sh == 6;
    g.blend = g.xray || (m.transparent && m.opacity < 0.999f); g.wire = m.wireframe;
    g.diffuse = g.dDiffuse ? texture(m.diffuseTexture) : 0;
    g.normal = g.dNormal ? texture(m.normalMapTexture) : 0;
    g.ao = g.dAo ? texture(m.aoMapTexture) : 0;
    g.alpha = g.dAlpha ? texture(m.transparencyMapTexture) : 0;
    if (g.dDiffuse && !g.diffuse) g.dDiffuse = false;
    if (g.dNormal && !g.normal) g.dNormal = false;
    if (g.dAo && !g.ao) g.dAo = false;
    const std::string lit = m.param("litsphereTexture");
    g.litsphere = lit.empty() ? 0 : texture(lit);
    if (!g.litsphere) g.litsphere = lit_standard;
    for (int c = 0; c < 3; ++c) { g.diff[c] = m.diffuse[c]; g.spec[c] = m.specular[c]; g.amb[c] = m.ambient[c]; g.emis[c] = m.emissive[c]; }
    g.diff[3] = m.opacity; g.spec[3] = m.shininess;
    g.normalInt = m.normalMapIntensity;
    auto f = [&](const char* name, float def) { const std::string v = m.param(name); if (!v.empty()) return (float)atof(v.c_str());
                                                 for (const auto& u : Mat::uniforms(s)) if (u.name == name && !u.def.empty()) return (float)atof(u.def.c_str()); return def; };
    g.additive = f("AdditiveShading", 0); g.silhouette = f("silhouetteThreshold", 0.2f); g.toonShine = f("shininess", 20);
    g.ambI = f("ambientIntensity", 0.4f); g.specI = f("specularIntensity", 0.2f); g.diffI = f("diffuseIntensity", 0.4f);
    g.specTh = f("specular_th", 0.3f); g.diffTh = f("diffuse_th", 0.5f); g.edge = f("edgefalloff", 1); g.xrayI = f("intensity", 0.7f);
}

void update(const Human::Body& b) {
    PROF("Viewer::update");
    keepNormals = lk.dragging && !lk.normalsWhileDragging;
    const bool keepFit = lk.dragging && !lk.fitWhileDragging;
    eth[0] = b.african; eth[1] = b.asian; eth[2] = b.caucasian;
    const std::vector<float>& pos = b.shown.empty() ? b.pos : b.shown;
    // the body: its visible triangles, less those under clothes (all of them under a topology)
    {
        const std::vector<char> hid = Human::hiddenVerts(b);
        std::vector<int> vt; std::vector<float> cuv;
        vt.reserve(b.tri.size()); cuv.reserve(b.tri.size() * 2);
        for (size_t t = 0; t + 2 < b.tri.size(); t += 3) {
            if (hid[b.tri[t]] || hid[b.tri[t + 1]] || hid[b.tri[t + 2]]) continue;
            for (size_t c = t; c < t + 3; ++c) {
                vt.push_back(b.tri[c]); const int u = b.triUv[c];
                if (u >= 0 && (size_t)u * 2 + 1 < b.uv.size()) cuv.insert(cuv.end(), {b.uv[(size_t)u * 2], b.uv[(size_t)u * 2 + 1]}); else cuv.insert(cuv.end(), {0, 0});
            }
        }
        std::vector<float> P, N, U, Tg;
        if (lk.distort != 0) {   // MakeHuman's Example plugin: the mesh pushed out along its normals
            std::vector<float> nn; normals(pos, b.tri, nn); std::vector<float> moved = pos;
            for (size_t k = 0; k < moved.size(); ++k) moved[k] += nn[k] * lk.distort;
            unroll(moved, vt, cuv, P, N, U, Tg);
        } else
        unroll(pos, vt, cuv, P, N, U, Tg);
        fill(bodyPart, P, N, U, Tg);
        matGL(b.skinMat, bodyPart.mat);
        bodyPart.tex = texture(b.skinTexture);
        bodyPart.mode = bodyPart.tex ? 4 : 0;
        for (int c = 0; c < 3; ++c) bodyPart.tint[c] = b.skinTint[c];
    }
    // the proxies, fitted to the body as it is now, in MakeHuman's z_depth order
    parts.resize(std::max(parts.size(), b.proxies.size()));
    for (size_t i = 0; i < parts.size(); ++i) {
        Part& pt = parts[i];
        if (i >= b.proxies.size()) { pt.count = 0; continue; }
        if (keepFit && pt.count) continue;   // "Fit objects real-time" off: the clothes catch up when the slider is let go
        const Human::Proxy& p = b.proxies[i];
        std::vector<float> fit; Human::fitProxy(b, p, fit);
        std::vector<float> P, N, U, Tg;
        unroll(fit, p.tri, p.uv, P, N, U, Tg);
        fill(pt, P, N, U, Tg);
        matGL(p.kind == "proxymeshes" ? b.skinMat : p.mat, pt.mat);
        const bool topology = p.kind == "proxymeshes";
        pt.tex = texture(topology ? b.skinTexture : p.texture);
        pt.mode = p.kind == "eyes" ? 1 : topology ? (pt.tex ? 4 : 0) : 3;
        pt.cutout = p.transparent || p.kind == "hair" || p.kind == "eyebrows" || p.kind == "eyelashes";
        for (int c = 0; c < 3; ++c) pt.tint[c] = topology ? b.skinTint[c] : p.tint[c];
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
    PROF("Viewer::render");
    if (w < 8 || h < 8) return fboTex;
    ImGuiIO& io = ImGui::GetIO();
    if (hovered) {
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) { const float sg = lk.invertTurn ? -lk.turnSpeed : lk.turnSpeed; lk.yaw -= io.MouseDelta.x * 0.008f * sg; lk.pitch = std::clamp(lk.pitch + io.MouseDelta.y * 0.006f * sg, -1.4f, 1.4f); }
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Right) || ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
            const float k = lk.dist * 0.0015f * lk.moveSpeed, cy = std::cos(lk.yaw), sy = std::sin(lk.yaw);
            target[0] -= io.MouseDelta.x * k * cy; target[2] += io.MouseDelta.x * k * sy; target[1] += io.MouseDelta.y * k;
        }
        if (io.MouseWheel != 0) lk.dist = std::clamp(lk.dist * std::pow(std::pow(0.88f, lk.zoomSpeed), lk.invertZoom ? -io.MouseWheel : io.MouseWheel), height * 0.12f, height * 6);
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
    glClearColor(lk.bg[0], lk.bg[1], lk.bg[2], lk.transparent ? 0.0f : 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    // a picture behind the person, fitted inside the view
    if (!lk.transparent && !lk.bgImage.empty()) {
        if (bgLoaded != lk.bgImage) { bgLoaded = lk.bgImage; bgTex = texture(lk.bgImage); }
        if (bgTex) {
            int iw = 1, ih = 1;
            glBindTexture(GL_TEXTURE_2D, bgTex);
            glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &iw); glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &ih);
            const float va = (float)w / h, ia = (float)iw / std::max(ih, 1);
            glUseProgram_(progBg);
            glUniform1i_(glGetUniformLocation_(progBg, "img"), 0);
            glUniform1f_(glGetUniformLocation_(progBg, "alpha"), lk.bgOpacity);
            glUniform3f_(glGetUniformLocation_(progBg, "bg"), lk.bg[0], lk.bg[1], lk.bg[2]);
            glUniform2f_(glGetUniformLocation_(progBg, "fit"), ia > va ? 1.0f : ia / va, ia > va ? va / ia : 1.0f);
            glActiveTexture_(GL_TEXTURE0);
            glDisable(GL_DEPTH_TEST);
            glBindVertexArray_(bgVao);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            glEnable(GL_DEPTH_TEST);
        }
    }
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
    float P[16] = {t / a, 0, 0, 0, 0, t, 0, 0, 0, 0, -(fr + n) / (fr - n), -1, 0, 0, -2 * fr * n / (fr - n), 0};
    if (lk.ortho) {   // the same framing as the perspective view at the target's distance
        const float hh = lk.dist / t;
        const float O[16] = {1 / (hh * a), 0, 0, 0, 0, 1 / hh, 0, 0, 0, 0, -2 / (fr - n), 0, 0, 0, -(fr + n) / (fr - n), 1};
        memcpy(P, O, sizeof O);
    }
    float mvp[16]; mul(P, V, mvp);
    // the floor
    if (lk.grid && !lk.transparent) {
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
    glUniform3f_(glGetUniformLocation_(prog, "lcol"), lk.lightCol[0], lk.lightCol[1], lk.lightCol[2]);
    {   // the scene's lights, turned into view space (those that turn with the view are there already)
        float dirs[24] = {0}, cols[24] = {0}, specs[24] = {0}; int nl = 0;
        mhN = 0;
        for (const auto& L : lk.lights) {
            if (!L.on || nl == 8) continue;
            const float cp2 = std::cos(L.pitch), d[3] = {cp2 * std::sin(L.yaw), std::sin(L.pitch), cp2 * std::cos(L.yaw)};
            float v[3];
            if (L.withView) { v[0] = d[0]; v[1] = d[1]; v[2] = d[2]; }
            else { v[0] = s[0] * d[0] + s[1] * d[1] + s[2] * d[2]; v[1] = u[0] * d[0] + u[1] * d[1] + u[2] * d[2]; v[2] = -(f[0] * d[0] + f[1] * d[1] + f[2] * d[2]); }
            for (int c = 0; c < 3; ++c) { dirs[nl * 3 + c] = v[c]; cols[nl * 3 + c] = L.col[c] * L.strength; specs[nl * 3 + c] = L.spec[c] * L.strength; }
            // MakeHuman's lights are points: far out along the direction, in view space
            for (int c = 0; c < 3; ++c) { mhPos[nl * 3 + c] = v[c] * 31.0f; mhCol[nl * 3 + c] = cols[nl * 3 + c]; mhSpec[nl * 3 + c] = specs[nl * 3 + c]; }
            mhN = nl + 1;
            ++nl;
        }
        glUniform1i_(glGetUniformLocation_(prog, "nl"), nl);
        glUniform3fv_(glGetUniformLocation_(prog, "ldir"), 8, dirs);
        glUniform3fv_(glGetUniformLocation_(prog, "lrgb"), 8, cols);
        glUniform3fv_(glGetUniformLocation_(prog, "lspec"), 8, specs);
        glUniform3f_(glGetUniformLocation_(prog, "amb"), lk.ambient[0], lk.ambient[1], lk.ambient[2]);
        glUniform1f_(glGetUniformLocation_(prog, "shine"), lk.shine);
        glUniform1f_(glGetUniformLocation_(prog, "shininess"), lk.shininess);
        glUniform1i_(glGetUniformLocation_(prog, "sceneSkin"), lk.sceneSkin ? 1 : 0);
    }
    glUniform1i_(glGetUniformLocation_(prog, "litA"), 0); glUniform1i_(glGetUniformLocation_(prog, "litB"), 1);
    glUniform1i_(glGetUniformLocation_(prog, "litC"), 2); glUniform1i_(glGetUniformLocation_(prog, "tex"), 3);
    const GLint uMode = glGetUniformLocation_(prog, "mode"), uHas = glGetUniformLocation_(prog, "hasTex"), uCut = glGetUniformLocation_(prog, "cutout");
    // MakeHuman's shaders: the frame's uniforms
    const bool mh = lk.mhShaders && lk.shading != 1 && progMh;
    if (mh) {
        glUseProgram_(progMh);
        auto U = [&](const char* n) { return glGetUniformLocation_(progMh, n); };
        glUniformMatrix4fv_(U("mvp"), 1, GL_FALSE, mvp); glUniformMatrix4fv_(U("view"), 1, GL_FALSE, V);
        glUniform3f_(U("eth"), eth[0], eth[1], eth[2]);
        glUniform1f_(U("light"), lk.light); glUniform3f_(U("lcol"), lk.lightCol[0], lk.lightCol[1], lk.lightCol[2]);
        glUniform3f_(U("sceneAmb"), lk.ambient[0], lk.ambient[1], lk.ambient[2]);
        glUniform3f_(U("l0pos"), mhN ? mhPos[0] : -10.99f, mhN ? mhPos[1] : 20, mhN ? mhPos[2] : 20);
        glUniform3f_(U("l0col"), mhN ? mhCol[0] : 0, mhN ? mhCol[1] : 0, mhN ? mhCol[2] : 0);
        glUniform3f_(U("l0spec"), mhN ? mhSpec[0] : 0, mhN ? mhSpec[1] : 0, mhN ? mhSpec[2] : 0);
        glUniform1i_(U("nl"), mhN);
        glUniform3fv_(U("lpos"), 8, mhPos); glUniform3fv_(U("lrgb"), 8, mhCol); glUniform3fv_(U("lspec"), 8, mhSpec);
        const char* units[] = {"diffuseTexture", "litsphereTexture", "normalmapTexture", "aomapTexture", "alphaTexture", "litA", "litB", "litC"};
        for (int k = 0; k < 8; ++k) glUniform1i_(U(units[k]), k);
        glUseProgram_(prog);
    }
    auto drawMh = [&](const Part& pt) {
        const MatGL& g = pt.mat;
        glUseProgram_(progMh);
        auto U = [&](const char* n) { return glGetUniformLocation_(progMh, n); };
        const GLuint tex[8] = {g.diffuse, g.litsphere, g.normal, g.ao, g.alpha, lit[0], lit[1], lit[2]};
        for (int k = 0; k < 8; ++k) { glActiveTexture_(GL_TEXTURE0 + k); glBindTexture(GL_TEXTURE_2D, tex[k]); }
        glUniform1i_(U("sh"), g.sh);
        glUniform1i_(U("dDiffuse"), g.dDiffuse); glUniform1i_(U("dNormal"), g.dNormal); glUniform1i_(U("dAo"), g.dAo); glUniform1i_(U("dAlpha"), g.dAlpha);
        glUniform1i_(U("autoSkin"), g.autoSkin); glUniform1i_(U("shadeless"), g.shadeless); glUniform1i_(U("cutout"), pt.cutout ? 1 : 0); glUniform1i_(U("blend"), g.blend);
        glUniform3f_(U("tint"), pt.tint[0], pt.tint[1], pt.tint[2]);
        const float d4[4] = {g.diff[0], g.diff[1], g.diff[2], g.diff[3]};
        glUniform3f_(U("mAmbient"), g.amb[0], g.amb[1], g.amb[2]); glUniform3f_(U("mEmissive"), g.emis[0], g.emis[1], g.emis[2]);
        glUniform4f_(U("mDiffuse"), d4[0], d4[1], d4[2], d4[3]); glUniform4f_(U("mSpecular"), g.spec[0], g.spec[1], g.spec[2], g.spec[3]);
        glUniform1f_(U("AdditiveShading"), g.additive); glUniform1f_(U("normalmapIntensity"), g.normalInt);
        glUniform1f_(U("silhouetteThreshold"), g.silhouette); glUniform1f_(U("toonShininess"), g.toonShine); glUniform1f_(U("ambientIntensity"), g.ambI);
        glUniform1f_(U("specularIntensity"), g.specI); glUniform1f_(U("diffuseIntensity"), g.diffI); glUniform1f_(U("specular_th"), g.specTh);
        glUniform1f_(U("diffuse_th"), g.diffTh); glUniform1f_(U("edgefalloff"), g.edge); glUniform1f_(U("xrayIntensity"), g.xrayI);
        if (g.blend) { glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); glDepthMask(GL_FALSE); }
        if (g.depthless) glDisable(GL_DEPTH_TEST);
        if (g.wire) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glBindVertexArray_(pt.vao);
        glDrawArrays(GL_TRIANGLES, 0, pt.count);
        if (g.wire) glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        if (g.blend) { glDisable(GL_BLEND); glDepthMask(GL_TRUE); }
        if (g.depthless) glEnable(GL_DEPTH_TEST);
        glUseProgram_(prog);
    };
    auto draw = [&](const Part& pt, bool lines) {
        if (!pt.count) return;
        if (mh) {
            drawMh(pt);
            if (lines) {
                glUseProgram_(progLine);
                glUniformMatrix4fv_(glGetUniformLocation_(progLine, "mvp"), 1, GL_FALSE, mvp);
                glUniform3f_(glGetUniformLocation_(progLine, "col"), 0.88f, 0.25f, 0.54f);
                glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); glEnable(GL_POLYGON_OFFSET_LINE); glPolygonOffset(-1, -1);
                glDrawArrays(GL_TRIANGLES, 0, pt.count);
                glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); glDisable(GL_POLYGON_OFFSET_LINE);
                glUseProgram_(prog);
            }
            return;
        }
        for (int k = 0; k < 3; ++k) { glActiveTexture_(GL_TEXTURE0 + k); glBindTexture(GL_TEXTURE_2D, pt.mode == 1 && k == 0 ? litEye : lit[k]); }
        glActiveTexture_(GL_TEXTURE0 + 3); glBindTexture(GL_TEXTURE_2D, pt.tex);
        const bool clay = lk.shading == 1 && pt.mode != 1;
        glUniform1i_(uMode, clay ? 2 : pt.mode);
        glUniform1i_(uHas, pt.tex ? 1 : 0);
        glUniform1i_(uCut, pt.cutout && pt.tex ? 1 : 0);
        glUniform3f_(glGetUniformLocation_(prog, "tint"), pt.tint[0], pt.tint[1], pt.tint[2]);
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
    if (lk.bones && !boneSegs.empty()) {
        glDisable(GL_DEPTH_TEST);
        glBindVertexArray_(gridVao); glBindBuffer_(GL_ARRAY_BUFFER, gridVbo);
        glBufferData_(GL_ARRAY_BUFFER, (GLsizeiptr)(boneSegs.size() * 4), boneSegs.data(), GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray_(0); glVertexAttribPointer_(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
        glUseProgram_(progLine);
        glUniformMatrix4fv_(glGetUniformLocation_(progLine, "mvp"), 1, GL_FALSE, mvp);
        glUniform3f_(glGetUniformLocation_(progLine, "col"), 0.88f, 0.25f, 0.54f);
        glLineWidth(2);
        glDrawArrays(GL_LINES, 0, (GLsizei)(boneSegs.size() / 3));
        glUniform3f_(glGetUniformLocation_(progLine, "col"), 1.0f, 1.0f, 1.0f);
        glPointSize(5);
        glDrawArrays(GL_POINTS, 0, (GLsizei)(boneSegs.size() / 3));
        glLineWidth(1);
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

void setBones(const std::vector<float>& lines) { boneSegs = lines; }

bool renderImage(int w, int h, int ss, std::vector<unsigned char>& rgba) {
    ss = std::clamp(ss, 1, 4);
    GLint maxSize = 4096; glGetIntegerv(GL_MAX_RENDERBUFFER_SIZE, &maxSize);
    while (ss > 1 && (w * ss > maxSize || h * ss > maxSize)) --ss;
    if (w * ss > maxSize || h * ss > maxSize || w < 8 || h < 8) return false;
    const int oldW = fbW, oldH = fbH;
    render(w * ss, h * ss, false);
    std::vector<unsigned char> big; int bw, bh;
    readPixels(big, bw, bh);
    rgba.assign((size_t)w * h * 4, 0);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            for (int c = 0; c < 4; ++c) {
                unsigned sum = 0;
                for (int j = 0; j < ss; ++j) for (int i = 0; i < ss; ++i) sum += big[((size_t)(y * ss + j) * bw + (x * ss + i)) * 4 + c];
                rgba[((size_t)y * w + x) * 4 + c] = (unsigned char)(sum / (ss * ss));
            }
    if (oldW >= 8 && oldH >= 8) render(oldW, oldH, false);
    return true;
}

}
