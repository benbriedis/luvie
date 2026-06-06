#include "jackObserver.hpp"
#include "jackTransport.hpp"
#include <FL/Fl.H>

JackObserver::JackObserver(JackTransport* jack) : jack_(jack) {}

JackObserver::~JackObserver() {
    if (timerScheduled_) Fl::remove_timeout(timerCb, this);
}

void JackObserver::start() {
    if (up_) {                 // already have JACK — re-notify so clients re-attach
        setState(State::Up);
        return;
    }
    attempt();
}

void JackObserver::stop() {
    if (timerScheduled_) {
        Fl::remove_timeout(timerCb, this);
        timerScheduled_ = false;
    }
    if (state_ == State::Polling) setState(State::Down);
}

void JackObserver::attempt() {
    if (jack_->open()) {
        up_ = true;
        setState(State::Up);
    } else {
        setState(State::Polling);
        schedulePoll();
    }
}

void JackObserver::schedulePoll() {
    if (timerScheduled_) return;
    Fl::add_timeout(1.0, timerCb, this);
    timerScheduled_ = true;
}

void JackObserver::timerCb(void* self) {
    auto* obs = static_cast<JackObserver*>(self);
    obs->timerScheduled_ = false;
    if (obs->state_ != State::Polling) return;  // stopped while waiting
    obs->attempt();
}

void JackObserver::setState(State s) {
    state_ = s;
    for (auto& l : listeners_) l(s);
}
