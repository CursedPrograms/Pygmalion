// Pygmalion - MakeHuman's socket server (plugins/8_server_socket, AGPL-3.0): a client (Blender's MPFB) connects,
// sends one JSON call {"function", "params", "data", "error"} and gets the answer back - JSON, or raw bytes for the
// mesh arrays - and the connection closes. The listening runs on its own thread; calls are answered on the main
// thread (poll), as MakeHuman's worker thread hands them to the GUI.
#pragma once
#include <functional>
#include <string>
#include <vector>

namespace Sock {

struct Answer {
    std::string bytes;      // what's sent back (JSON text, or binary)
};

// the call as text; answer() fills the reply
using Handler = std::function<void(const std::string& call, Answer& reply)>;

bool start(const std::string& host, int port, std::string& err);
void stop();
bool running();
// answers the calls waiting (call once a frame, on the main thread)
void poll(const Handler& h);
// what happened, for the Socket tab (and cleared by it)
std::vector<std::string> takeMessages();

}
