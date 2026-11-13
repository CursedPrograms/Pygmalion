// Pygmalion - MakeHuman's scene files (see mhscene.h)
#include "mhscene.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>

namespace fs = std::filesystem;

namespace MhScene {

namespace {
// ---------- a pickle reader for data only ----------
struct V {
    enum T { NONE, NUM, STR, SEQ, DICT, COLOR, MARK, GLOBAL } t = NONE;
    double n = 0;
    std::string s;
    std::vector<std::shared_ptr<V>> seq;                          // tuple / list
    std::vector<std::pair<std::shared_ptr<V>, std::shared_ptr<V>>> dict;
    double rgb[3] = {0, 0, 0};
};
using P = std::shared_ptr<V>;

struct Reader {
    const std::string& b; size_t i = 0; std::string err;
    explicit Reader(const std::string& bytes) : b(bytes) {}
    bool more() const { return i < b.size(); }
    bool need(size_t n) { if (i + n > b.size()) { err = "The file ends early"; return false; } return true; }
    uint32_t u(int n) { uint32_t v = 0; for (int k = 0; k < n; ++k) v |= (uint32_t)(uint8_t)b[i + k] << (8 * k); i += n; return v; }
    std::string line() { const size_t e = b.find('\n', i); std::string s = b.substr(i, e == std::string::npos ? std::string::npos : e - i); i = e == std::string::npos ? b.size() : e + 1; return s; }
    // one pickle (up to its STOP)
    bool one(P& out) {
        std::vector<P> st; std::map<uint32_t, P> memo;
        auto num = [](double n) { auto v = std::make_shared<V>(); v->t = V::NUM; v->n = n; return v; };
        auto str = [](std::string s) { auto v = std::make_shared<V>(); v->t = V::STR; v->s = std::move(s); return v; };
        auto popMark = [&](std::vector<P>& items) {
            size_t m = st.size(); while (m > 0 && st[m - 1]->t != V::MARK) --m;
            if (m == 0) { err = "No mark"; return false; }
            items.assign(st.begin() + (long)m, st.end()); st.resize(m - 1); return true;
        };
        auto tuple = [&](size_t n) { if (st.size() < n) { err = "Short stack"; return false; } auto v = std::make_shared<V>(); v->t = V::SEQ; v->seq.assign(st.end() - (long)n, st.end()); st.resize(st.size() - n); st.push_back(v); return true; };
        while (more()) {
            const uint8_t op = (uint8_t)b[i++];
            switch (op) {
            case 0x80: if (!need(1)) return false; ++i; break;                                   // PROTO
            case 0x95: if (!need(8)) return false; i += 8; break;                                // FRAME
            case '.': if (st.empty()) { err = "Empty pickle"; return false; } out = st.back(); return true;
            case 'K': if (!need(1)) return false; st.push_back(num(u(1))); break;
            case 'M': if (!need(2)) return false; st.push_back(num(u(2))); break;
            case 'J': if (!need(4)) return false; st.push_back(num((int32_t)u(4))); break;
            case 'G': { if (!need(8)) return false; uint64_t x = 0; for (int k = 0; k < 8; ++k) x = (x << 8) | (uint8_t)b[i + k]; i += 8; double d; memcpy(&d, &x, 8); st.push_back(num(d)); break; }
            case 'N': st.push_back(std::make_shared<V>()); break;
            case 0x88: st.push_back(num(1)); break;
            case 0x89: st.push_back(num(0)); break;
            case 'U': { if (!need(1)) return false; const uint32_t n = u(1); if (!need(n)) return false; st.push_back(str(b.substr(i, n))); i += n; break; }
            case 0x8c: { if (!need(1)) return false; const uint32_t n = u(1); if (!need(n)) return false; st.push_back(str(b.substr(i, n))); i += n; break; }
            case 'T': case 'X': { if (!need(4)) return false; const uint32_t n = u(4); if (!need(n)) return false; st.push_back(str(b.substr(i, n))); i += n; break; }
            case 'c': { auto v = std::make_shared<V>(); v->t = V::GLOBAL; v->s = line(); v->s += "." + line(); st.push_back(v); break; }
            case 0x93: { if (st.size() < 2) { err = "Short stack"; return false; } auto v = std::make_shared<V>(); v->t = V::GLOBAL; v->s = st[st.size() - 2]->s + "." + st.back()->s; st.resize(st.size() - 2); st.push_back(v); break; }   // STACK_GLOBAL
            case ')': { auto v = std::make_shared<V>(); v->t = V::SEQ; st.push_back(v); break; }
            case ']': { auto v = std::make_shared<V>(); v->t = V::SEQ; st.push_back(v); break; }
            case '}': { auto v = std::make_shared<V>(); v->t = V::DICT; st.push_back(v); break; }
            case '(': { auto v = std::make_shared<V>(); v->t = V::MARK; st.push_back(v); break; }
            case 'q': if (!need(1) || st.empty()) return false; memo[u(1)] = st.back(); break;
            case 'r': if (!need(4) || st.empty()) return false; memo[u(4)] = st.back(); break;
            case 0x94: if (st.empty()) return false; memo[(uint32_t)memo.size()] = st.back(); break;   // MEMOIZE
            case 'h': { if (!need(1)) return false; auto it = memo.find(u(1)); if (it == memo.end()) { err = "Bad memo"; return false; } st.push_back(it->second); break; }
            case 'j': { if (!need(4)) return false; auto it = memo.find(u(4)); if (it == memo.end()) { err = "Bad memo"; return false; } st.push_back(it->second); break; }
            case 0x85: if (!tuple(1)) return false; break;
            case 0x86: if (!tuple(2)) return false; break;
            case 0x87: if (!tuple(3)) return false; break;
            case 't': case 'l': { std::vector<P> items; if (!popMark(items)) return false; auto v = std::make_shared<V>(); v->t = V::SEQ; v->seq = items; st.push_back(v); break; }
            case 'a': { if (st.size() < 2) return false; auto x = st.back(); st.pop_back(); st.back()->seq.push_back(x); break; }
            case 'e': { std::vector<P> items; if (!popMark(items) || st.empty()) return false; for (auto& x : items) st.back()->seq.push_back(x); break; }
            case 's': { if (st.size() < 3) return false; auto v = st.back(); st.pop_back(); auto k = st.back(); st.pop_back(); st.back()->dict.push_back({k, v}); break; }
            case 'u': { std::vector<P> items; if (!popMark(items) || st.empty()) return false; for (size_t k = 0; k + 1 < items.size(); k += 2) st.back()->dict.push_back({items[k], items[k + 1]}); break; }
            case 0x81: {   // NEWOBJ: only MakeHuman's Color is made
                if (st.size() < 2) return false;
                st.pop_back(); auto cls = st.back(); st.pop_back();
                if (cls->t != V::GLOBAL || cls->s != "material.Color") { err = "Unexpected object " + cls->s; return false; }
                auto v = std::make_shared<V>(); v->t = V::COLOR; st.push_back(v); break;
            }
            case 'b': {    // BUILD: the Color's _r, _g, _b
                if (st.size() < 2) return false;
                auto state = st.back(); st.pop_back();
                if (st.back()->t != V::COLOR) { err = "Unexpected build"; return false; }
                for (const auto& kv : state->dict) { const std::string& k = kv.first->s; const int c = k == "_r" ? 0 : k == "_g" ? 1 : k == "_b" ? 2 : -1; if (c >= 0) st.back()->rgb[c] = kv.second->n; }
                break;
            }
            default: { char m[64]; snprintf(m, sizeof m, "Unknown pickle opcode 0x%02x", op); err = m; return false; }
            }
        }
        err = "The file ends early"; return false;
    }
};
bool three(const P& v, double out[3]) {
    if (v->t == V::COLOR) { for (int c = 0; c < 3; ++c) out[c] = v->rgb[c]; return true; }
    if (v->t == V::SEQ && v->seq.size() >= 3) { for (int c = 0; c < 3; ++c) out[c] = v->seq[c]->n; return true; }
    if (v->t == V::NUM) { for (int c = 0; c < 3; ++c) out[c] = v->n; return true; }
    return false;
}

// a float as the shortest decimal that reads back as it (0.3f is written as 0.3, as MakeHuman typed it)
double tidy(float f) {
    char b[48];
    for (int p = 1; p <= 9; ++p) { snprintf(b, sizeof b, "%.*g", p, (double)f); const double d = strtod(b, nullptr); if ((float)d == f) return d; }
    return (double)f;
}

// ---------- the writer: as Python 2's pickle protocol 2 writes MakeHuman's files ----------
struct W {
    std::string o; int memo = 0;
    void begin() { o += "\x80\x02"; memo = 1; }   // Python 2 numbers its memo from 1 here
    void end() { o += '.'; }
    void put() { o += 'q'; o += (char)memo++; }
    void f(double d) { uint64_t x; memcpy(&x, &d, 8); o += 'G'; for (int k = 7; k >= 0; --k) o += (char)((x >> (8 * k)) & 0xff); }
    void i(int n) { if (n >= 0 && n < 256) { o += 'K'; o += (char)n; } else { o += 'J'; for (int k = 0; k < 4; ++k) o += (char)((uint32_t)n >> (8 * k)); } }
    void s(const std::string& t) { if (t.size() < 256) { o += 'U'; o += (char)t.size(); } else { o += 'T'; for (int k = 0; k < 4; ++k) o += (char)((uint32_t)t.size() >> (8 * k)); } o += t; put(); }
    void color(const double c[3]) {
        o += "cmaterial\nColor\n"; put(); o += ")\x81"; put(); o += '}'; put(); o += '(';
        s("_g"); f(c[1]); s("_r"); f(c[0]); s("_b"); f(c[2]);
        o += "ub";
    }
    void tuple3(const double v[3]) { f(v[0]); f(v[1]); f(v[2]); o += '\x87'; put(); }
};
}

bool load(const std::string& path, Scene& sc, std::string& err) {
    std::ifstream in(fs::u8path(path), std::ios::binary);
    if (!in) { err = "Couldn't open " + path; return false; }
    std::ostringstream ss; ss << in.rdbuf(); const std::string bytes = ss.str();
    Reader r(bytes);
    P v;
    auto next = [&](P& out) { if (!r.one(out)) { if (err.empty()) err = path + ": " + r.err; return false; } return true; };
    if (!next(v)) return false;
    if (v->t != V::NUM || (int)v->n < 5) { err = "This scene file is too old (MakeHuman reads version 5)"; return false; }
    if ((int)v->n > 5) { err = "This scene file is newer than version 5"; return false; }
    Scene s;
    // the environment: ambience, skybox
    if (!next(v)) return false; three(v, s.ambience);
    if (!next(v)) return false; s.skybox = v->t == V::STR ? v->s : "";
    if (!next(v)) return false;
    const int n = (int)v->n;
    s.lights.clear();
    for (int k = 0; k < n && k < 64; ++k) {
        Light L;
        // in name order: areaLightSize, areaLights, attenuation, color, focus, fov, position, specular
        if (!next(v)) return false; L.areaLightSize = v->n;
        if (!next(v)) return false; L.areaLights = (int)v->n;
        if (!next(v)) return false; L.attenuation = v->n;
        if (!next(v)) return false; three(v, L.color);
        if (!next(v)) return false; three(v, L.focus);
        if (!next(v)) return false; L.fov = v->n;
        if (!next(v)) return false; three(v, L.position);
        if (!next(v)) return false; three(v, L.specular);
        s.lights.push_back(L);
    }
    sc = s;
    return true;
}

double keep(float f, double was) { return (float)was == f ? was : tidy(f); }

bool save(const Scene& s, const std::string& path, std::string& err) {
    W w;
    w.begin(); w.i(5); w.end();
    w.begin(); w.color(s.ambience); w.end();
    w.begin(); if (s.skybox.empty()) w.o += 'N'; else w.s(s.skybox); w.end();
    w.begin(); w.i((int)s.lights.size()); w.end();
    for (const Light& L : s.lights) {
        w.begin(); w.f(L.areaLightSize); w.end();
        w.begin(); w.i(L.areaLights); w.end();
        w.begin(); w.f(L.attenuation); w.end();
        w.begin(); w.color(L.color); w.end();
        w.begin(); w.tuple3(L.focus); w.end();
        w.begin(); w.f(L.fov); w.end();
        w.begin(); w.tuple3(L.position); w.end();
        w.begin(); w.color(L.specular); w.end();
    }
    std::ofstream o(fs::u8path(path), std::ios::binary);
    if (!o) { err = "Couldn't write " + path; return false; }
    o << w.o;
    return (bool)o;
}

}
