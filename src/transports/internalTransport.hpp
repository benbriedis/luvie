// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#ifndef INTERNAL_TRANSPORT_HPP
#define INTERNAL_TRANSPORT_HPP

#include "itransport.hpp"
#include "sequencer.hpp"
#include "rtThread.hpp"
#include <atomic>
#include <cstdint>
#include <thread>

class MidiScheduler;

/*
 * InternalTransport — the standalone app's own clock, for when JACK is not the
 * transport. It runs the shared Sequencer on a real-time thread exactly the way
 * JackTransport does from JACK's process callback, so both clocks generate MIDI
 * with the same engine and the same guarantees; only the clock source differs.
 *
 * The clock is a frame counter at a nominal kSampleRate, anchored to rtNowNs(). The
 * engine thread wakes every kPeriodNs while playing and renders the window up to
 * kLookaheadNs ahead of now. Every message is handed to the MidiScheduler stamped
 * with the time its frame falls at; the scheduler's sender thread delivers it then.
 * Rendering ahead is what lets delivery be punctual: an event is queued before it
 * is due, whatever the engine's own wake-up jitter.
 *
 * Stopped, the engine blocks on a semaphore. Commands (play, seek, a hand-off armed
 * while stopped, ...) post it, so it runs one cycle to act on them and goes back to
 * sleep — there is no polling.
 *
 * Thread model, as for JackTransport:
 *   - ITransport methods and the Sequencer setters run on the UI thread.
 *   - The engine thread calls renderCycle() and emit(). It never allocates, never
 *     takes a lock it can wait on (renderCycle only try_locks), and never calls
 *     into a MIDI driver.
 */
class InternalTransport : public Sequencer, public ITransport {
public:
    static constexpr double  kSampleRate  = 48000.0;    // nominal: frames are only a unit
    static constexpr int64_t kPeriodNs    = 1000000;    // engine wake-up while playing
    static constexpr int64_t kLookaheadNs = 3000000;    // how far ahead it renders

    InternalTransport();
    ~InternalTransport() override;

    // Start the engine thread, sending everything through `sched`. Call once, after
    // setTimeline(). stop() (also run by the destructor) joins it; the scheduler
    // must outlive it.
    void start(MidiScheduler* sched);
    void stop();

    // ITransport — called from the UI thread
    void  play()             override;
    void  pause()            override;
    void  rewind()           override;
    void  seek(float bars)   override;
    void  reanchor(float bars) override;
    float position()  const override;
    // The commanded state, not the engine's: the engine follows within a period, and
    // callers (the play button, the router) expect a play() to read back at once.
    bool  isPlaying() const override { return wantPlaying_.load(); }
    void  setLoopMode(bool loopMode) override { Sequencer::setLoopMode(loopMode); kick(); }
    void  setSongLoop(bool enabled, float startBar, float endBar) override {
        Sequencer::setSongLoop(enabled, startBar, endBar);
        kick();
    }
    // Hand-offs and scene changes: the Sequencer lands them on the RT thread, and a
    // stopped one lands on the next cycle — so make sure there is one.
    void  endLoopMode(float bars)    override { Sequencer::endLoopMode(bars);   kick(); }
    void  armScene(float atBar)      override { Sequencer::armScene(atBar);     kick(); }
    void  beginLoopMode(float atBar) override { Sequencer::beginLoopMode(atBar); kick(); }
    void  skipNoteOnce(int instrumentId, int midiPitch, double bar, double tolBars) override {
        Sequencer::skipNoteOnce(instrumentId, midiPitch, bar, tolBars);
    }

protected:
    // Sequencer output hook (engine thread): stamp the message with the time its
    // frame falls at and queue it for the scheduler.
    void emit(const std::string& port, double bar,
              const uint8_t* data, int len) override;

private:
    void run();
    void kick() { wake_.post(); }   // run at least one engine cycle

    MidiScheduler* sched_ = nullptr;
    std::thread    thread_;
    RtSemaphore    wake_;
    std::atomic<bool> running_{false};

    // ── Commands (UI thread writes, engine reads) ─────────────────────────────
    std::atomic<bool>    wantPlaying_{false};
    std::atomic<int64_t> seekFrame_{-1};   // pending relocate target, -1 = none

    // ── Published by the engine ───────────────────────────────────────────────
    std::atomic<int64_t> posFrames_{0};    // frame at the end of the rendered window

    // ── Engine-thread-only state ──────────────────────────────────────────────
    // The clock: frame anchorFrame_ falls at rtNowNs() == anchorNs_. Re-anchored on
    // play and on a relocate, so a seek moves the frames without moving time.
    int64_t anchorNs_      = 0;
    int64_t anchorFrame_   = 0;
    int64_t frame_         = 0;      // start of the next window to render
    int64_t cycleFrame_    = 0;      // start of the window being rendered (for emit)
    int64_t cycleFrames_   = 0;      // its length
    bool    rolling_       = false;  // what the engine last rendered
    bool    pendingJump_   = false;  // a relocate renderCycle has not yet consumed
};

#endif
