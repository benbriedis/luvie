import dlangui;
import grid;

mixin APP_ENTRY_POINT;

extern (C) int UIAppMain(string[] args) {
	Window window = Platform.instance.createWindow(
		"DlangUI example - HelloWorld", 
		null, WindowFlag.Resizable, 300, 300
	);




	auto vlayout = new VerticalLayout();
	vlayout.margins = 20;
	vlayout.padding = 10;
	vlayout.backgroundColor = 0xFFFFC0;
	vlayout.addChild(new TextWidget(null, "First text item"d));
	vlayout.addChild(new TextWidget(null, "Second text item"d));
	vlayout.addChild(new TextWidget(null, "Third text item"d));
	vlayout.addChild(new CheckBox(null, "Check box text"d));
	vlayout.addChild(new ImageTextButton(null, "dialog-ok", "Check box text"d));

	auto buttons = new HorizontalLayout();
	auto btn1 = new Button(null, "Ok"d);
	buttons.addChild(btn1);
	auto btn2 = new Button(null, "Cancel"d);
	buttons.addChild(btn2);
	vlayout.addChild(new TextWidget(null, ""d));
	vlayout.addChild(buttons);

	vlayout.addChild(buttons);

	auto grid = new Grid();
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

    window.mainWidget = vlayout;

    window.show();

    return Platform.instance.enterMessageLoop();
}

