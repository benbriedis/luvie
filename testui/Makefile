# USE:  "bear -- make"   for neovim

CC = g++-13

#cf using:  fltk-config --ldstaticflags
FLTK_CONFIG = ../deps/installation/bin/fltk-config
#XXX not sure I want to use  -std=gnu++23
CXXFLAGS = `$(FLTK_CONFIG) --cxxflags` -std=c++23 -std=gnu++23 -fmodules-ts -g
LDFLAGS = `$(FLTK_CONFIG) --ldflags` `$(FLTK_CONFIG) --libs`

TARGET = main

all: $(TARGET)

$(TARGET): main.cpp grid.cpp outerGrid.cpp
	$(CC) $(CXXFLAGS) main.cpp outerGrid.cpp grid.cpp -o $(TARGET) $(LDFLAGS) 

clean:
	rm -f $(TARGET)

