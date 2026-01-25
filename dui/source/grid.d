import dlangui;
import std.stdio;
import std.typecons;
import std.math.rounding;
//import std.math.traits : isNaN;
//import std.math;
import app;

struct GridSettings {
    int numRows = 10;
    int numCols = 20;
    int rowHeight = 30;
    int colWidth = 50;
    float snap = 0.25;  //OPTIONAL
    int popupWidth = 300;
}

enum Side { LEFT, RIGHT }

struct ResizeData {
    int cellIndex;
    Side side;
    Cell workingCell;
}

struct MoveData {
    int cellIndex;
    Point grabPosition;
    Cell lastValid;
    Cell workingCell;
    bool amOverlapping;
}

enum CursorModeType { Init, Pointer, Movable, Moving, Resizable, Resizing }

struct CursorMode {
    CursorModeType type;
    union {
		ResizeData resizeData;
		MoveData moveData;
    }
}

/*	
	In D, objects are always passed by reference by default when using class types, while struct types are passed by value
*/

alias AddCellHandler = void delegate(Cell cell);   //XXX ref? copy?


//XXX why are D syntatic errors not showing in Neovim?

class Grid : CanvasWidget 
{
    private {
        int _scaleX;
        uint _bgcolor = 0x101010;
        bool[4] _partVisible = [true, true, true, true];

        bool _needRepaint = true;
        Point[8] _directionVectors;


//	mode = CursorMode::MOVING(data.clone());
		CursorMode mode;
		GridSettings settings;
//XXX vec? also ref?	
		Cell[] cells;
		AddCellHandler addCellHandler;
    }

    ColorDrawBuf drawBuffer;
//   protected Ref!ColorDrawBuf texture;

    this(Cell[] cells,AddCellHandler addCellHandler) {
        super("My grid");

		this.cells = cells;
		this.addCellHandler = addCellHandler;

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
			leftButtonDown(&event);
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

	void leftButtonDown(MouseEvent* event)
	{
        GridSettings s = settings;  //XXX is this going to copy?

		if (mode.type == CursorModeType.Movable) {
writeln("Got Push - was MOVABLE, setting MOVING");                
			mode.type = CursorModeType.Moving;

			/* Required in case you click on a filled cell without moving it */
			//self.movingCell = Some(self.cells[self.selectedCell.unwrap()]);
//app::redraw();  //XXX dont do this all the time
		}
		else if (mode.type == CursorModeType.Resizable) {
			mode.type = CursorModeType.Resizing;
		}
		else {
			/* Add a cell: */
			int row = cast(int) floor(event.y / cast(float) s.rowHeight);
			int col = cast(int) floor(event.x / cast(float) s.colWidth);

			Cell cell = {row,col,length:1.0};

			if (event.x < s.colWidth * s.numCols && event.y < s.rowHeight * s.numRows) {
				if (overlappingCell(&cell,-1) < 0) {
//BUG can add multiple cells to one cell

					//state.cache.clear();  
					addCellHandler(cell);

//	void draw(DrawBuf buf,Rect rc)
//					draw();  //TODO check required. ALSO is draw() the correct call to use?
				}
			}
		}
	}

//XXX is Cell* a the best way to pass data through?
	/* 
		selected = -1 means ????
		Returns -1 if no overlapping cell.
	*/
    int overlappingCell(Cell* a,int selected) 
    {
        float aStart = a.col;
        float aEnd = a.col + a.length;

		foreach (i, b; this.cells) {	
            if (selected == i) 
                continue; 

            if (b.row != a.row) 
                continue; 

            float bStart = b.col;
            float bEnd = b.col + b.length;

            float firstEnd = aStart <= bStart ? aEnd : bEnd;
            float secondStart = aStart <= bStart ? bStart : aStart;

            if (firstEnd > secondStart) //XXX HACK
                return cast(int)i;
        }
        return -1;
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
writeln("Called draw()");		
        GridSettings s = settings;   //XXX ref?

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

        buf.drawFragment(rc.left, rc.top, drawBuffer, Rect(0,0,400,400));

        /* Draw cells: */
		foreach (i, c; this.cells) {	
			if (mode.type == CursorModeType.Moving) {
				if (i != mode.moveData.cellIndex) 
					drawCell(c);
			}
			else if (mode.type == CursorModeType.Resizing) {
                if (i != mode.resizeData.cellIndex) 
                    drawCell(c);
			}
		}

/*			
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
		

	void drawCell(Cell c)
	{
		auto s = settings;
		float lineWidth = 1.0;

		 //TODO possibly size to be inside grid, although maybe not at start
		int x = cast(int) round(c.col * s.colWidth);
		int y = cast(int) round(c.row * s.rowHeight + lineWidth);
		int width = cast(int) (c.length * s.colWidth).round();
		int height = cast(int) round(s.rowHeight - 2.0 * lineWidth);

//Rect a = Rect(1,1,1,1);

//XXX NB fillGradientRect(), drawFrame(), drawRescaled(), drawFocusRect() are interesting
		drawBuffer.drawRect(Rect(x,y,width,height),0x1293D8);

		/* Add the dark line on the left: */
		drawBuffer.drawRect(Rect(x,y,8,height),0x126090);
	}
}	

