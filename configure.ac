# Process this file with autoconf to produce a configure script.
AC_INIT(conspy.c, 1.10)
AM_INIT_AUTOMAKE([foreign])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([fcntl.h getopt.h stdarg.h stdlib.h string.h sys/ioctl.h sys/time.h termios.h unistd.h])
AC_CHECK_LIB(ncursesw, initscr)

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_TIME

# Checks for library functions.
AC_PROG_GCC_TRADITIONAL
AC_TYPE_SIGNAL
AC_CHECK_FUNCS([select strtol])

AC_CONFIG_FILES([Makefile-automake])
AC_OUTPUT()
