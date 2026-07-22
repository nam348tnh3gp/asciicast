COMPILER = gcc

all: asciicast

asciicast.o: asciicast.h asciicast.c
	${COMPILER} asciicast.c -ljpeg -c

asciicast: main.c asciicast.o
	${COMPILER} main.c asciicast.o -ljpeg -o asciicast

clean:
	rm -f *.o asciicast
