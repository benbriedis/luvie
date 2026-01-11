use std::fmt::Debug;
use fltk::{draw, enums::{self, FrameType,*}, prelude::*, widget::Widget,*};
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

pub struct Grid {
//    settings: &'a GridSettings,

    //XXX cells should really refer to the notes or patterns. Cells are the intersections of rows and cols.
//    cells: &'a Vec<Cell>,

    mode: CursorMode,

//NEW XXX: Q: do we need this in self?
    /* Extending this: */
    widget: Widget
}


impl Grid {

//TODO to cope with the static lifetime maybe use multiple owners? eg
//        let color = Rc::new(RefCell::new(Color::Red));

    pub fn new<'a:'static>(settings:&'a GridSettings, cells: &'a Vec<Cell>) -> Self
//    pub fn new(settings:&GridSettings, cells: &Vec<Cell>) -> Self
    {   
        let width = (settings.numCols as f32 * settings.colWidth).round() as i32;
        let height = (settings.numRows as f32 * settings.rowHeight).round() as i32;

        let mut widget = Widget::new(0,0,width,height,None); //XXX can we use Box here instead?
//        let mut widget = Widget::default(); //XXX can we use Box here instead?
//        widget.set_frame(FrameType::FlatBox);  //XXX do we want anything like this?

        widget.draw(|_w| {       //XXX is w = widget, = Grid, or other?
            draw(settings,cells,CursorMode::INIT);
        });

        widget.handle(|b, ev| match ev {
            Event::Enter => {
                println!("GOT Enter");
                true
            },
            Event::Push => {
                println!("GOT Push");

                /*
                if b.value() {
                    b.set_align(Align::Left | Align::Inside);
                } else {
                    b.set_align(Align::Right | Align::Inside);
                }
                */
                app::redraw();
                true
            }
//            _ => false,
            _ => {
                println!("GOT other event ev:{ev:?}");
                false
            }
        });

//XXX do we need to store all of these?        
        Self {
//            settings,
//            cells,
            mode: CursorMode::INIT,
            widget
        }
    }

    fn findCellForCursor(&mut self,settings:&GridSettings,cells:&Vec<Cell>, pos:Point)
    {
        let s = settings;
        let resizeZone: f32 = 8.0;

        let row = (pos.y as f32 / s.rowHeight).floor() as usize;

        self.mode = CursorMode::POINTER;

        for (i,n) in cells.iter().enumerate() {
            if n.row != row {
                continue;
            }

            let leftEdge = n.col * s.colWidth; 
            let rightEdge = (n.col + n.length) * s.colWidth;

//TODO consider converting to a true function now            
            if leftEdge - pos.x as f32 <= resizeZone && pos.x as f32 - leftEdge <= resizeZone {
                self.mode = CursorMode::RESIZABLE(ResizeData {
                    cellIndex: i,
                    side: Side::LEFT,
                    workingCell: Cell {row:n.row,col:n.col,length:n.length}
                });
            }
            else if rightEdge - pos.x as f32 <= resizeZone && pos.x - rightEdge <= resizeZone {
                self.mode = CursorMode::RESIZABLE(ResizeData {
                    cellIndex: i,
                    side: Side::RIGHT,
                    workingCell: Cell {row:n.row,col:n.col,length:n.length}
                });
            }
            else if pos.x as f32 >= leftEdge && pos.x as f32 <= rightEdge {
                let grabPos = Point {
                    x: pos.x - n.col * s.colWidth,
                    y: pos.y - n.row as f32 * s.rowHeight
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

    pub fn moving(&mut self,settings:&GridSettings,cells:&Vec<Cell>,pos:Point)
    {
        let CursorMode::MOVING(ref mut data) = self.mode else {
            return;
        };

        let s = settings;
        let cell = &mut data.workingCell;

        cell.col = (pos.x - data.grabPosition.x) / s.colWidth; 

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

        cell.row = ((pos.y - data.grabPosition.y + s.rowHeight/2.0) / s.rowHeight).floor() as usize;

        /* Ensure the note stays within Y bounds */
        if cell.row < 0 {
            cell.row = 0;
        }
        if cell.row >= s.numRows {
            cell.row = s.numRows - 1;
        }

        data.amOverlapping = overlappingCell(cells,&cell,Some(data.cellIndex)).is_some();

        if !data.amOverlapping {
            data.lastValid = cell.clone();
        }
    }

    /*
       NOTE the song editor will/may want 2 modes for this: probably the main one to preserve its bar alignment.
       The second one (optional) might allow it to move relative to the bar.
    */
    fn resizing(&mut self,settings:&GridSettings,cells:&Vec<Cell>,pos:Point)
    {
        let CursorMode::RESIZING(ref mut data) = self.mode else {
            return;
        };

        let s = settings;

    //XXX if changing grid size want the num of pixels to remain constant.
        let minLength = 10.0 / s.colWidth;

        let cell = &mut data.workingCell;

        match data.side {
            Side::LEFT => {
                let endCol = cell.col + cell.length;
                cell.col = pos.x / s.colWidth; 

                /* Apply snap: */
                if let Some(snap) = s.snap {
                    cell.col = (cell.col / snap).round() * snap;
                }

                let testCell = Cell{row:cell.row,col:cell.col,length: endCol - cell.col};
                let neighbour = overlappingCell(cells,&testCell,Some(data.cellIndex));
                let min = if let Some(n) = neighbour { cells[n].col + cells[n].length } else { 0.0 };
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
                cell.length = pos.x / s.colWidth - cell.col;

                /* Apply snap: */
                let mut endCol = cell.col + cell.length;
                if let Some(snap) = s.snap {
                    endCol = (endCol / snap).round() * snap;
                    cell.length = endCol - cell.col;
                }

                let neighbour = overlappingCell(cells,&cell,Some(data.cellIndex));
                let max = if let Some(n) = neighbour { cells[n].col } else { s.numCols as f32 };
                if cell.col + cell.length > max {
                    cell.length = max - cell.col;
                }

                if cell.length < minLength {
                    cell.length = minLength;
                }
            }
        }
    }
}    

fltk::widget_extends!(Grid, Widget, widget); //XXX better here or at bottom?


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

fn draw(settings:&GridSettings,cells:&Vec<Cell>,mode:CursorMode) 
{
    let s = settings;

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
    for (i,c) in cells.iter().enumerate() {
        match mode {
            CursorMode::MOVING(ref data) => {
                if i != data.cellIndex {
                    drawCell(settings,*c);
                }
            }
            CursorMode::RESIZING(ref data) => {
                if i != data.cellIndex {
                    drawCell(settings,*c);
                }
            }
            _ => drawCell(settings,*c)
        }
    };

    /* Draw selected note last so it sits on top */
    match mode {
        CursorMode::MOVING(ref data) => drawCell(settings,data.workingCell),
        CursorMode::RESIZING(ref data) => drawCell(settings,data.workingCell),
        _ => ()
    }

    // TODO maybe draw an app
}

fn drawCell(settings:&GridSettings,c:Cell)
{
    let s = settings;
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

/*
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
    let state = tree.state.downcast_ref::<State>();
    let s = self.settings;

    / * 
        THINK update() has to be called before the first call to draw() as self in draw is 
        immutable and cursor etc are unavailable in new().
    * /

    if self.mode == CursorMode::INIT {
        if let Some(pos) = cursor.position() {
            self.findCellForCursor(pos);

//                state.cache.clear();  
//                shell.request_redraw();
        }
    }

    match &event {
//            Event::Window(window::Event::RedrawRequested(now)) => {
//            }
        Event::Mouse(mouse::Event::ButtonPressed(mouse::Button::Left)) => {
            shell.capture_event();

            match &self.mode {
                CursorMode::MOVABLE(data) => {
                    self.mode = CursorMode::MOVING(data.clone());
                    / * Required in case you click on a filled cell without moving it * /
                    //self.movingCell = Some(self.cells[self.selectedCell.unwrap()]);
                }
                CursorMode::RESIZABLE(data) => {
                    self.mode = CursorMode::RESIZING(data.clone());
                }
                _ => {
                    if let Some(position) = cursor.position() {
                        / * Add a cell: * /
                        let row = (position.y / s.rowHeight).floor() as usize;
                        let col = (position.x / s.colWidth).floor() as f32;

                        let cell = Cell{row,col,length:1.0};

                        / * NOTE in future if the min values are > 0 then min contraints will need to be added * /
                        if position.x < s.colWidth * s.numCols as f32 && position.y < s.rowHeight * s.numRows as f32 {
                            if let None = overlappingCell(self.cells,&cell,None) {
                                state.cache.clear();  
//XXX being called when we click out of context menu                                
                                shell.publish(GridAreaMessage::Cells(CellMessage::AddCell(cell)));
                            }
                        }
                    }
                }
            }
        }
        Event::Mouse(mouse::Event::CursorMoved{position}) => {
            match &mut self.mode {
                CursorMode::MOVING(_) => {
                    self.moving(*position);
                    state.cache.clear();  
                    shell.request_redraw();
                }
                CursorMode::RESIZING(ref mut data) => {
                    self.resizing(*position);
                    state.cache.clear();  
                    shell.request_redraw();
                }
                _ => {
                    self.findCellForCursor(*position);
                }
            }

        }
        Event::Mouse(mouse::Event::ButtonReleased(mouse::Button::Left)) => {
            match &self.mode {
                CursorMode::MOVING(data) => {
                    state.cache.clear();  

                    if data.amOverlapping {
                        shell.publish(GridAreaMessage::Cells(CellMessage::ModifyCell(data.cellIndex,data.lastValid)));
//                            shell.publish(CellMessage::ModifyCell(data.cellIndex,data.lastValid));
                    }
                    else {
                        shell.publish(GridAreaMessage::Cells(CellMessage::ModifyCell(data.cellIndex,data.workingCell)));
                    }
                    self.mode = CursorMode::MOVABLE(data.clone()); //XXX can clone() be avoided here?
                }
                CursorMode::RESIZING(data) => {
                    state.cache.clear();  
                    shell.publish(GridAreaMessage::Cells(CellMessage::ModifyCell(data.cellIndex,data.workingCell)));
                    self.mode = CursorMode::RESIZABLE(data.clone());  //XXX can clone() be avoided here?
                }
                _ => {}
            }
        }
        //XXX or is using ButtonPressed better?
        Event::Mouse(mouse::Event::ButtonReleased(mouse::Button::Right)) => {
            shell.capture_event();

            match &self.mode {
                //XXX slightly hacky approach? Note that RESIZABLE slightly extends the clickable area. Possibly desirable.
                //    Only using MOVABLE would slightly shrink clickable are. Undesirable.
                CursorMode::MOVABLE(data)  => {
                    state.cache.clear();  
                    shell.publish(GridAreaMessage::RightClick(data.cellIndex));
                }
                CursorMode::RESIZABLE(data) => {
                    state.cache.clear();  
                    shell.publish(GridAreaMessage::RightClick(data.cellIndex));
                }
                _ => {}
            }
        }
        _ => ()
    }
}
*/

/*
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

