dnl Special Autoconf macros for GNU Tar         -*- autoconf -*-
dnl Copyright (C) 2009 Free Software Foundation, Inc.
dnl
dnl GNU tar is free software; you can redistribute it and/or modify
dnl it under the terms of the GNU General Public License as published by
dnl the Free Software Foundation; either version 3, or (at your option)
dnl any later version.
dnl
dnl GNU tar is distributed in the hope that it will be useful,
dnl but WITHOUT ANY WARRANTY; without even the implied warranty of
dnl MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
dnl GNU General Public License for more details.
dnl
dnl You should have received a copy of the GNU General Public License along
dnl with GNU tar.  If not, see <http://www.gnu.org/licenses/>.

AC_DEFUN([TAR_COMPR_PROGRAM],[
 m4_pushdef([tar_compr_define],translit($1,[a-z+-],[A-ZX_])[_PROGRAM])
 m4_pushdef([tar_compr_var],[tar_cv_compressor_]translit($1,[+-],[x_]))
 AC_ARG_WITH($1,
             AC_HELP_STRING([--with-]$1[=PROG],
	                    [use PROG as ]$1[ compressor program]),
             [tar_compr_var=${withval}],
	     [tar_compr_var=m4_if($2,,$1,$2)])
 AC_DEFINE_UNQUOTED(tar_compr_define, "$tar_compr_var",
                    [Define to the program name of ]$1[ compressor program])])

m4_include([m4.gnu/00gnulib.m4])
m4_include([m4.gnu/argmatch.m4])
m4_include([m4.gnu/clock_time.m4])
m4_include([m4.gnu/close-stream.m4])
m4_include([m4.gnu/closeout.m4])
m4_include([m4.gnu/extensions.m4])
m4_include([m4.gnu/dirname.m4])
m4_include([m4.gnu/dos.m4])
m4_include([m4.gnu/double-slash-root.m4])
m4_include([m4.gnu/fcntl-o.m4])
m4_include([m4.gnu/fpending.m4])
m4_include([m4.gnu/gettime.m4])
m4_include([m4.gnu/glibc21.m4])
m4_include([m4.gnu/gnulib-common.m4])
m4_include([m4.gnu/hash.m4])
m4_include([m4.gnu/include_next.m4])
m4_include([m4.gnu/inline.m4])
m4_include([m4.gnu/inttostr.m4])
m4_include([m4.gnu/localcharset.m4])
m4_include([m4.gnu/multiarch.m4])
m4_include([m4.gnu/quote.m4])
m4_include([m4.gnu/quotearg.m4])
m4_include([m4.gnu/safe-read.m4])
m4_include([m4.gnu/safe-write.m4])
m4_include([m4.gnu/size_max.m4])
m4_include([m4.gnu/ssize_t.m4])
m4_include([m4.gnu/stdbool.m4])
m4_include([m4.gnu/stdint.m4])
m4_include([m4.gnu/system.m4])
m4_include([m4.gnu/timespec.m4])
m4_include([m4.gnu/xalloc.m4])
m4_include([m4.gnu/xstrndup.m4])

m4_include([m4.gnu/gnulib-comp.m4])
