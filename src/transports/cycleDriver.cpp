// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#include "cycleDriver.hpp"
#include <algorithm>

bool CycleEventBuffer::push(int port, uint32_t frame, const uint8_t* data, int len)
{
    if (count >= cap) return false;
    Event& e = buf[count];
    e.frame = frame;
    e.seq   = static_cast<uint32_t>(count);
    e.port  = static_cast<int16_t>(port);
    e.len   = static_cast<uint8_t>(len < 3 ? len : 3);
    for (int i = 0; i < e.len; i++) e.data[i] = data[i];
    count++;
    return true;
}

void CycleEventBuffer::sort()
{
    // seq is the insertion index, a total-order tiebreak, so an in-place sort keeps
    // same-frame events in emission order without stable_sort's temporary buffer.
    std::sort(buf.get(), buf.get() + count, [](const Event& a, const Event& b) {
        if (a.port  != b.port)  return a.port  < b.port;
        if (a.frame != b.frame) return a.frame < b.frame;
        return a.seq < b.seq;
    });
}

CycleDriver::CycleDriver(double sampleRate, size_t eventCapacity)
    : events(eventCapacity), sr(sampleRate)
{
    setSampleRate(sampleRate);
}

void CycleDriver::runCycle(const ClockSample& c)
{
    RtStats::bump(stats.cycles);
    if (c.relocated) RtStats::bump(stats.relocates);
    curFrame  = c.frame;
    curFrames = c.nframes;

    // Both ends from the integer frame counter with the same expression, so this
    // window's end is bit-identical to the next one's start (see renderCycle).
    renderCycle(c.rolling, c.relocated,
                static_cast<double>(c.frame)             / sr,
                static_cast<double>(c.frame + c.nframes) / sr);
    wasPlaying = c.rolling;
    events.sort();
}

void CycleDriver::emit(const std::string& port, double bar, const uint8_t* data, int len)
{
    // Frame within this window, honouring the active sub-segment's time base so an
    // event past a loop wrap lands past the seam rather than at frame 0. A note-off
    // ends one frame early (half-open interval), so a flush same-pitch note
    // re-attacks.
    long off = segFrameOffset(bar);
    if (isNoteOff(data, len)) off -= 1;
    if (off < -1 || (curFrames > 0 && off > curFrames)) RtStats::bump(stats.clamped);
    if (off < 0) off = 0;
    if (curFrames > 0 && off > static_cast<long>(curFrames) - 1)
        off = static_cast<long>(curFrames) - 1;

    const int idx = resolvePort(port);
    if (idx < 0) {
        emitUnbuffered(port, off, data, len);
        return;
    }
    if (!events.push(idx, static_cast<uint32_t>(off), data, len))
        RtStats::bump(stats.dropped);
}
