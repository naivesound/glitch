VERSION = 0.0.0

GLITCH_BIN ?= glitch

CPPFLAGS = -DVERSION=\"${VERSION}\" -g
CFLAGS += -std=c99 -pedantic -Wall -Wextra -Wno-missing-field-initializers -g
CXXFLAGS += -std=c++0x

OBJS := src/main.o src/glitch.o \
	src/vendor/RtAudio.o src/vendor/RtMidi.o

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
	CXXFLAGS += -D__WINDOWS_WASAPI__ -D__WINDOWS_MM__
	LDFLAGS += -lole32 -lm -lksuser -lwinmm -lws2_32 -mwindows -static
endif

$(GLITCH_BIN): $(OBJS)
	$(CXX) $^ -o $@ $(LDFLAGS)

test: src/glitch_test.o
	$(CXX) $^ -o glitch_test
	./glitch_test
	rm -f glitch_test
src/glitch_test.o: src/glitch_test.c src/glitch.c src/glitch.h

# Compile glitch code to asm.js and webassembly
web: src/glitch.c src/glitch.h src/expr.h src/piano.h src/tr808.h src/math_lut.h
	mkdir -p _tmp/js _tmp/wasm
	docker run --rm -v $(shell pwd):/src naivesound/emcc \
		emcc src/glitch.c -o _tmp/js/glitchcore.js \
		-s EXPORTED_FUNCTIONS="['_glitch_create','_glitch_destroy','_glitch_compile',\
			'_glitch_eval','_glitch_xy','_glitch_sample_rate','_glitch_midi']" -O3
	docker run --rm -v $(shell pwd):/src naivesound/emcc \
		emcc src/glitch.c -o _tmp/wasm/glitchcore.html \
		-s WASM=1 \
		-s EXPORTED_FUNCTIONS="['_glitch_create','_glitch_destroy','_glitch_compile',\
			'_glitch_eval','_glitch_xy','_glitch_sample_rate','_glitch_midi']" -O3
	mv -f _tmp/js/glitchcore.js _tmp/js/glitchcore.js.mem web
	mv -f _tmp/wasm/glitchcore.wasm web
	mv -f _tmp/wasm/glitchcore.js web/glitchcore-loader.js
	rm -f _tmp/wasm/glitchcore.html
	rmdir _tmp/js _tmp/wasm
	rmdir  _tmp

android: src/glitch.c src/glitch.h src/expr.h src/piano.h src/tr808.h src/math_lut.h
	cp $^ android/app/src/main/cpp
	cd android && ./gradlew build

clean:
	rm -f $(GLITCH_BIN) *.o src/*.o src/vendor/*.o

.PHONY: clean test web android

