#include "markerRuler.hpp"
#include "popup.hpp"
#include <FL/Fl.H>
#include <FL/fl_draw.H>
#include <FL/Fl_Window.H>
#include <algorithm>
#include <cstdio>

MarkerRuler::MarkerRuler(int x, int y, int w, int h, int numCols, int colWidth,
                         Kind kind, double initBpm, int initNum, int initDen,
                         MarkerPopup* popup)
	: Fl_Widget(x, y, w, h),
	  kind(kind), numCols(numCols), colWidth(colWidth), popup(popup)
{
	markers.push_back({ nextId++, 0, true, initBpm, initNum, initDen });
}

void MarkerRuler::draw()
{
	fl_color(kind == TEMPO ? tempoBg : timeSigBg);
	fl_rectf(x(), y(), w(), h());

	fl_color(0xD1D5DB00);  // gray-300 border
	fl_line(x(), y() + h() - 1, x() + w() - 1, y() + h() - 1);

	static constexpr int flagW = 44;
	const int flagH = h() - 2;
	Fl_Color mc = (kind == TEMPO) ? tempoColor : timeSigColor;

	for (auto& m : markers) {
		int mx = barToPixel(m.bar);

		fl_color(mc);
		fl_line(mx, y(), mx, y() + h() - 2);
		fl_rectf(mx + 1, y() + 1, flagW, flagH);

		char label[16];
		if (kind == TEMPO)
			std::snprintf(label, sizeof(label), "%.0f", m.bpm);
		else
			std::snprintf(label, sizeof(label), "%d/%d", m.num, m.den);

		fl_color(FL_WHITE);
		fl_font(FL_HELVETICA, 10);
		fl_draw(label, mx + 1, y() + 1, flagW, flagH, FL_ALIGN_CENTER);
	}
}

int MarkerRuler::pixelToBar(int px) const
{
	return std::clamp(px / colWidth, 0, numCols - 1);
}

MarkerRuler::Marker* MarkerRuler::findMarkerAt(int px)
{
	static constexpr int grabZone = 5;
	static constexpr int flagW    = 44;
	for (auto& m : markers) {
		int mx = barToPixel(m.bar);
		if (std::abs(px - mx) <= grabZone || (px > mx && px < mx + flagW))
			return &m;
	}
	return nullptr;
}

MarkerRuler::Marker* MarkerRuler::findById(int id)
{
	for (auto& m : markers)
		if (m.id == id) return &m;
	return nullptr;
}

void MarkerRuler::openPopupForMarker(Marker& m)
{
	Fl_Window* win = window();
	Size       avail = { win ? win->w() : 800, win ? win->h() : 600 };
	Point2     anchor = { barToPixel(m.bar), y() };

	Point2 pos = calcPopupPos(avail, anchor, h(), popup->w(), popup->h());

	if (kind == TEMPO) {
		popup->openTempo(pos.x, pos.y, m.fixed, m.bpm,
			[this, id = m.id](double bpm) {
				if (auto* mk = findById(id)) { mk->bpm = bpm; redraw(); }
			},
			[this, id = m.id]() {
				markers.erase(std::find_if(markers.begin(), markers.end(),
					[id](const Marker& mk) { return mk.id == id; }));
				redraw();
			});
	} else {
		popup->openTimeSig(pos.x, pos.y, m.fixed, m.num, m.den,
			[this, id = m.id](int num, int den) {
				if (auto* mk = findById(id)) { mk->num = num; mk->den = den; redraw(); }
			},
			[this, id = m.id]() {
				markers.erase(std::find_if(markers.begin(), markers.end(),
					[id](const Marker& mk) { return mk.id == id; }));
				redraw();
			});
	}
}

int MarkerRuler::handle(int event)
{
	switch (event) {
	case FL_ENTER:
		return 1;
	case FL_LEAVE:
		window()->cursor(FL_CURSOR_DEFAULT);
		return 0;
	case FL_MOVE: {
		Marker* m = findMarkerAt(Fl::event_x());
		window()->cursor((m && !m->fixed) ? FL_CURSOR_WE : FL_CURSOR_CROSS);
		return 1;
	}
	case FL_PUSH: {
		Marker* m = findMarkerAt(Fl::event_x());
		if (m) {
			clickedId  = m->id;
			draggingId = m->fixed ? -1 : m->id;
		} else {
			clickedId  = -1;
			draggingId = -1;
		}
		didDrag = false;
		return 1;
	}
	case FL_DRAG: {
		if (draggingId >= 0) {
			int newBar = std::max(1, pixelToBar(Fl::event_x() - x()));
			Marker* m = findById(draggingId);
			if (m && m->bar != newBar) {
				bool occupied = std::any_of(markers.begin(), markers.end(),
					[&](const Marker& mk) { return mk.id != draggingId && mk.bar == newBar; });
				if (!occupied) {
					m->bar = newBar;
					std::sort(markers.begin(), markers.end(),
						[](const Marker& a, const Marker& b) { return a.bar < b.bar; });
					didDrag = true;
					redraw();
				}
			}
		}
		return 1;
	}
	case FL_RELEASE: {
		if (!didDrag) {
			if (clickedId >= 0) {
				Marker* m = findById(clickedId);
				if (m) openPopupForMarker(*m);
			} else {
				int bar = std::max(1, pixelToBar(Fl::event_x() - x()));
				bool occupied = std::any_of(markers.begin(), markers.end(),
					[bar](const Marker& mk) { return mk.bar == bar; });
				if (!occupied) {
					Marker* prev = nullptr;
					for (auto& mk : markers)
						if (mk.bar <= bar) prev = &mk;
					Marker newM;
					newM.id    = nextId++;
					newM.bar   = bar;
					newM.fixed = false;
					newM.bpm   = prev ? prev->bpm : 120.0;
					newM.num   = prev ? prev->num : 4;
					newM.den   = prev ? prev->den : 4;
					markers.push_back(newM);
					std::sort(markers.begin(), markers.end(),
						[](const Marker& a, const Marker& b) { return a.bar < b.bar; });
					redraw();
				}
			}
		}
		draggingId = -1;
		clickedId  = -1;
		didDrag    = false;
		return 1;
	}
	}
	return Fl_Widget::handle(event);
}
