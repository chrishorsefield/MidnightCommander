/* This file is part of GNU tar. 
   Copyright (C) 2006, 2007, 2008 Free Software Foundation, Inc.

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
#include <regex.h>
#include "common.h"

enum transform_type
  {
    transform_first,
    transform_global
  };

enum replace_segm_type
  {
    segm_literal,   /* Literal segment */
    segm_backref,   /* Back-reference segment */
    segm_case_ctl   /* Case control segment (GNU extension) */
  };

enum case_ctl_type
  {
    ctl_stop,       /* Stop case conversion */ 
    ctl_upcase_next,/* Turn the next character to uppercase */ 
    ctl_locase_next,/* Turn the next character to lowercase */
    ctl_upcase,     /* Turn the replacement to uppercase until ctl_stop */
    ctl_locase      /* Turn the replacement to lowercase until ctl_stop */
  };

struct replace_segm
{
  struct replace_segm *next;
  enum replace_segm_type type;
  union
  {
    struct
    {
      char *ptr;
      size_t size;
    } literal;                /* type == segm_literal */   
    size_t ref;               /* type == segm_backref */
    enum case_ctl_type ctl;   /* type == segm_case_ctl */ 
  } v;
};

struct transform
{
  struct transform *next;
  enum transform_type transform_type;
  int flags;
  unsigned match_number;
  regex_t regex;
  /* Compiled replacement expression */
  struct replace_segm *repl_head, *repl_tail;
  size_t segm_count; /* Number of elements in the above list */
};


/* Run case conversion specified by CASE_CTL on array PTR of SIZE
   characters. Returns pointer to statically allocated storage. */
static char *
run_case_conv (enum case_ctl_type case_ctl, char *ptr, size_t size)
{
  static char *case_ctl_buffer;
  static size_t case_ctl_bufsize;
  char *p;
  
  if (case_ctl_bufsize < size)
    {
      case_ctl_bufsize = size;
      case_ctl_buffer = xrealloc (case_ctl_buffer, case_ctl_bufsize);
    }
  memcpy (case_ctl_buffer, ptr, size);
  switch (case_ctl)
    {
    case ctl_upcase_next:
      case_ctl_buffer[0] = toupper ((unsigned char) case_ctl_buffer[0]);
      break;
      
    case ctl_locase_next:
      case_ctl_buffer[0] = tolower ((unsigned char) case_ctl_buffer[0]);
      break;
      
    case ctl_upcase:
      for (p = case_ctl_buffer; p < case_ctl_buffer + size; p++)
	*p = toupper ((unsigned char) *p);
      break;
      
    case ctl_locase:
      for (p = case_ctl_buffer; p < case_ctl_buffer + size; p++)
	*p = tolower ((unsigned char) *p);
      break;

    case ctl_stop:
      break;
    }
  return case_ctl_buffer;
}


static struct obstack stk;
static bool stk_init;

void
_single_transform_name_to_obstack (struct transform *tf, char *input)
{
  regmatch_t *rmp;
  int rc;
  size_t nmatches = 0;
  enum case_ctl_type case_ctl = ctl_stop,  /* Current case conversion op */
                     save_ctl = ctl_stop;  /* Saved case_ctl for \u and \l */
  
  /* Reset case conversion after a single-char operation */
#define CASE_CTL_RESET()  if (case_ctl == ctl_upcase_next     \
			      || case_ctl == ctl_locase_next) \
                            {                                 \
                              case_ctl = save_ctl;            \
                              save_ctl = ctl_stop;            \
			    }
  
  rmp = xmalloc ((tf->regex.re_nsub + 1) * sizeof (*rmp));

  while (*input)
    {
      size_t disp;
      char *ptr;
      
      rc = regexec (&tf->regex, input, tf->regex.re_nsub + 1, rmp, 0);
      
      if (rc == 0)
	{
	  struct replace_segm *segm;
	  
	  disp = rmp[0].rm_eo;

	  if (rmp[0].rm_so)
	    obstack_grow (&stk, input, rmp[0].rm_so);

	  nmatches++;
	  if (tf->match_number && nmatches < tf->match_number)
	    {
	      obstack_grow (&stk, input, disp);
	      input += disp;
	      continue;
	    }

	  for (segm = tf->repl_head; segm; segm = segm->next)
	    {
	      switch (segm->type)
		{
		case segm_literal:    /* Literal segment */
		  if (case_ctl == ctl_stop)
		    ptr = segm->v.literal.ptr;
		  else
		    {
		      ptr = run_case_conv (case_ctl,
					   segm->v.literal.ptr,
					   segm->v.literal.size);
		      CASE_CTL_RESET();
		    }
		  obstack_grow (&stk, ptr, segm->v.literal.size);
		  break;
	      
		case segm_backref:    /* Back-reference segment */
		  if (rmp[segm->v.ref].rm_so != -1
		      && rmp[segm->v.ref].rm_eo != -1)
		    {
		      size_t size = rmp[segm->v.ref].rm_eo
			              - rmp[segm->v.ref].rm_so;
		      ptr = input + rmp[segm->v.ref].rm_so;
		      if (case_ctl != ctl_stop)
			{
			  ptr = run_case_conv (case_ctl, ptr, size);
			  CASE_CTL_RESET();
			}
		      
		      obstack_grow (&stk, ptr, size);
		    }
		  break;

		case segm_case_ctl:
		  switch (segm->v.ctl)
		    {
		    case ctl_upcase_next:
		    case ctl_locase_next:
		      switch (save_ctl)
			{
			case ctl_stop:
			case ctl_upcase:
			case ctl_locase:
			  save_ctl = case_ctl;
			default:
			  break;
			}
		      /*FALL THROUGH*/
		      
		    case ctl_upcase:
		    case ctl_locase:
		    case ctl_stop:
		      case_ctl = segm->v.ctl;
		    }
		}
	    }
	}
      else
	{
	  disp = strlen (input);
	  obstack_grow (&stk, input, disp);
	}

      input += disp;

      if (tf->transform_type == transform_first)
	{
	  obstack_grow (&stk, input, strlen (input));
	  break;
	}
    }

  obstack_1grow (&stk, 0);
  free (rmp);
}

bool
_transform_name_to_obstack (int flags, char *input, char **output)
{
  bool alloced = false;
  
  if (!stk_init)
    {
      obstack_init (&stk);
      stk_init = true;
    }

  *output = input;
  return alloced;
}
  
bool
transform_name_fp (char **pinput, int flags,
		   char *(*fun)(char *, void *), void *dat)
{
    char *str;
    bool ret = _transform_name_to_obstack (flags, *pinput, &str);
    if (ret)
      {
	assign_string (pinput, fun ? fun (str, dat) : str);
	obstack_free (&stk, str);
      }
    else if (fun)
      {
	*pinput = NULL;
	assign_string (pinput, fun (str, dat));
	free (str);
	ret = true;
      }
    return ret;
}
