# USE:  "bear -- make"   for neovim

CC = g++-13

FLTK_CONFIG    = ../deps/installation/bin/fltk-config
FLTK_CXXFLAGS := $(shell $(FLTK_CONFIG) --cxxflags)
FLTK_LDFLAGS  := $(shell $(FLTK_CONFIG) --ldflags) $(shell $(FLTK_CONFIG) --libs)

CXXFLAGS = $(FLTK_CXXFLAGS) -std=c++23 -g -MMD -MP
LDFLAGS  = $(FLTK_LDFLAGS)

BUILDDIR = build
TARGET   = $(BUILDDIR)/main

SRCS = main.cpp appWindow.cpp editor.cpp songEditor.cpp patternEditor.cpp grid.cpp songGrid.cpp patternGrid.cpp trackLabels.cpp popup.cpp modernTabs.cpp cursors.cpp transport.cpp simpleTransport.cpp playhead.cpp markerPopup.cpp markerRuler.cpp observableTimeline.cpp patternPanel.cpp inlineEditDispatch.cpp noteLabels.cpp trackContextPopup.cpp

OBJS = $(SRCS:%.cpp=$(BUILDDIR)/%.o)
DEPS = $(SRCS:%.cpp=$(BUILDDIR)/%.d)

all: $(BUILDDIR) $(TARGET)

rebuild: clean all

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)
	@echo "Build complete: $(TARGET)"

$(BUILDDIR)/%.o: %.cpp
	$(CC) $(CXXFLAGS) -c $< -o $@

-include $(DEPS)

.PHONY: all rebuild clean

clean:
	rm -rf $(BUILDDIR)
