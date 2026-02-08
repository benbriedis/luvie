/* TODO 
    1. Read colours from themes. The loading_spinners/src/circular.rs example has a full on example.
       Lean into composition where possible...

    MINOR BUGS:
    1. Occasionally get a question mark cursor appearing briefly
    2. Clicking a dragging a cell quickly over another can sometime leave a small gap.
*/


use iced::{
    Element, Event, Length, Point, Rectangle, Renderer, Size, Theme, Vector, 
    advanced::{
        Clipboard, Shell, layout::{self, Layout}, renderer, 
        widget::{self,Widget, tree::{self, Tree}}
    }, mouse::{self}, widget::canvas
};
use std::fmt::Debug;
use crate::{ GridSettings, cells::{Cell,Cells}, 
    gridArea::{
        GridAreaMessage, 
        grid::{
            gridDraw::drawGrid, gridUpdate::gridUpdate
        }
    }
};
use mouse::Interaction::{Grab,Grabbing,ResizingHorizontally,NotAllowed};

mod gridCalcs;
mod gridDraw;
mod gridUpdate;

#[derive(Debug,Default,Clone,PartialEq)]
enum Side {
    #[default] LEFT,
    RIGHT
}

#[derive(Debug,Default,Clone,PartialEq)]
struct ResizeData {
    cellIndex: usize,
    side: Side,
    workingCell: Cell
}

#[derive(Debug,Default,Clone,PartialEq)]
struct MoveData {
    cellIndex: usize,
    grabPosition: Point,
    lastValid: Cell,
    workingCell: Cell,
    amOverlapping: bool,
}

#[derive(Debug,Default,Clone,PartialEq)]
enum CursorMode {
    #[default] INIT,  /* ie don't know the mode yet */
    POINTER,
    RESIZABLE(ResizeData),
    RESIZING(ResizeData),
    MOVABLE(MoveData),
    MOVING(MoveData),
}

pub struct Grid<'a> {
    settings: &'a GridSettings,
    //XXX cells should really refer to the notes or patterns. Cells are the intersections of rows and cols.
    cells: &'a Cells,
    mode: CursorMode,
}

impl<'a> Grid<'a> {
    /*
        Grid is basically a view and will be called whenever state changes,
        hence the state here is not mutable.
    */
    pub fn new(settings:&'a GridSettings, cells: &'a Cells) -> Self
    {
        Self {
            settings,
            cells: cells,
            mode: CursorMode::INIT,
        }
    }
}


#[derive(Default)]
struct State {
    cache: canvas::Cache,
}


impl<'a> Widget<GridAreaMessage, Theme, Renderer> for Grid<'a>
{
    fn tag(&self) -> tree::Tag {
        tree::Tag::of::<State>()
    }

    fn state(&self) -> tree::State {
        tree::State::new(State::default())
    }

    fn size(&self) -> Size<Length> {
        Size {
            width: Length::Shrink,
            height: Length::Shrink,
        }
    } 

    fn layout(
        &mut self,
        _tree: &mut widget::Tree,
        _renderer: &Renderer,
        _limits: &layout::Limits,
    ) -> layout::Node {
        let s = self.settings;
        layout::Node::new(Size::new(s.numCols as f32 * s.colWidth, s.numRows as f32 * s.rowHeight))
    }

    fn draw(&self, tree: &Tree, renderer: &mut Renderer, theme: &Theme,
        _style: &renderer::Style, layout: Layout<'_>, cursor: mouse::Cursor, _viewport: &Rectangle
    ) 
    {
// _style: Style { text_color: Color { r: 0.0, g: 0.0, b: 0.0, a: 1.0 } }

//XXX these extensions arent enough here. Probably just need background + text + custom colours
let exPalette = theme.extended_palette();

        let state = tree.state.downcast_ref::<State>();
        let bounds = layout.bounds();
//        let style = theme.style(&self.class, self.status.unwrap_or(Status::Active));
        let grid = self;

        let gridCache = state.cache.draw(renderer, bounds.size(), |frame| {
            drawGrid(grid, frame, layout.position());

            //XXX in theory only need to draw the horizontal lines once so long as the cells sit inside them 
            //XXX could possibly omit redrawing cells not on the last row too.
        });

        use iced::advanced::Renderer as _; 
        renderer.with_translation(
            Vector::new(bounds.x, bounds.y),

            |renderer| {
                use iced::advanced::graphics::geometry::Renderer as _;  
                renderer.draw_geometry(gridCache);
            },
        );
    }

    fn update(&mut self, tree: &mut Tree, event: &Event, layout: Layout<'_>, cursor: mouse::Cursor,
        _renderer: &Renderer, _clipboard: &mut dyn Clipboard, shell: &mut Shell<'_, GridAreaMessage>, _viewport: &Rectangle,
    ) 
    {
//println!("grid - update - layout.position:{:?}",layout.position());                
        let state = tree.state.downcast_ref::<State>();
        gridUpdate(self,state,event,layout,cursor,shell) 
    }

    fn mouse_interaction(&self,_tree: &Tree,_layout: Layout<'_>,_cursor: mouse::Cursor,_viewport: &Rectangle,_renderer: &Renderer) -> mouse::Interaction 
    {
        match &self.mode {
            CursorMode::INIT => mouse::Interaction::None,
            CursorMode::MOVABLE(_) => Grab,
            CursorMode::MOVING(data) => if data.amOverlapping { NotAllowed } else { Grabbing },
            CursorMode::RESIZABLE(_) => ResizingHorizontally,
            CursorMode::RESIZING(_) => ResizingHorizontally,
            _ => mouse::Interaction::default()
        }
    }
}

impl<'a> From<Grid<'a>> for Element<'a,GridAreaMessage> {
    fn from(grid: Grid<'a>) -> Self {
        Self::new(grid)
    }
}

