// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#include "transport.hpp"
#include "editor.hpp"
#include "popupStyle.hpp"
#include "modern/cursors.hpp"
#include "modern/svgGlyph.hpp"
#include <FL/Fl.H>
#include <FL/fl_draw.H>
#include <algorithm>

static constexpr Fl_Color pressedColor = 0x3B82F600; // blue accent
// Outline of a button waiting for its MIDI trigger: the amber param labels and
// scene buttons use for "Learning…".
static constexpr Fl_Color learningColor = 0xF59E0B00;

static constexpr Fl_Color alertColor   = 0xDC262600; // red
static constexpr Fl_Color bubbleBorder = 0x94A3B800; // slate grey

// ---------------------------------------------------------------------------

static constexpr int alertPad      = 8;
static constexpr int alertLineH    = 18;
static constexpr int alertFontSize = 12;

AlertPopup::AlertPopup()
	: BasePopup(0, 0, 10, 10)
{
	box(FL_NO_BOX);   // draw() paints the background and border
	end();
}

void AlertPopup::setLines(const std::vector<std::string>& l) {
	lines = l;
	fl_font(FL_HELVETICA, alertFontSize);
	int maxW = 0;
	for (const auto& s : lines)
		maxW = std::max(maxW, (int)fl_width(s.c_str()));
	size(maxW + 2 * alertPad, (int)lines.size() * alertLineH + 2 * alertPad);
}

void AlertPopup::draw() {
	fl_color(popupBg());
	fl_rectf(0, 0, w(), h());
	fl_color(bubbleBorder);
	fl_rect(0, 0, w(), h());

	fl_font(FL_HELVETICA, alertFontSize);
	fl_color(popupText());
	int ty = alertPad;
	for (const auto& s : lines) {
		fl_draw(s.c_str(), alertPad, ty, w() - 2 * alertPad, alertLineH,
		        FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
		ty += alertLineH;
	}
}

// ---------------------------------------------------------------------------

AlertIndicator::AlertIndicator(int x, int y, int w, int h)
	: Fl_Widget(x, y, w, h)
{
	// Creating a window clears the current group; preserve it so widgets built
	// after this indicator still get added to the enclosing group. The popup is
	// added to the main window (and owned by it) by the caller.
	Fl_Group* prev = Fl_Group::current();
	popup = new AlertPopup();
	popup->hide();
	Fl_Group::current(prev);
}

void AlertIndicator::setAlerts(const std::vector<std::string>& a) {
	if (alerts == a) return;
	alerts = a;
	if (popup->shown()) showPopup();   // refresh the open popup's contents
	redraw();
}

void AlertIndicator::showPopup() {
	popup->setLines(alerts.empty()
	                    ? std::vector<std::string>{"No notifications"}
	                    : alerts);
	// position() is window-relative (the popup is a sub-window of the
	// AppWindow). Right-align to the indicator, just above it; the transport
	// bar sits at the window bottom.
	int px = x() + w() - popup->w();
	int py = y() - popup->h() - 4;
	popup->position(std::max(0, px), py);
	popup->show();
}

// The warning triangle: src/icons/alertBorder.svg, alertFill.svg and
// alertMark.svg, minus the XML declaration. Re-copy them after editing the
// files: these constants are what get compiled in.
static const char* const kAlertBorderSvg =
	R"SVG(<svg xmlns="http://www.w3.org/2000/svg" version="1.1" viewBox="0 0 20 20"><path fill="currentColor" stroke="currentColor" stroke-width="3" stroke-linejoin="round" d="M10 2 L1.5 17.5 L18.5 17.5 Z"/></svg>)SVG";
static const char* const kAlertFillSvg =
	R"SVG(<svg xmlns="http://www.w3.org/2000/svg" version="1.1" viewBox="0 0 20 20"><path fill="currentColor" stroke="currentColor" stroke-width="1" stroke-linejoin="round" d="M10 2 L1.5 17.5 L18.5 17.5 Z"/></svg>)SVG";
static const char* const kAlertMarkSvg =
	R"SVG(<svg xmlns="http://www.w3.org/2000/svg" version="1.1" viewBox="0 0 20 20"><rect fill="currentColor" x="9" y="6.5" width="2" height="6.5" rx="1"/><circle fill="currentColor" cx="10" cy="15.25" r="1.25"/></svg>)SVG";

void AlertIndicator::draw() {
	// Clear our slice of the bar first so a previous (larger) glyph doesn't
	// show through when the state changes.
	fl_color(bgColor());
	fl_rectf(x(), y(), w(), h());

	// The triangle is layered from three pieces of artwork, each a single
	// colour: a fat-stroked triangle for the border, a thin-stroked one for the
	// face inside it, and the '!'. 20px in the stock 26 x 22 indicator; kept
	// even so the pieces centre on a whole pixel.
	const int size = (std::min(w(), h()) - 2) & ~1;
	const int cx   = x() + w() / 2;
	const int cy   = y() + h() / 2;
	const bool alerting = !alerts.empty();

	// Red with a white '!' while there are alerts; otherwise the same triangle,
	// quiet, in the popup's background.
	svgGlyph::draw(kAlertBorderSvg, cx, cy,
	               alerting ? fl_color_average(alertColor, FL_BLACK, 0.7f) : bubbleBorder, size);
	svgGlyph::draw(kAlertFillSvg, cx, cy, alerting ? alertColor : popupBg(), size);
	svgGlyph::draw(kAlertMarkSvg, cx, cy, alerting ? FL_WHITE : theme().transportIcon, size);
}

int AlertIndicator::handle(int event) {
	switch (event) {
	case FL_ENTER:
		showPopup();
		return 1;
	case FL_LEAVE:
		if (popup->shown()) popup->hide();
		return 1;
	}
	return Fl_Widget::handle(event);
}

// ---------------------------------------------------------------------------

TransportButton::TransportButton(int x, int y, int w, int h, Icon icon, Icon altIcon)
	: Fl_Button(x, y, w, h), icon(icon), altIcon(altIcon)
{
	box(FL_NO_BOX);
}

// The glyphs are src/icons/play.svg, pause.svg, rewind.svg, loop.svg and
// loopOff.svg, minus the XML declaration. Re-copy them after editing the files:
// these constants are what get compiled in. All share a 24-unit viewBox, so they
// line up with one another at any size.
namespace {
inline const char* kPlaySvg =
	R"SVG(<svg xmlns="http://www.w3.org/2000/svg" version="1.1" viewBox="0 0 24 24"><path fill="currentColor" stroke="currentColor" stroke-width="1.5" stroke-linejoin="round" d="M8.5 4.5 L8.5 19.5 L19.5 12 Z"/></svg>)SVG";
inline const char* kPauseSvg =
	R"SVG(<svg xmlns="http://www.w3.org/2000/svg" version="1.1" viewBox="0 0 24 24"><rect fill="currentColor" x="6" y="4" width="4" height="16" rx="1"/><rect fill="currentColor" x="14" y="4" width="4" height="16" rx="1"/></svg>)SVG";
inline const char* kRewindSvg =
	R"SVG(<svg xmlns="http://www.w3.org/2000/svg" version="1.1" viewBox="0 0 24 24"><rect fill="currentColor" x="5" y="4.5" width="3" height="15" rx="1"/><path fill="currentColor" stroke="currentColor" stroke-width="1.5" stroke-linejoin="round" d="M19.25 5.25 L19.25 18.75 L10 12 Z"/></svg>)SVG";
inline const char* kLoopSvg =
	R"SVG(<svg xmlns="http://www.w3.org/2000/svg" version="1.1" viewBox="0 0 24 24"><path fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" d="M15.01 6.27 A7 7 0 1 0 18 12"/><path fill="currentColor" stroke="currentColor" stroke-width="1" stroke-linejoin="round" d="M18 7.5 L21.5 13.5 L14.5 13.5 Z"/></svg>)SVG";
inline const char* kLoopOffSvg =
	R"SVG(<svg xmlns="http://www.w3.org/2000/svg" version="1.1" viewBox="0 0 24 24"><path fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" d="M15.01 6.27 A7 7 0 1 0 18 12"/><path fill="currentColor" stroke="currentColor" stroke-width="1" stroke-linejoin="round" d="M18 7.5 L21.5 13.5 L14.5 13.5 Z"/><path fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" d="M4.5 19.5 L19.5 4.5"/></svg>)SVG";
}

void TransportButton::drawIcon(int cx, int cy, int height, Fl_Color col, Icon icon) {
	const char* svg = kPlaySvg;
	switch (icon) {
	case PLAY:     svg = kPlaySvg;    break;
	case PAUSE:    svg = kPauseSvg;   break;
	case REWIND:   svg = kRewindSvg;  break;
	case LOOP:     svg = kLoopSvg;    break;
	case LOOP_OFF: svg = kLoopOffSvg; break;
	}
	svgGlyph::draw(svg, cx, cy, col, height);
}

void TransportButton::draw() {
	bool pressed  = value();
	bool inactive = !active_r() || visualDisabled;
	int  r  = (std::min(w(), h()) - 4) / 2;
	int  cx = x() + w() / 2;
	int  cy = y() + h() / 2;

	fl_color(bgColor());
	fl_rectf(x(), y(), w(), h());

	const Fl_Color borderColor = theme().transportBorder;
	const Fl_Color iconColor   = theme().transportIcon;
	Fl_Color border = inactive
	    ? fl_color_average(borderColor, bgColor(), 0.35f)
	    : borderColor;
	Fl_Color icon_col = inactive
	    ? fl_color_average(iconColor, bgColor(), 0.35f)
	    : (pressed ? FL_WHITE : iconColor);

	// The ring's 2px stroke straddles radius r; the learning ring sits just
	// outside it.
	if (pressed && !inactive)
		svgGlyph::disc(cx - r, cy - r, r * 2, pressedColor);
	else
		svgGlyph::ring(cx - r - 1, cy - r - 1, r * 2 + 2, 2, border);

	if (learning)
		svgGlyph::ring(cx - r - 2, cy - r - 2, r * 2 + 4, 2, learningColor);

	// The artwork's 24-unit box at two thirds of the button's diameter: 24px
	// in the stock 40px button, one unit to the pixel. Kept even, so the glyph
	// centres on a whole pixel as the ring does.
	drawIcon(cx, cy, (r * 4 / 3) & ~1, icon_col, useAlt ? altIcon : icon);
}

int TransportButton::handle(int event) {
	if (onContextMenu) {
		switch (event) {
		case FL_ENTER:
			window()->cursor(contextMenuCursorImage(), kContextCursorHotX, kContextCursorHotY);
			break;
		case FL_LEAVE:
			window()->cursor(FL_CURSOR_DEFAULT);
			break;
		case FL_PUSH:
			if (Fl::event_button() == FL_RIGHT_MOUSE) {
				// The menu takes the pointer; hand back the normal cursor first.
				window()->cursor(FL_CURSOR_DEFAULT);
				onContextMenu(Fl::event_x(), Fl::event_y());
				return 1;
			}
			break;
		}
	}
	return Fl_Button::handle(event);
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
	const int lw     = loopBtn->w();
	const int bw     = rewindBtn->w();
	const int pw     = playPauseBtn->w();
	const int gap    = 12;
	const int totalW = lw + gap + bw + gap + pw;
	int bx = x + (w - totalW) / 2;
	bx = std::max(x + 10, bx);
	int by = y + (h - rewindBtn->h()) / 2;
	loopBtn->position(bx, by);
	rewindBtn->position(bx + lw + gap, by);
	playPauseBtn->position(bx + lw + gap + bw + gap, by);

	int ay = y + (h - alertIndicator->h()) / 2;
	alertIndicator->position(x + w - 10 - alertIndicator->w(), ay);
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
		setPlayingState(playing);
		playPauseBtn->setAlt(playing);
		playPauseBtn->redraw();
		if (playing)
			stoppedAtEnd = false;
	}
}

void Transport::disableButtons()
{
	for (TransportButton* b : {loopBtn, rewindBtn, playPauseBtn}) {
		b->deactivate();
		b->redraw();
	}
}

void Transport::enableButtons()
{
	for (TransportButton* b : {loopBtn, rewindBtn, playPauseBtn}) {
		b->activate();
		b->redraw();
	}
}

void Transport::pressPlayPause()
{
	if (playPauseBtn->active_r()) playPauseBtn->do_callback();
}

void Transport::pressRewind()
{
	if (rewindBtn->active_r()) rewindBtn->do_callback();
}

void Transport::setTriggerVisual(bool playPauseLearning, const std::string& playPauseTip,
                                 bool rewindLearning,    const std::string& rewindTip)
{
	playPauseBtn->setLearning(playPauseLearning);
	rewindBtn->setLearning(rewindLearning);
	playPauseBtn->copy_tooltip(playPauseTip.empty() ? nullptr : playPauseTip.c_str());
	rewindBtn->copy_tooltip(rewindTip.empty() ? nullptr : rewindTip.c_str());
}

void Transport::setControlTransport(ITransport* ct)
{
	controlTransport = ct;
	rewindBtn->activate();
	playPauseBtn->activate();
	rewindBtn->redraw();
	playPauseBtn->redraw();
}

bool Transport::loopEnabled() const
{
	return loopOn;
}

void Transport::setLoopEnabled(bool on)
{
	if (loopOn == on) return;
	loopOn = on;
	loopBtn->setAlt(on);
	loopBtn->redraw();
}

void Transport::setLoopVisualDisabled(bool d)
{
	loopBtn->setVisualDisabled(d);
}

void Transport::setAlerts(const std::vector<std::string>& alerts)
{
	alertIndicator->setAlerts(alerts);
}

AlertPopup* Transport::alertPopup() const
{
	return alertIndicator->popupWindow();
}

Transport::Transport(int x, int y, int w, int h, ITransport* t)
	: Fl_Group(x, y, w, h), transport(t)
{
	box(FL_FLAT_BOX);
	onTheme([this] { color(bgColor()); });

	const int btnSize = h - 10;
	const int gap     = 12;
	const int totalW  = 3 * btnSize + 2 * gap;
	const int bx      = x + (w - totalW) / 2;
	const int by      = y + (h - btnSize) / 2;

	const int indH   = 22;
	const int alertW = 26;
	alertIndicator = new AlertIndicator(x + w - 10 - alertW, y + (h - indH) / 2,
	                                    alertW, indH);

	// Default state is "don't loop": the slashed LOOP_OFF icon shows; toggling to
	// "loop" swaps in the plain LOOP icon (like play/pause swaps its glyph).
	loopBtn = new TransportButton(bx, by, btnSize, btnSize,
	                              TransportButton::LOOP_OFF, TransportButton::LOOP);
	loopBtn->callback([](Fl_Widget* w, void* data) {
		Transport* t   = (Transport*)data;
		auto*      btn = (TransportButton*)w;
		t->loopOn = !t->loopOn;
		btn->setAlt(t->loopOn);
		btn->redraw();
		if (t->onLoopToggled) t->onLoopToggled(t->loopOn);
	}, this);

	rewindBtn = new TransportButton(bx + btnSize + gap, by, btnSize, btnSize,
	                                TransportButton::REWIND);
	rewindBtn->callback([](Fl_Widget*, void* data) {
		Transport* t = (Transport*)data;
		ITransport* ct = t->controlTransport ? t->controlTransport : t->transport;
		if (!ct) return;
		float bar = 0.0f;
		if (t->rewindTarget && t->rewindTarget(bar))
			ct->seek(bar);
		else
			ct->rewind();
		t->stoppedAtEnd     = false;
		t->setPlayingState(false);
		t->playPauseBtn->setAlt(false);
		t->playPauseBtn->redraw();
		if (t->onRewind) t->onRewind();
	}, this);

	playPauseBtn = new TransportButton(bx + 2 * (btnSize + gap), by, btnSize, btnSize,
	                                   TransportButton::PLAY, TransportButton::PAUSE);
	playPauseBtn->callback([](Fl_Widget* w, void* data) {
		Transport* t   = (Transport*)data;
		auto*      btn = (TransportButton*)w;
		ITransport* ct = t->controlTransport ? t->controlTransport : t->transport;
		if (!ct) return;
		if (t->transport->isPlaying()) {
			ct->pause();
			t->setPlayingState(false);
			btn->setAlt(false);
		} else {
			if (t->stoppedAtEnd) {
				ct->rewind();
				t->stoppedAtEnd = false;
			}
			ct->play();
			t->setPlayingState(true);
			btn->setAlt(true);
		}
		btn->redraw();
	}, this);

	rewindBtn->onContextMenu = [this](int wx, int wy) {
		if (onContextMenu) onContextMenu(Button::Rewind, wx, wy);
	};
	playPauseBtn->onContextMenu = [this](int wx, int wy) {
		if (onContextMenu) onContextMenu(Button::PlayPause, wx, wy);
	};

	end();
}
