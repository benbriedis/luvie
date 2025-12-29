#![allow(non_snake_case)]

use iced_aw::ContextMenu;
use std::fmt::Debug;
use iced::{
    Alignment, Element, Length, Point, Theme, advanced::Widget, alignment::Horizontal, widget::{Button, Container, Row, Text, column, row, slider, space }
};
use crate::grid::{Grid, GridMessage};


mod grid;  //NOTE this does NOT say this file is in 'grid'. Rather it says look in 'grid.rs' for a 'grid' module.


fn main() -> iced::Result {
    iced::application(GridApp::new,GridApp::update,GridApp::view) 
    .theme(Theme::Light)
//    .theme(Theme::custom(Theme::Light, palette))
    .run()
}

#[derive(Debug,Default,Clone,Copy,PartialEq)]
struct Cell {
    row: usize,
    col: f32,       //XXX awkward name given type. Might be the best we have for the moment though
    length: f32
}

struct GridApp {
    cells: Vec<Cell>,
    contextVisible: bool,
    velocity: u8
}

#[derive(Clone, Debug)]
pub enum Message {
    ButtonClicked,
    Choice1,
    Choice2,
    ValueChange(u8),
    CloseContext,

//    Child(GridMessage)
    Grid(GridMessage)
}

impl GridApp {
    /*
    fn default() -> Self
    {
        GridApp {
            grid: Grid::new(&cells),
            cells: Vec::new(),
            contextVisible: false,
            velocity: 0
        }
    }
    */

//    fn new(cells:Vec<Cell>) -> GridApp
    fn new() -> GridApp
    {
        GridApp {
            cells: Vec::new(),
            contextVisible: false,
            velocity: 0
        }
    }

    fn update(&mut self, message: Message) {
        match message {
            Message::ButtonClicked => self.contextVisible = true,  //TODO delete
            Message::ValueChange(value) => self.velocity = value,  //XXX => VelocityChange
            Message::Choice1 => self.contextVisible = false,
            Message::Choice2 => self.contextVisible = false,
            Message::CloseContext => self.contextVisible = false,
            Message::Grid(message) => {
println!("Got message from Grid: {:?}",message);                
                match message {
                    GridMessage::AddCell(cell) => self.cells.push(cell),
//                    GridMessage::MoveCell(i,cell) => self.cells[i] = cell,
                    GridMessage::ModifyCell(i,cell) => {
                        self.cells[i].col = cell.col;
                        self.cells[i].row = cell.row;
                        self.cells[i].length = cell.length;
                    }
//                    GridMessage::ResizeCell(i,cell) => self.cells[i] = cell,
                    GridMessage::RightClick(cell) => self.contextVisible = true,  //TODO use cell
                    _ => ()
                }
            }

/*            
            GridMessage::AddCell(cell) => self.cells.push(cell),
            GridMessage::DeleteCell(i) => (), //TODO
            GridMessage::MoveCell(i,cell) => (), //TODO copy values in
            GridMessage::ResizeCell(i,cell) => (),

            GridMessage::RightClick(point) => {
                println!("GOT right click Point coordinates: ({}, {})", point.x, point.y);
                self.contextVisible = true;
            }
*/
            _ => ()
        }
    }

//TODO pass in state
//TODO move the combined Grid+ContextMenu into a shared widget file

    fn view(&self) -> Element<'_, Message> {
println!("Called view()");
//        let grid = self.grid.view(&self.cells).map(Message::Grid);   //XXX HACK calling new and view with cells...
//        let grid = Grid::view(&self.cells).map(Message::Grid);   //XXX HACK calling new and view with cells...
//        let grid = Grid::new(&self.cells).map(Message::Grid);   //XXX HACK calling new and view with cells...

//        let grid = Grid::new(&self.cells).view(&self.cells).map(Message::Grid);   //XXX HACK calling new and view with cells...
//        let grid = Grid::new(&self.cells).map(Message::Grid);   //XXX HACK calling new and view with cells...
        let grid = Element::new(Grid::new(&self.cells)).map(Message::Grid);   //XXX HACK calling new and view with cells...

        if self.contextVisible {
println!("Called view()  - in contextVisible");
            ContextMenu::new(grid, || {
                column(vec![
                    iced::widget::button("Choice 1")
                        .on_press(Message::Choice1)
                        .into(),
                    iced::widget::button("Choice 2")
                        .on_press(Message::Choice2)
                        .into(),
                    
                    row![
                        "Velocity",
                        space::horizontal().width(Length::Fixed(8.0)),
                        slider(0..=255, self.velocity, Message::ValueChange)
                            .on_release(Message::CloseContext)
                    ].into()
                ])
                .width(200)
                .align_x(Horizontal::Left)
                .into()
            })
            .into()
        }
        else {
println!("Called view() - no contextVisible cells.length:{}",self.cells.len());
            grid
        }
    }
}

//        content.map(Message::Counter)
