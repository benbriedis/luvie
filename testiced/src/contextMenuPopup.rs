
use iced::{
    Renderer, Element, Event, Length, Point, Rectangle, Size, Theme, 
    advanced::{
        Clipboard, Shell, layout::{self, Layout}, renderer, widget::{self,Widget},
    }, 
    mouse::{self}
};



pub struct ContextMenuPopup<'a, Message>
{
    content: Element<'a, Message>,
    desiredPosition: Point,
    position: Point,
    width: f32,
    rowHeight: f32
}

impl<'a, Message> ContextMenuPopup<'a, Message>
{
    pub fn new(desiredPosition:Point,width:f32,rowHeight:f32,content: impl Into<Element<'a, Message>>) -> Self 
    {
        Self {
            content: content.into(),
            desiredPosition, 
            position: desiredPosition,
            width,
            rowHeight
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
            pos.y + self.rowHeight + verticalPadding
        };

        /* Use cell LHS if there is space, otherwise get as close as possible */
        let maxX = size.width - self.width - sidePadding;
        let x = if pos.x > maxX { maxX } else { pos.x };
        let x = if x < sidePadding { sidePadding } else { x };

        Point{x,y}
    }
}

impl<Message> Widget<Message, Theme, Renderer> for ContextMenuPopup<'_, Message>
{
    fn children(&self) -> Vec<widget::Tree> { self.content.as_widget().children() }

//XXX required for slider
    fn diff(&self, tree: &mut widget::Tree) { self.content.as_widget().diff(tree); }

    fn size(&self) -> Size<Length> { Size {width: Length::Fill, height: Length::Fill} }


    fn operate(&mut self,tree: &mut widget::Tree,layout: Layout<'_>,renderer: &Renderer,operation: &mut dyn widget::Operation) 
    {
        self.content.as_widget_mut().operate(tree, layout.children().next().unwrap(), renderer, operation);
    }

    fn update(&mut self,tree: &mut widget::Tree,event: &Event,layout: Layout<'_>,cursor: mouse::Cursor,
        renderer: &Renderer,clipboard: &mut dyn Clipboard,shell: &mut Shell<'_, Message>, viewport: &Rectangle) 
    {
        self.content.as_widget_mut().update(tree,event,layout.children().next().unwrap(),
            cursor,renderer,clipboard,shell, viewport);
    }

    fn draw(&self,tree: &widget::Tree,renderer: &mut Renderer,theme: &Theme,style: &renderer::Style,
        layout: Layout<'_>, cursor: mouse::Cursor, viewport: &Rectangle) 
    {
        let bounds = layout.bounds();

        if let Some(clipped_viewport) = bounds.intersection(viewport) {
            self.content.as_widget()
                .draw(tree, renderer, theme, style, layout.children().next().unwrap(),cursor, &clipped_viewport);
        }
    }

    fn layout(&mut self,tree: &mut widget::Tree,renderer: &Renderer,limits: &layout::Limits) -> layout::Node
    {
        self.position = self.popupPosition(limits.max(), self.desiredPosition);

        let node = self.content.as_widget_mut()
            .layout(tree, renderer, &layout::Limits::new(Size::ZERO, limits.max()))
            .move_to(self.position);

        layout::Node::with_children(limits.max(), vec![node])
    }
}

impl<'a, Message:'a> From<ContextMenuPopup<'a, Message>> for Element<'a, Message>
{
    fn from(popup: ContextMenuPopup<'a, Message>) -> Self {
        Element::new(popup)
    }
}

