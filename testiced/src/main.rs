/* TODO 
    1. Detect hover over move and sides.
       Maybe add scrollbars too...

    2. Heavy line on left of the "note"
    3. Implement move and resize
    4. Popups with delete and velocity slider, ...

    5. Old colours. Read from themes? Really theme extensions I think...
*/


#![allow(non_snake_case)]

use iced::mouse::{self, Cursor};
use iced::widget::canvas::{self, Canvas, Event, Geometry,Stroke,stroke,Path};
use iced::widget::{column, row, slider, text};
use iced::{
    Center, Color, Element, Fill, Point, Rectangle, Renderer, Size, Theme,
};
use std::fmt::Debug;


fn main() -> iced::Result {
    iced::application(GridApp::default,GridApp::update,GridApp::view) .run()
}

#[derive(Debug,Clone,PartialEq)]
struct Cell {
    row: i32,
    col: f32,       //XXX awkward name given type. Might be the best we have for the moment though
    length: f32
}

#[derive(Debug, Default)]
struct GridApp {
    grid: Grid
}

#[derive(Debug, Clone)]
pub enum Message {
    PointAdded(Cell),
    PointRemoved,
    MouseMoved(Point)
}

impl GridApp {
    //XXX can I move this into Grid?
    fn update(&mut self, message: Message) {
        match message {
            Message::PointAdded(cell) => {
                if !self.grid.cells.contains(&cell) {
                    self.grid.cells.push(cell);

//TODO clear the grid cache??
                }

            }
            Message::PointRemoved => {
                self.grid.cells.pop();
            }
            Message::MouseMoved(position) => {
                //self.mousePosition = Some(position);
                self.grid.mousePosition = position;
println!("Got position: {position:?}");
                self.grid.findNoteForCursor(position);
                //Task::none()
            }
        }

        self.grid.redraw();
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
    numRows: i32,
    numCols: i32,
    rowHeight: f32,
    colWidth: f32,

    cells: Vec<Cell>,
    cache: canvas::Cache,

//XXX the delineate example code has this optional. Maybe don't define if not in the area??
    mousePosition: Point,

    //TODO replace with a Point
    movingGrabXOffset: f32,
    movingGrabYOffset: f32,
    originalPosition: Cell,

    hoverState: CursorMode,
    selectedNote: i32,
    side:Side
}

impl Default for Grid {
    fn default() -> Self {
        Grid {
            numRows: 8, numCols:20, rowHeight:30.0, colWidth:40.0,
            cells: Vec::default(), 
            cache: canvas::Cache::default(), mousePosition: Point::default(), 
            movingGrabXOffset:0.0, movingGrabYOffset:0.0, 
            originalPosition: Cell{row:0,col:0.0,length:0.0},
            hoverState: CursorMode::NONE,selectedNote:0,side:Side::LEFT
         }
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
        let cursorPosition = cursor.position_in(bounds)?;

        match event {
            Event::Mouse(mouse::Event::ButtonPressed(button)) => { 
                match button {
                    mouse::Button::Left => { 

                        println!("Position: {cursorPosition:?}");

                        let row = (cursorPosition.y / self.rowHeight).floor() as i32;
                        let col = (cursorPosition.x / self.colWidth).floor() as f32;

                        println!("Row: {row}  Col: {col}");

                        Some(canvas::Action::publish(
                            Message::PointAdded(Cell{row,col,length:0.0}),
                        ))
                    },
                    mouse::Button::Right => {
                        Some(canvas::Action::publish(Message::PointRemoved))
                    },
                    _ => None
                }
            },
            Event::Mouse(mouse::Event::CursorMoved { position }) => {
//                Some(Message::MouseMoved(position))
//                Message::MouseMoved(position)

                //XXX do we need the Some? do we need the Action::publish() ?
                Some(canvas::Action::publish(Message::MouseMoved(*position)))   //XXX *?
            },
            _ => None,
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
println!("hoverState: {:?}",self.hoverState);

        match self.hoverState {
            CursorMode::MOVING => mouse::Interaction::Grab,

            CursorMode::RESIZING => mouse::Interaction::ResizingHorizontally,

            _ => mouse::Interaction::default()
        }


/*            
        if cursor.is_over(bounds) {
            mouse::Interaction::Crosshair
        } else {
            mouse::Interaction::default()
        }
*/
    }
}

#[derive(Debug)]
enum CursorMode {
    RESIZING,
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

        let row = (pos.y / self.rowHeight).floor() as i32;
        let col = (pos.x / self.colWidth).floor() as i32;

        let mut selectedIfResize:i32 = 0;

        self.hoverState = CursorMode::NONE;

        for (i,n) in self.cells.iter().enumerate() {
            if n.row != row {
                continue;
            }

            let leftEdge = n.col * self.colWidth; 
            let rightEdge = (n.col + n.length) * self.colWidth;

            if leftEdge - pos.x <= resizeZone && pos.x - leftEdge <= resizeZone {
                self.hoverState = CursorMode::RESIZING;
                self.side = Side::LEFT;
                selectedIfResize = i as i32;
            }
            else if rightEdge - pos.x <= resizeZone && pos.x - rightEdge <= resizeZone {
                self.hoverState = CursorMode::RESIZING;
                self.side = Side::RIGHT;
                selectedIfResize = i as i32;
            }
            else if pos.x >= leftEdge && pos.x <= rightEdge {
                //XXX only required on initial press down
                self.hoverState = CursorMode::MOVING;
                self.selectedNote = i as i32;
                self.movingGrabXOffset = pos.x - n.col * self.colWidth;
                self.movingGrabYOffset = pos.y - n.row as f32 * self.rowHeight;
                self.originalPosition = Cell {row:n.row,col:n.col,length:n.length};

//                window()->cursor(FL_CURSOR_HAND); 
    //			window()->cursor(FL_CURSOR_CROSS); 

//                redraw();
                /* Move takes precedence over resizing any neighbouring notes */
                
                return;
            }
        }

    //XXX can I move above now?
        if let CursorMode::RESIZING = self.hoverState {
            self.selectedNote = selectedIfResize;
//            window()->cursor(FL_CURSOR_WE); 
        }
//        else 
//            window()->cursor(FL_CURSOR_DEFAULT); 

//        redraw();
    }
}
