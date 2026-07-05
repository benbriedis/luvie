#ifndef MODERN_TABS_HPP
#define MODERN_TABS_HPP

#include <FL/Fl_Tabs.H>
#include <functional>
#include <vector>

class ModernTabs : public Fl_Tabs {
	static constexpr Fl_Color inactiveTab  = 0xF3F4F600;
	static constexpr Fl_Color defaultAccent = 0x3B82F600;
	static constexpr Fl_Color separator    = 0xD1D5DB00;
	static constexpr int      accentH      = 3;
	static constexpr int      toggleW      = 70;

	bool     modeIsLoop   = false;
	bool     transitioning = false;       // Loop→Song hand-off in progress (button yellow)
	bool     toggleHovered = false;
	int      leftReserve  = 0;
	int      rightReserve = 0;            // space reserved for a right-docked widget
	Fl_Widget* rightWidget = nullptr;     // e.g. the Settings gear button
	Fl_Color songColor    = 0x22C55E00;
	Fl_Color loopColor    = 0x3B82F600;
	Fl_Color transitionColor = 0xF59E0B00;  // amber, shown during the Loop→Song transition

	std::vector<Fl_Color> tabAccents;  // per-tab accent override

	int  tabBarH() const;
	void drawModeToggle(int tbH);
	void layoutRightWidget();

public:
	// The committed visual state of the mode toggle. Song and Loop are stable;
	// Transitioning is the yellow "Loop"-labelled state during a Loop→Song hand-off.
	// The button itself no longer commits a mode on click — it only *requests* one
	// via onModeChanged; the controller drives the visual back through setModeVisual.
	enum class ModeVisual { Song, Loop, Transitioning };

	std::function<void(bool isLoop)> onModeChanged;

	ModernTabs(int x, int y, int w, int h);

	void enableModeToggle(Fl_Color songCol, Fl_Color loopCol);
	void setModeVisual(ModeVisual v);
	// Dock a widget at the right end of the tab bar; tabs shrink to make room.
	void setRightWidget(Fl_Widget* w, int reserveW);
	void setTabAccent(int tabIdx, Fl_Color c);
	bool modeLoop() const { return modeIsLoop; }

	void draw() override;
	void resize(int x, int y, int w, int h) override;
	int  handle(int event) override;
};

#endif
