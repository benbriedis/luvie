#ifndef TRANSPORT_HPP
#define TRANSPORT_HPP

#include <FL/Fl_Group.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Widget.H>
#include "itransport.hpp"
#include "itimelineobserver.hpp"
#include <functional>
#include <string>

// Rounded pill on the LHS of the transport bar showing the active clock
// source. Lightish green when connected; red while waiting for JACK. Double
// click opens the Transport overlay.
class TransportIndicator : public Fl_Widget {
	std::string label;
	bool        waiting = false;

public:
	std::function<void()> onDoubleClick;

	TransportIndicator(int x, int y, int w, int h);
	void setLabel(const std::string& text);
	void setWaiting(bool w);
	void draw() override;
	int  handle(int event) override;
};


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
	TransportIndicator* indicator;
	TransportButton*    rewindBtn;
	TransportButton*    playPauseBtn;

	ITransport*         transport;
	ITransport*         controlTransport = nullptr;  // if set, buttons use this; otherwise transport
	bool                stoppedAtEnd     = false;
	bool                lastPlayingState = false;

public:
	Transport(int x, int y, int w, int h, ITransport* t);
	~Transport();
	void notifyEndReached();
	void notifySeek() { stoppedAtEnd = false; }
	void onTimelineChanged() override;
	void resize(int x, int y, int w, int h) override;

	// Disable the transport buttons (e.g. while waiting for JACK to start up).
	void disableButtons();
	// Re-enable the transport buttons once a clock source is available.
	void enableButtons();
	// Enable buttons and route their actions through ct.
	void setControlTransport(ITransport* ct);

	// Call from the FLTK main thread (e.g. via Fl::awake) when JACK transport
	// state or position has changed externally.
	void syncPlayState();

	// Update the LHS clock-source indicator.
	void setTransportLabel(const std::string& label);
	void setTransportWaiting(bool waiting);

	// Fired when the indicator is double clicked (to open the Transport overlay).
	void setIndicatorDoubleClick(std::function<void()> cb);
};

#endif
