#include "transport.hpp"
#include "editor.hpp"
#include <FL/Fl.H>
#include <FL/fl_draw.H>
#include <algorithm>

static constexpr Fl_Color borderColor = 0xCBD5E100;  // slate blue-grey
static constexpr Fl_Color pressedColor = 0x3B82F600; // blue accent
static constexpr Fl_Color iconColor = 0x37415100;    // dark slate

static constexpr Fl_Color connectedColor = 0x86EFAC00; // lightish green
static constexpr Fl_Color waitingColor   = 0xDC262600; // red
static constexpr Fl_Color connectedText  = 0x14532D00; // dark green

static constexpr int indicatorRadius = 5;

// Filled rounded rectangle with an explicit corner radius (FLTK's rounded box
// types scale the radius with the widget size, which is too large here).
static void roundedRectf(int x, int y, int w, int h, int r) {
	const int d = r * 2;
	fl_pie(x,         y,         d, d, 90,  180);
	fl_pie(x + w - d, y,         d, d, 0,   90);
	fl_pie(x,         y + h - d, d, d, 180, 270);
	fl_pie(x + w - d, y + h - d, d, d, 270, 360);
	fl_rectf(x + r, y,     w - d, h);
	fl_rectf(x,     y + r, w,     h - d);
}

// Matching rounded-rectangle outline.
static void roundedRect(int x, int y, int w, int h, int r) {
	const int d = r * 2;
	fl_arc(x,         y,         d, d, 90,  180);
	fl_arc(x + w - d, y,         d, d, 0,   90);
	fl_arc(x,         y + h - d, d, d, 180, 270);
	fl_arc(x + w - d, y + h - d, d, d, 270, 360);
	fl_xyline(x + r, y,         x + w - r - 1);
	fl_xyline(x + r, y + h - 1, x + w - r - 1);
	fl_yxline(x,         y + r, y + h - r - 1);
	fl_yxline(x + w - 1, y + r, y + h - r - 1);
}

// ---------------------------------------------------------------------------

TransportIndicator::TransportIndicator(int x, int y, int w, int h)
	: Fl_Widget(x, y, w, h)
{
}

void TransportIndicator::setLabel(const std::string& text) {
	if (label == text) return;
	label = text;
	redraw();
}

void TransportIndicator::setWaiting(bool w) {
	if (waiting == w) return;
	waiting = w;
	redraw();
}

void TransportIndicator::draw() {
	Fl_Color bg = waiting ? waitingColor : connectedColor;
	fl_color(bg);
	roundedRectf(x(), y(), w(), h(), indicatorRadius);

	fl_color(fl_color_average(bg, FL_BLACK, 0.7f));
	roundedRect(x(), y(), w(), h(), indicatorRadius);

	fl_color(waiting ? FL_WHITE : connectedText);
	fl_font(FL_HELVETICA, 11);
	fl_draw(label.c_str(), x(), y(), w(), h(), FL_ALIGN_CENTER);
}

int TransportIndicator::handle(int event) {
	switch (event) {
	case FL_PUSH:
		// Claim the click so we receive the following double-click report.
		if (Fl::event_button() == FL_LEFT_MOUSE) {
			if (Fl::event_clicks() > 0 && onDoubleClick) {
				Fl::event_clicks(0);
				onDoubleClick();
			}
			return 1;
		}
		break;
	}
	return Fl_Widget::handle(event);
}

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
	bool pressed  = value();
	bool inactive = !active_r();
	int  r  = (std::min(w(), h()) - 4) / 2;
	int  cx = x() + w() / 2;
	int  cy = y() + h() / 2;

	fl_color(bgColor);
	fl_rectf(x(), y(), w(), h());

	Fl_Color border = inactive
	    ? fl_color_average(borderColor, bgColor, 0.35f)
	    : borderColor;
	Fl_Color icon_col = inactive
	    ? fl_color_average(iconColor, bgColor, 0.35f)
	    : (pressed ? FL_WHITE : iconColor);

	if (pressed && !inactive) {
		fl_color(pressedColor);
		fl_pie(cx - r, cy - r, r * 2, r * 2, 0.0, 360.0);
	} else {
		fl_color(bgColor);
		fl_pie(cx - r, cy - r, r * 2, r * 2, 0.0, 360.0);
		fl_color(border);
		fl_line_style(FL_SOLID, 2);
		fl_arc(cx - r, cy - r, r * 2, r * 2, 0.0, 360.0);
		fl_line_style(0);
	}

	fl_color(icon_col);
	drawIcon(cx, cy, r / 2, useAlt ? altIcon : icon);
}

// ---------------------------------------------------------------------------

void Transport::notifyEndReached() {
	stoppedAtEnd = true;
	playPauseBtn->setAlt(false);
	playPauseBtn->redraw();
}

// ---------------------------------------------------------------------------

void Transport::resize(int x, int y, int w, int h)
{
	Fl_Widget::resize(x, y, w, h);
	const int bw     = rewindBtn->w();
	const int pw     = playPauseBtn->w();
	const int gap    = 12;
	const int totalW = bw + gap + pw;
	int bx = x + (w - totalW) / 2;
	bx = std::max(x + 10, bx);
	int by = y + (h - rewindBtn->h()) / 2;
	rewindBtn->position(bx, by);
	playPauseBtn->position(bx + bw + gap, by);

	int iy = y + (h - indicator->h()) / 2;
	indicator->position(x + 10, iy);
}

Transport::~Transport()
{
}

void Transport::onTimelineChanged()
{
}

void Transport::syncPlayState()
{
	bool playing = transport && transport->isPlaying();
	if (playing != lastPlayingState) {
		lastPlayingState = playing;
		playPauseBtn->setAlt(playing);
		playPauseBtn->redraw();
		if (playing)
			stoppedAtEnd = false;
	}
}

void Transport::disableButtons()
{
	rewindBtn->deactivate();
	playPauseBtn->deactivate();
	rewindBtn->redraw();
	playPauseBtn->redraw();
}

void Transport::enableButtons()
{
	rewindBtn->activate();
	playPauseBtn->activate();
	rewindBtn->redraw();
	playPauseBtn->redraw();
}

void Transport::setControlTransport(ITransport* ct)
{
	controlTransport = ct;
	rewindBtn->activate();
	playPauseBtn->activate();
	rewindBtn->redraw();
	playPauseBtn->redraw();
}

void Transport::setTransportLabel(const std::string& label)
{
	indicator->setLabel(label);
}

void Transport::setTransportWaiting(bool waiting)
{
	indicator->setWaiting(waiting);
}

void Transport::setIndicatorDoubleClick(std::function<void()> cb)
{
	indicator->onDoubleClick = std::move(cb);
}

Transport::Transport(int x, int y, int w, int h, ITransport* t)
	: Fl_Group(x, y, w, h), transport(t)
{
	box(FL_FLAT_BOX);
	color(bgColor);

	const int btnSize = h - 10;
	const int gap     = 12;
	const int totalW  = 2 * btnSize + gap;
	const int bx      = x + (w - totalW) / 2;
	const int by      = y + (h - btnSize) / 2;

	const int indW = 58;
	const int indH = 22;
	indicator = new TransportIndicator(x + 10, y + (h - indH) / 2, indW, indH);

	rewindBtn = new TransportButton(bx, by, btnSize, btnSize,
	                                TransportButton::REWIND);
	rewindBtn->callback([](Fl_Widget*, void* data) {
		Transport* t = (Transport*)data;
		ITransport* ct = t->controlTransport ? t->controlTransport : t->transport;
		if (!ct) return;
		ct->rewind();
		t->stoppedAtEnd     = false;
		t->lastPlayingState = false;
		t->playPauseBtn->setAlt(false);
		t->playPauseBtn->redraw();
	}, this);

	playPauseBtn = new TransportButton(bx + btnSize + gap, by, btnSize, btnSize,
	                                   TransportButton::PLAY, TransportButton::PAUSE);
	playPauseBtn->callback([](Fl_Widget* w, void* data) {
		Transport* t   = (Transport*)data;
		auto*      btn = (TransportButton*)w;
		ITransport* ct = t->controlTransport ? t->controlTransport : t->transport;
		if (!ct) return;
		if (t->transport->isPlaying()) {
			ct->pause();
			t->lastPlayingState = false;
			btn->setAlt(false);
		} else {
			if (t->stoppedAtEnd) {
				ct->rewind();
				t->stoppedAtEnd = false;
			}
			ct->play();
			t->lastPlayingState = true;
			btn->setAlt(true);
		}
		btn->redraw();
	}, this);

	end();
}
