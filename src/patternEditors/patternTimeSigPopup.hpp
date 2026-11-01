// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#ifndef PATTERN_TIME_SIG_POPUP_HPP
#define PATTERN_TIME_SIG_POPUP_HPP

#include <FL/Fl_Box.H>
#include <FL/Fl_Value_Input.H>
#include "modernButton.hpp"
#include "modern/denomBeatChoice.hpp"
#include "modern/inputEditorPopup.hpp"
#include "timeSettings.hpp"
#include <functional>

// Edits a pattern's time signature. The pattern is `bars * numerator` beats long,
// so a new signature needs a new bar count too: the popup shows (read-only) the
// fewest bars of the new signature that still hold the whole pattern, and that is
// what the pattern becomes, so no notes are lost. Trimming it is left to the
// panel's Bars control. Nothing is applied until the popup commits.
class PatternTimeSigPopup : public InputEditorPopup {
public:
	PatternTimeSigPopup();

	// lengthBeats is the pattern's current length, in its current denominator's
	// units. onOk gets the new signature and the bar count shown.
	void open(int wx, int wy, int top, int bottom, timeSettings::BeatUnit beat,
	          float lengthBeats,
	          std::function<void(int, int, timeSettings::BeatUnit, int)> onOk);

	int handle(int event) override;

private:
	void doOk() override;
	void sigChanged();
	int  numerator() const;
	// The fewest bars of the signature in the fields that still hold the pattern.
	int  barsToFit() const;

	Fl_Value_Input*  numInput    = nullptr;
	DenomBeatChoice* denomChoice = nullptr;
	Fl_Box*          barsValue   = nullptr;
	ModernButton*    okBtn       = nullptr;
	ModernButton*    cancelBtn   = nullptr;

	// What the pattern has now, so an unchanged OK writes nothing (and makes no
	// undo entry).
	int   origTop = 4, origBottom = 4, origBars = 1;
	timeSettings::BeatUnit origBeat = timeSettings::beatUnitDefault;
	float lengthBeats = 0.0f;

	std::function<void(int, int, timeSettings::BeatUnit, int)> onOkCb;
};

#endif
