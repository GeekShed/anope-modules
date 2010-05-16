/***************************************************************************************/
/* Anope Module : cs_redirect.c : v1.x                                                 */
/* Phil Lavin - phil@anope.org                                                         */
/*                                                                                     */
/* (c) 2010 Phil Lavin                                                                 */
/*                                                                                     */
/* Partly based on ircd_community_info.c by katsklaw                                   */
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
#include <math.h>
#include <assert.h>

#define AUTHOR "Phil"
#define VERSION "1.0"

/* Default database name */
#define DEFAULT_DB_NAME "cs_redirect.db"

#define CMODE_L 0x00004000

/* Multi-language stuff */
#define LANG_NUM_STRINGS			13

#define CSREDIRECT_SUCCESS			0
#define CSDEREDIRECT_SUCCESS		1
#define CSREDIRECT_SYNTAX			2
#define CSREDIRECT_HELP_REDIRECT	3
#define CSREDIRECT_HELP				4
#define CSREDIRECT_NOLOOP			5
#define CSREDIRECT_PTREXISTS		6
#define CSREDIRECT_XTOY				7
#define CSREDIRECT_TOY				8
#define CSREDIRECT_SAMECHAN			9
#define CSREDIRECT_REDIRECTING		10
#define CSREDIRECT_LIST_SYNTAX		11
#define CSREDIRECT_HELP_LIST		12

/****************************************TYPEDEFS***************************************/

typedef struct cslistitem_ ChannelListItem;

/***************************************VARIABLES***************************************/

char *csRedirectDBName = NULL;

/****************************************STRUCTS****************************************/

struct cslistitem_ {
	ChannelListItem *next, *prev;
	char *chan;
	char *redirect;
	time_t setTime;
};

/**********************************FORWARD DECLARATIONS*********************************/

int AnopeInit(int argc, char **argv);
void AnopeFini(void);

int do_redirect(User *u);
int do_info_head(User *u);
int do_info_tail(User *u);
int do_list(User *u);
void do_help(User *u);
int do_help_redirect(User *u);
int do_help_list(User *u);

int do_event_join(int argc, char **argv);
int do_event_expire(int argc, char **argv);
int do_event_save(int argc, char **argv);
int do_event_backup(int argc, char **argv);
int do_event_reload(int argc, char **argv);

time_t mCharToTime(char *ctime);
char *mTimeToChar(time_t ttime);
int mTimeSize();
char *mGetRedirectChan(ChannelInfo *ci);
int mLoadConfig(void);
int mloadConfParam(char *name, char *defaultVal, char **ptr);
int mLoadData(void);
void mAddLanguages(void);

/***************************************ANOPE REQS**************************************/

/**
* AnopeInit is called when the module is loaded
* @param argc Argument count
* @param argv Argument list
* @return MOD_CONT to allow the module, MOD_STOP to stop it
**/
int AnopeInit(int argc, char **argv)
{
	Command *c;
	EvtHook *hook = NULL;
	int status;

	moduleAddAuthor(AUTHOR);
	moduleAddVersion(VERSION);

	alog("cs_redirect: Loading configuration directives...");
	if (mLoadConfig()) {
		return MOD_STOP;
	}

	c = createCommand("REDIRECT", do_redirect, NULL, -1, -1, -1, -1, -1);
	status = moduleAddCommand(CHANSERV, c, MOD_HEAD);
	moduleAddHelp(c, do_help_redirect);
	moduleSetChanHelp(do_help);
	
	c = createCommand("INFO", do_info_head, NULL, -1, -1, -1, -1, -1);
	status = moduleAddCommand(CHANSERV, c, MOD_HEAD);
	
	c = createCommand("INFO", do_info_tail, NULL, -1, -1, -1, -1, -1);	
	status = moduleAddCommand(CHANSERV, c, MOD_TAIL);
	
	c = createCommand("LIST", do_list, NULL, -1, -1, -1, -1, -1);
	status = moduleAddCommand(CHANSERV, c, MOD_HEAD);
	moduleAddHelp(c, do_help_list);

	hook = createEventHook(EVENT_JOIN_CHANNEL, do_event_join);
	status = moduleAddEventHook(hook);
	
	hook = createEventHook(EVENT_DB_EXPIRE, do_event_expire);
	status = moduleAddEventHook(hook);
	
	hook = createEventHook(EVENT_DB_SAVING, do_event_save);
	status = moduleAddEventHook(hook);

	hook = createEventHook(EVENT_DB_BACKUP, do_event_backup);
	status = moduleAddEventHook(hook);

	hook = createEventHook(EVENT_RELOAD, do_event_reload);
	status = moduleAddEventHook(hook);

	mLoadData();
	mAddLanguages();

	return MOD_CONT;
}

/**
* Unload the module
**/
void AnopeFini(void)
{
	char *av[1];

	av[0] = sstrdup(EVENT_START);
	do_event_save(1, av);
	free(av[0]);

	if (csRedirectDBName)
		free(csRedirectDBName);
}

/************************************COMMAND HANDLERS***********************************/

/**
* Provide the user interface to add/remove/update ban appeal information
* about a channel.
* @param u The user who executed this command
* @return MOD_CONT if we want to process other commands in this command
* stack, MOD_STOP if we dont
**/
int do_redirect(User *u)
{
	char *text = NULL;
	char *orig = NULL;
	char *new = NULL;
	char *info = NULL;
	char *time = NULL;
	char *redirectChan = NULL;
	char *lastused;
	ChannelInfo *ci = NULL;
	ChannelInfo *ci2 = NULL;
	int rtn;
	int is_servadmin = is_services_admin(u);

	if (!(text = moduleGetLastBuffer())) {
		moduleNoticeLang(s_ChanServ, u, CSREDIRECT_SYNTAX, ci->name);
		rtn = MOD_STOP;
	}
	else if (!(orig = myStrGetToken(text, ' ', 0))) {
		moduleNoticeLang(s_ChanServ, u, CSREDIRECT_SYNTAX, ci->name);
		rtn = MOD_STOP;
	}
	else {
		/* SETTING */
		if ((new = myStrGetToken(text, ' ', 1))) {
			if (!(ci = cs_findchan(orig))) {
				notice_lang(s_ChanServ, u, CHAN_X_NOT_REGISTERED, orig);
			}
			else if (!(ci2 = cs_findchan(new))) {
				notice_lang(s_ChanServ, u, CHAN_X_NOT_REGISTERED, new);
			}
			else if (!is_servadmin && !is_real_founder(u, ci)) {
				notice_lang(s_ChanServ, u, ACCESS_DENIED);
			}
			else if (!is_servadmin && !is_real_founder(u, ci2) && !is_identified(u, ci2)) {
				notice_lang(s_ChanServ, u, ACCESS_DENIED);
			}
			else if (ci->flags & CI_SUSPENDED || ci2->flags & CI_SUSPENDED) {
				notice_lang(s_ChanServ, u, ACCESS_DENIED);
			}
			else if (ci->flags & CI_VERBOTEN || ci2->flags & CI_VERBOTEN) {
				notice_lang(s_ChanServ, u, ACCESS_DENIED);
			}
			else if (!is_servadmin && (info = moduleGetData(&ci2->moduleData, "redirect"))) {
				moduleNoticeLang(s_ChanServ, u, CSREDIRECT_NOLOOP);				
				free(info);
			}
			else if (ci == ci2) {
				moduleNoticeLang(s_ChanServ, u, CSREDIRECT_SAMECHAN);					
			}
			else if (!is_servadmin && (redirectChan = mGetRedirectChan(ci))) {
				moduleNoticeLang(s_ChanServ, u, CSREDIRECT_PTREXISTS, ci->name, redirectChan);
				free(redirectChan);
			}
			else {
				time = mTimeToChar(ci2->last_used);
		
				moduleAddData(&ci->moduleData, "redirect", ci2->name);
				
				if (!(lastused = moduleGetData(&ci->moduleData, "redirect-lastused"))) {
					moduleAddData(&ci->moduleData, "redirect-lastused", time);
				}
				else {
					free(lastused);
				}
				
				free(time);
				
				moduleNoticeLang(s_ChanServ, u, CSREDIRECT_SUCCESS, ci->name, ci2->name);
				alog("%s: %s!%s@%s redirected %s to %s",
					s_ChanServ, u->nick, u->username, u->host, ci->name, ci2->name);
			}
		
			free(new);
		}
		/* UNSETTING */
		else {
			if (!(ci = cs_findchan(orig))) {
				notice_lang(s_ChanServ, u, CHAN_X_NOT_REGISTERED, orig);
			}
			else if (!is_servadmin) {
				notice_lang(s_ChanServ, u, ACCESS_DENIED);
			}
			else {
				moduleDelData(&ci->moduleData, "redirect");
				moduleDelData(&ci->moduleData, "redirect-lastused");
			
				moduleNoticeLang(s_ChanServ, u, CSDEREDIRECT_SUCCESS, ci->name);
				alog("%s: %s!%s@%s removed the redirect on %s",
					s_ChanServ, u->nick, u->username, u->host, ci->name);
			}
		}
		
		rtn = MOD_CONT;
	}
	
	if (orig)
		free(orig);

	return rtn;;
}

/**
* Called after a user does a /msg chanserv info chan
* @param u The user who requested info
* @return MOD_CONT to continue processing commands or MOD_STOP to stop
**/
int do_info_head(User *u)
{
	char *text = NULL;
	char *info = NULL;
	char *chan = NULL;
	ChannelInfo *ci = NULL;	
	int rtn = MOD_CONT;

	if (is_services_admin(u)) {
		rtn = MOD_CONT;
	}
	else if (!(text = moduleGetLastBuffer())) {
		rtn = MOD_CONT;
	}
	else if (!(chan = myStrGetToken(text, ' ', 0))) {
		rtn = MOD_CONT;
	}
	else if (!(ci = cs_findchan(chan))) {
		rtn = MOD_CONT;
	}
	else if ((ci->flags & CI_VERBOTEN)) {
		rtn = MOD_CONT;
	}
	else if (!(info = moduleGetData(&ci->moduleData, "redirect"))) {
		rtn = MOD_CONT;
	}
	else {					
		moduleNoticeLang(s_ChanServ, u, CSREDIRECT_XTOY, ci->name, info);
		rtn = MOD_STOP;
	}

	if (chan)
		free(chan);
	if (info)
		free(info);
	
	return rtn;
}

/**
* Called after a user does a "/msg chanserv info chan"
* @param u The user who requested info
* @return MOD_CONT to continue processing commands or MOD_STOP to stop
**/
int do_info_tail(User *u)
{
	char *text = NULL;
	char *info = NULL;
	char *lastused = NULL;
	char *chan = NULL;
	char buf[BUFSIZE];
	struct tm *tm;
	time_t lu;
	ChannelInfo *ci = NULL;
	int rtn = MOD_CONT;

	if (!is_services_admin(u)) {
		rtn = MOD_CONT;
	}
	else if (!(text = moduleGetLastBuffer())) {
		rtn = MOD_CONT;
	}
	else if (!(chan = myStrGetToken(text, ' ', 0))) {
		rtn = MOD_CONT;
	}
	else if (!(ci = cs_findchan(chan))) {
		rtn = MOD_CONT;
	}
	else if ((ci->flags & CI_VERBOTEN)) {
		rtn = MOD_CONT;
	}
	else if (!(info = moduleGetData(&ci->moduleData, "redirect"))) {
		rtn = MOD_CONT;
	}
	else if (!(lastused = moduleGetData(&ci->moduleData, "redirect-lastused"))) {
		rtn = MOD_CONT;
	}
	else {
		lu = mCharToTime(lastused);
		tm = localtime(&lu);
		strftime_lang(buf, sizeof(buf), u, STRFTIME_DATE_TIME_FORMAT, tm);
	
		moduleNoticeLang(s_ChanServ, u, CSREDIRECT_TOY, info, buf);
		
		rtn = MOD_CONT;
	}
	
	if (chan)
		free(chan);
	if (info)
		free(info);
	if (lastused);
		free(lastused);

	return rtn;
}

/**
* Called after a user does a "/msg chanserv list"
* @param u The user who requested info
* @return MOD_CONT to continue processing commands or MOD_STOP to stop
**/
int do_list(User *u)
{
	char *text = NULL;
	char *pattern = NULL;
	char *arg = NULL;
	char *redirect = NULL;
	char *lastused = NULL;
	char buf[BUFSIZE];
	int i;
	int chanlen = 0;
	int redirectlen = 0;
	int rtn = MOD_CONT;
	time_t setTime;
	time_t expireTime;
	struct tm *tm;
	ChannelInfo *ci = NULL;
	ChannelListItem *cli = NULL;
	ChannelListItem *firstcli = NULL;
	ChannelListItem *nextcli = NULL;

	if (!(text = moduleGetLastBuffer())) {
		moduleNoticeLang(s_ChanServ, u, CSREDIRECT_LIST_SYNTAX);
		rtn = MOD_STOP;
	}
	else if (!(pattern = myStrGetToken(text, ' ', 0))) {
		moduleNoticeLang(s_ChanServ, u, CSREDIRECT_LIST_SYNTAX);
		rtn = MOD_STOP;
	}
	else if (!(arg = myStrGetToken(text, ' ', 1))) {
		rtn = MOD_CONT;
	}
	else if (stricmp(arg, "REDIRECTED") != 0) {
		rtn = MOD_CONT;
	}
	else if (!is_services_admin(u)) {
		notice_lang(s_ChanServ, u, ACCESS_DENIED);
		rtn = MOD_STOP;
	}
	else {
		for (i = 0; i < 256; i++) {
			for (ci = chanlists[i]; ci; ci = ci->next) {
				/* If the channel name matches pattern */
				if ((stricmp(pattern, ci->name) == 0) || match_wild_nocase(pattern, ci->name)) {
					/* If we have any info on this channel */
					if ((redirect = moduleGetData(&ci->moduleData, "redirect"))) {
						if ((lastused = moduleGetData(&ci->moduleData, "redirect-lastused"))) {
							setTime = mCharToTime(lastused);

							// Allocate memory
							if (!cli) {
								cli = (ChannelListItem *)malloc(sizeof(ChannelListItem));
										
								// Null the first previous pointer
								cli->prev = NULL;
								// Remember first
								firstcli = cli;
							}
							else {
								cli->next = (ChannelListItem *)malloc(sizeof(ChannelListItem));
											
								// Point next previous to current
								cli->next->prev = cli;
								// Make current next
								cli = cli->next;
							}
				
							// Null the current next pointer
							cli->next = NULL;
							
							// Add data
							cli->chan = sstrdup(ci->name);
							cli->redirect = sstrdup(redirect);
							cli->setTime = setTime;
							
							// Form maximum length ints
							if (strlen(ci->name) > chanlen)
								chanlen = strlen(ci->name);												
							if (strlen(redirect) > redirectlen)
								redirectlen = strlen(redirect);

							free(lastused);
						}
										
						free(redirect);
					}
				}
			}
		}
						
		// Ensure max len ints are > len of column headings
		if (chanlen < 7)
			chanlen = 7;			
		if (redirectlen < 8)
			redirectlen = 8;
			
		// Notice user col headings
		notice_user(s_ChanServ, u, "List of redirected channels matching %s:", pattern);
		notice_user(s_ChanServ, u, "   %-*s %-*s %s", chanlen + 2, "Channel", redirectlen + 2, "Redirect", "Expiry Date");
		
		// Loop and process linked list
		nextcli = firstcli;												
		while (nextcli != NULL) {
			// Cur cli is next cli
			cli = nextcli;

			// Next cli is next cli
			nextcli = cli->next;

			// Calc expire time of channel
			expireTime = cli->setTime + CSExpire;

			// Make pretty string representation of time
			tm = localtime(&expireTime);
			strftime_lang(buf, sizeof(buf), u, STRFTIME_DATE_TIME_FORMAT, tm);

			// Notice the user col contents
			notice_user(s_ChanServ, u, "   %-*s %-*s %s", chanlen + 2, cli->chan, redirectlen + 2, cli->redirect, buf);

			// Free the sstrdups and malloc for the struct
			free(cli->chan);
			free(cli->redirect);
			free(cli);
		}
		
		// Stop as we have handled the list ourselves
		rtn = MOD_STOP;
	}
	
	if (arg)
		free(arg);
	if (pattern)
		free(pattern);

	return rtn;
}

/**
* Called after a user does a "/msg chanserv help"
* @param u The user who requested info
* @return MOD_CONT to continue processing commands or MOD_STOP to stop
**/
void do_help(User *u)
{
	moduleNoticeLang(s_ChanServ, u, CSREDIRECT_HELP);
}

/**
* Called after a user does a "/msg chanserv help redirect"
* @param u The user who requested info
* @return MOD_CONT to continue processing commands or MOD_STOP to stop
**/
int do_help_redirect(User *u)
{
	moduleNoticeLang(s_ChanServ, u, CSREDIRECT_SYNTAX);
	notice_user(s_ChanServ, u, " \n");
	moduleNoticeLang(s_ChanServ, u, CSREDIRECT_HELP_REDIRECT, CSExpire / 86400);
	
	return MOD_CONT;
}

/**
* Called after a user does a "/msg chanserv help list"
* @param u The user who requested info
* @return MOD_CONT to continue processing commands or MOD_STOP to stop
**/
int do_help_list(User *u)
{
	moduleNoticeLang(s_ChanServ, u, CSREDIRECT_LIST_SYNTAX);
	notice_user(s_ChanServ, u, " \n");
	moduleNoticeLang(s_ChanServ, u, CSREDIRECT_HELP_LIST);
	
	return MOD_STOP;
}

/*************************************EVENT HANDLERS************************************/

/**
* Manage the JOIN_CHANNEL
* @return MOD_CONT
**/
int do_event_join(int argc, char **argv)
{
	char *redirect = NULL;
	char *lastused = NULL;
	char *key = NULL;
	User *u = NULL;
	ChannelInfo *ci = NULL;
	ChannelInfo *ci2 = NULL;
	Channel *c = NULL;
	int rtn = MOD_CONT;
	
	u = finduser(argv[1]);
	
	if (!u || u->isSuperAdmin) {
		rtn  = MOD_CONT;
	}
	else if (!(ci = cs_findchan(argv[2]))) {
		rtn = MOD_CONT;
	}
	else if (!(redirect = moduleGetData(&ci->moduleData, "redirect"))) {
		rtn = MOD_CONT;
	}
	else if (!(lastused = moduleGetData(&ci->moduleData, "redirect-lastused"))) {
		rtn = MOD_CONT;
	}
	else {
		/* Event Start */
		if (!stricmp(argv[0], EVENT_START)) {
			if (!(ci2 = cs_findchan(redirect))) {
				rtn = MOD_CONT;
			}		
			else if (ci->flags & CI_SUSPENDED || ci2->flags & CI_SUSPENDED) {
				rtn = MOD_CONT;
			}
			else if (ci->flags & CI_VERBOTEN || ci2->flags & CI_VERBOTEN) {
				rtn = MOD_CONT;
			}
			else {
				c = findchan(redirect);

				if (c && (c->mode & CMODE_L)) {
					rtn = MOD_CONT;
				}
				else {
					if (c && c->key)
						key = sstrdup(c->key);

					anope_cmd_svspart(s_ChanServ, u->nick, ci->name);
					moduleNoticeLang(s_ChanServ, u, CSREDIRECT_REDIRECTING, redirect);
					anope_cmd_svsjoin(s_OperServ, u->nick, redirect, key);
					
					rtn = MOD_STOP;
					
					if (key)
						free(key);
				}
			}
		}
		/* Event Stop */
		else {
			// Reset last used time
			ci->last_used = mCharToTime(lastused);
		}
	}
	
	if (lastused)
		free(lastused);
		
	if (redirect)
		free(redirect);

	return rtn;
}

/**
* Manage the DB_EXPIRE
* @return MOD_CONT
**/
int do_event_expire(int argc, char **argv)
{
	int i = 0;
	int lu;
	char *lastused = NULL;
	ChannelInfo *ci = NULL;
	
	if (!stricmp(argv[0], EVENT_START)) {
		for (i = 0; i < 256; i++) {
			for (ci = chanlists[i]; ci; ci = ci->next) {
				if ((lastused = moduleGetData(&ci->moduleData, "redirect-lastused"))) {
					lu = mCharToTime(lastused);
				
					ci->last_used = lu;
					free(lastused);
				}
			}
		}
	}
	
	return MOD_CONT;
}

/**
* Save all our data to our db file
* @return 0 for success
**/
int do_event_save(int argc, char **argv)
{
	ChannelInfo *ci = NULL;
	int i = 0;
	int ret = 0;
	FILE *out;
	char *info = NULL;
	char *lastused = NULL;

	if (argc >= 1) {
		if (!stricmp(argv[0], EVENT_START)) {
			if ((out = fopen(csRedirectDBName, "w")) == NULL) {
				alog("cs_redirect: ERROR: can not open the database file!");
				anope_cmd_global(s_OperServ, "cs_redirect: ERROR: can not open the database file!");
				ret = 1;
			} else {
				for (i = 0; i < 256; i++) {
					for (ci = chanlists[i]; ci; ci = ci->next) {
						/* If we have any info on this channel */
						if ((info = moduleGetData(&ci->moduleData, "redirect"))) {
							if ((lastused = moduleGetData(&ci->moduleData, "redirect-lastused"))) {
								fprintf(out, "%s %s %s\n", ci->name, info, lastused);
								free(lastused);
							}
							free(info);
						}
					}
				}

				fclose(out);
			}
		} else {
			ret = 0;
		}
	}

	return ret;
}

/**
* Backup our databases using the commands provided by Anope
* @return MOD_CONT
**/
int do_event_backup(int argc, char **argv)
{
	if (!stricmp(argv[0], EVENT_START))
		ModuleDatabaseBackup(csRedirectDBName);

	return MOD_CONT;
}

/**
* Manage the RELOAD EVENT
* @return MOD_CONT
**/
int do_event_reload(int argc, char **argv)
{
	int ret = 0;
	if (argc >= 1) {
		if (!stricmp(argv[0], EVENT_START)) {
			alog("cs_redirect: Reloading configuration directives...");
			ret = mLoadConfig();
		}
	}

	if (ret)
		alog("cs_redirect: ERROR: An error has occured while reloading the configuration file");

	return MOD_CONT;
}

/************************************CUSTOM FUNCTIONS***********************************/

/**
* Convert value referenced by char pointer to time_t
* @return time_t
**/
time_t mCharToTime(char *ctime) {
	time_t ttime;

	if (sizeof(time_t) == sizeof(long) || sizeof(time_t) == sizeof(int)) {
		ttime = strtol(ctime, (char **) NULL, 10);
	}
	else if (sizeof(time_t) == sizeof(long long)) {
		ttime = strtoll(ctime, (char **) NULL, 10);
	}
	
	return ttime;
}

/**
* Convert value referenced by char pointer to time_t
* @return time_t
**/
char *mTimeToChar(time_t ttime)
{
	// Accommodates up to 128 bit time_t values
	char *ctime = NULL;	
	ctime = malloc(mTimeSize());
	
	sprintf(ctime, "%ld", ttime);
	
	return ctime;
}

/**
* Get number of chars a time_t would consume as a string (including \0)
* @return unsigned long
**/
int mTimeSize()
{
	long double largestVal;
	int i = 0;
	
	largestVal = pow(2, sizeof(time_t) * 8);
	
	while (largestVal >= 10) {
		largestVal /= 10;
		i++;
	}
	
	return i + 1;
}

/**
* Get the name of the first channel found that redirects to ci
* @return char*
**/
char *mGetRedirectChan(ChannelInfo *ci)
{
	char *redirectChan;
	int i = 0;
	char *redirect;
	ChannelInfo *ci2 = NULL;

	for (i = 0; i < 256; i++) {
		for (ci2 = chanlists[i]; ci2; ci2 = ci2->next) {
			if ((redirect = moduleGetData(&ci2->moduleData, "redirect"))) {
				if (stricmp(redirect, ci->name) == 0) {
					redirectChan = sstrdup(ci2->name);

					free(redirect);
					return redirectChan;
				}

				free(redirect);
			}
		}
	}

	return NULL;
}

/**
* Load the configuration directives from Services configuration file.
* @return 0 for success
**/
int mLoadConfig(void)
{
	mloadConfParam("CSRedirectDBName", DEFAULT_DB_NAME, &csRedirectDBName);
	
	return 0;
}

/**
* Load a configuration directive from Services configuration file.
* @return 0 for success
**/
int mloadConfParam(char *name, char *defaultVal, char **ptr)
{
	char *tmp = NULL;

	Directive directivas[] = {
		{name, {{PARAM_STRING, PARAM_RELOAD, &tmp}}},
	};

	Directive *d = &directivas[0];
	moduleGetConfigDirective(d);

	if (tmp) {
		*ptr = tmp;
	} else {
		*ptr = sstrdup(defaultVal);
	}

	alog("cs_redirect: Directive %s loaded (%s)...", name, *ptr);

	return 0;
}

/**
* Load data from the db file, and populate our ChanBanInfo lines
* @return 0 for success
**/
int mLoadData(void)
{
	int ret = 0;
	FILE *in;

	char *orig = NULL;
	char *new = NULL;
	char *lastused;
	int len = 0;

	ChannelInfo *ci = NULL;

	/* will _never_ be this big thanks to the limit of channel lengths */
	char buffer[2000];
	
	if ((in = fopen(csRedirectDBName, "r")) == NULL) {
		alog("cs_redirect: WARNING: can not open the %s database file! (it might not exist, this is not fatal)", csRedirectDBName);
		ret = 1;
	} else {
		while (fgets(buffer, 1500, in)) {
			orig = myStrGetToken(buffer, ' ', 0);
			new = myStrGetToken(buffer, ' ', 1);
			lastused = myStrGetToken(buffer, ' ', 2);
			if (orig) {
				if (new) {
					if (lastused) {
						len = strlen(lastused);
						/* Take the \n from the end of the line */
						lastused[len - 1] = '\0';
						
						if ((ci = cs_findchan(orig))) {
							moduleAddData(&ci->moduleData, "redirect", new);
							moduleAddData(&ci->moduleData, "redirect-lastused", lastused);
						}
						
						free(lastused);
					}
					free(new);
				}
				free(orig);
			}
		}
	}
	return ret;
}

/**
* Manages the multilanguage stuff
**/
void mAddLanguages(void)
{
	char *langtable_en_us[] = {
		/* CSREDIRECT_SUCCESS */
		"Redirect added for %s to %s",
		/* CSDEREDIRECT_SUCCESS */
		"Redirect on %s unset",
		/* CSREDIRECT_SYNTAX */
		"Syntax: REDIRECT oldchannel newchannel",
		/* CSREDIRECT_HELP_REDIRECT */
		"Redirects all users joining oldchannel to newchannel.\n"
		"Once this is set it can only be reversed by a network admin and oldchannel\n"
		"will be automatically dropped after %d days.",
		/* CSREDIRECT_HELP */
		"    REDIRECT   Directs users who join the channel to another channel",
		/* CSREDIRECT_NOLOOP */
		"You cannot redirect to a channel on which a redirect already exists",
		/* CSREDIRECT_PTREXISTS */
		"You cannot create a redirect for %s because %s already\n"
		"redirects to this channel and chaining redirects is bad. Remove the existing\n"
		"redirect first.",
		/* CSREDIRECT_XTOY */
		"Channel %s redirects to %s",
		/* CSREDIRECT_TOY */
		"This channel redirects to %s (%s)",
		/* CSREDIRECT_SAMECHAN */
		"Original and new channels cannot be the same",
		/* CSREDIRECT_REDIRECTING */
		"This channel has moved to %s. Please update your autojoins.\n"
		"I am redirecting you now...",
		/* CSREDIRECT_LIST_SYNTAX */
		"Syntax: LIST pattern [FORBIDDEN] [SUSPENDED] [NOEXPIRE] [REDIRECTED]\n",
		/* CSREDIRECT_HELP_LIST */
        "Lists all registered channels matching the given pattern.\n"
        "Channels with the PRIVATE option set will only be displayed\n"
        "to Services admins. Channels with the NOEXPIRE option set\n"
        "will have a ! appended to the channel name for Services admins.\n"
		" \n"
        "If the FORBIDDEN, SUSPENDED or NOEXPIRE options are given, only\n"
        "channels which, respectively, are FORBIDden, SUSPENDed or have\n"
        "the NOEXPIRE flag set will be displayed.  If multiple options are\n"
        "given, more types of channels will be displayed. These options are\n"
        "limited to Services admins.\n",
	};

	moduleInsertLanguage(LANG_EN_US, LANG_NUM_STRINGS, langtable_en_us);
}

/* EOF */
