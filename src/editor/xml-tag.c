/*
   XML pair tags highlighting support.

   Copyright (C) 2012
   The Free Software Foundation, Inc.

   Written by:
   Ilia Maslakov <il.smind@gmail.com>, 2012
   Slava Zanko <slavazanko@gmail.com>, 2012

   This file is part of the Midnight Commander.

   The Midnight Commander is free software: you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the License,
   or (at your option) any later version.

   The Midnight Commander is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>

#include <stdlib.h>
#include <string.h>

#include "xml-tag.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

typedef struct
{
    struct
    {
        mc_search_t *search;
        off_t start;
        off_t end;
    } open;

    struct
    {
        mc_search_t *search;
        off_t start;
        off_t end;
    } close;

    gboolean backward;
    gsize found_len;
    int tags_count;
} xmltag_info_t;

typedef struct
{
    char *text;
    off_t start;
    gsize len;

} xmltag_match_word_t;

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static mc_search_t *
xmltag_create_search_object (const char *open_bracket, const char *text, const char *close_bracket)
{
    mc_search_t *xmltag_search;
    char *xml_tag;

    xml_tag = g_strconcat (open_bracket, text, close_bracket, NULL);

    xmltag_search = mc_search_new (xml_tag, -1);
    xmltag_search->search_type = MC_SEARCH_T_REGEX;
    xmltag_search->search_fn = edit_search_cmd_callback;

    g_free (xml_tag);

    return xmltag_search;
}

/* --------------------------------------------------------------------------------------------- */

static void
xmltag_init_struct (WEdit * edit, xmltag_info_t * info, xmltag_match_word_t match_word,
                    gboolean backward, gboolean in_screen)
{
    info->found_len = 0;
    info->tags_count = 0;
    info->backward = backward;
    if (backward)
    {
        if (in_screen)
            /* seach only on the current screen */
            info->open.start = edit_move_backward (edit, match_word.start, WIDGET (edit)->lines);
        else
            info->open.start = 0;

        info->close.start = match_word.start - 1;
        info->close.end = info->close.start;
    }
    else
    {
        info->open.start = match_word.start + match_word.len;
        if (in_screen)
            /* seach only on the current screen */
            info->open.end = edit_move_forward (edit, match_word.start, WIDGET (edit)->lines, 0);
        else
            info->open.end = edit->last_byte;

        info->close.start = info->open.start + 1;
        info->close.end = info->open.end;
    }
    info->open.search = xmltag_create_search_object ("<", match_word.text, "[>\\s]");
    info->close.search = xmltag_create_search_object ("</", match_word.text, ">");
}

/* --------------------------------------------------------------------------------------------- */

static void
xmltag_deinit_struct (xmltag_info_t info)
{
    mc_search_free (info.open.search);
    mc_search_free (info.close.search);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
xmltag_check_tags_count (WEdit * edit, xmltag_match_word_t match_word, xmltag_info_t * info)
{
    if (info->backward)
    {
        if (info->tags_count == 0)
        {
            edit->xmltag.close.pos = match_word.start - 2;
            edit->xmltag.close.len = match_word.len + 3;
            edit->xmltag.open.pos = (off_t) info->open.search->normal_offset;
            edit->xmltag.open.len = info->found_len;
            return TRUE;
        }
    }
    else
    {
        if (info->tags_count == 0)
        {
            edit->xmltag.open.pos = match_word.start - 1;
            edit->xmltag.open.len = match_word.len + 2;
            edit->xmltag.close.pos = (off_t) info->close.search->normal_offset;
            edit->xmltag.close.len = info->found_len;
            return TRUE;
        }
        else
            info->close.start = (off_t) info->close.search->normal_offset + 1;
    }
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
xmltag_find_backward (WEdit * edit, xmltag_match_word_t match_word, xmltag_info_t * info)
{
    off_t bol;
    off_t bot = info->open.start;
    off_t i = info->close.start;
    off_t k, k1;
    off_t j;
    int delta = match_word.len + 1;

    info->tags_count = 1;
    k1 = k = j = i - 1;

    while (j > bot)
    {
        bol = edit_bol (edit, j);
        j = bol - 1;

        while (i > bol)
        {
            i -= delta;
            if (mc_search_run (info->close.search, (void *) edit, i, k1, &info->found_len))
            {
                info->tags_count++;
                k = info->close.search->normal_offset;
            }
            if (mc_search_run (info->open.search, (void *) edit, i, k1, &info->found_len))
            {
                info->tags_count--;
                k = info->open.search->normal_offset;
                if (xmltag_check_tags_count (edit, match_word, info))
                    return TRUE;
            }
            k1 = k;
        }
    }
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
xmltag_find_forward (WEdit * edit, xmltag_match_word_t match_word, xmltag_info_t * info)
{
    while (mc_search_run (info->open.search, (void *) edit, info->open.start,
                          info->open.end, &info->found_len))
    {
        info->tags_count++;

        if (info->tags_count == 0)
            break;

        info->close.end = info->open.search->normal_offset;

        while (mc_search_run (info->close.search, (void *) edit, info->close.start,
                              info->close.end, &info->found_len))
        {
            info->tags_count--;
            if (xmltag_check_tags_count (edit, match_word, info))
                return TRUE;
        }
        info->open.start = info->open.search->normal_offset + 1;
    }

    info->found_len = 0;
    info->close.end = info->open.end;

    while (mc_search_run (info->close.search, (void *) edit, info->close.start,
                          info->close.end, &info->found_len))
    {
        if (xmltag_check_tags_count (edit, match_word, info))
            return TRUE;
        info->tags_count--;
    }

    return FALSE;

}

/* --------------------------------------------------------------------------------------------- */
/** Find the matching bracket in either direction, and sets edit->bracket.
 *
 * @param edit editor object
 * @param in_screen seach only on the current screen
 *
 * @return result of searching. TRUE on success
 */

static gboolean
xmltag_get_pair_tag (WEdit * edit, gboolean in_screen)
{
    gsize cut_len = 0;
    xmltag_info_t info;
    xmltag_match_word_t match_word;
    gboolean result = FALSE;

    if (!visualize_tags)
        return FALSE;

    memset (&match_word, 0, sizeof (match_word));

    edit->xmltag.open.pos = 0;
    edit->xmltag.open.len = 0;
    edit->xmltag.close.pos = 0;
    edit->xmltag.close.len = 0;

    /* search start of the current word */
    match_word.text =
        edit_get_word_from_pos (edit, edit->curs1, &match_word.start, &match_word.len, &cut_len);

    if (match_word.start > 0 && edit_get_byte (edit, match_word.start - 1) == '<')
    {
        xmltag_init_struct (edit, &info, match_word, FALSE, in_screen);
        result = xmltag_find_forward (edit, match_word, &info);
        xmltag_deinit_struct (info);
    }
    else if (match_word.start > 1 && edit_get_byte (edit, match_word.start - 2) == '<'
             && edit_get_byte (edit, match_word.start - 1) == '/')
    {
        xmltag_init_struct (edit, &info, match_word, TRUE, in_screen);
        result = xmltag_find_backward (edit, match_word, &info);
        xmltag_deinit_struct (info);
    }

    return result;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */
/**
 * Find and highlight pair for xml tag.
 * @param edit editor object
 */

void
edit_find_xmlpair (WEdit * edit)
{
    if (xmltag_get_pair_tag (edit, TRUE))
    {
        if (edit->xmltag.close.last != edit->xmltag.close.pos)
            edit->force |= REDRAW_PAGE;
        edit->xmltag.close.last = edit->xmltag.close.pos;
    }
}

/* --------------------------------------------------------------------------------------------- */
