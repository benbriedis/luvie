#include "appWindow.hpp"
#include <FL/Fl.H>

int AppWindow::handle(int event)
{
	switch (event) {
	case FL_PUSH:
	case FL_DRAG:
	case FL_RELEASE:
	case FL_MOVE:
	case FL_MOUSEWHEEL: {
		Fl_Window* active = nullptr;
		for (auto* p : popups)
			if (p->visible()) { active = p; break; }
		if (active) {
			int ex = Fl::event_x(), ey = Fl::event_y();
			bool inPopup = ex >= active->x() && ex < active->x() + active->w()
			            && ey >= active->y() && ey < active->y() + active->h();
			if (!inPopup) return 1;  // swallow — block everything underneath
		}
		break;
	}
	default:
		break;
	}
	return Fl_Double_Window::handle(event);
}
