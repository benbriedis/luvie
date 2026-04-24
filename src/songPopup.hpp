#ifndef SONG_POPUP_HPP
#define SONG_POPUP_HPP

#include "basePopup.hpp"
#include "timeline.hpp"
#include <functional>
#include <vector>

class Grid;
class ModernButton;

class SongPopup : public BasePopup {
public:
	SongPopup();

	void open(std::vector<Note>* notes, int noteIdx, Grid* grid,
	          std::function<void()> onDelete,
	          std::function<void()> onOpenPattern);

private:
	std::function<void()> onDeleteFn;
	std::function<void()> onOpenPatternFn;

	ModernButton* openPatternBtn = nullptr;
	ModernButton* deleteBtn      = nullptr;
};

#endif
