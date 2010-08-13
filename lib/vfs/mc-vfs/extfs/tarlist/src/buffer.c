/* Buffer management for tar.

   Copyright (C) 1988, 1992, 1993, 1994, 1996, 1997, 1999, 2000, 2001,
   2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010 Free Software
   Foundation, Inc.

   Written by John Gilmore, on 1985-08-25.

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

#include <signal.h>

#include <closeout.h>
#include <quotearg.h>

#include "common.h"
#include <rmt.h>

/* Number of retries before giving up on read.  */
#define READ_ERROR_MAX 10

/* Variables.  */

static void *record_buffer[2];  /* allocated memory */
union block *record_buffer_aligned[2];
static int record_index;

/* FIXME: The following variables should ideally be static to this
   module.  However, this cannot be done yet.  The cleanup continues!  */

union block *record_start;      /* start of record of archive */
union block *record_end;        /* last+1 block of archive record */
union block *current_block;     /* current block of archive */

static off_t record_start_block; /* block ordinal at record_start */

/* Where we write list messages (not errors, not interactions) to.  */
FILE *stdlis;

/* PID of child program, if compress_option or remote archive access.  */
static pid_t child_pid;

/* Error recovery stuff  */
static int read_error_count;

/* Have we hit EOF yet?  */
static bool hit_eof;

static bool read_full_records = false;

void (*flush_read_ptr) (void);


char *volume_label;
char *continued_file_name;
uintmax_t continued_file_size;
uintmax_t continued_file_offset;


/* Functions.  */

void
clear_read_error_count (void)
{
  read_error_count = 0;
}

/* Time-related functions */


void
set_start_time ()
{
  gettime (&start_time);
}


/* Compression detection */

enum compress_type {
  ct_tar,              /* Plain tar file */
  ct_none,             /* Unknown compression type */
  ct_compress,
  ct_gzip,
  ct_bzip2,
  ct_lzip,
  ct_lzma,
  ct_lzop,
  ct_xz
};

struct zip_magic
{
  enum compress_type type;
  size_t length;
  char *magic;
  char *program;
  char *option;
};

static struct zip_magic const magic[] = {
  { ct_tar },
  { ct_none, },
  { ct_compress, 2, "\037\235",  COMPRESS_PROGRAM, "-Z" },
  { ct_gzip,     2, "\037\213",  GZIP_PROGRAM,     "-z"  },
  { ct_bzip2,    3, "BZh",       BZIP2_PROGRAM,    "-j" },
  { ct_lzip,     4, "LZIP",      LZIP_PROGRAM,     "--lzip" },
  { ct_lzma,     6, "\xFFLZMA",  LZMA_PROGRAM,     "--lzma" },
  { ct_lzop,     4, "\211LZO",   LZOP_PROGRAM,     "--lzop" },
  { ct_xz,       6, "\0xFD7zXZ", XZ_PROGRAM,       "-J" },
};

#define NMAGIC (sizeof(magic)/sizeof(magic[0]))

#define compress_option(t) magic[t].option
#define compress_program(t) magic[t].program

/* Check if the file ARCHIVE is a compressed archive. */
enum compress_type
check_compressed_archive (bool *pshort)
{
  struct zip_magic const *p;
  bool sfr;
  bool temp;

  if (!pshort)
    pshort = &temp;
  
  /* Prepare global data needed for find_next_block: */
  record_end = record_start; /* set up for 1st record = # 0 */
  sfr = read_full_records;
  read_full_records = true; /* Suppress fatal error on reading a partial
                               record */
  *pshort = find_next_block () == 0;
  
  /* Restore global values */
  read_full_records = sfr;

  if (tar_checksum (record_start, true) == HEADER_SUCCESS)
    /* Probably a valid header */
    return ct_tar;

  for (p = magic + 2; p < magic + NMAGIC; p++)
    if (memcmp (record_start->buffer, p->magic, p->length) == 0)
      return p->type;

  return ct_none;
}

/* Guess if the archive is seekable. */
static void
guess_seekable_archive ()
{
  struct stat st;

  if (!use_compress_program_option
      && fstat (archive, &st) == 0)
    seekable_archive = S_ISREG (st.st_mode);
  else
    seekable_archive = false;
}

/* Open an archive named archive_name_array[0]. Detect if it is
   a compressed archive of known type and use corresponding decompression
   program if so */
int
open_compressed_archive ()
{
  archive = rmtopen (archive_name_array[0], O_RDONLY | O_BINARY,
                     MODE_RW, NULL);
  if (archive == -1)
    return archive;

      if (!use_compress_program_option)
        {
          bool shortfile;
          enum compress_type type = check_compressed_archive (&shortfile);

          switch (type)
            {
            case ct_tar:
              if (shortfile)
                ERROR ((0, 0, _("This does not look like a tar archive")));
              return archive;
      
            case ct_none:
              if (shortfile)
                ERROR ((0, 0, _("This does not look like a tar archive")));
              set_comression_program_by_suffix (archive_name_array[0], NULL);
              if (!use_compress_program_option)
		return archive;
              break;

            default:
              use_compress_program_option = compress_program (type);
              break;
            }
        }
      
      /* FD is not needed any more */
      rmtclose (archive);
      
      hit_eof = false; /* It might have been set by find_next_block in
                          check_compressed_archive */

      /* Open compressed archive */
      child_pid = sys_child_open_for_uncompress ();
      read_full_records = true;

  record_end = record_start; /* set up for 1st record = # 0 */

  return archive;
}


/* Compute and return the block ordinal at current_block.  */
off_t
current_block_ordinal (void)
{
  return record_start_block + (current_block - record_start);
}

/* Return the location of the next available input or output block.
   Return zero for EOF.  Once we have returned zero, we just keep returning
   it, to avoid accidentally going on to the next file on the tape.  */
union block *
find_next_block (void)
{
  if (current_block == record_end)
    {
      if (hit_eof)
        return 0;
      flush_archive ();
      if (current_block == record_end)
        {
          hit_eof = true;
          return 0;
        }
    }
  return current_block;
}

/* Indicate that we have used all blocks up thru BLOCK. */
void
set_next_block_after (union block *block)
{
  while (block >= current_block)
    current_block++;

  /* Do *not* flush the archive here.  If we do, the same argument to
     set_next_block_after could mean the next block (if the input record
     is exactly one block long), which is not what is intended.  */

  if (current_block > record_end)
    abort ();
}

/* Return the number of bytes comprising the space between POINTER
   through the end of the current buffer of blocks.  This space is
   available for filling with data, or taking data from.  POINTER is
   usually (but not always) the result of previous find_next_block call.  */
size_t
available_space_after (union block *pointer)
{
  return record_end->buffer - pointer->buffer;
}

/* Close file having descriptor FD, and abort if close unsuccessful.  */
void
xclose (int fd)
{
  if (close (fd) != 0)
    close_error (_("(pipe)"));
}

static void
init_buffer ()
{
  if (! record_buffer_aligned[record_index])
    record_buffer_aligned[record_index] =
      page_aligned_alloc (&record_buffer[record_index], record_size);

  record_start = record_buffer_aligned[record_index];
  current_block = record_start;
  record_end = record_start + blocking_factor;
}

/* Open an archive file.  The argument specifies whether we are
   reading or writing, or both.  */
static void
_open_archive (enum access_mode wanted_access)
{
  (void) wanted_access;

  if (record_size == 0)
    FATAL_ERROR ((0, 0, _("Invalid value for record_size")));

  if (archive_names == 0)
    FATAL_ERROR ((0, 0, _("No archive name given")));

  tar_stat_destroy (&current_stat_info);

  record_index = 0;
  init_buffer ();

  if (use_compress_program_option)
    {
          child_pid = sys_child_open_for_uncompress ();
          read_full_records = true;
          record_end = record_start; /* set up for 1st record = # 0 */
    }
  else if (strcmp (archive_name_array[0], "-") == 0)
    {
      read_full_records = true; /* could be a pipe, be safe */

          {
            bool shortfile;
            enum compress_type type;

            archive = STDIN_FILENO;

            type = check_compressed_archive (&shortfile);
            if (type != ct_tar && type != ct_none)
              FATAL_ERROR ((0, 0,
                            _("Archive is compressed. Use %s option"),
                            compress_option (type)));
            if (shortfile)
              ERROR ((0, 0, _("This does not look like a tar archive")));
          }
    }
  else
      {
        archive = open_compressed_archive ();
	if (archive >= 0)
	  guess_seekable_archive ();
      }

  if (archive < 0 || !sys_get_archive_stat ())
    {
      open_fatal (archive_name_array[0]);
    }

  SET_BINARY_MODE (archive);

  find_next_block ();       /* read it in, check for EOF */
}

/* Handle read errors on the archive.  If the read should be retried,
   return to the caller.  */
void
archive_read_error (void)
{
  read_error (*archive_name_cursor);

  if (record_start_block == 0)
    FATAL_ERROR ((0, 0, _("At beginning of tape, quitting now")));

  /* Read error in mid archive.  We retry up to READ_ERROR_MAX times and
     then give up on reading the archive.  */

  if (read_error_count++ > READ_ERROR_MAX)
    FATAL_ERROR ((0, 0, _("Too many errors, quitting")));
  return;
}

static void
short_read (size_t status)
{
  size_t left;                  /* bytes left */
  char *more;                   /* pointer to next byte to read */

  more = record_start->buffer + status;
  left = record_size - status;

  while (left % BLOCKSIZE != 0
         || (left && status && read_full_records))
    {
      if (status)
        while ((status = rmtread (archive, more, left)) == SAFE_READ_ERROR)
          archive_read_error ();

      if (status == 0)
        break;

      if (! read_full_records)
        {
          unsigned long rest = record_size - left;

          FATAL_ERROR ((0, 0,
                        ngettext ("Unaligned block (%lu byte) in archive",
                                  "Unaligned block (%lu bytes) in archive",
                                  rest),
                        rest));
        }

      left -= status;
      more += status;
    }

  record_end = record_start + (record_size - left) / BLOCKSIZE;
}

/*  Flush the current buffer to/from the archive.  */
void
flush_archive (void)
{
  record_start_block += record_end - record_start;
  current_block = record_start;
  record_end = record_start + blocking_factor;

  flush_read ();
}

off_t
seek_archive (off_t size)
{
  off_t start = current_block_ordinal ();
  off_t offset;
  off_t nrec, nblk;
  off_t skipped = (blocking_factor - (current_block - record_start))
                  * BLOCKSIZE;

  if (size <= skipped)
    return 0;
  
  /* Compute number of records to skip */
  nrec = (size - skipped) / record_size;
  if (nrec == 0)
    return 0;
  offset = rmtlseek (archive, nrec * record_size, SEEK_CUR);
  if (offset < 0)
    return offset;

  if (offset % record_size)
    FATAL_ERROR ((0, 0, _("rmtlseek not stopped at a record boundary")));

  /* Convert to number of records */
  offset /= BLOCKSIZE;
  /* Compute number of skipped blocks */
  nblk = offset - start;

  /* Update buffering info */
  record_start_block = offset - blocking_factor;
  current_block = record_end;

  return nblk;
}

/* Close the archive file.  */
void
close_archive (void)
{
  if (rmtclose (archive) != 0)
    close_error (*archive_name_cursor);

  sys_wait_for_child (child_pid, hit_eof);

  tar_stat_destroy (&current_stat_info);
  free (record_buffer[0]);
  free (record_buffer[1]);
}


/* Low-level flush functions */

/* Simple flush read (no multi-volume or label extensions) */
static void
simple_flush_read (void)
{
  size_t status;                /* result from system call */

  /* Clear the count of errors.  This only applies to a single call to
     flush_read.  */

  read_error_count = 0;         /* clear error count */

  for (;;)
    {
      status = rmtread (archive, record_start->buffer, record_size);
      if (status == record_size)
          return;
      if (status == SAFE_READ_ERROR)
        {
          archive_read_error ();
          continue;             /* try again */
        }
      break;
    }
  short_read (status);
}


/* GNU flush functions. These support multi-volume and archive labels in
   GNU and PAX archive formats. */

static void
_gnu_flush_read (void)
{
  size_t status;                /* result from system call */

  /* Clear the count of errors.  This only applies to a single call to
     flush_read.  */

  read_error_count = 0;         /* clear error count */

  for (;;)
    {
      status = rmtread (archive, record_start->buffer, record_size);
      if (status == record_size)
          return;

      /* The condition below used to include
              || (status > 0 && !read_full_records)
         This is incorrect since even if new_volume() succeeds, the
         subsequent call to rmtread will overwrite the chunk of data
         already read in the buffer, so the processing will fail */

      if (status == SAFE_READ_ERROR)
        {
          archive_read_error ();
          continue;
        }
      break;
    }
  short_read (status);
}

static void
gnu_flush_read (void)
{
  flush_read_ptr = simple_flush_read; /* Avoid recursion */
  _gnu_flush_read ();
  flush_read_ptr = gnu_flush_read;
}

void
flush_read ()
{
  flush_read_ptr ();
}

void
open_archive (enum access_mode wanted_access)
{
  flush_read_ptr = gnu_flush_read;

  _open_archive (wanted_access);
}
