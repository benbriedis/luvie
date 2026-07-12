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

// The beat definition a time signature implies: the crotchet, except for the
// compound signatures (an /8 whose numerator groups into threes: 6/8, 9/8, 12/8)
// which are counted in dotted crotchets. Every place the UI changes a time
// signature snaps the beat to this; the user is free to pick a different beat
// from the dropdown afterwards, and that choice sticks until the signature
// changes again.
inline constexpr BeatUnit impliedBeatUnit(int top, int bottom)
{
	if (bottom == 8 && top % 3 == 0) return BeatUnit::DottedCrotchet;
	return BeatUnit::Crotchet;
}

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

// One time-signature unit — one grid column of a pattern in that signature — in
// crotchets: a crotchet in x/4, a quaver in x/8, a minim in x/2.
inline constexpr double unitCrotchets(int bottom)
{
	return bottom > 0 ? 4.0 / (double)bottom : 1.0;
}

// How many of a pattern's grid beats elapse over one song bar — the factor that
// converts pattern beats to song bars and back.
//
// A pattern is timed by its OWN time signature and beat definition, not the
// song's: at 120 BPM a pattern whose beat is the crotchet plays 120 crotchets a
// minute, so one of its grid columns lasts unitCrotchets(its denominator) at
// that rate. Since the song bar it is measured against is timed the same way off
// the song's signature, the shared BPM cancels and this ratio is a constant.
//
// The consequence users see: a one-bar 4/4 pattern and a one-bar 8/8 pattern,
// both counted in crotchets, are both four crotchets long and so take the same
// time to play — the 8/8 one simply runs eight columns through the same bar.
inline constexpr double patternBeatsPerBar(double songBarCrotchets, BeatUnit songBeat,
                                           int patternBottom, BeatUnit patternBeat)
{
	// The pattern column measured in the song's crotchets: its own note value,
	// rescaled when the two beat definitions disagree (a pattern counted in
	// dotted crotchets runs 1.5x the crotchet rate of a song counted in crotchets).
	double columnCrotchets = unitCrotchets(patternBottom)
	                       * beatCrotchets(songBeat) / beatCrotchets(patternBeat);
	return columnCrotchets > 0.0 ? songBarCrotchets / columnCrotchets : 1.0;
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
