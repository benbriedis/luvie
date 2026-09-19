// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#ifndef LIVE_NOTE_LIGHTS_HPP
#define LIVE_NOTE_LIGHTS_HPP

#include <FL/Fl.H>
#include <FL/Fl_Widget.H>
#include <algorithm>
#include <array>
#include <chrono>

// Pitches arriving on the MIDI input, lit in a label column so the player can see
// which row a key or pad lands on. A note stays lit while it is held, and for at
// least kMinSeconds, since a drum pad's note-off can follow within a few
// milliseconds and would otherwise never be seen. UI thread only.
class LiveNoteLights {
    using Clock = std::chrono::steady_clock;
    static constexpr double kMinSeconds = 0.15;

    Fl_Widget*                         owner_;
    std::array<bool, 128>              held_{};
    std::array<Clock::time_point, 128> until_{};   // lit at least until this

    static void expireCb(void* self) { static_cast<LiveNoteLights*>(self)->owner_->redraw(); }

public:
    explicit LiveNoteLights(Fl_Widget* owner) : owner_(owner) {}
    ~LiveNoteLights() { Fl::remove_timeout(expireCb, this); }

    LiveNoteLights(const LiveNoteLights&)            = delete;
    LiveNoteLights& operator=(const LiveNoteLights&) = delete;

    void noteOn(int pitch) {
        if (pitch < 0 || pitch > 127) return;
        held_[pitch]  = true;
        until_[pitch] = Clock::now() + std::chrono::duration_cast<Clock::duration>(
                                           std::chrono::duration<double>(kMinSeconds));
        owner_->redraw();
    }

    void noteOff(int pitch) {
        if (pitch < 0 || pitch > 127 || !held_[pitch]) return;
        held_[pitch] = false;
        const double left = std::chrono::duration<double>(until_[pitch] - Clock::now()).count();
        // Released before its minimum was up: redraw again once it has passed.
        if (left > 0.0) Fl::add_timeout(left + 0.005, expireCb, this);
        else            owner_->redraw();
    }

    void clear() {
        Fl::remove_timeout(expireCb, this);
        held_.fill(false);
        until_.fill(Clock::time_point{});
        owner_->redraw();
    }

    bool lit(int pitch) const {
        if (pitch < 0 || pitch > 127) return false;
        return held_[pitch] || Clock::now() < until_[pitch];
    }
};

#endif
