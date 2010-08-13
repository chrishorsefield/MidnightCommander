/* Functions for dealing with sparse files

   Copyright (C) 2003, 2004, 2005, 2006, 2007 Free Software Foundation, Inc.

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
#include <inttostr.h>
#include "common.h"

struct tar_sparse_file;
static bool sparse_select_optab (struct tar_sparse_file *file);

enum sparse_scan_state
  {
    scan_begin,
    scan_block,
    scan_end
  };

struct tar_sparse_optab
{
  bool (*init) (struct tar_sparse_file *);
  bool (*done) (struct tar_sparse_file *);
  bool (*sparse_member_p) (struct tar_sparse_file *);
  bool (*dump_header) (struct tar_sparse_file *);
  bool (*fixup_header) (struct tar_sparse_file *);
  bool (*decode_header) (struct tar_sparse_file *);
  bool (*scan_block) (struct tar_sparse_file *, enum sparse_scan_state,
		      void *);
  bool (*dump_region) (struct tar_sparse_file *, size_t);
  bool (*extract_region) (struct tar_sparse_file *, size_t);
};

struct tar_sparse_file
{
  int fd;                           /* File descriptor */
  bool seekable;                    /* Is fd seekable? */
  off_t offset;                     /* Current offset in fd if seekable==false.
				       Otherwise unused */
  off_t dumped_size;                /* Number of bytes actually written
				       to the archive */
  struct tar_stat_info *stat_info;  /* Information about the file */
  struct tar_sparse_optab const *optab; /* Operation table */
  void *closure;                    /* Any additional data optab calls might
				       require */
};

static bool
tar_sparse_member_p (struct tar_sparse_file *file)
{
  if (file->optab->sparse_member_p)
    return file->optab->sparse_member_p (file);
  return false;
}

static bool
tar_sparse_init (struct tar_sparse_file *file)
{
  memset (file, 0, sizeof *file);

  if (!sparse_select_optab (file))
    return false;

  if (file->optab->init)
    return file->optab->init (file);

  return true;
}

static bool
tar_sparse_done (struct tar_sparse_file *file)
{
  if (file->optab->done)
    return file->optab->done (file);
  return true;
}

static bool
tar_sparse_decode_header (struct tar_sparse_file *file)
{
  if (file->optab->decode_header)
    return file->optab->decode_header (file);
  return true;
}

static bool
tar_sparse_fixup_header (struct tar_sparse_file *file)
{
  if (file->optab->fixup_header)
    return file->optab->fixup_header (file);
  return true;
}



static void
sparse_add_map (struct tar_stat_info *st, struct sp_array const *sp)
{
  struct sp_array *sparse_map = st->sparse_map;
  size_t avail = st->sparse_map_avail;
  if (avail == st->sparse_map_size)
    st->sparse_map = sparse_map =
      x2nrealloc (sparse_map, &st->sparse_map_size, sizeof *sparse_map);
  sparse_map[avail] = *sp;
  st->sparse_map_avail = avail + 1;
}

static struct tar_sparse_optab const oldgnu_optab;
static struct tar_sparse_optab const star_optab;
static struct tar_sparse_optab const pax_optab;

static bool
sparse_select_optab (struct tar_sparse_file *file)
{
  switch (current_format == DEFAULT_FORMAT ? archive_format : current_format)
    {
    case V7_FORMAT:
    case USTAR_FORMAT:
      return false;

    case OLDGNU_FORMAT:
    case GNU_FORMAT: /*FIXME: This one should disappear? */
      file->optab = &oldgnu_optab;
      break;

    case POSIX_FORMAT:
      file->optab = &pax_optab;
      break;

    case STAR_FORMAT:
      file->optab = &star_optab;
      break;

    default:
      return false;
    }
  return true;
}

bool
sparse_member_p (struct tar_stat_info *st)
{
  struct tar_sparse_file file;

  if (!tar_sparse_init (&file))
    return false;
  file.stat_info = st;
  return tar_sparse_member_p (&file);
}

bool
sparse_fixup_header (struct tar_stat_info *st)
{
  struct tar_sparse_file file;

  if (!tar_sparse_init (&file))
    return false;
  file.stat_info = st;
  return tar_sparse_fixup_header (&file);
}

enum dump_status
sparse_skip_file (struct tar_stat_info *st)
{
  bool rc = true;
  struct tar_sparse_file file;

  if (!tar_sparse_init (&file))
    return dump_status_not_implemented;

  file.stat_info = st;
  file.fd = -1;

  rc = tar_sparse_decode_header (&file);
  skip_file (file.stat_info->archive_file_size - file.dumped_size);
  return (tar_sparse_done (&file) && rc) ? dump_status_ok : dump_status_short;
}


/* Old GNU Format. The sparse file information is stored in the
   oldgnu_header in the following manner:

   The header is marked with type 'S'. Its `size' field contains
   the cumulative size of all non-empty blocks of the file. The
   actual file size is stored in `realsize' member of oldgnu_header.

   The map of the file is stored in a list of `struct sparse'.
   Each struct contains offset to the block of data and its
   size (both as octal numbers). The first file header contains
   at most 4 such structs (SPARSES_IN_OLDGNU_HEADER). If the map
   contains more structs, then the field `isextended' of the main
   header is set to 1 (binary) and the `struct sparse_header'
   header follows, containing at most 21 following structs
   (SPARSES_IN_SPARSE_HEADER). If more structs follow, `isextended'
   field of the extended header is set and next  next extension header
   follows, etc... */

enum oldgnu_add_status
  {
    add_ok,
    add_finish,
    add_fail
  };

static bool
oldgnu_sparse_member_p (struct tar_sparse_file *file __attribute__ ((unused)))
{
  return current_header->header.typeflag == GNUTYPE_SPARSE;
}

/* Add a sparse item to the sparse file and its obstack */
static enum oldgnu_add_status
oldgnu_add_sparse (struct tar_sparse_file *file, struct sparse *s)
{
  struct sp_array sp;

  if (s->numbytes[0] == '\0')
    return add_finish;
  sp.offset = OFF_FROM_HEADER (s->offset);
  sp.numbytes = SIZE_FROM_HEADER (s->numbytes);
  if (sp.offset < 0
      || file->stat_info->stat.st_size < sp.offset + sp.numbytes
      || file->stat_info->archive_file_size < 0)
    return add_fail;

  sparse_add_map (file->stat_info, &sp);
  return add_ok;
}

static bool
oldgnu_fixup_header (struct tar_sparse_file *file)
{
  /* NOTE! st_size was initialized from the header
     which actually contains archived size. The following fixes it */
  file->stat_info->archive_file_size = file->stat_info->stat.st_size;
  file->stat_info->stat.st_size =
    OFF_FROM_HEADER (current_header->oldgnu_header.realsize);
  return true;
}

/* Convert old GNU format sparse data to internal representation */
static bool
oldgnu_get_sparse_info (struct tar_sparse_file *file)
{
  size_t i;
  union block *h = current_header;
  int ext_p;
  enum oldgnu_add_status rc;

  file->stat_info->sparse_map_avail = 0;
  for (i = 0; i < SPARSES_IN_OLDGNU_HEADER; i++)
    {
      rc = oldgnu_add_sparse (file, &h->oldgnu_header.sp[i]);
      if (rc != add_ok)
	break;
    }

  for (ext_p = h->oldgnu_header.isextended;
       rc == add_ok && ext_p; ext_p = h->sparse_header.isextended)
    {
      h = find_next_block ();
      if (!h)
	{
	  ERROR ((0, 0, _("Unexpected EOF in archive")));
	  return false;
	}
      set_next_block_after (h);
      for (i = 0; i < SPARSES_IN_SPARSE_HEADER && rc == add_ok; i++)
	rc = oldgnu_add_sparse (file, &h->sparse_header.sp[i]);
    }

  if (rc == add_fail)
    {
      ERROR ((0, 0, _("%s: invalid sparse archive member"),
	      file->stat_info->orig_file_name));
      return false;
    }
  return true;
}

static struct tar_sparse_optab const oldgnu_optab = {
  NULL,  /* No init function */
  NULL,  /* No done function */
  oldgnu_sparse_member_p,
  NULL,
  oldgnu_fixup_header,
  oldgnu_get_sparse_info,
  NULL,  /* No scan_block function */
  NULL,
  NULL,
};


/* Star */

static bool
star_sparse_member_p (struct tar_sparse_file *file __attribute__ ((unused)))
{
  return current_header->header.typeflag == GNUTYPE_SPARSE;
}

static bool
star_fixup_header (struct tar_sparse_file *file)
{
  /* NOTE! st_size was initialized from the header
     which actually contains archived size. The following fixes it */
  file->stat_info->archive_file_size = file->stat_info->stat.st_size;
  file->stat_info->stat.st_size =
            OFF_FROM_HEADER (current_header->star_in_header.realsize);
  return true;
}

/* Convert STAR format sparse data to internal representation */
static bool
star_get_sparse_info (struct tar_sparse_file *file)
{
  size_t i;
  union block *h = current_header;
  int ext_p;
  enum oldgnu_add_status rc = add_ok;

  file->stat_info->sparse_map_avail = 0;

  if (h->star_in_header.prefix[0] == '\0'
      && h->star_in_header.sp[0].offset[10] != '\0')
    {
      /* Old star format */
      for (i = 0; i < SPARSES_IN_STAR_HEADER; i++)
	{
	  rc = oldgnu_add_sparse (file, &h->star_in_header.sp[i]);
	  if (rc != add_ok)
	    break;
	}
      ext_p = h->star_in_header.isextended;
    }
  else
    ext_p = 1;

  for (; rc == add_ok && ext_p; ext_p = h->star_ext_header.isextended)
    {
      h = find_next_block ();
      if (!h)
	{
	  ERROR ((0, 0, _("Unexpected EOF in archive")));
	  return false;
	}
      set_next_block_after (h);
      for (i = 0; i < SPARSES_IN_STAR_EXT_HEADER && rc == add_ok; i++)
	rc = oldgnu_add_sparse (file, &h->star_ext_header.sp[i]);
    }

  if (rc == add_fail)
    {
      ERROR ((0, 0, _("%s: invalid sparse archive member"),
	      file->stat_info->orig_file_name));
      return false;
    }
  return true;
}


static struct tar_sparse_optab const star_optab = {
  NULL,  /* No init function */
  NULL,  /* No done function */
  star_sparse_member_p,
  NULL,
  star_fixup_header,
  star_get_sparse_info,
  NULL,  /* No scan_block function */
  NULL, /* No dump region function */
  NULL,
};


/* GNU PAX sparse file format. There are several versions:

   * 0.0

   The initial version of sparse format used by tar 1.14-1.15.1.
   The sparse file map is stored in x header:

   GNU.sparse.size      Real size of the stored file
   GNU.sparse.numblocks Number of blocks in the sparse map
   repeat numblocks time
     GNU.sparse.offset    Offset of the next data block
     GNU.sparse.numbytes  Size of the next data block
   end repeat

   This has been reported as conflicting with the POSIX specs. The reason is
   that offsets and sizes of non-zero data blocks were stored in multiple
   instances of GNU.sparse.offset/GNU.sparse.numbytes variables, whereas
   POSIX requires the latest occurrence of the variable to override all
   previous occurrences.
   
   To avoid this incompatibility two following versions were introduced.

   * 0.1

   Used by tar 1.15.2 -- 1.15.91 (alpha releases).
   
   The sparse file map is stored in
   x header:

   GNU.sparse.size      Real size of the stored file
   GNU.sparse.numblocks Number of blocks in the sparse map
   GNU.sparse.map       Map of non-null data chunks. A string consisting
                       of comma-separated values "offset,size[,offset,size]..."

   The resulting GNU.sparse.map string can be *very* long. While POSIX does not
   impose any limit on the length of a x header variable, this can confuse some
   tars.

   * 1.0

   Starting from this version, the exact sparse format version is specified
   explicitely in the header using the following variables:

   GNU.sparse.major     Major version 
   GNU.sparse.minor     Minor version

   X header keeps the following variables:
   
   GNU.sparse.name      Real file name of the sparse file
   GNU.sparse.realsize  Real size of the stored file (corresponds to the old
                        GNU.sparse.size variable)

   The name field of the ustar header is constructed using the pattern
   "%d/GNUSparseFile.%p/%f".
   
   The sparse map itself is stored in the file data block, preceding the actual
   file data. It consists of a series of octal numbers of arbitrary length,
   delimited by newlines. The map is padded with nulls to the nearest block
   boundary.

   The first number gives the number of entries in the map. Following are map
   entries, each one consisting of two numbers giving the offset and size of
   the data block it describes.

   The format is designed in such a way that non-posix aware tars and tars not
   supporting GNU.sparse.* keywords will extract each sparse file in its
   condensed form with the file map attached and will place it into a separate
   directory. Then, using a simple program it would be possible to expand the
   file to its original form even without GNU tar.

   Bu default, v.1.0 archives are created. To use other formats,
   --sparse-version option is provided. Additionally, v.0.0 can be obtained
   by deleting GNU.sparse.map from 0.1 format: --sparse-version 0.1
   --pax-option delete=GNU.sparse.map
*/

static bool
pax_sparse_member_p (struct tar_sparse_file *file)
{
  return file->stat_info->sparse_map_avail > 0
          || file->stat_info->sparse_major > 0;
}

static bool
decode_num (uintmax_t *num, char const *arg, uintmax_t maxval)
{
  uintmax_t u;
  char *arg_lim;

  if (!ISDIGIT (*arg))
    return false;
  
  u = strtoumax (arg, &arg_lim, 10);

  if (! (u <= maxval && errno != ERANGE) || *arg_lim)
    return false;
  
  *num = u;
  return true;
}

static bool
pax_decode_header (struct tar_sparse_file *file)
{
  if (file->stat_info->sparse_major > 0)
    {
      uintmax_t u;
      char nbuf[UINTMAX_STRSIZE_BOUND];
      union block *blk;
      char *p;
      size_t i;

#define COPY_BUF(b,buf,src) do                                     \
 {                                                                 \
   char *endp = b->buffer + BLOCKSIZE;                             \
   char *dst = buf;                                                \
   do                                                              \
     {                                                             \
       if (dst == buf + UINTMAX_STRSIZE_BOUND -1)                  \
         {                                                         \
           ERROR ((0, 0, _("%s: numeric overflow in sparse archive member"), \
	          file->stat_info->orig_file_name));               \
           return false;                                           \
         }                                                         \
       if (src == endp)                                            \
	 {                                                         \
	   set_next_block_after (b);                               \
           file->dumped_size += BLOCKSIZE;                         \
           b = find_next_block ();                                 \
           src = b->buffer;                                        \
	   endp = b->buffer + BLOCKSIZE;                           \
	 }                                                         \
       *dst = *src++;                                              \
     }                                                             \
   while (*dst++ != '\n');                                         \
   dst[-1] = 0;                                                    \
 } while (0)                       

      set_next_block_after (current_header);
      file->dumped_size += BLOCKSIZE;
      blk = find_next_block ();
      p = blk->buffer;
      COPY_BUF (blk,nbuf,p);
      if (!decode_num (&u, nbuf, TYPE_MAXIMUM (size_t)))
	{
	  ERROR ((0, 0, _("%s: malformed sparse archive member"), 
		  file->stat_info->orig_file_name));
	  return false;
	}
      file->stat_info->sparse_map_size = u;
      file->stat_info->sparse_map = xcalloc (file->stat_info->sparse_map_size,
					     sizeof (*file->stat_info->sparse_map));
      file->stat_info->sparse_map_avail = 0;
      for (i = 0; i < file->stat_info->sparse_map_size; i++)
	{
	  struct sp_array sp;
	  
	  COPY_BUF (blk,nbuf,p);
	  if (!decode_num (&u, nbuf, TYPE_MAXIMUM (off_t)))
	    {
	      ERROR ((0, 0, _("%s: malformed sparse archive member"), 
		      file->stat_info->orig_file_name));
	      return false;
	    }
	  sp.offset = u;
	  COPY_BUF (blk,nbuf,p);
	  if (!decode_num (&u, nbuf, TYPE_MAXIMUM (size_t)))
	    {
	      ERROR ((0, 0, _("%s: malformed sparse archive member"), 
		      file->stat_info->orig_file_name));
	      return false;
	    }
	  sp.numbytes = u;
	  sparse_add_map (file->stat_info, &sp);
	}
      set_next_block_after (blk);
    }
  
  return true;
}

static struct tar_sparse_optab const pax_optab = {
  NULL,  /* No init function */
  NULL,  /* No done function */
  pax_sparse_member_p,
  NULL,
  NULL,
  pax_decode_header,  
  NULL,  /* No scan_block function */
  NULL,
  NULL,
};
