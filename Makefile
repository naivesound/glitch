CFLAGS += -std=c99
LDGLAFS += -lm

all: glitch

glitch: glitch.o
	$(CC) $^ $(LDGLAFS) -o $@

glitch.o: glitch.c expr.h

run: glitch
	./glitch "$(shell cat a.glitch)" | aplay -r 44100 -f S16_LE -c 1

pcm:
	timeout 1s ./glitch "$(shell cat a.glitch)" > out.pcm

clean:
	rm -f glitch
	rm -f *.o
