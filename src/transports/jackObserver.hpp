// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#ifndef JACK_OBSERVER_HPP
#define JACK_OBSERVER_HPP

#include <functional>
#include <memory>
#include <string>
#include <vector>

class JackTransport;

// Watches for the JACK server becoming available and notifies registered
// listeners (observer pattern) as its state changes.
//
//   Down    — not requested, or polling cancelled.
//   Polling — JACK requested but not yet available; retried once per second.
//   Up       — JACK is open and ready to use.
//
// If the server disappears while Up (detected via JackTransport's shutdown
// callback), serverLost() tears down the dead client and drops back to Polling
// so the observer reconnects automatically when a server reappears. Switching
// the UI away from JACK simply stops the observer rather than tearing it down.
//
// Connecting never blocks the UI thread for long. jack_client_open() runs on a
// short-lived worker thread; the UI thread waits briefly for it (so a healthy
// server comes Up synchronously, just as before) and otherwise carries on in
// Polling while an FLTK timer collects the result. That matters when the server is
// wedged rather than absent — a hung PipeWire daemon accepts the connection and then
// never answers, so jack_client_open() never returns. Before, that froze the whole
// app at startup, before it could even announce itself to a session manager. Now
// the app runs without JACK, serverUnresponsive() turns true after a few seconds,
// and the client comes Up by itself if the server ever answers. Only one connect is
// in flight at a time, so a wedged server costs one stuck thread, not one a second.
class JackObserver {
public:
    enum class State { Down, Polling, Up };
    using Listener = std::function<void(State)>;

    explicit JackObserver(JackTransport* jack);
    ~JackObserver();

    void addListener(Listener l) { listeners_.push_back(std::move(l)); }

    // Request JACK: opens it immediately if the server is running, otherwise
    // begins a once-per-second poll until it appears.
    void start();
    // Stop requesting JACK / cancel any in-flight poll.
    void stop();
    // Stop requesting JACK and close the client if one is open, ending in Down.
    // For when JACK is no longer wanted at all (unlike stop(), which keeps an open
    // client so a later start() can re-attach to it).
    void release();

    // The JACK server went away while we were Up: close the dead client and
    // resume polling so we reconnect when a server returns. Call on the UI
    // thread (wired to JackTransport::onShutdown).
    void serverLost();

    State state() const { return state_; }

    // A connection attempt has been waiting on the server for several seconds: the
    // server is there but not answering (as opposed to not running). Listeners are
    // re-notified with the current state whenever this changes.
    bool serverUnresponsive() const { return unresponsive_; }

    // What to tell the user while serverUnresponsive(): one popup line each. Names
    // PipeWire, and how to restart it, when that is the server in use. UI thread.
    static std::vector<std::string> unresponsiveAdvice();

    // How many connects are still stuck in jack_client_open() after their observer
    // was destroyed. While any are, the process must not run libjack's exit-time
    // finalizers: PipeWire's pw_deinit() destroys a condition variable the stuck
    // thread is waiting on, which blocks forever. The standalone app checks this on
    // the way out and skips them with _exit(). (A plugin must never do that to its
    // host; it simply leaves the thread behind.)
    static int strandedConnects();

private:
    struct Pending;                       // one in-flight connect (see .cpp)

    void attempt();                       // try to connect once; poll if it fails
    void finish(struct _jack_client* client, bool wanted);   // UI thread
    void schedulePoll();
    void scheduleCheck(double seconds);
    static void timerCb(void* self);
    static void checkCb(void* self);      // collects a slow connect's result
    void setState(State s);               // store + notify listeners

    JackTransport* jack_;
    State          state_          = State::Down;
    bool           up_             = false;  // JACK is currently open
    bool           timerScheduled_ = false;
    bool           checkScheduled_ = false;
    bool           unresponsive_   = false;
    std::shared_ptr<Pending> pending_;       // non-null while a connect is in flight
    std::vector<Listener> listeners_;
};

#endif
