// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#pragma once
#include "port.hpp"
#include <memory>
#include <mutex>

class RtMidiOut;

// A "native" MIDI output port backed by RtMidi (ALSA sequencer on Linux,
// CoreMIDI on macOS). It exposes a virtual port that other applications can
// connect to. Playback reaches it through the MidiScheduler, at each message's due
// time. If RtMidi
// can't open, the port becomes a no-op.
class RtMidiPort : public Port {
public:
    explicit RtMidiPort(std::string name);
    ~RtMidiPort() override;

    void rename(const std::string& n) override;

    MidiBackend backend()       const override { return MidiBackend::Native; }

    void noteOn (int ch, int pitch, int vel) override;
    void noteOff(int ch, int pitch)          override;
    void cc     (int ch, int num, int val)   override;
    void pitchBend(int ch, int value14)      override;
    void programChange(int ch, int bankMsb, int bankLsb, int program) override;
    void raw(const uint8_t* msg, int len)    override;

private:
    void send3(int status, int d1, int d2);

    std::unique_ptr<RtMidiOut> out_;   // null if RtMidi could not be opened
    // RtMidiOut is not thread-safe, and two threads send: the MidiScheduler's sender
    // (playback) and the UI thread (auditioning, pass-through, program changes).
    // Neither is a real-time thread, so a plain mutex is fine.
    std::mutex sendMutex_;
};
