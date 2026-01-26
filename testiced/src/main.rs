#![allow(non_snake_case)]

use std::{fmt::Debug};
use iced::{
    Element, Length, Theme, widget::{Scrollable, container, scrollable::{Direction, Scrollbar}} 
};
use crate::{cells::{CellMessage, Cells}, gridArea::{GridArea, GridAreaMessage, GridAreaState}};


mod gridArea;
mod cells;


fn main() -> iced::Result {
    iced::application(GridApp::new,GridApp::update,GridApp::view) 
    .theme(Theme::Light)
//    .theme(Theme::custom(Theme::Light, palette))
    .run()
}

//TODO maybe put cell stuff into its own Cell ADT file. Ditto for GridAreaState

pub struct GridSettings {
    numRows: usize,
    numCols: usize,
    rowHeight: f32,
    colWidth: f32,
    snap: Option<f32>,
    popupWidth: f32
}

#[derive(Clone, Debug)]
pub enum Message {
    GridArea(GridAreaMessage),
    Cells(CellMessage)
}

struct GridApp {
    settings: GridSettings,
    cells: Cells,
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
            Message::Cells(message) => cells::update(&mut self.cells, message),
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

