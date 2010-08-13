/* A tar (tape archiver) program.

   Copyright (C) 1988, 1992, 1993, 1994, 1995, 1996, 1997, 1999, 2000,
   2001, 2003, 2004, 2005, 2006, 2007, 2009 Free Software Foundation, Inc.

   Written by John Gilmore, starting 1985-08-25.

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

/* The following causes "common.h" to produce definitions of all the global
   variables, rather than just "extern" declarations of them.  GNU tar does
   depend on the system loader to preset all GLOBAL variables to neutral (or
   zero) values; explicit initialization is usually not done.  */
#define GLOBAL
#include "common.h"

#include <closeout.h>
#include <exitfail.h>
#include <quotearg.h>
#include <stdopen.h>

/* Local declarations.  */

#ifndef DEFAULT_ARCHIVE_FORMAT
# define DEFAULT_ARCHIVE_FORMAT GNU_FORMAT
#endif

#ifndef DEFAULT_BLOCKING
# define DEFAULT_BLOCKING 20
#endif


static void
parse_opt (char *arg)
{
  /* f */
  archive_name_array[archive_names++] = arg;

  /* t */
  verbose_option++;

  /* vv */
  verbose_option++;
  verbose_option++;
  warning_option |= WARN_VERBOSE_WARNINGS;
}

static void
decode_options (int argc, char **argv)
{
  /* MC: we expected 2 arguments: tarlist archive.tar */

  archive_format = DEFAULT_FORMAT;
  blocking_factor = DEFAULT_BLOCKING;
  record_size = DEFAULT_BLOCKING * BLOCKSIZE;

  /* Parse all options and non-options as they appear.  */

  if (argc != 2)
    exit (TAREXIT_FAILURE);

  parse_opt (argv[1]);

  /* Derive option values and check option consistency.  */

  if (archive_format == DEFAULT_FORMAT)
	archive_format = DEFAULT_ARCHIVE_FORMAT;

  /* Initialize stdlis */
  stdlis = stdout;

  archive_name_cursor = archive_name_array;
}


/* Tar proper.  */

/* Main routine for tar.  */
int
main (int argc, char **argv)
{
  set_start_time ();
  set_program_name (argv[0]);

  setlocale (LC_ALL, "");
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);

  exit_failure = TAREXIT_FAILURE;
  exit_status = TAREXIT_SUCCESS;
  set_quoting_style (0, DEFAULT_QUOTING_STYLE);

  /* Make sure we have first three descriptors available */
  stdopen ();

  /* Pre-allocate a few structures.  */

  allocated_archive_names = 1;
  archive_name_array =
    xmalloc (sizeof (const char *) * allocated_archive_names);
  archive_names = 0;

  /* Ensure default behavior for some signals */
  signal (SIGPIPE, SIG_IGN);
  /* System V fork+wait does not work if SIGCHLD is ignored.  */
  signal (SIGCHLD, SIG_DFL);

  /* Decode options.  */

  decode_options (argc, argv);

  /* Main command execution.  */

  read_and (list_archive);

  /* Dispose of allocated memory, and return.  */

  free (archive_name_array);

  if (exit_status == TAREXIT_FAILURE)
    error (0, 0, _("Exiting with failure status due to previous errors"));

  if (stdlis == stdout)
    close_stdout ();
  else if (ferror (stderr) || fclose (stderr) != 0)
    set_exit_status (TAREXIT_FAILURE);

  return exit_status;
}

void
tar_stat_init (struct tar_stat_info *st)
{
  memset (st, 0, sizeof (*st));
}

void
tar_stat_destroy (struct tar_stat_info *st)
{
  free (st->orig_file_name);
  free (st->file_name);
  free (st->link_name);
  free (st->uname);
  free (st->gname);
  free (st->sparse_map);
  free (st->dumpdir);
  xheader_destroy (&st->xhdr);
  memset (st, 0, sizeof (*st));
}

/* Set tar exit status to VAL, unless it is already indicating
   a more serious condition. This relies on the fact that the
   values of TAREXIT_ constants are ranged by severity. */
void
set_exit_status (int val)
{
  if (val > exit_status)
    exit_status = val;
}
