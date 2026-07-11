#ifndef TRANSPORT_ROUTER_HPP
#define TRANSPORT_ROUTER_HPP

#include "itransport.hpp"

// Forwards ITransport calls to a swappable backend so the rest of the UI can
// hold a single stable ITransport* while the active clock source changes at
// runtime (Internal vs JACK). On switch it carries the current position, play
// state and loop mode across so playback continues seamlessly.
class TransportRouter : public ITransport {
    ITransport* active_   = nullptr;
    bool        loopMode_ = false;

public:
    void setActive(ITransport* t) {
        if (t == active_) return;
        const float pos     = active_ ? active_->position()  : 0.0f;
        const bool  playing = active_ ? active_->isPlaying() : false;
        // Stop the outgoing transport before switching, otherwise it keeps
        // running (e.g. JACK transport stays rolling, so JackTransport's RT
        // engine keeps sequencing) while the incoming clock also plays — every
        // note then fires twice. Carry pos/play state onto the new transport.
        if (active_ && playing) active_->pause();
        active_ = t;
        if (active_) {
            active_->setLoopMode(loopMode_);
            active_->seek(pos);
            if (playing) active_->play();
        }
    }
    ITransport* active() const { return active_; }

    void  play()             override { if (active_) active_->play(); }
    void  pause()            override { if (active_) active_->pause(); }
    void  rewind()           override { if (active_) active_->rewind(); }
    void  seek(float bars)   override { if (active_) active_->seek(bars); }
    void  reanchor(float b)  override { if (active_) active_->reanchor(b); }
    float position()  const override { return active_ ? active_->position()  : 0.0f; }
    bool  isPlaying() const override { return active_ ? active_->isPlaying() : false; }
    void  setLoopMode(bool loopMode) override {
        loopMode_ = loopMode;
        if (active_) active_->setLoopMode(loopMode);
    }
};

#endif
