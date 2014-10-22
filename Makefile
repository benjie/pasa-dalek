all:
	gcc pasa.c -std=gnu99 -Wall -O3 -o pasa -lm -lncurses -lpulse -lpulse-simple -lfftw3

clean:
	rm pasa
