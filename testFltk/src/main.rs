#![allow(non_snake_case)]

extern crate lazy_static;

use fltk::{app, prelude::*, window::Window};
use fltk_theme::{ColorTheme, SchemeType, WidgetScheme, color_themes};
use std::sync::Mutex;

use gridArea::GridArea;

mod gridArea;
mod customBox;

#[derive(Debug,Default,Clone,Copy,PartialEq)]
struct Cell {
    row: usize,
//XXX possibly best leaving Cell as f32
    col: f32,       //XXX awkward name given type. Might be the best we have for the moment though
    length: f32
}


/*
    TODO
    1. Implement a "sweep" or "rapid" / "rapid add" mode for quickly adding or removing notes.
*/

//TODO move to separate file
/* This is the publicly viewable state. The Grid widget will have private state too. */
struct GridState {
    /* Settings. These can be changed by outside agents in time. */
    /* Using f32 and usize to be independent of Fltk at this level and to allow for high precision graphics in future */
    numRows: usize,
    numCols: usize,
    rowHeight: f32,
    colWidth: f32,
    snap: Option<f32>,
    popupWidth: f32,

    cells: Vec<Cell>
}

//TODO rename to GridInterface / GridControl / ... ?
impl GridState {
    fn new() -> Self {
        Self {
            numRows: 8,
            numCols: 20, 
            rowHeight: 30.0, 
            colWidth: 40.0,
            snap: Some(0.25),
            popupWidth: 200.0,

            cells: Vec::new() 
        }
    }

    fn addCell(cell:Cell)
    {
        let mut cells = CELLS.lock().unwrap();
        cells.cells.push(cell);
    }

    fn modifyCell(index: usize, cell: Cell) 
    {
        let mut cells = CELLS.lock().unwrap();
        cells.cells[index] = cell; 
    }

    fn removeCell(index: usize) 
    {
        let mut cells = CELLS.lock().unwrap();
        cells.cells.remove(index);
    }

    fn getCell(index: usize) -> Cell
    {
        let cells = CELLS.lock().unwrap();
        cells.cells[index]
    }

    fn cellIterator() -> impl Iterator<Item = &Cell>
    {
        let cells = CELLS.lock().unwrap();
        cells.cells.iter()
    }
}

lazy_static::lazy_static! {
    static ref CELLS: Mutex<GridState> = Mutex::new(GridState::new());
}


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

    let gridState = GridState::new();
    GridArea::new(&gridState);

    wind.make_resizable(true);
    wind.end();
    wind.show();
    /* Event handling */
    app.run().unwrap();
}

