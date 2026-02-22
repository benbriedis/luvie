#ifndef POPUP_HPP
#define POPUP_HPP

#include "cell.hpp"
#include "FL/Fl_Window.H"
#include <FL/Fl_Menu_Button.H>
#include <FL/Fl_Flex.H>

class Popup : public Fl_Window {

public:
	Popup();

	void open(int selected,std::vector<Note> notes);

  inline void deleteCallbackI();

protected:	

//    void draw() override;
//	int handle(int event) override;

private:
//XXX maybe move into outerGrid.hpp...	
	int selected;
	std::vector<Note> notes;
};

#endif

