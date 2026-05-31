#ifndef PATTERN_INSTANCE_CONTEXT_POPUP_HPP
#define PATTERN_INSTANCE_CONTEXT_POPUP_HPP

#include "basePopup.hpp"
#include "timeline.hpp"
#include <functional>
#include <vector>

class Grid;
class ModernButton;

class PatternInstanceContextPopup : public BasePopup {
public:
	PatternInstanceContextPopup();

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
