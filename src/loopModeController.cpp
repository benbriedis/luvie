#include "loopModeController.hpp"
#include "itransport.hpp"
#include "modern/modernTabs.hpp"
#include "editor.hpp"

#include <FL/Fl.H>
#include <cmath>

static constexpr double kPollInterval = 0.02;  // 20 ms — sub-frame alignment resolution

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
    if (state == State::TransitionToSong) {
        stopPoll();
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

void LoopModeController::enterLoop()
{
    // Remember where the song playhead was; freeze it there (greyed, non-interactive).
    frozenSongBar = transport->position();
    transport->setLoopMode(true);
    setEditorsLoopMode(true);   // gates sync() off → the active loop set freezes as-is
    songEditor->setPlayheadFrozen(true, frozenSongBar);
    songEditor->setSeekingEnabled(false);

    state = State::Loop;
    tabs->setModeVisual(ModernTabs::ModeVisual::Loop);
}

void LoopModeController::beginTransition()
{
    // With no clock advancing there is nothing to align to — resume immediately.
    if (!transport->isPlaying()) {
        finishToSong();
        return;
    }
    state = State::TransitionToSong;
    tabs->setModeVisual(ModernTabs::ModeVisual::Transitioning);   // yellow, still "Loop"
    pollPrevPhase = -1.0f;
    startPoll();
}

void LoopModeController::finishToSong()
{
    stopPoll();
    // Whole-bar seek back to the frozen position keeps bar-length loops phase-aligned.
    transport->seek(frozenSongBar);
    transport->setLoopMode(false);
    setEditorsLoopMode(false);   // re-enables sync() from the frozen bar onward
    songEditor->setPlayheadFrozen(false);
    songEditor->setSeekingEnabled(true);

    state = State::Song;
    tabs->setModeVisual(ModernTabs::ModeVisual::Song);
}

void LoopModeController::poll()
{
    if (state != State::TransitionToSong) return;
    if (!transport->isPlaying()) { finishToSong(); return; }

    const float target = frozenSongBar - std::floor(frozenSongBar);   // intra-bar phase
    const float pos    = transport->position();
    const float phase  = pos - std::floor(pos);

    if (pollPrevPhase >= 0.0f) {
        // Did the advancing transport just cross the frozen bar's intra-bar phase?
        bool crossed;
        if (phase >= pollPrevPhase)
            crossed = (pollPrevPhase <= target && target <= phase);       // same bar
        else
            crossed = (target >= pollPrevPhase || target <= phase);       // wrapped a bar
        if (crossed) { finishToSong(); return; }
    }
    pollPrevPhase = phase;
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
