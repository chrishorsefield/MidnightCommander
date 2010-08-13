/* Common declarations for the tar program.

   Copyright (C) 1988, 1992, 1993, 1994, 1996, 1997, 1999, 2000, 2001,
   2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010 Free Software Foundation, 
   Inc.

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

/* Declare the GNU tar archive format.  */
#include "tar.h"

/* The checksum field is filled with this while the checksum is computed.  */
#define CHKBLANKS	"        "	/* 8 blanks, no null */

/* Some constants from POSIX are given names.  */
#define NAME_FIELD_SIZE   100
#define PREFIX_FIELD_SIZE 155
#define UNAME_FIELD_SIZE   32
#define GNAME_FIELD_SIZE   32



/* Some various global definitions.  */

/* Name of file to use for interacting with user.  */

/* GLOBAL is defined to empty in tar.c only, and left alone in other *.c
   modules.  Here, we merely set it to "extern" if it is not already set.
   GNU tar does depend on the system loader to preset all GLOBAL variables to
   neutral (or zero) values, explicit initialization is usually not done.  */
#ifndef GLOBAL
# define GLOBAL extern
#endif

#define TAREXIT_SUCCESS PAXEXIT_SUCCESS
#define TAREXIT_DIFFERS PAXEXIT_DIFFERS
#define TAREXIT_FAILURE PAXEXIT_FAILURE


#include <full-write.h>
#include <quote.h>
#include <safe-read.h>
#include <timespec.h>
#define obstack_chunk_alloc xmalloc
#define obstack_chunk_free free
#include <obstack.h>
#include <progname.h>

#include <paxlib.h>

/* Log base 2 of common values.  */
#define LG_8 3
#define LG_64 6
#define LG_256 8

/* Information gleaned from the command line.  */

/* Main command option.  */

/* Selected format for output archive.  */
GLOBAL enum archive_format archive_format;

/* Size of each record, once in blocks, once in bytes.  Those two variables
   are always related, the second being BLOCKSIZE times the first.  They do
   not have _option in their name, even if their values is derived from
   option decoding, as these are especially important in tar.  */
GLOBAL int blocking_factor;
GLOBAL size_t record_size;

GLOBAL bool absolute_names_option;

/* Specified name of compression program, or "gzip" as implied by -z.  */
GLOBAL const char *use_compress_program_option;

/* Count how many times the option has been set, multiple setting yields
   more verbose behavior.  Value 0 means no verbosity, 1 means file name
   only, 2 means file name and all attributes.  More than 2 is just like 2.  */
GLOBAL int verbose_option;

/* Other global variables.  */

/* File descriptor for archive file.  */
GLOBAL int archive;

/* Timestamps: */
GLOBAL struct timespec start_time;        /* when we started execution */

GLOBAL struct tar_stat_info current_stat_info;

/* List of tape drive names, number of such tape drives, allocated number,
   and current cursor in list.  */
GLOBAL const char **archive_name_array;
GLOBAL size_t archive_names;
GLOBAL size_t allocated_archive_names;
GLOBAL const char **archive_name_cursor;

/* Structure for keeping track of filenames and lists thereof.  */
struct name
  {
    struct name *next;          /* Link to the next element */
    struct name *prev;          /* Link to the previous element */

    char *name;                 /* File name or globbing pattern */
    size_t length;		/* cached strlen (name) */
    bool cmdline;               /* true if this name was given in the
				   command line */
    
    int change_dir;		/* Number of the directory to change to.
				   Set with the -C option. */
    uintmax_t found_count;	/* number of times a matching file has
				   been found */
    
    /* The following members are used for incremental dumps only,
       if this struct name represents a directory;
       see incremen.c */
    struct directory *directory;/* directory meta-data and contents */
    struct name *parent;        /* pointer to the parent hierarchy */
    struct name *child;         /* pointer to the first child */
    struct name *sibling;       /* pointer to the next sibling */
    char *caname;               /* canonical name */
  };

GLOBAL bool seekable_archive;


/* Declarations for each module.  */

enum access_mode
{
  ACCESS_READ,
};

/* Module buffer.c.  */

extern FILE *stdlis;
extern char *volume_label;
extern char *continued_file_name;
extern uintmax_t continued_file_size;
extern uintmax_t continued_file_offset;

size_t available_space_after (union block *pointer);
off_t current_block_ordinal (void);
void close_archive (void);
union block *find_next_block (void);
void flush_read (void);
void flush_archive (void);
void open_archive (enum access_mode mode);
void set_next_block_after (union block *block);
void clear_read_error_count (void);
void xclose (int fd);
void archive_read_error (void);
off_t seek_archive (off_t size);
void set_start_time (void);

/* Module create.c.  */

enum dump_status
  {
    dump_status_ok,
    dump_status_short,
    dump_status_fail,
    dump_status_not_implemented
  };

/* Module list.c.  */

enum read_header
{
  HEADER_STILL_UNREAD,		/* for when read_header has not been called */
  HEADER_SUCCESS,		/* header successfully read and checksummed */
  HEADER_SUCCESS_EXTENDED,	/* likewise, but we got an extended header */
  HEADER_ZERO_BLOCK,		/* zero block where header expected */
  HEADER_END_OF_FILE,		/* true end of file while header expected */
  HEADER_FAILURE		/* ill-formed header, or bad checksum */
};

/* Operation mode for read_header: */

enum read_header_mode
{
  read_header_auto,             /* process extended headers automatically */
  read_header_x_raw,            /* return raw extended headers (return
				   HEADER_SUCCESS_EXTENDED) */
  read_header_x_global          /* when POSIX global extended header is read,
				   decode it and return
				   HEADER_SUCCESS_EXTENDED */
};
extern union block *current_header;
extern enum archive_format current_format;
extern size_t recent_long_name_blocks;
extern size_t recent_long_link_blocks;

void decode_header (union block *header, struct tar_stat_info *stat_info,
		    enum archive_format *format_pointer, int do_user_group);
char const *tartime (struct timespec t, bool full_time);

#define GID_FROM_HEADER(where) gid_from_header (where, sizeof (where))
#define MAJOR_FROM_HEADER(where) major_from_header (where, sizeof (where))
#define MINOR_FROM_HEADER(where) minor_from_header (where, sizeof (where))
#define MODE_FROM_HEADER(where, hbits) \
  mode_from_header (where, sizeof (where), hbits)
#define OFF_FROM_HEADER(where) off_from_header (where, sizeof (where))
#define SIZE_FROM_HEADER(where) size_from_header (where, sizeof (where))
#define TIME_FROM_HEADER(where) time_from_header (where, sizeof (where))
#define UID_FROM_HEADER(where) uid_from_header (where, sizeof (where))
#define UINTMAX_FROM_HEADER(where) uintmax_from_header (where, sizeof (where))

gid_t gid_from_header (const char *buf, size_t size);
major_t major_from_header (const char *buf, size_t size);
minor_t minor_from_header (const char *buf, size_t size);
mode_t mode_from_header (const char *buf, size_t size, unsigned *hbits);
off_t off_from_header (const char *buf, size_t size);
size_t size_from_header (const char *buf, size_t size);
time_t time_from_header (const char *buf, size_t size);
uid_t uid_from_header (const char *buf, size_t size);
uintmax_t uintmax_from_header (const char *buf, size_t size);

void list_archive (void);
void print_header (struct tar_stat_info *st, union block *blk,
	           off_t block_ordinal);
void read_and (void (*do_something) (void));
enum read_header read_header (union block **return_block,
			      struct tar_stat_info *info,
			      enum read_header_mode m);
enum read_header tar_checksum (union block *header, bool silent);
void skip_file (off_t size);
void skip_member (void);

/* Module misc.c.  */

void assign_string (char **dest, const char *src);

enum { BILLION = 1000000000, LOG10_BILLION = 9 };

pid_t xfork (void);
void xpipe (int fd[2]);

void *page_aligned_alloc (void **ptr, size_t size);

/* Module tar.c.  */

void tar_stat_init (struct tar_stat_info *st);
void tar_stat_destroy (struct tar_stat_info *st);
void set_exit_status (int val);

/* Module xheader.c.  */

void xheader_init (struct xheader *xhdr);
void xheader_decode (struct tar_stat_info *stat);
void xheader_decode_global (struct xheader *xhdr);
void xheader_read (struct xheader *xhdr, union block *header, size_t size);
void xheader_destroy (struct xheader *hdr);

/* Module system.c */

void sys_wait_for_child (pid_t, bool);
pid_t sys_child_open_for_uncompress (void);
bool sys_get_archive_stat (void);

/* Module sparse.c */
bool sparse_member_p (struct tar_stat_info *st);
bool sparse_fixup_header (struct tar_stat_info *st);
enum dump_status sparse_skip_file (struct tar_stat_info *st);

/* Module utf8.c */
bool string_ascii_p (const char *str);
bool utf8_convert (bool to_utf, char const *input, char **output);

/* Module transform.c */
#define XFORM_REGFILE  0x01
#define XFORM_LINK     0x02
#define XFORM_SYMLINK  0x04
#define XFORM_ALL      (XFORM_REGFILE|XFORM_LINK|XFORM_SYMLINK)

bool transform_member_name (char **pinput, int type);
bool transform_name_fp (char **pinput, int type,
			char *(*fun)(char *, void *), void *);

/* Module suffix.c */
void set_comression_program_by_suffix (const char *name, const char *defprog);

/* Module warning.c */
#define WARN_ALONE_ZERO_BLOCK    0x00000001
#define WARN_BAD_DUMPDIR         0x00000002
#define WARN_CACHEDIR            0x00000004
#define WARN_CONTIGUOUS_CAST     0x00000008
#define WARN_FILE_CHANGED        0x00000010
#define WARN_FILE_IGNORED        0x00000020
#define WARN_FILE_REMOVED        0x00000040
#define WARN_FILE_SHRANK         0x00000080
#define WARN_FILE_UNCHANGED      0x00000100
#define WARN_FILENAME_WITH_NULS  0x00000200
#define WARN_IGNORE_ARCHIVE      0x00000400
#define WARN_IGNORE_NEWER        0x00000800
#define WARN_NEW_DIRECTORY       0x00001000
#define WARN_RENAME_DIRECTORY    0x00002000
#define WARN_TIMESTAMP           0x00008000
#define WARN_UNKNOWN_CAST        0x00010000
#define WARN_UNKNOWN_KEYWORD     0x00020000
#define WARN_XDEV                0x00040000

/* The warnings composing WARN_VERBOSE_WARNINGS are enabled by default
   in verbose mode */
#define WARN_VERBOSE_WARNINGS    (WARN_RENAME_DIRECTORY|WARN_NEW_DIRECTORY)
#define WARN_ALL                 (0xffffffff & ~WARN_VERBOSE_WARNINGS)

void set_warning_option (const char *arg);

extern int warning_option;

#define WARNOPT(opt,args)			\
  do						\
    {						\
      if (warning_option & opt) WARN (args);	\
    }						\
  while (0)

