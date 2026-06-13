#ifndef NOTE_CONTEXT_POPUP_HPP
#define NOTE_CONTEXT_POPUP_HPP

#include "modern/contextMenuPopup.hpp"
#include "timeline.hpp"
#include <FL/Fl_Flex.H>
#include <functional>

class Grid;
class Fl_Slider;

class NoteContextPopup : public ContextMenuPopup {
public:
	NoteContextPopup();

	void open(int selected, std::vector<Note>* notes, Grid* grid,
	          std::function<void()> onDelete = nullptr,
	          std::function<void(float)> onVelocity = nullptr);

	// Variant used by drum grid: caller provides the dot's pixel position.
	void openForDot(int dotX, int dotY, Fl_Widget* w, int rowH, float velocity,
	                std::function<void()> onDelete,
	                std::function<void(float)> onVelocity = nullptr);

private:
	void onVelocityChanged();

	int selected;
	std::vector<Note>* notes;
	Grid* grid;
	Fl_Slider* velSlider;
	std::function<void()> onDeleteFn;
	std::function<void(float)> onVelocityFn;
};

#endif
