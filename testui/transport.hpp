#ifndef TRANSPORT_HPP
#define TRANSPORT_HPP

#include <FL/Fl_Group.H>
#include <FL/Fl_Button.H>

class Transport : public Fl_Group {
	Fl_Button* rewindBtn;
	Fl_Button* stopBtn;
	Fl_Button* playPauseBtn;
	bool playing = false;

public:
	Transport(int x, int y, int w, int h);
};

#endif
