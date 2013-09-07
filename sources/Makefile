CPP  = g++
EXECUTABLE = MIDItrk
SOURCES = $(EXECUTABLE).cpp RtMidi.cpp
REMOVE = rm -f
COPY = cp -f
ICON = MIDITRKicon.xpm
MENUENTRY = $(EXECUTABLE).desktop
BINDIR = ../
APPDIR = /opt/miditrk/bin/
ICONDIR = /opt/miditrk/share/icons/
MENUDIR = /opt/miditrk/share/applications

.PHONY: all all-before all-after clean install

all: all-before $(EXECUTABLE) all-after

$(EXECUTABLE): $(EXECUTABLE).cpp
	$(CPP) $(SOURCES) -o $(BINDIR)/$(EXECUTABLE) -Wall -D__MACOSX_CORE__ `sdl-config -cflags` -framework CoreMIDI -framework CoreAudio -framework CoreFoundation `sdl-config --libs`
#for Linux (ALSA): g++ -Wall -D__LINUX_ALSA__ -o midiprobe midiprobe.cpp RtMidi.cpp -lasound -lpthread
#for Jack (Linux/OSX): g++ -Wall -D__UNIX_JACK__ -o midiprobe midiprobe.cpp RtMidi.cpp -ljack
#for OSX: g++ -Wall -D__MACOSX_CORE__ -o midiprobe midiprobe.cpp RtMidi.cpp -framework CoreMIDI -framework CoreAudio -framework CoreFoundation

#aid to convert binaries into array (e.g. charset/bitmap/etc.):
bin2array: bin2array.c
	$(CPP) bin2array.c -o bin2array


clean:
	${REMOVE} $(BINDIR)/$(EXECUTABLE)
	${REMOVE} $(APPDIR)/$(EXECUTABLE)
	${REMOVE} $(ICONDIR)/$(ICON)
	${REMOVE} $(MENUDIR)/$(MENUENTRY)

install:
	$(COPY) $(BINDIR)/$(EXECUTABLE) $(APPDIR)
	$(COPY) $(ICON) $(ICONDIR)
	$(COPY) $(MENUENTRY) $(MENUDIR)
	chown 777 $(APPDIR)/$(EXECUTABLE)
