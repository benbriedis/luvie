use std::{cell::RefCell, fmt::Debug, rc::Rc};
use fltk::enums::{Cursor, Event, Color};
use fltk::{app::MouseButton, draw, prelude::*, widget::Widget, *};
use fltk::{app};
use crate::{Cell, GridSettings};

/* NOTE Fltk's use of i32 is a legacy, so I only use convert types to it at the very last moment */
#[derive(Debug,Default,Clone,PartialEq)]
struct Point {
    x: f32,
    y: f32
}

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

pub struct GridImpl<'a,F1,F2,F3>
where
    F1: Fn(Cell) + 'a,
    F2: Fn(usize,Cell) + 'a,
    F3: Fn(usize) + 'a
{
//XXX settings could be owned here
    settings: &'a GridSettings,

    //XXX cells should really refer to the notes or patterns. Cells are the intersections of rows and cols.
    cells: &'a Vec<Cell>,

    mode: CursorMode, 

    /* The Fltk widgets use 'static, so this hacky approach is probably reasonable */
//    mode: Rc<RefCell<CursorMode>>,


//XXX do we still need these Box if we have removed 'static?    
    addCell: F1,
    modifyCell: F2,
    rightClick: F3
}


impl<'a,F1,F2,F3> GridImpl<'a,F1,F2,F3>
where
    F1: Fn(Cell) + 'a,
    F2: Fn(usize,Cell),
    F3: Fn(usize)
{

//TODO to cope with the static lifetime maybe use multiple owners? eg
//        let color = Rc::new(RefCell::new(Color::Red));

    pub fn new(
        settings:&'a GridSettings,
        cells: &'a Vec<Cell>,
        addCell: F1,
        modifyCell: F2,
        rightClick: F3
    ) -> Self
    {   
//        let mode1 = Rc::new(RefCell::new(CursorMode::INIT));
//        let mode2 = Rc::clone(&mode1);

        Self {
            settings, 
            cells,
            mode: CursorMode::INIT,
            addCell,
            modifyCell,
            rightClick
        }
    }

    pub fn handleEvent(self:&mut Self,event:Event) -> bool
    {
        let s = self.settings;

        /* 
            THINK update() has to be called before the first call to draw() as self in draw is 
            immutable and cursor etc are unavailable in new().
        */

        if self.mode == CursorMode::INIT {
            self.findCellForCursor();

    //      state.cache.clear();  
            app::redraw();  //TODO check required
        }

    //TODO chop this up
        match event {
            Event::Enter => true,

            Event::Move => {
                //XXX can this here?
                self.findCellForCursor();
    println!("Got Event::Move  mode: {:?}",self.mode); 
    app::redraw();  //XXX dont do this all the time
                true
            }
            Event::Drag => {
                match self.mode {
                    //XXX call this here too?
                    //    findCellForCursor(settings,cells,mode);

                    CursorMode::MOVING(_) => {
    println!("Am moving2");                    
                        self.moving();
                        //state.cache.clear();  
                        app::redraw();  //TODO check required
                        true
                    }
                    CursorMode::RESIZING(ref mut data) => {
                        self.resizing();
                        //state.cache.clear();  
                        app::redraw();  //TODO check required
                        true
                    }
                    _ => false
                }
            }

            Event::Push => {
                if app::event_button() == MouseButton::Left as i32 {
    println!("Got Push");                
                    match &self.mode {
                        CursorMode::MOVABLE(data) => {
    println!("Got Push - was MOVABLE, setting MOVING");                
                            self.mode = CursorMode::MOVING(data.clone());
                            /* Required in case you click on a filled cell without moving it */
                            //self.movingCell = Some(self.cells[self.selectedCell.unwrap()]);
    //app::redraw();  //XXX dont do this all the time
                        }
                        CursorMode::RESIZABLE(data) => {
                            self.mode = CursorMode::RESIZING(data.clone());
                        }
                        _ => {
                            let curX = app::event_x() as f32;
                            let curY = app::event_y() as f32;

                            /* Add a cell: */
                            let row = (curY / s.rowHeight).floor() as usize;
                            let col = (curX / s.colWidth).floor() as f32;

                            let cell = Cell{row,col,length:1.0};

                            /* NOTE in future if the min values are > 0 then min contraints will need to be added */
                            if curX < s.colWidth * s.numCols as f32 && curY < s.rowHeight * s.numRows as f32 {
                                if let None = Self::overlappingCell(self.cells,&cell,None) {
                                    //state.cache.clear();  
                                    (self.addCell)(cell);
                                    app::redraw();  //TODO check required
                                }
                            }
                        }
                    };
                    true
                }
                else {
                    false
                }
            }
            Event::Released => {
    //TODO chop this up:
                if app::event_button() == MouseButton::Left as i32 {
    //TODO check left button released            
    //        Event::Mouse(mouse::Event::ButtonReleased(mouse::Button::Left)) => {
                    match &self.mode {
                        CursorMode::MOVING(data) => {
                            //state.cache.clear();  

                            if data.amOverlapping {
                               // shell.publish(GridAreaMessage::Cells(CellMessage::ModifyCell(data.cellIndex,data.lastValid)));
                                (self.modifyCell)(data.cellIndex,data.lastValid);
                            }
                            else {
                                //shell.publish(GridAreaMessage::Cells(CellMessage::ModifyCell(data.cellIndex,data.workingCell)));
                                (self.modifyCell)(data.cellIndex,data.workingCell);
                            }
                            self.mode = CursorMode::MOVABLE(data.clone()); //XXX can clone() be avoided here?
                            true
                        }
                        CursorMode::RESIZING(data) => {
                            //state.cache.clear();  
                            //shell.publish(GridAreaMessage::Cells(CellMessage::ModifyCell(data.cellIndex,data.workingCell)));
                            (self.modifyCell)(data.cellIndex,data.workingCell);
                            self.mode = CursorMode::RESIZABLE(data.clone());  //XXX can clone() be avoided here?
                            true
                        }
                        _ => false
                    }
                }
                else if app::event_button() == MouseButton::Right as i32 {
            //XXX or is using ButtonPressed better?
                    match &self.mode {
                        //XXX slightly hacky approach? Note that RESIZABLE slightly extends the clickable area. Possibly desirable.
                        //    Only using MOVABLE would slightly shrink clickable are. Undesirable.
                        CursorMode::MOVABLE(data)  => {
                            //state.cache.clear();  
                            //shell.publish(GridAreaMessage::RightClick(data.cellIndex));
                            (self.rightClick)(data.cellIndex);
                            true
                        }
                        CursorMode::RESIZABLE(data) => {
                            //state.cache.clear();  
                            //shell.publish(GridAreaMessage::RightClick(data.cellIndex));
                            (self.rightClick)(data.cellIndex);
                            true
                        }
                        _ => false
                    }
                }
                else {
                    false
                }
            }
            _ => false
        }
    }

//    fn draw(self:&Self, widget:&mut Widget,mode:&CursorMode) 
    pub fn draw(self:&Self)
    {
        let s = self.settings;

    //TODO cache this? or parts?

        //XXX in theory only need to draw the horizontal lines once so long as the cells sit inside them 
        //XXX could possibly omit redrawing cells not on the last row too.
        
        /* Draw the grid horizontal lines: */
        for i in 0..=s.numRows {
            let y = (i as f32 * s.rowHeight).round() as i32;
            let endX = (s.numCols as f32 * s.colWidth).round() as i32;
            draw::set_draw_color(Color::from_rgb(0x12, 0xee, 0x12));
            draw::draw_line(0, y, endX, y);
        }

        /* Draw the grid vertical lines: */
        for i in 0..=s.numCols {
            let x = (i as f32 * s.colWidth).round() as i32;
            let endY = (s.numRows as f32 * s.rowHeight).round() as i32;
            draw::set_draw_color(Color::from_rgb(0xee, 0x12, 0x12));
            draw::draw_line(x, 0, x, endY);
        }

        /* Draw cells: */
        for (i,c) in self.cells.iter().enumerate() {
            match self.mode {
                CursorMode::MOVING(ref data) => {
                    if i != data.cellIndex {
                        self.drawCell(*c);
                    }
                }
                CursorMode::RESIZING(ref data) => {
                    if i != data.cellIndex {
                        self.drawCell(*c);
                    }
                }
                _ => self.drawCell(*c)
            }
        };

        /* Draw selected note last so it sits on top */
        match self.mode {
            CursorMode::MOVING(ref data) => self.drawCell(data.workingCell),
            CursorMode::RESIZING(ref data) => self.drawCell(data.workingCell),
            _ => ()
        }

        // TODO maybe draw an app

    /* Set the cursor display: */
    match self.mode {
        CursorMode::POINTER => self.widget.window().unwrap().set_cursor(Cursor::Arrow),
        CursorMode::MOVABLE(ref data) => self.widget.window().unwrap().set_cursor(Cursor::Hand),
        CursorMode::MOVING(ref data) => self.widget.window().unwrap().set_cursor(Cursor::Hand),
        CursorMode::RESIZING(ref data) => self.widget.window().unwrap().set_cursor(Cursor::Arrow),
        _ => ()
    };

    }

    fn drawCell(self:&Self, c:Cell)
    {
        let s = self.settings;
        let lineWidth = 1.0;

         //TODO possibly size to be inside grid, although maybe not at start
        let x = (c.col * s.colWidth).round() as i32;
        let y = (c.row as f32 * s.rowHeight + lineWidth).round() as i32;
        let width = (c.length * s.colWidth).round() as i32;
        let height = (s.rowHeight - 2.0 * lineWidth).round() as i32;
        draw::set_draw_color(Color::from_rgb(0x12, 0x93, 0xD8));
        draw::draw_rectf(x,y,width,height);

        /* Add the dark line on the left: */
        let height = (s.rowHeight - 2.0 * lineWidth).round() as i32;
        draw::set_draw_color(Color::from_rgb(0x12, 0x60, 0x90));
        draw::draw_rectf(x,y,8,height);
    }


    //        layout::Node::new(Size::new(s.numCols as f32 * s.colWidth, s.numRows as f32 * s.rowHeight))


    fn findCellForCursor(self:&mut Self)
    {
        let s = self.settings;
        let resizeZone: f32 = 8.0;

        let curX = app::event_x() as f32;
        let curY = app::event_y() as f32;

        let row = (curY as f32 / s.rowHeight).floor() as usize;

        self.mode = CursorMode::POINTER;

        for (i,n) in self.cells.iter().enumerate() {
            if n.row != row {
                continue;
            }

            let leftEdge = n.col * s.colWidth; 
            let rightEdge = (n.col + n.length) * s.colWidth;

    //TODO consider converting to a true function now            
            if leftEdge - curX as f32 <= resizeZone && curX as f32 - leftEdge <= resizeZone {
                self.mode = CursorMode::RESIZABLE(ResizeData {
                    cellIndex: i,
                    side: Side::LEFT,
                    workingCell: Cell {row:n.row,col:n.col,length:n.length}
                });
            }
            else if rightEdge - curX as f32 <= resizeZone && curX - rightEdge <= resizeZone {
                self.mode = CursorMode::RESIZABLE(ResizeData {
                    cellIndex: i,
                    side: Side::RIGHT,
                    workingCell: Cell {row:n.row,col:n.col,length:n.length}
                });
            }
            else if curX as f32 >= leftEdge && curX as f32 <= rightEdge {
                let grabPos = Point {
                    x: curX - n.col * s.colWidth,
                    y: curY - n.row as f32 * s.rowHeight
                };
                self.mode = CursorMode::MOVABLE(MoveData { 
                    cellIndex: i,
                    grabPosition: grabPos,
                    lastValid: Cell {row:n.row,col:n.col,length:n.length},
                    workingCell: Cell {row:n.row,col:n.col,length:n.length},
                    amOverlapping: false
                });

                /* Move takes precedence over resizing any neighbouring notes */
                return;
            }
        }
    }

    pub fn moving(self:&mut Self)
    {
        let CursorMode::MOVING(ref mut data) = self.mode else {
            return;
        };

        let s = self.settings;
        let cell = &mut data.workingCell;

        let curX = app::event_x() as f32;
        let curY = app::event_y() as f32;

        cell.col = (curX - data.grabPosition.x) / s.colWidth; 

        /* Ensure the note stays within X bounds */
        if cell.col < 0.0 {
            cell.col = 0.0;
        }
        if cell.col + cell.length > s.numCols as f32 {
            cell.col = s.numCols as f32 - cell.length;
        }

        /* Apply snap */
    //FIXME For moves consider snapping on end if we picked it up closer to the end that the start (or having a mode)       
        if let Some(snap) = s.snap {
            cell.col = (cell.col / snap).round() * snap;
        }

        cell.row = ((curY - data.grabPosition.y + s.rowHeight/2.0) / s.rowHeight).floor() as usize;

        /* Ensure the note stays within Y bounds */
        if cell.row < 0 {
            cell.row = 0;
        }
        if cell.row >= s.numRows {
            cell.row = s.numRows - 1;
        }

        data.amOverlapping = Self::overlappingCell(self.cells,&cell,Some(data.cellIndex)).is_some();

        if !data.amOverlapping {
            data.lastValid = cell.clone();
        }

    //	fltk::app::set_cursor(if data.amOverlapping {Cursor::Wait} else {Cursor::Hand}); 
    //	widget.set_cursor(if data.amOverlapping {Cursor::Wait} else {Cursor::Hand}); 
        //XXX maybe just draw on cursor change? Do we have to redraw everything?
    //	redraw();
    }

    /*
       NOTE the song editor will/may want 2 modes for this: probably the main one to preserve its bar alignment.
       The second one (optional) might allow it to move relative to the bar.
    */
    fn resizing(self:&mut Self)
    {
        let CursorMode::RESIZING(ref mut data) = self.mode else {
            return;
        };

        let s = self.settings;

        let curX = app::event_x() as f32;
        let curY = app::event_y() as f32;


    //XXX if changing grid size want the num of pixels to remain constant.
        let minLength = 10.0 / s.colWidth;

        let cell = &mut data.workingCell;

        match data.side {
            Side::LEFT => {
                let endCol = cell.col + cell.length;
                cell.col = curX / s.colWidth; 

                /* Apply snap: */
                if let Some(snap) = s.snap {
                    cell.col = (cell.col / snap).round() * snap;
                }

                let testCell = Cell{row:cell.row,col:cell.col,length: endCol - cell.col};
                let neighbour = Self::overlappingCell(self.cells,&testCell,Some(data.cellIndex));
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
                cell.length = curX / s.colWidth - cell.col;

                /* Apply snap: */
                let mut endCol = cell.col + cell.length;
                if let Some(snap) = s.snap {
                    endCol = (endCol / snap).round() * snap;
                    cell.length = endCol - cell.col;
                }

                let neighbour = Self::overlappingCell(self.cells,&cell,Some(data.cellIndex));
                let max = if let Some(n) = neighbour { self.cells[n].col } else { s.numCols as f32 };
                if cell.col + cell.length > max {
                    cell.length = max - cell.col;
                }

                if cell.length < minLength {
                    cell.length = minLength;
                }
            }
        }
    }

  //  fn overlappingCell(self:&Self,a:&Cell,selected:Option<usize>) -> Option<usize>
    fn overlappingCell(cells:&Vec<Cell>,a:&Cell,selected:Option<usize>) -> Option<usize>
    {
        let aStart = a.col;
        let aEnd = a.col + a.length;

        for (i,b) in cells.iter().enumerate() {
            if let Some(sel) = selected {
                if sel == i {
                    continue; 
                }
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

/*
FIXME...

fn mouse_interaction(&self,_tree: &Tree,_layout: Layout<'_>,_cursor: mouse::Cursor,_viewport: &Rectangle,_renderer: &Renderer) -> mouse::Interaction 
{
    match &self.mode {
        CursorMode::INIT => mouse::Interaction::Help,  //XXX shouldnt actually be shown
        CursorMode::MOVABLE(_) => Grab,
        CursorMode::MOVING(data) => if data.amOverlapping { NotAllowed } else { Grabbing },
        CursorMode::RESIZABLE(_) => ResizingHorizontally,
        CursorMode::RESIZING(_) => ResizingHorizontally,
        _ => mouse::Interaction::default()
    }
}

*/

