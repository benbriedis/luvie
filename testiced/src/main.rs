use iced::widget::{button, column, text, Column};
use iced::mouse;
use iced::{Color, Rectangle, Renderer, Theme};
use iced::widget::canvas;
use iced::Element;


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
        column![
            button("+").on_press(Message::Increment),
            text(self.value),
            button("-").on_press(Message::Decrement),
        ]
    }
}

// ------------------------- Canvas test:

//XXX garbage
#[derive(Debug, Clone, Copy)]
enum Message2 {
}

// First, we define the data we need for drawing
#[derive(Default,Debug)]
struct Circle {
    radius: f32,
}

impl Circle {
    // Finally, we simply use our `Circle` to create the `Canvas`!
    //fn view<'a, Message3: 'a>(_state: &'a State) -> Element<'a, Message3> {
    fn view(&self) -> Element<Message2> {
        canvas(Circle { radius: 50.0 }).into()
    }

    fn update(&mut self, message: Message2) {}
}

// Then, we implement the `Program` trait
impl<Message4> canvas::Program<Message4> for Circle {
    // No internal state
    type State = ();

    fn draw(
        &self,
        _state: &(),
        renderer: &Renderer,
        _theme: &Theme,
        bounds: Rectangle,
        _cursor: mouse::Cursor
    ) -> Vec<canvas::Geometry> {
        // We prepare a new `Frame`
        let mut frame = canvas::Frame::new(renderer, bounds.size());

        // We create a `Path` representing a simple circle
        let circle = canvas::Path::circle(frame.center(), self.radius);

        // And fill it with some color
        frame.fill(&circle, Color::BLACK);

        // Then, we produce the geometry
        vec![frame.into_geometry()]
    }
}

pub fn main() -> iced::Result 
{
    //iced::run(Counter::update, Counter::view)
    iced::run(Circle::update, Circle::view)
}

