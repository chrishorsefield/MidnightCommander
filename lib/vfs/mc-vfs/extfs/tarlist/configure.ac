# Configure template for GNU tar.  -*- autoconf -*-

# Copyright (C) 1991, 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001,
# 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010 Free
# Software Foundation, Inc.

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3, or (at your option)
# any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301, USA.

AC_INIT([tarlist], [1.23], [mc-devel@gnome.org])
AC_CONFIG_SRCDIR([src/tar.c])
AC_CONFIG_MACRO_DIR([../../../../../m4])
AC_CONFIG_AUX_DIR([../../../../../config])
AC_CONFIG_HEADERS([config.h])
AC_PREREQ([2.60])
AM_INIT_AUTOMAKE([tarlist], [1.23], [gnits std-options nostdinc no-dist])

AC_SUBST([PACKAGE], [AC_PACKAGE_TARNAME])
AC_SUBST([VERSION], [AC_PACKAGE_VERSION])
AC_DEFINE_UNQUOTED([PACKAGE], "$PACKAGE", [Name of package])
AC_DEFINE_UNQUOTED([VERSION], "$VERSION", [Version number of package])

dnl Enable silent rules by default (if yes)
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AC_PROG_CC
gl_EARLY

AC_SYS_LARGEFILE
AC_ISC_POSIX
AC_C_INLINE

AC_CHECK_HEADERS_ONCE(fcntl.h memory.h net/errno.h \
  string.h \
  sys/param.h sys/device.h sys/filio.h \
  sys/time.h \
  unistd.h locale.h)

AC_CHECK_HEADERS([sys/buf.h], [], [],
[#if HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif])

AC_HEADER_SYS_WAIT

AC_HEADER_MAJOR
AC_HEADER_STAT
AC_HEADER_STDC

AC_MSG_CHECKING([for st_fstype string in struct stat])
AC_CACHE_VAL(diff_cv_st_fstype_string,
  [AC_TRY_COMPILE([#include <sys/types.h>
#include <sys/stat.h>], [struct stat s; s.st_fstype[0] = 'x';],
   diff_cv_st_fstype_string=yes,
   diff_cv_st_fstype_string=no)])
AC_MSG_RESULT($diff_cv_st_fstype_string)
if test $diff_cv_st_fstype_string = yes; then
  AC_DEFINE(HAVE_ST_FSTYPE_STRING, 1,
    [Define if struct stat has a char st_fstype[] member.])
fi

AC_TYPE_SIGNAL
AC_TYPE_MODE_T
AC_TYPE_PID_T
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_TYPE_UID_T
AC_CHECK_TYPE(major_t, , AC_DEFINE(major_t, int,
				   [Type of major device numbers.]))
AC_CHECK_TYPE(minor_t, , AC_DEFINE(minor_t, int,
				   [Type of minor device numbers.]))
AC_CHECK_DECLS([time],,, [#include <time.h>])

# gnulib modules
gl_INIT
# paxutils modules
PU_SYSTEM

AC_CHECK_FUNCS(setlocale regexec)
AC_CHECK_DECLS([time],,, [#include <time.h>])

AC_REPLACE_FUNCS(waitpid)

TAR_COMPR_PROGRAM(compress)
TAR_COMPR_PROGRAM(gzip)
TAR_COMPR_PROGRAM(bzip2)
TAR_COMPR_PROGRAM(lzip)
TAR_COMPR_PROGRAM(lzma)
TAR_COMPR_PROGRAM(lzop)
TAR_COMPR_PROGRAM(xz)

AC_DEFINE_UNQUOTED(DEFAULT_QUOTING_STYLE, escape_quoting_style,
  [Define to a default quoting style (see gnu/quotearg.h for the list)])

# Iconv
AM_ICONV
AC_CHECK_HEADERS(iconv.h)
AC_CHECK_TYPE(iconv_t,:,
              AC_DEFINE(iconv_t, int,
                        [Conversion descriptor type]),
              [
#ifdef HAVE_ICONV_H
# include <iconv.h>
#endif
])

# Gettext.
AM_GNU_GETTEXT([external], [need-ngettext], [../../../../../intl])
AM_GNU_GETTEXT_VERSION([0.16])

if test "x$USE_INCLUDED_LIBINTL" = xyes; then
    CPPFLAGS="$CPPFLAGS -I\$(top_builddir)/intl -I\$(top_srcdir)/intl -I../../../../../intl"
fi

AC_OUTPUT([Makefile\
           gnu/Makefile\
           lib/Makefile\
           po/Makefile.in\
           src/Makefile])
