/* System-dependent calls for tar.

   Copyright (C) 2003, 2004, 2005, 2006, 2007,
   2008 Free Software Foundation, Inc.

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
#include <rmt.h>
#include <signal.h>

extern union block *record_start; /* FIXME */

static struct stat archive_stat; /* stat block for archive file */

bool
sys_get_archive_stat (void)
{
  return fstat (archive, &archive_stat) == 0;
}

void
sys_wait_for_child (pid_t child_pid, bool eof)
{
  if (child_pid)
    {
      int wait_status;

      while (waitpid (child_pid, &wait_status, 0) == -1)
	if (errno != EINTR)
	  {
	    waitpid_error (use_compress_program_option);
	    break;
	  }

      if (WIFSIGNALED (wait_status))
	{
	  int sig = WTERMSIG (wait_status);
	  if (!(!eof && sig == SIGPIPE))
	    FATAL_ERROR ((0, 0, _("Child died with signal %d"), sig));
	}
      else if (WEXITSTATUS (wait_status) != 0)
	FATAL_ERROR ((0, 0, _("Child returned status %d"),
		      WEXITSTATUS (wait_status)));
    }
}

/* Return nonzero if NAME is the name of a regular file, or if the file
   does not exist (so it would be created as a regular file).  */
static int
is_regular_file (const char *name)
{
  struct stat stbuf;

  if (stat (name, &stbuf) == 0)
    return S_ISREG (stbuf.st_mode);
  else
    return errno == ENOENT;
}

#define	PREAD 0			/* read file descriptor from pipe() */
#define	PWRITE 1		/* write file descriptor from pipe() */

/* Duplicate file descriptor FROM into becoming INTO.
   INTO is closed first and has to be the next available slot.  */
static void
xdup2 (int from, int into)
{
  if (from != into)
    {
      int status = close (into);

      if (status != 0 && errno != EBADF)
	{
	  int e = errno;
	  FATAL_ERROR ((0, e, _("Cannot close")));
	}
      status = dup (from);
      if (status != into)
	{
	  if (status < 0)
	    {
	      int e = errno;
	      FATAL_ERROR ((0, e, _("Cannot dup")));
	    }
	  abort ();
	}
      xclose (from);
    }
}

void wait_for_grandchild (pid_t pid) __attribute__ ((__noreturn__));

/* Propagate any failure of the grandchild back to the parent.  */
void
wait_for_grandchild (pid_t pid)
{
  int wait_status;
  int exit_code = 0;
  
  while (waitpid (pid, &wait_status, 0) == -1)
    if (errno != EINTR)
      {
	waitpid_error (use_compress_program_option);
	break;
      }

  if (WIFSIGNALED (wait_status))
    raise (WTERMSIG (wait_status));
  else if (WEXITSTATUS (wait_status) != 0)
    exit_code = WEXITSTATUS (wait_status);
  
  exit (exit_code);
}

/* Set ARCHIVE for uncompressing, then reading an archive.  */
pid_t
sys_child_open_for_uncompress (void)
{
  int parent_pipe[2];
  int child_pipe[2];
  pid_t grandchild_pid;
  pid_t child_pid;

  xpipe (parent_pipe);
  child_pid = xfork ();

  if (child_pid > 0)
    {
      /* The parent tar is still here!  Just clean up.  */

      archive = parent_pipe[PREAD];
      xclose (parent_pipe[PWRITE]);
      return child_pid;
    }

  /* The newborn child tar is here!  */

  set_program_name (_("tar (child)"));
  signal (SIGPIPE, SIG_DFL);
  
  xdup2 (parent_pipe[PWRITE], STDOUT_FILENO);
  xclose (parent_pipe[PREAD]);

  /* Check if we need a grandchild tar.  This happens only if either:
     a) we're reading stdin: to force unblocking;
     b) the file is to be accessed by rmt: compressor doesn't know how;
     c) the file is not a plain file.  */

  if (strcmp (archive_name_array[0], "-") != 0
      && is_regular_file (archive_name_array[0]))
    {
      /* We don't need a grandchild tar.  Open the archive and lauch the
	 uncompressor.  */

      archive = open (archive_name_array[0], O_RDONLY | O_BINARY, MODE_RW);
      if (archive < 0)
	open_fatal (archive_name_array[0]);
      xdup2 (archive, STDIN_FILENO);
      execlp (use_compress_program_option, use_compress_program_option,
	      "-d", (char *) 0);
      exec_fatal (use_compress_program_option);
    }

  /* We do need a grandchild tar.  */

  xpipe (child_pipe);
  grandchild_pid = xfork ();

  if (grandchild_pid == 0)
    {
      /* The newborn grandchild tar is here!  Launch the uncompressor.  */

      set_program_name (_("tar (grandchild)"));

      xdup2 (child_pipe[PREAD], STDIN_FILENO);
      xclose (child_pipe[PWRITE]);
      execlp (use_compress_program_option, use_compress_program_option,
	      "-d", (char *) 0);
      exec_fatal (use_compress_program_option);
    }

  /* The child tar is still here!  */

  /* Prepare for unblocking the data from the archive into the
     uncompressor.  */

  xdup2 (child_pipe[PWRITE], STDOUT_FILENO);
  xclose (child_pipe[PREAD]);

  if (strcmp (archive_name_array[0], "-") == 0)
    archive = STDIN_FILENO;
  else
    archive = rmtopen (archive_name_array[0], O_RDONLY | O_BINARY,
		       MODE_RW, NULL);
  if (archive < 0)
    open_fatal (archive_name_array[0]);

  /* Let's read the archive and pipe it into stdout.  */

  while (1)
    {
      char *cursor;
      size_t maximum;
      size_t count;
      size_t status;

      clear_read_error_count ();

    error_loop:
      status = rmtread (archive, record_start->buffer, record_size);
      if (status == SAFE_READ_ERROR)
	{
	  archive_read_error ();
	  goto error_loop;
	}
      if (status == 0)
	break;
      cursor = record_start->buffer;
      maximum = status;
      while (maximum)
	{
	  count = maximum < BLOCKSIZE ? maximum : BLOCKSIZE;
	  if (full_write (STDOUT_FILENO, cursor, count) != count)
	    write_error (use_compress_program_option);
	  cursor += count;
	  maximum -= count;
	}
    }

  xclose (STDOUT_FILENO);

  wait_for_grandchild (grandchild_pid);
}
