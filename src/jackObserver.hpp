#ifndef JACK_OBSERVER_HPP
#define JACK_OBSERVER_HPP

#include <functional>
#include <vector>

class JackTransport;

// Watches for the JACK server becoming available and notifies registered
// listeners (observer pattern) as its state changes.
//
//   Down    — not requested, or polling cancelled.
//   Polling — JACK requested but not yet available; retried once per second.
//   Up       — JACK is open and ready to use.
//
// Once JACK has come up it stays "up" for the life of the process; switching
// the UI away from JACK simply stops the observer rather than tearing it down.
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

    State state() const { return state_; }

private:
    void attempt();                       // try open() once; poll if it fails
    void schedulePoll();
    static void timerCb(void* self);
    void setState(State s);               // store + notify listeners

    JackTransport* jack_;
    State          state_          = State::Down;
    bool           up_             = false;  // open() has succeeded at least once
    bool           timerScheduled_ = false;
    std::vector<Listener> listeners_;
};

#endif
