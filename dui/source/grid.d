import dlangui;
import std.stdio;

struct GridSettings {
    int numRows = 10;
    int numCols = 20;
    int rowHeight = 30;
    int colWidth = 50;
    float snap = 0.25;  //OPTIONAL
    int popupWidth = 300;
}


class Grid : CanvasWidget 
{
    private {
        int _scaleX;
        uint _bgcolor = 0x101010;
        bool[4] _partVisible = [true, true, true, true];

        bool _needRepaint = true;
        Point[8] _directionVectors;
    }

    ColorDrawBuf drawBuffer;
//   protected Ref!ColorDrawBuf texture;

    this() {
        super("My grid");
//        clickable = true;
//        focusable = true;
//        trackHover = true;

        //layoutWidth(FILL_PARENT).layoutHeight(FILL_PARENT);
        layoutWidth(600).layoutHeight(400);

        drawBuffer = new ColorDrawBuf(600,400);

		onDrawListener = delegate(CanvasWidget canvas, DrawBuf buf, Rect rc) {   //XXX delegate?
//XXX can remove some redraws due to cursor moves			

//			canvas.drawLine(Point(0,0), Point(400,400), 0xff0000);
			this.draw(buf,rc);
		};
    }

    // called to process click and notify listeners
    override protected bool handleClick() {
writeln("in handleClick");
		return super.handleClick();
    }

	override bool onMouseEvent(MouseEvent event) {
		if (event.action == MouseAction.ButtonDown && event.button == MouseButton.Left) {
writeln("onMouseEvent - LEFT BUTTON DOWN");		
			return true;
		}
//		if (event.action == MouseAction.FocusOut && _dragging) {
		if (event.action == MouseAction.FocusOut) {
writeln("onMouseEvent - FOCUS OUT");		
			return true;
		}
		if (event.action == MouseAction.FocusIn) {
writeln("onMouseEvent - FOCUS IN");		
			return true;
		}
        if (event.action == MouseAction.Move) {
writeln("onMouseEvent - MOVE x:",event.x," y:",event.y);		
		}
        if (event.action == MouseAction.ButtonUp && event.button == MouseButton.Left) {
writeln("onMouseEvent - LEFT BUTTON UP");		
		}
        if (event.action == MouseAction.Leave) {
writeln("onMouseEvent - LEAVE ");		
		}
        if (event.action == MouseAction.Cancel) {
writeln("onMouseEvent - CANCEL ");		
		}
		return false;
	}

/*
    bool onClick(Widget source)
	{
		writeln("onClick");
        //checked = !checked;
//        return super.handleClick();
		return true;
    }
*/	

	void draw(DrawBuf buf,Rect rc)
	{
		GridSettings s;

		drawBuffer.fill(0xFFFFFF);

    //TODO cache this? or parts?

        //XXX in theory only need to draw the horizontal lines once so long as the cells sit inside them 
        //XXX could possibly omit redrawing cells not on the last row too.
        
        /* Draw the grid horizontal lines: */
        for (int i = 0; i<= s.numRows; i++) {
            int y = i * s.rowHeight;
            int endX = s.numCols * s.colWidth;
			drawBuffer.drawLine(Point(0, y), Point(endX, y), 0x12ee12);
        }

        /* Draw the grid vertical lines: */
        for (int i = 0; i <= s.numCols; i++) {
            int x = i * s.colWidth;
            int endY = s.numRows * s.rowHeight;
			drawBuffer.drawLine(Point(x, 0), Point(x, endY), 0xee1212);
        }

        buf.drawFragment(rc.left, rc.top, drawBuffer, 
			Rect(0,0,400,400));

/*			
        / * Draw cells: * /
        for (i,c) in self.cells.iter().enumerate() {
            match self.mode {
                CursorMode::MOVING(ref data) => {
                    if i != data.cellIndex {
                        self.drawCell(*c);
                    }
                }
                CursorMode::RESIZING(ref data) => {
                    if i != data.cellIndex {
                        self.drawCell(*c);
                    }
                }
                _ => self.drawCell(*c)
            }
        };

        / * Draw selected note last so it sits on top * /
        match self.mode {
            CursorMode::MOVING(ref data) => self.drawCell(data.workingCell),
            CursorMode::RESIZING(ref data) => self.drawCell(data.workingCell),
            _ => ()
        }

        // TODO maybe draw an app

		 / * Set the cursor display: * /
		match self.mode {
			CursorMode::POINTER => self.widget.window().unwrap().set_cursor(Cursor::Arrow),
			CursorMode::MOVABLE(ref data) => self.widget.window().unwrap().set_cursor(Cursor::Hand),
			CursorMode::MOVING(ref data) => self.widget.window().unwrap().set_cursor(Cursor::Hand),
			CursorMode::RESIZING(ref data) => self.widget.window().unwrap().set_cursor(Cursor::Arrow),
			_ => ()
		}
*/
	}
}	

