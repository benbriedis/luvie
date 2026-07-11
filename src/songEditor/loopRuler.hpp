#ifndef LOOP_RULER_HPP
#define LOOP_RULER_HPP

#include <FL/Fl_Widget.H>

class LoopRulerContextPopup;

// A ruler strip carrying exactly two draggable markers: 'Start' (left-aligned to
// its column) and 'End' (right-aligned, so its right edge is flush with the RHS
// of the column it applies to). Sits between the BPM ruler and the Song Editor's
// position ruler. State is kept locally for now; more behaviour is wired later.
class LoopRuler : public Fl_Widget {
public:
	LoopRuler(int x, int y, int w, int h, int numCols, int colWidth);

	void resize(int x, int /*y*/, int w, int /*h*/) override { Fl_Widget::resize(x, y(), w, h()); }

	int startColumn() const { return startBar; }
	int endColumn()   const { return endBar; }

	// Place the markers programmatically (from the context menu). Start clamps to
	// [0, endBar]; End clamps to [startBar, numCols-1] — same rules as dragging.
	void setStartColumn(int bar);
	void setEndColumn(int bar);

	void setContextPopup(LoopRulerContextPopup* p) { contextPopup = p; }

private:
	int numCols;
	int colWidth;
	int offsetX  = 0;
	int clipLeft = 0;   // pixels from x() to clip the content region

	int startBar = 0;              // Start marker: left edge of this column
	int endBar;                    // End marker: right edge of this column

	enum Grab { NONE, START, END };
	Grab dragging = NONE;

	LoopRulerContextPopup* contextPopup = nullptr;

	static constexpr Fl_Color loopColor = 0x10B98100;  // emerald-500
	static constexpr Fl_Color loopBg    = 0xECFDF500;  // emerald-50

	void draw()            override;
	int  handle(int event) override;

	int  barLeftPixel(int bar)  const { return x() + offsetX + bar * colWidth; }
	int  barRightPixel(int bar) const { return barLeftPixel(bar) + colWidth; }
	int  pixelToBar(int px)     const;
	Grab markerAt(int px)       const;

public:
	void setOffsetX(int ox)  { offsetX  = ox; redraw(); }
	void setClipLeft(int cl) { clipLeft = cl; redraw(); }
	void setNumCols(int n);
};

#endif
