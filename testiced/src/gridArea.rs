
use std::{fmt::Debug};
use iced::{
    Background, Color, Element, Length::{self}, Point,  
    alignment::Horizontal, 
    border::radius, 
    widget::{
        Scrollable, column, container, mouse_area, opaque, row, scrollable::{Direction, Scrollbar}, slider, space, stack 
    }
};
use crate::{CellMessage, GridSettings, cells::{Cell, Cells}, gridArea::{contextMenuPopup::ContextMenuPopup, grid::Grid}};


mod grid;
mod contextMenuPopup;


#[derive(Default)]
pub struct GridAreaState {
    contextVisible: bool,
    contextCellIndex: Option<usize>,
}

pub struct GridArea<'a> {
    settings: &'a GridSettings,
    cells: &'a Cells,
}

#[derive(Clone, Copy, Debug)]
pub enum GridAreaMessage {
    Cells(CellMessage),
    HideContextMenu,
    RightClick(usize)
}

/*
    NOTE that GridArea is not a true custom widget (although I guess it is "by composition" as Iced would say). 
    It defines its own messages, update and view functions and these are called manually from the application
    in a clumsy fashion.

    Could it be converted into a proper widget? Needs to respond to messages coming
    to it from above I guess, or else capture the ones coming up... Likely to be hard.
*/

//TODO show velocity control get lighter or darker as it is being dragged

impl<'a> GridArea<'a> 
{
    /*
        GridArea is basically a view and will be called whenever state changes,
        hence the state here is not mutable.
    */
    pub fn new(settings:&'a GridSettings,cells:&'a Cells) -> Self
    {
        Self {
            settings,
            cells
        }
    }

    pub fn view(&self, state: &'a GridAreaState) -> Element<'a, GridAreaMessage> {
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

/*        
   //XXX why only 1 grid shown when the scrollbars are omitted?

        let outer = container(grid)
            .width(Length::Fill)
            .height(Length::Fill);
//        let outer = grid;
*/        

        if state.contextVisible {
            if let Some(index) = state.contextCellIndex {
                return self.displayContextMenu(state,outer.into(),index,&self.cells[index])
            }
        }
        outer.into()
    }

    fn displayContextMenu(&self,state: &'a GridAreaState,outer:Element<'a,GridAreaMessage>,index:usize,cell:&'a Cell) 
        -> Element<'a, GridAreaMessage>
    {
        let contextContent = 
            container(                
                column(vec![
                    iced::widget::button("Delete")
                        .on_press(GridAreaMessage::Cells(CellMessage::Delete(state.contextCellIndex.unwrap())))
                        .into(),
                    
                    row![
                        "Velocity",
                        space::horizontal().width(Length::Fixed(8.0)),
                        slider(0..=255, 
                            cell.velocity,
                            move |value| {
                                let mut newCell = cell.clone();
                                newCell.velocity = value;
                                GridAreaMessage::Cells(CellMessage::Modify(index,newCell))
                            })
                            .on_release(GridAreaMessage::HideContextMenu)
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

        let pos = Point{
            x: cell.col * self.settings.colWidth,
            y: cell.row as f32 * self.settings.rowHeight
        };
        context(outer,contextContent,GridAreaMessage::HideContextMenu,&self.settings,pos)
    }
}

fn context<'a, GridAreaMessage>(
    base: impl Into<Element<'a, GridAreaMessage>>,
    content: impl Into<Element<'a, GridAreaMessage>>,
    onBlur: GridAreaMessage,
    settings: &GridSettings,
    pos: Point
) -> Element<'a, GridAreaMessage>
where
    GridAreaMessage: Clone + 'a,
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

pub fn update(state:&mut GridAreaState, message: GridAreaMessage) {
println!("gridArea.rs  update() - 0");  
    match message {
        GridAreaMessage::Cells(message) => {
println!("gridArea.rs  update() - 1");  
            match message {
                CellMessage::Delete(_) =>  {
println!("gridArea.rs  update()   In Delete()");                    
                    state.contextCellIndex = None;
                    state.contextVisible = false;
                }
                _ => ()
            }
        },

        GridAreaMessage::HideContextMenu => state.contextVisible = false,

        GridAreaMessage::RightClick(cellIndex) => {
            state.contextCellIndex = Some(cellIndex);
            state.contextVisible = true;
        }
    }
}

