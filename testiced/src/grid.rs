/* TODO 
    1. Maybe add scrollbars too...

    5. Read colours from themes. The loading_spinners/src/circular.rs example has a full on example

    6. If context popup is showing and click elsewhere, dont add note
*/


use iced::{
    Element, Length, Size, Color, Point, Rectangle, Renderer, Vector, Event, Theme, 
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
use crate::Cell;

#[derive(Clone, Debug)]
pub enum GridMessage {
    AddCell(Cell),
    ModifyCell(usize,Cell),
    DeleteCell(usize),
    RightClick(Cell),
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

pub struct Grid<'a> {
    numRows: usize,
    numCols: usize,
    rowHeight: f32,
    colWidth: f32,
    snap: Option<f32>,

    cells: &'a Vec<Cell>,

    mousePosition: Option<Point>,
    grabPosition: Option<Point>,
    lastPosition: Option<Cell>,

    hoverState: CursorMode,


    selectedCell: Option<usize>,
    movingCell: Option<Cell>,   //XXX cf renaming wrt selectedCell

    side:Side,
    amOverlapping: bool,
}

impl<'a> Grid<'a> {
    pub fn new(cells: &'a Vec<Cell>) -> Self
    {
        Self {
            numRows: 8, numCols:20, rowHeight:30.0, colWidth:40.0,snap:Some(0.25 as f32),
            cells: cells,
            mousePosition: None,
            grabPosition: None,
            lastPosition: None,
            hoverState: CursorMode::NONE,
            selectedCell: None,
            movingCell: None,
            side: Side::LEFT,
            amOverlapping: false
        }
    }

    /*
    pub fn view(&self,cells: &'a Vec<Cell>) -> Element<GridMessage> 
    {
        Grid::new(cells).into()
   //    self.into()
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
                self.selectedCell = Some(i);
                self.side = Side::LEFT;
            }
            else if rightEdge - pos.x <= resizeZone && pos.x - rightEdge <= resizeZone {
                self.hoverState = CursorMode::RESIZABLE;
                self.selectedCell = Some(i);
                self.side = Side::RIGHT;
            }
            else if pos.x >= leftEdge && pos.x <= rightEdge {
                //XXX only required on initial press down
                self.hoverState = CursorMode::MOVABLE;
                self.selectedCell = Some(i);
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
        let mut cell = self.cells[self.selectedCell.unwrap()].clone();

        cell.col = (pos.x - self.grabPosition.unwrap().x) / self.colWidth; 

        /* Ensure the note stays within X bounds */
        if cell.col < 0.0 {
            cell.col = 0.0;
        }
        if cell.col + cell.length > self.numCols as f32 {
            cell.col = self.numCols as f32 - cell.length;
        }

        /* Apply snap */
//FIXME For moves consider snapping on end if we picked it up closer to the end that the start (or having a mode)       
        if let Some(snap) = self.snap {
            cell.col = (cell.col / snap).round() * snap;
        }

        cell.row = ((pos.y - self.grabPosition.unwrap().y + self.rowHeight/2.0) / self.rowHeight).floor() as usize;

        /* Ensure the note stays within Y bounds */
        if cell.row < 0 {
            cell.row = 0;
        }
        if cell.row >= self.numRows {
            cell.row = self.numRows - 1;
        }

        self.amOverlapping = self.overlappingCell(cell,self.selectedCell).is_some();

        if !self.amOverlapping {
            self.lastPosition = Some(cell);
        }

        self.movingCell = Some(cell);
    }

    /*
       NOTE the song editor will/may want 2 modes for this: probably the main one to preserve its bar alignment.
       The second one (optional) might allow it to move relative to the bar.
    */
    fn resizing(&mut self,pos:Point)
    {
    //XXX if changing grid size want the num of pixels to remain constant.
        let minLength = 10.0 / self.colWidth;

        let mut cell = self.cells[self.selectedCell.unwrap()].clone();

        match self.side {
            Side::LEFT => {
                let endCol = cell.col + cell.length;
                cell.col = pos.x / self.colWidth; 

                /* Apply snap: */
                if let Some(snap) = self.snap {
                    cell.col = (cell.col / snap).round() * snap;
                }

                let testCell = Cell{row:cell.row,col:cell.col,length: endCol - cell.col};
                let neighbour = self.overlappingCell(testCell,self.selectedCell);
                let min = if let Some(n) = neighbour { self.cells[n].col + self.cells[n].length } else { 0.0 };
                if cell.col < min {
                    cell.col = min;
                }

                cell.length = endCol - cell.col;
                if cell.length < minLength {
                    cell.length = minLength;
                    cell.col = endCol - minLength;
                }
            }
            Side::RIGHT => {
                cell.length = pos.x / self.colWidth - cell.col;

                /* Apply snap: */
                let mut endCol = cell.col + cell.length;
                if let Some(snap) = self.snap {
                    endCol = (endCol / snap).round() * snap;
                    cell.length = endCol - cell.col;
                }

                let neighbour = self.overlappingCell(cell,self.selectedCell);
                let max = if let Some(n) = neighbour { self.cells[n].col } else { self.numCols as f32 };
                if cell.col + cell.length > max {
                    cell.length = max - cell.col;
                }

                if cell.length < minLength {
                    cell.length = minLength;
                }
            }
        }
        self.movingCell = Some(cell);
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


#[derive(Default)]
struct State {
    cache: canvas::Cache,
}


impl<'a> Widget<GridMessage, Theme, Renderer> for Grid<'a>
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
            for (i,c) in self.cells.iter().enumerate() {
                if let Some(selectedCell) = self.selectedCell {
                    if i == selectedCell {
                        continue;
                    }
                }
                self.drawCell(frame,*c);
            };

            /* Draw selected note last so it sits on top */
            if let Some(cell) = self.movingCell {
                self.drawCell(frame,cell);
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
        shell: &mut Shell<'_, GridMessage>,
        _viewport: &Rectangle,
    ) {

        let state = tree.state.downcast_ref::<State>();

        match &event {
//            Event::Window(window::Event::RedrawRequested(now)) => {
//            }
            Event::Mouse(mouse::Event::ButtonPressed(mouse::Button::Left)) => {
                shell.capture_event();

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
                                state.cache.clear();  
                                shell.publish(GridMessage::AddCell(cell));
                            }
                        }
                    }
                }
            }
            Event::Mouse(mouse::Event::CursorMoved{position}) => {
                match self.hoverState {
                    CursorMode::MOVING => {
                        self.moving(*position);
                        state.cache.clear();  
                        shell.request_redraw();
                    }
                    CursorMode::RESIZING => {
                        self.resizing(*position);
                        state.cache.clear();  
                        shell.request_redraw();
                    }
                    _ => {
                        self.mousePosition = Some(*position);
                        self.findNoteForCursor(*position);
                    }
                }

            }
            Event::Mouse(mouse::Event::ButtonReleased(mouse::Button::Left)) => {
                match self.hoverState {
                    CursorMode::MOVING => {
                        self.hoverState = CursorMode::MOVABLE;
                        state.cache.clear();  

                        if self.amOverlapping {
                            shell.publish(GridMessage::ModifyCell(self.selectedCell.unwrap(),self.lastPosition.unwrap()));
                        }
                        else {
                            shell.publish(GridMessage::ModifyCell(self.selectedCell.unwrap(),self.movingCell.unwrap()));
                        }
                    }
                    CursorMode::RESIZING => {
                        self.hoverState = CursorMode::RESIZABLE;
                            state.cache.clear();  
                            shell.publish(GridMessage::ModifyCell(self.selectedCell.unwrap(),self.movingCell.unwrap()));
                    }
                    _ => {}
                }
                self.movingCell = None;
            }
            //XXX or is using ButtonPressed better?
            Event::Mouse(mouse::Event::ButtonReleased(mouse::Button::Right)) => {
                shell.capture_event();
                state.cache.clear();  
                shell.publish(GridMessage::RightClick(Cell{row:1,col:1.0,length:1.0}));  //FIXME
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

impl<'a> From<Grid<'a>> for Element<'a,GridMessage> {
    fn from(grid: Grid<'a>) -> Self {
        Self::new(grid)
    }
}

