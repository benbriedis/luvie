// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#include "modernDialog.hpp"
#include "modernButton.hpp"
#include "../theme.hpp"
#include <FL/Fl.H>
#include <FL/Fl_Double_Window.H>
#include <FL/fl_draw.H>
#include <algorithm>

namespace modernDialog {

namespace {

constexpr int kWidth      = 400;
constexpr int kPad        = 22;
constexpr int kTitleSize  = 15;
constexpr int kTextSize   = 13;
constexpr int kTitleGap   = 8;    // title to message
constexpr int kButtonsGap = 22;   // message to buttons
constexpr int kButtonH    = 30;
constexpr int kButtonGap  = 8;
constexpr int kButtonPadX = 16;   // either side of a button's label
constexpr int kButtonMinW = 84;

constexpr Fl_Color kAccent      = 0x3B82F600;   // the app's blue accent
constexpr Fl_Color kAccentHover = 0x2563EB00;

class Dialog : public Fl_Double_Window {
	std::string title_, message_;
	int titleH_ = 0, messageH_ = 0;
	int primary_ = -1;

public:
	int result;

	Dialog(const std::string& title, const std::string& message,
	       const std::vector<Button>& buttons, int cancelIndex)
		: Fl_Double_Window(kWidth, 100, "Luvie"),
		  title_(title), message_(message), result(cancelIndex)
	{
		// fl_measure() wraps to the width it is handed, giving the height.
		const int textW = kWidth - 2 * kPad;
		fl_font(FL_HELVETICA_BOLD, kTitleSize);
		int tw = textW;
		fl_measure(title_.c_str(), tw, titleH_, 0);
		if (!message_.empty()) {
			fl_font(FL_HELVETICA, kTextSize);
			int mw = textW;
			fl_measure(message_.c_str(), mw, messageH_, 0);
		}
		const int btnY = kPad + titleH_ + (message_.empty() ? 0 : kTitleGap + messageH_) + kButtonsGap;
		size(kWidth, btnY + kButtonH + kPad - 4);

		begin();
		// Right-aligned buttons are laid out from the right edge inwards, so
		// walk them backwards; Separate ones go from the left edge.
		int right = kWidth - kPad, left = kPad;
		fl_font(FL_HELVETICA, kTextSize);
		for (int i = (int)buttons.size() - 1; i >= 0; --i) {
			const Button& b = buttons[i];
			const int bw = std::max(kButtonMinW, (int)fl_width(b.label.c_str()) + 2 * kButtonPadX);
			int bx;
			if (b.role == Button::Separate) { bx = left; left += bw + kButtonGap; }
			else                            { right -= bw; bx = right; right -= kButtonGap; }
			auto* btn = new ModernButton(bx, btnY, bw, kButtonH);
			btn->copy_label(b.label.c_str());
			btn->labelsize(kTextSize);
			btn->setBorderWidth(1);
			btn->visible_focus(0);   // Enter and Escape are handled by the window
			if (b.role == Button::Primary) {
				primary_ = i;
				btn->color(kAccent);
				btn->labelcolor(FL_WHITE);
				btn->labelfont(FL_HELVETICA_BOLD);
				btn->setBorderColor(kAccent);
				btn->setHoverColor(kAccentHover);
			} else {
				btn->color(theme().overlayButtonBg);
				btn->labelcolor(theme().overlayText);
				btn->setBorderColor(theme().overlayBorder);
				btn->setHoverColor(theme().popupHover);
			}
			btn->callback([](Fl_Widget* w, void* idx) {
				auto* dlg = static_cast<Dialog*>(w->window());
				dlg->result = (int)(intptr_t)idx;
				dlg->hide();
			}, (void*)(intptr_t)i);
		}
		end();

		// Escape and the window's close button both land here.
		callback([](Fl_Widget* w, void*) { w->hide(); });
		set_modal();

		// Centre over the main window, or the screen when there is none yet.
		if (Fl_Window* parent = Fl::first_window()) {
			position(parent->x() + (parent->w() - w()) / 2,
			         parent->y() + (parent->h() - h()) / 3);
		} else {
			int sx, sy, sw, sh;
			Fl::screen_work_area(sx, sy, sw, sh);
			position(sx + (sw - w()) / 2, sy + (sh - h()) / 3);
		}
	}

	void draw() override {
		fl_color(theme().overlayBg);
		fl_rectf(0, 0, w(), h());

		const int textW = w() - 2 * kPad;
		int y = kPad;
		fl_font(FL_HELVETICA_BOLD, kTitleSize);
		fl_color(theme().overlayText);
		fl_draw(title_.c_str(), kPad, y, textW, titleH_,
		        FL_ALIGN_LEFT | FL_ALIGN_TOP | FL_ALIGN_WRAP | FL_ALIGN_INSIDE);
		y += titleH_ + kTitleGap;
		if (!message_.empty()) {
			fl_font(FL_HELVETICA, kTextSize);
			fl_color(theme().overlaySubText);
			fl_draw(message_.c_str(), kPad, y, textW, messageH_,
			        FL_ALIGN_LEFT | FL_ALIGN_TOP | FL_ALIGN_WRAP | FL_ALIGN_INSIDE);
		}
		draw_children();
	}

	int handle(int event) override {
		if (event == FL_KEYBOARD || event == FL_SHORTCUT) {
			const int k = Fl::event_key();
			if ((k == FL_Enter || k == FL_KP_Enter) && primary_ >= 0) {
				result = primary_;
				hide();
				return 1;
			}
		}
		return Fl_Double_Window::handle(event);
	}
};

}

int choice(const std::string& title, const std::string& message,
           const std::vector<Button>& buttons, int cancelIndex)
{
	Dialog dlg(title, message, buttons, cancelIndex);
	dlg.show();
	while (dlg.shown()) Fl::wait();
	return dlg.result;
}

void alert(const std::string& title, const std::string& message)
{
	choice(title, message, {{"OK", Button::Primary}}, 0);
}

}
