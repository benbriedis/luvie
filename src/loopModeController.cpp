// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#include "loopModeController.hpp"
#include "itransport.hpp"
#include "modern/modernTabs.hpp"
#include "editor.hpp"

#include <FL/Fl.H>
#include <cmath>

// The poll no longer times the hand-off — the engine does — so this is just how
// often the visuals check whether the switch has landed.
static constexpr double kPollInterval      = 0.02;   // 20 ms
static constexpr int    kMaxTransitionTicks = 250;   // 5 s; see the note in poll()

LoopModeController::~LoopModeController()
{
    stopPoll();
}

void LoopModeController::init(ITransport* t, ModernTabs* tb, Editor* se,
                              std::function<void(bool)> setLoop)
{
    transport          = t;
    tabs               = tb;
    songEditor         = se;
    setEditorsLoopMode = std::move(setLoop);
}

void LoopModeController::requestMode(bool loop)
{
    // A click during the hand-off cancels it: stay looping (back to a settled Loop).
    // The engine has an armed hand-off waiting for its bar phase, so it has to be told
    // — setLoopMode() drops any pending one.
    if (state == State::TransitionToSong) {
        stopPoll();
        transport->setLoopMode(true);
        state = State::Loop;
        tabs->setModeVisual(ModernTabs::ModeVisual::Loop);
        return;
    }

    if (loop) {
        if (state == State::Loop) return;
        enterLoop();
    } else {
        if (state == State::Song) return;
        beginTransition();
    }
}

// Settling into a mode: the transport flag, the editors, the frozen song playhead
// and the button visual. Every path that changes mode ends here, so none of them
// can leave one of the four out of step.
void LoopModeController::applyMode(bool loop, bool tellTransport)
{
    if (tellTransport) transport->setLoopMode(loop);
    // In loop mode this gates sync() off → the active loop set freezes as-is.
    setEditorsLoopMode(loop);
    songEditor->setPlayheadFrozen(loop, frozenSongBar);
    songEditor->setSeekingEnabled(!loop);

    state = loop ? State::Loop : State::Song;
    tabs->setModeVisual(loop ? ModernTabs::ModeVisual::Loop : ModernTabs::ModeVisual::Song);
    if (onModeSettled) onModeSettled();
}

void LoopModeController::enterLoop()
{
    // Remember where the song playhead was; freeze it there (greyed, non-interactive).
    frozenSongBar = transport->position();
    applyMode(true, true);
}

void LoopModeController::setMode(bool loop)
{
    stopPoll();
    if (loop) {
        if (state != State::Loop) enterLoop();
    } else if (state != State::Song) {
        applyMode(false, true);
    }
}

void LoopModeController::beginTransition()
{
    // Arm the hand-off straight away. The engine holds it until the next frame whose
    // intra-bar phase matches frozenSongBar's and switches there, so the wait happens
    // on the RT thread where it can be sample-accurate — rather than here, where the
    // message's travel time to the engine would shift it off the beat.
    transport->endLoopMode(frozenSongBar);

    // With no clock advancing there is no phase to wait for: the engine takes the
    // hand-off on its next cycle, so settle the visuals now.
    if (!transport->isPlaying()) {
        finishToSong();
        return;
    }
    state = State::TransitionToSong;
    tabs->setModeVisual(ModernTabs::ModeVisual::Transitioning);   // yellow, still "Loop"
    pollPrevPos  = transport->position();
    pollTicksLeft = kMaxTransitionTicks;
    startPoll();
}

void LoopModeController::finishToSong()
{
    stopPoll();
    // The engine has already switched (or, when stopped, will on its next cycle). This
    // only brings the editors, the frozen playhead and the button visual across — and
    // re-enables sync() from the frozen bar onward.
    applyMode(false, false);
}

void LoopModeController::poll()
{
    if (state != State::TransitionToSong) return;
    if (!transport->isPlaying()) { finishToSong(); return; }

    // Watch for the engine's switch rather than timing one. In Loop mode the clock
    // free-runs forward, so the only thing that moves the position backwards is the
    // hand-off landing — and it lands on frozenSongBar, which the second test pins
    // down (a rewind mid-transition also jumps back, but to bar 0).
    const float pos = transport->position();
    if (pos < pollPrevPos &&
        pos >= frozenSongBar - 0.01f && pos < frozenSongBar + 0.25f) {
        finishToSong();
        return;
    }
    pollPrevPos = pos;

    // Safety net, not a timing mechanism. The engine takes at most one bar to reach
    // the phase, so this only fires if the switch never happened (nothing armed
    // because there was no timeline to build from) or was missed — either way, a
    // permanently yellow button that will not go back to Song is the worse outcome.
    if (--pollTicksLeft <= 0) finishToSong();
}

void LoopModeController::pollCb(void* self)
{
    auto* c = static_cast<LoopModeController*>(self);
    c->poll();
    if (c->pollActive)
        Fl::repeat_timeout(kPollInterval, pollCb, self);
}

void LoopModeController::startPoll()
{
    if (pollActive) return;
    pollActive = true;
    Fl::add_timeout(kPollInterval, pollCb, this);
}

void LoopModeController::stopPoll()
{
    if (!pollActive) return;
    pollActive = false;
    Fl::remove_timeout(pollCb, this);
}
