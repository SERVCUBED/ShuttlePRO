
// Copyright 2013 Eric Messick (FixedImagePhoto.com/Contact)
// Copyright 2018 Albert Graef <aggraef@gmail.com>
// Copyright 2019 Ben Blain <servc.eu/contact>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#include <linux/input.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <errno.h>
#include <time.h>

#include <regex.h>

#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#include <X11/XKBlib.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>

#define unlikely(expr) __builtin_expect(!!(expr), 0)
#define   likely(expr) __builtin_expect(!!(expr), 1)

// delay in ms before processing each XTest event
// CurrentTime means no delay
#define DELAY CurrentTime

#define MAX_EV_NUM 6

// protocol for events from the shuttlepro HUD device
//
// ev.type values:
// EV_SYN 0
// EV_KEY 1
// EV_REL 2
// EV_MSC 4

/*
 * Button:
 * EV_REL REL_DIAL same
 * EV_MSC MSC_SCAN
 * EV_KEY code val/1
 * EV_SYN
 *
 * Side button, shuttle release:
 * EV_REL REL_DIAL same
 * EV_SYN
 * Shuttle release if |shuttevalue|=1
 *
 * Jog:
 * EV_REL REL_DIAL +-1
 * EV_SYN
 *
 * Shuttle:
 * EV_REL REL_WHEEL val/7,+-1
 * EV_REL 11 val/840 ignore
 * EV_REL REL_DIAL same or +-1
 * EV_SYN
 */

// ev.code when ev.type == KEY
#define EVENT_CODE_KEY1 256
// KEY2 257, etc...

// ev.value when ev.type == KEY
// 1 -> PRESS; 0 -> RELEASE

// ev.code when ev.type == JOGSHUTTLE
#define EVENT_CODE_JOG 7
#define EVENT_CODE_SHUTTLE 8
#define EVENT_CODE_UNKNOWN 11

// ev.value when ev.code == JOG
// 8 bit value changing by one for each jog step

// ev.value when ev.code == SHUTTLE
// -7 .. 7 encoding shuttle position

// we define these as extra KeySyms to represent mouse events
#define XK_Button_0 0x2000000 // just an offset, not a real button
#define XK_Button_1 0x2000001
#define XK_Button_2 0x2000002
#define XK_Button_3 0x2000003
#define XK_Button_8 0x2000008
#define XK_Button_9 0x2000009
#define XK_Scroll_Up 0x2000004
#define XK_Scroll_Down 0x2000005

#define PRESS 1
#define RELEASE 2
#define PRESS_RELEASE 3
#define HOLD 4

#define NUM_KEYS 13
#define NUM_SHUTTLES 15
#define NUM_SHUTTLE_INCRS 2
#define NUM_JOGS 2

typedef struct _stroke {
  struct _stroke *next;
  // nonzero keysym indicates a key event
  KeySym keysym;
  int press; // zero -> release, non-zero -> press
  // keysym == 0 => MIDI event
  int status, data; // status and, if applicable, first data byte
  // the incremental bit indicates an incremental control change (typically
  // used with endless rotary encoders) to be represented as a sign bit value
  uint8_t incr;
  // the dirty bit indicates a MIDI event for which a release event still
  // needs to be generated in key events
  uint8_t dirty;
} stroke;

#define KJS_KEY_DOWN 1
#define KJS_KEY_UP 2
#define KJS_SHUTTLE 3
#define KJS_SHUTTLE_INCR 4
#define KJS_JOG 5

typedef struct _translation {
  struct _translation *next;
  char *name;
  int is_default;
  regex_t regex;
  stroke *key_down[NUM_KEYS];
  stroke *key_up[NUM_KEYS];
  stroke *shuttle[NUM_SHUTTLES];
  stroke *shuttle_incr[NUM_SHUTTLE_INCRS];
  stroke *jog[NUM_JOGS];
} translation;

extern void print_stroke_sequence(char *name, char *up_or_down, stroke *s);
extern int debug_regex, debug_strokes, debug_keys;
extern int default_debug_regex, default_debug_strokes, default_debug_keys;
