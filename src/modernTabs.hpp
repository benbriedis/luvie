#ifndef MODERN_TABS_HPP
#define MODERN_TABS_HPP

#include <FL/Fl_Tabs.H>

class ModernTabs : public Fl_Tabs {
	static constexpr Fl_Color inactiveTab = 0xF3F4F600;
	static constexpr Fl_Color accent      = 0x3B82F600;
	static constexpr Fl_Color separator   = 0xD1D5DB00;
	static constexpr int      accentH     = 3;

	int tabBarH() const;

public:
	ModernTabs(int x, int y, int w, int h);

	void draw() override;
	void resize(int x, int y, int w, int h) override;
	int handle(int event) override;
};

#endif
