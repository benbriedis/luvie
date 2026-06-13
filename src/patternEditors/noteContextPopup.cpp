#include "noteContextPopup.hpp"
#include "FL/Fl.H"
#include "FL/Fl_Box.H"
#include "FL/Fl_Flex.H"
#include "FL/Fl_Slider.H"
#include "grid.hpp"

NoteContextPopup::NoteContextPopup() : ContextMenuPopup(0, 0)
{
	Fl_Flex *flex = new Fl_Flex(1,1,150,100);
	flex->box(FL_FLAT_BOX);
	flex->color(popupBg);
	flex->begin();
	flex->gap(10);

	Fl_Flex *sliderRow = new Fl_Flex(0, 0, 150, 30, Fl_Flex::HORIZONTAL);
	sliderRow->box(FL_FLAT_BOX);
	sliderRow->color(popupBg);
	sliderRow->begin();
	Fl_Box *velLabel = new Fl_Box(0, 0, 30, 30, "Vel");
	velLabel->labelcolor(popupText);
	velLabel->box(FL_NO_BOX);
	sliderRow->fixed(velLabel, 30);
	velSlider = new Fl_Slider(0, 0, 120, 30);
	velSlider->type(FL_HOR_NICE_SLIDER);
	velSlider->box(FL_FLAT_BOX);
	velSlider->color(popupInputBg);
	velSlider->selection_color(popupAccent);
	velSlider->bounds(0.0,1.0);
	velSlider->value(0.8);
	velSlider->when(FL_WHEN_CHANGED);
	velSlider->callback([](Fl_Widget*, void* me) {
		((NoteContextPopup*)me)->onVelocityChanged();
	}, this);
	sliderRow->end();

	ModernButton *deleteItem = new ModernButton(0, 0, 40, 30, "Delete");
	deleteItem->color(FL_WHITE);
	deleteItem->labelcolor(popupText);
	flex->fixed(deleteItem, 30);
	flex->margin(10,10,10,10);
	flex->end();

	resize(0,0,flex->w()+2,flex->h()+2);
	end();

	deleteItem->callback([](Fl_Widget*, void* me) {
		NoteContextPopup* self = (NoteContextPopup*)me;
		if (self->onDeleteFn)
			self->onDeleteFn();
		else
			self->notes->erase(self->notes->begin() + self->selected);
		self->hide();
		if (auto* win = self->window()) win->redraw();
	}, this);
}

void NoteContextPopup::onVelocityChanged()
{
	if (onVelocityFn)
		onVelocityFn((float)velSlider->value());
}

void NoteContextPopup::open(int mySelected, std::vector<Note>* myNotes, Grid* myGrid,
                 std::function<void()> onDelete, std::function<void(float)> onVelocity)
{
	selected     = mySelected;
	notes        = myNotes;
	grid         = myGrid;
	onDeleteFn   = std::move(onDelete);
	onVelocityFn = std::move(onVelocity);

	const Note& cell = (*notes)[mySelected];
	velSlider->value(cell.velocity);
	Point2 anchor = {
		(int)(grid->x() + cell.beat * grid->colWidth),
		(int)(grid->y() + cell.row * grid->rowHeight)
	};
	Fl_Window* win = grid->window();
	openAt({win->w(), win->h()}, anchor, grid->rowHeight);
}

void NoteContextPopup::openForDot(int dotX, int dotY, Fl_Widget* w, int rowH, float velocity,
                                   std::function<void()> onDelete,
                                   std::function<void(float)> onVelocity)
{
	onDeleteFn   = std::move(onDelete);
	onVelocityFn = std::move(onVelocity);
	notes = nullptr;
	grid  = nullptr;
	velSlider->value(velocity);

	Fl_Window* win = w->window();
	openAt({win->w(), win->h()}, {dotX, dotY}, rowH);
}
