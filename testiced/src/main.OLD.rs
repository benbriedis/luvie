use iced::widget::{button, column, text, Column};
use iced::mouse;
use iced::{Rectangle, Renderer, Theme};
use iced::widget::canvas;
use iced::widget::canvas::{Stroke, Path, stroke, Program, Canvas, Event};
use iced::{Element, Point};
use iced::{Fill, Font};


/*
#[derive(Default)]
struct Counter {
    value: i64,
}

#[derive(Debug, Clone, Copy)]
enum Message {
    Increment,
    Decrement,
}

impl Counter {
    fn update(&mut self, message: Message) {
        match message {
            Message::Increment => {
                self.value += 1;
            }
            Message::Decrement => {
                self.value -= 1;
            }
        }
    }

    fn view(&self) -> Column<Message> {
        let grid = (Grid {radius:40.0}).view();

        column![
            button("+").on_press(Message::Increment),
            text(self.value),
            button("-").on_press(Message::Decrement),

            grid,

            text("- Hello there!\n- General Kenobi!")
                .font(Font::MONOSPACE)
                .size(30) // in logical pixels
                .line_height(1.5) // relative to the size (=45px)
                .width(Fill)
                .height(Fill)
                .center()
        ]
    }
}
*/

// ------------------------- Canvas test:

#[derive(Debug, Clone, Copy)]
enum Message {
//    Click(Point)
    PointAdded(Point),
    PointRemoved,
}

#[derive(Default,Debug)]
struct Grid {
//    notes: Notes
    row: i32,
    col: i32,
}

/*
impl Grid {
    fn view(&self) -> Element<Message> 
    //fn view(&self) -> Element<'_, Message> 
    {
        canvas(Grid { row:1, col:1  }).into();
    }

    fn update(&mut self, message: Message) {
        match message {
//            Message::IterationSet(cur_iter) => {
//                self.graph.iteration = cur_iter;
//            }
            Message::PointAdded(point) => {
                self.row = 2;
                self.col = 2;
            }
            Message::PointRemoved => {
                self.row = 3;
                self.col = 3;
            }
        }

//        self.graph.redraw();
    }
}
*/

impl canvas::Program<Message> for Grid {
    // No internal state
    type State = ();


//    fn update(&self,_state: &mut Self::State)
    fn update(&self,_state: &mut Self::State,event: &Event,bounds: Rectangle,cursor: mouse::Cursor,) 
        -> Option<canvas::Action<Message>> 
    {
        let cursor_position = cursor.position_in(bounds)?;

        match event {
            Event::Mouse(mouse::Event::ButtonPressed(button)) => match button {
                mouse::Button::Left => Some(canvas::Action::publish(
                    Message::PointAdded(cursor_position),
                )),
                mouse::Button::Right => {
//                    Some(canvas::Action::publish(Message::PointRemoved))
                    Some(canvas::Action::publish(Message::PointRemoved))
                }
                _ => None,
            },
            _ => None,
        }
        .map(canvas::Action::and_capture)
    }

    fn draw(&self,_state: &(),renderer:&Renderer,_theme:&Theme,bounds:Rectangle,_cursor:mouse::Cursor)
        -> Vec<canvas::Geometry> 
    {
        let mut frame = canvas::Frame::new(renderer, bounds.size());

//        let size = Size {width:50.0, height:30.0};
//        let rect = canvas::Path::rectangle(frame.center(), size);
//XXX have a small stroke might look good
//        frame.fill(&rect, Color::BLACK);

        let palette = _theme.extended_palette();

        let numRows = 8;
        let numColumns = 20;
        let rowHeight = 20.0;
        let columnWidth = 30.0;

        /* Draw the horizontal lines: */
        for i in 0..=numRows {
            let line = Path::line(
                Point {x: 0.0,                             y: i as f32 * rowHeight},
                Point {x: numColumns as f32 * columnWidth, y: i as f32 * rowHeight}
            );

            frame.stroke(&line, Stroke {
                width: 1.0,
                style: stroke::Style::Solid(palette.secondary.strong.text),
                ..Stroke::default()
            });
        }

        /* Draw the vertical lines: */
        for i in 0..=numColumns {
            let line = Path::line(
                Point {x: i as f32 * columnWidth, y: 0.0},
                Point {x: i as f32 * columnWidth, y: numRows as f32 * rowHeight}
            );

            frame.stroke(&line, Stroke {
                width: 1.0,
                style: stroke::Style::Solid(palette.secondary.strong.text),
                ..Stroke::default()
            });
        }

        vec![frame.into_geometry()]
    }
}


pub fn main() -> iced::Result 
{
/*    
    iced::application(
        Grid::default,
        Grid::update,
        Grid::view,
    )
    .run()
*/

    iced::run(Grid::update, Grid::view)
    //iced::run(Circle::update, Circle::view)
}

