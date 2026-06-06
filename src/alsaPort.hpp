#pragma once
#include "port.hpp"
#include <alsa/asoundlib.h>

// Owns a lazily-opened ALSA sequencer client. Shared by all AlsaPorts. If the
// sequencer can't be opened, seq() returns nullptr and the ports become no-ops.
class AlsaClient {
public:
    ~AlsaClient();
    snd_seq_t* seq();          // opens on first use; nullptr on failure

private:
    snd_seq_t* seq_  = nullptr;
    bool       tried_ = false;
};

// An ALSA sequencer output port. Created/destroyed with the object; driven
// note-by-note by the Playhead (soft-sequenced).
class AlsaPort : public Port {
public:
    AlsaPort(AlsaClient* client, std::string name);
    ~AlsaPort() override;

    void rename(const std::string& n) override;

    MidiBackend backend()       const override { return MidiBackend::Native; }
    bool        softSequenced() const override { return true; }

    void noteOn (int ch, int pitch, int vel) override;
    void noteOff(int ch, int pitch)          override;
    void cc     (int ch, int num, int val)   override;
    void pitchBend(int ch, int value14)      override;
    void programChange(int ch, int bankMsb, int bankLsb, int program) override;

private:
    void send(snd_seq_event_t& ev);

    AlsaClient* client_;
    int         portId_ = -1;
};
