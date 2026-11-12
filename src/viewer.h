// Pygmalion - the 3D view: the body shaded with MakeHuman's litspheres (matcaps: the skin tone blended from the three
// ethnicities), the eyes textured, turned with the mouse. Drawn into its own texture, shown inside the window.
#pragma once
#include "human.h"

namespace Viewer {
bool init(const std::string& dataDir);          // after the GL context exists; loads the litspheres and eye textures
void update(const Human::Body& b);              // after the body changed: new positions and normals
struct Look { float yaw = 0.3f, pitch = 0.05f, dist = 0, light = 1; int shading = 0; bool grid = true, wire = false; };
Look& look();
enum Camera { CAM_FRONT, CAM_SIDE, CAM_BACK, CAM_FACE, CAM_FACE_SIDE };
void camera(int which);                         // the sliders' own views (MakeHuman's frontView, leftView...)
unsigned render(int w, int h, bool hovered);
void readPixels(std::vector<unsigned char>& rgba, int& w, int& h);
}
