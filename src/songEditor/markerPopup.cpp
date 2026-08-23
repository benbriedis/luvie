// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#include "markerPopup.hpp"
#include "timeSettings.hpp"
#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/fl_draw.H>
#include <algorithm>
#include <cstdio>

static constexpr int popupWMax = 194;   // base width; the time-sig row shrinks to hug its content
static constexpr int tempoW    = 182;   // wide enough for the curve dropdown and its label
static constexpr int pad     = 8;
static constexpr int row1Y   = pad;
static constexpr int row1H   = 22;
static constexpr int rowGap  = 6;       // between the tempo popup's stacked rows
static constexpr int row2Y   = pad + row1H + pad;
static constexpr int row2H   = 24;
static constexpr int labelW  = 50;      // "Curve" / "Start" / "End" column

// Row tops in the tempo popup. The End row is only present for a ramp; when it is
// hidden the discard button moves up to take its place (see layoutTempo).
static constexpr int curveY  = row1Y;
static constexpr int startY  = row1Y + row1H + rowGap;
static constexpr int endY    = startY + row1H + rowGap;

MarkerPopup::MarkerPopup(Kind k)
	: InputEditorPopup(popupWMax, row2Y + row2H + pad), kind(k)
{

	auto styleInput = [](Fl_Value_Input* inp) {
		inp->box(FL_FLAT_BOX);
		inp->color(popupInputBg);
		inp->textcolor(popupText);
		inp->cursor_color(popupText);
		inp->labelcolor(popupText);
	};

	auto styleChoice = [](ModernChoice* c) {
		c->color(popupInputBg);
		c->labelcolor(popupText);
		c->textcolor(popupText);
		c->setBorderColor(0x4B556300);
		c->setArrowColor(popupText);
	};

	// The window hugs its content; the time-sig row shrinks it once the denominator
	// dropdown reports the width it needs (see below).
	int winW = popupWMax;

	if (kind == TEMPO) {
		winW = tempoW;
		const int fieldX = pad + labelW;
		const int fieldW = tempoW - fieldX - pad;

		auto* curveLbl = new Fl_Box(pad, curveY, labelW, row1H, "Type");
		curveLbl->labelcolor(popupText);
		curveLbl->box(FL_NO_BOX);
		curveLbl->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
		curveChoice = new ModernChoice(fieldX, curveY, fieldW, row1H);
		curveChoice->add("Immediate");
		curveChoice->add("Linear");
		styleChoice(curveChoice);
		// Switching the curve shows or hides the End row, so the popup has to
		// re-lay out and resize while it is open.
		curveChoice->callback([](Fl_Widget*, void* d) {
			static_cast<MarkerPopup*>(d)->layoutTempo();
		}, this);

		bpmLabel = new Fl_Box(pad, startY, labelW, row1H, "BPM");
		bpmLabel->labelcolor(popupText);
		bpmLabel->box(FL_NO_BOX);
		bpmLabel->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
		input1 = new Fl_Value_Input(fieldX, startY, fieldW, row1H);
		input1->range(timeSettings::bpmMin, timeSettings::bpmMax);
		input1->step(1);
		styleInput(input1);

		// A ramp starts from the tempo already in force, so there is nothing to
		// type: this stands in for input1 and shows that tempo (see layoutTempo).
		startValue = new Fl_Box(fieldX, startY, fieldW, row1H);
		startValue->labelcolor(popupText);
		startValue->box(FL_NO_BOX);
		startValue->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
		startValue->hide();

		endLabel = new Fl_Box(pad, endY, labelW, row1H, "End");
		endLabel->labelcolor(popupText);
		endLabel->box(FL_NO_BOX);
		endLabel->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
		endBpmInput = new Fl_Value_Input(fieldX, endY, fieldW, row1H);
		endBpmInput->range(timeSettings::bpmMin, timeSettings::bpmMax);
		endBpmInput->step(1);
		styleInput(endBpmInput);

		discardY = endY + row1H + pad;
	} else {
		auto* lbl = new Fl_Box(pad, row1Y, 25, row1H, "Sig");
		lbl->labelcolor(popupText);
		lbl->box(FL_NO_BOX);
		constexpr int numW = 32;
		input1 = new Fl_Value_Input(pad + 29, row1Y, numW, row1H);
		input1->range(timeSettings::numeratorMin, timeSettings::numeratorMax);
		input1->step(1);
		styleInput(input1);
		constexpr int denomX = pad + 29 + numW + 16;
		auto* slash = new Fl_Box(pad + 29 + numW + 2, row1Y, 10, row1H, "/");
		slash->labelcolor(popupText);
		slash->box(FL_NO_BOX);
		denomChoice = new DenomBeatChoice(denomX, row1Y, 0, row1H);
		styleChoice(denomChoice);
		// Size the dropdown to its widest option and shrink the window to suit.
		denomChoice->size(denomChoice->naturalWidth(), row1H);
		winW = denomX + denomChoice->w() + pad;

		// Editing the numerator snaps the beat to the one the signature implies; the
		// user can then pick another entry before the popup commits.
		input1->callback([](Fl_Widget*, void* d) {
			static_cast<MarkerPopup*>(d)->snapBeat();
		}, this);

		discardY = row2Y;
	}

	popupW = winW;
	size(winW, discardY + row2H + pad);

	// The button hugs its label rather than stretching across the popup, and keeps
	// the one width for both labels so it does not resize as the popup is reused.
	fl_font(FL_HELVETICA, FL_NORMAL_SIZE);
	int btnW = (int)std::max(fl_width("Delete"), fl_width("Cancel")) + 24;
	discardBtn = new ModernButton(pad, discardY, btnW, row2H, "Delete");
	discardBtn->color(FL_WHITE);
	discardBtn->labelcolor(popupText);

	end();

	discardBtn->callback([](Fl_Widget*, void* d) {
		static_cast<MarkerPopup*>(d)->doDiscard();
	}, this);
}

// The numerator input honours its range() only while dragging; a typed value is
// accepted verbatim, so clamp it here at every read.
int MarkerPopup::numerator() const
{
	return std::clamp((int)input1->value(),
	                  timeSettings::numeratorMin, timeSettings::numeratorMax);
}

void MarkerPopup::snapBeat()
{
	int den = denomChoice->denominator();
	denomChoice->set(den, timeSettings::impliedBeatUnit(numerator(), den));
	denomChoice->redraw();
}

// Both labels do the same thing: drop the marker without committing the fields.
// On a marker this right-click just created that reads as cancelling it.
void MarkerPopup::doDiscard()
{
	if (onRemoveCb) onRemoveCb();
	commit();
}

void MarkerPopup::doOk()
{
	if (kind == TEMPO) {
		if (onOkTempo) onOkTempo(curve(), startBpm(), bpmOf(endBpmInput));
	} else {
		if (onOkTimeSig) onOkTimeSig(numerator(), denomChoice->denominator(),
		                             denomChoice->beatUnit());
	}
	commit();
}

int MarkerPopup::handle(int event)
{
	if (event == FL_KEYDOWN && Fl::event_key() == FL_Enter) {
		if (Fl::focus() == discardBtn) { doDiscard(); return 1; }
	}
	return InputEditorPopup::handle(event);
}

void MarkerPopup::relayout(bool fixed, bool isCreating)
{
	creating     = isCreating;
	discardFixed = fixed;

	// The last content row: the End row for a ramp, the BPM row otherwise, and
	// row 1 for the time-signature popup, which has only the one.
	int contentBottom = row1Y + row1H;
	if (kind == TEMPO)
		contentBottom = (curve() == timeSettings::TempoCurve::Linear ? endY : startY) + row1H;

	discardY = contentBottom + pad;

	// Children of an Fl_Window are placed in the window's own coordinates.
	discardBtn->position(pad, discardY);
	discardBtn->label(isCreating ? "Cancel" : "Delete");
	// Bar 0's marker cannot be deleted, but a marker just created can always be
	// cancelled — and bar 0 never is one, since markers are only added past it.
	(fixed && !isCreating) ? discardBtn->deactivate() : discardBtn->activate();

	int popupH = discardY + row2H + pad;
	// popW/popH drive ContextMenuPopup::resize(), which otherwise pins the size.
	popW = popupW;
	popH = popupH;
	size(popupW, popupH);
	redraw();
}

void MarkerPopup::layoutTempo()
{
	bool ramp = curve() == timeSettings::TempoCurve::Linear;
	bpmLabel->label(ramp ? "Start" : "BPM");

	if (startIsInherited()) {
		char buf[16];
		std::snprintf(buf, sizeof(buf), "%g", inheritedBpm);
		startValue->copy_label(buf);
		input1->hide();
		startValue->show();
		// The field the user was in has just gone; End is the only one left.
		// (contains(), not ==: the focus sits on Fl_Value_Input's inner Fl_Input.)
		if (input1->contains(Fl::focus())) endBpmInput->take_focus();
	} else {
		startValue->hide();
		input1->show();
	}

	if (ramp) { endLabel->show(); endBpmInput->show(); }
	else      { endLabel->hide(); endBpmInput->hide(); }
	relayout(discardFixed, creating);
}

// A ramp's start is the tempo it inherits — the user only supplies one on the
// first marker, which has no earlier tempo to ramp away from.
bool MarkerPopup::startIsInherited() const
{
	return curve() == timeSettings::TempoCurve::Linear && inheritedBpm > 0.0;
}

double MarkerPopup::startBpm() const
{
	return startIsInherited() ? inheritedBpm : bpmOf(input1);
}

timeSettings::TempoCurve MarkerPopup::curve() const
{
	return curveChoice && curveChoice->value() == 1
	     ? timeSettings::TempoCurve::Linear
	     : timeSettings::TempoCurve::Immediate;
}

// Fl_Value_Input honours its range() only while dragging; a typed value is
// accepted verbatim, so clamp it here at every read.
double MarkerPopup::bpmOf(const Fl_Value_Input* inp) const
{
	return std::clamp(inp->value(), timeSettings::bpmMin, timeSettings::bpmMax);
}

void MarkerPopup::openTempo(int wx, int wy, bool fixed, bool isCreating,
                             timeSettings::TempoCurve c, double bpm, double endBpm,
                             double inherited,
                             std::function<void(timeSettings::TempoCurve, double, double)> onOk,
                             std::function<void()> onRemove)
{
	inheritedBpm = inherited;
	curveChoice->value(c == timeSettings::TempoCurve::Linear ? 1 : 0);
	input1->value(bpm);
	endBpmInput->value(endBpm > 0.0 ? endBpm : bpm);
	creating     = isCreating;
	discardFixed = fixed;
	layoutTempo();
	onOkTempo  = std::move(onOk);
	onRemoveCb = std::move(onRemove);
	// Start is read-only on a ramp, so End is the field to land in.
	openEditor(wx, wy, startIsInherited() ? (Fl_Widget*)endBpmInput : (Fl_Widget*)input1);
}

void MarkerPopup::openTimeSig(int wx, int wy, bool fixed, bool isCreating,
                               int num, int den, timeSettings::BeatUnit beat,
                               std::function<void(int, int, timeSettings::BeatUnit)> onOk,
                               std::function<void()> onRemove)
{
	input1->value(num);
	denomChoice->set(den, beat);
	relayout(fixed, isCreating);
	onOkTimeSig = std::move(onOk);
	onRemoveCb  = std::move(onRemove);
	openEditor(wx, wy, input1);
}
