use fltk::{prelude::*, widget::Widget, *};
use crate::gridArea::grid::gridImpl::GridImpl;
use crate::{Cell, GridSettings};

pub mod gridImpl;


pub struct Grid
{
    widget: Widget
}


//TODO use self more if possible

impl Grid
{

//TODO to cope with the static lifetime maybe use multiple owners? eg
//        let color = Rc::new(RefCell::new(Color::Red));

    pub fn new<'a:'static,F1,F2,F3>(
        settings: &'a GridSettings,
        gridImpl: GridImpl<'a,F1,F2,F3>
    ) -> Self
    where
        F1: Fn(Cell) + 'static,
        F2: Fn(usize,Cell) + 'static,
        F3: Fn(usize) + 'static
    {   
        let width = (settings.numCols as f32 * settings.colWidth).round() as i32;
        let height = (settings.numRows as f32 * settings.rowHeight).round() as i32;

        let mut widget = Widget::new(0,0,width,height,None); //XXX can we use Box here instead?
//        let mut widget = Widget::default(); //XXX can we use Box here instead?
//        widget.set_frame(FrameType::FlatBox);  //XXX do we want anything like this?

//        let mode1 = Rc::new(RefCell::new(CursorMode::INIT));
//        let mode2 = Rc::clone(&mode1);

//XXX appears to more of a 'setDraw'...
        widget.draw(move |_this| { 
            gridImpl.draw();
        });

        widget.handle(move |_this, ev| {
            gridImpl.handleEvent(ev)
        });

        Self { widget }
    }
}    

// Defines a set of convenience functions for constructing and anchoring custom widgets.
// Usage: fltk::widget_extends!(CustomWidget, BaseWidget, member);
// It basically implements Deref and DerefMut on the custom widget, and adds the aforementioned methods.
// This means you can call widget methods directly, for e.x. `custom_widget.set_color(enums::Color::Red)`.
// For your custom widget to be treated as a widget it would need dereferencing: `group.add(&*custom_widget);`

fltk::widget_extends!(Grid, Widget, widget); 

