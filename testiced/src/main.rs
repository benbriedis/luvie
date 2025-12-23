/* TODO 
    1. Maybe add scrollbars too...

    2. Heavy line on left of the "note"
    3. Implement move and resize
    4. Popups with delete and velocity slider, ...

    5. Old colours. Read from themes? Really theme extensions I think...

6. Cursor should show "grabbing" when being moved, and "not allowed" before drop
7. Attempt to drop on another cell show return note to where it can from originally.
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
use mouse::Interaction::{Grab,ResizingHorizontally,NotAllowed};
use iced::window;



fn main() -> iced::Result {
    iced::application(GridApp::default,GridApp::update,GridApp::view) .run()
}

#[derive(Debug,Default,Clone,PartialEq)]
struct Cell {
    row: usize,
    col: f32,       //XXX awkward name given type. Might be the best we have for the moment though
    length: f32
}

#[derive(Debug, Default)]
struct GridApp {
}

impl GridApp {
    fn update(&mut self, message: Message) {
//        self.grid.update(message);  //XXX nice if we could manage this within the Grid
    }

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
    snap: f32,

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

        
//XXX what is this approach? cache.draw with a callback?
//TODO combine with the rest?

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
//            iced::Theme::style(iced::Theme::Class,None)
                    style: stroke::Style::Solid(Color::BLACK),
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
//                    style: stroke::Style::Solid(palette.secondary.weak.color),
                    style: stroke::Style::Solid(Color::BLACK),
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
        });

        use iced::advanced::Renderer as _; 
        renderer.with_translation(
            Vector::new(bounds.x, bounds.y),

            |renderer| {
                use iced::advanced::graphics::geometry::Renderer as _;  
                renderer.draw_geometry(gridCache);
            },
        );

        /* Draw the app border: */
/*        
        frame.stroke(
            &canvas::Path::rectangle(Point::ORIGIN, frame.size()),
            canvas::Stroke::default(),
        );

        vec![gridCache,frame.into_geometry()]
*/
    }

    fn mouse_interaction(&self,_tree: &Tree,_layout: Layout<'_>,_cursor: mouse::Cursor,_viewport: &Rectangle,_renderer: &Renderer) -> mouse::Interaction 
    {
        match self.hoverState {
            CursorMode::MOVABLE => if self.amOverlapping { NotAllowed } else { Grab },
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
        if self.snap > 0.0 {
            selected.col = (selected.col / self.snap).round() * self.snap;
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
        self.amOverlapping = self.overlappingNote().is_some();

        //XXX need clear?
    }

    /* Returns the note index, or -1 */
    fn overlappingNote(&mut self) -> Option<usize>
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

