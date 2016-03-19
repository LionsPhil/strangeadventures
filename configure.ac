#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([strangelp], [20051111-1], [lionsphil@gmail.com])
AC_CONFIG_SRCDIR([src/main.cpp])
AM_INIT_AUTOMAKE
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CXX

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h malloc.h memory.h stdlib.h string.h])

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([memset sqrt strncasecmp])

# Check for SDL
SDL_VERSION=1.2.0
AM_PATH_SDL($SDL_VERSION,
          :,
          AC_MSG_ERROR([*** SDL version $SDL_VERSION not found!])
)
CXXFLAGS="$CXXFLAGS $SDL_CFLAGS"
LIBS="$LIBS $SDL_LIBS"

# Check for SDL_mixer
AC_SEARCH_LIBS([Mix_OpenAudio], [SDL_mixer], [], [
	AC_MSG_ERROR([unable to find Mix_OpenAudio from libSDL_mixer])
])

# Configure flag to indicate a Windows build (Automake needs to know too)
AC_ARG_ENABLE([windows],
	AS_HELP_STRING([--enable-windows], [Build for Windows (you must also use mingw32!)]))
AS_IF([test "x$enable_windows" = "xyes"], [
	AC_DEFINE([WINDOWS], [], [Windows build])
])
AM_CONDITIONAL([WINDOWS], [test "x$enable_windows" = "xyes"])

# Support --enable-silent-rules or make V=0 for cleaner build logs
AM_SILENT_RULES

AC_CONFIG_FILES([Makefile
                 src/Makefile])
AC_OUTPUT
