#ifndef TRANSPORT_HPP
#define TRANSPORT_HPP

#include <FL/Fl_Group.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Widget.H>
#include "modern/basePopup.hpp"
#include "itransport.hpp"
#include "itimelineobserver.hpp"
#include <functional>
#include <string>
#include <vector>

// Hover popup listing the current alerts, shown just above the alert indicator
// while the pointer is over it. It is a sub-window of the main AppWindow (added
// via window->add()), so its position() is window-relative.
class AlertPopup : public BasePopup {
	std::vector<std::string> lines;

public:
	AlertPopup();
	void setLines(const std::vector<std::string>& l);
	void draw() override;
};


// Indicator to the right of the transport indicator. With no alerts it shows a
// '!' in a white triangle; with one or more alerts a '!' in a red triangle, and
// hovering pops up the list of alerts.
class AlertIndicator : public Fl_Widget {
	std::vector<std::string> alerts;
	AlertPopup*              popup = nullptr;

	void showPopup();

public:
	AlertIndicator(int x, int y, int w, int h);
	void setAlerts(const std::vector<std::string>& a);
	AlertPopup* popupWindow() const { return popup; }
	void draw() override;
	int  handle(int event) override;
};


class TransportButton : public Fl_Button {
public:
	enum Icon { REWIND, PLAY, PAUSE, LOOP, LOOP_OFF };

private:
	Icon icon;
	Icon altIcon;
	bool useAlt = false;
	bool visualDisabled = false;   // drawn greyed but still clickable

	void drawIcon(int cx, int cy, int s, Icon icon);

public:
	TransportButton(int x, int y, int w, int h, Icon icon, Icon altIcon = PLAY);
	void setAlt(bool alt) { useAlt = alt; }
	// Draw the button as if disabled without actually deactivating it (it stays
	// clickable). Used for the loop toggle while the app is in Loop mode.
	void setVisualDisabled(bool d) { visualDisabled = d; redraw(); }
	void draw() override;
};


class Transport : public Fl_Group, public ITimelineObserver {
	AlertIndicator*     alertIndicator;
	TransportButton*    loopBtn;
	TransportButton*    rewindBtn;
	TransportButton*    playPauseBtn;

	ITransport*         transport;
	ITransport*         controlTransport = nullptr;  // if set, buttons use this; otherwise transport
	bool                stoppedAtEnd     = false;
	bool                lastPlayingState = false;
	bool                loopOn           = false;  // song-loop toggle state

public:
	Transport(int x, int y, int w, int h, ITransport* t);
	~Transport();
	void notifyEndReached();
	void notifySeek() { stoppedAtEnd = false; }

	// Song-loop toggle (the circular button before rewind).
	bool loopEnabled() const;
	// Grey the loop toggle (Loop mode) without disabling it — it stays clickable.
	void setLoopVisualDisabled(bool d);
	std::function<void(bool loopOn)> onLoopToggled;
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

	// Replace the current set of alerts shown by the alert indicator.
	void setAlerts(const std::vector<std::string>& alerts);

	// The alert hover popup; the caller must add it to the main window
	// (window->add + registerPopup) so it positions as a sub-window.
	AlertPopup* alertPopup() const;
};

#endif
