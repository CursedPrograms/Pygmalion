// Pygmalion - MakeHuman's scene files (.mhscene, shared/scene.py, AGPL-3.0): a run of Python pickles - the version
// (5), the environment (ambience, skybox), the number of lights, then each light's attributes in name order. Read with
// a small pickle reader that only knows numbers, strings, tuples and MakeHuman's material.Color (nothing is run), and
// written byte for byte as MakeHuman writes them.
#pragma once
#include <string>
#include <vector>

namespace MhScene {

// doubles, as Python keeps them, so a file read and written again is the same file
struct Light {
    double position[3] = {-10.99, 20, 20}, focus[3] = {0, 0, 0};
    double color[3] = {1, 1, 1}, specular[3] = {1, 1, 1};
    double fov = 180, attenuation = 0, areaLightSize = 4;
    int areaLights = 1;
};
struct Scene {
    double ambience[3] = {0.3, 0.3, 0.3};
    std::string skybox;                 // "" : none
    std::vector<Light> lights = {Light()};
};

bool load(const std::string& path, Scene& s, std::string& err);
// a float as a double for the file: was (the number read from the file) if the float is still that number, else the
// shortest decimal that reads back as the float (0.3f is written 0.3, as MakeHuman's own numbers are typed)
double keep(float f, double was);
bool save(const Scene& s, const std::string& path, std::string& err);

}
