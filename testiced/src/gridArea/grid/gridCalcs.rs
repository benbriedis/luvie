
use iced::{ Point };
use crate::{ cells::{Cell,Cells}, gridArea::grid::{CursorMode, Grid, MoveData, ResizeData, Side} };
use std::ops::Sub;


pub fn findCellForCursor(grid: &Grid,pos:Point) -> CursorMode
{
    let s = grid.settings;
    let resizeZone: f32 = 8.0;

    let row = (pos.y / s.rowHeight).floor() as usize;

    let mut mode = CursorMode::POINTER;

    for (i,n) in grid.cells.iter().enumerate() {
        if n.row != row {
            continue;
        }

        let leftEdge = n.col * s.colWidth; 
        let rightEdge = (n.col + n.length) * s.colWidth;

        if leftEdge - pos.x <= resizeZone && pos.x - leftEdge <= resizeZone {
            mode = CursorMode::RESIZABLE(ResizeData {
                cellIndex: i,
                side: Side::LEFT,
                workingCell: *n
            });
        }
        else if rightEdge - pos.x <= resizeZone && pos.x - rightEdge <= resizeZone {
            mode = CursorMode::RESIZABLE(ResizeData {
                cellIndex: i,
                side: Side::RIGHT,
                workingCell: *n
            });
        }
        else if pos.x >= leftEdge && pos.x <= rightEdge {
            let grabPos = Point {
                x: pos.x - n.col * s.colWidth,
                y: pos.y - n.row as f32 * s.rowHeight
            };
            mode = CursorMode::MOVABLE(MoveData { 
                cellIndex: i,
                grabPosition: grabPos,
                lastValid: *n,
                workingCell: *n,
                amOverlapping: false
            });

            /* Move takes precedence over resizing any neighbouring notes */
            return mode;
        }
    }
    mode
}

pub fn moving(grid: &mut Grid,pos:Point)
{
    let CursorMode::MOVING(ref mut data) = grid.mode else {
        return;
    };

    let s = grid.settings;
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

    /* Ensure the note stays within Y bounds. NOTE the type requires >= 0. */
    if cell.row >= s.numRows {
        cell.row = s.numRows - 1;
    }

    data.amOverlapping = overlappingCell(grid.cells,&cell,Some(data.cellIndex)).is_some();

    if !data.amOverlapping {
        data.lastValid = cell.clone();
    }
}

/*
   NOTE the song editor will/may want 2 modes for this: probably the main one to preserve its bar alignment.
   The second one (optional) might allow it to move relative to the bar.
*/
pub fn resizing(grid: &mut Grid,pos:Point)
{
    let CursorMode::RESIZING(ref mut data) = grid.mode else {
        return;
    };

    let s = grid.settings;

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

            let testCell = Cell{length: endCol - cell.col, ..*cell};

            let neighbour = overlappingCell(grid.cells,&testCell,Some(data.cellIndex));
            let min = if let Some(n) = neighbour { grid.cells[n].col + grid.cells[n].length } else { 0.0 };
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

            let neighbour = overlappingCell(grid.cells,&cell,Some(data.cellIndex));
            let max = if let Some(n) = neighbour { grid.cells[n].col } else { s.numCols as f32 };
            if cell.col + cell.length > max {
                cell.length = max - cell.col;
            }

            if cell.length < minLength {
                cell.length = minLength;
            }
        }
    }
}

pub fn overlappingCell(cells:&Cells,a:&Cell,selected:Option<usize>) -> Option<usize>
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


