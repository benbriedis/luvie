#![allow(non_snake_case)]

use std::{fmt::Debug};
use iced::{
    Element, Length, Theme, widget::{Scrollable, container, scrollable::{Direction, Scrollbar}} 
};
use crate::gridArea::{GridArea, GridAreaMessage, GridAreaState};


mod gridArea;


fn main() -> iced::Result {
    iced::application(GridApp::new,GridApp::update,GridApp::view) 
    .theme(Theme::Light)
//    .theme(Theme::custom(Theme::Light, palette))
    .run()
}

//TODO maybe put cell stuff into its own Cell ADT file. Ditto for GridAreaState

#[derive(Debug,Default,Clone,Copy,PartialEq)]
pub struct Cell {
    row: usize,
    col: f32,       //XXX awkward name given type. Might be the best we have for the moment though
    length: f32,
    velocity: u8
}

pub struct GridSettings {
    numRows: usize,
    numCols: usize,
    rowHeight: f32,
    colWidth: f32,
    snap: Option<f32>,
    popupWidth: f32
}

#[derive(Clone, Debug, Copy)]
pub enum CellMessage {
    Add(Cell),
    Modify(usize,Cell),
    Delete(usize)
}

#[derive(Clone, Debug)]
pub enum Message {
    GridArea(GridAreaMessage),
    Cells(CellMessage)
}

struct GridApp {
    settings: GridSettings,
    cells: Vec<Cell>,
    /* Need to keep state of immediate children to work with Elm pattern */
    gridAreaState: GridAreaState,
}

impl GridApp 
{
    fn new() -> Self
    {
        let settings = GridSettings {
            numRows: 8,
            numCols: 20, 
            rowHeight: 30.0, 
            colWidth: 40.0,
            snap: Some(0.25),
            popupWidth: 200.0
        };

        Self {
            settings,
            cells: Vec::new(),
            gridAreaState: GridAreaState::default()
        }
    }

    fn update(&mut self, message: Message) {
        match message {
            Message::Cells(message) => {
                match message {
                    CellMessage::Add(cell) => self.cells.push(cell),
                    CellMessage::Modify(i,cell) => self.cells[i] = cell,
                    CellMessage::Delete(i) => { self.cells.remove(i); }
                }
            }
            _ => ()
        }
       gridArea::update(&mut self.gridAreaState,message);
    }

    fn view(&self) -> Element<'_, Message> {
        let gridArea = GridArea::new(&self.settings,&self.cells);
        let grid = gridArea.view(&self.gridAreaState);

        Scrollable::with_direction(
            container(grid),
            Direction::Both {
                vertical: Scrollbar::new(),
                horizontal: Scrollbar::new(),
            },
        )
        .width(Length::Fill)
        .height(Length::Fill)
        .into()
    }
}

