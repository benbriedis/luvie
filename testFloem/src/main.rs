
use floem::event::{Event, EventPropagation};
use floem::{ViewId, prelude::*};
use floem::views::dropdown::Dropdown;
use peniko::{Brush, Mix};
use peniko::color::{palette::css};
use peniko::kurbo::{Affine, Rect, Size, Line, Point,Stroke};
use floem::reactive::{Scope, SignalRead, create_effect, create_updater};

use floem::{
    context::PaintCx,
};


fn main() {
    floem::launch(counter_view);
}

const CUSTOM_CHECK_SVG: &str = r##"
<svg width="800px" height="800px" viewBox="0 0 24 24" fill="none" xmlns="http://www.w3.org/2000/svg">
<path fill-rule="evenodd" clip-rule="evenodd" d="M20.6097 5.20743C21.0475 5.54416 21.1294 6.17201 20.7926 6.60976L10.7926 19.6098C10.6172 19.8378 10.352 19.9793 10.0648 19.9979C9.77765 20.0166 9.49637 19.9106 9.29289 19.7072L4.29289 14.7072C3.90237 14.3166 3.90237 13.6835 4.29289 13.2929C4.68342 12.9024 5.31658 12.9024 5.70711 13.2929L9.90178 17.4876L19.2074 5.39034C19.5441 4.95258 20.172 4.87069 20.6097 5.20743Z" fill="#000000"/>
</svg>
"##;

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

    //XXX this just makes the adding and removing of notes reactive, not the changing of the notes themselves
    let cx = Scope::new();
//    let notes = cx.create_rw_signal(Vec::new());  THIS ONE COMPILES with the help of the cx
//    let notes = RwSignal::new(Vec::new());

//XXX non-reactive version:
    let notes = Vec::new();

    let noteGrid = NoteGrid::new(cx,notes,move || color.get())
        .style(|s| s.width(600).min_height(200).border(1))
        .debug_name("2d picker")
        ;

    let is_checked = RwSignal::new(true);

    let check1 = Checkbox::new_rw(is_checked).into_view();
    let check2 = Checkbox::new_rw_custom(is_checked, CUSTOM_CHECK_SVG).into_view();

//XXX how does items_center() relate to the CSS stuff? Is it an alternative?
    v_stack((counter_element,baseNotePane,chordPane,noteGrid,text_element,check1,check2))
        .style(|s| s.width_full().items_center())
//        .style(|s| s.width_full().display(Display::Flex).flex_direction(FlexDirection::Column).align_items(AlignItems::Center))  XXX also works...
}


//////////////////////////

#[derive(Clone)]
pub struct NotePosition {
    row: i64,
    col: i64
}

pub struct NoteGrid {
    cx: Scope,
    id: ViewId,
    size: Size,
    track: bool,

    cell_width: f64,
    cell_height: f64,
//    notes: &'a RwSignal<Vec<NotePosition>>
 //   notes: Fn() -> RwSignal<Vec<NotePosition>> + 'static 
//    notes: RwSignal<Vec<NotePosition>>   THIS REACTIVE ONE COMPILES
    notes: Vec<NotePosition>
}

impl NoteGrid {
  //  pub fn new(notes: &'static RwSignal<Vec<NotePosition>>, color: impl Fn() -> Color + 'static) -> Self {
  //  pub fn new(notes: impl Fn() -> RwSignal<Vec<NotePosition>> + 'static, color: impl Fn() -> Color + 'static) -> Self {

//    pub fn new(cx:Scope,notes:RwSignal<Vec<NotePosition>>, color: impl Fn() -> Color + 'static) -> Self {     THIS REACTIVE ONE COMPILES
    pub fn new(cx:Scope,notes:Vec<NotePosition>, color: impl Fn() -> Color + 'static) -> Self {
        let id = ViewId::new();

//self.id.inspect();

        Self {
            cx,
            id,
            size: Size::ZERO,
            track: false,
            cell_width: 40.0,
            cell_height: 20.0,
            notes
        }
    }
}

impl View for NoteGrid {
    fn id(&self) -> ViewId {
        self.id
    }

    fn compute_layout(&mut self, _cx: &mut floem::context::ComputeLayoutCx) -> Option<Rect> {
        self.size = self.id.get_size().unwrap_or_default();
        None
    }

    fn update(&mut self, _cx: &mut floem::context::UpdateCx, state: Box<dyn std::any::Any>) {
//XXX cf updating our grid data here instead?        
println!("In update");
//        if let Ok(color) = state.downcast::<Color>() {
//            self.current_color = color.convert();
 //       }
    }

    fn event_before_children(
        &mut self,
        _cx: &mut floem::context::EventCx,
        event: &Event,
    ) -> EventPropagation {
        match event {
            Event::Pointer(PointerEvent::Down(PointerButtonEvent { state, .. })) => {
                self.track = true;
//XXX probably dont want                
                self.id.request_active();

//XXX maybe move this to main struct like the code before?
                //XXX may be better on mouse up. What are others using?
                let pos = state.logical_point();
                let row = (pos.y / self.cell_height) as i64;
                let col = (pos.x / self.cell_width) as i64;

//XXX do we have to implement update()?
println!("Adding rect   row:{row}  col:{col}");
//                    self.notes.update(|val| val.clone().push(NotePosition{row,col})); THIS ONE COMPILES
                self.notes.push(NotePosition{row,col});

                //XXX if notes were reactive would paint be called automatically?
                //self.id.get_untracked().request_paint();
                self.id.request_paint();


//                        let notes = self.notes.get().clone().push(NotePosition{row,col});
//                       self.id.update_state(notes);


                //XXX is clone required here?

/*
                create_effect(move |_| {
                    self.notes.update(|val| val.clone().push(NotePosition{row,col}));
                });
*/

            }
            _ => {
                //XXX do we need?
                return EventPropagation::Continue;
            }
        }
        EventPropagation::Stop
    }

    fn paint(&mut self, cx: &mut PaintCx) 
    {
//TODO check
//self.id.layout_rect();
//self.id.get_content_rect()
//self.id.get_layout()
//etc

println!("In paint");

//println!("is_focused:{}",cx.is_focused(self.id));
//println!("debug_info:{}", cx.debug_info());
//println!("type_id:{}", cx.type_id());

//TODO only apply paint when our canvas if affected. (lapce may be OK example)

        let size = self.size;

//cx.save();
        let clip_path = Rect::ZERO.with_size(size).to_rounded_rect(8.);

        cx.push_layer(Mix::Normal, 1.0, Affine::IDENTITY, &clip_path);

        let cols = (size.width / self.cell_width).ceil() as usize;
        let rows = (size.height / self.cell_height).ceil() as usize;

        for col in 0..cols {
            let line = Line::new(Point::new(col as f64 * self.cell_width,0.0),Point::new(col as f64 * self.cell_width,rows as f64 * self.cell_height));
            cx.stroke(&line, &Brush::Solid(css::BLUE), &Stroke::new(1.0));
        }

        for row in 0..rows {
            let line = Line::new(Point::new(0.0,row as f64 * self.cell_height),Point::new(cols as f64 * self.cell_width,row as f64 * self.cell_height));
            cx.stroke(&line, &Brush::Solid(css::ORANGE), &Stroke::new(1.0));
        }

//XXX notion of paint isn't really reactive...        
//        for note in self.notes.get().iter() {   THIS ONE COMPILES
        for note in self.notes.iter() {
            let x0 = note.col as f64 * self.cell_width;
            let y0 = note.row as f64 * self.cell_height;

            /*
                Start a note right at x0 for musical reasons and add just before the next note.
                In the y axis let us see the grid lines.
            */
            let rect = Rect::new(x0, y0+1.0, x0+self.cell_width-1.0, y0+self.cell_height-1.0);
            cx.fill(&rect, &Brush::Solid(css::PINK), 0.);
        }

//        cx.restore();

        cx.pop_layer();

        let base_color = css::TRANSPARENT;
        cx.fill(&clip_path, base_color, 0.);


//XXX PaintCx.is_drag_paint() might be useful in future        
    }
}


