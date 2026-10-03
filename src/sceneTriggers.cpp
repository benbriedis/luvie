// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#include "sceneTriggers.hpp"
#include "luvieDebug.hpp"
#include <algorithm>
#include <cstdio>

namespace {

// The trigger a message would be, and whether it is a press (note on, any CC,
// program change) or a release (note off). Kind None for anything that cannot be
// a trigger. A CC carries its value; see handle() for how that is matched.
//
// Every CC value is a press. Plenty of controller buttons are toggles, sending 127
// on one press and 0 on the next, and counting only the 127 would make every other
// press do nothing. For a scene's own trigger a momentary button's release just
// picks the same scene again, which is a no-op; handle() keeps the navigation
// slots, where it would not be, to the value they learned.
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

} // namespace

void SceneTriggerMap::setTriggers(const Triggers& t, const PatternTriggers& p,
                                  const std::set<int>& livePatterns)
{
    triggers_ = t;
    patternTriggers_.clear();
    for (const auto& [patId, trig] : p)
        if (trig.kind != MidiTriggerKind::None && livePatterns.count(patId))
            patternTriggers_[patId] = trig;
    learning_        = -1;
    learningPattern_ = -1;
    displayChanged();
}

void SceneTriggerMap::startLearn(int slot)
{
    if (slot < 0 || slot >= kSlots) return;
    if (luvieDebug()) fprintf(stderr, "[luvie] slot learn: waiting for a trigger for %s\n",
                              slotName(slot).c_str());
    learning_        = slot;
    learningPattern_ = -1;
    if (onLearnStarted) onLearnStarted();
    displayChanged();
}

void SceneTriggerMap::cancelLearn()
{
    if (learning_ < 0 && learningPattern_ < 0) return;
    if (luvieDebug()) {
        if (learning_ >= 0)
            fprintf(stderr, "[luvie] scene learn: cancelled for %s\n", slotName(learning_).c_str());
        else
            fprintf(stderr, "[luvie] scene learn: cancelled for pattern %d\n", learningPattern_);
    }
    learning_        = -1;
    learningPattern_ = -1;
    displayChanged();
}

void SceneTriggerMap::startLearnPattern(int patId)
{
    if (patId < 0) return;
    if (luvieDebug()) fprintf(stderr, "[luvie] scene learn: waiting for a trigger for pattern %d\n",
                              patId);
    learning_        = -1;
    learningPattern_ = patId;
    if (onLearnStarted) onLearnStarted();
    displayChanged();
}

void SceneTriggerMap::clearPattern(int patId)
{
    if (isLearningPattern(patId)) learningPattern_ = -1;
    const bool had = patternTriggers_.erase(patId) > 0;
    if (had && onEdited) onEdited();
    displayChanged();
}

const MidiTrigger* SceneTriggerMap::patternBindingFor(int patId) const
{
    auto it = patternTriggers_.find(patId);
    return it == patternTriggers_.end() ? nullptr : &it->second;
}

void SceneTriggerMap::unbind(const MidiTrigger& t)
{
    // A CC trigger saved without a value stands for every value, so it goes too.
    auto taken = [&](const MidiTrigger& other) {
        return other == t || (other.sameControl(t) && other.value < 0);
    };
    for (auto& other : triggers_)
        if (taken(other)) other = {};
    for (auto it = patternTriggers_.begin(); it != patternTriggers_.end();)
        it = taken(it->second) ? patternTriggers_.erase(it) : std::next(it);
}

void SceneTriggerMap::clear(int slot)
{
    if (slot < 0 || slot >= kSlots) return;
    if (isLearning(slot)) learning_ = -1;
    const bool had = triggers_[slot].kind != MidiTriggerKind::None;
    triggers_[slot] = {};
    if (had && onEdited) onEdited();
    displayChanged();
}

const MidiTrigger* SceneTriggerMap::bindingFor(int slot) const
{
    if (slot < 0 || slot >= kSlots) return nullptr;
    const MidiTrigger& t = triggers_[slot];
    return t.kind == MidiTriggerKind::None ? nullptr : &t;
}

SceneTriggerMap::Fired SceneTriggerMap::handle(const std::string& input, const uint8_t* data,
                                               int len, bool& consumed)
{
    consumed = false;
    bool press = false;
    const bool learning = learning_ >= 0 || learningPattern_ >= 0;
    const MidiTrigger t = triggerOf(input, data, len, press);
    if (t.kind == MidiTriggerKind::None) {
        if (luvieDebug() && learning)
            fprintf(stderr, "[luvie] scene learn: ignored %02X, not a note, CC or program "
                            "change\n", len > 0 ? data[0] : 0);
        return {};
    }

    // Bank select is the preamble to a program change, never a trigger itself: were
    // it learned, every button in a row would bind the same bank select. It is
    // swallowed while learning, and wherever program changes switch scenes, so the
    // instrument is not left holding half a program change.
    if (isBankSelect(t)) {
        auto isProgramHere = [&](const MidiTrigger& b) {
            return b.kind == MidiTriggerKind::Program && b.channel == t.channel && b.input == t.input;
        };
        consumed = learning;
        for (const auto& b : triggers_)
            if (isProgramHere(b)) consumed = true;
        for (const auto& [patId, b] : patternTriggers_)
            if (isProgramHere(b)) consumed = true;
        return {};
    }

    if (learning && press) {
        // One button does one thing: taking it for this slot or pattern takes it
        // away from whichever had it.
        unbind(t);
        if (learning_ >= 0) {
            triggers_[learning_] = t;
            if (luvieDebug())
                fprintf(stderr, "[luvie] scene learn: %s -> %s\n",
                        slotName(learning_).c_str(), describe(t).c_str());
        } else {
            patternTriggers_[learningPattern_] = t;
            if (luvieDebug())
                fprintf(stderr, "[luvie] scene learn: pattern %d -> %s\n",
                        learningPattern_, describe(t).c_str());
        }
        learning_        = -1;
        learningPattern_ = -1;
        consumed         = true;
        if (onEdited) onEdited();
        displayChanged();
        return {};
    }

    // An exact match wins. Failing that, a control bound to one scene only fires it
    // whatever the value, so a toggle button's other value and a momentary button's
    // release still count. A control shared by several scenes, told apart by value,
    // fires only on the values they were learned with.
    //
    // The navigation slots only ever match exactly. Firing Next on a momentary
    // button's release would step twice per press; the price is that a toggle
    // button steps on every other press. Patterns and the transport slots are the
    // same, for the same reason: a release would toggle them straight back (or
    // rewind twice, harmlessly, but a toggle button would then rewind on release).
    int exactPattern = -1;
    for (const auto& [patId, b] : patternTriggers_) {
        if (!b.sameControl(t)) continue;
        consumed = true;
        if (b.value == t.value) exactPattern = patId;
    }
    if (press && exactPattern >= 0) return {-1, exactPattern};

    int exact = -1, any = -1, sharing = 0;
    for (int s = 0; s < kSlots; s++) {
        if (!triggers_[s].sameControl(t)) continue;
        consumed = true;
        if (triggers_[s].value == t.value) exact = s;
        if (!isSceneSlot(s)) continue;
        any = s;
        sharing++;
    }
    if (!press) return {};
    if (exact >= 0)   return {exact, -1};
    if (sharing == 1) return {any, -1};
    return {};
}

void SceneTriggerMap::renameInput(const std::string& oldName, const std::string& newName)
{
    bool changed = false;
    for (auto& t : triggers_)
        if (t.kind != MidiTriggerKind::None && t.input == oldName) {
            t.input = newName;
            changed = true;
        }
    for (auto& [patId, t] : patternTriggers_)
        if (t.input == oldName) {
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

std::string SceneTriggerMap::slotName(int slot)
{
    switch (slot) {
    case kNextScene:  return "Next scene";
    case kPrevScene:  return "Previous scene";
    case kFirstScene: return "First scene";
    case kPlayPause:  return "Play/Pause";
    case kRewind:     return "Rewind";
    case SceneBank::kSceneSong: return "Scene S";
    default: return "Scene " + std::to_string(slot);
    }
}

std::string SceneTriggerMap::learnMenuLabel(int slot) const
{
    const std::string what = isNavSlot(slot) ? "MIDI learn " + slotName(slot) : "MIDI learn";
    if (isLearning(slot)) return "Cancel " + what;
    const MidiTrigger* t = bindingFor(slot);
    return t ? what + " (" + describe(*t, false) + ")" : what;
}

int SceneTriggerMap::targetScene(int slot, int shown)
{
    switch (slot) {
    case kNextScene:  return std::min(shown + 1, SceneBank::kScenes - 1);
    case kPrevScene:  return std::max(shown - 1, 0);
    case kFirstScene: return 1;
    default:          return slot;
    }
}

std::string SceneTriggerMap::patternLearnMenuLabel(int patId) const
{
    if (isLearningPattern(patId)) return "Cancel MIDI learn";
    const MidiTrigger* t = patternBindingFor(patId);
    return t ? "MIDI learn (" + describe(*t, false) + ")" : "MIDI learn";
}
