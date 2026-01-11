#![allow(non_snake_case)]

use fltk::{app, button::Button, frame::Frame, prelude::*, window::Window};
use fltk_theme::{ColorTheme, SchemeType, ThemeType, WidgetScheme, WidgetTheme, color_themes};

use crate::{customBox::CustomBox, gridArea::createGridArea};

mod gridArea;
mod customBox;

#[derive(Debug,Default,Clone,Copy,PartialEq)]
struct Cell {
    row: usize,
    col: f32,       //XXX awkward name given type. Might be the best we have for the moment though
    length: f32
}

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

//XXX or use builder??

    let settings = GridSettings {
        numRows: 8,
        numCols: 20, 
        rowHeight: 30.0, 
        colWidth: 40.0,
        snap: Some(0.25),
        popupWidth: 200.0
    };

    let cells = Vec::new();

//    let mut but_dec = GridArea::default()
//    let mut but_dec = GridArea::new();
    createGridArea(&settings,&cells);
//pub fn createGridArea<'a>(settings:&'a GridSettings, cells: &'a Vec<Cell>) 


//    CustomBox::new(10,10, 100, 20,"mySlider" );


    wind.make_resizable(true);
    wind.end();
    wind.show();
    /* Event handling */
    app.run().unwrap();
}

