// Pygmalion - the 3D view: the body shaded with MakeHuman's litspheres (matcaps: the skin tone blended from the three
// ethnicities), the eyes textured, turned with the mouse. Drawn into its own texture, shown inside the window.
#pragma once
#include "human.h"
#include <string>
#include <vector>

namespace Viewer {
bool init(const std::string& dataDir);          // after the GL context exists; loads the litspheres and eye textures
void update(const Human::Body& b);              // after the body changed: new positions and normals
struct Look {
    float yaw = 0.3f, pitch = 0.05f, dist = 0, light = 1; int shading = 0; bool grid = true, wire = false;
    float lightCol[3] = {1, 1, 1};                  // the scene's light colour (Rendering > Scene)
    float bg[3] = {0x14 / 255.0f, 0x14 / 255.0f, 0x14 / 255.0f};
    bool transparent = false;                       // renders: no background, no floor
    bool ortho = false;                             // MakeHuman's orthographic view
    bool bones = false;                             // the skeleton over the body
    bool smooth = false;                            // MakeHuman's Smooth: one Catmull-Clark subdivision
    bool mhShaders = true;                          // each object drawn with its material's own MakeHuman shader
    // MakeHuman's slider behaviour: while a slider is dragged, refit the clothes / redo the normals (else when let go)
    float distort = 0;                              // Utilities > Example: the body pushed along its normals (view only)
    bool dragging = false, fitWhileDragging = true, normalsWhileDragging = true;
    std::string bgImage;                            // a picture behind the person (Settings > Background)
    float bgOpacity = 1;
    // MakeHuman's scene (Rendering > Scene): ambient light, shine and up to 8 lights (MakeHuman's limit). They light
    // clothes, hair, skin textures and clay; the default skin is MakeHuman's litsphere, which (as in MakeHuman) has its
    // light painted in. A light's direction is yaw/pitch; MakeHuman's own numbers (a point light at a position, looking
    // at a focus, in the view's axes) are kept beside it so its scene files go back out as they came in.
    struct Light {
        bool on = true; float yaw = 0, pitch = 0, col[3] = {1, 1, 1}, strength = 1; bool withView = true;
        float spec[3] = {1, 1, 1};                                   // its highlight's colour
        float pos[3] = {-10.99f, 20, 20}, focus[3] = {0, 0, 0};      // MakeHuman's position and focus
        float fov = 180, attenuation = 0, areaLightSize = 4; int areaLights = 1;   // its spot angle, falloff, softness
    };
    std::string skybox;                             // MakeHuman's environment skybox (kept, as MakeHuman doesn't draw it either)
    float ambient[3] = {0.38f, 0.38f, 0.38f};
    float shine = 0.04f, shininess = 24;
    std::vector<Light> lights = {Light{true, 0, 0, {1, 1, 1}, 0.72f, true}};
    bool sceneSkin = false;                         // light MakeHuman's litsphere skin by the scene too
    float turnSpeed = 1, moveSpeed = 1, zoomSpeed = 1;   // Settings > Mouse
    bool invertTurn = false, invertZoom = false;
};
Look& look();
// look at a point from a direction (yaw 0: the front; pi/2: the person's left side), from dist away
void focus(const float at[3], float dist, float yaw, float pitch);
void focusBody(float yaw, float pitch);         // the whole person
void viewTarget(float at[3]);                   // the point the view turns round
// Utilities > Skeleton Debug: the body coloured by one bone's weights (per body vertex, empty: off), and coloured
// lines (axes) and triangles (bone planes), 3 floats a point and 4 a colour
void setDebugWeights(const std::vector<float>& perVertex);
void setDebugShapes(const std::vector<float>& lines, const std::vector<float>& lineRgba, const std::vector<float>& tris, const std::vector<float>& triRgba);
float bodyHeight();
unsigned render(int w, int h, bool hovered);
void readPixels(std::vector<unsigned char>& rgba, int& w, int& h);
void setBones(const std::vector<float>& lines);  // posed bones, head and tail (6 floats each)
// a picture of the view at any size (ss: supersampling, 1..4), straight into rgba (top row first)
bool renderImage(int w, int h, int ss, std::vector<unsigned char>& rgba);
}
