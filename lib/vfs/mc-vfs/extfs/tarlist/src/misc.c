/* Miscellaneous functions, not really specific to GNU tar.

   Copyright (C) 1988, 1992, 1994, 1995, 1996, 1997, 1999, 2000, 2001,
   2003, 2004, 2005, 2006, 2007, 2009 Free Software Foundation, Inc.

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
#include "common.h"


/* Handling strings.  */

/* Assign STRING to a copy of VALUE if not zero, or to zero.  If
   STRING was nonzero, it is freed first.  */
void
assign_string (char **string, const char *value)
{
  if (*string)
    free (*string);
  *string = value ? xstrdup (value) : 0;
}


/* Fork, aborting if unsuccessful.  */
pid_t
xfork (void)
{
  pid_t p = fork ();
  if (p == (pid_t) -1)
    call_arg_fatal ("fork", _("child process"));
  return p;
}

/* Create a pipe, aborting if unsuccessful.  */
void
xpipe (int fd[2])
{
  if (pipe (fd) < 0)
    call_arg_fatal ("pipe", _("interprocess channel"));
}

/* Return PTR, aligned upward to the next multiple of ALIGNMENT.
   ALIGNMENT must be nonzero.  The caller must arrange for ((char *)
   PTR) through ((char *) PTR + ALIGNMENT - 1) to be addressable
   locations.  */

static inline void *
ptr_align (void *ptr, size_t alignment)
{
  char *p0 = ptr;
  char *p1 = p0 + alignment - 1;
  return p1 - (size_t) p1 % alignment;
}

/* Return the address of a page-aligned buffer of at least SIZE bytes.
   The caller should free *PTR when done with the buffer.  */

void *
page_aligned_alloc (void **ptr, size_t size)
{
  size_t alignment = getpagesize ();
  size_t size1 = size + alignment;
  if (size1 < size)
    xalloc_die ();
  *ptr = xmalloc (size1);
  return ptr_align (*ptr, alignment);
}
