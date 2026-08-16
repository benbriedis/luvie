// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#ifndef APPWINDOW_HPP
#define APPWINDOW_HPP

#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Window.H>
#include <functional>
#include <vector>

class AppWindow : public Fl_Double_Window {
	std::vector<Fl_Window*> popups;
	bool closingClick = false;
	bool inEdgeZone   = false;

	static constexpr int edgeZone = 6;      // px from window edge

	int       detectEdge()        const;
	Fl_Cursor edgeCursor(int dir) const;
	// Hands an interactive resize to the window manager. Returns false if this
	// platform has no such handoff, in which case the edge zone must not claim
	// the click — see wmResizeAvailable().
	bool      startWmResize(int dir);
	static bool wmResizeAvailable();

public:
	AppWindow(int w, int h) : Fl_Double_Window(w, h) {}

	void registerPopup(Fl_Window* p) { popups.push_back(p); }

	// Undo (ctrl-Z) and redo (ctrl-Y) are window-level accelerators rather than
	// grid ones: the grids never take keyboard focus, so there is no widget to
	// hang them off. An inline text input that does hold focus sees ctrl-Z first
	// and consumes it, so its own undo still works.
	std::function<void()> onUndo;
	std::function<void()> onRedo;

	// Escape clears any active multi-selection. It has to be routed through the
	// window because the grids never take keyboard focus and the window swallows
	// Escape before FLTK's shortcut fallback would reach them. Returns true when
	// something was actually cleared; the key is consumed either way, as before.
	std::function<bool()> onEscape;

	// Ctrl-A selects everything in the editor that is showing. Window-level for
	// the same reason as Escape: the grids never take focus, so FLTK only reaches
	// them by broadcasting the shortcut, and each one then has to work out
	// whether it was the intended target from where the cursor happens to be.
	std::function<void()> onSelectAll;

	// Delete/BackSpace with a multi-selection active, same reasoning again.
	// Returns false when there was nothing selected, in which case the key is
	// left to travel on: the grids also delete the single note under the cursor,
	// and that one genuinely does depend on where the cursor is.
	std::function<bool()> onDeleteSelection;

	// Every left/right press that reaches the window, in window coordinates,
	// before the widget under it sees it. Used to dismiss a multi-selection when
	// the click lands anywhere but the grid that owns it — the grid itself never
	// hears about clicks on its neighbours.
	std::function<void(int, int)> onClick;

	void openPopup(Fl_Window* p) {
		for (auto* q : popups)
			if (q != p) q->hide();
		p->show();
	}

	int handle(int event) override;
};

#endif
