# USE:  "bear -- make"   for neovim

CC = g++-13

#cf using:  fltk-config --ldstaticflags
FLTK_CONFIG = ../deps/installation/bin/fltk-config
#XXX not sure I want to use  -std=gnu++23
CXXFLAGS = `$(FLTK_CONFIG) --cxxflags` -std=c++23 -std=gnu++23 -fmodules-ts -g
#CXXFLAGS = `$(FLTK_CONFIG) --cxxflags` -std=c++23 -std=gnu++23 -g
LDFLAGS = `$(FLTK_CONFIG) --ldflags` `$(FLTK_CONFIG) --libs`

TARGET = main

all: $(TARGET)

SRCS = main.cpp appWindow.cpp editor.cpp songEditor.cpp patternEditor.cpp grid.cpp songGrid.cpp patternGrid.cpp trackLabels.cpp popup.cpp modernTabs.cpp cursors.cpp transport.cpp simpleTransport.cpp playhead.cpp markerPopup.cpp markerRuler.cpp observableTimeline.cpp

$(TARGET): $(SRCS)
	$(CC) $(CXXFLAGS) $(SRCS) -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET)

