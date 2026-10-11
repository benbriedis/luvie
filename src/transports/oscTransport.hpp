// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#ifndef OSC_TRANSPORT_HPP
#define OSC_TRANSPORT_HPP

#include "oscControl.hpp"
#include <functional>

class ObservableSong;

/*
 * OscTransport — the OscControl for Ardour: drives Ardour's transport through
 * Ardour's OSC control surface (Preferences > Control Surfaces > Novation >
 * Open Sound Control). Used by the LV2 UI when no JACK server is available to
 * carry transport commands (typically Ardour on Windows / macOS).
 *
 * Ardour's surface must be enabled; it listens on UDP 3819 by default. Ardour only
 * answers queries, so liveness is checked with probe(): a /transport_frame query
 * whose answer fires onReply. Ardour sends that answer to the sender's port when
 * its Port Mode is "Auto", or to its "Reply Manual Port" (default 8000) when it is
 * "Manual" (the default). We therefore listen on 8000 if we can, which works with
 * both modes, and fall back to an OS-assigned port, which then needs Auto.
 *
 * Everything runs on the UI thread: the reply socket is watched with Fl::add_fd,
 * so no thread or timer is needed here.
 */
class OscTransport : public OscControl {
public:
    static constexpr int kDefaultArdourPort = 3819;
    static constexpr int kDefaultReplyPort  = 8000;   // Ardour's default manual reply port

    OscTransport() = default;
    ~OscTransport() override;

    void setTimeline(ObservableSong* tl) { timeline_ = tl; }   // for seek() bar -> frame
    // The host's sample rate, needed to turn a bar into the sample position /locate
    // takes. Seeks are dropped until it is known.
    void setSampleRate(double sr) { sampleRate_ = sr; }

    // Target Ardour on localhost:ardourPort and open the reply socket. Reopens if
    // already open. False when the socket cannot be created or this build has no
    // OSC support (see available()).
    bool open(int ardourPort) override;
    void close() override;
    bool isOpen() const override { return server_ != nullptr; }

    // Whether this build includes OSC support (liblo).
    static bool available();

    // The port the reply socket listens on, or 0 when closed.
    int  replyPort() const { return replyPort_; }

    // Ask Ardour for its transport position. An answer fires onReply.
    void probe() override;

    // Whether the host is rolling, as last reported by the DSP. A locate passes it on
    // as /locate's roll flag: Ardour's roll = 0 means "roll only if Auto Play is on",
    // so without it a seek while playing would stop the transport.
    std::function<bool()> isRolling;

    // ITransport — called from the UI thread
    void  play()             override;
    void  pause()            override;
    void  rewind()           override;
    void  seek(float bars)   override;

private:
    void* server_   = nullptr;   // lo_server (opaque, so no liblo header leaks)
    void* address_  = nullptr;   // lo_address of Ardour's OSC surface
    int   sockFd_   = -1;        // fd registered with Fl::add_fd
    int   replyPort_ = 0;

    ObservableSong* timeline_   = nullptr;
    double          sampleRate_ = 0.0;

    void locate(long long sample);
    static void fdReadable(int fd, void* ud);
    static int  cbTransportFrame(const char* path, const char* types,
                                 void* argv[], int argc, void* msg, void* ud);
};

#endif
