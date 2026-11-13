// Pygmalion - MakeHuman's socket server: the listening thread (see socket.h)
#include "socket.h"

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>

#include <algorithm>
#include <atomic>
#include <deque>
#include <mutex>
#include <thread>

namespace Sock {

namespace {
struct Call { SOCKET conn; std::string text; };
std::mutex mu;
std::deque<Call> calls;
std::vector<std::string> messages;
std::thread worker;
std::atomic<bool> exiting{false}, up{false};
SOCKET listener = INVALID_SOCKET;

void message(const std::string& m) { std::lock_guard<std::mutex> l(mu); messages.push_back(m); if (messages.size() > 2000) messages.erase(messages.begin()); }

// a call is one JSON object: read until its braces close (MakeHuman reads one 8 KB piece; this also takes longer calls)
std::string readCall(SOCKET c) {
    std::string t; char buf[8192]; int depth = 0; bool inStr = false, esc = false, started = false;
    for (;;) {
        const int n = recv(c, buf, sizeof buf, 0);
        if (n <= 0) break;
        for (int i = 0; i < n; ++i) {
            const char ch = buf[i]; t.push_back(ch);
            if (inStr) { if (esc) esc = false; else if (ch == '\\') esc = true; else if (ch == '"') inStr = false; continue; }
            if (ch == '"') inStr = true;
            else if (ch == '{') { ++depth; started = true; }
            else if (ch == '}' && --depth == 0 && started) return t;
        }
        if (t.size() > (64u << 20)) break;
    }
    return t;
}

void run() {
    message("Waiting for connection.");
    while (!exiting) {
        sockaddr_in from{}; int len = sizeof from;
        SOCKET c = accept(listener, (sockaddr*)&from, &len);
        if (c == INVALID_SOCKET) { if (exiting) break; continue; }
        char ip[64] = "?"; inet_ntop(AF_INET, &from.sin_addr, ip, sizeof ip);
        message(std::string("Connected with ") + ip + ":" + std::to_string(ntohs(from.sin_port)));
        std::string text = readCall(c);
        message("Client says: '" + (text.size() > 300 ? text.substr(0, 300) + "..." : text) + "'");
        std::lock_guard<std::mutex> l(mu);
        calls.push_back({c, std::move(text)});
    }
}
}

bool start(const std::string& host, int port, std::string& err) {
    stop();
    static bool wsa = false;
    if (!wsa) { WSADATA d; if (WSAStartup(MAKEWORD(2, 2), &d) != 0) { err = "Winsock didn't start"; return false; } wsa = true; }
    message("Opening server socket... ");
    listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == INVALID_SOCKET) { err = "No socket"; return false; }
    BOOL yes = TRUE; setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof yes);
    sockaddr_in a{}; a.sin_family = AF_INET; a.sin_port = htons((u_short)port);
    if (inet_pton(AF_INET, host.c_str(), &a.sin_addr) != 1) { err = "Not an address: " + host; closesocket(listener); listener = INVALID_SOCKET; return false; }
    if (bind(listener, (sockaddr*)&a, sizeof a) != 0 || listen(listener, 10) != 0) {
        err = "Bind failed (error " + std::to_string(WSAGetLastError()) + "): is the port in use?";
        message(err); closesocket(listener); listener = INVALID_SOCKET; return false;
    }
    message("Opened on host " + host + "\nOpened at port " + std::to_string(port));
    exiting = false; up = true;
    worker = std::thread(run);
    return true;
}

void stop() {
    if (!up) return;
    message("Stopping socket connection");
    exiting = true;
    closesocket(listener); listener = INVALID_SOCKET;
    if (worker.joinable()) worker.join();
    std::lock_guard<std::mutex> l(mu);
    for (auto& c : calls) closesocket(c.conn);
    calls.clear();
    up = false;
}

bool running() { return up; }

void poll(const Handler& h) {
    for (;;) {
        Call c;
        { std::lock_guard<std::mutex> l(mu); if (calls.empty()) return; c = std::move(calls.front()); calls.pop_front(); }
        Answer a;
        h(c.text, a);
        size_t at = 0;
        while (at < a.bytes.size()) {
            const int n = send(c.conn, a.bytes.data() + at, (int)std::min<size_t>(a.bytes.size() - at, 1 << 20), 0);
            if (n <= 0) break;
            at += (size_t)n;
        }
        shutdown(c.conn, SD_SEND);
        closesocket(c.conn);
    }
}

std::vector<std::string> takeMessages() { std::lock_guard<std::mutex> l(mu); std::vector<std::string> m; m.swap(messages); return m; }

}
