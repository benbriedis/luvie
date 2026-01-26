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
use crate::{Cell, CellMessage, GridSettings, Message, gridArea::{contextMenuPopup::ContextMenuPopup, grid::Grid}};


mod grid;
mod contextMenuPopup;


//TODO consider recombining this with GridArea later if possible
#[derive(Default)]
pub struct GridAreaState {
    contextVisible: bool,
    contextCellIndex: Option<usize>,
}

pub struct GridArea<'a> {
//XXX do we really need both of these?    
    settings: &'a GridSettings,
    cells: &'a Vec<Cell>,
}

#[derive(Clone, Debug)]
pub enum GridAreaMessage {
    HideContextMenu,
    DeleteCell,
    RightClick(usize),
    CloseContext 
}

/*
    NOTE that GridArea is not a widget, but it is widget-like. It defines its own 
    messages, update and view functions and these are called manually from the application
    in a slightly clumsy fashion.

    Could it be converted into a proper widget? Needs to respond to messages coming
    to it from above I guess, or else capture the ones coming up... Likely to be hard.
*/


impl<'a> GridArea<'a> 
{
    /*
        GridArea is basically a view and will be called whenever state changes,
        hence the state here is not mutable.
    */
    pub fn new(settings:&'a GridSettings,cells:&'a Vec<Cell>) -> Self
    {
        Self {
            settings,
            cells
        }
    }

    pub fn view(&self, state: &'a GridAreaState) -> Element<'a, Message> {
        let grid = Element::new(Grid::new(self.settings,self.cells));

        let outer = Scrollable::with_direction(
            container(grid),
            Direction::Both {
                vertical: Scrollbar::new(),
                horizontal: Scrollbar::new(),
            },
        )
        .width(Length::Fill)
        .height(Length::Fill);

        if state.contextVisible {
            let index = state.contextCellIndex.unwrap();
            let cell = self.cells[index];

            let contextContent = 
                container(                
                    column(vec![
                        iced::widget::button("Delete")
                            .on_press(Message::Cells(CellMessage::Delete(state.contextCellIndex.unwrap())))
                            .into(),
                        
                        row![
                            "Velocity",
                            space::horizontal().width(Length::Fixed(8.0)),
                            slider(0..=255, 
                                cell.velocity, 
                                move |value| {
                                    let mut newCell = cell;
                                    newCell.velocity = value;
                                    Message::Cells(CellMessage::Modify(index,newCell))
                                })
                                .on_release(Message::GridArea(GridAreaMessage::CloseContext))
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

            let cell = self.cells[state.contextCellIndex.unwrap()];
            let pos = Point{
                x: cell.col * self.settings.colWidth,
                y: cell.row as f32 * self.settings.rowHeight
            };
            context(outer,contextContent,Message::GridArea(GridAreaMessage::HideContextMenu),&self.settings,pos)
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
    onBlur: Message,
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
            .on_press(onBlur)
        )
    ]
    .into()
}

pub fn update(state:&mut GridAreaState, message: Message) {
    match message {
        Message::Cells(message) => {
            match message {
                CellMessage::Delete(_) =>  {
                    state.contextCellIndex = None;
                    state.contextVisible = false;
                }
                _ => ()
            }
        },

        Message::GridArea(message) => {
            match message {
                GridAreaMessage::HideContextMenu => state.contextVisible = false,
                GridAreaMessage::CloseContext => state.contextVisible = false,

                GridAreaMessage::RightClick(cellIndex) => {
                    state.contextCellIndex = Some(cellIndex);
                    state.contextVisible = true;
                }
                _ => ()
            }
        }
    }
}

