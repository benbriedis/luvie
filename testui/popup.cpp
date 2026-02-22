#include "popup.hpp"
#include "FL/Enumerations.H"
#include "FL/Fl.H"
#include "FL/Fl_Box.H"
#include "FL/Fl_Window.H"
#include "FL/Fl_Flex.H"
#include "FL/Fl_Button.H"
#include "FL/Fl_Slider.H"
#include "FL/fl_ask.H"
#include <iostream>
#include <iterator>
#include "grid.hpp"

/*
   TODO 
   1. look for pretty slider
   2. when overlapping keep the last legal position
   3. Remove grey colour
   4. Look for or create 'orbidden' cursor icon
   5. Prettier buttons

   6. Copy popup postioning code over from the Iced version.

   7. Prevent creation of items underneith the popup
*/



// Callback function for menu items
void menuCallback(Fl_Widget* w, void* user_data) {
    const Fl_Menu_Item* item = ((Fl_Menu_Button*)w)->mvalue(); // Get the selected item
    std::cout << "Selected: " << item->label() << std::endl;
    std::cout << "Selected item: " << item << std::endl;

return;

/*
    Fl_Menu_Button* menu_button = static_cast<Fl_Menu_Button*>(w);
    const Fl_Menu_Item* chosen_item = menu_button->mvalue(); // Get the chosen item
    if (chosen_item) {
//        fl_alert("Selected: %s", chosen_item->label());
    }
*/	
}

Fl_Menu_Item menutable[] = {
	{"foo",0,0,0,FL_MENU_INACTIVE},
	{"delete",0,menuCallback},
	{"button",FL_F+4, 0, 0, FL_MENU_TOGGLE},
	{0}
};


//XXX are we sure window is the one to use?

Popup::Popup() : Fl_Window(0,0,0,0)
{   
	width = 200;

//Fl_Box *frame = new Fl_Box(0, 0, 0, 0);
//frame->box(FL_ENGRAVED_BOX);

	//XXX haven't been able to size the flex up from its contents. Its meant to be possible...
	//    The children heights are being calculated from the flex box, not the other way around.
	Fl_Flex *flex = new Fl_Flex(0,0,150,100);

flex->box(FL_BORDER_BOX);

    flex->begin();
	flex->gap(10);
	Fl_Button *deleteItem = new Fl_Button(0, 0, 40, 30, "Delete");  //XXX remove new?


//XXX NOT WORKING
 flex->fixed(deleteItem, 40); 

	//XXX cf a "value" slider instead
	Fl_Slider *slider = new Fl_Slider(0, 0, 150, 30, "Vel");
    slider->type(FL_HOR_NICE_SLIDER);
    slider->box(FL_FLAT_BOX);
	slider->bounds(0.0,1.0);
	slider->value(0.5);

//XXX NOT WORKING
 flex->fixed(deleteItem, 30); 

flex->margin(10,10,10,10);

	//flex->resizable(NULL);
    flex->end();


	resize(0,0,flex->w(),flex->h());  //XXX couldnt get resizable to work
	//resizable(flex);


	end();

	deleteItem->callback([](Fl_Widget *w, void *me) {
		Popup* me2 = (Popup*)me;

		me2->notes->erase(me2->notes->begin() + me2->selected);
		me2->hide();
//XXX if not required can remove link to grid		
//		me2->grid->redraw();
	}, this);


//	Fl::grab(this);
//TODO use Fl::grab(0) to disable	
}



void Popup::open(int mySelected,std::vector<Note>* myNotes,MyGrid* myGrid) 
{ 
	selected = mySelected;
	notes = myNotes;
	grid = myGrid;

Note cell = (*notes)[mySelected];

Point desiredPosition = new Point(cell.col * grid->colWidth,cell.row * grid->rowHeight);

//    fn layout(&mut self,tree: &mut widget::Tree,renderer: &Renderer,limits: &layout::Limits) -> layout::Node
position = popupPosition(limits.max(), desiredPosition);

//TODO set to position...


	show();
}


/*
void Popup::draw() 
{
	Fl_Window::draw();
	fl_color(FL_RED);
	fl_line_style(FL_SOLID, 3);
	// We draw it slightly inside to ensure it's visible
	fl_rect(1, 1, w() - 2, h() - 2);
	fl_line_style(0);  

	/ *
        Fl_Window::draw();

        auto borderColor = FL_RED;
        auto bgColor = FL_WHITE;
        auto outPad = 5;  // Outer padding
        auto bThick = 4;  // Border thickness
        auto inPad = 5;   // Inner padding

        // 1. Draw outer background (padding)
        fl_color(bgColor);
        fl_rectf(0, 0, w(), h());

        // 2. Draw border
        fl_color(borderColor);
        fl_rectf(outPad, outPad, w() - 2*outPad, h() - 2*outPad);

        // 3. Draw inner background
        fl_color(FL_WHITE); // Or your inner content color
        fl_rectf(outPad + bThick, outPad + bThick, 
                 w() - 2*(outPad + bThick), h() - 2*(outPad + bThick));

        Fl_Window::draw();
* /		
}
*/



Point2 Popup::popupPosition(Size size, Point2 pos) 
{
	float verticalPadding = 4.0;
	//TODO probably need to account from internal padding of popup    
	float sidePadding = 30.0;

	// TODO calculate?
	float height = 85.0;

	/* Place above if place, otherwise below */
	bool placeAbove = pos.y >= (height + verticalPadding);

	float y = placeAbove ? pos.y - height - verticalPadding : pos.y + grid->rowHeight + verticalPadding;

	/* Use cell LHS if there is space, otherwise get as close as possible */
	float maxX = size.width - width - sidePadding;

	float x = pos.x > maxX ? maxX : pos.x;
	x = x < sidePadding ? sidePadding : x;

	return Point2(x,y);
}

