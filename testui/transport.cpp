#include "transport.hpp"
#include "outerGrid.hpp"
#include <FL/Fl.H>
#include <FL/fl_draw.H>
#include <algorithm>
#include <cstdio>

static constexpr Fl_Color borderColor = 0xCBD5E100;  // slate blue-grey
static constexpr Fl_Color pressedColor = 0x3B82F600; // blue accent
static constexpr Fl_Color iconColor = 0x37415100;    // dark slate

// ---------------------------------------------------------------------------

TransportButton::TransportButton(int x, int y, int w, int h, Icon icon, Icon altIcon)
	: Fl_Button(x, y, w, h), icon(icon), altIcon(altIcon)
{
	box(FL_NO_BOX);
}

void TransportButton::drawIcon(int cx, int cy, int s, Icon icon) {
	switch (icon) {
	case PLAY:
		fl_polygon(cx - s*2/3, cy - s,
		           cx - s*2/3, cy + s,
		           cx + s,     cy);
		break;

	case PAUSE: {
		int bw   = std::max(2, s / 2);
		int span = s * 8 / 10;
		fl_rectf(cx - span,        cy - s, bw, s * 2);
		fl_rectf(cx + span - bw,   cy - s, bw, s * 2);
		break;
	}

	case STOP: {
		int ss = s * 4 / 5;
		fl_rectf(cx - ss, cy - ss, ss * 2, ss * 2);
		break;
	}

	case REWIND: {
		int barW  = std::max(2, s / 3);
		int triHW = s - barW - 2;
		int lx    = cx - (barW + 2 + triHW * 2) / 2;
		fl_rectf(lx, cy - s, barW, s * 2);
		fl_polygon(lx + barW + 2 + triHW * 2, cy - s,
		           lx + barW + 2 + triHW * 2, cy + s,
		           lx + barW + 2,             cy);
		break;
	}
	}
}

void TransportButton::draw() {
	bool pressed = value();
	int  r  = (std::min(w(), h()) - 4) / 2;
	int  cx = x() + w() / 2;
	int  cy = y() + h() / 2;

	fl_color(bgColor);
	fl_rectf(x(), y(), w(), h());

	if (pressed) {
		fl_color(pressedColor);
		fl_pie(cx - r, cy - r, r * 2, r * 2, 0.0, 360.0);
	} else {
		fl_color(bgColor);
		fl_pie(cx - r, cy - r, r * 2, r * 2, 0.0, 360.0);
		fl_color(borderColor);
		fl_line_style(FL_SOLID, 2);
		fl_arc(cx - r, cy - r, r * 2, r * 2, 0.0, 360.0);
		fl_line_style(0);
	}

	fl_color(pressed ? FL_WHITE : iconColor);
	drawIcon(cx, cy, r / 2, useAlt ? altIcon : icon);
}

// ---------------------------------------------------------------------------

void Transport::pollCb(void* data) {
	auto* self = static_cast<Transport*>(data);
	self->updatePosition();
	if (self->transport && self->transport->isPlaying())
		Fl::repeat_timeout(1.0, pollCb, data);
}

void Transport::updatePosition() {
	int secs = transport ? (int)transport->position() : 0;
	std::snprintf(posText, sizeof(posText), "Position: %d secs", secs);
	posLabel->label(posText);
	posLabel->redraw();
}

// ---------------------------------------------------------------------------

Transport::Transport(int x, int y, int w, int h, ITransport* t)
	: Fl_Group(x, y, w, h), transport(t)
{
	box(FL_FLAT_BOX);
	color(bgColor);

	const int btnSize = h - 10;
	const int gap     = 12;
	const int totalW  = 3 * btnSize + 2 * gap;
	const int bx      = x + (w - totalW) / 2;
	const int by      = y + (h - btnSize) / 2;

	rewindBtn = new TransportButton(bx, by, btnSize, btnSize,
	                                TransportButton::REWIND);
	rewindBtn->callback([](Fl_Widget*, void* data) {
		Transport* t = (Transport*)data;
		if (t->transport) {
			t->transport->rewind();
			t->updatePosition();
		}
	}, this);

	stopBtn = new TransportButton(bx + btnSize + gap, by, btnSize, btnSize,
	                              TransportButton::STOP);
	stopBtn->callback([](Fl_Widget*, void* data) {
		Transport* t = (Transport*)data;
		if (!t->transport) return;
		bool wasPlaying = t->transport->isPlaying();
		t->transport->stop();
		if (wasPlaying) {
			t->playPauseBtn->setAlt(false);
			t->playPauseBtn->redraw();
			Fl::remove_timeout(pollCb, t);
		}
		t->updatePosition();
	}, this);

	playPauseBtn = new TransportButton(bx + 2 * (btnSize + gap), by, btnSize, btnSize,
	                                   TransportButton::PLAY, TransportButton::PAUSE);
	playPauseBtn->callback([](Fl_Widget* w, void* data) {
		Transport* t   = (Transport*)data;
		auto*      btn = (TransportButton*)w;
		if (!t->transport) return;
		if (t->transport->isPlaying()) {
			t->transport->pause();
			btn->setAlt(false);
			Fl::remove_timeout(pollCb, t);
		} else {
			t->transport->play();
			btn->setAlt(true);
			Fl::add_timeout(1.0, pollCb, t);
		}
		t->updatePosition();
		btn->redraw();
	}, this);

	// Position label — sits to the right of the buttons
	const int labelX = bx + totalW + 20;
	const int labelW = x + w - labelX - 10;
	posLabel = new Fl_Box(labelX, by, labelW, btnSize);
	posLabel->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
	posLabel->labelcolor(iconColor);

	updatePosition();
	end();
}
