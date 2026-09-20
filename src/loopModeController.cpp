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
// A bar position within this of a bar line counts as being on it: ceil() must not
// push a playhead sitting exactly on bar 12 out to bar 13 on a float rounding hair.
static constexpr float  kBarEps            = 1.0e-4f;

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
    // A click during a hand-off cancels it and settles back where it came from. The
    // engine has an armed hand-off waiting for its bar line, so it has to be told —
    // setLoopMode() (in applyMode) drops any pending one.
    if (state == State::TransitionToSong) {
        if (!loop) return;              // already going there
        stopPoll();
        applyMode(true, true);          // re-freezes the head, dropping the hand-off
        return;
    }
    if (state == State::TransitionToLoop) {
        if (loop) return;               // already going there
        stopPoll();
        // applyMode() releases the tempo hold enterLoop() put on at the arm: the song
        // is staying, so it keeps its own map. The playhead needs nothing — it was
        // never greyed on this path.
        applyMode(false, true);
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
    // The tempo freeze first: Loop Mode runs the clock on a map pinned at the frozen
    // bar, Song Mode on the song's own, and the transport's rebuild below has to see
    // the right one. On the way out this lands after the engine has already switched,
    // which is what keeps the run-up's clock matching the loops that are still
    // sounding.
    if (setTempoFreeze) setTempoFreeze(loop, frozenSongBar);
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
    // Stopped: no bar to round out and no clock to wait on, so freeze the head where
    // it stands and settle now.
    if (!transport->isPlaying()) {
        frozenSongBar = transport->position();
        applyMode(true, true);
        return;
    }

    // Playing: the song plays out the rest of its bar and the loops come in on the
    // next downbeat — the mirror of beginTransition(). The playhead freezes on that
    // bar line, so that is also the bar a later Loop -> Song resumes from.
    const float pos = transport->position();
    handoffAt     = std::ceil(pos - kBarEps);
    frozenSongBar = handoffAt;
    handoffOffset = 0.0f;

    // The tempo freeze goes on before the arm, not with the settle: Loop Mode's clock
    // runs on a map pinned at the frozen bar, and the engine builds the snapshot it
    // parks from that map the moment it is armed. applyMode() below is told not to
    // repeat it.
    if (setTempoFreeze) setTempoFreeze(true, frozenSongBar);
    transport->beginLoopMode(handoffAt);

    state = State::TransitionToLoop;
    tabs->setModeVisual(ModernTabs::ModeVisual::EnteringLoop);   // yellow, still "Song"
    // The song playhead is deliberately left alone here — live, red and moving. It is
    // still the thing driving playback until the seam, which is exactly what the other
    // direction greys it to say it is *not*. It freezes at the bar line when the
    // switch lands, in applyMode().

    pollPrevPos   = pos;
    pollTicksLeft = kMaxTransitionTicks;
    startPoll();
}

void LoopModeController::finishToLoop()
{
    stopPoll();
    // The engine has already switched. tellTransport is false for the same reason as
    // finishToSong(): this is only the visuals catching up. setTempoFreeze has already
    // run, at the arm.
    applyMode(true, false);
}

void LoopModeController::setMode(bool loop)
{
    stopPoll();
    if (loop) {
        if (state == State::Loop) return;
        // Deliberately not enterLoop(): a project load has no bar in progress to round
        // out, and arming a hand-off for one would leave the mode yellow and unsettled
        // until a clock that may not even be running crossed it. Freeze where the
        // loaded playhead is and settle, which is what this did before Song -> Loop
        // learned to wait.
        frozenSongBar = transport->position();
        applyMode(true, true);
    } else if (state != State::Song) {
        applyMode(false, true);
    }
}

void LoopModeController::beginTransition()
{
    // With no clock advancing there is nothing to run up to and no bar line to wait
    // for: the engine takes the hand-off on its next cycle, so resume exactly where
    // the playhead froze and settle the visuals now.
    if (!transport->isPlaying()) {
        transport->endLoopMode(frozenSongBar);
        finishToSong();
        return;
    }

    // The song comes back in on a bar line — the loops play out the rest of the bar
    // the playhead froze in, and the downbeat is where the song takes over. An
    // integer resume bar is also what puts the engine's landing on a bar line, since
    // it aligns the switch to the resume bar's intra-bar phase (see handoffPoint()).
    const float resumeBar = std::ceil(frozenSongBar - kBarEps);

    // Where the switch will land: the next bar line of the free-running loop clock.
    // Same rule the engine applies, so the two agree without a round trip. Read the
    // clock before arming, while the position is still unambiguously the loops'.
    const float pos = transport->position();
    handoffAt = std::ceil(pos - kBarEps);

    // Arm the hand-off straight away. The engine holds it until that bar line and
    // switches there, so the wait happens on the RT thread where it can be
    // sample-accurate — rather than here, where the message's travel time to the
    // engine would shift it off the beat.
    transport->endLoopMode(resumeBar);

    handoffOffset = resumeBar - handoffAt;

    state = State::TransitionToSong;
    tabs->setModeVisual(ModernTabs::ModeVisual::LeavingLoop);   // yellow, still "Loop"
    // Unfreeze the song playhead — still greyed, since the loops are what is
    // sounding — displaced so it walks up to resumeBar and arrives there exactly as
    // the switch lands. What it has to cover is the time left on the clock, which is
    // the honest thing to show: the head says how long the loops have left.
    songEditor->setPlayheadHandoff(true, handoffOffset);

    pollPrevPos   = pos;
    pollTicksLeft = kMaxTransitionTicks;
    startPoll();
}

void LoopModeController::finishToSong()
{
    stopPoll();
    // The engine has already switched (or, when stopped, will on its next cycle). This
    // only brings the editors, the playhead — now live and red, at the resume bar the
    // grey head just walked to — and the button visual across.
    applyMode(false, false);
}

void LoopModeController::poll()
{
    // Song -> Loop. Simpler to watch than the other direction: the loop clock picks up
    // where the song left off rather than being dragged back to a resume bar, so the
    // position never moves backwards and the crossing is the whole test.
    if (state == State::TransitionToLoop) {
        if (!transport->isPlaying()) { finishToLoop(); return; }
        const float pos = transport->position();
        if (pos >= handoffAt - kBarEps) { finishToLoop(); return; }
        // Same safety net as below: a permanently yellow button that will not settle
        // is the worse outcome if the switch never happened.
        if (--pollTicksLeft <= 0) finishToLoop();
        return;
    }

    if (state != State::TransitionToSong) return;
    if (!transport->isPlaying()) { finishToSong(); return; }

    // Watch for the engine's switch rather than timing one. Normally the loops have
    // run on past the resume bar, so the switch drags the position back to it — and
    // a free-running loop clock moves backwards for nothing else. That test is worth
    // preferring: the clock crosses handoffAt a cycle or two before the engine acts
    // on it, and settling on the crossing alone would flash the head at the loops'
    // position. It only fails to fire when the switch does not move the position at
    // all (the resume bar *is* the bar line being waited for, so the displayed offset
    // is zero) or moves it forward (a rewind during Loop mode put the clock behind);
    // in both of those there is nothing to flash, so the crossing is enough.
    const float pos = transport->position();
    if (pos < pollPrevPos || (handoffOffset >= 0.0f && pos >= handoffAt - kBarEps)) {
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
