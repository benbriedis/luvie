/* TODO 
    1. Read colours from themes. The loading_spinners/src/circular.rs example has a full on example.
       Lean into composition where possible...

    MINOR BUGS:
    1. Occasionally get a question mark cursor appearing briefly
    2. Clicking a dragging a cell quickly over another can sometime leave a small gap.
*/


use iced::{
    Color, Element, Event, Length, Point, Rectangle, Renderer, Size, Theme, Vector, advanced:: {
        Clipboard, Shell, graphics::geometry::Frame, layout::{self, Layout}, renderer, widget::{self,Widget,
            tree::{self, Tree},
        }
    }, mouse::{self}, widget::canvas::{self, Path, Stroke, stroke}
};
use std::fmt::Debug;
use crate::{ CellMessage, GridSettings, cells::{Cell,Cells}, gridArea::{GridAreaMessage, grid::gridCalcs::{findCellForCursor, moving, overlappingCell, resizing}}};
use mouse::Interaction::{Grab,Grabbing,ResizingHorizontally,NotAllowed};

mod gridCalcs;

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

pub struct Grid<'a> {
    settings: &'a GridSettings,
    //XXX cells should really refer to the notes or patterns. Cells are the intersections of rows and cols.
    cells: &'a Cells,
    mode: CursorMode,
}

impl<'a> Grid<'a> {
    /*
        Grid is basically a view and will be called whenever state changes,
        hence the state here is not mutable.
    */
    pub fn new(settings:&'a GridSettings, cells: &'a Cells) -> Self
    {
        Self {
            settings,
            cells: cells,
            mode: CursorMode::INIT,
        }
    }
}


#[derive(Default)]
struct State {
    cache: canvas::Cache,
}


impl<'a> Widget<GridAreaMessage, Theme, Renderer> for Grid<'a>
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
        let s = self.settings;
//    println!("layout() self: {:?}",self);
    println!("layout() tree: {:?}",_tree);
//    println!("layout() renderer: {:?}",_renderer);
    println!("layout() limits: {:?}",_limits);
    println!("");
        layout::Node::new(Size::new(s.numCols as f32 * s.colWidth, s.numRows as f32 * s.rowHeight))
    }

    fn draw(
        &self,
        tree: &Tree,
        renderer: &mut Renderer,
        theme: &Theme,
        _style: &renderer::Style,
        layout: Layout<'_>,
        cursor: mouse::Cursor,
        _viewport: &Rectangle,    //XXX relationship with layout.bounds?
    ) {
        let s = self.settings;
// _style: Style { text_color: Color { r: 0.0, g: 0.0, b: 0.0, a: 1.0 } }

//XXX these extensions arent enough here. Probably just need background + text + custom colours
let exPalette = theme.extended_palette();

        let state = tree.state.downcast_ref::<State>();

        let bounds = layout.bounds();
//        let style = theme.style(&self.class, self.status.unwrap_or(Status::Active));

//println!("view() self: {:?}",self);
println!("view() tree: {:?}",tree);
//println!("view() renderer: {:?}",renderer);
println!("view() cursor: {:?}",cursor);

println!("view() bounds: {:?}",bounds);
println!("view() layout: {:?}",layout);
println!("view() viewport: {:?}",_viewport);
println!("");


        let gridCache = state.cache.draw(renderer, layout.bounds().size(), |frame| {
            let origin = layout.position();
println!("origin: {:?}",origin);

            let point = |x:f32,y:f32| Point{x: x, y: y};

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
            for (i,c) in self.cells.iter().enumerate() {
                match self.mode {
                    CursorMode::MOVING(ref data) => {
                        if i != data.cellIndex {
                            drawCell(self,frame,origin,*c);
                        }
                    }
                    CursorMode::RESIZING(ref data) => {
                        if i != data.cellIndex {
                            drawCell(self,frame,origin,*c);
                        }
                    }
                    _ => drawCell(self,frame,origin,*c)
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
            match self.mode {
                CursorMode::MOVING(ref data) => drawCell(self,frame,origin,data.workingCell),
                CursorMode::RESIZING(ref data) => drawCell(self,frame,origin,data.workingCell),
                _ => ()
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
        layout: Layout<'_>,
        cursor: mouse::Cursor,
        _renderer: &Renderer,
        _clipboard: &mut dyn Clipboard,
        shell: &mut Shell<'_, GridAreaMessage>,
        _viewport: &Rectangle,
    ) {
//println!("grid - update - layout.position:{:?}",layout.position());                

        let point = |x:f32,y:f32| Point {x: x - layout.position().x, y: y - layout.position().y};


        let state = tree.state.downcast_ref::<State>();
        let s = self.settings;

        /* 
            THINK update() has to be called before the first call to draw() as self in draw is 
            immutable and cursor etc are unavailable in new().
        */

        if self.mode == CursorMode::INIT {
            if let Some(pos) = cursor.position() {
                self.mode = findCellForCursor(&self,point(pos.x,pos.y));

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
                        self.mode = CursorMode::MOVING(data.clone());     //XXX implement copy instead?
                        /* Required in case you click on a filled cell without moving it */
                        //self.movingCell = Some(self.cells[self.selectedCell.unwrap()]);
                    }
                    CursorMode::RESIZABLE(data) => {
                        self.mode = CursorMode::RESIZING(data.clone());
                    }
                    _ => {
                        if let Some(pos) = cursor.position() {
                            let p = point(pos.x,pos.y);
    
println!("Clicked left mouse button p: {}",p);

                            /* Add a cell: */
                            let row = (p.y / s.rowHeight).floor() as usize;
                            let col = (p.x / s.colWidth).floor() as f32;

                            let cell = Cell{row,col,length:1.0,velocity:100};

                            /* NOTE in future if the min values are > 0 then min contraints will need to be added */
                            if p.x < s.colWidth * s.numCols as f32 && p.y < s.rowHeight * s.numRows as f32 {
                                if let None = overlappingCell(self.cells,&cell,None) {
                                    state.cache.clear();  
   //XXX being called when we click out of context menu                                
                                    shell.publish(GridAreaMessage::Cells(CellMessage::Add(cell)));
                                }
                            }
                        }
                    }
                }
            }
            Event::Mouse(mouse::Event::CursorMoved{position}) => {
                match &mut self.mode {
                    CursorMode::MOVING(_) => {
                        moving(self, *position);
                        state.cache.clear();  
                        shell.request_redraw();
                    }
                    CursorMode::RESIZING(_) => {
                        resizing(self,*position);
                        state.cache.clear();  
                        shell.request_redraw();
                    }
                    _ => {
                        self.mode = findCellForCursor(&self,*position);
                    }
                }

            }
            Event::Mouse(mouse::Event::ButtonReleased(mouse::Button::Left)) => {
                match &self.mode {
                    CursorMode::MOVING(data) => {
                        state.cache.clear();  

                        if data.amOverlapping {
                            shell.publish(GridAreaMessage::Cells(CellMessage::Modify(data.cellIndex,data.lastValid)));
                        }
                        else {
                            shell.publish(GridAreaMessage::Cells(CellMessage::Modify(data.cellIndex,data.workingCell)));
                        }
                        self.mode = CursorMode::MOVABLE(data.clone()); //XXX can clone() be avoided here?
                    }
                    CursorMode::RESIZING(data) => {
                        state.cache.clear();  
                        shell.publish(GridAreaMessage::Cells(CellMessage::Modify(data.cellIndex,data.workingCell)));
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
}

impl<'a> From<Grid<'a>> for Element<'a,GridAreaMessage> {
    fn from(grid: Grid<'a>) -> Self {
        Self::new(grid)
    }
}


fn drawCell(grid: &Grid,frame:&mut Frame<Renderer>,origin:Point, c: Cell)
{
    let point = |x:f32,y:f32| Point {x: origin.x + x, y: origin.y + y};
//        let point = |x:f32,y:f32| Point {x, y};

    let s = grid.settings;
    let lineWidth = 1.0;

     //TODO possibly size to be inside grid, although maybe not at start
    let point = point(c.col as f32 * s.colWidth, c.row as f32 * s.rowHeight +lineWidth);
    let size = Size::new(c.length * s.colWidth, s.rowHeight - 2.0 * lineWidth);
    let path = Path::rectangle(point,size);
    frame.fill(&path, Color::from_rgb8(0x12, 0x93, 0xD8));

    /* Add the dark line on the left: */
    let size2 = Size::new(8.0, s.rowHeight - 2.0 * lineWidth);
    let path2 = Path::rectangle(point,size2);
    frame.fill(&path2, Color::from_rgb8(0x12, 0x60, 0x90));
}

fn drawGrid(grid: &Grid,frame:&mut Frame<Renderer>, origin:Point)
{
    let point = |x:f32,y:f32| Point {x: origin.x + x, y: origin.y + y};

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
                    drawCell(grid,frame,origin,*c);
                }
            }
            CursorMode::RESIZING(ref data) => {
                if i != data.cellIndex {
                    drawCell(grid,frame,origin,*c);
                }
            }
            _ => drawCell(grid,frame,origin,*c)
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
        CursorMode::MOVING(ref data) => drawCell(grid,frame,origin,data.workingCell),
        CursorMode::RESIZING(ref data) => drawCell(grid,frame,origin,data.workingCell),
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
