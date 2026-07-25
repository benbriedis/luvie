#include "noteContextPopup.hpp"
#include "FL/Fl.H"
#include "FL/Fl_Box.H"
#include "FL/Fl_Group.H"
#include "FL/fl_draw.H"
#include "modern/modernSlider.hpp"
#include "noteColor.hpp"
#include "grid.hpp"

namespace {
constexpr int popupWidth   = 170;
constexpr int rowPad       = 8;   // side padding inside the velocity row
constexpr int velLabelW    = 26;
constexpr int sliderInsetY = 3;   // keeps the thumb clear of the row edges
}

// The velocity row highlights on hover like the menu items below it. Hover state
// is driven by NoteContextPopup::handle(), which sees every move over the popup
// — the slider child would otherwise swallow the row's enter/leave events.
class VelocityRow : public Fl_Group {
	bool hovered = false;

	void draw() override {
		fl_color(color());
		fl_rectf(x(), y(), w(), h());
		draw_children();
	}

public:
	VelocityRow(int x, int y, int w, int h) : Fl_Group(x, y, w, h) {
		box(FL_NO_BOX);
		color(popupBg);
	}

	// The slider paints its own background from its parent's colour, so the
	// children follow the row's tint.
	void setHovered(bool h) {
		if (h == hovered) return;
		hovered = h;
		Fl_Color c = h ? ContextMenuPopup::hoverCol : popupBg;
		color(c);
		for (int i = 0; i < children(); i++)
			child(i)->color(c);
		redraw();
	}
};

NoteContextPopup::NoteContextPopup() : ContextMenuPopup(popupWidth, 2 + 3 * btnH)
{
	velRow = new VelocityRow(1, 1, popW - 2, btnH);
	velRow->begin();

	Fl_Box *velLabel = new Fl_Box(1 + rowPad, 1, velLabelW, btnH, "Vel");
	velLabel->labelcolor(popupText);
	velLabel->box(FL_NO_BOX);
	velLabel->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

	const int sliderX = 1 + rowPad + velLabelW;
	velSlider = new ModernSlider(sliderX, 1 + sliderInsetY,
	                             popW - 1 - rowPad - sliderX, btnH - 2 * sliderInsetY);
	velSlider->type(FL_HORIZONTAL);
	velSlider->color(popupBg);
	// Thumb uses the 80%-velocity reference blue; the filled bar tracks the
	// note's velocity colour (light blue = soft, dark blue = loud).
	velSlider->selection_color(velocityFill(0.8f));
	velSlider->setFillColorFn([](double v) { return velocityFill((float)v); });
	velSlider->bounds(0.0,1.0);
	velSlider->value(0.8);
	velSlider->when(FL_WHEN_CHANGED);
	velSlider->callback([](Fl_Widget*, void* me) {
		((NoteContextPopup*)me)->onVelocityChanged();
	}, this);

	velRow->end();

	ModernButton *deleteItem = addItem(1, "Delete");
	transposeItem            = addItem(2, "Transpose");
	end();
	hide();

	deleteItem->callback([](Fl_Widget*, void* me) {
		NoteContextPopup* self = (NoteContextPopup*)me;
		if (self->onDeleteFn)
			self->onDeleteFn();
		else
			self->notes->erase(self->notes->begin() + self->selected);
		self->hide();
		if (auto* win = self->window()) win->redraw();
	}, this);

	transposeItem->callback([](Fl_Widget*, void* me) {
		NoteContextPopup* self = (NoteContextPopup*)me;
		// Hand the transpose popup this menu's position so it opens in place.
		auto fn = self->onTransposeFn;
		int  px = self->x(), py = self->y();
		self->hide();
		if (fn) fn(px, py);
	}, this);
}

int NoteContextPopup::handle(int event)
{
	switch (event) {
	case FL_ENTER:
	case FL_MOVE:
	case FL_PUSH:
		velRow->setHovered(Fl::event_inside(velRow));
		break;
	case FL_LEAVE:
	case FL_HIDE:
		velRow->setHovered(false);
		break;
	}
	return ContextMenuPopup::handle(event);
}

void NoteContextPopup::onVelocityChanged()
{
	if (onVelocityFn)
		onVelocityFn((float)velSlider->value());
}

// The Transpose item only applies to the note editors, so the menu grows and
// shrinks by one row depending on whether the opener supplied a handler.
void NoteContextPopup::showTranspose(bool on)
{
	on ? transposeItem->show() : transposeItem->hide();
	popH = 2 + (on ? 3 : 2) * btnH;
	size(popW, popH);
}

void NoteContextPopup::open(int mySelected, std::vector<Note>* myNotes, Grid* myGrid,
                 std::function<void()> onDelete, std::function<void(float)> onVelocity,
                 std::function<void(int,int)> onTranspose)
{
	selected      = mySelected;
	notes         = myNotes;
	grid          = myGrid;
	onDeleteFn    = std::move(onDelete);
	onVelocityFn  = std::move(onVelocity);
	onTransposeFn = std::move(onTranspose);
	showTranspose((bool)onTransposeFn);

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
	onDeleteFn    = std::move(onDelete);
	onVelocityFn  = std::move(onVelocity);
	onTransposeFn = nullptr;
	showTranspose(false);
	notes = nullptr;
	grid  = nullptr;
	velSlider->value(velocity);

	Fl_Window* win = w->window();
	openAt({win->w(), win->h()}, {dotX, dotY}, rowH);
}
