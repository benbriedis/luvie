// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#include "markerPopup.hpp"
#include "timeSettings.hpp"
#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/fl_draw.H>
#include <algorithm>

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
// hidden the Delete button moves up to take its place (see layoutTempo).
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

		auto* curveLbl = new Fl_Box(pad, curveY, labelW, row1H, "Curve");
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

		endLabel = new Fl_Box(pad, endY, labelW, row1H, "End");
		endLabel->labelcolor(popupText);
		endLabel->box(FL_NO_BOX);
		endLabel->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
		endBpmInput = new Fl_Value_Input(fieldX, endY, fieldW, row1H);
		endBpmInput->range(timeSettings::bpmMin, timeSettings::bpmMax);
		endBpmInput->step(1);
		styleInput(endBpmInput);

		deleteY = endY + row1H + pad;
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

		deleteY = row2Y;
	}

	popupW = winW;
	size(winW, deleteY + row2H + pad);

	// The button hugs its label rather than stretching across the popup.
	fl_font(FL_HELVETICA, FL_NORMAL_SIZE);
	int deleteW = (int)fl_width("Delete") + 24;
	deleteBtn = new ModernButton(pad, deleteY, deleteW, row2H, "Delete");
	deleteBtn->color(FL_WHITE);
	deleteBtn->labelcolor(popupText);

	end();

	deleteBtn->callback([](Fl_Widget*, void* d) {
		static_cast<MarkerPopup*>(d)->doDelete();
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

void MarkerPopup::doDelete()
{
	if (onDeleteCb) onDeleteCb();
	commit();
}

void MarkerPopup::doOk()
{
	if (kind == TEMPO) {
		if (onOkTempo) onOkTempo(curve(), bpmOf(input1), bpmOf(endBpmInput));
	} else {
		if (onOkTimeSig) onOkTimeSig(numerator(), denomChoice->denominator(),
		                             denomChoice->beatUnit());
	}
	commit();
}

int MarkerPopup::handle(int event)
{
	if (event == FL_KEYDOWN && Fl::event_key() == FL_Enter) {
		if (Fl::focus() == deleteBtn) { doDelete(); return 1; }
	}
	return InputEditorPopup::handle(event);
}

void MarkerPopup::relayout(bool fixed, bool showDelete)
{
	showingDelete = showDelete;
	deleteFixed   = fixed;

	// The last content row: the End row for a ramp, the BPM row otherwise, and
	// row 1 for the time-signature popup, which has only the one.
	int contentBottom = row1Y + row1H;
	if (kind == TEMPO)
		contentBottom = (curve() == timeSettings::TempoCurve::Linear ? endY : startY) + row1H;

	deleteY = contentBottom + pad;

	int popupH;
	if (showDelete) {
		// Children of an Fl_Window are placed in the window's own coordinates.
		deleteBtn->position(pad, deleteY);
		deleteBtn->show();
		fixed ? deleteBtn->deactivate() : deleteBtn->activate();
		popupH = deleteY + row2H + pad;
	} else {
		// Freshly created marker: no Delete row until it is re-opened to edit.
		deleteBtn->hide();
		popupH = deleteY;
	}
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
	if (ramp) { endLabel->show(); endBpmInput->show(); }
	else      { endLabel->hide(); endBpmInput->hide(); }
	relayout(deleteFixed, showingDelete);
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

void MarkerPopup::openTempo(int wx, int wy, bool fixed, bool showDelete,
                             timeSettings::TempoCurve c, double bpm, double endBpm,
                             std::function<void(timeSettings::TempoCurve, double, double)> onOk,
                             std::function<void()> onDelete)
{
	curveChoice->value(c == timeSettings::TempoCurve::Linear ? 1 : 0);
	input1->value(bpm);
	endBpmInput->value(endBpm > 0.0 ? endBpm : bpm);
	showingDelete = showDelete;
	deleteFixed   = fixed;
	layoutTempo();
	onOkTempo  = std::move(onOk);
	onDeleteCb = std::move(onDelete);
	openEditor(wx, wy, input1);
}

void MarkerPopup::openTimeSig(int wx, int wy, bool fixed, bool showDelete,
                               int num, int den, timeSettings::BeatUnit beat,
                               std::function<void(int, int, timeSettings::BeatUnit)> onOk,
                               std::function<void()> onDelete)
{
	input1->value(num);
	denomChoice->set(den, beat);
	relayout(fixed, showDelete);
	onOkTimeSig = std::move(onOk);
	onDeleteCb  = std::move(onDelete);
	openEditor(wx, wy, input1);
}
