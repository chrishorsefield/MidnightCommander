/* mcburn.h
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

void do_burn (void);
void do_session (void);
void do_blank (void);
void burn_config (void);
void init_burn_config (void);   /* initialize burner config dialog */
void init_burn (void);          /* initialize burner dialog */
void init_burn_img (void);      /* initialize burner dialog */
void load_mcburn_settings (void);
void save_mcburn_settings (void);

#endif                          /* MC_CDBURN_H */
