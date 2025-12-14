#ifndef POPUP_HPP
#define POPUP_HPP

#include "FL/Fl_Window.H"
#include <FL/Fl_Menu_Button.H>
#include <FL/Fl_Flex.H>

class Popup : public Fl_Window {

private:
	Fl_Flex flex;

public:
	Popup();
};

#endif

