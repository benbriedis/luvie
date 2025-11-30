# USE:  "bear -- make"   for neovim

CC = g++

#cf using:  fltk-config --ldstaticflags
FLTK_CONFIG = ../deps/installation/bin/fltk-config
CXXFLAGS = `$(FLTK_CONFIG) --cxxflags`
LDFLAGS = `$(FLTK_CONFIG) --ldflags` `$(FLTK_CONFIG) --libs`

TARGET = main

all: $(TARGET)

$(TARGET): main.cpp grid.cpp
	$(CC) $(CXXFLAGS) main.cpp grid.cpp -o $(TARGET) $(LDFLAGS) 

clean:
	rm -f $(TARGET)

