
# Copyright 2013 Eric Messick (FixedImagePhoto.com/Contact)

#CFLAGS=-g -W -Wall
CFLAGS=-O3 -W -Wall

INSTALL_DIR=/usr/local/bin

# Check to see whether we have Jack installed. Needs pkg-config.
JACK := $(shell pkg-config --libs jack 2>/dev/null)

ifneq ($(JACK),)
JACK_DEF = -DHAVE_JACK=1
JACK_OBJ = jackdriver.o
endif

CPPFLAGS += $(JACK_DEF)

OBJ = readconfig.o shuttlepro.o $(JACK_OBJ)

all: shuttlepro

install: all
	install shuttle shuttlepro ${INSTALL_DIR}

shuttlepro: ${OBJ}
	gcc ${CFLAGS} ${OBJ} -o shuttlepro -L /usr/X11R6/lib -lX11 -lXtst $(JACK)

clean:
	rm -f shuttlepro keys.h $(OBJ)

keys.h: keys.sed /usr/include/X11/keysymdef.h
	sed -f keys.sed < /usr/include/X11/keysymdef.h > keys.h

readconfig.o: shuttle.h keys.h
shuttlepro.o: shuttle.h
