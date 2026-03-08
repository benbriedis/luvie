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
		// Swallow the FL_RELEASE that follows a click which closed a popup.
		if (event == FL_RELEASE && closingClick) {
			closingClick = false;
			return 1;
		}

		Fl_Window* active = nullptr;
		for (auto* p : popups)
			if (p->visible()) { active = p; break; }
		if (active) {
			int ex = Fl::event_x(), ey = Fl::event_y();
			bool inPopup = ex >= active->x() && ex < active->x() + active->w()
			            && ey >= active->y() && ey < active->y() + active->h();
			if (!inPopup) {
				if (event == FL_PUSH) { active->hide(); closingClick = true; }
				return 1;
			}
		}
		break;
	}
	default:
		break;
	}
	return Fl_Double_Window::handle(event);
}
