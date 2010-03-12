/*
   Function definitions for cdrecord support in Midnight Commander

   Copyright (C) 2001,2009,2010 The Free Software Foundation, Inc.

   Written by:
   Bart Friederichs <bart@friesoft.nl>, 2001
   Andy Shevchenko <andy.shevchenko@gmail.com>, 2005-2008
   Sergey E. Ovsyannikov <ose@ntmk.ru>, 2009, 2010
   Slava Zanko <slavazanko@gmail.com>, 2010

   This file is part of the Midnight Commander.

   The Midnight Commander is free software; you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Midnight Commander is distributed in the hope that it will be
   useful, but WITHOUT ANY WARRANTY; without even the implied warranty
   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
   MA 02110-1301, USA.
 */

/*
 * TODO for future versions
 * - Beautify the burn dialog so that it looks like all dialogs in mc.
 * - Build in multi-burner/cdrom support
 * - Check for enough drive space for image in $HOME_DIR
 */

#include <config.h>

#include <string.h>
#include <stdio.h>
/* Needed for the extern declarations of integer parameters */
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <ctype.h>
#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif

#include "lib/global.h"
#include "lib/tty/tty.h"
#include "lib/tty/win.h"
#include "lib/skin.h"

#include "layout.h"
#include "widget.h"
#include "setup.h"              /* For save_setup() */
#include "dialog.h"             /* For do_refresh() */
#include "main.h"

#include "dir.h"
#include "file.h"
#include "layout.h"             /* For nice_rotating_dash */
#include "option.h"

#include "cdburn.h"
#include "charsets.h"
#include "wtools.h"
#include "main.h"
#include "execute.h"

/*** global variables ****************************************************************************/

int interimage = 0;
int dummyrun = 0;
int joliet = 1;
int rockridge = 1;
int multi = 1;
int fast = 1;
int speed = 12;
int scsi_bus = -1, scsi_id = -1, scsi_lun = -1;
int device = -1;
char *cdwriter = NULL;

int start = -1, finish = -1;

/*** file scope macro definitions ****************************************************************/

#define TOGGLE_VARIABLE 0
#define INPUT_VARIABLE 1

/* Burner options box coords */
#define BX	4
#define BY	2

/* Filesystem option box coords */
#define FY	2

/* Blank option box coords */
#define LY	4

/* widget types */
#define CHECKBOX	1
#define INPUT		2

/* option category */
#define BURNER		1
#define FS		2
#define BLANK		3

#define BURN_DLG_WIDTH	40
#define BLANK_DLG_WIDTH	40

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/


/* watch it! $HOME_DIR will be prepended to this path */
static const char *configfile = "/.mc/mcburn.conf";


static int burner_option_width = 0, fs_option_width = 0, blank_option_width = 0;
static int FX = 0;
static char *burn_options_title, *burner_title, *fs_title, *blank_title;
/*static char *burn_title; */
static Dlg_head *burn_conf_dlg;
static Dlg_head *burn_dlg;
static Dlg_head *burn_img_dlg;
static Dlg_head *blank_dlg;
static char *burndir;
static char *cdrecord_path;
static char *mkisofs_path;
/* amount of burner and fs options */
static int burner_options, fs_options, blank_options;

/* one struct with all burner settings */
/* *INDENT-OFF* */
static struct
{
    const char *text;
    int *variable;
    int type;                   /* CHECKBOX, INPUT */
    int category;               /* BURNER, FS */
    WCheck *w_check;
    WInput *w_input;
    const char *tk;
    const char *description;

    /* only applicable for the input widget; shoot me for the overhead */
    int i_length;
} options[] =
{
    {
        N_("make &Intermediate image"),
        &interimage, CHECKBOX, BURNER, NULL, NULL,
        "interimage", N_("Make intermediate image"), 0
    },
    {
        N_("&Dummy run"),
        &dummyrun, CHECKBOX, BURNER, NULL, NULL,
        "dummyrun", N_("Turn the laser off"), 0
    },
    {
        N_("&Multisession CD"),
        &multi, CHECKBOX, BURNER, NULL, NULL,
        "multi", N_("Create a multi-session CD"), 0
    },
    {
        N_("Device"),
        &device, INPUT, BURNER, NULL, NULL,
        "device", N_("Device"), 12
    },
    {
        N_("Speed"),
        &speed, INPUT, BURNER, NULL, NULL,
        "speed", N_("Speed"), 3
    },
    {
        N_("&Joliet extensions"),
        &joliet, CHECKBOX, FS, NULL, NULL,
        "joliet", N_("Use Joliet extensions"), 0
    },
    {
        N_("&RockRidge extensions"),
        &rockridge, CHECKBOX, FS, NULL, NULL,
        "rockridge", N_("Use RockRidge extensions"), 0
    },
    {
        N_("&Fast blank"),
        &fast, CHECKBOX, BLANK, NULL, NULL,
        "fast", N_("Minimally blank the disk"), 0
    },
    {0, 0, 0, 0}
};
/* *INDENT-ON* */

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void
init_blank (void)
{
    int i;
    int line = 3;
    char buffer[1024];

    int dialog_height = 0, dialog_width = BLANK_DLG_WIDTH;

    /* button titles */
    char *burn_button = _("&Blank");
    char *cancel_button = _("&Cancel");

    /* we need the height and width of the dialog
       that depends on the settings */
    for (i = 0; options[i].tk; i++)
        switch (options[i].category)
        {
        case BLANK:
            if (*options[i].variable == 1)
            {
                if (strlen (_(options[i].description)) + 12 > BLANK_DLG_WIDTH)
                    dialog_width = strlen (_(options[i].description)) + 12;
                /* just print the setting when it is checked */
                dialog_height++;
            }
            break;
        case BURNER:
            if (options[i].type == INPUT)
                /* these are always printed */
                dialog_height++;
            break;
        default:
            break;
        }

    dialog_height += 7;

    blank_dlg = create_dlg (0, 0, dialog_height, dialog_width + 5, dialog_colors, NULL,
                            "CD Blank", _("Blank the CD"), DLG_CENTER);

    add_widget (blank_dlg, label_new (3, 3, _("Settings:")));

    /* run through all options */
    for (i = 0; options[i].tk; i++)
        switch (options[i].category)
        {
        case BLANK:
            if (*options[i].variable == 1)
                add_widget (blank_dlg, label_new (line++, 14, _(options[i].description)));
            break;
        case BURNER:
            if (options[i].type == INPUT)
            {
                add_widget (blank_dlg, label_new (line, 14, _(options[i].description)));
                if (!strcmp (options[i].tk, "device"))
                    sprintf (buffer, "%02d,%02d,%02d", scsi_bus, scsi_id, scsi_lun);
                else
                    sprintf (buffer, "%d", *options[i].variable);
                add_widget (blank_dlg,
                            label_new (line++, strlen (_(options[i].description)) + 15, buffer));
            }
            break;
        default:
            break;
        }

    add_widget (blank_dlg,
                button_new (dialog_height - 3, dialog_width / 2 - strlen (burn_button) - 3, B_ENTER,
                            DEFPUSH_BUTTON, burn_button, 0));

    add_widget (blank_dlg, button_new (dialog_height - 3, dialog_width / 2 + 5,
                                       B_CANCEL, NORMAL_BUTTON, cancel_button, 0));
}

/* --------------------------------------------------------------------------------------------- */

/* check for program and return a pointer to a string containing the full path */
static char *
check_for (const char *program)
{
    char *command;
    FILE *output;

    char buffer[1024];
    char *fullpath;

    command = malloc ((strlen (program) + 11) * sizeof (char));
    strcpy (command, "which ");
    strcat (command, program);
    strcat (command, " 2>&1");

    if (!(output = popen (command, "r")))
    {
        message (1, MSG_ERROR, _("An error occurred checking for program (popen failed)"));
        return NULL;
    }

    while (!feof (output))
        fgets (buffer, 1024, output);

    /* remove newline from buffer */
    buffer[strlen (buffer) - 1] = '\0';

    /* not starting with '/' means it is not found */
    if (buffer[0] != '/')
        return NULL;

    fullpath = malloc ((strlen (buffer) + 1) * sizeof (char));
    strncpy (fullpath, buffer, strlen (buffer) + 1);

    pclose (output);
    return fullpath;
}

/* --------------------------------------------------------------------------------------------- */

/* scan for CD recorder
   This functions executes 'cdrecord -scanbus' and checks the output for 'CD-ROM'. It gets the first occurrence.
   In the future, it should give all available CD-ROMs so that the user can choose from them
   Also, the bus, id and lun received from the line can be wrong.

   return 1 if a recorder is found, 0 otherwise
 */
static int
scan_for_recorder (char *cdrecord_command)
{
    char *command;
    char buffer[1024];
    FILE *output;

    if ((scsi_bus == -1) || (scsi_id == -1) || (scsi_lun == -1))
    {
        command = calloc ((strlen (cdrecord_command) + 15), sizeof (char));
        strncpy (command, cdrecord_command, strlen (cdrecord_command));
        strcat (command, " -scanbus 2>&1");

        if (!(output = popen (command, "r")))
        {
            message (1, MSG_ERROR, _("An error occurred scanning for writers (popen failed)"));
            return 0;
        }

        while (!feof (output))
        {
            size_t i;

            fgets (buffer, 1024, output);

            /* remove newline from buffer */
            buffer[strlen (buffer) - 1] = '\0';

            for (i = 0; i < strlen (buffer); i++)
                if (buffer[i] == '\'')
                    break;

            /* parse all lines from 'cdrecord -scanbus'
               and select the first CD-ROM */
            if (buffer[0] == '\t' && strstr (buffer, "CD-ROM"))
            {
                /* this is a scsi cdrom player in this line */
                scsi_bus = buffer[1] - 48;
                scsi_id = buffer[3] - 48;
                scsi_lun = buffer[5] - 48;

                /* free the memory first, before allocating new */
                if (cdwriter)
                    free (cdwriter);
                cdwriter = calloc (26, sizeof (char));
                strncpy (cdwriter, &buffer[i + 1], 8);
                strcat (cdwriter, " ");
                strncat (cdwriter, &buffer[i + 12], 16);

                /* remove this to select _last_ occurence,
                   i should make a menu where the user can choose */
                break;
            }
        }
        free (command);
        pclose (output);
    }

    /* if the scsi_* vars are still -1, no recorder was found */
    if ((scsi_bus == -1) || (scsi_id == -1) || (scsi_lun == -1))
        return 0;
    else
        return 1;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* this lets the user choose a dir and burn that dir, with the
   current options */
void
do_burn (void)
{
    char buffer[1024];

    /* make sure cdrecord and mkisofs is available, and get their full path */
    mkisofs_path = check_for ("mkisofs");
    if (mkisofs_path == NULL)
    {
        message (1, MSG_ERROR, _("Couldn't find mkisofs"));
        return;
    }
    cdrecord_path = check_for ("cdrecord");
    if (cdrecord_path == NULL)
    {
        message (1, MSG_ERROR, _("Couldn't find cdrecord"));
        return;
    }

    if (!strcmp (current_panel->dir.list[current_panel->selected].fname, ".."))
    {
        message (1, MSG_ERROR, _("You can't burn the parent-directory"));
        return;
    }

    burndir =
        concat_dir_and_file (current_panel->cwd,
                             current_panel->dir.list[current_panel->selected].fname);

    if (!S_ISDIR (current_panel->dir.list[current_panel->selected].st.st_mode))
    {
        /*
           message(1, MSG_ERROR, _("You can't burn a single file to CD"));
           return;
         */
        int ret_value;

        if (!scan_for_recorder (cdrecord_path))
        {
            sprintf (buffer, _("No CD-Writer found"));
            message (1, MSG_ERROR, buffer);
            return;
        }

        init_burn_img ();
        run_dlg (burn_img_dlg);
        ret_value = burn_img_dlg->ret_value;
        destroy_dlg (burn_img_dlg);

        if (ret_value == B_ENTER)
        {
            /* here, the actual burning takes place
             * construct a (series of) command(s) to execute
             */
            sprintf (buffer, "echo \"Burning %s to CD ...\"", burndir);
            shell_execute (buffer, EXECUTE_INTERNAL);

            sprintf (buffer, "echo \"Burning CD...\"");
            shell_execute (buffer, EXECUTE_INTERNAL);
            strcpy (buffer, cdrecord_path);
            if (dummyrun)
                strcat (buffer, " -dummy");
            sprintf (buffer, "%s -v speed=%d", buffer, speed);
            sprintf (buffer, "%s dev=%d,%d,%d", buffer, scsi_bus, scsi_id, scsi_lun);
            sprintf (buffer, "%s -eject -overburn driveropts=burnproof", buffer);
            sprintf (buffer, "%s -data %s", buffer,
                     current_panel->dir.list[current_panel->selected].fname);

            /* execute the burn command */
            shell_execute (buffer, EXECUTE_INTERNAL);
        }
    }
    else
    {
        int ret_value;

        if (!scan_for_recorder (cdrecord_path))
        {
            sprintf (buffer, _("No CD-Writer found"));
            message (1, MSG_ERROR, buffer);
            return;
        }

        init_burn ();
        run_dlg (burn_dlg);
        ret_value = burn_dlg->ret_value;
        destroy_dlg (burn_dlg);

        if (ret_value == B_ENTER)
        {
            /* here, the actual burning takes place
             * construct a (series of) command(s) to execute
             */
            char cpname[1024] = "";
            char cont_str[1024] = "";

            int i =
                source_codepage > 0 ? source_codepage : display_codepage > 0 ? display_codepage : 0;

            sprintf (buffer, "echo \"Burning %s to CD ...\"", burndir);
            shell_execute (buffer, EXECUTE_INTERNAL);

            sprintf (cpname, "%s", i ? codepages[i].id : "");
            for (i = 0; cpname[i]; i++)
                cpname[i] = tolower (cpname[i]);

            if ((start >= 0) && (finish >= 0))
                sprintf (cont_str, " -C %d,%d -dev %d,%d,%d", start, finish, scsi_bus, scsi_id,
                         scsi_lun);

                /** continue here
                 ** 1. make a mkisofs command
                 ** 2. make a cdrecord command
                 ** 3. pipe them if necessary (!interimage)
                 ** 4. run consecutively if necessary (interimage)
                 ** NOTE: dummyrun is NOT before writing, its just a dummy run (nice for testing purposes)
                 **/

            /* STEP 1: create an image if the user wants to. Put the image in $HOMEDIR
             * it's the user's responsibility to make sure the is enough room (TODO 3)
             * this is where the fs-options come in, using -r for RockRidge and -J for Joliet extensions
             */
            if (interimage)
            {
                sprintf (buffer, "echo \"Building image...\"");
                shell_execute (buffer, EXECUTE_INTERNAL);
                strcpy (buffer, mkisofs_path);
                if (rockridge)
                    strcat (buffer, " -r");
                if (cpname[0])
                {
                    strcat (buffer, " -input-charset ");
                    strcat (buffer, cpname);
                }
                if (joliet)
                    strcat (buffer, " -J -joliet-long");
                if (strlen (cont_str))
                    strcat (buffer, cont_str);
                strcat (buffer, " -o ");
                strcat (buffer, home_dir);
                strcat (buffer, "/mcburn.iso \"");
                strcat (buffer, burndir);
                strcat (buffer, "\"");
                shell_execute (buffer, EXECUTE_INTERNAL);
                /*
                   }
                 */
                /* STEP 2: create a cdrecord command, this is where speed, dummy, multi and the scsi_* vars come in
                 * also, check for an image or pipe it right into cdrecord
                 *
                 * STEP 2b: cdrecord without a pipe (assume the $HOME/mcburn.iso exists (TODO 4))
                 */
                /*
                   if (interimage) {
                 */
                sprintf (buffer, "echo \"Burning CD...\"");
                shell_execute (buffer, EXECUTE_INTERNAL);
                strcpy (buffer, cdrecord_path);
                if (dummyrun)
                    strcat (buffer, " -dummy");
                if (multi)
                    strcat (buffer, " -multi");
                sprintf (buffer, "%s -v speed=%d", buffer, speed);
                sprintf (buffer, "%s dev=%d,%d,%d", buffer, scsi_bus, scsi_id, scsi_lun);
                sprintf (buffer, "%s -eject -overburn driveropts=burnproof", buffer);
                sprintf (buffer, "%s -data %s/mcburn.iso", buffer, home_dir);

                /* execute the burn command */
                shell_execute (buffer, EXECUTE_INTERNAL);

                sprintf (buffer, "echo \"Erase image...\"");
                shell_execute (buffer, EXECUTE_INTERNAL);
                strcpy (buffer, "/bin/rm ");
                strcat (buffer, home_dir);
                strcat (buffer, "/mcburn.iso");
                shell_execute (buffer, EXECUTE_INTERNAL);

            }
            else
            {
                /* no image present, pipe mkisofs into cdrecord */
                /* first get the size of the image to build */
                FILE *mkisofs_pipe;
                int imagesize;

                sprintf (buffer, "%s %s %s -q -print-size \"%s\"/  2>&1 | sed -e \"s/.* = //\"",
                         mkisofs_path, rockridge ? "-r" : "", joliet ? "-J -joliet-long" : "",
                         burndir);
                mkisofs_pipe = popen (buffer, "r");
                /*FIXME: need to respect return value... */
                fgets (buffer, 1024, mkisofs_pipe);
                imagesize = atoi (buffer);
                pclose (mkisofs_pipe);

                sprintf (buffer,
                         "[ \"0%d\" -ne 0 ] && %s %s %s %s %s%s \"%s\" | %s %s %s speed=%d dev=%d,%d,%d -eject -overburn driveropts=burnproof -data -",
                         imagesize, mkisofs_path, rockridge ? "-r" : "",
                         cpname[0] ? "-input-charset" : "", cpname[0] ? cpname : "",
                         joliet ? "-J -joliet-long" : "", strlen (cont_str) ? cont_str : "",
                         burndir, cdrecord_path, dummyrun ? "-dummy" : "", multi ? "-multi" : "",
                         speed, scsi_bus, scsi_id, scsi_lun);
                /* execute the burn command */
                shell_execute (buffer, EXECUTE_INTERNAL);
            }
        }
    }
}

/* --------------------------------------------------------------------------------------------- */

void
do_session (void)
{
    char buffer[1024];
    FILE *cdrecord_pipe;

    if (!(cdrecord_path = check_for ("cdrecord")))
    {
        message (1, MSG_ERROR, _("Couldn't find cdrecord"));
        return;
    }

    if (!scan_for_recorder (cdrecord_path))
    {
        sprintf (buffer, _("No CD-Writer found"));
        message (1, MSG_ERROR, buffer);
        return;
    }

    sprintf (buffer, "%s -msinfo dev=%d,%d,%d 2>&1 | head -1", cdrecord_path, scsi_bus, scsi_id,
             scsi_lun);
    cdrecord_pipe = popen (buffer, "r");
    fgets (buffer, 1024, cdrecord_pipe);
    pclose (cdrecord_pipe);
    sscanf (buffer, "%i,%i", &start, &finish);

    if ((start == -1) && (finish == -1))
    {
        message (1, MSG_ERROR, _("Couldn't continue session"));
        return;
    }

    do_burn ();
    start = -1, finish = -1;
}

/* --------------------------------------------------------------------------------------------- */

void
do_blank (void)
{
    char buffer[1024];

    if (!(cdrecord_path = check_for ("cdrecord")))
    {
        message (1, MSG_ERROR, _("Couldn't find cdrecord"));
        return;
    }

    if (!scan_for_recorder (cdrecord_path))
    {
        sprintf (buffer, _("No CD-Writer found"));
        message (1, MSG_ERROR, buffer);
        return;
    }

    init_blank ();
    run_dlg (blank_dlg);

    if (blank_dlg->ret_value == B_ENTER)
    {
        /* here, the actual burning takes place
         * construct a (series of) command(s) to execute
         */

        strcpy (buffer, "echo \"Erase CD ...\"");
        shell_execute (buffer, EXECUTE_INTERNAL);

        sprintf (buffer, "%s speed=%d dev=%d,%d,%d -eject -force blank=%s",
                 cdrecord_path, speed, scsi_bus, scsi_id, scsi_lun, fast ? "fast" : "all");

        /* execute the burn command */
        shell_execute (buffer, EXECUTE_INTERNAL);
    }
    destroy_dlg (blank_dlg);
}

/* --------------------------------------------------------------------------------------------- */

/* this initializes the burn dialog box
   there will be no way back after OK'ing this one */
void
init_burn (void)
{
    int i;
    int line = 5;
    char buffer[1024];

    size_t dialog_height = 0, dialog_width = BURN_DLG_WIDTH;

    /* button titles */
    char *burn_button = _("&Burn");
    char *cancel_button = _("&Cancel");

    /* we need the height and width of the dialog
       that depends on the settings */
    for (i = 0; options[i].tk; i++)
        switch (options[i].type)
        {
        case CHECKBOX:
            if ((*options[i].variable == 1) && (options[i].category != BLANK))
            {
                if (strlen (_(options[i].description)) + 12 > BURN_DLG_WIDTH)
                    dialog_width = strlen (_(options[i].description)) + 12;
                /* just print the setting when it is checked */
                dialog_height++;
            }
            break;
        case INPUT:
            /* these are always printed */
            dialog_height++;
            break;
        default:
            break;
        }

    dialog_height += 9;

    if (strlen (burndir) + 12 > dialog_width)
        dialog_width = strlen (burndir) + 12;

    if ((start == -1) && (finish == -1))
        strcpy (buffer, _("Burn directory to CD"));
    else
        strcpy (buffer, _("Burn next session"));

    burn_dlg =
        create_dlg (0, 0, dialog_height, dialog_width + 5, dialog_colors, NULL, "CD Burn", buffer,
                    DLG_CENTER);

    add_widget (burn_dlg, label_new (3, 3, _("Directory:")));
    add_widget (burn_dlg, label_new (3, 14, burndir));
    add_widget (burn_dlg, label_new (5, 3, _("Settings:")));

    /* run through all options */
    for (i = 0; options[i].tk; i++)
        switch (options[i].type)
        {
        case CHECKBOX:
            if ((*options[i].variable == 1) && (options[i].category != BLANK))
                add_widget (burn_dlg, label_new (line++, 14, _(options[i].description)));
            break;
        case INPUT:
            add_widget (burn_dlg, label_new (line, 14, _(options[i].description)));
            if (!strcmp (options[i].tk, "device"))
                sprintf (buffer, "%02d,%02d,%02d", scsi_bus, scsi_id, scsi_lun);
            else
                sprintf (buffer, "%d", *options[i].variable);
            add_widget (burn_dlg,
                        label_new (line++, strlen (_(options[i].description)) + 15, buffer));
            break;
        default:
            break;
        }

    add_widget (burn_dlg,
                button_new (dialog_height - 3, dialog_width / 2 - strlen (burn_button) - 4, B_ENTER,
                            DEFPUSH_BUTTON, burn_button, 0));

    add_widget (burn_dlg, button_new (dialog_height - 3, dialog_width / 2 + 4,
                                      B_CANCEL, NORMAL_BUTTON, cancel_button, 0));
}

/* --------------------------------------------------------------------------------------------- */

void
init_burn_img (void)
{
    int i;
    int line = 5;
    char buffer[1024];

    size_t dialog_height = 0, dialog_width = BURN_DLG_WIDTH;

    /* button titles */
    char *burn_button = _("&Burn");
    char *cancel_button = _("&Cancel");

    /* we need the height and width of the dialog
       that depends on the settings */
    for (i = 0; options[i].tk; i++)
        switch (options[i].type)
        {
        case CHECKBOX:
            if ((*options[i].variable == 1) && (options[i].category != BLANK)
                && (options[i].category != FS) && !strcmp (options[i].tk, "dummyrun"))
            {
                if (strlen (_(options[i].description)) + 12 > BURN_DLG_WIDTH)
                    dialog_width = strlen (_(options[i].description)) + 12;
                /* just print the setting when it is checked */
                dialog_height++;
            }
            break;
        case INPUT:
            /* these are always printed */
            dialog_height++;
            break;
        default:
            break;
        }

    dialog_height += 9;

    if (strlen (burndir) + 12 > dialog_width)
        dialog_width = strlen (burndir) + 12;

    strcpy (buffer, _("Burn image to CD"));

    burn_img_dlg = create_dlg (0, 0, dialog_height, dialog_width + 5, dialog_colors, NULL,
                               "CD Burn", buffer, DLG_CENTER);

    add_widget (burn_img_dlg, label_new (3, 3, _("Image:")));
    add_widget (burn_img_dlg, label_new (3, 14, burndir));
    add_widget (burn_img_dlg, label_new (5, 3, _("Settings:")));

    /* run through all options */
    for (i = 0; options[i].tk; i++)
        switch (options[i].type)
        {
        case CHECKBOX:
            if ((*options[i].variable == 1) && (options[i].category != BLANK)
                && (options[i].category != FS) && !strcmp (options[i].tk, "dummyrun"))
                add_widget (burn_img_dlg, label_new (line++, 14, _(options[i].description)));
            break;
        case INPUT:
            add_widget (burn_img_dlg, label_new (line, 14, _(options[i].description)));
            if (!strcmp (options[i].tk, "device"))
                sprintf (buffer, "%02d,%02d,%02d", scsi_bus, scsi_id, scsi_lun);
            else
                sprintf (buffer, "%d", *options[i].variable);
            add_widget (burn_img_dlg,
                        label_new (line++, strlen (_(options[i].description)) + 15, buffer));
            break;
        default:
            break;
        }

    add_widget (burn_img_dlg,
                button_new (dialog_height - 3, dialog_width / 2 - strlen (burn_button) - 4, B_ENTER,
                            DEFPUSH_BUTTON, burn_button, 0));

    add_widget (burn_img_dlg, button_new (dialog_height - 3, dialog_width / 2 + 4,
                                          B_CANCEL, NORMAL_BUTTON, cancel_button, 0));
}

/* --------------------------------------------------------------------------------------------- */

/***************************************************************************************************
  MC-Burn options functions
***************************************************************************************************/

/* this shows the burn options dialog */
void
burn_config (void)
{
    int result, i;

    init_burn_config ();

    run_dlg (burn_conf_dlg);

    /* they pushed the OK or Save button, set the variables right */
    result = burn_conf_dlg->ret_value;

    if (result == B_ENTER || result == B_EXIT)
        for (i = 0; options[i].tk; i++)
            switch (options[i].type)
            {
            case CHECKBOX:
                if (options[i].w_check->state & C_CHANGE)
                    *options[i].variable = !(*options[i].variable);
                break;
            case INPUT:
                if (!strcmp (options[i].tk, "device"))
                {
                    scsi_bus = scsi_id = scsi_lun = -1;
                    sscanf (options[i].w_input->buffer, "%d,%d,%d", &scsi_bus, &scsi_id, &scsi_lun);
                    if ((scsi_bus == -1) || (scsi_id == -1) || (scsi_lun == -1))
                        scsi_bus = scsi_id = scsi_lun = -1;
                    *options[i].variable = scsi_bus * 256 + scsi_id * 16 + scsi_lun;
                }
                else
                    *options[i].variable = atoi (options[i].w_input->buffer);
                break;
            }

    /* If they pressed the save button, save the values to ~/.mc/mcburn.conf */
    if (result == B_EXIT)
    {
        save_mcburn_settings ();
    }

    destroy_dlg (burn_conf_dlg);
}

/* --------------------------------------------------------------------------------------------- */

/* this initializes the burn options dialog box */
void
init_burn_config (void)
{
    int i = 0;
    static int dialog_height = 0, dialog_width = 0;
    static int b1, b2, b3;

    /* button title */
    char *ok_button = _("&Ok");
    char *cancel_button = _("&Cancel");
    char *save_button = _("&Save");
    register int l1;

    burner_options = 0;
    fs_options = 0;
    blank_options = 0;

    /* count the amount of burner and fs options */
    for (i = 0; options[i].tk; i++)
        switch (options[i].category)
        {
        case BURNER:
            burner_options++;
            break;
        case FS:
            fs_options++;
            break;
        case BLANK:
            blank_options++;
            break;
        }

    /* similar code is in options.c */
    burn_options_title = _(" CD-Burn options ");
    burner_title = _(" Burner options ");
    fs_title = _(" Filesystem options ");
    blank_title = _(" Blank options ");

    /* get the widths for the burner options and the fs options */
    burner_option_width = strlen (burner_title) + 1;
    fs_option_width = strlen (fs_title) + 1;
    blank_option_width = strlen (blank_title) + 1;
    for (i = 0; options[i].tk; i++)
    {
        /* make sure the whole inputfield width is accounted for */
        if (options[i].type == INPUT)
            l1 = options[i].i_length;
        else
            l1 = 0;

        /* calculate longest width of text */
        if (options[i].category == BURNER)
        {
            options[i].text = _(options[i].text);
            l1 += strlen (options[i].text) + 7;
            if (l1 > burner_option_width)
                burner_option_width = l1;
        }

        if ((options[i].category == FS) || (options[i].category == BLANK))
        {
            options[i].text = _(options[i].text);
            l1 += strlen (options[i].text) + 7;
            if (l1 > fs_option_width)
                fs_option_width = l1;
        }

    }

    l1 = 11 + strlen (ok_button) + strlen (save_button) + strlen (cancel_button);

    i = (burner_option_width + fs_option_width - l1) / 4;
    b1 = 5 + i;
    b2 = b1 + strlen (ok_button) + i + 6;
    b3 = b2 + strlen (save_button) + i + 4;

    dialog_width = burner_option_width + fs_option_width + 7;
    FX = FY + burner_option_width + 2;

    /* figure out the height for the burn options dialog */
    if (burner_options > (fs_options + blank_options + 2))
        dialog_height = burner_options + 7;
    else
        dialog_height = fs_options + blank_options + 2 + 7;

    burn_conf_dlg = create_dlg (0, 0, dialog_height, dialog_width, dialog_colors, NULL,
                                "CD Burn options", _(burn_options_title), DLG_CENTER);

    /* add the burner options */
    for (i = 0; options[i].tk; i++)
    {
        int x = 0, y = 0;
        char *defvalue;

        /* first find out where the widget should go (burner option or fs option) */
        switch (options[i].category)
        {
        case BURNER:
            x = BX + 1;
            y = BY + 1 + i;
            break;
        case FS:
            x = FX + 1;
            y = FY - burner_options + 1 + i;
            break;
        case BLANK:
            x = FX + 1;
            y = LY - burner_options + 1 + i;
            break;
        default:
            break;
        }

        /* then create a new widget, depending on the type
         * afterwards, add it to the dialog box
         */
        switch (options[i].type)
        {
        case CHECKBOX:
            /* here, the values are taken from the variables and put in the checkboxes */
            options[i].w_check = check_new (y, x, *options[i].variable, options[i].text);
            add_widget (burn_conf_dlg, options[i].w_check);
            break;
        case INPUT:
            defvalue = malloc ((options[i].i_length + 1) * sizeof (char));
            if (!strcmp (options[i].tk, "device"))
            {
                scsi_bus = *options[i].variable / 256;
                scsi_id = (*options[i].variable - 256 * scsi_bus) / 16;
                scsi_lun = (*options[i].variable - 256 * scsi_bus - 16 * scsi_id);
                if ((scsi_bus == -1) || (scsi_id == -1) || (scsi_lun == -1))
                    scsi_bus = scsi_id = scsi_lun = -1;
                snprintf (defvalue, options[i].i_length, "%02d,%02d,%02d", scsi_bus, scsi_id,
                          scsi_lun);
            }
            else
                snprintf (defvalue, options[i].i_length, "%d", *options[i].variable);

            /* a label */
            add_widget (burn_conf_dlg, label_new (y, x, options[i].text));
            /* and the input widget behind it */
            options[i].w_input = input_new (y, x + strlen (options[i].text) + 1,
                                            INPUT_COLOR, options[i].i_length, defvalue,
                                            options[i].tk, INPUT_COMPLETE_DEFAULT);
            add_widget (burn_conf_dlg, options[i].w_input);
            break;
        default:
            break;
        }
    }

    add_widget (burn_conf_dlg, groupbox_new (2, 3, 5 + 2, dialog_width / 2 - 2, burner_title));

    add_widget (burn_conf_dlg,
                groupbox_new (2, dialog_width / 2 + 1, 2 + 2, dialog_width / 2 - 4, fs_title));

    add_widget (burn_conf_dlg,
                groupbox_new (4 + 2, dialog_width / 2 + 1, 1 + 2, dialog_width / 2 - 4,
                              blank_title));

    /* add the OK, Cancel and Save buttons */
    add_widget (burn_conf_dlg,
                button_new (dialog_height - 3, b1, B_ENTER, DEFPUSH_BUTTON, ok_button, 0));

    add_widget (burn_conf_dlg,
                button_new (dialog_height - 3, b2, B_EXIT, NORMAL_BUTTON, save_button, 0));

    add_widget (burn_conf_dlg,
                button_new (dialog_height - 3, b3, B_CANCEL, NORMAL_BUTTON, cancel_button, 0));

}

/* --------------------------------------------------------------------------------------------- */

/***************************************************************************************************
  MC-Burn configfile functions
***************************************************************************************************/

/* this loads the settings from ~/.mc/mcburn.conf */
void
load_mcburn_settings (void)
{
    FILE *mcburn_settings_file;
    char *filename;
    int i, setting;
    char buff[128];

    filename = malloc (strlen (home_dir) + strlen (configfile) + 1);
    strcpy (filename, home_dir);
    strcat (filename, configfile);

    printf ("Loading CD Burn extension settings...");

    if ((mcburn_settings_file = fopen (filename, "r")) == NULL)
    {
        printf ("failed\n");
    }
    else
    {
        while (!feof (mcburn_settings_file))
        {
            fscanf (mcburn_settings_file, "%s %d", buff, &setting);

            for (i = 0; options[i].tk; i++)
            {
                if (!strcmp (options[i].tk, buff))
                {
                    *options[i].variable = setting;
                    if (!strcmp (options[i].tk, "device"))
                    {
                        scsi_bus = *options[i].variable / 256;
                        scsi_id = (*options[i].variable - 256 * scsi_bus) / 16;
                        scsi_lun = (*options[i].variable - 256 * scsi_bus - 16 * scsi_id);
                    }
                }
            }
        }
        fclose (mcburn_settings_file);
        printf ("done\n");
    }
}

/* --------------------------------------------------------------------------------------------- */

/* this saves the settings to ~/.mc/mcburn.conf */
void
save_mcburn_settings (void)
{
    FILE *mcburn_settings_file;
    int i;
    char *filename;

    filename = malloc (strlen (home_dir) + strlen (configfile) + 1);
    strcpy (filename, home_dir);
    strcat (filename, configfile);

    if ((mcburn_settings_file = fopen (filename, "w")) == NULL)
    {
        message (0, _(" Save failed "), filename);
    }
    else
    {
        /* first, print a message not to tamper with the file */
        fprintf (mcburn_settings_file, "# This file is generated by MC-Burn. DO NOT EDIT!\n");
        for (i = 0; options[i].tk; i++)
        {
            if (!strcmp (options[i].tk, "device"))
                fprintf (mcburn_settings_file, "%s %d\n", options[i].tk,
                         scsi_bus * 256 + scsi_id * 16 + scsi_lun);
            else
                fprintf (mcburn_settings_file, "%s %d\n", options[i].tk, *options[i].variable);
        }
        fclose (mcburn_settings_file);
    }
}

/* --------------------------------------------------------------------------------------------- */
