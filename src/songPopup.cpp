#include "songPopup.hpp"
#include "appWindow.hpp"
#include "modernButton.hpp"
#include "popupStyle.hpp"
#include "popup.hpp"
#include "grid.hpp"
#include <FL/Fl_Window.H>

static constexpr int kPad  = 10;
static constexpr int kGap  = 10;
static constexpr int kBtnH = 30;
static constexpr int kBtnX = 1 + kPad;              // 11
static constexpr int kBtnW = 130;
static constexpr int kPopW = kBtnX + kBtnW + kPad + 1; // 152
static constexpr int kPopH = 1 + kPad + kBtnH + kGap + kBtnH + kPad + 1; // 92

SongPopup::SongPopup() : BasePopup(0, 0, kPopW, kPopH)
{
	color(popupBg);
	box(FL_BORDER_BOX);

	openPatternBtn = new ModernButton(kBtnX, 1 + kPad, kBtnW, kBtnH, "Open pattern");
	openPatternBtn->color(FL_WHITE);
	openPatternBtn->labelcolor(popupText);

	deleteBtn = new ModernButton(kBtnX, 1 + kPad + kBtnH + kGap, kBtnW, kBtnH, "Delete");
	deleteBtn->color(FL_WHITE);
	deleteBtn->labelcolor(popupText);

	end();

	openPatternBtn->callback([](Fl_Widget*, void* me) {
		auto* self = (SongPopup*)me;
		if (self->onOpenPatternFn) self->onOpenPatternFn();
		self->hide();
		if (auto* win = self->window()) win->redraw();
	}, this);

	deleteBtn->callback([](Fl_Widget*, void* me) {
		auto* self = (SongPopup*)me;
		if (self->onDeleteFn) self->onDeleteFn();
		self->hide();
		if (auto* win = self->window()) win->redraw();
	}, this);
}

void SongPopup::open(std::vector<Note>* notes, int noteIdx, Grid* grid,
                     std::function<void()> onDelete, std::function<void()> onOpenPattern)
{
	onDeleteFn      = std::move(onDelete);
	onOpenPatternFn = std::move(onOpenPattern);

	const Note& cell = (*notes)[noteIdx];
	Point2 anchor = {
		(int)(grid->x() + cell.beat * grid->colWidth),
		(int)(grid->y() + cell.pitch * grid->rowHeight)
	};
	Fl_Window* win = grid->window();
	Point2 pos = calcPopupPos({win->w(), win->h()}, anchor, grid->rowHeight, w(), h());
	position(pos.x, pos.y);

	if (auto* aw = dynamic_cast<AppWindow*>(window()))
		aw->openPopup(this);
	else
		show();
}
