// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#ifndef CARLA_OSC_TRANSPORT_HPP
#define CARLA_OSC_TRANSPORT_HPP

#include "oscControl.hpp"
#include <thread>

class ObservableSong;

/*
 * CarlaOscTransport — the OscControl for Carla: drives Carla's transport through
 * the remote-control OSC interface that carla-control uses (Settings > Configure
 * Carla > OSC). On by default on Linux; off on Windows / macOS, where it is most
 * wanted.
 *
 * Carla's protocol, as checked against Carla 2.5.10:
 *   - Commands go over TCP: /ctrl/transport_play, /ctrl/transport_pause and
 *     /ctrl/transport_relocate (int64 frame), each led by an int32 message id.
 *     A relocate keeps a rolling transport rolling and a stopped one stopped.
 *   - Carla only acts on them once a client has sent /register <reply URL>.
 *     It then sends that client a burst of /ctrl/cb state, and answers every
 *     command with /ctrl/resp (id, error text) — always over TCP, to a connection
 *     it opens itself.
 *   - It keeps one registration. Ownership is checked by IP only, so a second
 *     local client's commands still work but its answers go to the first.
 *
 * So liveness is judged on the TCP side: Carla is up when a /register (or a
 * command) can be sent to its port. Carla's answers only tell us whether we hold
 * the registration; we re-register before a command when we do not, so commands
 * keep working if another client (or another Luvie) registers and later leaves.
 *
 * Threading. A TCP connect can block — on Windows a refused localhost connect takes
 * about two seconds — and Carla's replies arrive on connections it opens to us,
 * which FLTK cannot watch. So one I/O thread owns everything that talks to Carla:
 * it blocks in lo_servers_wait() on a UDP command socket and the TCP reply server
 * (liblo caps that wait at 100 s, so it is not a polling loop). The UI thread
 * sends it commands as datagrams to that UDP socket, and it reports back the same
 * way, to a UDP socket the UI watches with Fl::add_fd — as OscTransport does —
 * so onReply fires on the UI thread with nothing to marshal and nothing left
 * pending once close() returns.
 */
class CarlaOscTransport : public OscControl {
public:
    static constexpr int kDefaultCarlaPort = 22752;

    CarlaOscTransport() = default;
    ~CarlaOscTransport() override;

    void setTimeline(ObservableSong* tl) { timeline_ = tl; }   // for seek() bar -> frame
    // The host's sample rate, needed to turn a bar into the frame a relocate takes.
    // Seeks are dropped until it is known.
    void setSampleRate(double sr) { sampleRate_ = sr; }

    bool open(int carlaPort) override;
    void close() override;
    bool isOpen() const override { return uiSrv_ != nullptr; }

    // Register with Carla (if not already) and report whether it can be reached.
    void probe() override;

    // ITransport — called from the UI thread
    void play()           override;
    void pause()          override;
    void rewind()         override;
    void seek(float bars) override;

private:
    // ── UI thread ─────────────────────────────────────────────────────────────
    void* uiSrv_  = nullptr;   // lo_server (UDP): reports from the I/O thread
    void* ioAddr_ = nullptr;   // lo_address of the I/O thread's command socket
    int   uiFd_   = -1;        // uiSrv_'s fd, registered with Fl::add_fd
    void  toIo(const char* path);
    void  locate(long long frame);
    static void fdReadable(int fd, void* ud);

    // ── I/O thread (created by open(), used only by run() until it is joined) ──
    void* ioSrv_     = nullptr;   // lo_server (UDP): commands from the UI
    void* replySrv_  = nullptr;   // lo_server (TCP): Carla's answers
    void* carlaAddr_ = nullptr;   // lo_address (TCP) of Carla's OSC port
    void* uiAddr_    = nullptr;   // lo_address of uiSrv_
    char  replyUrl_[64] = {};
    bool  quit_      = false;
    bool  owner_     = false;     // Carla is answering us: we hold the registration
    bool  lastSendOk_ = false;
    bool  unanswered_ = false;    // a command was sent and Carla has not answered since
    int   nextId_    = 1;
    std::thread thread_;

    void run();
    bool sendRegister();
    void command(const char* path, bool withFrame, long long frame);
    void reachable();             // tell the UI Carla took a message

    ObservableSong* timeline_   = nullptr;
    double          sampleRate_ = 0.0;
};

#endif
