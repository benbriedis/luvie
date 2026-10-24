// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#include "internalTransport.hpp"
#include "midiScheduler.hpp"
#include <algorithm>

InternalTransport::InternalTransport()
    : CycleDriver(kSampleRate, 0)   // no buffer: everything goes to the scheduler
{
}

InternalTransport::~InternalTransport()
{
    stop();
}

void InternalTransport::start(MidiScheduler* sched)
{
    if (running_.load()) return;
    sched_ = sched;
    // From here the engine thread owns the snapshot; setters reach it as commands.
    attachRt(true);
    running_.store(true);
    thread_ = std::thread([this] { run(); });
    started_.wait();
}

void InternalTransport::stop()
{
    if (!running_.exchange(false)) return;
    kick();
    if (thread_.joinable()) thread_.join();
    attachRt(false);
}

// ── ITransport (UI thread) ────────────────────────────────────────────────────

void InternalTransport::play()  { wantPlaying_.store(true);  kick(); }
void InternalTransport::pause() { wantPlaying_.store(false); kick(); }

void InternalTransport::rewind()
{
    // As JackTransport::rewind: an explicit reposition re-establishes the identity
    // frame <-> bar mapping, and supersedes any armed switch.
    setSecsOffset(0.0);
    cancelPending();
    seekFrame_.store(0);
    posFrames_.store(0);   // read back at once; the engine confirms next cycle
    kick();
}

void InternalTransport::seek(float bars)
{
    if (!timeline) return;
    setSecsOffset(0.0);
    cancelPending();
    const auto frame = static_cast<int64_t>(timeline->barToSeconds(std::max(0.0f, bars))
                                            * kSampleRate);
    seekFrame_.store(frame);
    posFrames_.store(frame);
    kick();
}

void InternalTransport::reanchor(float bars)
{
    if (!timeline) return;
    // See JackTransport::reanchor: keep the frames rolling and slide the song's
    // timeline so the current frame still reads as `bars` under the new tempo map.
    const double secs = static_cast<double>(posFrames_.load()) / kSampleRate;
    setSecsOffset(secs - timeline->barToSeconds(bars));
}

float InternalTransport::position() const
{
    if (!timeline) return 0.0f;
    // The engine wraps the song loop itself, so while it is armed the honest position
    // is the wrapped bar it publishes (as for JACK).
    if (songLoopActive())
        return loopedPosition();
    const double secs = static_cast<double>(posFrames_.load()) / kSampleRate;
    return static_cast<float>(timeline->secondsToBar(secs - secsOffsetSecs()));
}

// ── Engine thread ─────────────────────────────────────────────────────────────

void InternalTransport::run()
{
    realtime_ = rtPromoteCurrentThread(kPeriodNs);
    started_.post();

    const auto periodFrames = static_cast<int64_t>(kSampleRate * kPeriodNs / 1e9);
    auto frameToNs = [this](int64_t f) {
        return anchorNs_ + static_cast<int64_t>((f - anchorFrame_) * 1e9 / kSampleRate);
    };
    int64_t nextWake = 0;

    while (running_.load()) {
        // A relocate. While rolling the clock is not interrupted: the new frames are
        // pinned to the time rendering had reached, so what is already queued plays
        // out and the new position follows straight on.
        const int64_t target = seekFrame_.exchange(-1);
        if (target >= 0) {
            if (rolling_) anchorNs_ = frameToNs(frame_);
            frame_ = anchorFrame_ = target;
        }

        const bool want = wantPlaying_.load();
        if (want && !rolling_) {
            // Starting: the first frame sounds one lookahead from now, which is the
            // time it takes the engine to have it queued.
            anchorNs_    = rtNowNs() + kLookaheadNs;
            anchorFrame_ = frame_;
            nextWake     = rtNowNs();
        }

        // This cycle's window. Rolling: from where the last one ended to the frame
        // falling one lookahead from now — both from the integer frame counter, so
        // consecutive windows abut exactly (see renderCycle). Stopped: a nominal
        // period at the parked frame, as JACK hands a stopped client, for the
        // engine's stop/relocate resets.
        int64_t end = frame_;
        if (want) {
            const int64_t horizon = rtNowNs() + kLookaheadNs - anchorNs_;
            end = std::max(frame_, anchorFrame_ +
                                   static_cast<int64_t>(horizon * kSampleRate / 1e9));
        }
        runCycle({frame_, want ? end - frame_ : periodFrames, want, target >= 0});
        frame_   = end;
        rolling_ = want;
        posFrames_.store(frame_);

        if (want) {
            nextWake += kPeriodNs;
            const int64_t now = rtNowNs();
            if (nextWake < now) nextWake = now + kPeriodNs;   // overran: don't sprint
            rtSleepUntil(nextWake);
        } else {
            wake_.wait();                                      // idle until a command
        }
    }
}

void InternalTransport::emitUnbuffered(const std::string& port, long offset,
                                       const uint8_t* data, int len)
{
    if (!sched_) return;
    const int64_t frame = cycleFrame() + offset;
    const int64_t dueNs = anchorNs_ + static_cast<int64_t>((frame - anchorFrame_) * 1e9 / kSampleRate);
    sched_->push(MidiScheduler::Producer::Internal, port.c_str(), dueNs, data, len);
}
