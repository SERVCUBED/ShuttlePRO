
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

#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#include <X11/XKBlib.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>

#define unlikely(expr) __builtin_expect(!!(expr), 0)
#define   likely(expr) __builtin_expect(!!(expr), 1)

#define errf(f, args...) fprintf(stderr, f, ##args)
#define prnf(f, args...) fprintf(stdout, f, ##args)

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

#define NUM_KEYS 13

#pragma pack(1)
typedef struct mainopts
{
    int nread, fd;
    unsigned first_time:1;
    unsigned hotplug:1;
} mainopts;
#pragma pack(0)
