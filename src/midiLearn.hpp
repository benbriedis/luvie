// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#ifndef MIDI_LEARN_HPP
#define MIDI_LEARN_HPP

#include "timelineIO.hpp"   // MidiSrc, MidiLearnBindings
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>

// Which hardware control drives which param-lane type, plus the value each bound
// type last received. One binding per type for the whole project, so learning
// Modulation once serves every pattern's Modulation lane and the Song Editor's.
//
// UI thread only: fed from the MIDI input's sink, after the RT ring has been
// drained. The bindings are saved with the project; the values are not.
class MidiLearnMap {
public:
    MidiLearnMap() : bindings_(defaultMidiLearnBindings()) {}

    // A binding changed because the user did something: the project is dirty.
    std::function<void()> onEdited;
    // Anything a label shows changed (a binding, the learn state, a value).
    std::function<void()> onDisplayChanged;
    // The value range of `type` on the instrument it drives (see paramMaxValue()),
    // which handle() scales incoming values to. 127 when unset.
    std::function<int(const std::string& type)> laneMaxFor;
    // A learn began. The app cancels any scene-trigger learn, so one control never
    // completes two learns at once.
    std::function<void()> onLearnStarted;

    const MidiLearnBindings& bindings() const { return bindings_; }
    // From a loaded project. Not an edit, so onEdited does not fire.
    void setBindings(const MidiLearnBindings& b);

    // The next CC, pitch bend or channel pressure to arrive is bound to `type`.
    // Only one type learns at a time; starting another cancels the first.
    void startLearn(const std::string& type);
    // The next control to arrive makes a new parameter on the instrument: it is
    // handed to `complete`, which creates the parameter — sending what the control
    // sends — or picks the one already doing so, adds its lane, and returns its
    // name, which the control is then bound to. Empty binds nothing. Cancels, and
    // is cancelled by, any other learn.
    void startLearnNew(int instrumentId, std::function<std::string(const MidiSrc&)> complete);
    void cancelLearn();
    bool isLearning(const std::string& type) const { return !learning_.empty() && learning_ == type; }
    bool isLearningNew() const { return learningNew_ != 0; }

    // A parameter was renamed. Its control moves to the new name, or with keepOld
    // (another instrument still has a parameter of the old name) stays where it is.
    void rename(const std::string& from, const std::string& to, bool keepOld);

    void clear(const std::string& type);
    const MidiSrc* bindingFor(const std::string& type) const;
    // Last value received for `type`, in that lane's units (see laneMaxFor).
    std::optional<int> value(const std::string& type) const;

    // Feed one raw channel message. Completes a pending learn first. Returns true,
    // with the bound type, the control and its raw value, when the message comes
    // from a bound control; false for anything else. The caller scales the value
    // (scaleToLane) to the range the type has on each instrument it reaches.
    bool handle(const uint8_t* data, int len, std::string& typeOut, MidiSrc& srcOut, int& rawOut);
    static int scaleToLane(const MidiSrc& src, int raw, int laneMax);

    // Short name for a label: "CC1", "Bend", "Pressure".
    static std::string describe(const MidiSrc& src);

    // The context-menu item that starts (or, mid-learn, cancels) learning `type`,
    // naming the current binding: "MIDI learn (CC1)".
    std::string learnMenuLabel(const std::string& type) const;
    // What that item does when chosen.
    void toggleLearn(const std::string& type) {
        if (isLearning(type)) cancelLearn(); else startLearn(type);
    }

private:
    MidiLearnBindings          bindings_;
    std::map<std::string, int> values_;
    std::string                learning_;
    int                        learningNew_ = 0;   // instrument, for startLearnNew()
    std::function<std::string(const MidiSrc&)> learnNewComplete_;

    void displayChanged() { if (onDisplayChanged) onDisplayChanged(); }
};

#endif
