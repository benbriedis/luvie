
use iced::{
    Element, Event, Length, Point, Rectangle, Renderer, Size, Theme, Vector, advanced:: {
        Clipboard, Shell, layout::{self, Layout}, renderer, widget::{self,Widget,
        }
    }, mouse::{self}, overlay
};


//XXX maybe even hard code in some of these generic parameters?
//pub struct ContextMenuPopup<'a, Message, Theme = crate::Theme, Renderer = crate::Renderer>
pub struct ContextMenuPopup<'a, Message, Theme, Renderer >
//where
    //Renderer: Renderer,
{
    content: Element<'a, Message, Theme, Renderer>,
    desiredPosition: Point,
    position: Point,
    width: f32
}

//TODO delete some generic prameters
impl<'a, Message, Theme, Renderer> ContextMenuPopup<'a, Message, Theme, Renderer>
//where
    //Renderer: core::Renderer,
{
    pub fn new(desiredPosition:Point,width:f32,content: impl Into<Element<'a, Message, Theme, Renderer>>) -> Self 
    {
        Self {
            content: content.into(),
            desiredPosition, 
            position: desiredPosition,
            width 
        }
    }

    fn popupPosition(&self,size:Size, pos:Point) -> Point
    {
        let verticalPadding = 4.0;
    //TODO probably need to account from internal padding of popup    
        let sidePadding = 30.0;

        // TODO calculate?
        let height = 85.0;

        /* Place above if place, otherwise below */
        let placeAbove = pos.y >= (height + verticalPadding);
        let y = if placeAbove {
            pos.y - height - verticalPadding
        } 
        else {
//FIXME HACK            
            pos.y + 30.0 + verticalPadding
//            pos.y + settings.rowHeight + verticalPadding
        };

        /* Use cell LHS if there is space, otherwise get as close as possible */
        let maxX = size.width - self.width - sidePadding;
        let x = if pos.x > maxX { maxX } else { pos.x };
        let x = if x < sidePadding { sidePadding } else { x };

        Point{x,y}
    }
}

impl<Message, Theme> Widget<Message, Theme, Renderer>
    for ContextMenuPopup<'_, Message, Theme, Renderer>
//where
//    Renderer: core::Renderer,
{
//TODO delete?    
    fn tag(&self) -> widget::tree::Tag {
        self.content.as_widget().tag()
    }

    fn state(&self) -> widget::tree::State {
        self.content.as_widget().state()
    }

    fn children(&self) -> Vec<widget::Tree> {
        self.content.as_widget().children()
    }

    fn diff(&self, tree: &mut widget::Tree) {
        self.content.as_widget().diff(tree);
    }

    fn size(&self) -> Size<Length> 
    {
        Size {width: Length::Fill, height: Length::Fill}
    }

    fn layout(&mut self,tree: &mut widget::Tree,renderer: &Renderer,limits: &layout::Limits) -> layout::Node
    {
        self.position = self.popupPosition(limits.max(), self.desiredPosition);

        let node = self.content.as_widget_mut()
            .layout(tree, renderer, &layout::Limits::new(Size::ZERO, limits.max()))
            .move_to(self.position);

        layout::Node::with_children(limits.max(), vec![node])
    }

    fn operate(&mut self,tree: &mut widget::Tree,layout: Layout<'_>,renderer: &Renderer,operation: &mut dyn widget::Operation) 
    {
        self.content.as_widget_mut().operate(
            tree,
            layout.children().next().unwrap(),
            renderer,
            operation,
        );
    }

    fn update(&mut self,tree: &mut widget::Tree,event: &Event,layout: Layout<'_>,cursor: mouse::Cursor,
        renderer: &Renderer,clipboard: &mut dyn Clipboard,shell: &mut Shell<'_, Message>, viewport: &Rectangle) 
    {
        self.content.as_widget_mut().update(tree,event,layout.children().next().unwrap(),
            cursor,renderer,clipboard,shell, viewport);
    }

    fn mouse_interaction(&self,tree: &widget::Tree,layout: Layout<'_>,cursor: mouse::Cursor,
        viewport: &Rectangle,renderer: &Renderer) -> mouse::Interaction 
    {
        self.content.as_widget().mouse_interaction(tree,layout.children().next().unwrap(), cursor,
            viewport, renderer
        )
    }

    fn draw(&self,tree: &widget::Tree,renderer: &mut Renderer,theme: &Theme,style: &renderer::Style,
        layout: Layout<'_>, cursor: mouse::Cursor, viewport: &Rectangle) 
    {
        let bounds = layout.bounds();

        if let Some(clipped_viewport) = bounds.intersection(viewport) {
            self.content.as_widget().draw(tree, renderer, theme, style, layout.children().next().unwrap(),
                cursor, &clipped_viewport);
        }
    }

    fn overlay<'b>(&'b mut self,tree: &'b mut widget::Tree,layout: Layout<'b>,renderer: &Renderer,
        viewport: &Rectangle, translation: Vector,) -> Option<overlay::Element<'b, Message, Theme, Renderer>> 
    {
        self.content.as_widget_mut().overlay(tree, layout.children().next().unwrap(),
            renderer,viewport, translation
        )
    }
}

impl<'a, Message, Theme> From<ContextMenuPopup<'a, Message, Theme, Renderer>>
    for Element<'a, Message, Theme, Renderer>
where
    Message: 'a,
    Theme: 'a,
//    Renderer: core::Renderer + 'a,
//    Renderer: 'a,
{
    fn from(
        popup: ContextMenuPopup<'a, Message, Theme, Renderer>,
    ) -> Element<'a, Message, Theme, Renderer> {
        Element::new(popup)
    }
}

