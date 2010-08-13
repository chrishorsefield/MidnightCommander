/* POSIX extended headers for tar.

   Copyright (C) 2003, 2004, 2005, 2006, 2007, 2009, 2010 Free Software
   Foundation, Inc.

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


/* Keyword options */

struct keyword_list
{
  struct keyword_list *next;
  char *pattern;
  char *value;
};

static void
xheader_list_append (struct keyword_list **root, char const *kw,
		     char const *value)
{
  struct keyword_list *kp = xmalloc (sizeof *kp);
  kp->pattern = xstrdup (kw);
  kp->value = value ? xstrdup (value) : NULL;
  kp->next = *root;
  *root = kp;
}

/* General Interface */

#define XHDR_PROTECTED 0x01
#define XHDR_GLOBAL    0x02

struct xhdr_tab
{
  char const *keyword;
  void (*coder) (struct tar_stat_info const *, char const *,
		 struct xheader *, void const *data);
  void (*decoder) (struct tar_stat_info *, char const *, char const *, size_t);
  int flags;
};

/* This declaration must be extern, because ISO C99 section 6.9.2
   prohibits a tentative definition that has both internal linkage and
   incomplete type.  If we made it static, we'd have to declare its
   size which would be a maintenance pain; if we put its initializer
   here, we'd need a boatload of forward declarations, which would be
   even more of a pain.  */
extern struct xhdr_tab const xhdr_tab[];

static struct xhdr_tab const *
locate_handler (char const *keyword)
{
  struct xhdr_tab const *p;

  for (p = xhdr_tab; p->keyword; p++)
    if (strcmp (p->keyword, keyword) == 0)
      return p;
  return NULL;
}

/* Decode a single extended header record, advancing *PTR to the next record.
   Return true on success, false otherwise.  */
static bool
decode_record (struct xheader *xhdr,
	       char **ptr,
	       void (*handler) (void *, char const *, char const *, size_t),
	       void *data)
{
  char *start = *ptr;
  char *p = start;
  uintmax_t u;
  size_t len;
  char *len_lim;
  char const *keyword;
  char *nextp;
  size_t len_max = xhdr->buffer + xhdr->size - start;

  while (*p == ' ' || *p == '\t')
    p++;

  if (! ISDIGIT (*p))
    {
      if (*p)
	ERROR ((0, 0, _("Malformed extended header: missing length")));
      return false;
    }

  errno = 0;
  len = u = strtoumax (p, &len_lim, 10);
  if (len != u || errno == ERANGE)
    {
      ERROR ((0, 0, _("Extended header length is out of allowed range")));
      return false;
    }

  if (len_max < len)
    {
      int len_len = len_lim - p;
      ERROR ((0, 0, _("Extended header length %*s is out of range"),
	      len_len, p));
      return false;
    }

  nextp = start + len;

  for (p = len_lim; *p == ' ' || *p == '\t'; p++)
    continue;
  if (p == len_lim)
    {
      ERROR ((0, 0,
	      _("Malformed extended header: missing blank after length")));
      return false;
    }

  keyword = p;
  p = strchr (p, '=');
  if (! (p && p < nextp))
    {
      ERROR ((0, 0, _("Malformed extended header: missing equal sign")));
      return false;
    }

  if (nextp[-1] != '\n')
    {
      ERROR ((0, 0, _("Malformed extended header: missing newline")));
      return false;
    }

  *p = nextp[-1] = '\0';
  handler (data, keyword, p + 1, nextp - p - 2); /* '=' + trailing '\n' */
  *p = '=';
  nextp[-1] = '\n';
  *ptr = nextp;
  return true;
}

static void
decx (void *data, char const *keyword, char const *value, size_t size)
{
  struct xhdr_tab const *t;
  struct tar_stat_info *st = data;

  t = locate_handler (keyword);
  if (t)
    t->decoder (st, keyword, value, size);
  else
    WARNOPT (WARN_UNKNOWN_KEYWORD,
	     (0, 0, _("Ignoring unknown extended header keyword `%s'"),
	      keyword));
}

void
xheader_decode (struct tar_stat_info *st)
{
  if (st->xhdr.size)
    {
      char *p = st->xhdr.buffer + BLOCKSIZE;
      while (decode_record (&st->xhdr, &p, decx, st))
	continue;
    }
}

static void
decg (void *data, char const *keyword, char const *value,
      size_t size __attribute__((unused)))
{
  struct keyword_list **kwl = data;
  struct xhdr_tab const *tab = locate_handler (keyword);
  if (tab && (tab->flags & XHDR_GLOBAL))
    tab->decoder (data, keyword, value, size);
  else
    xheader_list_append (kwl, keyword, value);
}

void
xheader_decode_global (struct xheader *xhdr)
{
  if (xhdr->size)
    {
      char *p = xhdr->buffer + BLOCKSIZE;

      while (decode_record (xhdr, &p, decg, NULL))
	continue;
    }
}

void
xheader_init (struct xheader *xhdr)
{
  if (!xhdr->stk)
    {
      xhdr->stk = xmalloc (sizeof *xhdr->stk);
      obstack_init (xhdr->stk);
    }
}

void
xheader_read (struct xheader *xhdr, union block *p, size_t size)
{
  size_t j = 0;

  size += BLOCKSIZE;
  xhdr->size = size;
  xhdr->buffer = xmalloc (size + 1);
  xhdr->buffer[size] = '\0';

  do
    {
      size_t len = size;

      if (len > BLOCKSIZE)
	len = BLOCKSIZE;

      if (!p)
	FATAL_ERROR ((0, 0, _("Unexpected EOF in archive")));
      
      memcpy (&xhdr->buffer[j], p->buffer, len);
      set_next_block_after (p);

      p = find_next_block ();

      j += len;
      size -= len;
    }
  while (size > 0);
}

void
xheader_destroy (struct xheader *xhdr)
{
  if (xhdr->stk)
    {
      obstack_free (xhdr->stk, NULL);
      free (xhdr->stk);
      xhdr->stk = NULL;
    }
  else
    free (xhdr->buffer);
  xhdr->buffer = 0;
  xhdr->size = 0;
}


/* Implementations */

static void
out_of_range_header (char const *keyword, char const *value,
		     uintmax_t minus_minval, uintmax_t maxval)
{
  char minval_buf[UINTMAX_STRSIZE_BOUND + 1];
  char maxval_buf[UINTMAX_STRSIZE_BOUND];
  char *minval_string = umaxtostr (minus_minval, minval_buf + 1);
  char *maxval_string = umaxtostr (maxval, maxval_buf);
  if (minus_minval)
    *--minval_string = '-';

  /* TRANSLATORS: The first %s is the pax extended header keyword
     (atime, gid, etc.).  */
  ERROR ((0, 0, _("Extended header %s=%s is out of range %s..%s"),
	  keyword, value, minval_string, maxval_string));
}

static void
decode_string (char **string, char const *arg)
{
  if (*string)
    {
      free (*string);
      *string = NULL;
    }
  if (!utf8_convert (false, arg, string))
    {
      /* FIXME: report error and act accordingly to --pax invalid=UTF-8 */
      assign_string (string, arg);
    }
}

enum decode_time_status
  {
    decode_time_success,
    decode_time_range,
    decode_time_bad_header
  };

static enum decode_time_status
_decode_time (struct timespec *ts, char const *arg, char const *keyword)
{
  time_t s;
  unsigned long int ns = 0;
  char *p;
  char *arg_lim;
  bool negative = *arg == '-';

  errno = 0;

  if (ISDIGIT (arg[negative]))
    {
      if (negative)
	{
	  intmax_t i = strtoimax (arg, &arg_lim, 10);
	  if (TYPE_SIGNED (time_t) ? i < TYPE_MINIMUM (time_t) : i < 0)
	    return decode_time_range;
	  s = i;
	}
      else
	{
	  uintmax_t i = strtoumax (arg, &arg_lim, 10);
	  if (TYPE_MAXIMUM (time_t) < i)
	    return decode_time_range;
	  s = i;
	}

      p = arg_lim;

      if (errno == ERANGE)
	return decode_time_range;

      if (*p == '.')
	{
	  int digits = 0;
	  bool trailing_nonzero = false;

	  while (ISDIGIT (*++p))
	    if (digits < LOG10_BILLION)
	      {
		ns = 10 * ns + (*p - '0');
		digits++;
	      }
	    else
	      trailing_nonzero |= *p != '0';

	  while (digits++ < LOG10_BILLION)
	    ns *= 10;

	  if (negative)
	    {
	      /* Convert "-1.10000000000001" to s == -2, ns == 89999999.
		 I.e., truncate time stamps towards minus infinity while
		 converting them to internal form.  */
	      ns += trailing_nonzero;
	      if (ns != 0)
		{
		  if (s == TYPE_MINIMUM (time_t))
		    return decode_time_range;
		  s--;
		  ns = BILLION - ns;
		}
	    }
	}

      if (! *p)
	{
	  ts->tv_sec = s;
	  ts->tv_nsec = ns;
	  return decode_time_success;
	}
    }

  return decode_time_bad_header;
}

static bool
decode_time (struct timespec *ts, char const *arg, char const *keyword)
{
  switch (_decode_time (ts, arg, keyword))
    {
    case decode_time_success:
      return true;
    case decode_time_bad_header:
      ERROR ((0, 0, _("Malformed extended header: invalid %s=%s"),
	      keyword, arg));
      return false;
    case decode_time_range:
      out_of_range_header (keyword, arg, - (uintmax_t) TYPE_MINIMUM (time_t),
			   TYPE_MAXIMUM (time_t));
      return false;
    }
  return true;
}

static bool
decode_num (uintmax_t *num, char const *arg, uintmax_t maxval,
	    char const *keyword)
{
  uintmax_t u;
  char *arg_lim;

  if (! (ISDIGIT (*arg)
	 && (errno = 0, u = strtoumax (arg, &arg_lim, 10), !*arg_lim)))
    {
      ERROR ((0, 0, _("Malformed extended header: invalid %s=%s"),
	      keyword, arg));
      return false;
    }

  if (! (u <= maxval && errno != ERANGE))
    {
      out_of_range_header (keyword, arg, 0, maxval);
      return false;
    }

  *num = u;
  return true;
}

static void
dummy_decoder (struct tar_stat_info *st __attribute__ ((unused)),
	       char const *keyword __attribute__ ((unused)),
	       char const *arg __attribute__ ((unused)),
	       size_t size __attribute__((unused)))
{
}

static void
atime_decoder (struct tar_stat_info *st,
	       char const *keyword,
	       char const *arg,
	       size_t size __attribute__((unused)))
{
  struct timespec ts;
  if (decode_time (&ts, arg, keyword))
    st->atime = ts;
}

static void
gid_decoder (struct tar_stat_info *st,
	     char const *keyword,
	     char const *arg,
	     size_t size __attribute__((unused)))
{
  uintmax_t u;
  if (decode_num (&u, arg, TYPE_MAXIMUM (gid_t), keyword))
    st->stat.st_gid = u;
}

static void
gname_decoder (struct tar_stat_info *st,
	       char const *keyword __attribute__((unused)),
	       char const *arg,
	       size_t size __attribute__((unused)))
{
  decode_string (&st->gname, arg);
}

static void
linkpath_decoder (struct tar_stat_info *st,
		  char const *keyword __attribute__((unused)),
		  char const *arg,
		  size_t size __attribute__((unused)))
{
  decode_string (&st->link_name, arg);
}

static void
ctime_decoder (struct tar_stat_info *st,
	       char const *keyword,
	       char const *arg,
	       size_t size __attribute__((unused)))
{
  struct timespec ts;
  if (decode_time (&ts, arg, keyword))
    st->ctime = ts;
}

static void
mtime_decoder (struct tar_stat_info *st,
	       char const *keyword,
	       char const *arg,
	       size_t size __attribute__((unused)))
{
  struct timespec ts;
  if (decode_time (&ts, arg, keyword))
    st->mtime = ts;
}

static void
path_decoder (struct tar_stat_info *st,
	      char const *keyword __attribute__((unused)),
	      char const *arg,
	      size_t size __attribute__((unused)))
{
  decode_string (&st->orig_file_name, arg);
  decode_string (&st->file_name, arg);
  st->had_trailing_slash = strip_trailing_slashes (st->file_name);
}

static void
size_decoder (struct tar_stat_info *st,
	      char const *keyword,
	      char const *arg,
	      size_t size __attribute__((unused)))
{
  uintmax_t u;
  if (decode_num (&u, arg, TYPE_MAXIMUM (off_t), keyword))
    st->stat.st_size = u;
}

static void
uid_decoder (struct tar_stat_info *st,
	     char const *keyword,
	     char const *arg,
	     size_t size __attribute__((unused)))
{
  uintmax_t u;
  if (decode_num (&u, arg, TYPE_MAXIMUM (uid_t), keyword))
    st->stat.st_uid = u;
}

static void
uname_decoder (struct tar_stat_info *st,
	       char const *keyword __attribute__((unused)),
	       char const *arg,
	       size_t size __attribute__((unused)))
{
  decode_string (&st->uname, arg);
}

static void
sparse_size_decoder (struct tar_stat_info *st,
		     char const *keyword,
		     char const *arg,
		     size_t size __attribute__((unused)))
{
  uintmax_t u;
  if (decode_num (&u, arg, TYPE_MAXIMUM (off_t), keyword))
    st->stat.st_size = u;
}

static void
sparse_numblocks_decoder (struct tar_stat_info *st,
			  char const *keyword,
			  char const *arg,
			  size_t size __attribute__((unused)))
{
  uintmax_t u;
  if (decode_num (&u, arg, SIZE_MAX, keyword))
    {
      st->sparse_map_size = u;
      st->sparse_map = xcalloc (u, sizeof st->sparse_map[0]);
      st->sparse_map_avail = 0;
    }
}

static void
sparse_offset_decoder (struct tar_stat_info *st,
		       char const *keyword,
		       char const *arg,
		       size_t size __attribute__((unused)))
{
  uintmax_t u;
  if (decode_num (&u, arg, TYPE_MAXIMUM (off_t), keyword))
    {
      if (st->sparse_map_avail < st->sparse_map_size)
	st->sparse_map[st->sparse_map_avail].offset = u;
      else
	ERROR ((0, 0, _("Malformed extended header: excess %s=%s"),
		"GNU.sparse.offset", arg));
    }
}

static void
sparse_numbytes_decoder (struct tar_stat_info *st,
			 char const *keyword,
			 char const *arg,
			 size_t size __attribute__((unused)))
{
  uintmax_t u;
  if (decode_num (&u, arg, SIZE_MAX, keyword))
    {
      if (st->sparse_map_avail < st->sparse_map_size)
	st->sparse_map[st->sparse_map_avail++].numbytes = u;
      else
	ERROR ((0, 0, _("Malformed extended header: excess %s=%s"),
		keyword, arg));
    }
}

static void
sparse_map_decoder (struct tar_stat_info *st,
		    char const *keyword,
		    char const *arg,
		    size_t size __attribute__((unused)))
{
  int offset = 1;

  st->sparse_map_avail = 0;
  while (1)
    {
      uintmax_t u;
      char *delim;
      struct sp_array e;

      if (!ISDIGIT (*arg))
	{
	  ERROR ((0, 0, _("Malformed extended header: invalid %s=%s"),
		  keyword, arg));
	  return;
	}

      errno = 0;
      u = strtoumax (arg, &delim, 10);
      if (offset)
	{
	  e.offset = u;
	  if (!(u == e.offset && errno != ERANGE))
	    {
	      out_of_range_header (keyword, arg, 0, TYPE_MAXIMUM (off_t));
	      return;
	    }
	}
      else
	{
	  e.numbytes = u;
	  if (!(u == e.numbytes && errno != ERANGE))
	    {
	      out_of_range_header (keyword, arg, 0, TYPE_MAXIMUM (size_t));
	      return;
	    }
	  if (st->sparse_map_avail < st->sparse_map_size)
	    st->sparse_map[st->sparse_map_avail++] = e;
	  else
	    {
	      ERROR ((0, 0, _("Malformed extended header: excess %s=%s"),
		      keyword, arg));
	      return;
	    }
	}

      offset = !offset;

      if (*delim == 0)
	break;
      else if (*delim != ',')
	{
	  ERROR ((0, 0,
		  _("Malformed extended header: invalid %s: unexpected delimiter %c"),
		  keyword, *delim));
	  return;
	}

      arg = delim + 1;
    }

  if (!offset)
    ERROR ((0, 0,
	    _("Malformed extended header: invalid %s: odd number of values"),
	    keyword));
}

static void
dumpdir_decoder (struct tar_stat_info *st,
		 char const *keyword __attribute__((unused)),
		 char const *arg,
		 size_t size)
{
  st->dumpdir = xmalloc (size);
  memcpy (st->dumpdir, arg, size);
}

static void
volume_label_decoder (struct tar_stat_info *st,
		      char const *keyword __attribute__((unused)),
		      char const *arg,
		      size_t size __attribute__((unused)))
{
  decode_string (&volume_label, arg);
}

static void
volume_size_decoder (struct tar_stat_info *st,
		     char const *keyword,
		     char const *arg, size_t size)
{
  uintmax_t u;
  if (decode_num (&u, arg, TYPE_MAXIMUM (uintmax_t), keyword))
    continued_file_size = u;
}

static void
volume_offset_decoder (struct tar_stat_info *st,
		       char const *keyword,
		       char const *arg, size_t size)
{
  uintmax_t u;
  if (decode_num (&u, arg, TYPE_MAXIMUM (uintmax_t), keyword))
    continued_file_offset = u;
}

static void
volume_filename_decoder (struct tar_stat_info *st,
			 char const *keyword __attribute__((unused)),
			 char const *arg,
			 size_t size __attribute__((unused)))
{
  decode_string (&continued_file_name, arg);
}

static void
sparse_major_decoder (struct tar_stat_info *st,
		      char const *keyword,
		      char const *arg,
		      size_t size)
{
  uintmax_t u;
  if (decode_num (&u, arg, TYPE_MAXIMUM (unsigned), keyword))
    st->sparse_major = u;
}

static void
sparse_minor_decoder (struct tar_stat_info *st,
		      char const *keyword,
		      char const *arg,
		      size_t size)
{
  uintmax_t u;
  if (decode_num (&u, arg, TYPE_MAXIMUM (unsigned), keyword))
    st->sparse_minor = u;
}

struct xhdr_tab const xhdr_tab[] = {
  { "atime",	NULL,		atime_decoder,	  0 },
  { "comment",	NULL,		dummy_decoder,	  0 },
  { "charset",	NULL,		dummy_decoder,	  0 },
  { "ctime",	NULL,		ctime_decoder,	  0 },
  { "gid",	NULL,		gid_decoder,	  0 },
  { "gname",	NULL,		gname_decoder,	  0 },
  { "linkpath",	NULL,		linkpath_decoder, 0 },
  { "mtime",	NULL,		mtime_decoder,	  0 },
  { "path",	NULL,		path_decoder,	  0 },
  { "size",	NULL,		size_decoder,	  0 },
  { "uid",	NULL,		uid_decoder,	  0 },
  { "uname",	NULL,		uname_decoder,	  0 },

  /* Sparse file handling */
  { "GNU.sparse.name",       NULL, path_decoder,
    XHDR_PROTECTED },
  { "GNU.sparse.major",      NULL, sparse_major_decoder,
    XHDR_PROTECTED },
  { "GNU.sparse.minor",      NULL, sparse_minor_decoder,
    XHDR_PROTECTED },
  { "GNU.sparse.realsize",   NULL, sparse_size_decoder,
    XHDR_PROTECTED },
  { "GNU.sparse.numblocks",  NULL, sparse_numblocks_decoder,
    XHDR_PROTECTED },

  /* tar 1.14 - 1.15.90 keywords. */
  { "GNU.sparse.size",       NULL, sparse_size_decoder,
    XHDR_PROTECTED },
  /* tar 1.14 - 1.15.1 keywords. Multiple instances of these appeared in 'x'
     headers, and each of them was meaningful. It confilcted with POSIX specs,
     which requires that "when extended header records conflict, the last one
     given in the header shall take precedence." */
  { "GNU.sparse.offset",     NULL, sparse_offset_decoder,
    XHDR_PROTECTED },
  { "GNU.sparse.numbytes",   NULL, sparse_numbytes_decoder,
    XHDR_PROTECTED },
  /* tar 1.15.90 keyword, introduced to remove the above-mentioned conflict. */
  { "GNU.sparse.map",        NULL /* Unused, see pax_dump_header() */,
    sparse_map_decoder, 0 },

  { "GNU.dumpdir",           NULL, dumpdir_decoder,
    XHDR_PROTECTED },

  /* Keeps the tape/volume label. May be present only in the global headers.
     Equivalent to GNUTYPE_VOLHDR.  */
  { "GNU.volume.label", NULL, volume_label_decoder,
    XHDR_PROTECTED | XHDR_GLOBAL },

  /* These may be present in a first global header of the archive.
     They provide the same functionality as GNUTYPE_MULTIVOL header.
     The GNU.volume.size keeps the real_s_sizeleft value, which is
     otherwise kept in the size field of a multivolume header.  The
     GNU.volume.offset keeps the offset of the start of this volume,
     otherwise kept in oldgnu_header.offset.  */
  { "GNU.volume.filename", NULL, volume_filename_decoder,
    XHDR_PROTECTED | XHDR_GLOBAL },
  { "GNU.volume.size", NULL, volume_size_decoder,
    XHDR_PROTECTED | XHDR_GLOBAL },
  { "GNU.volume.offset", NULL, volume_offset_decoder,
    XHDR_PROTECTED | XHDR_GLOBAL },

  { NULL, NULL, NULL, 0 }
};
