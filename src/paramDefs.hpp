// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#ifndef PARAM_DEFS_HPP
#define PARAM_DEFS_HPP

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

// What a param lane sends to its instrument's synth. A lane names its parameter
// (ParamLane::type) and the instrument's ParamDef of that name says what to send.
// Synths disagree about what a CC does, so the definitions belong to the
// instrument: "Cutoff" can be CC74 on one synth and CC19 on another.
//
// The standard MIDI parameters below are presets: an instrument that has no
// definition of its own for "Volume" uses the standard one. A definition of the
// same name on the instrument overrides it, so a user can point "Volume" at
// another CC, or rename and retarget anything.
//
// Which hardware control *drives* a parameter is a separate thing — see
// MidiLearnMap. The pitch wheel can drive a "Cutoff" that sends CC74.
enum class ParamOutKind { CC, PitchBend, Pressure };

// The value a parameter rests at: where a new lane starts, and for Centre, the
// line its lane draws through the middle.
enum class ParamRest { Min, Centre, Max };

struct ParamDef {
    std::string  name;
    ParamOutKind kind = ParamOutKind::CC;
    int          cc   = 0;               // CC only: 0-127
    ParamRest    rest = ParamRest::Min;
    // The output range a lane's full sweep is mapped onto, so a lane can focus on
    // part of what the synth's control covers. Either way round: min above max
    // inverts it. -1 for outMax is the top of the output (paramMaxValue()), which
    // stays the top when the output changes kind.
    int          outMin = 0;
    int          outMax = -1;

    bool sameOutput(const ParamDef& o) const {
        return kind == o.kind && (kind != ParamOutKind::CC || cc == o.cc);
    }
    bool operator==(const ParamDef& o) const;
};

// 16383 for pitch bend (14-bit), 127 for a CC or channel pressure.
inline int paramMaxValue(ParamOutKind kind)
{
    return kind == ParamOutKind::PitchBend ? 16383 : 127;
}
inline int paramMaxValue(const ParamDef& d) { return paramMaxValue(d.kind); }

// The output range, in the output's own units.
inline int paramOutMin(const ParamDef& d) { return std::clamp(d.outMin, 0, paramMaxValue(d)); }
inline int paramOutMax(const ParamDef& d)
{
    return d.outMax < 0 ? paramMaxValue(d) : std::clamp(d.outMax, 0, paramMaxValue(d));
}

inline bool ParamDef::operator==(const ParamDef& o) const
{
    return name == o.name && sameOutput(o) && rest == o.rest
        && paramOutMin(*this) == paramOutMin(o) && paramOutMax(*this) == paramOutMax(o);
}

// What a lane value (0..paramMaxValue()) sends: mapped onto the parameter's
// output range, rounded to nearest. Allocation-free.
inline int paramOutValue(const ParamDef& d, int laneValue)
{
    const int top = paramMaxValue(d);
    const int lo  = paramOutMin(d);
    const int hi  = paramOutMax(d);
    const int v   = std::clamp(laneValue, 0, top);
    if (lo == 0 && hi == top) return v;
    const long long span = (long long)(hi - lo) * v;
    return lo + (int)((span + (span < 0 ? -top / 2 : top / 2)) / top);
}

// Where a new lane starts.
inline int paramDefaultValue(const ParamDef& d)
{
    const int maxVal = paramMaxValue(d);
    switch (d.rest) {
        case ParamRest::Centre: return (maxVal + 1) / 2;   // 64, or 8192 for bend
        case ParamRest::Max:    return maxVal;
        default:                return 0;
    }
}

struct StandardParam {
    const char*  name;
    ParamOutKind kind;
    int          cc;
    ParamRest    rest;
    bool         core;   // listed in the parameter menu itself, not under "Other"
};

// The General MIDI controllers worth automating, under their usual names.
inline constexpr StandardParam kStandardParams[] = {
    {"Pitch",           ParamOutKind::PitchBend, 0,  ParamRest::Centre, true},
    {"Modulation",      ParamOutKind::CC,        1,  ParamRest::Min,    true},
    {"Volume",          ParamOutKind::CC,        7,  ParamRest::Max,    true},
    {"Pan",             ParamOutKind::CC,        10, ParamRest::Centre, true},
    {"Expression",      ParamOutKind::CC,        11, ParamRest::Max,    true},
    {"Breath",          ParamOutKind::CC,        2,  ParamRest::Min,    false},
    {"Foot",            ParamOutKind::CC,        4,  ParamRest::Min,    false},
    {"Portamento Time", ParamOutKind::CC,        5,  ParamRest::Min,    false},
    {"Balance",         ParamOutKind::CC,        8,  ParamRest::Centre, false},
    {"Sustain",         ParamOutKind::CC,        64, ParamRest::Min,    false},
    {"Portamento",      ParamOutKind::CC,        65, ParamRest::Min,    false},
    {"Sostenuto",       ParamOutKind::CC,        66, ParamRest::Min,    false},
    {"Soft Pedal",      ParamOutKind::CC,        67, ParamRest::Min,    false},
    {"Resonance",       ParamOutKind::CC,        71, ParamRest::Min,    false},
    {"Release",         ParamOutKind::CC,        72, ParamRest::Min,    false},
    {"Attack",          ParamOutKind::CC,        73, ParamRest::Min,    false},
    {"Cutoff",          ParamOutKind::CC,        74, ParamRest::Min,    false},
    {"Reverb",          ParamOutKind::CC,        91, ParamRest::Min,    false},
    {"Chorus",          ParamOutKind::CC,        93, ParamRest::Min,    false},
    {"Pressure",        ParamOutKind::Pressure,  0,  ParamRest::Min,    false},
};

inline ParamDef toParamDef(const StandardParam& s)
{
    return {s.name, s.kind, s.cc, s.rest};
}

inline const StandardParam* standardParam(const std::string& name)
{
    for (const auto& s : kStandardParams)
        if (name == s.name) return &s;
    return nullptr;
}

// The standard parameter that sends this output, if any.
inline const StandardParam* standardParamFor(ParamOutKind kind, int cc)
{
    for (const auto& s : kStandardParams)
        if (s.kind == kind && (kind != ParamOutKind::CC || s.cc == cc)) return &s;
    return nullptr;
}

// A name for a parameter just learned from a control: the standard name if there
// is one ("Cutoff" for CC74), otherwise "CC21".
inline std::string defaultParamName(ParamOutKind kind, int cc)
{
    if (const auto* s = standardParamFor(kind, cc)) return s->name;
    return "CC" + std::to_string(cc);
}

// Short description of an output, for labels: "CC74", "Bend", "Pressure".
inline std::string describeParamOutput(ParamOutKind kind, int cc)
{
    switch (kind) {
        case ParamOutKind::PitchBend: return "Bend";
        case ParamOutKind::Pressure:  return "Pressure";
        default:                      return "CC" + std::to_string(cc);
    }
}
inline std::string describeParamOutput(const ParamDef& d) { return describeParamOutput(d.kind, d.cc); }

// A parameter's output as one int, for the playback engines' event tables: the CC
// number 0-127, or one of these.
inline constexpr int kParamOutBend     = -1;
inline constexpr int kParamOutPressure = -2;
inline int paramOutCode(const ParamDef& d)
{
    switch (d.kind) {
        case ParamOutKind::PitchBend: return kParamOutBend;
        case ParamOutKind::Pressure:  return kParamOutPressure;
        default:                      return d.cc & 0x7F;
    }
}

// The channel message that sets a parameter: m gets the bytes, the return is how
// many (2 for pressure, otherwise 3). value is clamped to the output's range.
// Allocation-free, so the RT engine uses it too.
inline int encodeParamMessage(int outCode, int ch, int value, uint8_t m[3])
{
    ch &= 0x0F;
    if (outCode == kParamOutBend) {
        value = std::clamp(value, 0, 16383);
        m[0] = static_cast<uint8_t>(0xE0 | ch);
        m[1] = static_cast<uint8_t>(value & 0x7F);
        m[2] = static_cast<uint8_t>((value >> 7) & 0x7F);
        return 3;
    }
    value = std::clamp(value, 0, 127);
    if (outCode == kParamOutPressure) {
        m[0] = static_cast<uint8_t>(0xD0 | ch);
        m[1] = static_cast<uint8_t>(value);
        return 2;
    }
    m[0] = static_cast<uint8_t>(0xB0 | ch);
    m[1] = static_cast<uint8_t>(outCode & 0x7F);
    m[2] = static_cast<uint8_t>(value);
    return 3;
}

// The instrument's own definition of `name`, else the standard one, else nothing:
// a lane naming neither is not sent at all.
inline bool resolveParamDef(const std::vector<ParamDef>& instDefs, const std::string& name,
                            ParamDef& out)
{
    for (const auto& d : instDefs)
        if (d.name == name) { out = d; return true; }
    if (const auto* s = standardParam(name)) { out = toParamDef(*s); return true; }
    return false;
}

#endif
