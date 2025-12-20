use iced::mouse;
use iced::widget::canvas::{self, Canvas, Event, Geometry,Stroke,stroke,Path};
use iced::widget::{column, row, slider, text};
use iced::{
    Center, Color, Element, Fill, Point, Rectangle, Renderer, Size, Theme,
};
use rand::Rng;
use std::fmt::Debug;


fn main() -> iced::Result {
    iced::application(GridApp::default,GridApp::update,GridApp::view) .run()
}

#[derive(Debug,Clone)]
struct Cell {
    row: i32,
    col: i32
}

#[derive(Debug, Default)]
struct GridApp {
    grid: Grid
}

#[derive(Debug, Clone)]
pub enum Message {
    IterationSet(i32),
    PointAdded(Cell),
    PointRemoved,
}

impl GridApp {
    fn update(&mut self, message: Message) {
        match message {
            Message::IterationSet(cur_iter) => {
                self.grid.iteration = cur_iter;
            }
            Message::PointAdded(cell) => {
                self.grid.cells.push(cell);
                self.grid.random_points.clear();
            }
            Message::PointRemoved => {
                self.grid.cells.pop();
                self.grid.random_points.clear();
            }
        }

        self.grid.redraw();
    }

    fn view(&self) -> Element<'_, Message> {
        column![
            Canvas::new(&self.grid).width(Fill).height(Fill),
            row![
                text!("Iteration: {:?}", self.grid.iteration),
                slider(0..=10000, self.grid.iteration, Message::IterationSet)
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
    numColumns: i32,
    rowHeight: f32,
    columnWidth: f32,

    cells: Vec<Cell>,
    iteration: i32,
    random_points: Vec<Point>,
    cache: canvas::Cache,
}

impl Default for Grid {
    fn default() -> Self {
        Grid {
            numRows:8,numColumns:20,rowHeight:20.0,columnWidth:30.0,
            iteration:0,cells: Vec::default(), random_points: Vec::default(),
            cache: canvas::Cache::default()
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
        let cursor_position = cursor.position_in(bounds)?;

        match event {
            Event::Mouse(mouse::Event::ButtonPressed(button)) => match button {
                mouse::Button::Left => { 

                    println!("Position: {cursor_position:?}");

                    let row = (cursor_position.y / self.rowHeight).floor() as i32;
                    let col = (cursor_position.x / self.columnWidth).floor() as i32;

                    println!("Row: {row}  Col: {col}");


                    Some(canvas::Action::publish(
                        Message::PointAdded(Cell{row,col}),
                    ))
                },
                mouse::Button::Right => {
                    Some(canvas::Action::publish(Message::PointRemoved))
                }
                _ => None,
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

println!("In draw");        

        let geom = self.cache.draw(renderer, bounds.size(), |frame| {
println!("In cache.draw");        

            for c in self.cells.iter() {
                //TODO possibly size to be inside grid, although maybe not at start
                let point = Point::new(c.col as f32 * self.columnWidth, c.row as f32 * self.rowHeight);
                let size = Size::new(self.columnWidth, self.rowHeight);

                let path = canvas::Path::rectangle(point,size);
                frame.fill(&path, Color::from_rgb8(0x12, 0x93, 0xD8));
            };
        });



        let mut frame = canvas::Frame::new(renderer, bounds.size());

        let palette = _theme.extended_palette();

        /* Draw the app border: */
        frame.stroke(
            &canvas::Path::rectangle(Point::ORIGIN, frame.size()),
            canvas::Stroke::default(),
        );

        /* Draw the horizontal lines: */
        for i in 0..=self.numRows {
            let line = Path::line(
                Point {x: 0.0, y: i as f32 * self.rowHeight},
                Point {x: self.numColumns as f32 * self.columnWidth, y: i as f32 * self.rowHeight}
            );

            frame.stroke(&line, Stroke {
                width: 1.0,
//                style: stroke::Style::Solid(palette.secondary.strong.text),
                style: stroke::Style::Solid(palette.secondary.weak.color),
                ..Stroke::default()
            });
        }

        /* Draw the vertical lines: */
        for i in 0..=self.numColumns {
            let line = Path::line(
                Point {x: i as f32 * self.columnWidth, y: 0.0},
                Point {x: i as f32 * self.columnWidth, y: self.numRows as f32 * self.rowHeight}
            );

            frame.stroke(&line, Stroke {
                width: 1.0,
//                style: stroke::Style::Solid(palette.secondary.strong.text),
                style: stroke::Style::Solid(palette.secondary.weak.color),
                ..Stroke::default()
            });
        }

        vec![geom,frame.into_geometry()]
    }
}

impl Grid {
    fn redraw(&mut self) {
        self.cache.clear();
    }
}
