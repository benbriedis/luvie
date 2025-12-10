
#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Sys_Menu_Bar.H>
#include <FL/Fl_Toggle_Button.H>
#include <FL/Fl_Menu_Button.H>
#include <FL/Fl_Scheme_Choice.H>
#include <FL/Fl_Value_Slider.H>
#include <stdio.h>
#include <stdlib.h>
#include <FL/fl_draw.H>
#include <FL/Fl_Terminal.H>
#include <FL/fl_ask.H>
#include <FL/fl_string_functions.h>



Fl_Menu_Item menutable[] = {
  {"foo",0,0,0,FL_MENU_INACTIVE},
  {"&File",0,0,0,FL_SUBMENU},
  {"button",FL_F+4, 0, 0, FL_MENU_TOGGLE},
  {0}
};


int main(int argc, char **argv) {
  Fl_Window window(700,500);

  Fl_Menu_Button mb(0,0,400,400,"&popup");
  mb.type(Fl_Menu_Button::POPUP3);
  mb.menu(menutable);

  window.resizable(&mb);
//  window.size_range(300,400,0,500);

  window.end();

  window.show(argc, argv);
  return Fl::run();
}
