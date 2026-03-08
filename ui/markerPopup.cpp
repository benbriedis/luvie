#include "markerPopup.hpp"
#include <FL/Fl.H>
#include <FL/Fl_Box.H>

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
	: Fl_Window(0, 0, popupW, popupH), kind(k)
{
	if (kind == TEMPO) {
		new Fl_Box(pad, row1Y, 30, row1H, "BPM");
		input1 = new Fl_Value_Input(pad + 34, row1Y, popupW - pad - 34 - pad, row1H);
		input1->range(20, 300);
		input1->step(1);
	} else {
		new Fl_Box(pad, row1Y, 25, row1H, "Sig");
		input1 = new Fl_Value_Input(pad + 29, row1Y, 44, row1H);
		input1->range(1, 32);
		input1->step(1);
		new Fl_Box(pad + 29 + 44 + 2, row1Y, 10, row1H, "/");
		input2 = new Fl_Value_Input(pad + 29 + 44 + 16, row1Y, 44, row1H);
		input2->range(1, 32);
		input2->step(1);
	}

	const int btnW = (popupW - 2 * pad - pad) / 2;  // two buttons + gap
	deleteBtn      = new Fl_Button(pad, row2Y, btnW, row2H, "Delete");
	auto* okBtn    = new Fl_Button(pad + btnW + pad, row2Y, btnW, row2H, "OK");

	end();

	deleteBtn->callback([](Fl_Widget*, void* d) {
		auto* self = static_cast<MarkerPopup*>(d);
		if (self->onDeleteCb) self->onDeleteCb();
		self->hide();
	}, this);

	okBtn->callback([](Fl_Widget*, void* d) {
		static_cast<MarkerPopup*>(d)->doOk();
	}, this);
}

void MarkerPopup::doOk()
{
	if (kind == TEMPO) {
		if (onOkTempo) onOkTempo(input1->value());
	} else {
		if (onOkTimeSig) onOkTimeSig((int)input1->value(), (int)input2->value());
	}
	hide();
}

int MarkerPopup::handle(int event)
{
	if (event == FL_KEYDOWN && Fl::event_key() == FL_Enter) {
		if (kind == TIME_SIG && !onSecondField) {
			input2->take_focus();
			onSecondField = true;
		}
		else
			doOk();
		return 1;
	}

	int ret = Fl_Window::handle(event);
	switch (event) {
	case FL_PUSH:
	case FL_DRAG:
	case FL_RELEASE:
	case FL_MOVE:
	case FL_MOUSEWHEEL:
		return 1;
	}
	return ret;
}

void MarkerPopup::openTempo(int wx, int wy, bool fixed, double bpm,
                             std::function<void(double)> onOk,
                             std::function<void()> onDelete)
{
	input1->value(bpm);
	fixed ? deleteBtn->deactivate() : deleteBtn->activate();
	onOkTempo  = std::move(onOk);
	onDeleteCb = std::move(onDelete);
	position(wx, wy);
	show();
	input1->take_focus();
}

void MarkerPopup::openTimeSig(int wx, int wy, bool fixed, int num, int den,
                               std::function<void(int, int)> onOk,
                               std::function<void()> onDelete)
{
	input1->value(num);
	input2->value(den);
	onSecondField = false;
	fixed ? deleteBtn->deactivate() : deleteBtn->activate();
	onOkTimeSig = std::move(onOk);
	onDeleteCb  = std::move(onDelete);
	position(wx, wy);
	show();
	input1->take_focus();
}
