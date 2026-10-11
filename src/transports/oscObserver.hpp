// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#ifndef OSC_OBSERVER_HPP
#define OSC_OBSERVER_HPP

#include <functional>
#include <vector>

class OscControl;

// Watches whether a host's OSC control (Ardour's surface, Carla's OSC) is
// answering, and notifies registered listeners as that changes. The OSC
// counterpart of JackObserver.
//
//   Down        — not requested, or stopped.
//   Waiting     — requested, but the host has not answered; re-probed once per second.
//   Up          — the host answered: transport commands will be acted on.
//   Unavailable — the sockets could not be opened, or this build has no OSC.
//
// While Up nothing is polled: each transport command is followed by a probe, and
// if that goes unanswered the observer drops back to Waiting.
class OscObserver {
public:
    enum class State { Down, Waiting, Up, Unavailable };
    using Listener = std::function<void(State)>;

    explicit OscObserver(OscControl* osc);
    ~OscObserver();

    void addListener(Listener l) { listeners_.push_back(std::move(l)); }

    // Target the host's OSC port on localhost and start probing it.
    void start(int port);
    // Stop probing and close the socket.
    void stop();

    State state() const { return state_; }

private:
    void probe();                         // send a probe and arm the answer deadline
    void answered();                      // the host replied
    void cancelTimer();
    static void deadlineCb(void* self);   // the probe went unanswered
    void setState(State s);               // store + notify listeners (on change)

    OscControl*   osc_;
    State         state_          = State::Down;
    bool          timerScheduled_ = false;
    std::vector<Listener> listeners_;
};

#endif
