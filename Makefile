BUNDLE = build/luvie.lv2
#TODO check these flags
#C_FLAGS = -g -DDEBUG
C_FLAGS = -fPIC


all: $(BUNDLE)

prepare-build:
	mkdir --parent -- ./build

$(BUNDLE): build/luvie.so
	mkdir -- "$(BUNDLE)"
	cp -- manifest.ttl luvie.ttl build/luvie.so "$(BUNDLE)"

build/luvie.so: prepare-build build/luvie.o
	mkdir --parent -- build/
	gcc $(C_FLAGS) -shared build/luvie.o \
		-lm -o build/luvie.so

build/luvie.o: prepare-build
	gcc $(C_FLAGS) src/luvie.c -c -o build/luvie.o


