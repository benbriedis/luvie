// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#include "oscObserver.hpp"
#include "oscTransport.hpp"
#include <FL/Fl.H>

// How long Ardour has to answer a probe. It answers from its own event loop, so
// this is generous; it is also the retry interval while Waiting.
static constexpr double kProbeTimeout = 1.0;

OscObserver::OscObserver(OscTransport* osc) : osc_(osc) {
    osc_->onReply = [this]() { answered(); };
    osc_->onCommandSent = [this]() {
        // Only re-check from Up: while Waiting the retry timer is already probing.
        if (state_ == State::Up && !timerScheduled_) probe();
    };
}

OscObserver::~OscObserver() {
    cancelTimer();
    osc_->onReply       = nullptr;
    osc_->onCommandSent = nullptr;
}

void OscObserver::start(int port) {
    cancelTimer();
    if (!osc_->open(port)) {
        setState(State::Unavailable);
        return;
    }
    setState(State::Waiting);
    probe();
}

void OscObserver::stop() {
    cancelTimer();
    osc_->close();
    setState(State::Down);
}

void OscObserver::probe() {
    osc_->probe();
    if (!timerScheduled_) {
        Fl::add_timeout(kProbeTimeout, deadlineCb, this);
        timerScheduled_ = true;
    }
}

void OscObserver::answered() {
    if (state_ != State::Waiting && state_ != State::Up) return;   // stopped meanwhile
    cancelTimer();
    setState(State::Up);
}

void OscObserver::cancelTimer() {
    if (timerScheduled_) {
        Fl::remove_timeout(deadlineCb, this);
        timerScheduled_ = false;
    }
}

void OscObserver::deadlineCb(void* self) {
    auto* obs = static_cast<OscObserver*>(self);
    obs->timerScheduled_ = false;
    if (obs->state_ != State::Waiting && obs->state_ != State::Up) return;
    obs->setState(State::Waiting);
    obs->probe();                         // retry; re-arms the deadline
}

void OscObserver::setState(State s) {
    if (s == state_) return;
    state_ = s;
    for (auto& l : listeners_) l(s);
}
