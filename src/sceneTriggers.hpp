// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#ifndef SCENE_TRIGGERS_HPP
#define SCENE_TRIGGERS_HPP

#include "timelineIO.hpp"   // MidiTrigger
#include "sceneBank.hpp"
#include <array>
#include <cstdint>
#include <functional>
#include <string>

// Which controller button switches the Loop Editor to which scene: MIDI learn for
// the scene buttons. A trigger is a note, a CC or a program change, on a given
// input and channel, and learning listens to every input.
//
// UI thread only: fed from the MIDI input's sink, after the RT ring has been
// drained. The triggers are saved with the project (AppState::sceneTriggers).
class SceneTriggerMap {
public:
    using Triggers = std::array<MidiTrigger, SceneBank::kScenes>;

    // A trigger changed because the user did something: the project is dirty.
    std::function<void()> onEdited;
    // Anything a scene button shows changed (a trigger or the learn state).
    std::function<void()> onDisplayChanged;
    // A learn began. The app cancels any param-lane learn, so one control never
    // completes two learns at once.
    std::function<void()> onLearnStarted;

    const Triggers& triggers() const { return triggers_; }
    // From a loaded project. Not an edit, so onEdited does not fire.
    void setTriggers(const Triggers& t);

    // The next note, CC or program change to arrive, on any input, triggers `scene`.
    // Only one scene learns at a time; starting another cancels the first.
    void startLearn(int scene);
    void cancelLearn();
    bool isLearning(int scene) const { return learning_ >= 0 && learning_ == scene; }
    bool learning() const { return learning_ >= 0; }
    void toggleLearn(int scene) { if (isLearning(scene)) cancelLearn(); else startLearn(scene); }

    void clear(int scene);
    const MidiTrigger* bindingFor(int scene) const;

    // Feed one raw channel message from input `input`. Completes a pending learn
    // first. Returns the scene to switch to, or -1. `consumed` is set when the
    // message belongs to a trigger — its press, or a note trigger's release — so
    // the caller sends it nowhere else.
    int handle(const std::string& input, const uint8_t* data, int len, bool& consumed);

    // An input was renamed: its triggers follow it.
    void renameInput(const std::string& oldName, const std::string& newName);

    // Short description for a tooltip: "Note 36 ch10, pads", or without the input
    // where space is tight.
    static std::string describe(const MidiTrigger& t, bool withInput = true);
    // The context-menu item that starts (or, mid-learn, cancels) learning `scene`,
    // naming the current trigger: "MIDI learn (Note 36 ch10)".
    std::string learnMenuLabel(int scene) const;

private:
    Triggers triggers_{};
    int      learning_ = -1;

    void displayChanged() { if (onDisplayChanged) onDisplayChanged(); }
};

#endif
