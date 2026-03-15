#include "popup.hpp"
#include "appWindow.hpp"
#include "modernButton.hpp"
#include "popupStyle.hpp"
#include "FL/Fl.H"
#include "FL/Fl_Box.H"
#include "FL/Fl_Window.H"
#include "FL/Fl_Flex.H"
#include "FL/Fl_Slider.H"
#include "grid.hpp"

Popup::Popup() : Fl_Window(0,0,0,0)
{
	color(popupBg);
	box(FL_BORDER_BOX);

	Fl_Flex *flex = new Fl_Flex(1,1,150,100);
	flex->box(FL_FLAT_BOX);
	flex->color(popupBg);
	flex->begin();
	flex->gap(10);
	ModernButton *deleteItem = new ModernButton(0, 0, 40, 30, "Delete");
	deleteItem->color(FL_WHITE);
	deleteItem->labelcolor(popupText);
	flex->fixed(deleteItem, 40);

	Fl_Flex *sliderRow = new Fl_Flex(0, 0, 150, 30, Fl_Flex::HORIZONTAL);
	sliderRow->box(FL_FLAT_BOX);
	sliderRow->color(popupBg);
	sliderRow->begin();
	Fl_Box *velLabel = new Fl_Box(0, 0, 30, 30, "Vel");
	velLabel->labelcolor(popupText);
	velLabel->box(FL_NO_BOX);
	sliderRow->fixed(velLabel, 30);
	Fl_Slider *slider = new Fl_Slider(0, 0, 120, 30);
	slider->type(FL_HOR_NICE_SLIDER);
	slider->box(FL_FLAT_BOX);
	slider->color(popupInputBg);
	slider->selection_color(popupAccent);
	slider->bounds(0.0,1.0);
	slider->value(0.5);
	sliderRow->end();

	flex->fixed(deleteItem, 30);
	flex->margin(10,10,10,10);
	flex->end();

	resize(0,0,flex->w()+2,flex->h()+2);
	end();

	deleteItem->callback([](Fl_Widget*, void* me) {
		Popup* self = (Popup*)me;
		if (self->onDeleteFn)
			self->onDeleteFn();
		else
			self->notes->erase(self->notes->begin() + self->selected);
		self->hide();
		if (auto* win = self->window()) win->redraw();
	}, this);
}

void Popup::open(int mySelected, std::vector<Note>* myNotes, Grid* myGrid,
                 std::function<void()> onDelete)
{
	selected   = mySelected;
	notes      = myNotes;
	grid       = myGrid;
	onDeleteFn = std::move(onDelete);

	Note cell = (*notes)[mySelected];

	Point2 desiredPosition = {
		(int)(grid->x() + cell.beat * grid->colWidth),
		(int)(grid->y() + cell.pitch * grid->rowHeight)
	};

	Fl_Window* win = grid->window();
	Size available = {win->w(), win->h()};

	Point2 pos = popupPosition(available, desiredPosition);
	position(pos.x, pos.y);

	if (auto* aw = dynamic_cast<AppWindow*>(window()))
		aw->openPopup(this);
	else
		show();
}

Point2 calcPopupPos(Size available, Point2 anchor, int anchorHeight, int popupW, int popupH)
{
	const int vpad = 4, hpad = 10;
	int x = anchor.x;
	if (x + popupW > available.width - hpad) x = available.width - popupW - hpad;
	if (x < hpad) x = hpad;
	int y = anchor.y + anchorHeight + vpad;
	if (y + popupH > available.height - vpad)
		y = anchor.y - popupH - vpad;
	if (y < vpad) y = vpad;
	return Point2(x, y);
}

Point2 Popup::popupPosition(Size size, Point2 pos)
{
	const float verticalPadding = 4.0;
	const float sidePadding     = 10.0;
	float height = h();

	bool placeAbove = pos.y >= (height + verticalPadding);
	float y = placeAbove ? pos.y - height - verticalPadding
	                     : pos.y + grid->rowHeight + verticalPadding;

	float maxX = size.width - w() - sidePadding;
	float x = pos.x > maxX ? maxX : pos.x;
	x = x < sidePadding ? sidePadding : x;

	return Point2(x, y);
}
