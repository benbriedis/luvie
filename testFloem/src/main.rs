
use floem::kurbo::Stroke;
use floem::prelude::*;
use floem::taffy::FlexDirection;
use floem::views::dropdown::Dropdown;
use peniko::{Brush, Mix};
use peniko::color::{palette::css};
use std::sync::atomic::{AtomicU32, Ordering};
use peniko::kurbo::{Affine, Rect, Shape, Size, Line, Point};

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

    let items = create_rw_signal(vec![1,1,2,2,3,3,4,4]);
    let unique_atomic = AtomicU32::new(0);
    let dyn_stack_element = dyn_stack(
            move || items.get(),
            move |_item| unique_atomic.fetch_add(1, Ordering::Relaxed),
            move |item| label(move || item),
        ).style(|s| s.flex_direction(FlexDirection::Column));

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

//XXX slider not showing for some reason...
    let sliderTest = slider::Slider::new(|| 40.pct());
//        .slider_style(|s| {
//            s.edge_align(true)
//            .handle_radius(50.pct())
  //          .bar_color(Color::BLACK)
//            .bar_color(css::BLACK)
 //           .bar_radius(100.pct())
//            .accent_bar_color(css::GREEN)
  //          .accent_bar_radius(100.pct())
  //          .accent_bar_height(100.pct())
//        });

//XXX is Floem's SVG stuff relevant?

    let paintTest = canvas(move |cx, size| {
        let rect_path = Rect::ZERO.with_size(size).to_rounded_rect(8.);

        let base_color = css::TRANSPARENT;

        draw_transparency_checkerboard(cx, size, &rect_path);

        cx.fill(&rect_path, base_color, 0.);
    })
    .style(|s| s.size(500, 500));

    v_stack((counter_element,baseNotePane,chordPane,sliderTest,text_element,paintTest, dyn_stack_element)).style(|s| s.gap(10)) //XXX gap is not working 
}

fn draw_transparency_checkerboard(cx: &mut PaintCx, size: Size, clip_path: &impl Shape) 
{
    cx.push_layer(Mix::Normal, 1.0, Affine::IDENTITY, clip_path);

    let cell_width = 20.0;
    let cell_height = 10.0;

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

    cx.pop_layer();
}

