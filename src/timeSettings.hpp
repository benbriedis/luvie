#ifndef TIME_SETTINGS_HPP
#define TIME_SETTINGS_HPP

#include <array>

// Single source of truth for the time-signature and BPM settings the UI offers:
// the denominator dropdown options, the numerator range and the BPM range.
// Used by the song editor's time-signature ruler, the Loop Editor panel and the
// pattern editors' control bar.
namespace timeSettings {

// Denominator dropdown: the note value that one beat is worth.
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

}

#endif
