
/*

 Contour ShuttlePro v2 interface

 Copyright 2013 Eric Messick (FixedImagePhoto.com/Contact)

 Copyright 2018 Albert Graef <aggraef@gmail.com>, various improvements

 Copyright 2019 Ben Blain <servc.eu/contact>, major overhaul and personalisations

 Based on a version (c) 2006 Trammell Hudson <hudson@osresearch.net>

 which was in turn

 Based heavily on code by Arendt David <admin@prnet.org>

*/

#include "shuttle.h"

#define ISSET(var, pos) ((var)&(1u<<(pos)))

typedef struct input_event EV[MAX_EV_NUM];

unsigned short jogvalue = 0;
short shuttlevalue = 0;
uint16_t buttonsPressed = 0;

unsigned int kbd_interval = 200;

#define ISPRESSED(btn) ISSET(buttonsPressed, btn)

#define NUM_KEYMAP_COMMANDS 4
typedef struct keymapCommand {
    void (*f) (char *, uint);
    char *arg;
} keymapCommand;

void _printf (char *a, uint v)
{ printf ("%s, Value: %i", a, v); }
keymapCommand keymapCommands[NUM_KEYMAP_COMMANDS] = {{&_printf, "Command 1"},
                                                     {&_printf, "Command 2"}/*,
                                                     {&_printf, "Command 3"},
                                                     {&_printf, "Command 4"}*/};

typedef struct _keymapvalue {
    uint value:29;
    uint code:2;
} keymapValue; // __attribute__((packed))

#define NUM_KEYMAPS 3
#define KMV_CODE_KEY = 0
#define KMV_CODE_BUTTON_CURRENT_WINDOW = 1
#define KMV_CODE_BUTTON_POINTER_WINDOW = 2
#define KMV_CODE_COMMAND = 3

const uint16_t keymapsMask[NUM_KEYMAPS - 1] = {1u << 10u, 1u << 9u};
// NOTE: Must keep the very first element zeroed (see getKeymap).
const keymapValue keymap[NUM_KEYMAPS][NUM_KEYS] = {
    {{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0,  0}, {0, 0}, {0, 0}},
    {{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {1, 1}, {0, 0}, {0, 0}, {0, 0}, {0,  0}, {0, 0}, {0, 0}},
    {{8, 1}, {1, 1}, {3, 1}, {9, 1}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {37, 0}, {0, 0}, {0, 0}}};

Display *display;

void
initdisplay (void)
{
  int event, opcode, error, major, minor;
  unsigned int kbd_delay;

  display = XOpenDisplay (0);
  if (!display)
    {
      errf ("unable to open X display");
      exit (1);
    }
  if (!XTestQueryExtension (display, &event, &error, &major, &minor))
    {
      errf ("Xtest extensions not supported.");
      XCloseDisplay (display);
      exit (1);
    }
  if (XkbQueryExtension (display, &opcode, &event, &error, &major, &minor))
    XkbGetAutoRepeatRate (display, XkbUseCoreKbd, &kbd_delay, &kbd_interval);
  else
    {
      errf ("Xkb extensions not supported. Continuing with default values.");
    }
}

const keymapValue *getKeymap (uint16_t code)
{
  int i = 0;
  for (; i < NUM_KEYMAPS - 1; ++i)
    {
      // We can see the first element is statically all zero, so compare with it
      if (unlikely(
          buttonsPressed & keymapsMask[i] && memcmp (&keymap[0][0], &keymap[i][code], sizeof (keymapValue)) != 0))
        return &keymap[i][code];
    }
  return &keymap[i][code];
}

void
send_button (unsigned int button, unsigned int press)
{
  prnf ("send_button (%d, %d)", button, press);
  XTestFakeButtonEvent (display, button, press, DELAY);
}

void
send_button_to_active (unsigned int button, unsigned int press)
{
  // TODO get active window and use XSendEvent
  XTestFakeButtonEvent (display, button, press, DELAY);
}

void
send_key (unsigned int keycode, unsigned int press)
{
  prnf ("send_key (%d, %d)", keycode, press);
  XTestFakeKeyEvent (display, keycode, press, DELAY);
}

void
key (unsigned short code, unsigned int value)
{
  code -= EVENT_CODE_KEY1;

  if (likely(code <= NUM_KEYS))
    {
      const keymapValue *km;
      prnf ("Key: %d %d", code, value);

      if (value)
        buttonsPressed |= (1u << code);
      else
        buttonsPressed ^= (1u << code);
      km = getKeymap (code);
      switch (km->code)
        {
          case 0b00:
            if (!km->value)
              {
                errf ("key(%d) unbound", code);
                return;
              }
          send_key (km->value, value);
          break;
          case 0b01:
            send_button (km->value, value);
          break;
          case 0b10:
            send_button_to_active (km->value, value);
          break;
          case 0b11:
            {
              keymapCommand kc = keymapCommands[km->value];
              kc.f (kc.arg, value);
              return; // Commands must flush the server themselves
            }
        }
      // Switch to XSync instead?
      XFlush (display);
    }
  else
    {
      errf ("key(%d, %d) out of range", code + EVENT_CODE_KEY1, value);
    }
}

void
shuttle (int value)
{
  prnf ("Shuttle: %d, last: %d", value, shuttlevalue);
  // if value = 0 stop timer
  // [if last value = 0,] start timer with delay = kbd_interval * (1-|value|/7)

  // For now, just use shuttle to scroll quickly
  if (value == 0)
    return;

  // TODO This temp variable should be optimised out somehow. I don't like it here
  uint8_t btn = (value > 0) ? 5 : 4;
  for (int i = 0; i < abs (value); ++i)
    {
      send_button (btn, 1);
      send_button (btn, 0);
    }
  XFlush (display);
}

void
jog (bool direction)
{
  prnf ("Jog: %d, %i", direction, jogvalue);
  if (ISPRESSED (10u))
    {
      if (!direction)
        send_key (50, 1);
      send_key (23, 1);
      send_key (23, 0);
      if (!direction)
        send_key (50, 0);
    }
  else
    {
      uint8_t button = ISPRESSED (9u) ? (direction ? 7 : 6) : (direction ? 5 : 4);
      send_button (button, 1);
      send_button (button, 0);
    }
  XFlush (display);
}

void
handle_event (EV ev, int count)
{
  bool hasWheelEV = false;
  for (int i = 0; i < count; ++i)
    {
      switch (ev[i].type)
        {
          case EV_SYN:
            return;
          case EV_REL:
            if (ev[i].code == REL_WHEEL) // Shuttle
              {
                hasWheelEV = true;
                if (shuttlevalue != ev[i].value)
                  {
                    shuttle (ev[i].value);
                    shuttlevalue = ev[i].value;
                  }

                break;
              }
          if (ev[i].code == REL_DIAL) // Jog
            {
              if (ev[i].value == jogvalue)
                {
                  if (!hasWheelEV && (uint) shuttlevalue & 1u && count < 3)
                    {
                      shuttle (0);
                      shuttlevalue = 0;
                    }
                  break;
                }
              if (((uint) ev[i].value ^ jogvalue) & 1u)
                jog (ev[i].value > jogvalue);
              else if ((jogvalue ^ (uint) ev[i].value) == 254u)
                // Quick fix for skipping zero jumps:
                // 255 -> 1, 1 -> 255
                jog (ev[i].value < jogvalue);

              prnf ("%d, %d", jogvalue, ev[i].value);
              jogvalue = ev[i].value;
            }
          break;
          case EV_KEY:
            key (ev[i].code, ev[i].value);
          break;
        }
    }
}

void help (char *progname)
{
  errf ("Usage: %s [-h] [-p] [device]", progname);
  errf ("-h print this message");
  errf ("-p enable hot-plugging");
  errf ("device, if specified, is the name of the shuttle device to open.");
  errf ("Otherwise the program will try to find a suitable device on its own.");
}

#include <glob.h>

uint8_t quit = 0;

void quit_callback ()
{
  quit = 1;
}

int
main (int argc, char **argv)
{
  EV ev;
  const int evsize = sizeof (struct input_event);
  char *dev_name;
  // Put these single bit fields in a structure to save memory
  struct mainopts o = {
      .first_time = 1,
      .hotplug = 0
  };
  int opt;

  while ((opt = getopt (argc, argv, "hp::")) != -1)
    {
      switch (opt)
        {
          case 'h':
            help (argv[0]);
          exit (0);
          case 'p':
            o.hotplug = 1;
          break;
          default:
            errf ("Try -h for help.");
          exit (1);
        }
    }

  if (optind + 1 < argc)
    {
      help (argv[0]);
      exit (1);
    }

  if (optind >= argc)
    {
      // Try to find a suitable device. The following glob pattern should
      // hopefully cover all existing versions.
      glob_t globbuf;
      if (glob ("/dev/input/by-id/usb-*Shuttle*-event-if*",
                0, NULL, &globbuf))
        {
          errf ("%s: found no suitable shuttle device", argv[0]);
          errf ("Please make sure that your shuttle device is connected.");
          errf ("You can also specify the device name on the command line.");
          errf ("Try -h for help.");
          exit (1);
        }
      else
        {
          dev_name = globbuf.gl_pathv[0];
          errf ("%s: found shuttle device:\n%s", argv[0], dev_name);
        }
    }
  else
    {
      dev_name = argv[optind];
    }

  initdisplay ();

  struct sigaction int_handler = {.sa_handler = quit_callback};
  sigaction (SIGINT, &int_handler, 0);
  while (!quit)
    {
      o.fd = open (dev_name, O_RDONLY);
      if (o.fd < 0)
        {
          perror (dev_name);
          if (o.first_time)
            {
              exit (1);
            }
        }
      else
        {
          // Flag it as exclusive access
          if (ioctl (o.fd, EVIOCGRAB, 1) < 0)
            {
              perror ("evgrab ioctl");
            }
          else
            {
              o.first_time = 0;
              while (!quit)
                {
                  o.nread = read (o.fd, &ev, sizeof (ev));
                  if (o.nread < 0 && (o.hotplug || quit))
                    {
                      quit = 1;
                      break;
                    }
                  if (o.nread < evsize)
                    {
                      if (o.nread < 0)
                        {
                          perror ("read event");
                          break;
                        }
                      else
                        {
                          errf ("short read: %d", o.nread);
                          break;
                        }
                    }
                  else
                    {
                      handle_event (ev, o.nread / evsize);
                    }
                }
            }
        }
      close (o.fd);
      if (!quit) sleep (1);
    }
}
