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
    std::string bgImage;                            // a picture behind the person (Settings > Background)
    float bgOpacity = 1;
};
Look& look();
// look at a point from a direction (yaw 0: the front; pi/2: the person's left side), from dist away
void focus(const float at[3], float dist, float yaw, float pitch);
void focusBody(float yaw, float pitch);         // the whole person
float bodyHeight();
unsigned render(int w, int h, bool hovered);
void readPixels(std::vector<unsigned char>& rgba, int& w, int& h);
void setBones(const std::vector<float>& lines);  // posed bones, head and tail (6 floats each)
// a picture of the view at any size (ss: supersampling, 1..4), straight into rgba (top row first)
bool renderImage(int w, int h, int ss, std::vector<unsigned char>& rgba);
}
