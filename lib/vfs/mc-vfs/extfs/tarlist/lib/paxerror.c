/* Miscellaneous error functions

   Copyright (C) 2005, 2007 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 3, or (at your option) any later
   version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
   Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.  */

#include <system.h>
#include <paxlib.h>
#include <quote.h>
#include <quotearg.h>

/* Decode MODE from its binary form in a stat structure, and encode it
   into a 9-byte string STRING, terminated with a NUL.  */

void
pax_decode_mode (mode_t mode, char *string)
{
  *string++ = mode & S_IRUSR ? 'r' : '-';
  *string++ = mode & S_IWUSR ? 'w' : '-';
  *string++ = (mode & S_ISUID
	       ? (mode & S_IXUSR ? 's' : 'S')
	       : (mode & S_IXUSR ? 'x' : '-'));
  *string++ = mode & S_IRGRP ? 'r' : '-';
  *string++ = mode & S_IWGRP ? 'w' : '-';
  *string++ = (mode & S_ISGID
	       ? (mode & S_IXGRP ? 's' : 'S')
	       : (mode & S_IXGRP ? 'x' : '-'));
  *string++ = mode & S_IROTH ? 'r' : '-';
  *string++ = mode & S_IWOTH ? 'w' : '-';
  *string++ = (mode & S_ISVTX
	       ? (mode & S_IXOTH ? 't' : 'T')
	       : (mode & S_IXOTH ? 'x' : '-'));
  *string = '\0';
}

/* Report an error associated with the system call CALL and the
   optional name NAME.  */
void
call_arg_error (char const *call, char const *name)
{
  int e = errno;
  /* TRANSLATORS: %s after `Cannot' is a function name, e.g. `Cannot open'.
     Directly translating this to another language will not work, first because
     %s itself is not translated.
     Translate it as `%s: Function %s failed'. */
  ERROR ((0, e, _("%s: Cannot %s"), quotearg_colon (name), call));
}

/* Report a fatal error associated with the system call CALL and
   the optional file name NAME.  */
void
call_arg_fatal (char const *call, char const *name)
{
  int e = errno;
  /* TRANSLATORS: %s after `Cannot' is a function name, e.g. `Cannot open'.
     Directly translating this to another language will not work, first because
     %s itself is not translated.
     Translate it as `%s: Function %s failed'. */
  FATAL_ERROR ((0, e, _("%s: Cannot %s"), quotearg_colon (name),  call));
}

/* Report a warning associated with the system call CALL and
   the optional file name NAME.  */
void
call_arg_warn (char const *call, char const *name)
{
  int e = errno;
  /* TRANSLATORS: %s after `Cannot' is a function name, e.g. `Cannot open'.
     Directly translating this to another language will not work, first because
     %s itself is not translated.
     Translate it as `%s: Function %s failed'. */
  WARN ((0, e, _("%s: Warning: Cannot %s"), quotearg_colon (name), call));
}

void
close_error (char const *name)
{
  call_arg_error ("close", name);
}

void
close_warn (char const *name)
{
  call_arg_warn ("close", name);
}

void
exec_fatal (char const *name)
{
  call_arg_fatal ("exec", name);
}

void
open_error (char const *name)
{
  call_arg_error ("open", name);
}

void
open_fatal (char const *name)
{
  call_arg_fatal ("open", name);
}

void
open_warn (char const *name)
{
  call_arg_warn ("open", name);
}

void
read_error (char const *name)
{
  call_arg_error ("read", name);
}

void
read_fatal (char const *name)
{
  call_arg_fatal ("read", name);
}

void
rmdir_error (char const *name)
{
  call_arg_error ("rmdir", name);
}

void
savedir_error (char const *name)
{
  call_arg_error ("savedir", name);
}

void
savedir_warn (char const *name)
{
  call_arg_warn ("savedir", name);
}

void
seek_error (char const *name)
{
  call_arg_error ("seek", name);
}

void
seek_warn (char const *name)
{
  call_arg_warn ("seek", name);
}

void
stat_fatal (char const *name)
{
  call_arg_fatal ("stat", name);
}

void
stat_error (char const *name)
{
  call_arg_error ("stat", name);
}

void
stat_warn (char const *name)
{
  call_arg_warn ("stat", name);
}

void
truncate_error (char const *name)
{
  call_arg_error ("truncate", name);
}

void
truncate_warn (char const *name)
{
  call_arg_warn ("truncate", name);
}

void
unlink_error (char const *name)
{
  call_arg_error ("unlink", name);
}

void
utime_error (char const *name)
{
  call_arg_error ("utime", name);
}

void
waitpid_error (char const *name)
{
  call_arg_error ("waitpid", name);
}

void
write_error (char const *name)
{
  call_arg_error ("write", name);
}
