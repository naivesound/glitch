CFLAGS += -std=c99 -g
CXXFLAGS += -g

all:
	@echo make glitch.alsa
	@echo make glitch.mac
	@echo make glitch.exe

glitch.alsa: main.o glitch/glitch.o rt/RtAudio-alsa.o rt/RtMidi-alsa.o
	$(CXX) $^ -g -pthread -lasound -lm -o $@
glitch.pulse: main.o glitch/glitch.o rt/RtAudio-pulse.o rt/RtMidi-alsa.o
	$(CXX) $^ -g -pthread -lasound -lpulse -lpulse-simple -lm -o $@
glitch.jack: main.o glitch/glitch.o rt/RtAudio-jack.o rt/RtMidi-jack.o
	$(CXX) $^ -g -pthread -ljack -lm -o $@

glitch.mac: main.o glitch/glitch.o rt/RtAudio-coreaudio.o rt/RtMidi-coreaudio.o
	$(CXX) $^ -g -framework CoreAudio -framework CoreMIDI -framework CoreFoundation -o $@

glitch.exe: main.o glitch/glitch.o rt/RtAudio-wasapi.o rt/RtMidi-winmm.o
	$(CXX) $^ -g -o $@ -lole32 -lm -lksuser -lwinmm -lws2_32 -mwindows -static

main.o: main.cpp sys.h glitch/glitch.h
glitch/glitch.o: glitch/glitch.c glitch/expr.h glitch/glitch.h

rt/RtMidi-alsa.o: rt/RtMidi.cpp rt/RtMidi.h
	$(CXX) -c $< $(CXXFLAGS) -D__LINUX_ALSA__ -o $@
rt/RtMidi-jack.o: rt/RtMidi.cpp rt/RtMidi.h
	$(CXX) -c $< $(CXXFLAGS) -D__UNIX_JACK__ -o $@
rt/RtMidi-winmm.o: rt/RtMidi.cpp rt/RtMidi.h
	$(CXX) -c $< $(CXXFLAGS) -D__WINDOWS_MM__ -o $@
rt/RtMidi-coreaudio.o: rt/RtMidi.cpp rt/RtMidi.h
	$(CXX) -c $< $(CXXFLAGS) -D__MACOSX_CORE__ -o $@

rt/RtAudio-alsa.o: rt/RtAudio.cpp rt/RtAudio.h
	$(CXX) -c $< $(CXXFLAGS) -D__LINUX_ALSA__ -o $@
rt/RtAudio-pulse.o: rt/RtAudio.cpp rt/RtAudio.h
	$(CXX) -c $< $(CXXFLAGS) -D__LINUX_PULSE__ -o $@
rt/RtAudio-jack.o: rt/RtAudio.cpp rt/RtAudio.h
	$(CXX) -c $< $(CXXFLAGS) -D__UNIX_JACK__ -o $@
rt/RtAudio-coreaudio.o: rt/RtAudio.cpp rt/RtAudio.h
	$(CXX) -c $< $(CXXFLAGS) -D__MACOSX_CORE__ -o $@
rt/RtAudio-wasapi.o: rt/RtAudio.cpp rt/RtAudio.h
	$(CXX) -c $< -D__WINDOWS_WASAPI__ -Irt -o $@

asmjs: glitch/glitch.c glitch/glitch.h glitch/expr.h
	emcc glitch/glitch.c -o glitch/glitchcore.js \
		-s EXPORTED_FUNCTIONS="['_glitch_create','_glitch_destroy','_glitch_compile','_glitch_eval','_glitch_xy','_glitch_sample_rate']" -O3

clean:
	rm -f glitch.alsa glitch.pulse glitch.exe glitch.mac glitch.jack
	rm -f *.o rt/*.o glitch/*.o
