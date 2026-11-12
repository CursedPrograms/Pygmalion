// Pygmalion - the 3D view: the body shaded with MakeHuman's litspheres (matcaps: the skin tone blended from the three
// ethnicities), the eyes textured, turned with the mouse. Drawn into its own texture, shown inside the window.
#pragma once
#include "human.h"

namespace Viewer {
bool init(const std::string& dataDir);          // after the GL context exists; loads the litspheres and eye textures
void update(const Human::Body& b);              // after the body changed: new positions and normals
struct Look { float yaw = 0.3f, pitch = 0.05f, dist = 0, light = 1; int shading = 0; bool grid = true, wire = false; };
Look& look();
// look at a point from a direction (yaw 0: the front; pi/2: the person's left side), from dist away
void focus(const float at[3], float dist, float yaw, float pitch);
void focusBody(float yaw, float pitch);         // the whole person
float bodyHeight();
unsigned render(int w, int h, bool hovered);
void readPixels(std::vector<unsigned char>& rgba, int& w, int& h);
}
