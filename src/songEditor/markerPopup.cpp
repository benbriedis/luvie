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
static constexpr int popupH  = row2Y + row2H + pad;  // with the Delete row
static constexpr int popupHSlim = row2Y;             // input row only, no Delete

MarkerPopup::MarkerPopup(Kind k)
	: InputEditorPopup(popupW, popupH), kind(k)
{

	auto styleInput = [](Fl_Value_Input* inp) {
		inp->box(FL_FLAT_BOX);
		inp->color(popupInputBg);
		inp->textcolor(popupText);
		inp->cursor_color(popupText);
		inp->labelcolor(popupText);
	};

	if (kind == TEMPO) {
		auto* lbl = new Fl_Box(pad, row1Y, 30, row1H, "BPM");
		lbl->labelcolor(popupText);
		lbl->box(FL_NO_BOX);
		input1 = new Fl_Value_Input(pad + 34, row1Y, popupW - pad - 34 - pad, row1H);
		input1->range(timeSettings::bpmMin, timeSettings::bpmMax);
		input1->step(1);
		styleInput(input1);
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
		denomChoice->color(popupInputBg);
		denomChoice->labelcolor(popupText);
		denomChoice->setBorderColor(0x4B556300);
		denomChoice->setArrowColor(popupText);
		for (const char* v : timeSettings::denominatorLabels)
			denomChoice->add(v);
	}

	deleteBtn = new ModernButton(pad, row2Y, popupW - 2 * pad, row2H, "Delete");
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
		if (onOkTimeSig) onOkTimeSig((int)input1->value(), den);
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

void MarkerPopup::openTimeSig(int wx, int wy, bool fixed, bool showDelete, int num, int den,
                               std::function<void(int, int)> onOk,
                               std::function<void()> onDelete)
{
	input1->value(num);
	denomChoice->value(timeSettings::denominatorIndex(den));
	configureDelete(fixed, showDelete);
	onOkTimeSig = std::move(onOk);
	onDeleteCb  = std::move(onDelete);
	openEditor(wx, wy, input1);
}
