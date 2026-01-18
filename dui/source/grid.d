import dlangui;
import std.stdio;

struct GridSettings {
    int numRows = 10;
    int numCols = 20;
    int rowHeight = 20;
    int colWidth = 30;
    float snap = 0.25;  //OPTIONAL
    int popupWidth = 300;
}


class Grid : CanvasWidget {

    private {
        int _scaleX;
        uint _bgcolor = 0x101010;
        bool[4] _partVisible = [true, true, true, true];

        bool _needRepaint = true;
        Point[8] _directionVectors;
    }

    ColorDrawBuf _drawBuf;

    this() {
        super("My grid");

writeln("In grid this()");

        //layoutWidth(FILL_PARENT).layoutHeight(FILL_PARENT);
        layoutWidth(400).layoutHeight(400);

		onDrawListener = delegate(CanvasWidget canvas, DrawBuf buf, Rect rc) {
//XXX can remove some redraws due to cursor moves			

			this.draw(buf);

			int x = rc.left;
			int y = rc.top;

writeln("In grid this() - onDrawListener x:",x,"y:",y);

			buf.fillRect(Rect(x+20, y+20, x+150, y+200), 0x80FF80);
/*
			buf.fillRect(Rect(x+90, y+80, x+250, y+250), 0x80FF80FF);
			canvas.font.drawText(buf, x + 40, y + 50, "fillRect()"d, 0xC080C0);
			buf.drawFrame(Rect(x + 400, y + 30, x + 550, y + 150), 0x204060, Rect(2,3,4,5), 0x80704020);
			canvas.font.drawText(buf, x + 400, y + 5, "drawFrame()"d, 0x208020);
			canvas.font.drawText(buf, x + 300, y + 100, "drawPixel()"d, 0x000080);
			for (int i = 0; i < 80; i++)
				buf.drawPixel(x+300 + i * 4, y+140 + i * 3 % 100, 0xFF0000 + i * 2);
			canvas.font.drawText(buf, x + 300, y + 420, "drawLine()"d, 0x800020);

			// Poly line example
			PointF[] poly = [vec2(x+130, y+150), vec2(x+240, y+80), vec2(x+170, y+170), vec2(x+380, y+270), vec2(x+220, y+400), vec2(x+130, y+330)];
			buf.polyLineF(poly, 18.0f, 0x80804020, true, 0x80FFFF00);
			canvas.font.drawText(buf, x + 190, y + 260, "polyLineF()"d, 0x603010);
			PointF[] poly2 = [vec2(x+430, y+250), vec2(x+540, y+180), vec2(x+470, y+270), vec2(x+580, y+300),
				vec2(x+620, y+400), vec2(x+480, y+350), vec2(x+520, y+450), vec2(x+480, y+430)];
			buf.fillPolyF(poly2, 0x80203050);
			canvas.font.drawText(buf, x + 500, y + 460, "fillPolyF()"d, 0x203050);

			buf.drawEllipseF(x+300, y+600, 200, 150, 3, 0x80008000, 0x804040FF);
			canvas.font.drawText(buf, x + 300, y + 600, "fillEllipseF()"d, 0x208050);

			buf.drawEllipseArcF(x+540, y+600, 150, 180, 45, 130, 3, 0x40008000, 0x804040FF);
			canvas.font.drawText(buf, x + 540, y + 580, "drawEllipseArcF()"d, 0x208050);
*/				
		};
    }

//ColorDrawBuf _drawBuf;

	void draw(DrawBuf buf)
	{
		GridSettings s;

/*
_drawBuf = new ColorDrawBuf(_fullScrollableArea.width, _fullScrollableArea.height);

buf.drawFragment(_clientRect.left, _clientRect.top, _drawBuf, 
				 Rect(_visibleScrollableArea.left, _visibleScrollableArea.top, 
					  _visibleScrollableArea.left + _clientRect.width, _visibleScrollableArea.top + _clientRect.height));
*/


//        let s = self.settings;

		buf.fill(0xFFFFFF);

    //TODO cache this? or parts?

        //XXX in theory only need to draw the horizontal lines once so long as the cells sit inside them 
        //XXX could possibly omit redrawing cells not on the last row too.
        
        /* Draw the grid horizontal lines: */
        for (int i = 0; i<= s.numRows; i++) {
            int y = i * s.rowHeight;
            int endX = s.numCols * s.colWidth;
			buf.drawLine(Point(0, y), Point(endX, y), 0x12ee12);
        }

        /* Draw the grid vertical lines: */
        for (int i = 0; i <= s.numCols; i++) {
            int x = i * s.colWidth;
            int endY = s.numRows * s.rowHeight;
			buf.drawLine(Point(x, 0), Point(x, endY), 0xee1212);
        }

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

