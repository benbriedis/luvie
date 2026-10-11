// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#include "internalTransport.hpp"
#include "midiScheduler.hpp"
#include <algorithm>

InternalTransport::InternalTransport()
{
    setSampleRate(kSampleRate);
}

InternalTransport::~InternalTransport()
{
    stop();
}

void InternalTransport::start(MidiScheduler* sched)
{
    if (running_.load()) return;
    sched_ = sched;
    running_.store(true);
    thread_ = std::thread([this] { run(); });
    started_.wait();
}

void InternalTransport::stop()
{
    if (!running_.exchange(false)) return;
    kick();
    if (thread_.joinable()) thread_.join();
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
            pendingJump_ = true;
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
        cycleFrame_  = frame_;
        cycleFrames_ = want ? end - frame_ : periodFrames;
        const int64_t windowEnd = want ? end : frame_ + periodFrames;

        const bool ran = renderCycle(want, pendingJump_,
                                     static_cast<double>(frame_)    / kSampleRate,
                                     static_cast<double>(windowEnd) / kSampleRate);
        // Only a cycle that actually rendered consumes the stop/jump; otherwise the
        // snapshot was busy and the next cycle retries (as for JACK).
        if (ran) {
            wasPlaying   = want;
            pendingJump_ = false;
        }
        frame_   = end;
        rolling_ = want;
        posFrames_.store(frame_);

        if (want) {
            nextWake += kPeriodNs;
            const int64_t now = rtNowNs();
            if (nextWake < now) nextWake = now + kPeriodNs;   // overran: don't sprint
            rtSleepUntil(nextWake);
        } else if (!ran) {
            rtSleepUntil(rtNowNs() + kPeriodNs);               // retry the busy cycle
        } else {
            wake_.wait();                                      // idle until a command
        }
    }
}

void InternalTransport::emit(const std::string& port, double bar,
                             const uint8_t* data, int len)
{
    if (!sched_) return;
    // As JackTransport::emit: the frame within this window, honouring the active
    // sub-segment's time base; a note-off ends one frame early (half-open interval).
    long off = segFrameOffset(bar);
    if (isNoteOff(data, len)) off -= 1;
    if (off < 0) off = 0;
    if (cycleFrames_ > 0 && off > cycleFrames_ - 1) off = static_cast<long>(cycleFrames_ - 1);

    const int64_t frame = cycleFrame_ + off;
    const int64_t dueNs = anchorNs_ + static_cast<int64_t>((frame - anchorFrame_) * 1e9 / kSampleRate);
    sched_->push(MidiScheduler::Producer::Internal, port.c_str(), dueNs, data, len);
}
