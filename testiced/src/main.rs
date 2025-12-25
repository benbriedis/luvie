#![allow(non_snake_case)]

use iced::widget::{column, row};
use iced::{ Center, Element, Theme };
use std::fmt::Debug;

mod grid;  //NOTE this does NOT say this file is in 'grid'. Rather it says look in 'grid.rs' for a 'grid' module.


fn main() -> iced::Result {
    iced::application(GridApp::default,GridApp::update,GridApp::view) 
    .theme(Theme::Light)
//    .theme(Theme::custom(Theme::Light, palette))
    .run()
}

pub enum Message { }

#[derive(Debug, Default)]
struct GridApp {
}

impl GridApp {
    fn update(&mut self, message: Message) { }

    fn view(&self) -> Element<'_, Message> {
        column![
            grid::Grid::new(),
//            .width(Fill).height(Fill),

            row![
//                slider(0..=10000, self.grid.iteration, Message::IterationSet)
            ]
            .padding(10)
            .spacing(20),
        ]
        .align_x(Center)
        .into()
    }
}

