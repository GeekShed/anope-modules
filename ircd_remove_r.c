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
/*****************************************HEADER****************************************/

#include <module.h>

#define AUTHOR "Phil"
#define VERSION "1.0"

#define UMODE_R 0x80000000

/****************************************TYPEDEFS***************************************/

typedef struct affecteduser_ AffectedUser;

/***************************************VARIABLES***************************************/

AffectedUser *firstItem = NULL;
AffectedUser *curItem = NULL;

/****************************************STRUCTS****************************************/

struct affecteduser_ {
	AffectedUser *next, *prev;
	char *nick;
	time_t time;
};

/**********************************FORWARD DECLARATIONS*********************************/

int do_event_join(int argc, char **argv);
int do_event_part(int argc, char **argv);

void addItem(char *nick);
void delItem(AffectedUser *item);
AffectedUser *findItemByNick(char *nick);

/***************************************ANOPE REQS**************************************/

/**
* AnopeInit is called when the module is loaded
* @param argc Argument count
* @param argv Argument list
* @return MOD_CONT to allow the module, MOD_STOP to stop it
**/
int AnopeInit(int argc, char **argv)
{
	EvtHook *hook;

	moduleAddAuthor(AUTHOR);
	moduleAddVersion(VERSION);

	hook = createEventHook(EVENT_JOIN_CHANNEL, do_event_join);
	moduleAddEventHook(hook);

	hook = createEventHook(EVENT_PART_CHANNEL, do_event_part);
	moduleAddEventHook(hook);

	return MOD_CONT;
}

/**
* Unload the module
**/
void AnopeFini(void) {}

/*************************************EVENT HANDLERS************************************/

int do_event_join(int argc, char **argv) {
	User *u = NULL;

	if (stricmp(argv[0], EVENT_START) == 0) {
		if (stricmp(argv[2], "#applobby") == 0) {
			if ((u = finduser(argv[1]))) {
				if (u->mode & UMODE_R) {
					common_svsmode(u, "-R", NULL);
					addItem(u->nick);
				}
			}
		}
	}

	return MOD_CONT;
}

int do_event_part(int argc, char **argv) {
	AffectedUser *af = NULL;
	User *u = NULL;

	if (stricmp(argv[0], EVENT_START) == 0) {
		if (stricmp(argv[2], "#applobby") == 0) {
			if ((af = findItemByNick(argv[1]))) {
				if ((u = finduser(argv[1]))) {
					common_svsmode(u, "+R", NULL);
				}

				delItem(af);
			}
		}
	}

	return MOD_CONT;
}

/************************************CUSTOM FUNCTIONS***********************************/

void addItem(char *nick) {
	// There's no items in the list yet
	if (curItem == NULL) {
		curItem = (AffectedUser *)malloc(sizeof(AffectedUser));
		curItem->prev = NULL;

		firstItem = curItem;
	}
	// There's items
	else {
		curItem->next = (AffectedUser *)malloc(sizeof(AffectedUser));
		curItem->next->prev = curItem;

		curItem = curItem->next;
	}

	curItem->next = NULL;

	curItem->nick = sstrdup(nick);
	curItem->time = time(NULL);
}

void delItem(AffectedUser *item) {
	if (item != NULL) {
		// Relink list without item
		// If it's the first item (i.e. null prev)
		if (item->prev == NULL) {
			// If there's only 1 item in the list
			if (item->next == NULL) {
				firstItem = NULL;
				curItem = NULL;
			}
			else {
				item->next->prev = NULL;
				firstItem = item->next;
			}
		}
		// If it's the last item (i.e. null next)
		else if (item->next == NULL) {
			item->prev->next = NULL;
			// NOTE TO PHIL: THE FOLLOWING LINE IS MUY IMPORTANTE - Adam
			curItem = item->prev;
		}
		// If it's a middle item
		else {
			item->prev->next = item->next;
			item->next->prev = item->prev;
		}

		// Free like you've never freed before!
		free(item->nick);
		free(item);
	}
}

AffectedUser *findItemByNick(char *nick) {
	AffectedUser *item = NULL;

	for (item = firstItem; item; item = item->next) {
		if (stricmp(nick, item->nick) == 0) {
			return item;
		}
	}

	return NULL;
}

/* EOF */
