#ifndef MARKER_RULER_HPP
#define MARKER_RULER_HPP

#include <FL/Fl_Widget.H>
#include "observableTimeline.hpp"
#include "markerPopup.hpp"

class MarkerRuler : public Fl_Widget, public ITimelineObserver {
public:
	enum Kind { TEMPO, TIME_SIG };

	MarkerRuler(int x, int y, int w, int h, int numCols, int colWidth,
	            Kind kind, ObservableTimeline* timeline, MarkerPopup* popup);
	~MarkerRuler();

	void onTimelineChanged() override { redraw(); }

private:
	Kind                kind;
	int                 numCols;
	int                 colWidth;
	ObservableTimeline* timeline;
	MarkerPopup*        popup;

	int  draggingBar = -1;
	int  clickedBar  = -1;
	bool didDrag     = false;

	static constexpr Fl_Color tempoColor   = 0xF59E0B00;  // amber-400
	static constexpr Fl_Color timeSigColor = 0x8B5CF600;  // violet-500
	static constexpr Fl_Color tempoBg      = 0xFEF3C700;  // amber-50
	static constexpr Fl_Color timeSigBg    = 0xEDE9FE00;  // violet-50

	void draw()           override;
	int  handle(int event) override;

	int  offsetX = 0;
	int  barToPixel(int bar) const { return x() + offsetX + bar * colWidth; }
	int  pixelToBar(int px)  const;
	int  findBarAt(int px)   const;  // bar of marker under px, or -1
	bool isFixed(int bar)    const   { return bar == 0; }
	void openPopupFor(int bar);

public:
	void setOffsetX(int ox) { offsetX = ox; redraw(); }
};

#endif
