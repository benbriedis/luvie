
use iced::{
    Color, Point, Renderer, Size, 
    advanced::{graphics::geometry::Frame},  
    widget::canvas::{Path, Stroke, stroke}
};
use crate::{ cells::{Cell}, gridArea::{grid::{CursorMode, Grid, }}};


pub fn drawGrid(grid: &Grid,frame:&mut Frame<Renderer>, origin:Point)
{
//XXX could the 2nd grid be displayed in the correct place but be cut out or deleted out or even be displayed too low?


//    let point = |x:f32,y:f32| Point {x: origin.x + x, y: origin.y + y};
    let point = |x:f32,y:f32| Point {x: x, y: y};  //TODO delete out

    let s = grid.settings;

    //XXX in theory only need to draw the horizontal lines once so long as the cells sit inside them 
    //XXX could possibly omit redrawing cells not on the last row too.

    /* Draw the grid horizontal lines: */
    for i in 0..=s.numRows {
        let line = Path::line(
            point(0.0, i as f32 * s.rowHeight),
            point(s.numCols as f32 * s.colWidth, i as f32 * s.rowHeight)
        );

        frame.stroke(&line, Stroke {
            width: 1.0,
            style: stroke::Style::Solid(Color::from_rgb8(0x12, 0xee, 0x12)),
//                    style: stroke::Style::Solid(exPalette.primary.base.color),
            ..Stroke::default()
        });
    }

    /* Draw the grid vertical lines: */
    for i in 0..=s.numCols {
        let line = Path::line(
            point(i as f32 * s.colWidth, 0.0),
            point(i as f32 * s.colWidth, s.numRows as f32 * s.rowHeight)
        );

        frame.stroke(&line, Stroke {
            width: 1.0,
            style: stroke::Style::Solid(Color::from_rgb8(0xee, 0x12, 0x12)),
//                    style: stroke::Style::Solid(exPalette.secondary.base.color),
            ..Stroke::default()
        });
    }

    /* Draw cells: */
    for (i,c) in grid.cells.iter().enumerate() {
        match grid.mode {
            CursorMode::MOVING(ref data) => {
                if i != data.cellIndex {
                    drawCell(grid,frame,*c);
                }
            }
            CursorMode::RESIZING(ref data) => {
                if i != data.cellIndex {
                    drawCell(grid,frame,*c);
                }
            }
            _ => drawCell(grid,frame,*c)
//FIXME remove
/*                    
            _ => {
                let pnt = point(c.col as f32 * s.colWidth, c.row as f32 * s.rowHeight +1.0);
                let size = Size::new(c.length * s.colWidth, s.rowHeight - 2.0 * 1.0);
                let path = Path::rectangle(pnt,size);
                frame.fill(&path, Color::from_rgb8(0x12, 0x93, 0xD8));
            }
*/
        }
    };

    /* Draw selected note last so it sits on top */
    match grid.mode {
        CursorMode::MOVING(ref data) => drawCell(grid,frame,data.workingCell),
        CursorMode::RESIZING(ref data) => drawCell(grid,frame,data.workingCell),
        _ => ()
    }

    /* Draw the app border: */
/*            
    frame.stroke(
        &canvas::Path::rectangle(Point::ORIGIN, frame.size()),
        canvas::Stroke::default(),
    );
*/
}

pub fn drawCell(grid: &Grid,frame:&mut Frame<Renderer>, c: Cell)
{
    let s = grid.settings;
    let lineWidth = 1.0;

     //TODO possibly size to be inside grid, although maybe not at start
    let point = Point{x: c.col as f32 * s.colWidth, y: c.row as f32 * s.rowHeight +lineWidth};
    let size = Size::new(c.length * s.colWidth, s.rowHeight - 2.0 * lineWidth);
    let path = Path::rectangle(point,size);
    frame.fill(&path, Color::from_rgb8(0x12, 0x93, 0xD8));

    /* Add the dark line on the left: */
    let size2 = Size::new(8.0, s.rowHeight - 2.0 * lineWidth);
    let path2 = Path::rectangle(point,size2);
    frame.fill(&path2, Color::from_rgb8(0x12, 0x60, 0x90));
}

