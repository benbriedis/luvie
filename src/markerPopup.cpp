#include "markerPopup.hpp"
#include "appWindow.hpp"
#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/fl_draw.H>
#include "popupStyle.hpp"

// Layout constants (shared by both kinds)
static constexpr int popupW  = 160;
static constexpr int popupH  = 70;
static constexpr int pad     = 8;
static constexpr int row1Y   = pad;        // y of input row
static constexpr int row1H   = 22;
static constexpr int row2Y   = pad + row1H + pad;  // y of button row  (8+22+8=38)
static constexpr int row2H   = 24;
// total h = row2Y + row2H + pad = 38+24+8 = 70 ✓

MarkerPopup::MarkerPopup(Kind k)
	: BasePopup(0, 0, popupW, popupH), kind(k)
{
	color(popupBg);
	box(FL_BORDER_BOX);

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
		input1->range(20, 300);
		input1->step(1);
		styleInput(input1);
	} else {
		auto* lbl = new Fl_Box(pad, row1Y, 25, row1H, "Sig");
		lbl->labelcolor(popupText);
		lbl->box(FL_NO_BOX);
		input1 = new Fl_Value_Input(pad + 29, row1Y, 44, row1H);
		input1->range(1, 32);
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
		for (const char* v : {"1", "2", "4", "8", "16", "32"})
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
	committed = true;
	if (onDeleteCb) onDeleteCb();
	hide();
	if (auto* win = window()) win->redraw();
}

void MarkerPopup::doOk()
{
	committed = true;
	if (kind == TEMPO) {
		if (onOkTempo) onOkTempo(input1->value());
	} else {
		static constexpr int denoms[] = {1, 2, 4, 8, 16, 32};
		int den = (denomChoice->value() >= 0) ? denoms[denomChoice->value()] : 4;
		if (onOkTimeSig) onOkTimeSig((int)input1->value(), den);
	}
	hide();
	if (auto* win = window()) win->redraw();
}

void MarkerPopup::hide()
{
	if (!committed && visible()) {
		doOk();
		return;
	}
	Fl_Window::hide();
}

int MarkerPopup::handle(int event)
{
	if (event == FL_KEYDOWN && Fl::event_key() == FL_Enter) {
		Fl_Widget* f = Fl::focus();
		if (f == input1) {
			doOk();
			return 1;
		}
		if (f == deleteBtn) { doDelete(); return 1; }
		doOk();
		return 1;
	}
	return BasePopup::handle(event);
}

void MarkerPopup::openTempo(int wx, int wy, bool fixed, double bpm,
                             std::function<void(double)> onOk,
                             std::function<void()> onDelete)
{
	committed  = false;
	input1->value(bpm);
	fixed ? deleteBtn->deactivate() : deleteBtn->activate();
	onOkTempo  = std::move(onOk);
	onDeleteCb = std::move(onDelete);
	position(wx, wy);
	if (auto* aw = dynamic_cast<AppWindow*>(window()))
		aw->openPopup(this);
	else
		show();
	redraw();
	input1->take_focus();
}

void MarkerPopup::openTimeSig(int wx, int wy, bool fixed, int num, int den,
                               std::function<void(int, int)> onOk,
                               std::function<void()> onDelete)
{
	committed = false;
	input1->value(num);
	static constexpr int denoms[] = {1, 2, 4, 8, 16, 32};
	int idx = 2; // default to 4
	for (int i = 0; i < 6; ++i) if (denoms[i] == den) { idx = i; break; }
	denomChoice->value(idx);
	fixed ? deleteBtn->deactivate() : deleteBtn->activate();
	onOkTimeSig = std::move(onOk);
	onDeleteCb  = std::move(onDelete);
	position(wx, wy);
	if (auto* aw = dynamic_cast<AppWindow*>(window()))
		aw->openPopup(this);
	else
		show();
	redraw();
	input1->take_focus();
}
