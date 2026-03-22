#ifndef APPWINDOW_HPP
#define APPWINDOW_HPP

#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Window.H>
#include <vector>

class AppWindow : public Fl_Double_Window {
	std::vector<Fl_Window*> popups;
	bool closingClick = false;
	bool inEdgeZone   = false;

	static constexpr int edgeZone = 6;      // px from window edge

	int       detectEdge()        const;
	Fl_Cursor edgeCursor(int dir) const;
	void      startWmResize(int dir);

public:
	AppWindow(int w, int h) : Fl_Double_Window(w, h) {}

	void registerPopup(Fl_Window* p) { popups.push_back(p); }

	void openPopup(Fl_Window* p) {
		for (auto* q : popups)
			if (q != p) q->hide();
		p->show();
	}

	int handle(int event) override;
};

#endif
