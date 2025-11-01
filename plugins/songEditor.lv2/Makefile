BUNDLE = build/songEditor.lv2
#TODO check these flags
#C_FLAGS = -g -DDEBUG
C_FLAGS = -fPIC


all: $(BUNDLE)

prepare-build:
	mkdir --parent -- ./build

$(BUNDLE): build/songEditor.so
	mkdir -- "$(BUNDLE)"
	cp -- manifest.ttl songEditor.ttl build/songEditor.so "$(BUNDLE)"

build/songEditor.so: prepare-build build/songEditor.o
	mkdir --parent -- build/
	gcc $(C_FLAGS) -shared build/songEditor.o \
		-lm -o build/songEditor.so

build/songEditor.o: prepare-build
	gcc $(C_FLAGS) src/songEditor.c -c -o build/songEditor.o


