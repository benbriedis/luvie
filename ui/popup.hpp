#ifndef POPUP_HPP
#define POPUP_HPP

#include "timeline.hpp"
#include "basePopup.hpp"
#include <FL/Fl_Flex.H>
#include <functional>

class Grid;

typedef struct { int width; int height; } Size;
typedef struct { int x;     int y;      } Point2;

Point2 calcPopupPos(Size available, Point2 anchor, int anchorHeight, int popupW, int popupH);

class Popup : public BasePopup {
public:
	Popup();

	void open(int selected, std::vector<Note>* notes, Grid* grid,
	          std::function<void()> onDelete = nullptr);

private:
	int selected;
	std::vector<Note>* notes;
	Grid* grid;
	std::function<void()> onDeleteFn;

	Point2 popupPosition(Size size, Point2 pos);
};

#endif
