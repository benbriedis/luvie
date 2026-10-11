// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#include "oscTransport.hpp"
#include "observableSong.hpp"
#include <FL/Fl.H>
#include <algorithm>
#include <climits>
#include <cstdio>
#include <string>

#ifdef LUVIE_HAVE_OSC
#include <lo/lo.h>
#endif

OscTransport::~OscTransport() { close(); }

#ifdef LUVIE_HAVE_OSC

bool OscTransport::available() { return true; }

bool OscTransport::open(int ardourPort)
{
    close();

    // Quiet: failing to bind the preferred reply port is expected (another Luvie
    // instance, or anything else, may hold it) and is handled by the fallback.
    auto quiet = [](int, const char*, const char*) {};

    lo_server srv = lo_server_new(std::to_string(kDefaultReplyPort).c_str(), quiet);
    if (!srv) srv = lo_server_new(nullptr, quiet);   // OS-assigned: needs Ardour's Auto mode
    if (!srv) {
        fprintf(stderr, "[osc] failed to open a reply socket\n");
        return false;
    }
    server_    = srv;
    replyPort_ = lo_server_get_port(srv);

    lo_server_add_method(srv, "/transport_frame", nullptr,
        (lo_method_handler)cbTransportFrame, this);
    // Anything else Ardour sends (feedback the user may have enabled by default for
    // new surfaces) is accepted and ignored.
    lo_server_add_method(srv, nullptr, nullptr,
        [](const char*, const char*, lo_arg**, int, lo_message, void*) { return 0; }, nullptr);

    address_ = lo_address_new("127.0.0.1", std::to_string(ardourPort).c_str());

    sockFd_ = lo_server_get_socket_fd(srv);
    if (sockFd_ >= 0)
        Fl::add_fd(sockFd_, FL_READ, fdReadable, this);
    return true;
}

void OscTransport::close()
{
    if (sockFd_ >= 0) Fl::remove_fd(sockFd_);
    sockFd_ = -1;
    if (server_)  lo_server_free((lo_server)server_);
    if (address_) lo_address_free((lo_address)address_);
    server_    = nullptr;
    address_   = nullptr;
    replyPort_ = 0;
}

// Every message goes out from the reply socket (lo_send_from), so in Ardour's Auto
// port mode its answer comes back to the socket we are watching.
void OscTransport::probe()
{
    if (!server_) return;
    lo_send_from((lo_address)address_, (lo_server)server_, LO_TT_IMMEDIATE,
                 "/transport_frame", "");
}

void OscTransport::play()
{
    if (!server_) return;
    lo_send_from((lo_address)address_, (lo_server)server_, LO_TT_IMMEDIATE,
                 "/transport_play", "");
    if (onCommandSent) onCommandSent();
}

void OscTransport::pause()
{
    if (!server_) return;
    lo_send_from((lo_address)address_, (lo_server)server_, LO_TT_IMMEDIATE,
                 "/transport_stop", "");
    if (onCommandSent) onCommandSent();
}

void OscTransport::locate(long long sample)
{
    if (!server_) return;
    // Ardour's /locate takes (int32 sample, int32 roll). roll = 1 keeps a rolling
    // transport rolling. roll = 0 is "roll if appropriate", which for a locate means
    // only if Ardour's Auto Play preference is on, so it would stop a playing
    // transport; it is the right flag only when stopped.
    const int pos  = static_cast<int>(std::clamp<long long>(sample, 0, INT_MAX));
    const int roll = (isRolling && isRolling()) ? 1 : 0;
    lo_send_from((lo_address)address_, (lo_server)server_, LO_TT_IMMEDIATE,
                 "/locate", "ii", pos, roll);
    if (onCommandSent) onCommandSent();
}

void OscTransport::fdReadable(int /*fd*/, void* ud)
{
    auto* self = static_cast<OscTransport*>(ud);
    while (self->server_ && lo_server_recv_noblock((lo_server)self->server_, 0) > 0) {}
}

int OscTransport::cbTransportFrame(const char*, const char*, void*[], int, void*, void* ud)
{
    auto* self = static_cast<OscTransport*>(ud);
    if (self->onReply) self->onReply();
    return 0;
}

#else   // no liblo: OSC control is unavailable and every command is a no-op

bool OscTransport::available()      { return false; }
bool OscTransport::open(int)        { return false; }
void OscTransport::close()          {}
void OscTransport::probe()          {}
void OscTransport::play()           {}
void OscTransport::pause()          {}
void OscTransport::locate(long long) {}
void OscTransport::fdReadable(int, void*) {}
int  OscTransport::cbTransportFrame(const char*, const char*, void*[], int, void*, void*) { return 0; }

#endif

void OscTransport::rewind() { locate(0); }

void OscTransport::seek(float bars)
{
    if (!timeline_ || sampleRate_ <= 0.0) return;
    const double secs = timeline_->barToSeconds(std::max(0.0f, bars));
    locate(static_cast<long long>(secs * sampleRate_));
}
