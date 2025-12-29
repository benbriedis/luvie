/* TODO 
    1. Maybe add scrollbars too...

    4. Popups with delete and velocity slider, ...

    5. Read colours from themes. The loading_spinners/src/circular.rs example has a full on example
*/


use iced::{
    Element, Length, Size, Color, Point, Rectangle, Renderer, Vector, Event, Theme, Task,
    widget::{
        canvas::{self,Stroke,stroke,Path},
    },
    advanced:: {
        Clipboard,Shell,renderer,
        layout::{self, Layout},
        widget::{self,Widget,
            tree::{self, Tree},
        },
        graphics::geometry::Frame,
    },
    mouse::{self}
};

use std::fmt::Debug;
use mouse::Interaction::{Grab,Grabbing,ResizingHorizontally,NotAllowed};

#[derive(Debug,Default,Clone,Copy,PartialEq)]
struct Cell {
    row: usize,
    col: f32,       //XXX awkward name given type. Might be the best we have for the moment though
    length: f32
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

pub struct Grid<Message> {
    numRows: usize,
    numCols: usize,
    rowHeight: f32,
    colWidth: f32,
    snap: Option<f32>,

    cells: Vec<Cell>,

    mousePosition: Option<Point>,
    grabPosition: Option<Point>,
    lastPosition: Option<Cell>,

    hoverState: CursorMode,
    selectedNote: Option<usize>,
    side:Side,
    amOverlapping: bool,

    onRightClick: Message
}

#[derive(Default)]
struct State {
    cache: canvas::Cache,
}


impl<Message:Clone> Widget<Message, Theme, Renderer> for Grid<Message>
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
        theme: &Theme,
        _style: &renderer::Style,
        layout: Layout<'_>,
        _cursor: mouse::Cursor,
        _viewport: &Rectangle,    //XXX relationship with layout.bounds?
    ) {

// _style: Style { text_color: Color { r: 0.0, g: 0.0, b: 0.0, a: 1.0 } }

//XXX these extensions arent enough here. Probably just need background + text + custom colours
let exPalette = theme.extended_palette();

        let state = tree.state.downcast_ref::<State>();

        let bounds = layout.bounds();
//        let style = theme.style(&self.class, self.status.unwrap_or(Status::Active));

        let gridCache = state.cache.draw(renderer, layout.bounds().size(), |frame| {
            //XXX in theory only need to draw the horizontal lines once so long as the cells sit inside them 
            //XXX could possibly omit redrawing cells not on the last row too.
        
            /* Draw the grid horizontal lines: */
            for i in 0..=self.numRows {
                let line = Path::line(
                    Point {x: 0.0, y: i as f32 * self.rowHeight},
                    Point {x: self.numCols as f32 * self.colWidth, y: i as f32 * self.rowHeight}
                );

                frame.stroke(&line, Stroke {
                    width: 1.0,
                    style: stroke::Style::Solid(Color::from_rgb8(0x12, 0xee, 0x12)),
//                    style: stroke::Style::Solid(exPalette.primary.base.color),
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
//                    style: stroke::Style::Solid(exPalette.secondary.base.color),
                    ..Stroke::default()
                });
            }

            /* Draw cells: */
            for c in self.cells.iter() {
                self.drawCell(frame,*c);
            };

            /* Draw selected note last so it sits on top */
            if let Some(current) = self.selectedNote {
                self.drawCell(frame,self.cells[current]);
            }

            /* Draw the app border: */
/*            
            frame.stroke(
                &canvas::Path::rectangle(Point::ORIGIN, frame.size()),
                canvas::Stroke::default(),
            );
*/
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

        let mut redrawAll = || {
            state.cache.clear();  
            shell.request_redraw();
        };

        match &event {
//            Event::Window(window::Event::RedrawRequested(now)) => {
//            }
            Event::Mouse(mouse::Event::ButtonPressed(mouse::Button::Left)) => {
                match self.hoverState {
                    CursorMode::MOVABLE => {
                        self.hoverState = CursorMode::MOVING;
                    }
                    CursorMode::RESIZABLE => {
                        self.hoverState = CursorMode::RESIZING;
                    }
                    _ => {
                        if let Some(position) = cursor.position() {
                            /* Add a cell: */
                            let row = (position.y / self.rowHeight).floor() as usize;
                            let col = (position.x / self.colWidth).floor() as f32;

                            let cell = Cell{row,col,length:1.0};

                            if let None = self.overlappingCell(cell,None) {
                                self.cells.push(cell);
                                redrawAll();
                            }
                        }
                    }
                }
            }
            Event::Mouse(mouse::Event::ButtonReleased(mouse::Button::Left)) => {
                match self.hoverState {
                    CursorMode::MOVING => {
                        if self.amOverlapping {
                            self.cells[self.selectedNote.unwrap()] = self.lastPosition.unwrap();
                            redrawAll();
                        }
                        self.hoverState = CursorMode::MOVABLE;
                    }
                    CursorMode::RESIZING => {
                        self.hoverState = CursorMode::RESIZABLE;
                    }
                    _ => {}         //XXX can I use if let if let else?
                }
            }
            Event::Mouse(mouse::Event::CursorMoved{position}) => {
                match self.hoverState {
                    CursorMode::MOVING => {
                        self.moving(*position);
                        redrawAll();
                    }
                    CursorMode::RESIZING => {
                        self.resizing(*position);
                        redrawAll();
                    }
                    _ => {
                        self.mousePosition = Some(*position);
                        self.findNoteForCursor(*position);
                    }
                }
            }
            //XXX or is using ButtonPressed better?
            Event::Mouse(mouse::Event::ButtonReleased(mouse::Button::Right)) => {
println!("Got right click in grid.rs");                
//XXX ummm... cell?
//                shell.publish(self.onRightClick.clone());
                shell.publish(self.onRightClick.clone());
                shell.capture_event();
            }
            _ => ()
        }
    }

    fn mouse_interaction(&self,_tree: &Tree,_layout: Layout<'_>,_cursor: mouse::Cursor,_viewport: &Rectangle,_renderer: &Renderer) -> mouse::Interaction 
    {
        match self.hoverState {
            CursorMode::MOVABLE => Grab,
            CursorMode::MOVING => if self.amOverlapping { NotAllowed } else { Grabbing },
            CursorMode::RESIZABLE => ResizingHorizontally,
            CursorMode::RESIZING => ResizingHorizontally,
            _ => mouse::Interaction::default()
        }
    }
}

impl<'a,Message:'a + Clone> From<Grid<Message>> for Element<'a,Message> {
    fn from(grid: Grid<Message>) -> Self {
        Self::new(grid)
    }
}

impl<Message:Clone> Grid<Message> {
    pub fn new<F>(onRightClick:F) -> Self
    where
//        F: FnOnce(Cell) -> Message,
        F: FnOnce(Point) -> Message,
    {
        Self {
            numRows: 8, numCols:20, rowHeight:30.0, colWidth:40.0,snap:Some(0.25 as f32),
            cells: [].to_vec(),
            mousePosition: None,
            grabPosition: None,
            lastPosition: None,
            hoverState: CursorMode::NONE,
            selectedNote: None,
            side: Side::LEFT,
            amOverlapping: false,
//            onRightClick: onRightClick(Cell{row:1,col:1.0,length:1.0})
            onRightClick: onRightClick(Point{x:1.0,y:1.0})
        }
    }

    /*
    pub fn view(&self) -> Element<'_,Message> {
//        Grid::new().into()
        self.into()
    }
    */

    fn drawCell(&self,frame:&mut Frame<Renderer>,c: Cell)
    {
        let lineWidth = 1.0;

         //TODO possibly size to be inside grid, although maybe not at start
        let point = Point::new(c.col as f32 * self.colWidth, c.row as f32 * self.rowHeight +lineWidth);
        let size = Size::new(c.length * self.colWidth, self.rowHeight - 2.0 * lineWidth);
        let path = canvas::Path::rectangle(point,size);
        frame.fill(&path, Color::from_rgb8(0x12, 0x93, 0xD8));

        /* Add the dark line on the left: */
        let size2 = Size::new(8.0, self.rowHeight - 2.0 * lineWidth);
        let path2 = canvas::Path::rectangle(point,size2);
        frame.fill(&path2, Color::from_rgb8(0x12, 0x60, 0x90));
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
                self.selectedNote = Some(i);
                self.side = Side::LEFT;
            }
            else if rightEdge - pos.x <= resizeZone && pos.x - rightEdge <= resizeZone {
                self.hoverState = CursorMode::RESIZABLE;
                self.selectedNote = Some(i);
                self.side = Side::RIGHT;
            }
            else if pos.x >= leftEdge && pos.x <= rightEdge {
                //XXX only required on initial press down
                self.hoverState = CursorMode::MOVABLE;
                self.selectedNote = Some(i);
                self.grabPosition = Some(Point {
                    x: pos.x - n.col * self.colWidth,
                    y: pos.y - n.row as f32 * self.rowHeight
                });
                self.lastPosition = Some(Cell {row:n.row,col:n.col,length:n.length});
                /* Move takes precedence over resizing any neighbouring notes */
                return;
            }
        }
    }

    fn moving(&mut self,pos:Point)
    {
        let selected = &mut self.cells[self.selectedNote.unwrap()];

        selected.col = (pos.x - self.grabPosition.unwrap().x) / self.colWidth; 

        /* Ensure the note stays within X bounds */
        if selected.col < 0.0 {
            selected.col = 0.0;
        }
        if selected.col + selected.length > self.numCols as f32 {
            selected.col = self.numCols as f32 - selected.length;
        }

        /* Apply snap */
//FIXME For moves consider snapping on end if we picked it up closer to the end that the start (or having a mode)       
        if let Some(snap) = self.snap {
            selected.col = (selected.col / snap).round() * snap;
        }

        selected.row = ((pos.y - self.grabPosition.unwrap().y + self.rowHeight/2.0) / self.rowHeight).floor() as usize;

        /* Ensure the note stays within Y bounds */
        if selected.row < 0 {
            selected.row = 0;
        }
        if selected.row >= self.numRows {
            selected.row = self.numRows - 1;
        }

    //TODO find or implement a no-drop / not-allow / forbidden icon (circle with cross through it, or just X)
        let testCell = self.cells[self.selectedNote.unwrap()];
        self.amOverlapping = self.overlappingCell(testCell,self.selectedNote).is_some();

        if !self.amOverlapping {
            self.lastPosition = Some(testCell);
        }
    }



    /*
       NOTE the song editor will/may want 2 modes for this: probably the main one to preserve its bar alignment.
       The second one (optional) might allow it to move relative to the bar.
    */
    fn resizing(&mut self,pos:Point)
    {
    //XXX if changing grid size want the num of pixels to remain constant.
        let minLength = 10.0 / self.colWidth;

        let selected = self.cells[self.selectedNote.unwrap()];

        let mut selectedCol = selected.col;
        let mut selectedLength = selected.length;

        match self.side {
            Side::LEFT => {
                let endCol = selectedCol + selectedLength;
                selectedCol = pos.x / self.colWidth; 

                /* Apply snap: */
                if let Some(snap) = self.snap {
                    selectedCol = (selectedCol / snap).round() * snap;
                }

                let testCell = Cell{row:selected.row,col:selectedCol,length: endCol - selectedCol};
                let neighbour = self.overlappingCell(testCell,self.selectedNote);
                let min = if let Some(n) = neighbour { self.cells[n].col + self.cells[n].length } else { 0.0 };
                if selectedCol < min {
                    selectedCol = min;
                }

                selectedLength = endCol - selectedCol;
                if selectedLength < minLength {
                    selectedLength = minLength;
                    selectedCol = endCol - minLength;
                }
            }
            Side::RIGHT => {
                selectedLength = pos.x / self.colWidth - selectedCol;

                /* Apply snap: */
                let mut endCol = selectedCol + selectedLength;
                if let Some(snap) = self.snap {
                    endCol = (endCol / snap).round() * snap;
                    selectedLength = endCol - selectedCol;
                }

                let testCell = Cell{row:selected.row,col:selectedCol,length: selectedLength};
                let neighbour = self.overlappingCell(testCell,self.selectedNote);
                let max = if let Some(n) = neighbour { self.cells[n].col } else { self.numCols as f32 };
                if selectedCol + selectedLength > max {
                    selectedLength = max - selectedCol;
                }

                if selectedLength < minLength {
                    selectedLength = minLength;
                }
            }
        }

        self.cells[self.selectedNote.unwrap()].col = selectedCol;
        self.cells[self.selectedNote.unwrap()].length = selectedLength;
    }

    fn overlappingCell(&mut self,a:Cell,selected:Option<usize>) -> Option<usize>
    {
        let aStart = a.col;
        let aEnd = a.col + a.length;

        for (i,b) in self.cells.iter().enumerate() {
            if let Some(sel) = selected {
                if sel == i { continue; }
            }

            if b.row != a.row {
                continue;
            }

            let bStart = b.col;
            let bEnd = b.col + b.length;

            let firstEnd = if aStart <= bStart { aEnd } else { bEnd };
            let secondStart = if aStart <= bStart { bStart } else { aStart } ;

            if firstEnd > secondStart { //XXX HACK
                return Some(i);
            }
        }
        return None;
    }
}


