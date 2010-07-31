/***************************************************************************************/
/* Anope Module : ircd_remove_r.c : v1.x                                               */
/* Phil Lavin - phil@geekshed.net                                                      */
/*                                                                                     */
/* (c) 2010 Phil Lavin                                                                 */
/*                                                                                     */
/* This program is free software; you can redistribute it and/or modify it under the   */
/* terms of the GNU General Public License as published by the Free Software           */
/* Foundation; either version 1, or (at your option) any later version.                */
/*                                                                                     */
/*  This program is distributed in the hope that it will be useful, but WITHOUT ANY    */
/*  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A    */
/*  PARTICULAR PURPOSE.  See the GNU General Public License for more details.          */
/*                                                                                     */
/***************************************************************************************/

#include <module.h>

#define AUTHOR "Phil"
#define VERSION "1.0"

/*************************************************************************/

int do_event_join(int argc, char **argv);

/*************************************************************************/

/**
* AnopeInit is called when the module is loaded
* @param argc Argument count
* @param argv Argument list
* @return MOD_CONT to allow the module, MOD_STOP to stop it
**/
int AnopeInit(int argc, char **argv)
{
	Command *c;
	int status;

	moduleAddAuthor(AUTHOR);
	moduleAddVersion(VERSION);

	hook = createEventHook(EVENT_JOIN_CHANNEL, do_event_join);
	moduleAddEventHook(hook);

	return MOD_CONT;
}

/**
* Unload the module
**/
void AnopeFini(void) {}

/*************************************************************************/

int do_event_join(int argc, char **argv) {
	User *u = NULL;

	if (stricmp(argv[0], EVENT_START) == 0) {
		if (stricmp(argv[2], "#applobby") == 0) {
			if ((u = finduser(argv[1]))) {
				common_svsmode(u, "-R", NULL);
			}
		}
	}
}

/* EOF */
