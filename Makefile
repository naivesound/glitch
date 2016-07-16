VERSION = 0.0.0

CPPFLAGS = -DVERSION=\"${VERSION}\"
CFLAGS += -std=c99 -pedantic -Wall -Wextra -Wno-missing-field-initializers
CXXFLAGS +=

OBJS := src/main.o src/glitch.o src/RtAudio.o src/RtMidi.o

ifeq ($(alsa),1)
	CXXFLAGS += -D__LINUX_ALSA__
	LDFLAGS += -lasound -pthread
endif
ifeq ($(pulse),1)
	CXXFLAGS += -D__LINUX_PULSE__
	LDFLAGS += -lpulse -lpulse-simple -pthread
endif
ifeq ($(jack),1)
	CXXFLAGS += -D__UNIX_JACK__
	LDFLAGS += -ljack -pthread
endif

ifeq ($(macos),1)
	CXXFLAGS += -D__MACOSX_CORE__
	LDFLAGS += -framework CoreAudio -framework CoreMIDI -framework CoreFoundation
endif
ifeq ($(windows),1)
	CXXFLAGS += -D__WINDOWS_WASAPI__ -D__WINDOWS_MM__ -Isrc
	LDFLAGS += -lole32 -lm -lksuser -lwinmm -lws2_32 -mwindows -static
endif

glitch: $(OBJS)
	$(CXX) $^ -o $@ $(LDFLAGS)

# Compile glitch code to asm.js
js: src/glitch.c src/glitch.h src/expr.h
	mkdir -p js
	emcc src/glitch.c -o js/glitchcore.js \
		-s EXPORTED_FUNCTIONS="['_glitch_create','_glitch_destroy','_glitch_compile','_glitch_eval','_glitch_xy','_glitch_sample_rate']" -O3

clean:
	rm -f glitch *.o src/*.o rt/*.o

.PHONY: clean js

