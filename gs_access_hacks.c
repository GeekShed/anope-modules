/***************************************************************************************/
/* Anope Module : gs_access_hacks.c : v1.x                                             */
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

int doCont(User *u);
int doNsSendPass(User *u);
int doCsSendPass(User *u);

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

	/* HS LIST */
	c = createCommand("LIST", doCont, is_host_setter, -1, -1, -1, -1, -1);
	status = moduleAddCommand(HOSTSERV, c, MOD_HEAD);

	/* OS DEFCON */
	c = createCommand("DEFCON", doCont, is_services_root, -1, -1, -1, -1, -1);
	moduleAddCommand(OPERSERV, c, MOD_HEAD);

	/* NS SENDPASS */
	c = createCommand("SENDPASS", doNsSendPass, NULL, -1, -1, -1, -1, -1);
	moduleAddCommand(NICKSERV, c, MOD_HEAD);

	/* CS SENDPASS */
	c = createCommand("SENDPASS", doCsSendPass, NULL, -1, -1, -1, -1, -1);
	moduleAddCommand(CHANSERV, c, MOD_HEAD);

	return MOD_CONT;
}

/**
* Unload the module
**/
void AnopeFini(void) {}

/*************************************************************************/

int doCont(User *u)
{
	return MOD_CONT;
}

int doNsSendPass(User *u) {
	notice_user(s_NickServ, u, "This command is disabled for security reasons. Please join #help and ask a network staff member for a password reset.");

	return MOD_CONT;
}

int doCsSendPass(User *u) {
	notice_user(s_ChanServ, u, "This command is disabled for security reasons. Please join #help for assistance.");

	return MOD_CONT;
}

/* EOF */
