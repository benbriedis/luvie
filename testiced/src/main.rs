#![allow(non_snake_case)]

use std::{fmt::Debug};
use iced::{
    Element, Length, Theme, widget::{Scrollable, column, container, scrollable::{Direction, Scrollbar}} 
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

#[derive(Debug)]
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
    GridArea1(GridAreaMessage),
    GridArea2(GridAreaMessage),
    Cells1(CellMessage),
    Cells2(CellMessage)
}

struct GridApp {
    settings1: GridSettings,
    cells1: Cells,

    settings2: GridSettings,
    cells2: Cells,
    /*
       Need to keep state of immediate children to work with Elm pattern. 
       Could Widget state / tree state somehow be used for this instead?
    */
    gridAreaState1: GridAreaState,
    gridAreaState2: GridAreaState,
}

impl GridApp 
{
    fn new() -> Self
    {
        let settings1 = GridSettings {
            numRows: 8,
            numCols: 20, 
            rowHeight: 30.0, 
            colWidth: 40.0,
            snap: Some(0.25),
            popupWidth: 200.0
        };

        let settings2 = GridSettings {
            numRows: 8,
            numCols: 20, 
            rowHeight: 35.0, 
            colWidth: 50.0,
            snap: Some(0.25),
            popupWidth: 250.0
        };

        Self {
            settings1,
            cells1: Vec::new(),

            settings2,
            cells2: Vec::new(),

            gridAreaState1: GridAreaState::default(),
            gridAreaState2: GridAreaState::default()
        }
    }

    fn update(&mut self, message: Message) {
//FIXME switch between 1 and 2        
        if let Message::Cells1(msg) = message {
            cells::update(&mut self.cells1, msg);
        }
        if let Message::Cells2(msg) = message {
            cells::update(&mut self.cells2, msg);
        }
        if let Message::GridArea1(msg) = message {
            gridArea::update(&mut self.gridAreaState1,msg);
        }
        if let Message::GridArea2(msg) = message {
            gridArea::update(&mut self.gridAreaState1,msg);
        }
    }

    fn view(&self) -> Element<'_, Message> {
        let gridArea1 = GridArea::new(&self.settings1,&self.cells1);
        let grid1 = gridArea1.view(&self.gridAreaState1).map(|msg| { 
            match msg {
                GridAreaMessage::Cells(msg2) => {
                    Message::Cells1(msg2)
                }
                _ => Message::GridArea1(msg)
            }
        });

//    window::resize_events().map(|(_id, size)| Message::WindowResized(size))
//task.map(Message::Conversation)

        let gridArea2 = GridArea::new(&self.settings2,&self.cells2);
        let grid2 = gridArea2.view(&self.gridAreaState2).map(|msg| {
            match msg {
                GridAreaMessage::Cells(msg2) => {
                    Message::Cells2(msg2)
                }
                _ => Message::GridArea2(msg)
            }
        });

        // -1.0 hack to prevent weird bug
        let gridHeight = self.settings1.numRows as f32 * self.settings1.rowHeight - 1.0; 
        println!("Height: {}",gridHeight);

        column![
/*            
            Scrollable::with_direction(
                container(grid),
                Direction::Both {
                    vertical: Scrollbar::new(),
                    horizontal: Scrollbar::new(),
                },
            )
            .width(Length::Fill)
            .height(gridHeight),
*/
/*            
            container(grid1)
                .width(Length::Fill)
                .height(gridHeight),

            container(grid2)
                .width(Length::Fill)
                .height(gridHeight),
*/
            container(grid1).width(Length::Fill).height(gridHeight),
            container(grid2).width(Length::Fill).height(gridHeight),
        ]
        .spacing(30)
        .width(Length::Fill)
        .into()
    }
}

