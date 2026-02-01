
use iced::{
    Event, Point, 
    advanced::{
        Shell, layout::Layout
    }, mouse::{self}
};
use crate::{ CellMessage, cells::Cell, 
    gridArea::{
        GridAreaMessage, 
        grid::{
            CursorMode, Grid, State, gridCalcs::{findCellForCursor, moving, overlappingCell, resizing}
        }
    }
};


pub fn gridUpdate(grid: &mut Grid, state: &State, event: &Event, layout: Layout<'_>, cursor: mouse::Cursor, shell: &mut Shell<'_, GridAreaMessage>) 
{
    let translate = |x:f32,y:f32| Point {x: x - layout.position().x, y: y - layout.position().y};

    /* 
        THINK update() has to be called before the first call to draw() as self in draw is 
        immutable and cursor etc are unavailable in new().
    */

    if grid.mode == CursorMode::INIT {
        if let Some(pos) = cursor.position() {
            grid.mode = findCellForCursor(&grid,translate(pos.x,pos.y));
//                state.cache.clear();  
//                shell.request_redraw();
        }
    }

    match &event {
        Event::Mouse(mouse::Event::ButtonPressed(mouse::Button::Left)) => 
            leftPressed(grid, state, layout, cursor, shell),

        Event::Mouse(mouse::Event::CursorMoved{position}) => 
            movedCursor(grid, state, shell, &translate(position.x,position.y)),

        Event::Mouse(mouse::Event::ButtonReleased(mouse::Button::Left)) => 
            leftReleased(grid, state, shell),

        //XXX or is using ButtonPressed better?
        Event::Mouse(mouse::Event::ButtonReleased(mouse::Button::Right)) => 
            rightClick(grid, state, shell),

        _ => ()
    }
}

fn leftPressed(grid: &mut Grid, state: &State, layout: Layout<'_>, cursor: mouse::Cursor, shell: &mut Shell<'_, GridAreaMessage>) 
{
    let point = |x:f32,y:f32| Point {x: x - layout.position().x, y: y - layout.position().y};

    let s = grid.settings;
    shell.capture_event();

    match &grid.mode {
        CursorMode::MOVABLE(data) => {
            grid.mode = CursorMode::MOVING(data.clone());     //XXX implement copy instead?
            /* Required in case you click on a filled cell without moving it */
            //grid.movingCell = Some(grid.cells[grid.selectedCell.unwrap()]);
        }
        CursorMode::RESIZABLE(data) => {
            grid.mode = CursorMode::RESIZING(data.clone());
        }
        _ => {
            if let Some(pos) = cursor.position() {
                let p = point(pos.x,pos.y);

                /* Add a cell: */
                let row = (p.y / s.rowHeight).floor() as usize;
                let col = (p.x / s.colWidth).floor() as f32;

                let cell = Cell{row,col,length:1.0,velocity:100};

                /* NOTE in future if the min values are > 0 then min contraints will need to be added */
                if p.x < s.colWidth * s.numCols as f32 && p.y < s.rowHeight * s.numRows as f32 {
                    if let None = overlappingCell(grid.cells,&cell,None) {
                        state.cache.clear();  
//XXX being called when we click out of context menu                                
                        shell.publish(GridAreaMessage::Cells(CellMessage::Add(cell)));
                    }
                }
            }
        }
    }
}

fn movedCursor(grid: &mut Grid, state: &State, shell: &mut Shell<'_, GridAreaMessage>, position:&Point) 
{
    match &mut grid.mode {
        CursorMode::MOVING(_) => {
            moving(grid, *position);
            state.cache.clear();  
            shell.request_redraw();
        }
        CursorMode::RESIZING(_) => {
            resizing(grid,*position);
            state.cache.clear();  
            shell.request_redraw();
        }
        _ => {
            grid.mode = findCellForCursor(&grid,*position);
        }
    }
}

fn leftReleased(grid: &mut Grid, state: &State, shell: &mut Shell<'_, GridAreaMessage>) 
{
    match &grid.mode {
        CursorMode::MOVING(data) => {
            state.cache.clear();  

            if data.amOverlapping {
                shell.publish(GridAreaMessage::Cells(CellMessage::Modify(data.cellIndex,data.lastValid)));
            }
            else {
                shell.publish(GridAreaMessage::Cells(CellMessage::Modify(data.cellIndex,data.workingCell)));
            }
            grid.mode = CursorMode::MOVABLE(data.clone()); //XXX can clone() be avoided here?
        }
        CursorMode::RESIZING(data) => {
            state.cache.clear();  
            shell.publish(GridAreaMessage::Cells(CellMessage::Modify(data.cellIndex,data.workingCell)));
            grid.mode = CursorMode::RESIZABLE(data.clone());  //XXX can clone() be avoided here?
        }
        _ => {}
    }
}

fn rightClick(grid: &mut Grid, state: &State, shell: &mut Shell<'_, GridAreaMessage>) 
{
    shell.capture_event();

    match &grid.mode {
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

