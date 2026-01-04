#![allow(non_snake_case)]

use std::{fmt::Debug};
use iced::{
    Background, Color, Element, Length::{self}, Point, Theme, 
    alignment::Horizontal, 
    border::radius, 
    widget::{
        Scrollable, column, container, mouse_area, opaque, row, scrollable::{Direction, Scrollbar}, slider, space, stack 
    }
};
use crate::{contextMenuPopup::ContextMenuPopup, grid::{Grid, GridMessage}};


mod grid;  //NOTE this does NOT say this file is in 'grid'. Rather it says look in 'grid.rs' for a 'grid' module.
mod contextMenuPopup;


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

#[derive(Clone, Debug)]
pub enum Message {
    HideContextMenu,
    DeleteCell,
    ValueChange(u8),        //XXX ==> VelocityChange
    CloseContext,           //XXX unused?
    Grid(GridMessage)
}

impl GridApp {

//TODO move the combined Grid+ContextMenu into a shared widget file

    fn new() -> GridApp
    {
        let settings = GridSettings {
            numRows: 8,
            numCols: 20, 
            rowHeight: 30.0, 
            colWidth: 40.0,
            snap: Some(0.25),
            popupWidth: 200.0
        };

        GridApp {
            settings,
            cells: Vec::new(),
            contextVisible: false,
            contextCellIndex: None,
            velocity: 0
        }
    }

    fn update(&mut self, message: Message) {
        match message {
            Message::HideContextMenu => self.contextVisible = false,
            Message::ValueChange(value) => self.velocity = value,  //XXX => VelocityChange
            Message::CloseContext => self.contextVisible = false,
            Message::DeleteCell => {
                self.cells.remove(self.contextCellIndex.unwrap());
                self.contextCellIndex = None;
                self.contextVisible = false;
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
                        self.contextVisible = true;
                    }
                    _ => ()
                }
            }
            _ => ()
        }
    }

    fn view(&self) -> Element<'_, Message> {
        let grid = Element::new(Grid::new(&self.settings,&self.cells)).map(Message::Grid);   //XXX HACK calling new and view with cells...

        let outer = Scrollable::with_direction(
            container(grid),
            Direction::Both {
                vertical: Scrollbar::new(),
                horizontal: Scrollbar::new(),
            },
        )
        .width(Length::Fill)
        .height(Length::Fill);

        if self.contextVisible {
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
                    .width(self.settings.popupWidth)
                    .align_x(Horizontal::Left)
                )                
                .padding(10)
                .style(|_theme| {
                    container::Style {
                        background: Some(Background::Color([1.0,1.0,1.0,1.0].into())),
                        border: iced::Border {color:Color{r:0.6,g:0.6,b:0.8,a:1.0}, width:2.0, radius:radius(0.1) },
                        //border: YYY,
                        ..container::Style::default()
                    }
                });

            let cell = self.cells[self.contextCellIndex.unwrap()];
            let pos = Point{
                x: cell.col * self.settings.colWidth,
                y: cell.row as f32 * self.settings.rowHeight
            };
            context(outer,contextContent,Message::HideContextMenu,&self.settings,pos)
        }
        else {
            outer.into()
        }
    }
}

//XXX cf function and parameter names here...

fn context<'a, Message>(
    base: impl Into<Element<'a, Message>>,
    content: impl Into<Element<'a, Message>>,
    on_blur: Message,
    settings: &GridSettings,
    pos: Point
) -> Element<'a, Message>
where
    Message: Clone + 'a,
{
//TODO integrate more of this into the popup code?
    let contextPopup = ContextMenuPopup::new(
        pos,
        settings.popupWidth,
        settings.rowHeight,

        container(
            opaque(
                content
            )
        )
    );

    stack![
        base.into(),
        opaque(
            mouse_area(
                container(
                    contextPopup
                )
            )
            .on_press(on_blur)
        )
    ]
    .into()
}

