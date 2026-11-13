// Pygmalion - Utilities > Profile (MakeHuman's 7_profile, which shows cProfile's numbers): Pygmalion's main steps
// timed as they run - calls, their own time, and the time with the steps they call ("cumulative")
#pragma once
#include <chrono>
#include <map>
#include <string>
#include <vector>

namespace Prof {

struct Stat { long long calls = 0; double total = 0, cumul = 0; };   // seconds: own time, and with the steps inside
inline bool on = true;
inline std::map<std::string, Stat>& stats() { static std::map<std::string, Stat> s; return s; }
inline std::vector<double>& childTime() { static std::vector<double> c; return c; }   // per open scope: time spent in its children

// PROF("name") at the top of a step
struct Scope {
    const char* name; std::chrono::steady_clock::time_point t0;
    explicit Scope(const char* n) : name(n), t0(std::chrono::steady_clock::now()) { if (on) childTime().push_back(0); }
    ~Scope() {
        if (!on || childTime().empty()) return;
        const double dt = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        const double kids = childTime().back(); childTime().pop_back();
        Stat& s = stats()[name]; ++s.calls; s.cumul += dt; s.total += dt - kids;
        if (!childTime().empty()) childTime().back() += dt;
    }
};
inline void reset() { stats().clear(); }

}
#define PROF_CAT2(a, b) a##b
#define PROF_CAT(a, b) PROF_CAT2(a, b)
#define PROF(name) Prof::Scope PROF_CAT(prof_, __LINE__)(name)
