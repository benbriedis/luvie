#include "transport.hpp"
#include <iostream>

Transport::Transport(int x, int y, int w, int h) : Fl_Group(x, y, w, h) {
	const int btnW = 50, btnH = 32, gap = 8;
	const int totalW = 3 * btnW + 2 * gap;
	const int bx = x + (w - totalW) / 2;
	const int by = y + (h - btnH) / 2;

	rewindBtn = new Fl_Button(bx, by, btnW, btnH, "\u23ee");  // ⏮
	rewindBtn->box(FL_FLAT_BOX);
	rewindBtn->callback([](Fl_Widget*, void*) {
		std::cout << "Rewind to start pressed\n";
	});

	stopBtn = new Fl_Button(bx + btnW + gap, by, btnW, btnH, "\u23f9");  // ⏹
	stopBtn->box(FL_FLAT_BOX);
	stopBtn->callback([](Fl_Widget*, void*) {
		std::cout << "Stop pressed\n";
	});

	playPauseBtn = new Fl_Button(bx + 2 * (btnW + gap), by, btnW, btnH, "\u25b6");  // ▶
	playPauseBtn->box(FL_FLAT_BOX);
	playPauseBtn->callback([](Fl_Widget* w, void* data) {
		Transport* t = (Transport*)data;
		t->playing = !t->playing;
		if (t->playing) {
			w->label("\u23f8");  // ⏸
			std::cout << "Play pressed\n";
		} else {
			w->label("\u25b6");  // ▶
			std::cout << "Pause pressed\n";
		}
		w->redraw();
	}, this);

	end();
}
