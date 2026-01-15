#![allow(non_snake_case)]

use std::{cell::RefCell, rc::Rc};

use fltk::{app, button::Button, frame::Frame, prelude::*, window::Window};
use fltk_theme::{ColorTheme, SchemeType, ThemeType, WidgetScheme, WidgetTheme, color_themes};

use crate::{customBox::CustomBox, gridArea::{GridArea}};

mod gridArea;
mod customBox;

#[derive(Debug,Default,Clone,Copy,PartialEq)]
struct Cell {
    row: usize,
//XXX possibly best leaving Cell as f32
    col: f32,       //XXX awkward name given type. Might be the best we have for the moment though
    length: f32
}

/* Using f32 and usize to be independent of Fltk at this level and to allow for high precision graphics in future */
struct GridSettings {
    numRows: usize,
    numCols: usize,
    rowHeight: f32,
    colWidth: f32,
    snap: Option<f32>,
    popupWidth: f32
}

struct GridApp {
    settings: GridSettings,
    cells: Vec<Cell>,
    contextVisible: bool,
    contextCellIndex: Option<usize>,
    velocity: u8
}

/*
    TODO
    1. Implement a "sweep" or "rapid" / "rapid add" mode for quickly adding or removing notes.
*/

//XXX do we really have to use static? Fltk's draw() function is requiring it

static cells_rc_refcell:Rc<RefCell<Vec<Cell>>> = Rc::new(RefCell::new(Vec::new()));

static SETTINGS: GridSettings = GridSettings {
    numRows: 8,
    numCols: 20, 
    rowHeight: 30.0, 
    colWidth: 40.0,
    snap: Some(0.25),
    popupWidth: 200.0
};

fn main() {
//    let app = app::App::default().with_scheme(app::Scheme::Gtk);
    let app = app::App::default().with_scheme(app::Scheme::Oxy);

    let theme = ColorTheme::new(color_themes::BLACK_THEME);
    theme.apply();

//    let widgetTheme = WidgetTheme::new(ThemeType::AquaClassic);
//    widgetTheme.apply();

    let widgetScheme = WidgetScheme::new(SchemeType::Clean);
    widgetScheme.apply();

    let mut wind = Window::default()
        .with_size(160, 200)
        .center_screen()
        .with_label("Counter");
/*
    let mut frame = Frame::default()
        .with_size(100, 40)
        .center_of(&wind)
        .with_label("0");
    let mut but_inc = Button::default()
        .size_of(&frame)
        .above_of(&frame, 0)
        .with_label("+");
    let mut but_dec = Button::default()
        .size_of(&frame)
        .below_of(&frame, 0)
        .with_label("-");
*/

//XXX or use builder??

/*    
    let settings = GridSettings {
        numRows: 8,
        numCols: 20, 
        rowHeight: 30.0, 
        colWidth: 40.0,
        snap: Some(0.25),
        popupWidth: 200.0
    };

    let cells = Vec::new();
*/

/*
    let onAddCell = move |cell:Cell| {
        let numCells = CELLS.len();
        println!("Got onAddCell cell.length:{:?} numCells:{numCells}",cell.length);
        CELLS.push(cell);
    };
*/

    let onAddCell = {
        let cells_clone = Rc::clone(&cells_rc_refcell);
        move |cell: Cell| {
            let mut cells = cells_clone.borrow_mut(); // Get mutable borrow at runtime
            println!("Got onAddCell");
            cells.push(cell);
        }
    };

    /*
    let onModifyCell = move |cellIndex:usize,cell:&mut Cell| {
        let numCells = CELLS.len();
        println!("Got right click cellIndex:{cellIndex} numCells:{numCells}");
    };
    */

    let onModifyCell = {
        let cells_clone = Rc::clone(&cells_rc_refcell);
        move |cellIndex: usize, cell: &mut Cell| { // Note: cell:&mut Cell is tricky here
            let mut cells = cells_clone.borrow_mut();
            println!("Got right click");
            // cells[cellIndex] = cell; // Or update in place
            // You'll need to manage lifetimes carefully when passing &mut Cell
        }
    };

//XXX why are we cloning this thing?
    GridArea::new(&SETTINGS,&cells_rc_refcell,onAddCell,onModifyCell);

//    CustomBox::new(10,10, 100, 20,"mySlider" );


    wind.make_resizable(true);
    wind.end();
    wind.show();
    /* Event handling */
    app.run().unwrap();
}

