// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#ifndef MARKER_POPUP_HPP
#define MARKER_POPUP_HPP

#include <FL/Fl_Box.H>
#include <FL/Fl_Value_Input.H>
#include "modernButton.hpp"
#include "modernChoice.hpp"
#include "modern/denomBeatChoice.hpp"
#include "modern/inputEditorPopup.hpp"
#include "timeSettings.hpp"
#include <functional>

class MarkerPopup : public InputEditorPopup {
public:
	enum Kind { TEMPO, TIME_SIG };

	explicit MarkerPopup(Kind kind);

	// A tempo marker is either an instantaneous change (curve Immediate, endBpm
	// unused) or a ramp from bpm to endBpm — see BpmMarker. The popup only edits
	// the tempos and the curve; a ramp's length is set by dragging it on the ruler.
	//
	// inheritedBpm is the tempo already in force where the marker sits: a ramp
	// starts from it, so the Start row shows it read-only instead of asking for a
	// value. Pass 0 when there is none (the first marker) and Start stays an input.
	//
	// creating means the marker was just added by this right-click and has never
	// been edited: the discard button then reads "Cancel", since removing it undoes
	// the creation rather than deleting something the user had put there.
	void openTempo(int wx, int wy, bool fixed, bool creating,
	               timeSettings::TempoCurve curve, double bpm, double endBpm,
	               double inheritedBpm,
	               std::function<void(timeSettings::TempoCurve, double, double)> onOk,
	               std::function<void()>                                        onRemove);

	void openTimeSig(int wx, int wy, bool fixed, bool creating,
	                 int num, int den, timeSettings::BeatUnit beat,
	                 std::function<void(int, int, timeSettings::BeatUnit)> onOk,
	                 std::function<void()>                                 onRemove);

	int handle(int event) override;

private:
	void doOk() override;
	void doDiscard();
	void snapBeat();
	int  numerator() const;
	// Show or hide the End-BPM row for the current curve, then re-lay out: the
	// discard button moves and the window height changes with it.
	void layoutTempo();
	void relayout(bool fixed, bool isCreating);
	double bpmOf(const Fl_Value_Input* inp) const;
	// The tempo the marker starts at: inherited for a ramp, typed otherwise.
	double startBpm() const;
	// Whether the Start row shows the inherited tempo rather than an input.
	bool   startIsInherited() const;
	timeSettings::TempoCurve curve() const;

	Kind             kind;
	Fl_Value_Input*  input1      = nullptr;   // BPM / start BPM, or the numerator
	Fl_Value_Input*  endBpmInput = nullptr;   // TEMPO + Linear only
	Fl_Box*          bpmLabel    = nullptr;
	Fl_Box*          endLabel    = nullptr;
	Fl_Box*          startValue  = nullptr;   // stands in for input1 on a ramp
	ModernChoice*    curveChoice = nullptr;   // TEMPO only
	DenomBeatChoice* denomChoice = nullptr;   // TIME_SIG only
	// Removes the marker, as "Delete" when editing one and as "Cancel" while the
	// marker is still the one this right-click just created.
	ModernButton*    discardBtn  = nullptr;

	double inheritedBpm  = 0.0;   // TEMPO only; 0 = nothing precedes this marker
	bool creating      = false;
	bool discardFixed  = false;
	int  discardY      = 0;
	int  popupW        = 0;   // hugs the content; set once in the constructor

	std::function<void(timeSettings::TempoCurve, double, double)> onOkTempo;
	std::function<void(int, int, timeSettings::BeatUnit)>         onOkTimeSig;
	std::function<void()>                                         onRemoveCb;
};

#endif
