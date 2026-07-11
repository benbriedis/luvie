#include "transport.hpp"
#include "editor.hpp"
#include "popupStyle.hpp"
#include <FL/Fl.H>
#include <FL/fl_draw.H>
#include <algorithm>

static constexpr Fl_Color borderColor = 0xCBD5E100;  // slate blue-grey
static constexpr Fl_Color pressedColor = 0x3B82F600; // blue accent
static constexpr Fl_Color iconColor = 0x37415100;    // dark slate

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
	color(popupBg);
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
	fl_color(popupBg);
	fl_rectf(0, 0, w(), h());
	fl_color(bubbleBorder);
	fl_rect(0, 0, w(), h());

	fl_font(FL_HELVETICA, alertFontSize);
	fl_color(popupText);
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

void AlertIndicator::draw() {
	// Clear our slice of the bar first so a previous (larger) glyph doesn't
	// show through when the state changes.
	fl_color(bgColor);
	fl_rectf(x(), y(), w(), h());

	int cx = x() + w() / 2;
	int cy = y() + h() / 2;
	int s  = std::min(w(), h()) / 2 - 2;   // glyph half-extent

	if (!alerts.empty()) {
		// Red warning triangle with a white '!'.
		fl_color(alertColor);
		fl_polygon(cx, cy - s, cx - s, cy + s, cx + s, cy + s);
		fl_color(fl_color_average(alertColor, FL_BLACK, 0.7f));
		fl_loop(cx, cy - s, cx - s, cy + s, cx + s, cy + s);
		fl_color(FL_WHITE);
		fl_font(FL_HELVETICA_BOLD, s + 4);
		// Centre on the triangle's axis (+1 optical nudge for the glyph's bearing).
		fl_draw("!", cx - s + 1, cy - s + 2, s * 2, s * 2, FL_ALIGN_CENTER);
	} else {
		// Same warning triangle as the alert state, but with a white background.
		fl_color(popupBg);
		fl_polygon(cx, cy - s, cx - s, cy + s, cx + s, cy + s);
		fl_color(bubbleBorder);
		fl_loop(cx, cy - s, cx - s, cy + s, cx + s, cy + s);
		fl_color(iconColor);
		fl_font(FL_HELVETICA_BOLD, s + 4);
		// Centre on the triangle's axis (+1 optical nudge for the glyph's bearing).
		fl_draw("!", cx - s + 1, cy - s + 2, s * 2, s * 2, FL_ALIGN_CENTER);
	}
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

	case LOOP:
	case LOOP_OFF: {
		// A ring with a gap on the right, capped by an arrowhead, suggesting a
		// repeating cycle. LOOP_OFF adds a diagonal slash (looping disabled).
		int rr = s;
		int lineW = std::max(2, s * (3 / 8));
		fl_line_style(FL_SOLID | FL_CAP_ROUND, lineW);
		fl_arc(cx - rr, cy - rr, rr * 2, rr * 2, 55.0, 340.0);
		fl_line_style(0);
		// Arrowhead at the lower end of the gap, pointing up into it.
		int ah = std::max(3, s * 2 / 3);
		int ax = cx + rr;             // ~3 o'clock, at the ring
		int ay = cy;
		fl_polygon(ax, ay - ah,       // tip (up, into the gap)
		           ax - ah, ay + ah/2,
		           ax + ah, ay + ah/2);
		if (icon == LOOP_OFF) {
			fl_line_style(FL_SOLID | FL_CAP_ROUND, lineW);
			fl_line(cx - rr, cy + rr, cx + rr, cy - rr);
			fl_line_style(0);
		}
		break;
	}
	}
}

void TransportButton::draw() {
	bool pressed  = value();
	bool inactive = !active_r() || visualDisabled;
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

bool Transport::loopEnabled() const
{
	return loopOn;
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
	color(bgColor);

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
		ct->rewind();
		t->stoppedAtEnd     = false;
		t->lastPlayingState = false;
		t->playPauseBtn->setAlt(false);
		t->playPauseBtn->redraw();
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
