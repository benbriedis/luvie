#ifndef MARKER_POPUP_HPP
#define MARKER_POPUP_HPP

#include <FL/Fl_Value_Input.H>
#include "modernButton.hpp"
#include "modernChoice.hpp"
#include "modern/denomBeatChoice.hpp"
#include "modern/inputEditorPopup.hpp"
#include "timeSettings.hpp"
#include <functional>

class MarkerPopup : public InputEditorPopup {
public:
	enum Kind { TEMPO, TIME_SIG };

	explicit MarkerPopup(Kind kind);

	void openTempo(int wx, int wy, bool fixed, bool showDelete, double bpm,
	               std::function<void(double)> onOk,
	               std::function<void()>       onDelete);

	void openTimeSig(int wx, int wy, bool fixed, bool showDelete,
	                 int num, int den, timeSettings::BeatUnit beat,
	                 std::function<void(int, int, timeSettings::BeatUnit)> onOk,
	                 std::function<void()>                                 onDelete);

	int handle(int event) override;

private:
	void doOk() override;
	void doDelete();
	void snapBeat();
	int  numerator() const;
	void configureDelete(bool fixed, bool showDelete);
	Kind             kind;
	Fl_Value_Input*  input1      = nullptr;
	DenomBeatChoice* denomChoice = nullptr;
	ModernButton*    deleteBtn   = nullptr;

	int deleteY    = 0;
	int popupH     = 0;
	int popupHSlim = 0;
	int popupW     = 0;   // hugs the content; set once in the constructor

	std::function<void(double)>                            onOkTempo;
	std::function<void(int, int, timeSettings::BeatUnit)>  onOkTimeSig;
	std::function<void()>                                  onDeleteCb;
};

#endif
