all:
	gcc pasa.c -std=gnu99 -Wall -o pasa -lm -lncurses -lpulse -lpulse-simple -lfftw3

clean:
	rm pasa
