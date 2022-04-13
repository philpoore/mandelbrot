
all: compile

compile:
	g++ -O2 -std=c++20 src/main.cpp -l sdl2 -o mandelbrot