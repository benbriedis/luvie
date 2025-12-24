/* TODO 
    1. Maybe add scrollbars too...

    2. Heavy line on left of the "note"
    3. Implement move and resize
    4. Popups with delete and velocity slider, ...

    5. Old colours. Read from themes? Really theme extensions I think...

8. cf snap... maybe is better showing it "live"?
*/


#![allow(non_snake_case)]

use iced::mouse::{self};
use iced::widget::{column, row};
use iced::widget::canvas::{self,Stroke,stroke,Path};
use iced::{ Center, Color, Element, Point, Rectangle, Renderer, Size, Length,Vector, Event};
use iced::advanced::renderer;
use iced::advanced::widget::{self,Widget};
use iced::advanced::layout::{self, Layout};
use iced::advanced::{Clipboard,Shell};
use iced::advanced::widget::tree::{self, Tree};
use std::fmt::Debug;
use mouse::Interaction::{Grab,Grabbing,ResizingHorizontally,NotAllowed};
use iced::window;



fn main() -> iced::Result {
    iced::application(GridApp::default,GridApp::update,GridApp::view) .run()
}

#[derive(Debug,Default,Clone,Copy,PartialEq)]
struct Cell {
    row: usize,
    col: f32,       //XXX awkward name given type. Might be the best we have for the moment though
    length: f32
}

#[derive(Debug, Default)]
struct GridApp {
}

impl GridApp {
    fn update(&mut self, message: Message) { }

    fn view(&self) -> Element<'_, Message> {
        column![
            Grid::new(),
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



#[derive(Debug,Default)]
struct Grid {
    numRows: usize,
    numCols: usize,
    rowHeight: f32,
    colWidth: f32,
    snap: Option<f32>,

    cells: Vec<Cell>,

//XXX the delineate example code has this optional. Maybe don't define if not in the area??
    mousePosition: Point,

    //TODO replace with a Point
    movingGrabXOffset: f32,
    movingGrabYOffset: f32,
    originalPosition: Cell,

    hoverState: CursorMode,
    selectedNote: usize,
    side:Side,
    amOverlapping: bool
}

#[derive(Debug, Clone)]
pub enum Message {
    ButtonPressed(mouse::Button,Point),
    ButtonReleased(mouse::Button),
    MouseMoved(Point)
}

#[derive(Default)]
struct State {
    cache: canvas::Cache,
}


//XXX do we need all these generic types?
impl<Message, Theme > Widget<Message, Theme, Renderer> for Grid
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
        layout::Node::new(Size::new(self.numCols as f32 * self.colWidth, self.numRows as f32 * self.rowHeight))
    }

    fn draw(
        &self,
        tree: &Tree,
        renderer: &mut Renderer,
        _theme: &Theme,
        _style: &renderer::Style,
        layout: Layout<'_>,
        _cursor: mouse::Cursor,
        _viewport: &Rectangle,    //XXX relationship with layout.bounds?
    ) {
        let state = tree.state.downcast_ref::<State>();

        let bounds = layout.bounds();
//        let style = theme.style(&self.class, self.status.unwrap_or(Status::Active));

        let gridCache = state.cache.draw(renderer, layout.bounds().size(), |frame| {
            /* Draw the grid horizontal lines: */
            for i in 0..=self.numRows {
                let line = Path::line(
                    Point {x: 0.0, y: i as f32 * self.rowHeight},
                    Point {x: self.numCols as f32 * self.colWidth, y: i as f32 * self.rowHeight}
                );

                frame.stroke(&line, Stroke {
                    width: 1.0,
//                    style: stroke::Style::Solid(palette.secondary.weak.color),
                    style: stroke::Style::Solid(Color::from_rgb8(0x12, 0xee, 0x12)),
                    ..Stroke::default()
                });
            }

            /* Draw the grid vertical lines: */
            for i in 0..=self.numCols {
                let line = Path::line(
                    Point {x: i as f32 * self.colWidth, y: 0.0},
                    Point {x: i as f32 * self.colWidth, y: self.numRows as f32 * self.rowHeight}
                );

                frame.stroke(&line, Stroke {
                    width: 1.0,
                    style: stroke::Style::Solid(Color::from_rgb8(0xee, 0x12, 0x12)),
                    ..Stroke::default()
                });
            }

            /* Draw cells: */
            for c in self.cells.iter() {
                //TODO possibly size to be inside grid, although maybe not at start
                let point = Point::new(c.col as f32 * self.colWidth, c.row as f32 * self.rowHeight);
                let size = Size::new(self.colWidth, self.rowHeight);

                let path = canvas::Path::rectangle(point,size);
                frame.fill(&path, Color::from_rgb8(0x12, 0x93, 0xD8));
            };

            /* Draw the app border: */
            frame.stroke(
                &canvas::Path::rectangle(Point::ORIGIN, frame.size()),
                canvas::Stroke::default(),
            );
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

    fn mouse_interaction(&self,_tree: &Tree,_layout: Layout<'_>,_cursor: mouse::Cursor,_viewport: &Rectangle,_renderer: &Renderer) -> mouse::Interaction 
    {
        match self.hoverState {
            CursorMode::MOVABLE => Grab,
            CursorMode::MOVING => if self.amOverlapping { NotAllowed } else { Grabbing },
            CursorMode::RESIZABLE => ResizingHorizontally,
            _ => mouse::Interaction::default()
        }
    }

    fn update(
        &mut self,
        tree: &mut Tree,
        event: &Event,
        _layout: Layout<'_>,
        cursor: mouse::Cursor,
        _renderer: &Renderer,
        _clipboard: &mut dyn Clipboard,
        shell: &mut Shell<'_, Message>,
        _viewport: &Rectangle,
    ) {

        let state = tree.state.downcast_ref::<State>();

        match &event {

            Event::Window(window::Event::RedrawRequested(now)) => {
                state.cache.clear();
                shell.request_redraw();
            }

            Event::Mouse(mouse::Event::ButtonPressed(mouse::Button::Left)) => {
                match self.hoverState {
                    CursorMode::MOVABLE => {
                        self.hoverState = CursorMode::MOVING;
                    }
                    _ => {
                        if let Some(position) = cursor.position() {
                            /* Add a cell: */
                            let row = (position.y / self.rowHeight).floor() as usize;
                            let col = (position.x / self.colWidth).floor() as f32;

                            let cell = Cell{row,col,length:1.0};

                            if !self.cells.contains(&cell) {
                                self.cells.push(cell);
            //TODO clear the grid cache??
                            }
                        }
                    }
                }
            }
            Event::Mouse(mouse::Event::ButtonReleased(mouse::Button::Left)) => {
                match self.hoverState {
                    CursorMode::MOVING => {
                        if self.amOverlapping {
                            self.cells[self.selectedNote] = self.originalPosition;
                        }
                        self.hoverState = CursorMode::MOVABLE;
                    }
                    _ => {}         //XXX can I use if let if let else?
                }
            }
            Event::Mouse(mouse::Event::CursorMoved{position}) => {
                match self.hoverState {
                    CursorMode::MOVING => {
                        self.moving(*position);
                    }
                    CursorMode::RESIZING => {
                        self.resizing(*position);
                    }
                    _ => {
                        self.mousePosition = *position;
                        self.findNoteForCursor(*position);
                    }
                }
            }
            _ => ()
        }
    }
}

impl<Message> From<Grid> for Element<'_, Message> {
    fn from(grid: Grid) -> Self {
        Self::new(grid)
    }
}


#[derive(Debug,Default,Clone)]
enum CursorMode {
    RESIZABLE,
    RESIZING,
    MOVABLE,
    MOVING,
    #[default] NONE
}

#[derive(Debug,Default)]
enum Side {
    #[default] LEFT,
    RIGHT
}

impl Grid {
    fn new() -> Self
    {
        Self {
            numRows: 8, numCols:20, rowHeight:30.0, colWidth:40.0,snap:0.25,
            ..Default::default()
        }
    }

    fn findNoteForCursor(&mut self,pos:Point)
    {
        let resizeZone: f32 = 5.0;

        let row = (pos.y / self.rowHeight).floor() as usize;

        self.hoverState = CursorMode::NONE;

        for (i,n) in self.cells.iter().enumerate() {
            if n.row != row {
                continue;
            }

            let leftEdge = n.col * self.colWidth; 
            let rightEdge = (n.col + n.length) * self.colWidth;

            if leftEdge - pos.x <= resizeZone && pos.x - leftEdge <= resizeZone {
                self.hoverState = CursorMode::RESIZABLE;
                self.selectedNote = i;
                self.side = Side::LEFT;
            }
            else if rightEdge - pos.x <= resizeZone && pos.x - rightEdge <= resizeZone {
                self.hoverState = CursorMode::RESIZABLE;
                self.selectedNote = i;
                self.side = Side::RIGHT;
            }
            else if pos.x >= leftEdge && pos.x <= rightEdge {
                //XXX only required on initial press down
                self.hoverState = CursorMode::MOVABLE;
                self.selectedNote = i;
                self.movingGrabXOffset = pos.x - n.col * self.colWidth;
                self.movingGrabYOffset = pos.y - n.row as f32 * self.rowHeight;
                self.originalPosition = Cell {row:n.row,col:n.col,length:n.length};
                /* Move takes precedence over resizing any neighbouring notes */
                return;
            }
        }
    }

    fn moving(&mut self,pos:Point)
    {
        let selected = &mut self.cells[self.selectedNote];

        selected.col = (pos.x - self.movingGrabXOffset) / self.colWidth; 

        /* Ensure the note stays within X bounds */
        if selected.col < 0.0 {
            selected.col = 0.0;
        }
        if selected.col + selected.length > self.numCols as f32 {
            selected.col = self.numCols as f32 - selected.length;
        }

        /* Apply snap */
//FIXME snap on release...  For moves consider snapping on end if we picked it up closer to the end that the start (or having a mode)       
        if let Some(snap) = self.snap {
            selected.col = (selected.col / snap).round() * snap;
        }

//XXX its probably floor, but maybe round...
        selected.row = ((pos.y - self.movingGrabYOffset + self.rowHeight/2.0) / self.rowHeight).floor() as usize;

        /* Ensure the note stays within Y bounds */
        if selected.row < 0 {
            selected.row = 0;
        }
        if selected.row >= self.numRows {
            selected.row = self.numRows - 1;
        }

    //TODO find or implement a no-drop / not-allow / forbidden icon (circle with cross through it, or just X)
        self.amOverlapping = self.overlappingCell().is_some();

        //XXX need clear?
    }



    /*
       NOTE the song editor will/may want 2 modes for this: probably the main one to preserve its bar alignment.
       The second one (optional) might allow it to move relative to the bar.
    */
    fn resizing(&mut self,pos:Point)
    {
    //XXX if changing grid size want the num of pixels to remain constant.
        let minLength = 10.0 / self.colWidth;

//let neighbour = self.overlappingCell();
        let selected = &mut self.cells[self.selectedNote];

        let x = pos.x;

    //TODO restrict length to a certain minimum				
        match self.side {
            Side::LEFT => {
                let endCol = selected.col + selected.length;
                selected.col = x / self.colWidth; 

                /* Apply snap: */
                if let Some(snap) = self.snap {
                    selected.col = ((selected.col / snap) * snap).round();
                }

                let neighbour = self.overlappingCell();
                let min = if let Some(n) = neighbour { self.cells[n].col + self.cells[n].length } else { 0.0 };
                if selected.col < min {
                    selected.col = min;
                }

                selected.length = endCol - selected.col;

                if selected.length < minLength {
                    selected.length = minLength;
                    selected.col = endCol - minLength;
                }

//                redraw();
            }
            Side::RIGHT => {
                selected.length = x / self.colWidth - selected.col;

                /* Apply snap: */
                let mut endCol = selected.col + selected.length;
                if let Some(snap) = self.snap {
                    endCol = ((endCol / snap) * snap).round();
                    selected.length = endCol - selected.col;
                }

                let neighbour = self.overlappingCell();
                let max = if let Some(n) = neighbour { self.cells[n].col } else { self.numCols as f32 };
                if selected.col + selected.length > max {
                    selected.length = max - selected.col;
                }

                if selected.length < minLength {
                    selected.length = minLength;
                }

//                redraw();
            }
        }
        //Right side to change duration...
    }

    fn overlappingCell(&mut self) -> Option<usize>
    {
        let a = &self.cells[self.selectedNote];
        let aStart = a.col;
        let aEnd = a.col + a.length;

        for (i,b) in self.cells.iter().enumerate() {
            if i == self.selectedNote || b.row != a.row {
                continue;
            }

            let bStart = b.col;
            let bEnd = b.col + b.length;

            let firstEnd = if aStart <= bStart { aEnd } else { bEnd };
            let secondStart = if aStart <= bStart { bStart } else { aStart };

            if firstEnd > secondStart {
                return Some(i);
            }
        }
        return None;
    }
}

