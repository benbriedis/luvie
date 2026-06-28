#ifndef NOTE_AUDITIONER_HPP
#define NOTE_AUDITIONER_HPP

#include "playhead.hpp"   // MidiInstrRoute
#include <functional>
#include <string>
#include <vector>

class PortRegistry;

// Plays one-off audition notes — e.g. clicking a row label in a pattern editor.
// Sends note-on immediately and schedules the matching note-off after a wall-clock
// duration via an FLTK timeout. Routes through the same PortRegistry + instrument
// routing as the song playhead, so a click sounds on the selected instrument's port.
class NoteAuditioner {
public:
    ~NoteAuditioner();

    void setPortRegistry(PortRegistry* r)                  { portReg = r; }
    void setInstrRoute(std::function<MidiInstrRoute(int)> r) { instrRoute = std::move(r); }

    // Alternative sink used when there is no local PortRegistry (LV2 plugin mode):
    // the note is emitted by the host instead, e.g. forwarded to the DSP's MIDI out.
    // (ch, midi, velocity, on) — on=false is the matching note-off.
    void setMidiSink(std::function<void(int, int, int, bool)> s) { midiSink = std::move(s); }

    // Note-on to the instrument's port now; note-off after `seconds`.
    void play(int instrumentId, int midi, int velocity, float seconds);

private:
    struct Pending { NoteAuditioner* self; std::string portName; int channel; int pitch; };
    static void offCb(void* data);
    void        sendOff(const Pending* p);

    PortRegistry*                      portReg = nullptr;
    std::function<MidiInstrRoute(int)> instrRoute;
    std::function<void(int, int, int, bool)> midiSink;   // plugin-mode emission
    std::vector<Pending*>              pending;   // outstanding timeout payloads
};

#endif
