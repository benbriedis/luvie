# USE:  "bear -- make"   for neovim

CC = g++-13

FLTK_CONFIG    = ../deps/installation/bin/fltk-config
FLTK_CXXFLAGS := $(shell $(FLTK_CONFIG) --cxxflags)
FLTK_LDFLAGS  := $(shell $(FLTK_CONFIG) --ldflags) $(shell $(FLTK_CONFIG) --libs)

JACK_CFLAGS := $(shell pkg-config --cflags jack 2>/dev/null)
JACK_LIBS   := $(shell pkg-config --libs   jack 2>/dev/null)

LO_CFLAGS   := $(shell pkg-config --cflags liblo 2>/dev/null)
LO_LIBS     := $(shell pkg-config --libs   liblo 2>/dev/null)

CXXFLAGS = $(FLTK_CXXFLAGS) $(JACK_CFLAGS) $(LO_CFLAGS) -std=c++23 -g -fPIC -MMD -MP
LDFLAGS  = $(FLTK_LDFLAGS) $(JACK_LIBS) $(LO_LIBS)

BUILDDIR = build
TARGET   = $(BUILDDIR)/luvie

SRCS = main.cpp appWindow.cpp editor.cpp songEditor.cpp patternEditor.cpp grid.cpp songGrid.cpp patternGrid.cpp trackLabels.cpp popup.cpp songPopup.cpp modernTabs.cpp cursors.cpp transport.cpp simpleTransport.cpp jackTransport.cpp playhead.cpp markerPopup.cpp markerRuler.cpp observableTimeline.cpp activePatternSet.cpp patternPanel.cpp inlineEditDispatch.cpp noteLabels.cpp trackContextPopup.cpp drumGrid.cpp drumPatternEditor.cpp pianorollGrid.cpp pianorollEditor.cpp loopEditor.cpp luvieApp.cpp nsm.cpp timelineIO.cpp connectionsOverlay.cpp

OBJS     = $(SRCS:%.cpp=$(BUILDDIR)/%.o)
DEPS     = $(SRCS:%.cpp=$(BUILDDIR)/%.d)
LIB      = $(BUILDDIR)/libluvie.a
LIB_OBJS = $(filter-out $(BUILDDIR)/main.o, $(OBJS))

all: $(BUILDDIR) $(TARGET) $(LIB)

rebuild: clean all

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)
	@echo "Build complete: $(TARGET)"

$(LIB): $(LIB_OBJS)
	ar rcs $@ $^

$(BUILDDIR)/%.o: %.cpp
	$(CC) $(CXXFLAGS) -c $< -o $@

-include $(DEPS)

INSTALL_BIN  = $(HOME)/.local/bin
INSTALL_APP  = $(HOME)/.local/share/applications
DESKTOP_SRC  = ../luvie.desktop

install: $(TARGET)
	install -Dm755 $(TARGET) $(INSTALL_BIN)/luvie
	install -Dm644 $(DESKTOP_SRC) $(INSTALL_APP)/luvie.desktop
	@update-desktop-database $(INSTALL_APP) 2>/dev/null || true
	@echo "Installed: $(INSTALL_BIN)/luvie"
	@echo "Installed: $(INSTALL_APP)/luvie.desktop"

uninstall:
	rm -f $(INSTALL_BIN)/luvie $(INSTALL_APP)/luvie.desktop
	@update-desktop-database $(INSTALL_APP) 2>/dev/null || true

.PHONY: all rebuild clean install uninstall

clean:
	rm -rf $(BUILDDIR)
