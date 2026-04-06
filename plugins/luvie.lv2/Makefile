CC      = gcc
CXX     = g++-13

FLTK_CONFIG  = ../../deps/installation/bin/fltk-config
FLTK_CXXFLAGS := $(shell $(FLTK_CONFIG) --cxxflags)
FLTK_LDFLAGS  := $(shell $(FLTK_CONFIG) --ldflags) $(shell $(FLTK_CONFIG) --libs)

LV2_INC  = -I../../deps/sources/lv2/include
SRC_INC  = -I../../src
SRC_LIB  = ../../src/build/libluvie.a

JACK_CFLAGS := $(shell pkg-config --cflags jack 2>/dev/null)
JACK_LIBS   := $(shell pkg-config --libs   jack 2>/dev/null)

CFLAGS   = -std=c11   -Wall -Wextra -fPIC -O2 $(LV2_INC)
CXXFLAGS = -std=c++23 -Wall -Wextra -fPIC -O2 $(LV2_INC) $(SRC_INC) $(FLTK_CXXFLAGS) $(JACK_CFLAGS)

DSP_TARGET = luvie_dsp.so
UI_TARGET  = luvie_ui.so

.PHONY: all clean

all: $(DSP_TARGET) $(UI_TARGET)

$(DSP_TARGET): luvie_dsp.o
	$(CC) -shared -fPIC -lm -o $@ $^

$(UI_TARGET): luvie_ui.o $(SRC_LIB)
	$(CXX) -shared -fPIC -o $@ $^ $(FLTK_LDFLAGS) $(JACK_LIBS)

luvie_dsp.o: luvie_dsp.c
	$(CC) $(CFLAGS) -c -o $@ $<

luvie_ui.o: luvie_ui.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f luvie_dsp.o luvie_ui.o $(DSP_TARGET) $(UI_TARGET)
