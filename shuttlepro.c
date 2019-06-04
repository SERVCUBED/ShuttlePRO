
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

#define ISSET(var, pos) ((var)&(1<<(pos)))

typedef struct input_event EV[MAX_EV_NUM];

unsigned short jogvalue = 0;
int shuttlevalue = 0;
uint16_t buttonsPressed = 0;

#define ISPRESSED(btn) ISSET(buttonsPressed, btn)

#define NUM_KEYMAP_COMMANDS 4
typedef struct _keymapcommand {
    void (*f)(char *);
    char *arg;
} keymapCommand;

void _printf(char *a) { printf("%s\n", a); }
keymapCommand keymapCommands[NUM_KEYMAP_COMMANDS] = { { &_printf, "Command 1"}, { &_printf, "Command 2"}};

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
const keymapValue keymap[NUM_KEYMAPS][NUM_KEYS] = {
    { { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } },
    { { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } },
    { { 1, 1 }, { 3, 1 }, { 0, 3 }, { 1, 3 }, { XK_Pointer_Button1, 0 }, { XK_Pointer_DfltBtnPrev, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } } };

Display *display;

void
initdisplay (void)
{
  int event, error, major, minor;

  display = XOpenDisplay (0);
  if (!display)
    {
      fprintf (stderr, "unable to open X display\n");
      exit (1);
    }
  if (!XTestQueryExtension (display, &event, &error, &major, &minor))
    {
      fprintf (stderr, "Xtest extensions not supported\n");
      XCloseDisplay (display);
      exit (1);
    }
}

inline const keymapValue *getKeymap(uint16_t code)
{
  int i = 0;
  for (; i < NUM_KEYMAPS - 1; ++i)
    {
      // We can see the first element is statically all zero, so compare with it
      if (unlikely(buttonsPressed & keymapsMask[i] && memcmp (&keymap[0][0], &keymap[i][code], sizeof (keymapValue)) != 0))
        return &keymap[i][code];
    }
  return &keymap[i][code];
}

inline void
send_button (unsigned int button, unsigned int press)
{
  printf ("send_button (%d, %d)\n", button, press);
  XTestFakeButtonEvent (display, button, press, DELAY);
}

void
send_button_to_active (unsigned int button, unsigned int press)
{
  // TODO get active window and use XSendEvent
  XTestFakeButtonEvent (display, button, press, DELAY);
}

inline void
send_key (KeyCode keycode, unsigned int press)
{
  XTestFakeKeyEvent (display, keycode, press, DELAY);
}

void
key (unsigned short code, unsigned int value)
{
  code -= EVENT_CODE_KEY1;

  if (likely(code <= NUM_KEYS))
    {
      const keymapValue *km;
      fprintf (stdout, "Key: %d %d\n", code, value);

      km = getKeymap (code);
      switch (km->code)
        {
          case 0b00:
            if (!km->value)
              {
                fprintf (stderr, "key(%d) unbound\n", code);
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
            if (value)
              {
                keymapCommand kc = keymapCommands[km->value];
                kc.f (kc.arg);
              }
            return;
        }
      // Switch to XSync instead?
      XFlush (display);
    }
  else
    {
      fprintf (stderr, "key(%d, %d) out of range\n", code + EVENT_CODE_KEY1, value);
    }
}

void
shuttle (int value)
{
  fprintf (stdout, "Shuttle: %d\n", value);
}

void
jog (bool direction)
{
  fprintf (stdout, "Jog: %d\n", direction);
  send_button (direction? 4 : 5, 1);
  send_button (direction? 4 : 5, 0);
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
                if (((uint) ev[i].value ^ (uint) jogvalue) & 1u)
                  {
                    jog (ev[i].value > jogvalue);
                  }
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
  fprintf (stderr, "Usage: %s [-h] [-o] [-p] [-d[rsk]] [device]\n", progname);
  fprintf (stderr, "-h print this message\n");
  fprintf (stderr, "-p enable hot-plugging\n");
  fprintf (stderr, "-d debug (r = regex, s = strokes, k = keys; default: all)\n");
  fprintf (stderr, "device, if specified, is the name of the shuttle device to open.\n");
  fprintf (stderr, "Otherwise the program will try to find a suitable device on its own.\n");
}

#include <glob.h>

// Helper functions to process the command line, so that we can pass it to
// Jack session management.

static char *command_line;
static size_t len;

static void add_command (char *arg)
{
  char *a = arg;
  // Do some simplistic quoting if the argument contains blanks. This won't do
  // the right thing if the argument also contains quotes. Oh well.
  if ((strchr (a, ' ') || strchr (a, '\t')) && !strchr (a, '"'))
    {
      a = malloc (strlen (arg) + 3);
      sprintf (a, "\"%s\"", arg);
    }
  if (!command_line)
    {
      len = strlen (a);
      command_line = malloc (len + 1);
      strcpy (command_line, a);
    }
  else
    {
      size_t l = strlen (a) + 1;
      command_line = realloc (command_line, len + l + 1);
      command_line[len] = ' ';
      strcpy (command_line + len + 1, a);
      len += l;
    }
  if (a != arg) free (a);
}

static char *absolute_path (char *name)
{
  if (*name == '/')
    {
      return name;
    }
  else
    {
      // This is a relative pathname, we turn it into a canonicalized absolute
      // path.  NOTE: This requires glibc. We should probably rewrite this code
      // to be more portable.
      char *pwd = getcwd (NULL, 0);
      if (!pwd)
        {
          perror ("getcwd");
          return name;
        }
      else
        {
          char *path = malloc (strlen (pwd) + strlen (name) + 2);
          static char abspath[PATH_MAX];
          sprintf (path, "%s/%s", pwd, name);
          if (!realpath (path, abspath)) strcpy (abspath, path);
          free (path);
          free (pwd);
          return abspath;
        }
    }
}

uint8_t quit = 0;

void quit_callback ()
{
  quit = 1;
}

int
main (int argc, char **argv)
{
  EV ev;
  int evsize = sizeof (struct input_event);
  int nread;
  char *dev_name;
  int fd;
  int first_time = 1;
  int opt, hotplug = 0;

  // Start recording the command line to be passed to Jack session management.
  add_command (argv[0]);

  while ((opt = getopt (argc, argv, "hopd::r:")) != -1)
    {
      switch (opt)
        {
          case 'h':
            help (argv[0]);
          exit (0);
          case 'p':
            hotplug = 1;
          add_command ("-p");
          break;
          case 'o':
            break;
          case 'd':
            if (optarg && *optarg)
              {
                const char *a = optarg;
                char buf[100];
                snprintf (buf, 100, "-d%s", optarg);
                add_command (buf);
                while (*a)
                  {
                    switch (*a)
                      {
                        case 'r':
//                          default_debug_regex = 1;
                        break;
                        case 's':
//                          default_debug_strokes = 1;
                        break;
                        case 'k':
//                          default_debug_keys = 1;
                        break;
                        default:
                          fprintf (stderr, "%s: unknown debugging option (-d), must be r, s, or k\n", argv[0]);
                        fprintf (stderr, "Try -h for help.\n");
                        exit (1);
                      }
                    ++a;
                  }
              }
            else
              {
//                default_debug_regex = default_debug_strokes = default_debug_keys = 1;
                add_command ("-d");
              }
          break;
          case 'j':
            break;
          case 'r':
          add_command ("-r");
          // We need to convert this to an absolute pathname for Jack session
          // management.
          add_command (absolute_path (optarg));
          break;
          default:
            fprintf (stderr, "Try -h for help.\n");
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
          fprintf (stderr, "%s: found no suitable shuttle device\n", argv[0]);
          fprintf (stderr, "Please make sure that your shuttle device is connected.\n");
          fprintf (stderr, "You can also specify the device name on the command line.\n");
          fprintf (stderr, "Try -h for help.\n");
          exit (1);
        }
      else
        {
          dev_name = globbuf.gl_pathv[0];
          fprintf (stderr, "%s: found shuttle device:\n%s\n", argv[0], dev_name);
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
      fd = open (dev_name, O_RDONLY);
      if (fd < 0)
        {
          perror (dev_name);
          if (first_time)
            {
              exit (1);
            }
        }
      else
        {
          // Flag it as exclusive access
          if (ioctl (fd, EVIOCGRAB, 1) < 0)
            {
              perror ("evgrab ioctl");
            }
          else
            {
              first_time = 0;
              while (!quit)
                {
                  nread = read (fd, &ev, sizeof (ev) * MAX_EV_NUM);
                  if (nread < 0 && (hotplug || quit))
                    {
                      quit = 1;
                      break;
                    }
                  if (nread < evsize)
                    {
                      if (nread < 0)
                        {
                          perror ("read event");
                          break;
                        }
                      else
                        {
                          fprintf (stderr, "short read: %d\n", nread);
                          break;
                        }
                    }
                  else
                    {
                      handle_event (ev, nread / evsize);
                    }
                }
            }
        }
      close (fd);
      if (!quit) sleep (1);
    }
}
