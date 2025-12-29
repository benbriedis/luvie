#![allow(non_snake_case)]

use iced_aw::ContextMenu;
use std::fmt::Debug;
use iced::{
    Alignment, Element, Length, Theme, advanced::Widget, alignment::Horizontal, widget::{Button, Container, Row, Text, column, row, slider, space }
};
use crate::grid::{Grid};


mod grid;  //NOTE this does NOT say this file is in 'grid'. Rather it says look in 'grid.rs' for a 'grid' module.


fn main() -> iced::Result {
    iced::application(GridApp::new,GridApp::update,GridApp::view) 
    .theme(Theme::Light)
//    .theme(Theme::custom(Theme::Light, palette))
    .run()
}

struct GridApp {
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
    RightClick
}

impl GridApp {
    fn new() -> GridApp
    {
        GridApp {
            contextVisible: false,
            velocity: 0
        }
    }

    fn update(&mut self, message: Message) {
        match message {
            Message::ButtonClicked => self.contextVisible = true,  //TODO delete
            Message::ValueChange(value) => self.velocity = value,
            Message::Choice1 => self.contextVisible = false,
            Message::Choice2 => self.contextVisible = false,
            Message::CloseContext => self.contextVisible = false,
            Message::RightClick => {
println!("Got right click!");                
                self.contextVisible = true
            }
            _ => ()
        }
    }

//TODO move the combined Grid+ContextMenu into a shared widget file

    fn view(&self) -> Element<'_, Message> {
//        let xxx = self.grid.view().map(Message::GridAction).on_press(Message::ButtonClicked);
//        let xxx = self.grid.view().onRightClick(Message::RightClick);
       
/*       
       column!( [
           xxx
       ]).into();    
*/
        let grid = Grid::new(Message::RightClick); 

        if self.contextVisible {
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
//                .width(200)
//                .align_x(Horizontal::Left)
                .into()
            })
            .into()
        }
        else {
            grid.into()
        }
    }
}

