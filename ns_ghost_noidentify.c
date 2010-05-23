/***************************************************************************************/
/* Anope Module : ns_ghost_noidentify.c : v1.x                                         */
/* Phil Lavin - phil@anope.org                                                         */
/*                                                                                     */
/* (c) 2010 Phil Lavin                                                                 */
/*                                                                                     */
/* Many thanks to Brigitte for the translations                                        */
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
#include <assert.h>

#define AUTHOR "Phil"
#define VERSION "1.0"

/* Time (in seconds) after ghost/recover in which an automatic identify will happen */
#define AUTO_ID_TIMEOUT		60

/* Language Stuff */
#define LANG_NUM_STRINGS	2

#define NOID_GHOST			0
#define NOID_RECOVER		1

/****************************************TYPEDEFS***************************************/

typedef struct recentrgitem_ RecentRGItem;

/***************************************VARIABLES***************************************/

RecentRGItem *firstItem = NULL;
RecentRGItem *curItem = NULL;

/****************************************STRUCTS****************************************/

struct recentrgitem_ {
	RecentRGItem *next, *prev;
	char *source;
	char *dest;
	time_t time;
	int ghost;
};

/**********************************FORWARD DECLARATIONS*********************************/

int do_event_nick(int argc, char **argv);
int do_event_ghost(int argc, char **argv);
int do_event_recover(int argc, char **argv);
int do_event_logoff(int argc, char **argv);

void addItem(char *source, char *dest, int ghost);
void delItem(RecentRGItem *item);
int timeoutList(time_t time);
void clearList();
RecentRGItem *findItemBySource(char *source);
int forceid(User *u);

void m_AddLanguages(void);

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

	hook = createEventHook(EVENT_CHANGE_NICK, do_event_nick);
	moduleAddEventHook(hook);
	
	hook = createEventHook(EVENT_NICK_GHOSTED, do_event_ghost);
	moduleAddEventHook(hook);
	
	hook = createEventHook(EVENT_NICK_RECOVERED, do_event_recover);
	moduleAddEventHook(hook);
	
	hook = createEventHook(EVENT_USER_LOGOFF, do_event_logoff);
	moduleAddEventHook(hook);

	m_AddLanguages();

	return MOD_CONT;
}

/**
* Unload the module
**/
void AnopeFini(void) {
	// Free the memory used by the list
	clearList();
}

/*************************************EVENT HANDLERS************************************/

int do_event_nick(int argc, char **argv)
{
	RecentRGItem *item = NULL;
	User *u = NULL;
	
	// Remove timed out entries
	timeoutList(time(NULL) - AUTO_ID_TIMEOUT);
	
	// If an entry exists in the list for the user's nick before the nick change
	if ((item = findItemBySource(argv[1]))) {
		// If the user's new nick is the one they ghosted/recovered
		if (stricmp(argv[0], item->dest) == 0) {
			// If the new user exists
			if ((u = finduser(argv[0]))) {
				// Identify user
				if (forceid(u)) {
					if (item->ghost == 1) {
						moduleNoticeLang(s_NickServ, u, NOID_GHOST);
						alog("ns_ghost_noidentify: %s was automatically identified to services after using GHOST", u->nick);
					}
					else {
						moduleNoticeLang(s_NickServ, u, NOID_RECOVER);
						alog("ns_ghost_noidentify: %s was automatically identified to services after using RECOVER", u->nick);
					}
				}
			}
		}
		
		delItem(item);
	}
	
	return MOD_CONT;
}

int do_event_logoff(int argc, char **argv)
{
	RecentRGItem *item = NULL;
	
	if ((item = findItemBySource(argv[0]))) {
		delItem(item);
	}
	
	return MOD_CONT;
}

int do_event_ghost(int argc, char **argv)
{
	RecentRGItem *item = NULL;

	if (stricmp(argv[0], EVENT_STOP) == 0) {
		// If the 'ghoster' already has an entry in the list
		if ((item = findItemBySource(argv[1]))) {
			// Delete it
			delItem(item);
		}
	
		addItem(argv[1], argv[2], 1);
	}
	
	return MOD_CONT;
}

int do_event_recover(int argc, char **argv)
{
	RecentRGItem *item = NULL;

	if (stricmp(argv[0], EVENT_STOP) == 0) {
		// If the 'recoverer' already has an entry in the list
		if ((item = findItemBySource(argv[1]))) {
			// Delete it
			delItem(item);
		}
	
		addItem(argv[1], argv[2], 0);
	}
	
	return MOD_CONT;
}

/************************************CUSTOM FUNCTIONS***********************************/

void addItem(char *source, char *dest, int ghost)
{
	// There's no items in the list yet
	if (curItem == NULL) {
		curItem = (RecentRGItem *)malloc(sizeof(RecentRGItem));
		curItem->prev = NULL;
		
		firstItem = curItem;
	}
	// There's items
	else {
		curItem->next = (RecentRGItem *)malloc(sizeof(RecentRGItem));
		curItem->next->prev = curItem;
		
		curItem = curItem->next;
	}
	
	curItem->next = NULL;
	
	curItem->source = sstrdup(source);
	curItem->dest = sstrdup(dest);
	curItem->time = time(NULL);
	curItem->ghost = ghost;
}

void delItem(RecentRGItem *item)
{
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
		}
		// If it's a middle item
		else {
			item->prev->next = item->next;
			item->next->prev = item->prev;
		}
		
		// Free like you've never freed before!
		free(item->source);
		free(item->dest);
		free(item);
	}
}

// Remove all items from the list older than time
int timeoutList(time_t time)
{
	RecentRGItem *item = NULL;
	RecentRGItem *nextItem = NULL;
	int count = 0;
	
	nextItem = firstItem;
	
	while (nextItem) {
		item = nextItem;
		nextItem = item->next;
	
		if (item->time < time) {
			delItem(item);
			count++;
		}
	}
	
	return count;
}

void clearList()
{
	while (firstItem) {
		delItem(firstItem);
	}
}

RecentRGItem *findItemBySource(char *source)
{
	RecentRGItem *item = NULL;

	for (item = firstItem; item; item = item->next) {
		if (stricmp(source, item->source) == 0) {
			return item;
		}
	}
	
	return NULL;
}

/* Following function modified from do_forceid() in os_forceid.c by n00bie */
/* Copyright remains with original author(s) */
int forceid(User *u)
{
	NickAlias *na;
	char *vHost;
	char *vIdent = NULL;
	
	if (nick_identified(u)) {
		return 0;
	} else {
		if (u->na) {
			na = u->na;
			na->status |= NS_IDENTIFIED;
			na->last_seen = time(NULL);
			change_user_mode(u, "+r", "");

			if (NSModeOnID) {
				do_setmodes(u);
			}
			if (NSNickTracking) {
				nsStartNickTracking(u);
			}

			if (na->status & NS_IDENTIFIED) {
				vHost = getvHost(u->nick);
				vIdent = getvIdent(u->nick);
				if (vHost == NULL) {
					return 1;
				} else {
					if (vIdent) {
						notice_user(s_HostServ, u, "Your vhost of \2%s@%s\2 is now activated.", vIdent, vHost);
					} else {
						notice_user(s_HostServ, u, "Your vhost of \2%s\2 is now activated.", vHost);
					}
					anope_cmd_vhost_on(u->nick, vIdent, vHost);
				}
			}
		}
	}

	return 1;
}

/* Language stuffs */
void m_AddLanguages(void)
{
	char *langtable_en_us[] = {
		/* NOID_GHOST */
		"You have been automatically identified as you recently ghosted this nickname.",
		/* NOID_RECOVER */
		"You have been automatically identified as you recently recovered this nickname."
	};
	
	char *langtable_de[] = {
		/* NOID_GHOST */
		"Sie wurden automatisch identifiziert, da Sie vor Kurzem diese Nahme \"geghostet\" haben.",
		/* NOID_RECOVER */
		"Sie wurden automatisch identifiziert, da Sie vor Kurzem diese Nahme regeneriert haben."
	};
	
	char *langtable_fr[] = {
		/* NOID_GHOST */
		"Vous êtes identifié automatiquement, parce que vous avez récemment \"ghosté\" ce nom.",
		/* NOID_RECOVER */
		"Vous êtes identifié automatiquement, parce que vous avez récemment récupéré ce nom."
	};
	
	char *langtable_nl[] = {
		/* NOID_GHOST */
		"Je bent automatisch geïdentificeerd, omdat je recent deze naam geghost hebt.",
		/* NOID_RECOVER */
		"Je bent automatisch geïdentificeerd, omdat je recent deze naam gerecovered hebt."
	};

	moduleInsertLanguage(LANG_EN_US, LANG_NUM_STRINGS, langtable_en_us);
	moduleInsertLanguage(LANG_DE, LANG_NUM_STRINGS, langtable_de);
	moduleInsertLanguage(LANG_FR, LANG_NUM_STRINGS, langtable_fr);
	moduleInsertLanguage(LANG_NL, LANG_NUM_STRINGS, langtable_nl);
}

/* EOF */
