// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#include "sceneTriggers.hpp"
#include "luvieDebug.hpp"
#include <cstdio>

namespace {

// The trigger a message would be, and whether it is a press (note on, any CC,
// program change) or a release (note off). Kind None for anything that cannot be
// a trigger. A CC carries its value; see handle() for how that is matched.
//
// Every CC value is a press. Plenty of controller buttons are toggles, sending 127
// on one press and 0 on the next, and counting only the 127 would make every other
// press do nothing. A momentary button's release just picks the same scene again,
// which is a no-op.
MidiTrigger triggerOf(const std::string& input, const uint8_t* d, int len, bool& press)
{
    MidiTrigger t;
    if (len < 2) return t;
    t.channel = d[0] & 0x0F;
    t.num     = d[1] & 0x7F;
    t.input   = input;
    const int value = len >= 3 ? (d[2] & 0x7F) : 0;
    switch (d[0] & 0xF0) {
    case 0x90: t.kind = MidiTriggerKind::Note;    press = value > 0; break;
    case 0x80: t.kind = MidiTriggerKind::Note;    press = false;     break;
    case 0xB0: t.kind = MidiTriggerKind::CC;      press = true; t.value = value; break;
    case 0xC0: t.kind = MidiTriggerKind::Program; press = true;      break;
    default:   return {};
    }
    if (len < 3 && t.kind != MidiTriggerKind::Program) return {};
    return t;
}

// Bank select, MSB and LSB. A controller button that sends a program change
// usually sends these first, with the same values whichever button it is, so they
// say nothing about which button was pressed.
bool isBankSelect(const MidiTrigger& t)
{
    return t.kind == MidiTriggerKind::CC && (t.num == 0 || t.num == 32);
}

const char* sceneName(int scene)
{
    static const char* names[SceneBank::kScenes] = {"S", "1", "2", "3", "4"};
    return scene >= 0 && scene < SceneBank::kScenes ? names[scene] : "?";
}

} // namespace

void SceneTriggerMap::setTriggers(const Triggers& t)
{
    triggers_ = t;
    learning_ = -1;
    displayChanged();
}

void SceneTriggerMap::startLearn(int scene)
{
    if (scene < 0 || scene >= SceneBank::kScenes) return;
    if (luvieDebug()) fprintf(stderr, "[luvie] scene learn: waiting for a trigger for scene %s\n",
                              sceneName(scene));
    learning_ = scene;
    if (onLearnStarted) onLearnStarted();
    displayChanged();
}

void SceneTriggerMap::cancelLearn()
{
    if (learning_ < 0) return;
    if (luvieDebug()) fprintf(stderr, "[luvie] scene learn: cancelled for scene %s\n",
                              sceneName(learning_));
    learning_ = -1;
    displayChanged();
}

void SceneTriggerMap::clear(int scene)
{
    if (scene < 0 || scene >= SceneBank::kScenes) return;
    if (isLearning(scene)) learning_ = -1;
    const bool had = triggers_[scene].kind != MidiTriggerKind::None;
    triggers_[scene] = {};
    if (had && onEdited) onEdited();
    displayChanged();
}

const MidiTrigger* SceneTriggerMap::bindingFor(int scene) const
{
    if (scene < 0 || scene >= SceneBank::kScenes) return nullptr;
    const MidiTrigger& t = triggers_[scene];
    return t.kind == MidiTriggerKind::None ? nullptr : &t;
}

int SceneTriggerMap::handle(const std::string& input, const uint8_t* data, int len, bool& consumed)
{
    consumed = false;
    bool press = false;
    const MidiTrigger t = triggerOf(input, data, len, press);
    if (t.kind == MidiTriggerKind::None) {
        if (luvieDebug() && learning_ >= 0)
            fprintf(stderr, "[luvie] scene learn: ignored %02X, not a note, CC or program "
                            "change\n", len > 0 ? data[0] : 0);
        return -1;
    }

    // Bank select is the preamble to a program change, never a trigger itself: were
    // it learned, every button in a row would bind the same bank select. It is
    // swallowed while learning, and wherever program changes switch scenes, so the
    // instrument is not left holding half a program change.
    if (isBankSelect(t)) {
        consumed = learning_ >= 0;
        for (const auto& b : triggers_)
            if (b.kind == MidiTriggerKind::Program && b.channel == t.channel && b.input == t.input)
                consumed = true;
        return -1;
    }

    if (learning_ >= 0 && press) {
        // One button switches to one scene: taking it for this scene takes it away
        // from whichever scene had it. A CC trigger saved without a value stands for
        // every value, so it goes too.
        for (auto& other : triggers_)
            if (other == t || (other.sameControl(t) && other.value < 0)) other = {};
        triggers_[learning_] = t;
        if (luvieDebug())
            fprintf(stderr, "[luvie] scene learn: scene %s -> %s\n",
                    sceneName(learning_), describe(t).c_str());
        learning_ = -1;
        consumed  = true;
        if (onEdited) onEdited();
        displayChanged();
        return -1;
    }

    // An exact match wins. Failing that, a control bound to one scene only fires it
    // whatever the value, so a toggle button's other value and a momentary button's
    // release still count. A control shared by several scenes, told apart by value,
    // fires only on the values they were learned with.
    int exact = -1, any = -1, sharing = 0;
    for (int s = 0; s < SceneBank::kScenes; s++) {
        if (!triggers_[s].sameControl(t)) continue;
        consumed = true;
        any = s;
        sharing++;
        if (triggers_[s].value == t.value) exact = s;
    }
    if (!press) return -1;
    if (exact >= 0)   return exact;
    if (sharing == 1) return any;
    return -1;
}

void SceneTriggerMap::renameInput(const std::string& oldName, const std::string& newName)
{
    bool changed = false;
    for (auto& t : triggers_)
        if (t.kind != MidiTriggerKind::None && t.input == oldName) {
            t.input = newName;
            changed = true;
        }
    if (!changed) return;
    if (onEdited) onEdited();
    displayChanged();
}

std::string SceneTriggerMap::describe(const MidiTrigger& t, bool withInput)
{
    std::string s;
    switch (t.kind) {
    case MidiTriggerKind::Note:    s = "Note "    + std::to_string(t.num); break;
    case MidiTriggerKind::CC:
        s = "CC" + std::to_string(t.num);
        if (t.value >= 0) s += "=" + std::to_string(t.value);
        break;
    case MidiTriggerKind::Program: s = "Program " + std::to_string(t.num); break;
    case MidiTriggerKind::None:    return {};
    }
    s += " ch" + std::to_string(t.channel + 1);
    if (withInput && !t.input.empty()) s += ", " + t.input;
    return s;
}

std::string SceneTriggerMap::learnMenuLabel(int scene) const
{
    if (isLearning(scene)) return "Cancel MIDI learn";
    const MidiTrigger* t = bindingFor(scene);
    return t ? "MIDI learn (" + describe(*t, false) + ")" : "MIDI learn";
}
