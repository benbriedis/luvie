#ifndef MARKER_POPUP_HPP
#define MARKER_POPUP_HPP

#include <FL/Fl_Window.H>
#include <FL/Fl_Value_Input.H>
#include "modernButton.hpp"
#include <functional>

class MarkerPopup : public Fl_Window {
public:
	enum Kind { TEMPO, TIME_SIG };

	explicit MarkerPopup(Kind kind);

	void openTempo(int wx, int wy, bool fixed, double bpm,
	               std::function<void(double)> onOk,
	               std::function<void()>       onDelete);

	void openTimeSig(int wx, int wy, bool fixed, int num, int den,
	                 std::function<void(int, int)> onOk,
	                 std::function<void()>          onDelete);

	int  handle(int event) override;
	void hide() override;

private:
	void doOk();
	void doDelete();
	bool committed = false;
	Kind            kind;
	Fl_Value_Input* input1    = nullptr;
	Fl_Value_Input* input2    = nullptr;
	ModernButton*   deleteBtn = nullptr;

	std::function<void(double)>   onOkTempo;
	std::function<void(int, int)> onOkTimeSig;
	std::function<void()>         onDeleteCb;
	bool onSecondField = false;
};

#endif
