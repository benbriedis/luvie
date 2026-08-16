// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#ifndef NOTE_CONTEXT_POPUP_HPP
#define NOTE_CONTEXT_POPUP_HPP

#include "modern/contextMenuPopup.hpp"
#include "timeline.hpp"
#include <functional>

class Grid;
class ModernSlider;
class VelocityRow;

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

	int handle(int event) override;

private:
	void onVelocityChanged();

	int selected;
	std::vector<Note>* notes;
	Grid* grid;
	VelocityRow*  velRow;
	ModernSlider* velSlider;
	std::function<void()> onDeleteFn;
	std::function<void(float)> onVelocityFn;
};

#endif
