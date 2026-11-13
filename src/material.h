// Pygmalion - MakeHuman's materials (shared/material.py, AGPL-3.0): everything a .mhmat holds - colours, shininess,
// opacity, the flags, the seven texture maps and their strengths, subsurface scattering, the shader with its config,
// defines and parameters, the uv map - read and written as MakeHuman reads and writes them.
#pragma once
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace Mat {

// MakeHuman's shaders (data/shaders/glsl): "" is [None], fixed-function lighting
extern const char* SHADERS[];          // "litsphere", "normalmap", "phong", "skin", "toon", "xray" (sorted, as listed)
extern const int SHADER_COUNT;
// the defines a shader can take, read from its files (#ifdef NAME), and its own uniforms (name, type, default)
struct Uniform { std::string name, type, def; };
const std::vector<std::string>& defineables(const std::string& shader);
const std::vector<Uniform>& uniforms(const std::string& shader);
// the defines the shader config turns on (material.py's _shaderConfigDefines) and the uniforms the material sets
extern const char* CONFIG_DEFINES[];   // DIFFUSE BUMPMAP NORMALMAP DISPLACEMENT SPECULARMAP VERTEX_COLOR TRANSPARENCYMAP AOMAP
bool isMaterialParam(const std::string& uniform);

struct Material {
    std::string name = "UnnamedMaterial", description = "UnnamedMaterial material", filename;
    std::vector<std::string> tags;
    float ambient[3] = {1, 1, 1}, diffuse[3] = {1, 1, 1}, specular[3] = {1, 1, 1}, emissive[3] = {0, 0, 0};
    float shininess = 0.2f, opacity = 1, translucency = 0;
    float viewPortColor[3] = {0, 0, 0}, viewPortAlpha = 1; bool hasViewPortColor = false;
    bool shadeless = false, wireframe = false, transparent = false, alphaToCoverage = true, backfaceCull = true, depthless = false;
    bool castShadows = true, receiveShadows = true, autoBlendSkin = false;
    // textures: absolute paths ("" none); intensities 0..1
    std::string diffuseTexture, bumpMapTexture, normalMapTexture, displacementMapTexture, specularMapTexture, transparencyMapTexture, aoMapTexture;
    float bumpMapIntensity = 1, normalMapIntensity = 1, displacementMapIntensity = 1, specularMapIntensity = 1, transparencyMapIntensity = 1, aoMapIntensity = 1;
    bool sssEnabled = false; float sssRScale = 0, sssGScale = 0, sssBScale = 0;
    std::string shader;                                   // "litsphere", "phong"... ("" : none)
    std::string uvMap;                                    // an alternative uv set (.mhuv/.obj, "" : the mesh's own)
    // the shader config (diffuse, bump, normal, displacement, spec, vertexColors, transparency, ambientOcclusion)
    bool cfgDiffuse = true, cfgBump = true, cfgNormal = true, cfgDisplacement = true, cfgSpec = true, cfgVertexColors = true, cfgTransparency = true, cfgAo = true;
    std::vector<std::string> shaderDefines;               // the custom ones (not the config's)
    std::vector<std::pair<std::string, std::string>> shaderParams;   // name, value as written ("1 0.5 0.2", a texture path...)

    bool supportsDiffuse() const { return !diffuseTexture.empty(); }
    bool supportsBump() const { return !bumpMapTexture.empty(); }
    bool supportsNormal() const { return !normalMapTexture.empty(); }
    bool supportsDisplacement() const { return !displacementMapTexture.empty(); }
    bool supportsSpecular() const { return !specularMapTexture.empty(); }
    bool supportsTransparency() const { return !transparencyMapTexture.empty(); }
    bool supportsAo() const { return !aoMapTexture.empty(); }
    // the defines in force (material.py _updateShaderConfig: a config option on and its texture there, plus the custom ones)
    std::set<std::string> defines() const;
    std::string param(const std::string& name, const std::string& def = "") const;
    void setParam(const std::string& name, const std::string& value);
};

// resolve: a path as written in the file, and the file's folder -> an absolute path ("" if not found)
using Resolver = std::function<std::string(const std::string& written, const std::string& folder)>;
bool fromFile(const std::string& path, Material& m, const Resolver& resolve);
// written as material.py's toFile; paths relative to the file's folder where they're under it
bool toFile(const Material& m, const std::string& path, std::string& err, const std::vector<std::string>& comments = {});
// the shader files' folder (data/shaders/glsl), for defineables/uniforms
void setShaderFolder(const std::string& folder);

}
