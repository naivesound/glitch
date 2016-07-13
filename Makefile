CFLAGS += -std=c99 -g
CXXFLAGS += -g

all:
	@echo make glitch.alsa
	@echo make glitch.mac
	@echo make glitch.exe

glitch.alsa: main.o glitch.o RtAudio-alsa.o RtMidi-alsa.o
	$(CXX) $^ -g -pthread -lasound -lm -o $@
glitch.pulse: main.o glitch.o RtAudio-pulse.o RtMidi-alsa.o
	$(CXX) $^ -g -pthread -lasound -lpulse -lpulse-simple -lm -o $@
glitch.jack: main.o glitch.o RtAudio-jack.o RtMidi-jack.o
	$(CXX) $^ -g -pthread -ljack -lm -o $@

glitch.mac: main.o glitch.o RtAudio-coreaudio.o RtMidi-coreaudio.o
	$(CXX) $^ -g -pthread -framework CoreAudio -framework CoreMIDI -framework CoreFoundation -o $@

glitch.exe: main.o glitch.o RtAudio-wasapi.o RtMidi-winmm.o
	$(CXX) $^ -g -o $@ -lole32 -lm -lksuser -lwinmm -lws2_32 -mwindows -static

main.o: main.cpp glitch.h sys.h
glitch.o: glitch.c expr.h glitch.h

RtMidi-alsa.o: RtMidi.cpp RtMidi.h
	$(CXX) -c $< $(CXXFLAGS) -D__LINUX_ALSA__ -o $@
RtMidi-jack.o: RtMidi.cpp RtMidi.h
	$(CXX) -c $< $(CXXFLAGS) -D__UNIX_JACK__ -o $@
RtMidi-winmm.o: RtMidi.cpp RtMidi.h
	$(CXX) -c $< $(CXXFLAGS) -D__WINDOWS_MM__ -o $@
RtMidi-coreaudio.o: RtMidi.cpp RtMidi.h
	$(CXX) -c $< $(CXXFLAGS) -D__MACOSX_CORE__ -o $@

RtAudio-alsa.o: RtAudio.cpp RtAudio.h
	$(CXX) -c $< $(CXXFLAGS) -D__LINUX_ALSA__ -o $@
RtAudio-pulse.o: RtAudio.cpp RtAudio.h
	$(CXX) -c $< $(CXXFLAGS) -D__LINUX_PULSE__ -o $@
RtAudio-jack.o: RtAudio.cpp RtAudio.h
	$(CXX) -c $< $(CXXFLAGS) -D__UNIX_JACK__ -o $@
RtAudio-coreaudio.o: RtAudio.cpp RtAudio.h
	$(CXX) -c $< $(CXXFLAGS) -D__MACOSX_CORE__ -o $@
RtAudio-wasapi.o: RtAudio.cpp RtAudio.h
	$(CXX) -c $< -D__WINDOWS_WASAPI__ -I. -o $@

clean:
	rm -f glitch.alsa glitch.pulse glitch.exe glitch.mac glitch.jack
	rm -f *.o
