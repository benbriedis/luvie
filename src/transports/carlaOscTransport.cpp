// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#include "carlaOscTransport.hpp"
#include "observableSong.hpp"
#include <FL/Fl.H>
#include <algorithm>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#ifdef LUVIE_HAVE_OSC
#include <lo/lo.h>
#endif

CarlaOscTransport::~CarlaOscTransport() { close(); }

#ifdef LUVIE_HAVE_OSC

namespace {
bool endsWith(const char* s, const char* suffix)
{
    const size_t n = std::strlen(s), m = std::strlen(suffix);
    return n >= m && std::strcmp(s + n - m, suffix) == 0;
}

lo_address localhost(lo_server srv)
{
    return lo_address_new("127.0.0.1", std::to_string(lo_server_get_port(srv)).c_str());
}
}

bool CarlaOscTransport::open(int carlaPort)
{
    close();

    auto quiet = [](int, const char*, const char*) {};
    lo_server ui    = lo_server_new_with_proto(nullptr, LO_UDP, quiet);
    lo_server io    = lo_server_new_with_proto(nullptr, LO_UDP, quiet);
    lo_server reply = lo_server_new_with_proto(nullptr, LO_TCP, quiet);
    if (!ui || !io || !reply) {
        fprintf(stderr, "[osc] failed to open sockets for Carla\n");
        for (lo_server s : {ui, io, reply}) if (s) lo_server_free(s);
        return false;
    }
    uiSrv_     = ui;
    ioSrv_     = io;
    replySrv_  = reply;
    ioAddr_    = localhost(io);
    uiAddr_    = localhost(ui);
    carlaAddr_ = lo_address_new_with_proto(LO_TCP, "127.0.0.1",
                                           std::to_string(carlaPort).c_str());
    snprintf(replyUrl_, sizeof replyUrl_, "osc.tcp://127.0.0.1:%d/ctrl",
             lo_server_get_port(reply));

    // UI side: the I/O thread's reports.
    lo_server_add_method(ui, "/reachable", nullptr,
        [](const char*, const char*, lo_arg**, int, lo_message, void* ud) {
            auto* self = static_cast<CarlaOscTransport*>(ud);
            if (self->onReply) self->onReply();
            return 0;
        }, this);
    uiFd_ = lo_server_get_socket_fd(ui);
    if (uiFd_ >= 0) Fl::add_fd(uiFd_, FL_READ, fdReadable, this);

    // I/O side: commands from the UI ...
    lo_server_add_method(io, nullptr, nullptr,
        [](const char* path, const char* types, lo_arg** argv, int, lo_message, void* ud) {
            auto* self = static_cast<CarlaOscTransport*>(ud);
            if      (!std::strcmp(path, "/play"))  self->command("/ctrl/transport_play",  false, 0);
            else if (!std::strcmp(path, "/pause")) self->command("/ctrl/transport_pause", false, 0);
            else if (!std::strcmp(path, "/locate") && !std::strcmp(types, "h"))
                self->command("/ctrl/transport_relocate", true, argv[0]->h);
            else if (!std::strcmp(path, "/probe")) {
                // Up, a probe follows each command, which has just shown whether
                // Carla can be reached; otherwise registering is the test.
                if (self->lastSendOk_ || self->sendRegister())
                    self->reachable();
            }
            else if (!std::strcmp(path, "/quit")) self->quit_ = true;
            return 0;
        }, this);

    // ... and Carla's answers. Only the registered client gets any, so any message
    // at all means the registration is ours.
    lo_server_add_method(reply, nullptr, nullptr,
        [](const char* path, const char* types, lo_arg** argv, int argc, lo_message, void* ud) {
            auto* self = static_cast<CarlaOscTransport*>(ud);
            self->owner_       = true;
            self->unanswered_  = false;
            if (endsWith(path, "/exit") || endsWith(path, "/exit-error"))
                self->owner_ = false;   // Carla is shutting down
            else if (endsWith(path, "/resp") && argc == 2 && !std::strcmp(types, "is")
                     && (&argv[1]->s)[0])   // a non-empty error text
                fprintf(stderr, "[osc] Carla: %s\n", &argv[1]->s);
            return 0;
        }, this);

    quit_       = false;
    owner_      = false;
    lastSendOk_ = false;
    unanswered_ = false;
    thread_ = std::thread([this] { run(); });
    return true;
}

void CarlaOscTransport::close()
{
    if (thread_.joinable()) {
        toIo("/quit");
        thread_.join();
    }
    if (uiFd_ >= 0) Fl::remove_fd(uiFd_);
    uiFd_ = -1;
    for (void** s : {&uiSrv_, &ioSrv_, &replySrv_})
        if (*s) { lo_server_free((lo_server)*s); *s = nullptr; }
    for (void** a : {&ioAddr_, &uiAddr_, &carlaAddr_})
        if (*a) { lo_address_free((lo_address)*a); *a = nullptr; }
}

// ── UI thread ─────────────────────────────────────────────────────────────────

void CarlaOscTransport::toIo(const char* path)
{
    if (!uiSrv_) return;
    lo_send_from((lo_address)ioAddr_, (lo_server)uiSrv_, LO_TT_IMMEDIATE, path, "");
}

void CarlaOscTransport::probe() { toIo("/probe"); }

void CarlaOscTransport::play()
{
    if (!uiSrv_) return;
    toIo("/play");
    if (onCommandSent) onCommandSent();
}

void CarlaOscTransport::pause()
{
    if (!uiSrv_) return;
    toIo("/pause");
    if (onCommandSent) onCommandSent();
}

void CarlaOscTransport::locate(long long frame)
{
    if (!uiSrv_) return;
    lo_send_from((lo_address)ioAddr_, (lo_server)uiSrv_, LO_TT_IMMEDIATE,
                 "/locate", "h", (int64_t)std::max(0LL, frame));
    if (onCommandSent) onCommandSent();
}

void CarlaOscTransport::fdReadable(int, void* ud)
{
    auto* self = static_cast<CarlaOscTransport*>(ud);
    while (self->uiSrv_ && lo_server_recv_noblock((lo_server)self->uiSrv_, 0) > 0) {}
}

// ── I/O thread ────────────────────────────────────────────────────────────────

void CarlaOscTransport::run()
{
    lo_server srvs[2] = {(lo_server)ioSrv_, (lo_server)replySrv_};
    while (!quit_) {
        lo_servers_wait(srvs, nullptr, 2, INT_MAX);
        while (!quit_ && lo_server_recv_noblock(srvs[0], 0) > 0) {}
        while (lo_server_recv_noblock(srvs[1], 0) > 0) {}
    }
    // Hand the registration back, so another client can take it. Only if it is
    // ours: Carla checks the sender's address, not the URL, so this would
    // otherwise unregister whoever does hold it.
    if (owner_)
        lo_send((lo_address)carlaAddr_, "/unregister", "s", replyUrl_);
}

bool CarlaOscTransport::sendRegister()
{
    lastSendOk_ = lo_send((lo_address)carlaAddr_, "/register", "s", replyUrl_) >= 0;
    if (!lastSendOk_) owner_ = false;
    return lastSendOk_;
}

void CarlaOscTransport::command(const char* path, bool withFrame, long long frame)
{
    // The last command was never answered, so its answer went elsewhere: someone
    // else unregistered us (Carla checks only the address), or took over. Register
    // again rather than send into the void.
    if (unanswered_) owner_ = false;
    // Two tries: after Carla restarts, the first send finds the old connection
    // dead (liblo then drops it) and the second reconnects and re-registers.
    for (int attempt = 0; attempt < 2; ++attempt) {
        if (!owner_ && !sendRegister()) continue;
        lo_message m = lo_message_new();
        lo_message_add_int32(m, nextId_++);
        if (withFrame) lo_message_add_int64(m, frame);
        lastSendOk_ = lo_send_message((lo_address)carlaAddr_, path, m) >= 0;
        lo_message_free(m);
        if (lastSendOk_) { unanswered_ = true; return; }
        owner_ = false;
    }
}

void CarlaOscTransport::reachable()
{
    lo_send_from((lo_address)uiAddr_, (lo_server)ioSrv_, LO_TT_IMMEDIATE, "/reachable", "");
}

#else   // no liblo: Carla control is unavailable and every command is a no-op

bool CarlaOscTransport::open(int)             { return false; }
void CarlaOscTransport::close()               {}
void CarlaOscTransport::toIo(const char*)     {}
void CarlaOscTransport::probe()               {}
void CarlaOscTransport::play()                {}
void CarlaOscTransport::pause()               {}
void CarlaOscTransport::locate(long long)     {}
void CarlaOscTransport::fdReadable(int, void*) {}
void CarlaOscTransport::run()                 {}
bool CarlaOscTransport::sendRegister()        { return false; }
void CarlaOscTransport::command(const char*, bool, long long) {}
void CarlaOscTransport::reachable()           {}

#endif

void CarlaOscTransport::rewind() { locate(0); }

void CarlaOscTransport::seek(float bars)
{
    if (!timeline_ || sampleRate_ <= 0.0) return;
    const double secs = timeline_->barToSeconds(std::max(0.0f, bars));
    locate(static_cast<long long>(secs * sampleRate_));
}
