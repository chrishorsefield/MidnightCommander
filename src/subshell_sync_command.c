/* Synchronization between command line widget and command line subshell.
   Copyright (C) 2011 Free Software Foundation, Inc.

   Written by:
   Slava Zanko <slavazanko@gmail.com>, 2011

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

   This widget is derived from the WInput widget, it's used to cope
   with all the magic of the command input line, we depend on some
   help from the program's callback.

 */

/** \file subshell_sync_command.c
 *  \brief Source: Synchronization between command line widget and command line subshell.
 */

#include <config.h>

#include "lib/global.h"

#ifdef HAVE_SUBSHELL_SUPPORT

#include "lib/tty/key.h"        /* key_def_t routines */
#include "filemanager/command.h"        /* cmdline */

#include "subshell_sync_command.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

typedef enum
{
    AC_NONE,
    AC_PRESSED_FIRST,
    AC_PRESSED_FIRST_NEED_SECOND,
    AC_PRESSED_SECOND,
    AC_INPROGRESS
}
autocompletion_stage_t;

typedef enum
{
    SSYNC_NONE,
    SSYNC_TAB_KEY,
}
subshell_sync_action_t;

/*** file scope variables ************************************************************************/
static gboolean sync_initialized = FALSE;
static gboolean key_pressed = FALSE;
static autocompletion_stage_t autocompletion_status = AC_NONE;

static key_def_t *sync_keymap = NULL;   /* for associate shell ESC-sequences to WInput actions */

static key_define_t bash_keydefs[] = {
    {KEY_UP, ESC_STR "[A", MCKEY_NOACTION},
    {KEY_DOWN, ESC_STR "[B", MCKEY_NOACTION},
    {KEY_RIGHT, ESC_STR "[C", MCKEY_NOACTION},
    {KEY_LEFT, ESC_STR "[D", MCKEY_NOACTION},
    {KEY_BACKSPACE, "\177", MCKEY_NOACTION},
    {KEY_M_CTRL | 'd',  "\004", MCKEY_NOACTION},
    {KEY_M_CTRL | 'o',  "\017", MCKEY_NOACTION},
    {KEY_ENTER, "\n", MCKEY_NOACTION},
    {/*KEY_M_CTRL | 'k'*/ 013, "\013", MCKEY_NOACTION},
    {KEY_M_CTRL | 'w', "\027", MCKEY_NOACTION},
    {KEY_DC, ESC_STR "[3~", MCKEY_NOACTION},
    {'\t', "\t", MCKEY_NOACTION},
    {0, NULL, MCKEY_NOACTION},
};

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void
subshell_sync_handle_autocompletion(int key_code, const char *data_from_keyboard, size_t length)
{
    (void) key_code;
    (void) data_from_keyboard;
    (void) length;

    if (autocompletion_status == AC_NONE)
    {
        mc_log ("got TAB char. Autocompletion?\n");
        if (autocompletion_status == AC_PRESSED_FIRST_NEED_SECOND)
        {
            mc_log ("got TAB char. Autocompletion menu?\n");
            autocompletion_status = AC_PRESSED_SECOND;
            return;
        }
        autocompletion_status = AC_PRESSED_FIRST;
        return;
    }

    if (autocompletion_status == AC_PRESSED_SECOND)
    {
        mc_log ("Inside autocompletion menu. Need to describe exit routines\n");
        autocompletion_status = AC_NONE;
        return;
    }
    autocompletion_status = AC_NONE;

}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */
/**
 * Initialization of synchronization benween subshell command line and panels command line
 *
 * @param subshell_type type of shell (bash, zsh etc)
 */

void
subshell_sync_command_init (subshell_type_t subshell_type)
{
    key_define_t *keydefs = NULL;

    if (sync_initialized)
        return;

    switch (subshell_type)
    {
    case BASH:
        keydefs = bash_keydefs;
        sync_initialized = TRUE;
        break;
    default:
        break;
    }
    if (keydefs != NULL)
    {
        sync_initialized = TRUE;
        key_def_define_sequences (&sync_keymap, keydefs);
    }

}

/* --------------------------------------------------------------------------------------------- */
/**
 * Denitialization of synchronization benween subshell command line and panels command line
 */

void
subshell_sync_command_deinit (void)
{
    if (!sync_initialized)
        return;

    key_def_free (&sync_keymap);
    sync_keymap = NULL;
    sync_initialized = FALSE;
}

/* --------------------------------------------------------------------------------------------- */
/**
 *
 */

void
subshell_sync_to_command_start (const char *data_from_keyboard, size_t length)
{
    int key_code;

    if (!sync_initialized)
        return;

    key_code = key_def_search_by_sequence (sync_keymap, data_from_keyboard, length);

{
const char *dt = data_from_keyboard;
size_t dt_len = length;
mc_log("subshell_sync_to_command_start: key_code=%d\n", key_code);
mc_log ("got length=%zu:", dt_len);
while (dt_len-- !=0)
{
    mc_log (" [%02x] '%c'", (unsigned char) *dt, (*dt >31)?*dt:'.');
    dt++;
}
mc_log("\n");
}
    if (key_code == 0)
    {
        key_pressed = TRUE;
        return;
    }

    switch (key_code)
    {
        case KEY_M_CTRL | 'd':
        case KEY_M_CTRL | 'o':
            return;
        break;
        case KEY_ENTER:
            return;
        break;
        case '\t':
            subshell_sync_handle_autocompletion(key_code, data_from_keyboard, length);
            return;
        break;
    }
    if (autocompletion_status != AC_NONE)
    {
        subshell_sync_handle_autocompletion(key_code, data_from_keyboard, length);
        return;
    }
    input_handle_char (cmdline, key_code);

}

/* --------------------------------------------------------------------------------------------- */
/**
 *
 */

void
subshell_sync_to_command_end (const char *data_from_subshell, size_t length)
{
    char *data_from_subshell_0;

    if (!sync_initialized)
        return;

    if (autocompletion_status != AC_NONE)
    {
        if (autocompletion_status == AC_PRESSED_FIRST && *data_from_subshell == 0x7)
        {
            autocompletion_status = AC_PRESSED_FIRST_NEED_SECOND;
            return;
        }
        autocompletion_status = AC_NONE;
        key_pressed = TRUE;
    }

    if (!key_pressed)
        return;


    key_pressed = FALSE;

    data_from_subshell_0 = g_strndup (data_from_subshell, length);
    input_insert (cmdline, data_from_subshell_0, FALSE);
    g_free (data_from_subshell_0);
}

/* --------------------------------------------------------------------------------------------- */


#endif /* HAVE_SUBSHELL_SUPPORT */
