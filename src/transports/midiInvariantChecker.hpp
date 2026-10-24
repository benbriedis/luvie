// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#ifndef MIDI_INVARIANT_CHECKER_HPP
#define MIDI_INVARIANT_CHECKER_HPP

#include <cstdint>
#include <cstring>

/*
 * MidiInvariantChecker — watches an output MIDI stream for the things a sequencer
 * must never produce, whatever the buffer size, the clock or the host:
 *
 *   doubleOn    a note-on for a pitch already sounding on that port and channel
 *               (the earlier note can then be cut by either note-off)
 *   orphanOff   a note-off for a pitch that is not sounding
 *   zeroLength  a note whose off lands on the very frame of its on — a note
 *               clipped to nothing, which no instrument plays
 *   backwards   an event earlier than the one before it on the same port
 *
 * plus heldCount(), the notes still sounding, which must be zero after a stop.
 * All Notes Off (CC 123) clears its channel, as an instrument would. That is legal
 * MIDI, so it is not a violation — but every note it silences was cut short of the
 * length it was written with, which is audible clipping, so those are counted too
 * (cutByReset). Outside a real stop or seek that count should be zero.
 *
 * Frames are absolute (not cycle-relative), so a stream rendered in any buffer
 * size reads the same. Fixed-size tables and no allocation, so it can watch the
 * RT thread's own output in a debug build; it is ~260 KB, so allocate it once,
 * off the RT thread. An optional callback reports every completed note with its
 * on/off frames, which is how a test checks durations against what was written.
 */
class MidiInvariantChecker {
public:
    static constexpr int kPorts = 16;

    struct Counts {
        uint32_t doubleOn   = 0;
        uint32_t orphanOff  = 0;
        uint32_t zeroLength = 0;
        uint32_t backwards  = 0;
        uint32_t total() const { return doubleOn + orphanOff + zeroLength + backwards; }
    };
    struct Stats {
        uint32_t noteOns    = 0;
        uint32_t cutByReset = 0;   // sounding notes silenced by All Notes Off
    };

    using NoteFn = void (*)(void* ctx, int port, int ch, int pitch,
                            int64_t onFrame, int64_t offFrame);

    MidiInvariantChecker() { reset(); }

    void setNoteCallback(NoteFn fn, void* ctx) { noteFn = fn; noteCtx = ctx; }

    void reset()
    {
        for (auto& p : on) for (auto& c : p) for (auto& n : c) n = kOff;
        for (auto& f : lastFrame) f = INT64_MIN;
        for (auto& p : offRun) for (auto& r : p) r = {};
        counts = {};
        stats  = {};
        held   = 0;
    }

    // Returns the violations this event caused (0 = fine), so a caller can log it.
    uint32_t onEvent(int port, int64_t frame, const uint8_t* d, int len)
    {
        if (len < 1 || port < 0 || port >= kPorts) return 0;
        const uint32_t before = counts.total();
        if (frame < lastFrame[port]) counts.backwards++;
        lastFrame[port] = frame;

        const int status = d[0] & 0xF0;
        const int ch     = d[0] & 0x0F;
        if (len >= 3 && status == 0xB0 && d[1] == 123) {        // All Notes Off
            // A sequencer's reset releases its own notes with explicit note-offs
            // first — a frame early, like every note-off — and those are cut just
            // the same as any the CC itself silences.
            if (offRun[port][ch].frame == frame || offRun[port][ch].frame == frame - 1)
                stats.cutByReset += offRun[port][ch].count;
            offRun[port][ch].count = 0;
            for (int p = 0; p < 128; p++)
                if (on[port][ch][p] != kOff) {
                    stats.cutByReset++;
                    report(port, ch, p, frame);
                    on[port][ch][p] = kOff;
                    held--;
                }
        } else if (len >= 3 && (status == 0x80 || (status == 0x90 && d[2] == 0))) {
            const int p = d[1] & 0x7F;
            if (on[port][ch][p] == kOff) {
                counts.orphanOff++;
            } else {
                OffRun& r = offRun[port][ch];
                if (r.frame == frame) r.count++;
                else { r.frame = frame; r.count = 1; }
                if (on[port][ch][p] == frame) counts.zeroLength++;
                report(port, ch, p, frame);
                on[port][ch][p] = kOff;
                held--;
            }
        } else if (len >= 3 && status == 0x90) {
            const int p = d[1] & 0x7F;
            stats.noteOns++;
            if (on[port][ch][p] != kOff) {
                counts.doubleOn++;
                report(port, ch, p, frame);   // the earlier note ends here, in effect
            } else {
                held++;
            }
            on[port][ch][p] = frame;
        }
        return counts.total() - before;
    }

    int           heldCount() const { return held; }
    const Counts& violations() const { return counts; }
    const Stats&  noteStats()  const { return stats; }

private:
    static constexpr int64_t kOff = INT64_MIN;

    void report(int port, int ch, int pitch, int64_t offFrame)
    {
        if (noteFn) noteFn(noteCtx, port, ch, pitch, on[port][ch][pitch], offFrame);
    }

    int64_t on[kPorts][16][128];   // onset frame of each sounding note, kOff if none
    int64_t lastFrame[kPorts];
    // The note-offs most recently seen on each channel, all on one frame.
    struct OffRun { int64_t frame = INT64_MIN; uint32_t count = 0; };
    OffRun  offRun[kPorts][16];
    Counts  counts;
    Stats   stats;
    int     held = 0;
    NoteFn  noteFn  = nullptr;
    void*   noteCtx = nullptr;
};

#endif
