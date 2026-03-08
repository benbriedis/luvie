#ifndef MARKER_RULER_HPP
#define MARKER_RULER_HPP

#include <FL/Fl_Widget.H>
#include <vector>
#include "markerPopup.hpp"

class MarkerRuler : public Fl_Widget {
public:
	enum Kind { TEMPO, TIME_SIG };

	struct Marker {
		int    id;
		int    bar;
		bool   fixed;
		double bpm;
		int    num;
		int    den;
	};

	MarkerRuler(int x, int y, int w, int h, int numCols, int colWidth,
	            Kind kind, double initBpm, int initNum, int initDen,
	            MarkerPopup* popup);

private:
	Kind         kind;
	int          numCols;
	int          colWidth;
	MarkerPopup* popup;

	std::vector<Marker> markers;
	int nextId     = 1;
	int draggingId = -1;
	int clickedId  = -1;
	bool didDrag   = false;

	static constexpr Fl_Color tempoColor   = 0xF59E0B00;  // amber-400
	static constexpr Fl_Color timeSigColor = 0x8B5CF600;  // violet-500
	static constexpr Fl_Color tempoBg      = 0xFEF3C700;  // amber-50
	static constexpr Fl_Color timeSigBg    = 0xEDE9FE00;  // violet-50

	void draw()          override;
	int  handle(int event) override;

	int     barToPixel(int bar) const { return x() + bar * colWidth; }
	int     pixelToBar(int px)  const;
	Marker* findMarkerAt(int px);
	Marker* findById(int id);
	void    openPopupForMarker(Marker& m);
};

#endif
