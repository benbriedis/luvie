#![allow(non_snake_case)]

use std::fmt::Debug;
use iced::{
    Alignment, Background, Border, Color, Element, Length, Padding, Point, Theme, advanced::Widget, alignment::Horizontal, border::Radius, widget::{Button, Container, Row, Text, center, column, container, mouse_area, opaque, row, slider, space, stack }
};
use crate::grid::{Grid, GridMessage};


mod grid;  //NOTE this does NOT say this file is in 'grid'. Rather it says look in 'grid.rs' for a 'grid' module.

/*
    1. Overlay should show near the selected cell.
    2. Close context menu.
*/

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
    HideContextMenu,
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
        match message {
            Message::HideContextMenu => self.contextVisible = true,
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
                        self.contextCellIndex = Some(cellIndex);
                        self.contextVisible = true
                    }
                    _ => ()
                }
            }
            _ => ()
        }
    }

//TODO move the combined Grid+ContextMenu into a shared widget file

    fn view(&self) -> Element<'_, Message> {
        let grid = Element::new(Grid::new(&self.cells)).map(Message::Grid);   //XXX HACK calling new and view with cells...

        if self.contextVisible {
// HACK need to set this when clicking out of context menu to close. This is possibly fragile
//self.contextVisible = false;

            let contextContent = 
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
                });

            context(grid,contextContent,Message::HideContextMenu)
        }
        else {
            grid
        }
    }
}

fn context<'a, Message>(
    base: impl Into<Element<'a, Message>>,
    content: impl Into<Element<'a, Message>>,
    on_blur: Message,
) -> Element<'a, Message>
where
    Message: Clone + 'a,
{
    stack![
        base.into(),
        opaque(
            mouse_area(center(opaque(content)).style(|_theme| {
                container::Style {
                    background: Some(
                        Color {
                            a: 0.4,
                            ..Color::BLACK
                        }
                        .into(),
                    ),
                    ..container::Style::default()
                }
            }))
            .on_press(on_blur)
        )
    ]
    .into()
}

