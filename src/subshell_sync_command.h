/** \file subshell_sync_command.h
 *  \brief Header: Synchronization between command line widget and command line subshell.
 */

#ifndef MC__SUBSHELL_SYNC_COMMAND_H
#define MC__SUBSHELL_SYNC_COMMAND_H

#ifdef HAVE_SUBSHELL_SUPPORT

#include "subshell.h"

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

void subshell_sync_command_init (subshell_type_t subshell_type);
void subshell_sync_command_deinit (void);

void subshell_sync_to_command_start (const char *data_from_keyboard, size_t length);
void subshell_sync_to_command_end (const char *data_from_subshell, size_t length);

/*** inline functions ****************************************************************************/

#endif /* HAVE_SUBSHELL_SUPPORT */

#endif /* MC__SUBSHELL_SYNC_COMMAND_H */
