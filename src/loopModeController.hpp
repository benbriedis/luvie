// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#ifndef LOOP_MODE_CONTROLLER_HPP
#define LOOP_MODE_CONTROLLER_HPP

#include <functional>

class ITransport;
class ModernTabs;
class Editor;

// Owns the tri-state Song/Loop mode and drives the transport + visuals through a
// switch. The mode toggle only *requests* a mode; this controller decides how and
// when to commit it:
//
//   Song → Loop  : instant. Freeze the song playhead at its current bar (greyed,
//                  fixed) and flip the transport to loop mode. The LoopManager's
//                  active set is NOT cleared, so whatever was sounding keeps
//                  looping (sync() is gated off while the playhead is loop-active).
//   Loop → Song  : the looper keeps running (button yellow, "Loop") until the next
//                  bar line, then the song comes in on that downbeat — at the bar
//                  line at or after the frozen bar, so the loops play out the rest of
//                  the bar the playhead froze in. The song playhead unfreezes at the
//                  click and walks the run-up, greyed, reaching the resume bar as the
//                  switch lands; there it turns red and the mode settles.
//
// The hand-off is *armed* on the click and landed by the engine, not by this class.
// Two reasons. A seek would relocate the clock — dipping JACK through
// JackTransportStarting (and the host, in plugin mode), silencing notes and resetting
// controllers. And the moment matters as much as the manner: in plugin mode the switch
// travels UI → host → LV2 worker, which takes tens of milliseconds, so applying it on
// arrival snapped playback backwards by that latency however carefully this class had
// aligned the phase first. ITransport::endLoopMode() therefore hands the engine the
// resume bar and lets its RT thread pick the frame — the next one whose intra-bar phase
// matches, which for an integer resume bar is the next bar line — beat-exact whenever
// the message happens to arrive.
//
// UI-thread only. The FLTK timeout below no longer decides any timing; it only watches
// for the switch so the editors and the button visual follow it. Nor is the grey run-up
// it draws timed here: the head is displaced by a fixed number of bars, so the clock
// itself carries it to the resume bar.
class LoopModeController {
public:
    ~LoopModeController();

    // songEditor is the Song Editor (for freeze + seek-gating). setEditorsLoopMode
    // flips playhead loop mode on every editor (song + pattern editors).
    void init(ITransport* transport, ModernTabs* tabs, Editor* songEditor,
              std::function<void(bool)> setEditorsLoopMode);

    // Wire to ModernTabs::onModeChanged. `loop` is the requested mode.
    void requestMode(bool loop);

    // Adopt a mode from a loaded project. Settles immediately in both directions:
    // a project load has no in-flight loop phase to hand off, so the Loop → Song
    // alignment wait (and its run-up to the next bar line) would be meaningless.
    void setMode(bool loop);

    // Pin / unpin the clock's tempo map (ObservableSong::holdTempo). Called
    // with the mode and the frozen bar before anything else in a settle, so the
    // engine's rebuild sees the map the new mode is to run on.
    std::function<void(bool loop, float atBar)> setTempoFreeze;

    // Fired whenever the mode settles — the user's toggle, the end of a Loop →
    // Song hand-off, or setMode(). Lets the owner notice a mode change without
    // having to poll or duplicate the transition rules.
    std::function<void()> onModeSettled;

    // True while settled in Song mode (not Loop, not mid-transition-to-Song).
    bool isSongMode() const { return state == State::Song; }
    // True in Loop mode, including while handing back to Song: the loops are still
    // the thing playing until the hand-off completes.
    bool isLoopMode() const { return state != State::Song; }

private:
    enum class State { Song, Loop, TransitionToSong };

    // The settle itself: transport, editors, visuals. tellTransport is false only on
    // the Loop → Song path, where beginTransition() already armed the engine and this
    // is just the visuals catching up with a switch that has happened.
    void applyMode(bool loop, bool tellTransport);
    void enterLoop();        // Song → Loop
    void beginTransition();  // Loop → Song: arm the hand-off, then watch for it
    void finishToSong();     // the switch has landed (or cannot): settle the visuals
    void poll();             // transition tick: watch for the engine's switch
    void startPoll();
    void stopPoll();
    static void pollCb(void* self);

    ITransport* transport = nullptr;
    ModernTabs* tabs      = nullptr;
    Editor*     songEditor = nullptr;
    std::function<void(bool)> setEditorsLoopMode;

    State state         = State::Song;
    float frozenSongBar = 0.0f;
    float handoffAt     = 0.0f;    // clock bar the armed hand-off will land on
    float handoffOffset = 0.0f;    // resume bar - handoffAt: the run-up head's shift
    float pollPrevPos   = 0.0f;    // last transport position seen by poll()
    int   pollTicksLeft = 0;       // safety-net countdown; see poll()
    bool  pollActive    = false;
};

#endif
