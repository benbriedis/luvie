use iced::widget::{button, column, text, Column};
use iced::mouse;
use iced::{Color, Rectangle, Renderer, Theme};
use iced::widget::canvas;
use iced::widget::canvas::{Stroke,Path,stroke};
use iced::{Element,Point};
use iced::{Fill, Font,Size};


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
        let myCircle = (Circle {radius:40.0}).view();

        column![
            button("+").on_press(Message::Increment),
            text(self.value),
            button("-").on_press(Message::Decrement),

            myCircle,

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

// ------------------------- Canvas test:

//XXX garbage
//#[derive(Debug, Clone, Copy)]
//enum Message2 {
//}

#[derive(Default,Debug)]
struct Circle {
    radius: f32,
}

impl Circle {
    fn view(&self) -> Element<Message> {
        canvas(Circle { radius: 50.0 }).into()
    }

    fn update(&mut self, message: Message) {}
}

impl<Message4> canvas::Program<Message4> for Circle {
    // No internal state
    type State = ();

    fn draw(&self,_state: &(),renderer:&Renderer,_theme:&Theme,bounds:Rectangle,_cursor:mouse::Cursor) -> Vec<canvas::Geometry> {
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

    /*
    fn draw(&self, bounds: Rectangle, _: iced::canvas::Cursor) -> Vec<iced::canvas::Geometry> {
        let mut frame = Frame::new(bounds.size());
        frame.stroke(
            &Path::rectangle(
                Point {
                    x: bounds.width / 10.,
                    y: bounds.height / 10.,
                },
                Size {
                    width: 4. * bounds.width / 5.,
                    height: 4. * bounds.height / 5.,
                },
            ),
            Stroke::default(),
        );
        vec![frame.into_geometry()]
    }
    */
}




pub fn main() -> iced::Result 
{
    iced::run(Counter::update, Counter::view)
    //iced::run(Circle::update, Circle::view)
}

