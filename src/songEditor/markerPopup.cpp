#include "markerPopup.hpp"
#include "timeSettings.hpp"
#include <FL/Fl.H>
#include <FL/Fl_Box.H>

static constexpr int popupW  = 160;
static constexpr int pad     = 8;
static constexpr int row1Y   = pad;
static constexpr int row1H   = 22;
static constexpr int row2Y   = pad + row1H + pad;
static constexpr int row2H   = 24;
// The time-signature popup carries a second input row (the beat definition), so
// its Delete button — and with it the popup's height — sits one row lower.
static constexpr int row3Y   = row2Y + row2H + pad;
static constexpr int beatChoiceW = 46;

MarkerPopup::MarkerPopup(Kind k)
	: InputEditorPopup(popupW, row2Y + row2H + pad), kind(k)
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

	if (kind == TEMPO) {
		auto* lbl = new Fl_Box(pad, row1Y, 30, row1H, "BPM");
		lbl->labelcolor(popupText);
		lbl->box(FL_NO_BOX);
		input1 = new Fl_Value_Input(pad + 34, row1Y, popupW - pad - 34 - pad, row1H);
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
		auto* slash = new Fl_Box(pad + 29 + 44 + 2, row1Y, 10, row1H, "/");
		slash->labelcolor(popupText);
		slash->box(FL_NO_BOX);
		denomChoice = new ModernChoice(pad + 29 + 44 + 16, row1Y, 44, row1H);
		styleChoice(denomChoice);
		for (const char* v : timeSettings::denominatorLabels)
			denomChoice->add(v);

		auto* beatLbl = new Fl_Box(pad, row2Y, 44, row2H, "Beat =");
		beatLbl->labelcolor(popupText);
		beatLbl->box(FL_NO_BOX);
		beatLbl->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
		beatChoice = new BeatUnitChoice(pad + 48, row2Y, beatChoiceW, row2H);
		styleChoice(beatChoice);

		deleteY = row3Y;
	}

	popupH     = deleteY + row2H + pad;
	popupHSlim = deleteY;

	deleteBtn = new ModernButton(pad, deleteY, popupW - 2 * pad, row2H, "Delete");
	deleteBtn->color(FL_WHITE);
	deleteBtn->labelcolor(popupText);

	end();

	deleteBtn->callback([](Fl_Widget*, void* d) {
		static_cast<MarkerPopup*>(d)->doDelete();
	}, this);
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
		int den = timeSettings::denominatorAt(denomChoice->value());
		if (onOkTimeSig) onOkTimeSig((int)input1->value(), den, beatChoice->beatUnit());
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
	denomChoice->value(timeSettings::denominatorIndex(den));
	beatChoice->setBeatUnit(beat);
	configureDelete(fixed, showDelete);
	onOkTimeSig = std::move(onOk);
	onDeleteCb  = std::move(onDelete);
	openEditor(wx, wy, input1);
}
