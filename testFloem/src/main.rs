
use floem::event::{Event, EventPropagation};
use floem::{ViewId, prelude::*};
use floem::taffy::{AlignItems, FlexDirection,Display};
use floem::views::dropdown::Dropdown;
use peniko::color::{AlphaColor, Hsl};
use peniko::{Brush, Gradient, Mix};
use peniko::color::{palette::css};
use std::sync::atomic::{AtomicU32, Ordering};
use peniko::kurbo::{Affine, Rect, Shape, Size, Line, Point, Circle,Stroke};
use floem::reactive::create_updater;

use floem::{
    context::PaintCx,
};


fn main() {
    floem::launch(counter_view);
}

fn counter_view() -> impl IntoView {
    let mut counter1 = RwSignal::new(0);
    let text = RwSignal::new("Hello, World!".to_string());
    let base_note = RwSignal::new("B");
    let chord = RwSignal::new("major triad");


    let counter_element = 
        h_stack((
            button("Increment").action(move || counter1 += 1),
            label(move || format!("Value: {counter1}")),
            button("Decrement").action(move || counter1 -= 1),
        ))
        .style(|s| s.size_full().items_center().justify_center().gap(10));

    let text_element = 
        h_stack((
            label(move || text.get()),
            button("Change Text").action(move || text.set("Hello, Floem!".to_string()))
        ))
        .style(|s| s.size_full().items_center().justify_center().gap(10));

    let baseNotePane = 
        h_stack((
            label(|| "Note"),
            Dropdown::new_rw(base_note, ["A","Bb","B","C"])  //XXX cf sharps
        ))
        .style(|s| s.size_full().items_center().justify_center().gap(10));

    let chordPane = 
        h_stack((
            label(|| "Chord"),
            Dropdown::new_rw(chord, ["major","minor","dim 7"])  //XXX cf 5ths, b5ths
        ))
        .style(|s| s.size_full().items_center().justify_center().gap(10));

    let color = RwSignal::new(css::ORANGE);

    let noteGrid = SatValuePicker::new(move || color.get())
        .on_change(move |c| color.set(c))
        .style(|s| s.width(600).min_height(200).border(1))
        .debug_name("2d picker")
        ;

//XXX how does items_center() relate to the CSS stuff? Is it an alternative?
    v_stack((counter_element,baseNotePane,chordPane,noteGrid,text_element))
        .style(|s| s.width_full().items_center())
//        .style(|s| s.width_full().display(Display::Flex).flex_direction(FlexDirection::Column).align_items(AlignItems::Center))  XXX also works...
}


//////////////////////////


pub struct SatValuePicker {
    id: ViewId,
    size: Size,
    current_color: AlphaColor<Hsl>,
    on_change: Option<Box<dyn Fn(Color)>>,
    track: bool,
}

impl SatValuePicker {
    pub fn new(color: impl Fn() -> Color + 'static) -> Self {
        let id = ViewId::new();
        let color = create_updater(color, move |c| id.update_state(c));
        Self {
            id,
            size: Size::ZERO,
            current_color: color.convert(),
            on_change: None,
            track: false,
        }
    }

    fn position_to_hsl(&self, pos: Point) -> AlphaColor<Hsl> {
        let hue = self.current_color.components[0];

println!("to_hsl  x: {}",pos.x);
println!("to_hsl  y: {}",pos.y);

        let saturation =
            (pos.x / self.size.width * 100.).clamp(0.0 + f64::EPSILON, 100.0 - f64::EPSILON);

        let value = ((1.0 - (pos.y / self.size.height)) * 100.)
            .clamp(0.0 + f64::EPSILON, 100.0 - f64::EPSILON);

        let alpha = self.current_color.components[3];

        AlphaColor::<Hsl>::new([hue, saturation as f32, value as f32, alpha])
    }

    pub fn on_change(mut self, on_change: impl Fn(Color) + 'static) -> Self {
        self.on_change = Some(Box::new(on_change));
        self
    }
}

impl View for SatValuePicker {
    fn id(&self) -> ViewId {
        self.id
    }

    fn compute_layout(&mut self, _cx: &mut floem::context::ComputeLayoutCx) -> Option<Rect> {
        self.size = self.id.get_size().unwrap_or_default();
        None
    }

    fn update(&mut self, _cx: &mut floem::context::UpdateCx, state: Box<dyn std::any::Any>) {
        if let Ok(color) = state.downcast::<Color>() {
            self.current_color = color.convert();
        }
    }

    fn event_before_children(
        &mut self,
        _cx: &mut floem::context::EventCx,
        event: &Event,
    ) -> EventPropagation {
        if let Some(on_change) = &self.on_change {
            match event {
                Event::Pointer(PointerEvent::Down(PointerButtonEvent { state, .. })) => {
                    self.current_color = self.position_to_hsl(state.logical_point());
                    on_change(self.current_color.convert());
                    self.track = true;
                    self.id.request_active();
                }
                Event::Pointer(PointerEvent::Up(PointerButtonEvent { state, .. })) => {
                    self.current_color = self.position_to_hsl(state.logical_point());
                    on_change(self.current_color.convert());
                    self.id.clear_active();
                    self.track = false;
                }
                Event::Pointer(PointerEvent::Move(pu)) => {
                    if self.track {
                        self.current_color = self.position_to_hsl(pu.current.logical_point());
                        on_change(self.current_color.convert());
                    }
                }
                _ => {
                    return EventPropagation::Continue;
                }
            }
        }
        EventPropagation::Stop
    }

    fn paint(&mut self, cx: &mut PaintCx) 
    {
        let size = self.size;

        let clip_path = Rect::ZERO.with_size(size).to_rounded_rect(8.);

        cx.push_layer(Mix::Normal, 1.0, Affine::IDENTITY, &clip_path);

        let cell_width = 40.0;
        let cell_height = 20.0;

        let cols = (size.width / cell_width).ceil() as usize;
        let rows = (size.height / cell_height).ceil() as usize;

        for col in 0..cols {
            let line = Line::new(Point::new(col as f64 * cell_width,0.0),Point::new(col as f64 * cell_width,rows as f64 * cell_height));
            cx.stroke(&line, &Brush::Solid(css::BLUE), &Stroke::new(1.0));
        }

        for row in 0..rows {
            let line = Line::new(Point::new(0.0,row as f64 * cell_height),Point::new(cols as f64 * cell_width,row as f64 * cell_height));
            cx.stroke(&line, &Brush::Solid(css::ORANGE), &Stroke::new(1.0));
        }

//XXX only showing once the cursor leaves the canvas!
        if size.width > 0.0 && size.height > 0.0 {
            let saturation = self.current_color.components[1];
            let value = self.current_color.components[2];

            let x_pos = saturation as f64 / 100.0 * size.width;
            let y_pos = (1.0 - value as f64 / 100.0) * size.height;

            let indicator_radius = 6.0;
            let indicator_circle = Circle::new(Point::new(x_pos, y_pos), indicator_radius);
            let inner_indicator_circle =
                Circle::new(Point::new(x_pos, y_pos), indicator_radius - 2.);

            cx.stroke(&indicator_circle, css::WHITE, &Stroke::new(2.0));
            cx.stroke(&inner_indicator_circle, css::BLACK, &Stroke::new(2.0));
        }

//        cx.restore();
//cx.save();

        cx.pop_layer();

        let base_color = css::TRANSPARENT;
        cx.fill(&clip_path, base_color, 0.);
    }
}


