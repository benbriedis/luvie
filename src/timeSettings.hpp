#ifndef TIME_SETTINGS_HPP
#define TIME_SETTINGS_HPP

#include <array>

// Single source of truth for the time-signature, beat-definition and tempo
// settings the UI offers: the denominator dropdown options, the numerator range,
// the beat-definition dropdown options and the BPM range. Used by the song
// editor's time-signature ruler, the Loop Editor panel and the pattern editors'
// control bar.
//
// Tempo vocabulary — the two are NOT the same number:
//   BPM  beats per minute, where one beat is the BeatUnit stored next to the
//        time signature. This is what the user types.
//   CPM  crotchets per minute. This is what JACK and LV2 mean when they say
//        "BPM", and what all bar/seconds conversion runs on.
// CPM = BPM * beatCrotchets(beat).
namespace timeSettings {

// Denominator dropdown: the note value that one time-signature unit is worth.
inline constexpr std::array<int, 3>         denominators      = {2, 4, 8};
inline constexpr std::array<const char*, 3> denominatorLabels = {"2", "4", "8"};
inline constexpr int denominatorDefault      = 4;
inline constexpr int denominatorDefaultIndex = 1;

inline constexpr int numeratorMin     = 1;
inline constexpr int numeratorMax     = 17;
inline constexpr int numeratorDefault = 4;

inline constexpr double bpmMin     = 20.0;
inline constexpr double bpmMax     = 400.0;
inline constexpr double bpmDefault = 120.0;

// Beat definition: the note value one BPM beat is worth. Stored alongside every
// time signature. The enum values are persisted in the song file, so they must
// stay stable; 0 (Crotchet) is the value an older file without one loads as.
enum class BeatUnit { Crotchet = 0, DottedCrotchet = 1, Minim = 2 };

inline constexpr std::array<BeatUnit, 3> beatUnits =
	{BeatUnit::Crotchet, BeatUnit::DottedCrotchet, BeatUnit::Minim};
inline constexpr BeatUnit beatUnitDefault      = BeatUnit::Crotchet;
inline constexpr int      beatUnitDefaultIndex = 0;

// The beat's length in crotchets. The one number that turns a BPM into a CPM.
inline constexpr double beatCrotchets(BeatUnit u)
{
	switch (u) {
	case BeatUnit::DottedCrotchet: return 1.5;
	case BeatUnit::Minim:          return 2.0;
	case BeatUnit::Crotchet:       break;
	}
	return 1.0;
}

// One bar's length in crotchets, from the time signature alone. A 6/8 bar is
// three crotchets whether its beat is a crotchet or a dotted crotchet.
inline constexpr double barCrotchets(int top, int bottom)
{
	return bottom > 0 ? (double)top * 4.0 / (double)bottom : (double)top;
}

// How long one bar lasts, from the two quantities above. Every bar<->seconds
// conversion in the app funnels through this so they cannot drift apart.
inline constexpr double secondsPerBar(double barCrotchetCount, double cpm)
{
	return cpm > 0.0 ? barCrotchetCount * 60.0 / cpm : 0.0;
}

// Dropdown index for a denominator. A value that is no longer offered (e.g. a 16
// loaded from an older song file) falls back to the default entry.
inline int denominatorIndex(int denominator)
{
	for (int i = 0; i < (int)denominators.size(); ++i)
		if (denominators[i] == denominator) return i;
	return denominatorDefaultIndex;
}

// Denominator for a dropdown index; an unset selection (-1) gives the default.
inline int denominatorAt(int index)
{
	if (index < 0 || index >= (int)denominators.size()) return denominatorDefault;
	return denominators[index];
}

inline int beatUnitIndex(BeatUnit u)
{
	for (int i = 0; i < (int)beatUnits.size(); ++i)
		if (beatUnits[i] == u) return i;
	return beatUnitDefaultIndex;
}

// Beat definition for a dropdown index; an unset selection (-1) gives the default.
inline BeatUnit beatUnitAt(int index)
{
	if (index < 0 || index >= (int)beatUnits.size()) return beatUnitDefault;
	return beatUnits[index];
}

}

#endif
