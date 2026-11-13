// Pygmalion - the splash: a figure drawn in lines on a plinth (the statue that came to life), the pink one stepping off
// it, the name, the version, the credits and what's loading. Atelier greys and the Cursed pink, solid.
#include "splash.h"

#include <imgui.h>

#include <cfloat>
#include <cmath>

#ifndef PYGMALION_VERSION
#define PYGMALION_VERSION "1.0.0"
#endif

namespace Splash {

static ImFont *titleFont = nullptr, *smallFont = nullptr;
void setFonts(ImFont* title, ImFont* small) { titleFont = title; smallFont = small; }

static const ImU32 BG = IM_COL32(0x1a, 0x1a, 0x1a, 255), LINE = IM_COL32(0x33, 0x33, 0x33, 255),
                   TEXT = IM_COL32(0xd6, 0xd6, 0xd6, 255), DIM = IM_COL32(0x9a, 0x9a, 0x9a, 255),
                   PINK = IM_COL32(0x00, 0x7b, 0xb7, 255), GREY = IM_COL32(0x77, 0x77, 0x77, 255);

static void text(ImDrawList* dl, ImFont* f, float size, ImVec2 p, ImU32 col, const char* s) { dl->AddText(f ? f : ImGui::GetFont(), size, p, col, s); }
static float textWidth(ImFont* f, float size, const char* s) { return (f ? f : ImGui::GetFont())->CalcTextSizeA(size, FLT_MAX, 0, s).x; }

// a standing figure as a few thick strokes: head, body, arms, legs
static void figure(ImDrawList* dl, ImVec2 o, float s, ImU32 col, float stride) {
    auto P = [&](float x, float y) { return ImVec2(o.x + x * s, o.y + y * s); };
    dl->AddCircle(P(0, -82), 11 * s, col, 24, 3);
    dl->AddLine(P(0, -70), P(0, -10), col, 4);                       // spine
    dl->AddLine(P(-20, -60), P(20, -60), col, 4);                    // shoulders
    dl->AddLine(P(-20, -60), P(-28 - stride * 4, -24), col, 3);      // arms
    dl->AddLine(P(20, -60), P(28 + stride * 6, -26), col, 3);
    dl->AddLine(P(-11, -10), P(11, -10), col, 4);                    // hips
    dl->AddLine(P(-11, -10), P(-13 - stride * 10, 40), col, 4);      // legs
    dl->AddLine(P(11, -10), P(14 + stride * 14, 40), col, 4);
}

void draw(ImDrawList* dl, float x, float y, const char* status, float progress) {
    const ImVec2 o(x, y), end(x + W, y + H);
    dl->PushClipRect(o, end, true);
    dl->AddRectFilled(o, end, BG);
    // the plinth and the statue on it, grey; beside it the same figure in pink, stepping down
    const float px = x + 440, py = y + 250;
    dl->AddRectFilled(ImVec2(px - 46, py + 40), ImVec2(px + 46, py + 70), LINE);
    dl->AddRectFilled(ImVec2(px - 56, py + 70), ImVec2(px + 56, py + 78), IM_COL32(0x44, 0x44, 0x44, 255));
    figure(dl, ImVec2(px, py), 1.25f, GREY, 0);
    figure(dl, ImVec2(px + 120, py + 38), 1.25f, PINK, 1);
    for (int i = 0; i < 5; ++i) dl->AddCircleFilled(ImVec2(px + 64 + i * 9, py - 30 + i * 13), 2.2f, PINK, 8);   // a spark between them

    const float ts = 50, tx = x + 40, ty = y + 92;
    text(dl, titleFont, ts, ImVec2(tx, ty), TEXT, "Pygma");
    text(dl, titleFont, ts, ImVec2(tx + textWidth(titleFont, ts, "Pygma"), ty), PINK, "lion");
    text(dl, nullptr, 16, ImVec2(tx + 2, ty + 62), DIM, "Version " PYGMALION_VERSION);
    text(dl, nullptr, 16, ImVec2(tx + 2, ty + 84), DIM, "People in 3D by Cursed Entertainment");

    const float cs = 13, cry = y + 306;
    text(dl, smallFont, cs, ImVec2(tx, cry), DIM, "The body, its shapes and its maths are MakeHuman's (AGPL-3.0; the data CC0).");
    text(dl, smallFont, cs, ImVec2(tx, cry + 17), DIM, "Built with Dear ImGui, SDL2 and stb.");
    text(dl, smallFont, cs, ImVec2(tx, cry + 34), DIM, "Farica\xe2\x98\x86Kimora \xc2\xa9 Cursed Entertainment");
    if (status) text(dl, nullptr, 16, ImVec2(tx, y + 368), TEXT, status);
    if (progress >= 0) {
        const float p = progress > 1 ? 1 : progress;
        dl->AddRectFilled(ImVec2(x, end.y - 3), end, LINE);
        dl->AddRectFilled(ImVec2(x, end.y - 3), ImVec2(x + W * p, end.y), PINK);
    }
    dl->AddRect(o, end, LINE);
    dl->PopClipRect();
}

}
