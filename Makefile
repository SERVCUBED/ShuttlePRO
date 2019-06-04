
# Copyright 2013 Eric Messick (FixedImagePhoto.com/Contact)
# Copyright 2018 Albert Graef (aggraef@gmail.com)
# Copyright 2019 Ben Blain (servc.eu/contact)

#CFLAGS=-g -W -Wall
CFLAGS=-O3 -W -Wall

prefix=/usr/local
bindir=$(DESTDIR)$(prefix)/bin
mandir=$(DESTDIR)$(prefix)/share/man/man1
datadir=$(DESTDIR)/etc
udevdir=$(DESTDIR)/etc/udev/rules.d

# Check to see whether we have Jack installed. Needs pkg-config.
# JACK := $(shell pkg-config --libs jack 2>/dev/null)


#CPPFLAGS += $(JACK_DEF)

OBJ = shuttlepro.o

# Only try to install the manual page if it's actually there, to prevent
# errors if pandoc isn't installed.
INSTALL_TARGETS = shuttlepro $(wildcard shuttlepro.1)

.PHONY: all install uninstall man pdf clean realclean

all: shuttlepro

install: all
	install -d $(bindir) $(datadir) $(mandir)
	install shuttlepro $(bindir)
	install -m 0644 example.shuttlerc $(datadir)/shuttlerc
# If present, the manual page will be installed along with the program.
ifneq ($(findstring shuttlepro.1, $(INSTALL_TARGETS)),)
	install -m 0644 shuttlepro.1 $(mandir)
else
	@echo "Manual page not found, create it with 'make man'."
endif

uninstall:
	rm -f $(bindir)/shuttlepro $(mandir)/shuttlepro.1 $(datadir)/shuttlerc

shuttlepro: $(OBJ)
	gcc $(CFLAGS) $(OBJ) -o shuttlepro -L /usr/X11R6/lib -lX11 -lXtst

# This creates the manual page from the README. Requires pandoc
# (http://pandoc.org/).
man: shuttlepro.1

# Manual page in pdf format. This also needs groff.
pdf: shuttlepro.pdf

shuttlepro.1: README.md
	pandoc -s -tman $< > $@

shuttlepro.pdf: shuttlepro.1
# This assumes that man does the right thing when given a file instead of a
# program name, and that it understands groff's -T option.
	man -Tpdf ./shuttlepro.1 > $@

# Shamanon's udev hotplugging. *Only* install these if you're know what you're
# doing! (See the Hotplugging section in the manual for details.)
udev-rules = $(patsubst %.rules.in, %.rules, $(wildcard udev/*.rules.in))

%.rules: %.rules.in
	sed -e 's:@prefix@:$(prefix):g' < $< > $@

udev/shuttle-hotplug: udev/shuttle-hotplug.in
	sed -e 's:@prefix@:$(prefix):g' < $< > $@

install-udev: $(udev-rules) udev/shuttle-hotplug
	install -d $(bindir) $(udevdir)
	install udev/shuttle-hotplug $(bindir)
	install -m 0644 $(udev-rules) $(udevdir)
	rm -f $(udev-rules) udev/shuttle-hotplug

uninstall-udev:
	rm -f $(bindir)/shuttle-hotplug $(addprefix $(udevdir)/, $(notdir $(udev-rules)))

clean:
	rm -f shuttlepro keys.h $(OBJ)

realclean:
	rm -f shuttlepro shuttlepro.1 shuttlepro.pdf keys.h $(OBJ)

keys.h: keys.sed /usr/include/X11/keysymdef.h
	sed -f keys.sed < /usr/include/X11/keysymdef.h > keys.h

readconfig.o: shuttle.h keys.h

shuttlepro.o: shuttle.h
