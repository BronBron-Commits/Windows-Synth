CC=gcc
CFLAGS=-O2 -Wall
LIBS=`sdl2-config --cflags --libs`

SRC=src/main.c src/synth.c
OUT=build/synth.exe

all:
	mkdir -p build
	$(CC) $(SRC) $(CFLAGS) $(LIBS) -o $(OUT)

clean:
	rm -rf build
