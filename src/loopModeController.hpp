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
//   Loop → Song  : the looper keeps running (button yellow, "Loop") until the
//                  transport's position within the bar lines up with the frozen
//                  song bar, then it seeks back a whole number of bars and resumes
//                  song playback from exactly where the playhead was frozen.
//
// UI-thread only; the transition is polled on an FLTK timeout, and both backends
// (Internal + JACK) are driven purely through ITransport::seek()/setLoopMode().
class LoopModeController {
public:
    ~LoopModeController();

    // songEditor is the Song Editor (for freeze + seek-gating). setEditorsLoopMode
    // flips playhead loop mode on every editor (song + pattern editors).
    void init(ITransport* transport, ModernTabs* tabs, Editor* songEditor,
              std::function<void(bool)> setEditorsLoopMode);

    // Wire to ModernTabs::onModeChanged. `loop` is the requested mode.
    void requestMode(bool loop);

    // True while settled in Song mode (not Loop, not mid-transition-to-Song).
    bool isSongMode() const { return state == State::Song; }

private:
    enum class State { Song, Loop, TransitionToSong };

    void enterLoop();        // Song → Loop
    void beginTransition();  // Loop → Song (starts the poll, or finishes if paused)
    void finishToSong();     // commit the seek-back + flip to song mode
    void poll();             // transition tick: watch for intra-bar phase alignment
    void startPoll();
    void stopPoll();
    static void pollCb(void* self);

    ITransport* transport = nullptr;
    ModernTabs* tabs      = nullptr;
    Editor*     songEditor = nullptr;
    std::function<void(bool)> setEditorsLoopMode;

    State state         = State::Song;
    float frozenSongBar = 0.0f;
    float pollPrevPhase = -1.0f;   // last intra-bar phase seen; -1 = not yet sampled
    bool  pollActive    = false;
};

#endif
