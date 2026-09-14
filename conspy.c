/*
 * conspy
 * ------
 *
 * A text-mode VNC like program for Linux virtual terminals.
 *
 * Author: Russell Stuart, russell-conspy@stuart.id.au
 *         22/05/2003
 *
 * To compile:
 *   gcc -Werror -Wall -Wextra -O2 --std=c99 -lncurses -o conspy conspy.c
 *
 *
 * License
 * -------
 *
 * Copyright (c) 2009-2014,2015,2016 Russell Stuart.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or (at
 * your option) any later version.
 *
 * The copyright holders grant you an additional permission under Section 7
 * of the GNU Affero General Public License, version 3, exempting you from
 * the requirement in Section 6 of the GNU General Public License, version 3,
 * to accompany Corresponding Source with Installation Information for the
 * Program or any work based on the Program. You are still required to
 * comply with all other Section 6 requirements to provide Corresponding
 * Source.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#define	_GNU_SOURCE
#include <ncursesw/curses.h>
#include <locale.h>
#include <wchar.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/kd.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <termios.h>
#include <term.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>

/* GNU/kFreeBSD has different #define's */
#if defined(__FreeBSD_kernel__)
#define K_UNICODE K_CODE
#define IUCLC 0
#endif

extern int errno;

/*
 * Version info.
 */
static char conspy_date[]	= "2016-02-16";
static char conspy_version[]	= "1.14";

/*
 * VGA colour definitions, as found in a nibble in an attribute
 * byte within VGA video memory.
 */
#define	VGA_BLACK	0x00
#define	VGA_BLUE	0x01
#define	VGA_GREEN	0x02
#define	VGA_CYAN	0x03
#define	VGA_RED		0x04
#define	VGA_MAGENTA	0x05
#define	VGA_YELLOW	0x06
#define	VGA_WHITE	0x07

/*
 * Box characters used by the Linux console.
 */
#define	IBM_BLOCK	0x0c
#define	IBM_BOARD	0x09
#define	IBM_BTEE	0xcb
#define	IBM_CKBOARD	0x0a
#define	IBM_DARROW	0x19
#define	IBM_DEGREE	0xb0
#define	IBM_GEQUAL	0x14
#define	IBM_HLINE	0xca
#define	IBM_LANTERN	0xdf
#define	IBM_LARROW	0x16
#define	IBM_LLCORNER	0xc3
#define	IBM_LRCORNER	0xc9
#define	IBM_LTEE	0xc7
#define	IBM_PI		0x1f
#define	IBM_PLMINUS	0xb1
#define	IBM_RTEE	0xcd
#define	IBM_STERLING	0xa3
#define	IBM_TTEE	0xce
#define	IBM_UARROW	0x18
#define	IBM_ULCORNER	0xc6
#define	IBM_URCORNER	0xcc
#define	IBM_VLINE	0xc5

/*
 * This function maps a VGA colour pair to a curses COLOR_PAIR()
 * number.  In the curses scheme colour pair 0 must be white text
 * on a black background, so the origin is moved to there.
 */
#define	VGA_PAIR(foreground, background) \
			(((foreground) + ((background) << 3)) ^ 0x07)

/*
 * Forward declarations.
 */
static void cleanup();
static void conspy(int use_colour);
static void finish(int signal);
static void init_cursesbox();
static void process_command_line(int argc, char** argv);
static int setup();
static void syserror(char* message, ...);
static void usage(char* message, ...);

/*
 * Local variables.
 */
static char*		me;
static struct termios	old_termios;
static int		opt_columns;
static int		opt_lines;
static int		opt_viewonly;
static int  		tty_handle = -1;
static char 		tty_name[20];
static int  		device_handle = -1;
static char 		device_name[20];
static int		uni_handle = -1;
static char             uni_name[20];
/*
 * This array translates a VGA colour (defined above) to a
 * CURSES colour.
 */
static short		colour_map[] =
  { COLOR_BLACK,	/* VGA_BLACK */
    COLOR_BLUE,		/* VGA_BLUE */
    COLOR_GREEN,	/* VGA_GREEN */
    COLOR_CYAN,		/* VGA_CYAN */
    COLOR_RED,		/* VGA_RED */
    COLOR_MAGENTA,	/* VGA_MAGENTA */
    COLOR_YELLOW,	/* VGA_YELLOW */
    COLOR_WHITE,	/* VGA_WHITE */
  };

/*
 * Special IBM characters & their translations.
 */
static unsigned short cursesbox[256];

/*
 * A character as it appears in the VGA video buffer.
 */
struct vidchar {
  unsigned short	vidchar_charattr;	/* Attr in msb, char in lsb */
#define	VIDCHAR_CHAR(vidchar)		((vidchar)->vidchar_charattr & 0xFF)
#define	VIDCHAR_ATTRIBUTE(vidchar)	((vidchar)->vidchar_charattr >> 8)
};


/*
 * The data returned by reading a /dev/vcsa device.
 */
struct vidbuf {
  unsigned char		vidbuf_lines;		/* Line on screen */
  unsigned char		vidbuf_columns;		/* Columns on screen */
  unsigned char		vidbuf_curcolumn;	/* Column cursor is in */
  unsigned char		vidbuf_curline;		/* Line cursor is in */
  struct vidchar	vidbuf_chars[0];	/* Char in VGA video buf */
};

#define VIDBUF_SIZE(cols, lines) (sizeof(struct vidbuf) + cols * lines * sizeof(struct vidchar))


#define UNIBUF_SIZE(cols, lines) (cols * lines * sizeof(uint32_t))


/*
 * Options we allow.
 */
static struct option options[] =
  {
    {"geometry",	1,	0,	'g'},
    {"version",		0,	0,	'V'},
    {"viewonly",	0,	0,	'v'},
    {0,0,0,0},
  };

/*
 * Entry point.
 */
int main(int argc, char** argv)
{
  setlocale(LC_ALL, "");

  int use_colour;

  me = strrchr(argv[0], '/');
  me = me == 0 ? argv[0] : me + 1;
  process_command_line(argc, argv);
  use_colour = setup();
  init_cursesbox();
  conspy(use_colour);
  cleanup();
  /*
   * Clear screen & home cursor.  Added when _I_ became confused
   * as to whether this program was running or not(!)
   */
  if (tigetstr("clear") != (char*)0)
  {
    putp(tigetstr("clear"));
    fflush(stdout);
  }
  return 0;
}

/*
 * Print our a usage message and exit.
 */
static void usage(char* message, ...)
{
  va_list		list;

  if (message != 0)
  {
    fprintf(stderr, "%s: ", me);
    va_start(list, message);
    vfprintf(stderr, message, list);
    va_end(list);
    fprintf(stderr, ".\n");
  }
  fprintf(stderr, "usage: %s [options] [virtual_console].\n", me);
  fprintf(stderr, "options:\n");
  fprintf(stderr, "  -g G,--geometry=G Console size is G, format COLSxROWS.\n");
  fprintf(stderr, "  -V,--version      Print %s's version number and exit.\n", me);
  fprintf(stderr, "  -v,--viewonly     Don't send keystrokes to the console.\n");
  fprintf(stderr, "virtual_console:\n");
  fprintf(stderr, "  omitted  Track the current console.\n");
  fprintf(stderr, "  1..63    Virtual console N.\n");
  fprintf(stderr, "To exit, quickly press escape 3 times.\n");
  exit(1);
}

/*
 * Process the command line.
 */
static void process_command_line(int argc, char** argv)
{
  char*			end;
  int			opt;
  size_t		optindex;
  char			opts[(sizeof(options) / sizeof(*options))*2 + 1];
  char 			vcc_name[sizeof(device_name)];
  int			vcc_errno;
  char 			vcsa_name[sizeof(device_name)];
  int			vcsa_errno;
  char*			virtual_console;

  opt = 0;
  for (optindex = 0; optindex < sizeof(options)/sizeof(*options); optindex += 1)
  {
    opts[opt++] = options[optindex].val;
    if (options[optindex].has_arg)
      opts[opt++] = ':';
  }
  opts[opt] = '\0';
  while ((opt = getopt_long(argc, argv, opts, options, 0)) != -1)
  {
    switch (opt)
    {
      case 'g':
	opt_lines = 0;
	opt_columns = strtol(optarg, &end, 10);
	if (*end == 'x' || *end == 'X') {
	  opt_lines = strtol(end + 1, &end, 10);
	}
	if (*end != '\0' || opt_lines <= 0 || opt_columns <= 0)
	{
	  usage("geometry must be COLSxROWS");
	}
	printf("%s: version %s %s\n", me, conspy_version, conspy_date);
	exit(0);
      case 'V':
	printf("%s: version %s %s\n", me, conspy_version, conspy_date);
	exit(0);
      case 'v':
	opt_viewonly = 1;
	break;
      default:
	usage(0);
    }
  }
  virtual_console = argv[optind];
  if (virtual_console != 0)
  {
    strtol(virtual_console, &end, 10);
    if (*end != '\0' || end - virtual_console > 2)
      usage("invalid virtual console \"%s\"", virtual_console);
  }
  /*
   *  Verify we can open the devices.
   */
  strcpy(vcsa_name, "/dev/vcsa");
  vcc_errno = 0;
  if (virtual_console != 0)
    strcat(vcsa_name, virtual_console);
  strcpy(vcc_name, "/dev/vcc/a");
  if (virtual_console != 0 && *virtual_console != '\0')
    strcat(vcc_name, virtual_console);
  else
    strcat(vcc_name, "0");
  strcpy(device_name, vcsa_name);
  device_handle = open(vcsa_name, O_RDONLY);
  vcsa_errno = errno;
  if (device_handle == -1)
  {
    strcpy(device_name, vcc_name);
    device_handle = open(vcc_name, O_RDONLY);
    vcc_errno = errno;
  }
  if (device_handle == -1)
  {
    fprintf(stderr, "%s: could not open either the alternate device files.\n", me);
    errno = vcsa_errno;
    perror(vcsa_name);
    errno = vcc_errno;
    perror(vcc_name);
    exit(1);
  }
  /* Open unicode device if available.  */
  strcpy(uni_name, vcsa_name);
  uni_name[8] = 'u';
  uni_handle = open(uni_name, O_RDONLY);

  if (!opt_viewonly)
  {
    strcpy(tty_name, "/dev/tty");
    if (virtual_console != 0 && *virtual_console != '\0')
      strcat(tty_name, virtual_console);
    else
      strcat(tty_name, "0");
    tty_handle = open(tty_name, O_WRONLY);
    if (tty_handle == -1 && errno == ENOENT)
    {
      strcpy(tty_name, "/dev/vc/");
      if (virtual_console != 0 && *virtual_console != '\0')
	strcat(tty_name, virtual_console);
      else
	strcat(tty_name, "0");
      tty_handle = open(tty_name, O_WRONLY);
    }
    if (tty_handle == -1)
    {
      perror(tty_name);
      exit(1);
    }
  }
}

/*
 * Initialise the Curses box char set.  This must follow
 */
static void init_cursesbox()
{
  cursesbox[IBM_BLOCK]		= ACS_BLOCK;
  cursesbox[IBM_BOARD]		= ACS_BOARD;
  cursesbox[IBM_BTEE]		= ACS_BTEE;
  cursesbox[IBM_CKBOARD]	= ACS_CKBOARD;
  cursesbox[IBM_DARROW]		= ACS_DARROW;
  cursesbox[IBM_DEGREE]		= ACS_DEGREE;
  cursesbox[IBM_GEQUAL]		= ACS_GEQUAL;
  cursesbox[IBM_HLINE]		= ACS_HLINE;
  cursesbox[IBM_LANTERN]	= ACS_LANTERN;
  cursesbox[IBM_LARROW]		= ACS_LARROW;
  cursesbox[IBM_LLCORNER]	= ACS_LLCORNER;
  cursesbox[IBM_LRCORNER]	= ACS_LRCORNER;
  cursesbox[IBM_LTEE]		= ACS_LTEE;
  cursesbox[IBM_PI]		= ACS_PI;
  cursesbox[IBM_PLMINUS]	= ACS_PLMINUS;
  cursesbox[IBM_RTEE]		= ACS_RTEE;
  cursesbox[IBM_STERLING]	= ACS_STERLING;
  cursesbox[IBM_TTEE]		= ACS_TTEE;
  cursesbox[IBM_UARROW]		= ACS_UARROW;
  cursesbox[IBM_ULCORNER]	= ACS_ULCORNER;
  cursesbox[IBM_URCORNER]	= ACS_URCORNER;
  cursesbox[IBM_VLINE]		= ACS_VLINE;
}

/*
 * Print an OS error and die.
 */
static void syserror(char* message, ...)
{
  int			errnr = errno;
  va_list		list;

  cleanup();
  va_start(list, message);
  vfprintf(stderr, message, list);
  va_end(list);
  fprintf(stderr, ": %s.\n", strerror(errnr));
  exit(1);
}

/*
 * Allocate some memory.
 */
static void* checked_malloc(size_t bytes)
{
  void* result = malloc(bytes);
  if (result == 0)
    syserror("memory allocation failed");
  return result;
}

/*
 * Die, possibly from a signal.
 */
static void finish(int sig)
{
  sigset_t              sigset;

  cleanup();
  if (sig <= 0)
    exit(-sig);
  sigemptyset(&sigset);
  sigaddset(&sigset, sig);
  sigprocmask(SIG_UNBLOCK, &sigset, 0);
  signal(sig, SIG_DFL);
  kill(getpid(), sig);
  pause();
}

/*
 * Set up curses and the TTY.
 */
static int setup()
{
  int			colour;
  int			background;
  int			foreground;
  struct termios	termios;
  int			use_colour;

  /*
   * Get a copy of the current TTY settings.
   */
  if (tcgetattr(0, &old_termios) == -1)
  {
    perror("tcgetattr(0)");
    exit(1);
  }
  /*
   * Start curses.
   */
  (void)signal(SIGHUP, finish);
  (void)signal(SIGINT, finish);
  (void)signal(SIGTERM, finish);
  (void)initscr();
  (void)nonl();
  /*
   * Set up the tty if input is enabled. All characters must be passed through to
   * us unaltered in this case.
   */
  termios = old_termios;
  if (!opt_viewonly) 
  {
    termios.c_iflag &= ~(BRKINT|INLCR|ICRNL|IXON|IXOFF|IUCLC|IXANY|IMAXBEL);
    termios.c_oflag &= ~(OPOST);
    termios.c_lflag &= ~(ISIG|ICANON|ECHO);
    if (tcsetattr(0, TCSANOW, &termios) == -1)
      syserror("tcsetattr(0)");
  }
  /*
   * Set up the colour map, if we can.
   */
  use_colour = 0;
  if (has_colors())
  {
    start_color();
    if (COLOR_PAIRS >= 64)
      use_colour = 1;
  }
  if (use_colour)
  {
    for (foreground = 0; foreground < 8; foreground += 1)
    {
      for (background = 0; background < 8; background += 1)
      {
	colour = VGA_PAIR(foreground, background);
	if (colour != 0)
	  init_pair(colour, colour_map[foreground], colour_map[background]);
      }
    }
  }
  return use_colour;
}

/*
 * Shut down curses, and restore everything.
 */
static void cleanup()
{
  tcsetattr(0, TCSANOW, &old_termios);
  endwin();
  if (device_handle != -1)
    close(device_handle);
  if (tty_handle != -1)
    close(tty_handle);
}

/*
 * This is where the actual work is done.
 */
static void conspy(int use_colour)
{
  unsigned short	box;
  size_t		bytes_read;
  unsigned int		column;
  attr_t		curses_attribute;
  short			curses_colour;
  int			escape_pressed;
  int			escape_notpressed;
  int			ioerror_count;
  unsigned int		key_count;
  unsigned int		key_index;
  uint32_t		keyboard_mode;
  char			keys_pressed[256];
  unsigned int		last_attribute;
  unsigned int		last_columns;
  unsigned int		last_lines;
  unsigned int		line;
  wchar_t		line_buf[256];
  int			line_chars;
  fd_set		readset;
  int			result;
  struct timeval	timeval;
  unsigned int		curr_columns;
  unsigned int		curr_lines;
  int			tty_result;
  unsigned int		video_attribute;
  int			video_char;
  struct vidbuf*	vidbuf;
  struct vidbuf*        prevbuf;
  struct vidbuf*        tmpbuf;
  size_t		vidbuf_size;
  struct vidchar*	vidchar;
  struct vidchar*       prevchar;
  uint32_t*             unibuf;
  size_t                unibuf_size;
  uint32_t*             unichar;
  size_t                offset;
  int                   changes;
  int 			sleep;
  int                   is_different;

  curses_colour = 0;
  curses_attribute = 0;
  escape_notpressed = 0;
  escape_pressed = 0;
  ioerror_count = 0;
  key_count = 0;
  last_attribute = ~0U;
  last_columns = 0;
  last_lines = 0;
  curr_columns = opt_columns ? opt_columns : 80;
  curr_lines = opt_lines ? opt_lines : 25;

  vidbuf_size = VIDBUF_SIZE(curr_columns, curr_lines) + sizeof(vidchar);
  vidbuf = checked_malloc(vidbuf_size);
  prevbuf = checked_malloc(vidbuf_size);

  unibuf_size = UNIBUF_SIZE(curr_columns, curr_lines) + sizeof(uint32_t);
  unibuf = checked_malloc(unibuf_size);

  sleep = 40;

  for (;;)
  {
    changes = 0;
    /*
     * Read the video buffer.
     */
    for (;;)
    {
	if (lseek(device_handle, 0L, SEEK_SET) != 0L)
	  syserror(device_name);
	bytes_read = read(device_handle, vidbuf, vidbuf_size);
	if (bytes_read < sizeof(*vidbuf) || bytes_read > vidbuf_size)
	  syserror(device_name);
	if (bytes_read < vidbuf_size)
	  break;
	vidbuf_size *= 2;
	free(vidbuf);
	free(prevbuf);
	vidbuf = checked_malloc(vidbuf_size);
        prevbuf = checked_malloc(vidbuf_size);
        changes += 1000;
    }
    if (bytes_read == VIDBUF_SIZE(opt_columns, opt_lines))
    {
      curr_columns = opt_columns;
      curr_lines = opt_lines;
    }
    else
    {
      int i, j = -1, k = -1;
      for (i = 0; i <= 7; i += 1)
      {
	curr_columns = vidbuf->vidbuf_columns + (i / 2 * 256);
	curr_lines = vidbuf->vidbuf_lines + (i % 2 * 256);
	if (bytes_read == VIDBUF_SIZE(curr_columns, curr_lines))
	{
	  k = j;
	  j = i;
	}
      }
      if (j == -1 || k != -1)
      {
	fprintf(stderr, "\nCan not guess the geometry of the console.\n");
	exit(1);
      }
      curr_columns = vidbuf->vidbuf_columns + (j / 2 * 256);
      curr_lines = vidbuf->vidbuf_lines + (j % 2 * 256);
    }
    /*
     * If the screen size has changed blank out the unused portions.
     */
    if (curr_lines < last_lines && last_lines < (unsigned)LINES)
    {
      move(curr_lines, 0);
      clrtobot();
    }
    if (curr_columns < last_columns && last_columns < (unsigned)COLS)
    {
      for (line = 0; line < last_lines && line < (unsigned)LINES; line += 1)
      {
	move(line, last_columns);
	clrtoeol();
      }
    }
    last_lines = curr_lines;
    last_columns = curr_columns;

    if (uni_handle != -1)
    {
      if (unibuf_size != UNIBUF_SIZE(curr_columns, curr_lines)) {
	free(unibuf);
        unibuf_size = UNIBUF_SIZE(curr_columns, curr_lines);
        unibuf = checked_malloc(unibuf_size);
      }

      if (lseek(uni_handle, 0, SEEK_SET) != 0)
	syserror(uni_name);

      read(uni_handle, unibuf, unibuf_size);
    }

    /*
     * Write the data to the screen.
     */
    vidchar = vidbuf->vidbuf_chars;
    prevchar = prevbuf->vidbuf_chars;
    unichar = unibuf;
    offset = 0;
    changes = 0;

    for (line = 0; line < curr_lines && line < (unsigned)LINES; line += 1)
    {
      line_chars = 0;
      for (column = 0; column < curr_columns; column += 1)
      {
	if (column >= (unsigned)COLS)
	{
	  offset += curr_columns - column;
	  break;
	}
	// ignore lines until there is a difference....
        is_different = memcmp(&vidchar[offset], &prevchar[offset], sizeof(struct vidchar));
        if (line_chars > 0 || is_different) {
  	  video_attribute = VIDCHAR_ATTRIBUTE(&vidchar[offset]);
          if (uni_handle != -1)
          {
	    video_char = unichar[offset];
            box = 0;
          }
          else
          {
            video_char = VIDCHAR_CHAR(&vidchar[offset]);
            box = cursesbox[video_char];
          }
	  if (box != 0)
	  {
	    video_attribute |= 0x100;
	    video_char = box;
	  }
	  if (video_char < ' ')
	    video_char = ' ';
	  if (video_attribute != last_attribute || !is_different)
	  {
	    if (line_chars > 0)
	    {
	      move(line, column - line_chars);
              wattr_set(stdscr, curses_attribute, curses_colour, NULL);
              waddnwstr(stdscr, line_buf, line_chars);
	      changes += line_chars;
	      line_chars = 0;
	    }
	    curses_attribute = A_NORMAL;
	    if (video_attribute & 0x100)
	      curses_attribute |= A_ALTCHARSET;
	    if (video_attribute & 0x80)
	      curses_attribute |= A_BLINK;
	    if (video_attribute & 0x08)
	      curses_attribute |= A_BOLD;
	    if (use_colour)
	    {
	      curses_colour =
		VGA_PAIR(video_attribute & 0x7, video_attribute>>4 & 0x7);
	    }
	    last_attribute = video_attribute;
	  }
          if (is_different) {
            line_buf[line_chars++] = video_char;
          }
        }
        offset += 1;
      }
      if (line_chars > 0) {
        move(line, column - line_chars);
        wattr_set(stdscr, curses_attribute, curses_colour, NULL);
        waddnwstr(stdscr, line_buf, line_chars);
        changes += line_chars;
        // addchnstr(line_buf, line_chars);
        // wchgat(stdscr, line_chars, curses_attribute, curses_colour, 0);
      }
    }

    if (changes || vidbuf->vidbuf_curline != prevbuf->vidbuf_curline || vidbuf->vidbuf_curcolumn != prevbuf->vidbuf_curcolumn) {
      if (vidbuf->vidbuf_curline < LINES && vidbuf->vidbuf_curcolumn < COLS)
        move(vidbuf->vidbuf_curline, vidbuf->vidbuf_curcolumn);

      refresh();

      sleep = changes > 100 ? 100 : 10;
    } else {
      if (sleep < 200) {
        sleep++;
      }
    }

    tmpbuf = vidbuf;
    vidbuf = prevbuf;
    prevbuf = tmpbuf;

    /*
     * Wait for 1/4 or a second, or for a character to be pressed.
     */
    FD_ZERO(&readset);
    FD_SET(0, &readset);
    timeval.tv_sec = 0;
    timeval.tv_usec = sleep * 1000L;
    result = select(0 + 1, &readset, 0, 0, &timeval);
    if (result == -1)
    {
      if (errno != EINTR)
	syserror("select([tty_handle],0,0,timeval)");
      endwin();
      refresh();
      continue;
    }
    /*
     * Read the keys pressed.
     */
    bytes_read = 0;
    if (result == 1)
    {
      bytes_read =
	read(0, keys_pressed + key_count, sizeof(keys_pressed) - key_count);
      if (bytes_read == ~0U)
	syserror(tty_name);
    }
    /*
     * Do exit processing.
     */
    if (result == 0 && ++escape_notpressed == 4)
    {					/* >1sec since last key press */
      escape_pressed = 0;		/* That ends any exit sequence */
      escape_notpressed = 0;
    }
    for (key_index = key_count; key_index < key_count+bytes_read; key_index += 1)
    {					/* See if escape pressed 3 times */
      if (keys_pressed[key_index] != '\033')
	escape_pressed = 0;
      else if (++escape_pressed == 3)
	return;
      if (keys_pressed[key_index] == ('L' & 0x1F))
	wrefresh(curscr);
    }
    /*
     * Insert all keys pressed into the virtual console's input
     * buffer.  Don't do this if the virtual console is in scan
     * code mode - giving ASCII characters to a program expecting
     * scan codes will confuse it.
     */
    if (!opt_viewonly)
    {
      /*
       * Close & re-open tty in case they have swapped virtual consoles.
       */
      close(tty_handle);
      tty_handle = open(tty_name, O_WRONLY);
      if (tty_handle == -1)
	syserror(tty_name);
      key_count += bytes_read;
      tty_result = ioctl(tty_handle, KDGKBMODE, &keyboard_mode);
      if (tty_result == -1)
	;
      else if (keyboard_mode != K_XLATE && keyboard_mode != K_UNICODE)
	key_count = 0;			/* Keyboard is in scan code mode */
      else
      {
	for (key_index = 0; key_index < key_count; key_index += 1)
	{
	  tty_result = ioctl(tty_handle, TIOCSTI, keys_pressed + key_index);
	  if (tty_result == -1)
	    break;
	}
	if (key_index == key_count)	/* All keys sent? */
	  key_count = 0;		/* Yes - clear the buffer */
	else
	{
	  memmove(keys_pressed, keys_pressed+key_index, key_count-key_index);
	  key_count -= key_index;
	}
      }
      /*
       * We sometimes get spurious IO errors on the TTY as programs
       * close and re-open it.  Usually they will just go away, if
       * we are patient.
       */
      if (tty_result != -1)
	ioerror_count = 0;
      else if (errno != EIO || ++ioerror_count > 4)
	syserror(tty_name);
    }
  }
}
