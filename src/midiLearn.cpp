// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#include "midiLearn.hpp"
#include "paramLaneTypes.hpp"

namespace {

// The control a message comes from and its raw value: 7-bit for CC and pressure,
// 14-bit for pitch bend. Kind None for anything that is not a learnable control.
MidiSrc sourceOf(const uint8_t* d, int len, int& raw)
{
    if (len < 2) return {};
    switch (d[0] & 0xF0) {
    case 0xB0:
        if (len < 3) return {};
        raw = d[2] & 0x7F;
        return {MidiSrcKind::CC, d[1] & 0x7F};
    case 0xD0:
        raw = d[1] & 0x7F;
        return {MidiSrcKind::Pressure, 0};
    case 0xE0:
        if (len < 3) return {};
        raw = (d[1] & 0x7F) | ((d[2] & 0x7F) << 7);
        return {MidiSrcKind::PitchBend, 0};
    default:
        return {};
    }
}

// Raw control value -> lane units. A 7-bit control on the 14-bit Pitch lane maps
// 64 to 8192 exactly, so a centred knob means no bend, and still reaches both ends.
int scaleToLane(const MidiSrc& src, int raw, int laneMax)
{
    const bool wide = src.kind == MidiSrcKind::PitchBend;
    if (laneMax > 127) {
        if (wide) return raw;
        return raw <= 64 ? raw * 128 : 8192 + (raw - 64) * 8191 / 63;
    }
    return wide ? raw >> 7 : raw;
}

} // namespace

void MidiLearnMap::setBindings(const MidiLearnBindings& b)
{
    bindings_ = b;
    values_.clear();
    learning_.clear();
    displayChanged();
}

void MidiLearnMap::startLearn(const std::string& type)
{
    learning_ = type;
    displayChanged();
}

void MidiLearnMap::cancelLearn()
{
    if (learning_.empty()) return;
    learning_.clear();
    displayChanged();
}

void MidiLearnMap::clear(const std::string& type)
{
    if (isLearning(type)) learning_.clear();
    if (bindings_.erase(type) == 0) { displayChanged(); return; }
    values_.erase(type);
    if (onEdited) onEdited();
    displayChanged();
}

const MidiSrc* MidiLearnMap::bindingFor(const std::string& type) const
{
    auto it = bindings_.find(type);
    return it == bindings_.end() ? nullptr : &it->second;
}

std::optional<int> MidiLearnMap::value(const std::string& type) const
{
    auto it = values_.find(type);
    if (it == values_.end()) return std::nullopt;
    return it->second;
}

bool MidiLearnMap::handle(const uint8_t* data, int len, std::string& typeOut, int& valueOut)
{
    int raw = 0;
    const MidiSrc src = sourceOf(data, len, raw);
    if (src.kind == MidiSrcKind::None) return false;

    if (!learning_.empty()) {
        // One control drives one type: taking it for this type takes it away from
        // whichever type had it, so a fader never silently writes to two lanes.
        for (auto it = bindings_.begin(); it != bindings_.end();) {
            if (it->second == src && it->first != learning_) {
                values_.erase(it->first);
                it = bindings_.erase(it);
            } else {
                ++it;
            }
        }
        bindings_[learning_] = src;
        learning_.clear();
        if (onEdited) onEdited();
    }

    for (const auto& [type, bound] : bindings_) {
        if (bound != src) continue;
        typeOut  = type;
        valueOut = scaleToLane(src, raw, laneMaxValue(type));
        values_[type] = valueOut;
        displayChanged();
        return true;
    }
    return false;
}

std::string MidiLearnMap::describe(const MidiSrc& src)
{
    switch (src.kind) {
    case MidiSrcKind::CC:        return "CC" + std::to_string(src.num);
    case MidiSrcKind::PitchBend: return "Bend";
    case MidiSrcKind::Pressure:  return "Pressure";
    case MidiSrcKind::None:      break;
    }
    return {};
}

std::string MidiLearnMap::learnMenuLabel(const std::string& type) const
{
    if (isLearning(type)) return "Cancel MIDI learn";
    const MidiSrc* src = bindingFor(type);
    return src ? "MIDI learn (" + describe(*src) + ")" : "MIDI learn";
}
