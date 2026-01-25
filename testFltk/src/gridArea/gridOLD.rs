use fltk::{prelude::*, widget::Widget, *};
use crate::gridArea::gridOLD::gridImpl::GridImpl;
use crate::{Cell, GridSettings};

pub mod gridImpl;


pub struct Grid<F1>
where
    F1: Fn(usize)
{
}


//TODO use self more if possible

//TODO consider re-cobining grid + gridImpl

impl<F1> Grid<F1>
where
    F1: Fn(usize)
{

//TODO to cope with the static lifetime maybe use multiple owners? eg
//        let color = Rc::new(RefCell::new(Color::Red));

    pub fn new(
        settings: GridSettings,
        mut gridImpl: GridImpl<F1>
    ) -> Self
    where
        F1: Fn(usize)
    {   
        let width = (settings.numCols as f32 * settings.colWidth).round() as i32;
        let height = (settings.numRows as f32 * settings.rowHeight).round() as i32;

        let mut widget = Widget::new(0,0,width,height,None); //XXX can we use Box here instead?
//        let mut widget = Widget::default(); //XXX can we use Box here instead?
//        widget.set_frame(FrameType::FlatBox);  //XXX do we want anything like this?

//        let mode1 = Rc::new(RefCell::new(CursorMode::INIT));
//        let mode2 = Rc::clone(&mode1);

//XXX appears to more of a 'setDraw'...
// fn draw<F: FnMut(&mut Self) + 'static>(&mut self, cb: F) {

        widget.draw(|_this| { 
            (&gridImpl).draw();
        });

// fn handle<F: FnMut(&mut Self, $crate::enums::Event) -> bool + 'static>(&mut self, cb: F) {
//commenting out 'addCell' SEEMS to help: Fn(Cell) + 'a
        widget.handle(|_this, ev| {
            (&mut gridImpl).handleEvent(ev)
        });

        Self { widget, gridImpl }
    }
}    

// Defines a set of convenience functions for constructing and anchoring custom widgets.
// Usage: fltk::widget_extends!(CustomWidget, BaseWidget, member);
// It basically implements Deref and DerefMut on the custom widget, and adds the aforementioned methods.
// This means you can call widget methods directly, for e.x. `custom_widget.set_color(enums::Color::Red)`.
// For your custom widget to be treated as a widget it would need dereferencing: `group.add(&*custom_widget);`

fltk::widget_extends!(Grid, Widget, widget); 

