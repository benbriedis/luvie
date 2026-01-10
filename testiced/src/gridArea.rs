#![allow(non_snake_case)]

use std::{fmt::Debug};
use iced::{
    Background, Color, Element, Event, Length::{self}, Point, Rectangle, Renderer, Theme, advanced::{Clipboard, Layout, Shell, widget::Tree}, alignment::Horizontal, border::radius, mouse, widget::{
        Scrollable, column, container, mouse_area, opaque, row, scrollable::{Direction, Scrollbar}, slider, space, stack 
    }
};
use crate::{Cell, CellMessage, contextMenuPopup::ContextMenuPopup, grid::{Grid, GridMessage}};


mod grid;  //NOTE this does NOT say this file is in 'grid'. Rather it says look in 'grid.rs' for a 'grid' module.
mod contextMenuPopup;


fn main() -> iced::Result {
    iced::application(GridApp::new,GridApp::update,GridApp::view) 
    .theme(Theme::Light)
//    .theme(Theme::custom(Theme::Light, palette))
    .run()
}

pub struct GridSettings {
    numRows: usize,
    numCols: usize,
    rowHeight: f32,
    colWidth: f32,
    snap: Option<f32>,
    popupWidth: f32
}

struct GridApp {
    settings: &GridSettings,
    cells: &Vec<Cell>,
    contextVisible: bool,
    contextCellIndex: Option<usize>,
    velocity: u8
}

#[derive(Clone, Debug)]
pub enum GridAreaMessage {
    HideContextMenu,
    CloseContext,           //XXX unused?
    RightClick(usize),
    Cells(CellMessage)
}

impl GridApp {

//TODO move the combined Grid+ContextMenu into a shared widget file

    fn new(settings:&GridSettings,cells:&Vec<Cell>) -> GridApp
    {
        GridApp {settings,cells,
            contextVisible: false,
            contextCellIndex: None,
            velocity: 0
        }
    }

//    fn update(&mut self, message: GridAreaMessage) {
    fn update(
        &mut self,
        tree: &mut Tree,
        event: &Event,
        _layout: Layout<'_>,
        cursor: mouse::Cursor,
        _renderer: &Renderer,
        _clipboard: &mut dyn Clipboard,
        shell: &mut Shell<'_, GridAreaMessage>,
        _viewport: &Rectangle,
    ) {

//TODO look at event and shell. Can we get the child messages from either? Do we need to access children()?
//     eg see contextMenuPopup.rs which uses 'content'

//ALT: buy into the whole Elm thing... can have an app-level update method
        
        match message {
            GridAreaMessage::HideContextMenu => self.contextVisible = false,
//            GridAreaMessage::ValueChange(value) => self.velocity = value,  //XXX => VelocityChange
            GridAreaMessage::CloseContext => self.contextVisible = false,
            GridAreaMessage::RightClick(cellIndex) => {
                self.contextCellIndex = Some(cellIndex);
                self.contextVisible = true;
            }
            GridAreaMessage::Cells(msg) => {
                match msg {
//XXX cf mapping these messages from GridAreaMessage to CellMessage instead                    
                    CellMessage::DeleteCell => {
//                        self.cells.remove(self.contextCellIndex.unwrap());
                        self.contextCellIndex = None;
                        self.contextVisible = false;

                        shell.publish(GridAreaMessage::Cells(CellMessage::DeleteCell(cell)));
                    }
//TODO try publishing these to be handled by parent                    
                    CellMessage::AddCell(cell) => self.cells.push(cell),
                    CellMessage::ModifyCell(i,cell) => {
                        self.cells[i].col = cell.col;
                        self.cells[i].row = cell.row;
                        self.cells[i].length = cell.length;
                    }
                }
            }
        }
    }

    fn view(&self) -> Element<'_, GridAreaMessage> {
        let grid = Element::new(Grid::new(&self.settings,&self.cells)).map(GridAreaMessage::Grid);   //XXX HACK calling new and view with cells...

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
                            .on_press(GridAreaMessage::DeleteCell)
                            .into(),
                        
                        row![
                            "Velocity",
                            space::horizontal().width(Length::Fixed(8.0)),
                            slider(0..=255, self.velocity, GridAreaMessage::ValueChange)
                                .on_release(GridAreaMessage::CloseContext)
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
            context(outer,contextContent,GridAreaMessage::HideContextMenu,&self.settings,pos)
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

