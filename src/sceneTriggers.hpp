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

// Which controller button does what to the Loop Editor's scenes: MIDI learn for
// the scene buttons. Each scene has its own trigger, and three more slots are
// shared by all of them — Next, Previous and First scene — so a controller can
// step through the scenes without a button per scene. A trigger is a note, a CC or
// a program change, on a given input and channel, and learning listens to every
// input.
//
// Slots 0..kScenes-1 are the scenes themselves (0 is Scene S); the navigation
// slots follow. Everything below that takes an int takes a slot.
//
// UI thread only: fed from the MIDI input's sink, after the RT ring has been
// drained. The triggers are saved with the project (AppState::sceneTriggers).
class SceneTriggerMap {
public:
    static constexpr int kNextScene  = SceneBank::kScenes;
    static constexpr int kPrevScene  = SceneBank::kScenes + 1;
    static constexpr int kFirstScene = SceneBank::kScenes + 2;
    static constexpr int kSlots      = SceneBank::kScenes + 3;
    static bool isNavSlot(int slot) { return slot >= kNextScene && slot < kSlots; }

    using Triggers = std::array<MidiTrigger, kSlots>;

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

    // The next note, CC or program change to arrive, on any input, is bound to
    // `slot`. Only one slot learns at a time; starting another cancels the first.
    void startLearn(int slot);
    void cancelLearn();
    bool isLearning(int slot) const { return learning_ >= 0 && learning_ == slot; }
    // The slot waiting for a trigger, or -1.
    int  learningSlot() const { return learning_; }
    void toggleLearn(int slot) { if (isLearning(slot)) cancelLearn(); else startLearn(slot); }

    void clear(int slot);
    const MidiTrigger* bindingFor(int slot) const;

    // Feed one raw channel message from input `input`. Completes a pending learn
    // first. Returns the slot whose trigger fired, or -1; targetScene() turns that
    // into a scene. `consumed` is set when the message belongs to a trigger — its
    // press, or a note trigger's release — so the caller sends it nowhere else.
    int handle(const std::string& input, const uint8_t* data, int len, bool& consumed);

    // The scene a fired slot switches to, from the scene currently shown. Next and
    // Previous stop at the ends rather than wrapping: First is the way back round.
    // First is Scene 1, the first of the user's own; Scene S, the song-linked one,
    // is still reachable one step before it with Previous.
    static int targetScene(int slot, int shown);

    // An input was renamed: its triggers follow it.
    void renameInput(const std::string& oldName, const std::string& newName);

    // Short description for a tooltip: "Note 36 ch10, pads", or without the input
    // where space is tight.
    static std::string describe(const MidiTrigger& t, bool withInput = true);
    // What a slot is, for a menu: "Next scene". Scenes are "Scene S", "Scene 1"...
    static std::string slotName(int slot);
    // The context-menu item that starts (or, mid-learn, cancels) learning `slot`,
    // naming the current trigger: "MIDI learn (Note 36 ch10)" for the scene's own,
    // "MIDI learn Next scene (CC20=1 ch1)" for a navigation slot.
    std::string learnMenuLabel(int slot) const;

private:
    Triggers triggers_{};
    int      learning_ = -1;

    void displayChanged() { if (onDisplayChanged) onDisplayChanged(); }
};

static_assert(std::tuple_size_v<decltype(AppState::sceneTriggers)> == SceneTriggerMap::kSlots,
              "AppState::sceneTriggers holds one trigger per SceneTriggerMap slot");

#endif
