module app;

import dlangui;
import grid;
import std.stdio;


mixin APP_ENTRY_POINT;

struct Cell {
    int row;
//XXX possibly best leaving Cell as f32
    float col;       //XXX awkward name given type. Might be the best we have for the moment though
    float length;
}


extern (C) int UIAppMain(string[] args) {
	Window window = Platform.instance.createWindow(
		"DlangUI example - HelloWorld", 
		null, WindowFlag.Resizable, 300, 300
	);

	auto vlayout = new VerticalLayout();
	vlayout.margins = 20;
	vlayout.padding = 10;
	vlayout.backgroundColor = 0xFFFFC0;

	auto buttons = new HorizontalLayout();
	auto btn1 = new Button(null, "Ok"d);
	buttons.addChild(btn1);
	auto btn2 = new Button(null, "Cancel"d);
	buttons.addChild(btn2);
	vlayout.addChild(new TextWidget(null, ""d));
	vlayout.addChild(buttons);

	vlayout.addChild(buttons);

	Cell[] cells = new Cell[0];

	auto addCellHandler = delegate(Cell cell) {
		cells ~= cell;
writeln("Adding cell #num:",cells.length);		
	};

	auto grid = new Grid(cells,addCellHandler);
	vlayout.addChild(grid);

//Connect method or ~= operator will append new handler to list of existing handlers.
	btn1.click = delegate(Widget src) {
		window.showMessageBox("Button btn1 pressed"d, "Editor content:"d );
		return true;
	};	
//OR USE:	
//btn1.click.connect(delegate(Widget src) {

//As a handler, instead of delegate you can use instance of class implementing interface used to define signal.

	btn2.click = delegate(Widget src) {
		window.close();
		return true;
	};

/*		
	grid.onClick = delegate(Widget src) {
		writeln("onClick");
	};
*/		
	grid.click = delegate (Widget w) { writeln("grid.click")  ; return true; };

    window.mainWidget = vlayout;

    window.show();

    return Platform.instance.enterMessageLoop();
}

