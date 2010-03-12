/* cdburn.h
 * Header file for cdrecord support in Midnight Commander
 * Copyright 2001 Bart Friederichs
 */

#ifndef MC_CDBURN_H
#define MC_CDBURN_H

/*** typedefs (not structures) and defined constants *********************************************/

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

void cdburn_do_burn (void);
void cdburn_do_session (void);
void cdburn_do_blank (void);
void cdburn_config (void);
void cdburn_load_settings (void);
void cdburn_save_settings (void);

#endif                          /* MC_CDBURN_H */
