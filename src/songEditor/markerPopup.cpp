#include "markerPopup.hpp"
#include "timeSettings.hpp"
#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <algorithm>

static constexpr int popupWMax = 194;   // TEMPO width, and the time-sig starting point
static constexpr int pad     = 8;
static constexpr int row1Y   = pad;
static constexpr int row1H   = 22;
static constexpr int row2Y   = pad + row1H + pad;
static constexpr int row2H   = 24;

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
		auto* lbl = new Fl_Box(pad, row1Y, 30, row1H, "BPM");
		lbl->labelcolor(popupText);
		lbl->box(FL_NO_BOX);
		input1 = new Fl_Value_Input(pad + 34, row1Y, popupWMax - pad - 34 - pad, row1H);
		input1->range(timeSettings::bpmMin, timeSettings::bpmMax);
		input1->step(1);
		styleInput(input1);

		deleteY = row2Y;
	} else {
		auto* lbl = new Fl_Box(pad, row1Y, 25, row1H, "Sig");
		lbl->labelcolor(popupText);
		lbl->box(FL_NO_BOX);
		input1 = new Fl_Value_Input(pad + 29, row1Y, 44, row1H);
		input1->range(timeSettings::numeratorMin, timeSettings::numeratorMax);
		input1->step(1);
		styleInput(input1);
		constexpr int denomX = pad + 29 + 44 + 16;
		auto* slash = new Fl_Box(pad + 29 + 44 + 2, row1Y, 10, row1H, "/");
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

	popupH     = deleteY + row2H + pad;
	popupHSlim = deleteY;
	popupW     = winW;

	size(winW, popupH);

	deleteBtn = new ModernButton(pad, deleteY, winW - 2 * pad, row2H, "Delete");
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
		if (onOkTempo) onOkTempo(input1->value());
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

void MarkerPopup::configureDelete(bool fixed, bool showDelete)
{
	if (showDelete) {
		deleteBtn->show();
		fixed ? deleteBtn->deactivate() : deleteBtn->activate();
		popH = popupH;
	} else {
		// Freshly created marker: no Delete row until it is re-opened to edit.
		deleteBtn->hide();
		popH = popupHSlim;
	}
	// popH drives ContextMenuPopup::resize(), which otherwise pins the size.
	size(popupW, popH);
}

void MarkerPopup::openTempo(int wx, int wy, bool fixed, bool showDelete, double bpm,
                             std::function<void(double)> onOk,
                             std::function<void()> onDelete)
{
	input1->value(bpm);
	configureDelete(fixed, showDelete);
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
	configureDelete(fixed, showDelete);
	onOkTimeSig = std::move(onOk);
	onDeleteCb  = std::move(onDelete);
	openEditor(wx, wy, input1);
}
