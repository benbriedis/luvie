# USE:  "bear -- make"   for neovim

CC = g++-13

FLTK_CONFIG    = ../deps/installation/bin/fltk-config
FLTK_CXXFLAGS := $(shell $(FLTK_CONFIG) --cxxflags)
FLTK_LDFLAGS  := $(shell $(FLTK_CONFIG) --ldflags) $(shell $(FLTK_CONFIG) --libs)

CXXFLAGS = $(FLTK_CXXFLAGS) -std=c++23 -std=gnu++23 -fmodules-ts -g -MMD -MP
LDFLAGS  = $(FLTK_LDFLAGS)

TARGET = main

SRCS = main.cpp appWindow.cpp editor.cpp songEditor.cpp patternEditor.cpp grid.cpp songGrid.cpp patternGrid.cpp trackLabels.cpp popup.cpp modernTabs.cpp cursors.cpp transport.cpp simpleTransport.cpp playhead.cpp markerPopup.cpp markerRuler.cpp observableTimeline.cpp patternPanel.cpp inlineEditDispatch.cpp noteLabels.cpp

OBJS = $(SRCS:.cpp=.o)
DEPS = $(SRCS:.cpp=.d)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

%.o: %.cpp
	$(CC) $(CXXFLAGS) -c $< -o $@

-include $(DEPS)

clean:
	rm -f $(TARGET) $(OBJS) $(DEPS)
