#ifndef TRANSPORT_HPP
#define TRANSPORT_HPP

#include <FL/Fl_Group.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Box.H>
#include "itransport.hpp"

class TransportButton : public Fl_Button {
public:
	enum Icon { REWIND, STOP, PLAY, PAUSE };

private:
	Icon icon_;
	Icon altIcon_;
	bool useAlt_ = false;

	void drawIcon(int cx, int cy, int s, Icon icon);

public:
	TransportButton(int x, int y, int w, int h, Icon icon, Icon altIcon = PLAY);
	void setAlt(bool alt) { useAlt_ = alt; }
	void draw() override;
};


class Transport : public Fl_Group {
	TransportButton* rewindBtn;
	TransportButton* stopBtn;
	TransportButton* playPauseBtn;
	Fl_Box*          posLabel_;

	ITransport* transport_;
	char        posText_[64];

	static void pollCb(void* data);
	void        updatePosition();

public:
	Transport(int x, int y, int w, int h, ITransport* transport);
};

#endif
