OBJS = main.o gfx.o vec3.o effects.o sfx.o version.o world.o system.o game.o video.o ping.o
CXXFLAGS = -O2 -g -W -Wall `sdl-config --cflags` -std=c++0x
LDFLAGS = -O2 `sdl-config --libs` -lGL -lGLU -lGLEW -lpng -lvorbisfile -logg -ltheoradec
CXX = g++
CC = gcc
DATA = $(wildcard data/*)

kaal: $(OBJS) kaal.dat
	$(CXX) -o $@ $(OBJS) $(LDFLAGS)

kaal.dat: $(DATA)
	./create-pack.py $(DATA)

version.c: $(wildcard *.cc) $(wildcard *.h)
	@echo "const char *version = \"$(shell git rev-parse HEAD|cut -c1-6)_$(shell date -I)\";" >version.c
