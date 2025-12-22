/* TODO 
    1. Maybe add scrollbars too...

    2. Heavy line on left of the "note"
    3. Implement move and resize
    4. Popups with delete and velocity slider, ...

    5. Old colours. Read from themes? Really theme extensions I think...

6. Convert Canvas into a custom Widget
*/


#![allow(non_snake_case)]

use iced::mouse::{self, Cursor};
use iced::widget::canvas::{self, Canvas, Event, Geometry,Stroke,stroke,Path};
use iced::widget::{column, row, slider, text};
use iced::{ Center, Color, Element, Fill, Point, Rectangle, Renderer, Size, Theme};
use std::fmt::Debug;
use mouse::Interaction::{Grab,ResizingHorizontally,NotAllowed};


fn main() -> iced::Result {
    iced::application(GridApp::default,GridApp::update,GridApp::view) .run()
}

#[derive(Debug,Clone,PartialEq)]
struct Cell {
    row: usize,
    col: f32,       //XXX awkward name given type. Might be the best we have for the moment though
    length: f32
}

#[derive(Debug, Default)]
struct GridApp {
    grid: Grid
}

impl GridApp {
    fn update(&mut self, message: Message) {
        self.grid.update(message);  //XXX nice if we could manage this within the Grid
    }

    fn view(&self) -> Element<'_, Message> {
        column![
            Canvas::new(&self.grid).width(Fill).height(Fill),
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



#[derive(Debug)]
struct Grid {
    numRows: usize,
    numCols: usize,
    rowHeight: f32,
    colWidth: f32,
    snap: f32,

    cells: Vec<Cell>,
    cache: canvas::Cache,

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

impl Default for Grid {
    fn default() -> Self {
//XXX consider add a new() function or similar so I can avoid recursion         
        Grid {
            numRows: 8, numCols:20, rowHeight:30.0, colWidth:40.0,
            cells: Vec::default(), 
            cache: canvas::Cache::default(), mousePosition: Point::default(), 
            movingGrabXOffset:0.0, movingGrabYOffset:0.0, 
            originalPosition: Cell{row:0,col:0.0,length:0.0},
            hoverState: CursorMode::NONE,selectedNote:0,side:Side::LEFT,amOverlapping:false,snap:0.25
         }
    }
}    

#[derive(Debug, Clone)]
pub enum Message {
    ButtonPressed(mouse::Button,Point),
    ButtonReleased(mouse::Button),
    MouseMoved(Point)
}

//TODO probably move this stuff back down later...
impl Grid {
    fn update(&mut self, message: Message) {

        match message {
            Message::ButtonPressed(button,position) => {
                match button {
                    mouse::Button::Left => { 
                        match self.hoverState {
                            CursorMode::MOVABLE => {
                                self.hoverState = CursorMode::MOVING;
                            }
                            _ => {
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
                    },
                    _ => ()
                }
            }
            Message::ButtonReleased(button) => { 
                //XXX might if let or if else work better here

                match button {
                    mouse::Button::Left => { 
                        match self.hoverState {
                            CursorMode::MOVING => {
                                self.hoverState = CursorMode::MOVABLE;
                            }
                            _ => {}
                        }
                    },
                    _ => {}
                }
            }
            Message::MouseMoved(position) => {
                match self.hoverState {
                    CursorMode::MOVING => {
                        self.moving(position);
                    }
                    _ => {
                        self.mousePosition = position;
                        self.findNoteForCursor(position);
                    }
                }
            }
        }

        self.redraw();
    }
}

impl canvas::Program<Message> for Grid {

    type State = ();

    fn update(
        &self,
        _state: &mut Self::State,
        event: &Event,
        bounds: Rectangle,
        cursor: mouse::Cursor,
    ) -> Option<canvas::Action<Message>> {

        match event {
            Event::Mouse(mouse::Event::ButtonPressed(button)) => {
                //let position = cursor.position_in(bounds)?;
                let position = cursor.position()?;

                Some(canvas::Action::publish(
                    Message::ButtonPressed(*button,position)
                ))
            }
            Event::Mouse(mouse::Event::ButtonReleased(button)) => 
                Some(canvas::Action::publish(
                    Message::ButtonReleased(*button)
                )),
            Event::Mouse(mouse::Event::CursorMoved { position }) => 
                Some(canvas::Action::publish(
                    Message::MouseMoved(*position)
                )),
            _ => None
        }
        .map(canvas::Action::and_capture)
    }

    fn draw(
        &self,
        _state: &Self::State,
        renderer: &Renderer,
        _theme: &Theme,
        bounds: Rectangle,
        _cursor: mouse::Cursor,
    ) -> Vec<Geometry> {

//XXX what is this approach? cache.draw with a callback?
//TODO combine with the rest?

        let gridCache = self.cache.draw(renderer, bounds.size(), |frame| {
            let palette = _theme.extended_palette();

            /* Draw the grid horizontal lines: */
            for i in 0..=self.numRows {
                let line = Path::line(
                    Point {x: 0.0, y: i as f32 * self.rowHeight},
                    Point {x: self.numCols as f32 * self.colWidth, y: i as f32 * self.rowHeight}
                );

                frame.stroke(&line, Stroke {
                    width: 1.0,
    //                style: stroke::Style::Solid(palette.secondary.strong.text),
                    style: stroke::Style::Solid(palette.secondary.weak.color),
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
    //                style: stroke::Style::Solid(palette.secondary.strong.text),
                    style: stroke::Style::Solid(palette.secondary.weak.color),
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


        /* Draw the app border: */
        //XXX could be put in its own separate cache I guess
        let mut frame = canvas::Frame::new(renderer, bounds.size());
        frame.stroke(
            &canvas::Path::rectangle(Point::ORIGIN, frame.size()),
            canvas::Stroke::default(),
        );

        vec![gridCache,frame.into_geometry()]
    }

    fn mouse_interaction(&self,_state: &Self::State,bounds: Rectangle,cursor: mouse::Cursor) 
        -> mouse::Interaction 
    {
        match self.hoverState {
            CursorMode::MOVABLE => if self.amOverlapping { NotAllowed } else { Grab },
            CursorMode::RESIZABLE => ResizingHorizontally,
            _ => mouse::Interaction::default()
        }
    }
}

#[derive(Debug,Clone)]
enum CursorMode {
    RESIZABLE,
    MOVABLE,
    MOVING,
    NONE
}

#[derive(Debug)]
enum Side {
    LEFT,
    RIGHT
}

impl Grid {
    fn redraw(&mut self) {
        self.cache.clear();
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

