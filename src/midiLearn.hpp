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

    const MidiLearnBindings& bindings() const { return bindings_; }
    // From a loaded project. Not an edit, so onEdited does not fire.
    void setBindings(const MidiLearnBindings& b);

    // The next CC, pitch bend or channel pressure to arrive is bound to `type`.
    // Only one type learns at a time; starting another cancels the first.
    void startLearn(const std::string& type);
    void cancelLearn();
    bool isLearning(const std::string& type) const { return !learning_.empty() && learning_ == type; }

    void clear(const std::string& type);
    const MidiSrc* bindingFor(const std::string& type) const;
    // Last value received for `type`, in that lane's units (see laneMaxValue()).
    std::optional<int> value(const std::string& type) const;

    // Feed one raw channel message. Completes a pending learn first. Returns true,
    // with the bound type and its value scaled to the lane's range, when the message
    // comes from a bound control; false for anything else.
    bool handle(const uint8_t* data, int len, std::string& typeOut, int& valueOut);

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

    void displayChanged() { if (onDisplayChanged) onDisplayChanged(); }
};

#endif
