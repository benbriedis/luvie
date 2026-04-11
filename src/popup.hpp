#ifndef POPUP_HPP
#define POPUP_HPP

#include "timeline.hpp"
#include "basePopup.hpp"
#include <FL/Fl_Flex.H>
#include <functional>

class Grid;

struct Size   { int width; int height; };
struct Point2 { int x;     int y;      };

Point2 calcPopupPos(Size available, Point2 anchor, int anchorHeight, int popupW, int popupH);

class Popup : public BasePopup {
public:
	Popup();

	void open(int selected, std::vector<Note>* notes, Grid* grid,
	          std::function<void()> onDelete = nullptr);

	// Generic variant: caller provides the note's pixel position and delete callback.
	void openAt(int dotX, int dotY, Fl_Widget* w, int rowH,
	            std::function<void()> onDelete);

private:
	int selected;
	std::vector<Note>* notes;
	Grid* grid;
	std::function<void()> onDeleteFn;

	Point2 popupPosition(Size size, Point2 pos);
};

#endif
