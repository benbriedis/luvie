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

    // Note-on to the instrument's port now; note-off after `seconds`.
    void play(int instrumentId, int midi, int velocity, float seconds);

private:
    struct Pending { NoteAuditioner* self; std::string portName; int channel; int pitch; };
    static void offCb(void* data);

    PortRegistry*                      portReg = nullptr;
    std::function<MidiInstrRoute(int)> instrRoute;
    std::vector<Pending*>              pending;   // outstanding timeout payloads
};

#endif
