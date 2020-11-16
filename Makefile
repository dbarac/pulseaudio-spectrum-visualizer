CC=gcc
LIBS= -lm -lpulse -lpulse-simple

spectrum_visualizer: spectrum_visualizer.c
	gcc spectrum_visualizer.c -o spectrum_visualizer $(LIBS)
