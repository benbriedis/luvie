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
	void openTempo(int wx, int wy, bool fixed, bool showDelete,
	               timeSettings::TempoCurve curve, double bpm, double endBpm,
	               std::function<void(timeSettings::TempoCurve, double, double)> onOk,
	               std::function<void()>                                        onDelete);

	void openTimeSig(int wx, int wy, bool fixed, bool showDelete,
	                 int num, int den, timeSettings::BeatUnit beat,
	                 std::function<void(int, int, timeSettings::BeatUnit)> onOk,
	                 std::function<void()>                                 onDelete);

	int handle(int event) override;

private:
	void doOk() override;
	void doDelete();
	void snapBeat();
	int  numerator() const;
	// Show or hide the End-BPM row for the current curve, then re-lay out: the
	// Delete button moves and the window height changes with it.
	void layoutTempo();
	void relayout(bool fixed, bool showDelete);
	double bpmOf(const Fl_Value_Input* inp) const;
	timeSettings::TempoCurve curve() const;

	Kind             kind;
	Fl_Value_Input*  input1      = nullptr;   // BPM / start BPM, or the numerator
	Fl_Value_Input*  endBpmInput = nullptr;   // TEMPO + Linear only
	Fl_Box*          bpmLabel    = nullptr;
	Fl_Box*          endLabel    = nullptr;
	ModernChoice*    curveChoice = nullptr;   // TEMPO only
	DenomBeatChoice* denomChoice = nullptr;   // TIME_SIG only
	ModernButton*    deleteBtn   = nullptr;

	bool showingDelete = false;
	bool deleteFixed   = false;
	int  deleteY       = 0;
	int  popupW        = 0;   // hugs the content; set once in the constructor

	std::function<void(timeSettings::TempoCurve, double, double)> onOkTempo;
	std::function<void(int, int, timeSettings::BeatUnit)>         onOkTimeSig;
	std::function<void()>                                         onDeleteCb;
};

#endif
