#ifndef TRANSPORT_HPP
#define TRANSPORT_HPP

#include <FL/Fl_Group.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Box.H>
#include "itransport.hpp"
#include "observableTimeline.hpp"

class TransportButton : public Fl_Button {
public:
	enum Icon { REWIND, PLAY, PAUSE };

private:
	Icon icon;
	Icon altIcon;
	bool useAlt = false;

	void drawIcon(int cx, int cy, int s, Icon icon);

public:
	TransportButton(int x, int y, int w, int h, Icon icon, Icon altIcon = PLAY);
	void setAlt(bool alt) { useAlt = alt; }
	void draw() override;
};


class Transport : public Fl_Group, public ITimelineObserver {
	TransportButton*    rewindBtn;
	TransportButton*    playPauseBtn;
	Fl_Box*             posLabel;

	ITransport*         transport;
	ObservableTimeline* timeline;
	bool                stoppedAtEnd = false;
	char                posText[64];

	static void pollCb(void* data);
	void        updatePosition();

public:
	Transport(int x, int y, int w, int h, ITransport* t, ObservableTimeline* tl);
	~Transport();
	void notifyEndReached();
	void notifySeek() { stoppedAtEnd = false; }
	void onTimelineChanged() override;
	void resize(int x, int y, int w, int h) override;
};

#endif
