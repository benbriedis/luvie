#![allow(non_snake_case)]

use iced_aw::{ContextMenu, card::Status, context_menu, menu};
use std::fmt::Debug;
use iced::{
    Alignment, Background, Border, Color, Element, Length, Padding, Point, Theme, advanced::Widget, alignment::Horizontal, border::Radius, widget::{Button, Container, Row, Text, column, container, row, slider, space }
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
    contextCellIndex: Option<usize>,
    velocity: u8
}

#[derive(Clone, Debug)]
pub enum Message {
    DeleteCell,
    ValueChange(u8),        //XXX ==> VelocityChange
    CloseContext,           //XXX unused?
    Grid(GridMessage)
}

impl GridApp {

    fn new() -> GridApp
    {
        GridApp {
            cells: Vec::new(),
            contextVisible: false,
            contextCellIndex: None,
            velocity: 0
        }
    }

    fn update(&mut self, message: Message) {
println!("main GOT MESSAGE: {:?}",message);                
        match message {
            Message::ValueChange(value) => self.velocity = value,  //XXX => VelocityChange
            Message::CloseContext => self.contextVisible = false,
            Message::DeleteCell => {
                self.contextVisible = false
            }
            Message::Grid(message) => {
                match message {
                    GridMessage::AddCell(cell) => self.cells.push(cell),
                    GridMessage::ModifyCell(i,cell) => {
                        self.cells[i].col = cell.col;
                        self.cells[i].row = cell.row;
                        self.cells[i].length = cell.length;
                    }
                    GridMessage::RightClick(cellIndex) => {
println!("main.rs  GOT RightClick");                        
                        self.contextCellIndex = Some(cellIndex);
                        self.contextVisible = true
                    }
                    _ => ()
                }
            }
            _ => {
            }
        }
    }

//TODO move the combined Grid+ContextMenu into a shared widget file

    fn view(&self) -> Element<'_, Message> {
        let grid = Element::new(Grid::new(&self.cells)).map(Message::Grid);   //XXX HACK calling new and view with cells...

println!("view()  contextVisible: {}",self.contextVisible);

        if self.contextVisible {
// HACK need to set this when clicking out of context menu to close. This is possibly fragile
//self.contextVisible = false;

            ContextMenu::new(grid, || {
                container(                
                    column(vec![
                        iced::widget::button("Delete")
                            .on_press(Message::DeleteCell)
                            .into(),
                        
                        row![
                            "Velocity",
                            space::horizontal().width(Length::Fixed(8.0)),
                            slider(0..=255, self.velocity, Message::ValueChange)
                                .on_release(Message::CloseContext)
                        ].into()
                    ])
                    .spacing(10)
                    .width(200)
                    .align_x(Horizontal::Left)
                )                
                .padding(10)
                .style(|_theme| {
                    container::Style {
                        background: Some(Background::Color([1.0,1.0,1.0,1.0].into())),
//                        border: border::color(palette.background.strong.color).width(1)
                        //border: YYY,
                        ..container::Style::default()
                    }
                })
                .into()
            })
//XXX there is a 'class()' as well...            
//XXX this is just the default setting I think
            .style(|_theme:&iced::Theme, _status: Status | context_menu::Style{
                background: Background::Color([0.87, 0.87, 0.87, 0.3].into())
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
