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
	bool     toggleHovered = false;
	int      leftReserve  = 0;
	Fl_Color songColor    = 0x22C55E00;
	Fl_Color loopColor    = 0x3B82F600;

	std::vector<Fl_Color> tabAccents;  // per-tab accent override

	int  tabBarH() const;
	void drawModeToggle(int tbH);

public:
	std::function<void(bool isLoop)> onModeChanged;

	ModernTabs(int x, int y, int w, int h);

	void enableModeToggle(Fl_Color songCol, Fl_Color loopCol);
	void setTabAccent(int tabIdx, Fl_Color c);
	bool modeLoop() const { return modeIsLoop; }

	void draw() override;
	void resize(int x, int y, int w, int h) override;
	int  handle(int event) override;
};

#endif
