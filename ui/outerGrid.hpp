#ifndef OUTER_GRID_HPP
#define OUTER_GRID_HPP

#include "grid.hpp"
#include "playhead.hpp"
#include "popup.hpp"
#include <FL/Fl_Group.H>
#include <functional>
#include <vector>

const Fl_Color bgColor = FL_WHITE;

class OuterGrid : public Fl_Group {
public:
	static constexpr int       rulerH     = 20;
private:
	static constexpr Fl_Color  rulerBg    = 0xFEFCE800;  // pale yellow
	static constexpr Fl_Color  rulerBorder = 0xD1D5DB00; // gray-300

	Playhead playhead;  // declared before grid — initialised first
	MyGrid   grid;
	bool     rulerDragging = false;

	void draw() override;
	int  handle(int event) override;

public:
	std::function<void()> onEndReached;
	std::function<void()> onSeek;

	OuterGrid(int x, int y, std::vector<Note> notes, int numRows, int numCols,
	          int rowHeight, int colWidth, float snap, Popup& popup);

	void setTransport(ITransport* t, ObservableTimeline* tl);
};

#endif
