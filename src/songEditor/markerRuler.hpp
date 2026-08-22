// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#ifndef MARKER_RULER_HPP
#define MARKER_RULER_HPP

#include <FL/Fl_Widget.H>
#include "observableSong.hpp"
#include "markerPopup.hpp"

class MarkerRuler : public Fl_Widget, public ITimelineObserver {
public:
	enum Kind { TEMPO, TIME_SIG };

	MarkerRuler(int x, int y, int w, int h, int numCols, int colWidth,
	            Kind kind, ObservableSong* timeline,
	            MarkerPopup* tempoPopup, MarkerPopup* timeSigPopup);
	~MarkerRuler();

	void onTimelineChanged() override { redraw(); }
	void resize(int x, int /*y*/, int w, int /*h*/) override { Fl_Widget::resize(x, y(), w, h()); }

private:
	Kind                kind;
	int                 numCols;
	int                 colWidth;
	ObservableSong*     timeline;
	MarkerPopup*        tempoPopup;
	MarkerPopup*        timeSigPopup;

	// What is under the pointer. A tempo ramp is a band spanning whole bars: its
	// two edges resize it and its body moves it, mirroring LoopRuler's handles.
	enum Grab { NONE, MARKER, RAMP_START, RAMP_END, RAMP_BODY };
	struct Hit {
		int  bar  = -1;    // the marker's own bar, or -1 for empty ruler space
		Grab grab = NONE;
	};

	int  draggingBar    = -1;   // the marker being dragged; -1 = none
	Grab draggingGrab   = NONE;
	int  dragBarOffset  = 0;    // bars between the grab point and the marker's bar
	int  clickedBar     = -1;

	static constexpr Fl_Color tempoColor   = 0xF59E0B00;  // amber-400
	static constexpr Fl_Color timeSigColor = 0x8B5CF600;  // violet-500
	static constexpr Fl_Color tempoBg      = 0xFEF3C700;  // amber-50
	static constexpr Fl_Color timeSigBg    = 0xEDE9FE00;  // violet-50

	void draw()           override;
	int  handle(int event) override;

	int  offsetX  = 0;
	int  clipLeft = 0;   // pixels from x() to clip the content region
	int  barToPixel(int bar) const { return x() + offsetX + bar * colWidth; }
	int  pixelToBar(int px)      const;
	int  pixelToBoundary(int px) const;
	void dragTo(int px);             // px relative to x(), as pixelToBar wants it
	Hit  hitTest(int px)     const;  // px is absolute, as Fl::event_x() gives it
	bool isFixed(int bar)    const   { return bar == 0; }
	bool occupied(Kind k, int bar) const;
	// creating: the marker was just added by this right-click, so the popup offers
	// Cancel (which removes it again) in place of Delete.
	void openPopupFor(Kind k, int bar, bool creating);
	void addMarker(Kind k, int bar);

public:
	void setOffsetX(int ox)   { offsetX  = ox; redraw(); }
	void setClipLeft(int cl)  { clipLeft = cl; redraw(); }
	void setNumCols(int n)    { numCols  = n;  redraw(); }
	void setColWidth(int cw)  { colWidth = cw; redraw(); }
};

#endif
