#include "nsm.hpp"
#include <lo/lo.h>
#include <FL/Fl.H>
#include <cstdlib>
#include <cstdio>
#include <unistd.h>
#include <string>

// ── Construction / destruction ────────────────────────────────────────────────

NsmClient::NsmClient() = default;

NsmClient::~NsmClient() {
    if (sockFd_ >= 0) Fl::remove_fd(sockFd_);
    if (server_)  lo_server_free((lo_server)server_);
    if (nsmAddr_) lo_address_free((lo_address)nsmAddr_);
}

// ── init ──────────────────────────────────────────────────────────────────────

bool NsmClient::init(const char* appName, const char* exe) {
    const char* url = std::getenv("NSM_URL");
    if (!url) {
        fprintf(stderr, "[nsm] NSM_URL not set — running without NSM\n");
        return false;
    }
    fprintf(stderr, "[nsm] NSM_URL = %s\n", url);

    // Create OSC server on a random port (NULL = OS-assigned).
    auto errHandler = [](int num, const char* msg, const char* path) {
        fprintf(stderr, "[nsm] liblo error %d in %s: %s\n", num, path ? path : "?", msg ? msg : "");
    };
    lo_server srv = lo_server_new(nullptr, errHandler);
    if (!srv) {
        fprintf(stderr, "[nsm] Failed to create lo_server\n");
        return false;
    }
    server_ = srv;

    // Register method handlers.
    lo_server_add_method(srv, "/nsm/client/open", "sss",
        (lo_method_handler)cbOpen, this);
    lo_server_add_method(srv, "/nsm/client/save", "",
        (lo_method_handler)cbSave, this);
    // Catch /reply messages (e.g. the announce reply) — wildcard type.
    lo_server_add_method(srv, "/reply", nullptr,
        (lo_method_handler)cbReply, this);

    // Connect to the NSM server address.
    lo_address addr = lo_address_new_from_url(url);
    if (!addr) {
        fprintf(stderr, "[nsm] Failed to parse NSM_URL: %s\n", url);
        return false;
    }
    nsmAddr_ = addr;

    // Register the server's socket fd with FLTK so we're woken only when
    // NSM actually sends a message — no timer or polling needed.
    sockFd_ = lo_server_get_socket_fd(srv);
    if (sockFd_ >= 0)
        Fl::add_fd(sockFd_, FL_READ, fdReadable, this);

    lo_send_from(addr, srv, LO_TT_IMMEDIATE,
        "/nsm/server/announce",
        "sssiii",
        appName,          // human-readable name
        "",               // capabilities (none claimed yet)
        exe,              // executable name
        1,                // API major version
        2,                // API minor version
        (int)getpid());
    active_ = true;
    return true;
}

// ── fd callback ───────────────────────────────────────────────────────────────

void NsmClient::fdReadable(int /*fd*/, void* ud) {
    auto* self = (NsmClient*)ud;
    // Drain all pending messages (there may be more than one).
    while (lo_server_recv_noblock((lo_server)self->server_, 0) > 0) {}
}

// ── Helpers ───────────────────────────────────────────────────────────────────

void NsmClient::replyOk(const char* addr) {
    if (!nsmAddr_ || !server_) return;
    lo_send_from((lo_address)nsmAddr_, (lo_server)server_,
        LO_TT_IMMEDIATE, "/reply", "ss", addr, "OK");
}

void NsmClient::replyError(const char* addr, int code, const char* message) {
    if (!nsmAddr_ || !server_) return;
    lo_send_from((lo_address)nsmAddr_, (lo_server)server_,
        LO_TT_IMMEDIATE, "/error", "sis", addr, code, message);
}

// ── OSC handlers ─────────────────────────────────────────────────────────────

int NsmClient::cbOpen(const char* /*oscPath*/, const char* /*types*/,
                      void* argv[], int /*argc*/, void* /*msg*/, void* ud)
{
    auto* self = (NsmClient*)ud;
    auto** args = (lo_arg**)argv;
    std::string path        = &args[0]->s;
    std::string displayName = &args[1]->s;

    bool ok = self->onOpen && self->onOpen(path, displayName);
    if (ok)
        self->replyOk("/nsm/client/open");
    else
        self->replyError("/nsm/client/open", -1, "open failed");
    return 0;
}

int NsmClient::cbSave(const char* /*oscPath*/, const char* /*types*/,
                      void* /*argv*/[], int /*argc*/, void* /*msg*/, void* ud)
{
    auto* self = (NsmClient*)ud;
    bool ok = self->onSave && self->onSave();
    if (ok)
        self->replyOk("/nsm/client/save");
    else
        self->replyError("/nsm/client/save", -1, "save failed");
    return 0;
}

int NsmClient::cbReply(const char* /*oscPath*/, const char* /*types*/,
                       void* argv[], int argc, void* /*msg*/, void* /*ud*/)
{
    // The first string argument is the address this is a reply to.
    if (argc < 1) return 0;
    auto** args = (lo_arg**)argv;
    const char* replyTo = &args[0]->s;
    if (std::string(replyTo) == "/nsm/server/announce") {
        const char* welcome = (argc >= 2) ? &args[1]->s : "";
        printf("[nsm] Connected to session manager: %s\n", welcome);
    }
    return 0;
}
