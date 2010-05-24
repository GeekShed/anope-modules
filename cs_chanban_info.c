/***************************************************************************************/
/* Anope Module : cs_chanban_info.c : v1.x                                             */
/* Phil Lavin - phil@geekshed.net                                                      */
/*                                                                                     */
/* (c) 2010 Phil Lavin                                                                 */
/*                                                                                     */
/* Fairly heavily based on ircd_community_info.c by katsklaw                           */
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
#include <assert.h>

#define AUTHOR "Phil"
#define VERSION "1.0"

/* Default database name */
#define DEFAULT_DB_NAME "chanban_info.db"
#define DEFAULT_XML_DB_NAME "chanban_info.xml"

/* Max length of ban appeal info string */
#define MAX_STR_LEN 300

/* Multi-language stuff */
#define LANG_NUM_STRINGS   5

#define CCHANBAN_ADD_SUCCESS  0
#define CCHANBAN_DEL_SUCCESS  1
#define CCHANBAN_HELP         2
#define CCHANBAN_HELP_CMD     3
#define CCHANBAN_STR_TOO_LONG 4

/*************************************************************************/

char *chanBanDBName = NULL;
char *chanBanXMLDBName = NULL;

int myAddBanAppealInfo(User * u);
int doModCont(User * u);
int myChanInfo(User * u);

int mBanAppealHelp(User * u);
int mMainSetHelp(User * u);
void m_AddLanguages(void);

int mLoadData(void);
int mSaveData(int argc, char **argv);
int mBackupData(int argc, char **argv);
int mLoadConfig();
int loadConfParam(char *name, char *defaultVal, char **ptr);
int mEventReload(int argc, char **argv);

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
	EvtHook *hook = NULL;

	int status;

	moduleAddAuthor(AUTHOR);
	moduleAddVersion(VERSION);

	alog("cs_chanban_info: Loading configuration directives...");
	if (mLoadConfig()) {
		return MOD_STOP;
	}

	c = createCommand("SET", myAddBanAppealInfo, NULL, -1, -1, -1, -1, -1);
	status = moduleAddCommand(CHANSERV, c, MOD_HEAD);

	/*
	Add the SET command again for the purpose of adding stuff to /cs help set
	calls doModCont as using NULL causes Anope to crash. This is due to a
	bug in 1.8.4. This is fixed in 1.8.5.
	*/
	c = createCommand("SET", doModCont, NULL, -1, -1, -1, -1, -1);
	moduleAddHelp(c, mMainSetHelp);
	status = moduleAddCommand(CHANSERV, c, MOD_TAIL);

	c = createCommand("SET BANINFO", NULL, NULL, -1, -1, -1, -1, -1);
	moduleAddHelp(c, mBanAppealHelp);
	moduleAddCommand(CHANSERV, c, MOD_HEAD);

	c = createCommand("INFO", myChanInfo, NULL, -1, -1, -1, -1, -1);
	status = moduleAddCommand(CHANSERV, c, MOD_TAIL);

	hook = createEventHook(EVENT_DB_SAVING, mSaveData);
	status = moduleAddEventHook(hook);

	hook = createEventHook(EVENT_DB_BACKUP, mBackupData);
	status = moduleAddEventHook(hook);

	hook = createEventHook(EVENT_RELOAD, mEventReload);
	status = moduleAddEventHook(hook);

	mLoadData();
	m_AddLanguages();

	return MOD_CONT;
}

/**
* Unload the module
**/
void AnopeFini(void)
{
	char *av[1];

	av[0] = sstrdup(EVENT_START);
	mSaveData(1, av);
	free(av[0]);

	if (chanBanDBName)
		free(chanBanDBName);
	if (chanBanXMLDBName)
		free(chanBanXMLDBName);
}

/*************************************************************************/

/**
* Provide the user interface to add/remove/update ban appeal information
* about a channel.
* @param u The user who executed this command
* @return MOD_CONT if we want to process other commands in this command
* stack, MOD_STOP if we dont
**/
int myAddBanAppealInfo(User * u)
{
	char *text = NULL;
	char *cmd = NULL;
	char *chan = NULL;
	char *info = NULL;
	ChannelInfo *ci = NULL;
	int is_servadmin = is_services_admin(u);

	/* Get the last buffer anope recived */
	text = moduleGetLastBuffer();
	if (text) {
		chan = myStrGetToken(text, ' ', 0);
		cmd = myStrGetToken(text, ' ', 1);

		if (cmd && chan) {
			if (strcasecmp(cmd, "BANINFO") == 0) {
				/* Get channel if it exists */
				if (!(ci = cs_findchan(chan))) {
					notice_lang(s_ChanServ, u, CHAN_X_NOT_REGISTERED, chan);
					free(chan);
					free(cmd);
					return MOD_STOP;
				}

				/* Check access */
				if (!is_servadmin && !check_access(u, ci, CA_SET)) {
					notice_lang(s_ChanServ, u, ACCESS_DENIED);
					free(chan);
					free(cmd);
					return MOD_STOP;
				}

				info = myStrGetTokenRemainder(text, ' ', 2);

				/* Info has been specified - i.e. ban info is being set */
				if (info) {
					info = normalizeBuffer(info);

					/* Check length is <= MAX_STR_LEN */
					if (strlen(info) > MAX_STR_LEN) {
						moduleNoticeLang(s_ChanServ, u, CCHANBAN_STR_TOO_LONG, MAX_STR_LEN);
						free(chan);
						free(cmd);
						free(info);

						return MOD_STOP;
					}

					/* Add the module data to the channel */
					moduleAddData(&ci->moduleData, "chanbaninfo", info);
					moduleNoticeLang(s_ChanServ, u, CCHANBAN_ADD_SUCCESS, chan);
					
					alog("cs_chanban_info: %s!%s@%s set ban appeal info on %s to: %s", u->nick, u->username, u->host, chan, info);

					free(info);
				}
				/* Info has NOT been specified - i.e. ban info is being unset */
				else {
					/* Del the module data from the channel */
					moduleDelData(&ci->moduleData, "chanbaninfo");
					moduleNoticeLang(s_ChanServ, u,	CCHANBAN_DEL_SUCCESS, chan);
					
					alog("cs_chanban_info: %s!%s@%s unset ban appeal info on %s", u->nick, u->username, u->host, chan);
				}

				free(cmd);
				free(chan);
				return MOD_STOP;
			}

			free(cmd);
			free(chan);
		}
	}

	return MOD_CONT;
}

/*************************************************************************/

/**
* Called after a user does /cs set
* Dummy function to get around (possible) bug in 1.8.4
* @param u The user who did the command
* @return MOD_CONT to continue processing commands or MOD_STOP to stop
**/
int doModCont(User * u)
{
	return MOD_CONT;
}

/*************************************************************************/

/*
 * Following function modified from http://www.lonelycactus.com/guilebook/x1355.html
 * Copyright remains with the original author
 * Modifications copyright Phil Lavin (2010)
 * This function converts the characters < > & and quotation marks
 * into their html entities, which are &lt; &gt; &amp; and &quot;
 * it also converts chars < ASCII 32 (space) and >= ASCII 127 (DEL) to their
 * &#xx; equivalents
 */
static char *html_entities (char const *raw_text) {
	int raw_length;
	int out_length;
	char *out_text;
	char new_char[2];
	int i;

	assert(raw_text != (char *)NULL);

	raw_length = strlen(raw_text);

	if (raw_length == 0) {
		out_text = (char *)malloc(sizeof(char));
		out_text[0] = '\0';
	} else {
		/* Count the number of characters needed for the output string */
		out_length = 0;
		for (i = 0; i < raw_length; i++) {
			unsigned char ch = (unsigned char)raw_text[i];

			if ((ch > 0 && ch < 32) || (ch >= 127)) {
				char entity[4]; // Max size 3 digits + \0 = 4
				sprintf(entity, "%d", (int)ch);
				out_length += (strlen(entity) + 7);
			} else {
				switch (ch) {
					case '<':
					case '>':
						out_length += 8;
						break;
					case '&':
						out_length += 9;
						break;
					case '"':
						out_length += 10;
						break;
					default:
						out_length += 1;
				}
			}
		}

		/* Convert some tags into their HTML entities */
		out_text = (char *)malloc((out_length + 1) * sizeof(char));
		out_text[0] = '\0';
		for (i = 0; i < raw_length; i++) {
			unsigned char ch = (unsigned char)raw_text[i];

			if ((ch > 0 && ch < 32) || (ch >= 127)) {
				char entity[11]; // Max size 6 char + 3 digits + 1 char + \0 = 11
				sprintf(entity, "&amp;#%d;", (int)ch);
				strcat(out_text, entity);
			} else {
				switch (ch) {
					case '<':
						   strcat(out_text, "&amp;lt;");
						   break;
					case '>':
						   strcat(out_text, "&amp;gt;");
						   break;
					case '&':
						   strcat(out_text, "&amp;amp;");
						   break;
					case '"':
						   strcat(out_text, "&amp;quot;");
						   break;
					default:
						   new_char[0] = ch;
						   new_char[1] = '\0';
						   strncat(out_text, new_char, 1);
						   break;
				}
			}
		}
	}

	return (out_text);
}

/*************************************************************************/

/**
* Called after a user does a /msg chanserv info chan
* @param u The user who requested info
* @return MOD_CONT to continue processing commands or MOD_STOP to stop
**/
int myChanInfo(User * u)
{
	char *text = NULL;
	char *chan = NULL;
	char *info = NULL;
	ChannelInfo *ci = NULL;

	/* Get the last buffer anope recived */
	text = moduleGetLastBuffer();
	if (text) {
		chan = myStrGetToken(text, ' ', 0);
		if (chan) {
			if ((ci = cs_findchan(chan))) {
				/* If we have any info on this channel */
				if ((info = moduleGetData(&ci->moduleData, "chanbaninfo"))) {
					notice_user(s_ChanServ, u, "Ban appeal information: %s", info);
					free(info);
				}
			}
			free(chan);
		}
	}
	return MOD_CONT;
}

/*************************************************************************/

/**
* Load data from the db file, and populate our ChanBanInfo lines
* @return 0 for success
**/
int mLoadData(void)
{
	int ret = 0;
	FILE *in;

	char *type = NULL;
	char *name = NULL;
	char *info = NULL;
	int len = 0;

	ChannelInfo *ci = NULL;

	/* will _never_ be this big thanks to the 512 limit of a message */
	char buffer[2000];
	if ((in = fopen(chanBanDBName, "r")) == NULL) {
		alog("cs_chanban_info: WARNING: can not open the %s database file! (it might not exist, this is not fatal)", chanBanDBName);
		ret = 1;
	} else {
		while (fgets(buffer, 1500, in)) {
			type = myStrGetToken(buffer, ' ', 0);
			name = myStrGetToken(buffer, ' ', 1);
			info = myStrGetTokenRemainder(buffer, ' ', 2);
			if (type) {
				if (name) {
					if (info) {
						len = strlen(info);
						/* Take the \n from the end of the line */
						info[len - 1] = '\0';
						if (stricmp(type, "C") == 0) {
							if ((ci = cs_findchan(name))) {
								moduleAddData(&ci->moduleData, "chanbaninfo",
								info);
							}
						}
						free(info);
					}
					free(name);
				}
				free(type);
			}
		}
	}
	return ret;
}

/**
* Save all our data to our db file
* @return 0 for success
**/
int mSaveData(int argc, char **argv)
{
	ChannelInfo *ci = NULL;
	int i = 0;
	int ret = 0;
	FILE *out;
	FILE *xmlout;
	char *info = NULL;

	if (argc >= 1) {
		if (!stricmp(argv[0], EVENT_START)) {
			/* Regular Database */
			if ((out = fopen(chanBanDBName, "w")) == NULL) {
				alog("cs_chanban_info: ERROR: can not open the regular database file!");
				anope_cmd_global(s_OperServ, "cs_chanban_info: ERROR: can not open the regular database file!");
				ret = 1;
			} else {
				/* XML Database */
				if ((xmlout = fopen(chanBanXMLDBName, "w")) == NULL) {
					alog("cs_chanban_info: ERROR: can not open the XML database file!");
					anope_cmd_global(s_OperServ, "cs_chanban_info.c: ERROR: can not open the XML database file!");
					ret = 1;
				} else {
					fprintf(xmlout, "<?xml version=\"1.0\" encoding=\"ISO-8859-1\" standalone=\"no\" ?>\n");
					fprintf(xmlout, "<baninfolist>\n");

					for (i = 0; i < 256; i++) {
						for (ci = chanlists[i]; ci; ci = ci->next) {
							/* If we have any info on this channel */
							if ((info = moduleGetData(&ci->moduleData, "chanbaninfo"))) {
								fprintf(out, "C %s %s\n", ci->name, info);
								fprintf(xmlout, "\t<baninfo>\n");

								char *nameEnt = html_entities(ci->name);
								char *infoEnt = html_entities(info);

								fprintf(xmlout, "\t\t<chan>%s</chan>\n", nameEnt);
								fprintf(xmlout, "\t\t<info>%s</info>\n", infoEnt);
								fprintf(xmlout, "\t</baninfo>\n");

								free(info);
								free(nameEnt);
								free(infoEnt);
							}
						}
					}

					fprintf(xmlout, "</baninfolist>\n");

					fclose(out);
					fclose(xmlout);
				}
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
int mBackupData(int argc, char **argv)
{
	if (!stricmp(argv[0], EVENT_START))
		ModuleDatabaseBackup(chanBanDBName);

	return MOD_CONT;
}

/**
* Load the configuration directives from Services configuration file.
* @return 0 for success
**/
int mLoadConfig(void)
{
	loadConfParam("chanBanDBName", DEFAULT_DB_NAME, &chanBanDBName);
	loadConfParam("chanBanXMLDBName", DEFAULT_XML_DB_NAME, &chanBanXMLDBName);
}

int loadConfParam(char *name, char *defaultVal, char **ptr)
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

	alog("cs_chanban_info: Directive %s loaded (%s)...", name, *ptr);

	return 0;
}

/**
* Manage the RELOAD EVENT
* @return MOD_CONT
**/
int mEventReload(int argc, char **argv)
{
	int ret = 0;
	if (argc >= 1) {
		if (!stricmp(argv[0], EVENT_START)) {
			alog("cs_chanban_info: Reloading configuration directives...");
			ret = mLoadConfig();
		} else {
			/* Nothing for now */
		}
	}

	if (ret)
		alog("cs_chanban_info: ERROR: An error has occured while reloading the configuration file");

	return MOD_CONT;
}

/*************************************************************************/

/**
* manages the multilanguage stuff
**/
void m_AddLanguages(void)
{
	char *langtable_en_us[] = {
		/* CCHANBAN_ADD_SUCCESS */
		"Ban appeal info for %s changed.",
		/* CCHANBAN_DEL_SUCCESS */
		"Ban appeal info for %s unset.",
		/* CCHANBAN_HELP */
		"Syntax: SET channel BANINFO [info]\n"
		"Associates the given ban appeal information with the channel. This\n"
		"information will be displayed whenever someone requests information on\n"
		"the channel with the INFO command. If no parameter is given, the currently\n"
		"associated ban appeal information will be deleted" ,
		/* CCHANBAN_HELP_CMD */
		" \n"
		"Commands added by cs_chanban_info:\n"
		"    BANINFO         Associate a ban appeal procedure with the channel",
		/* CCHANBAN_STR_TOO_LONG */
		"Ban appeal procedure text is too long. The limit is %d characters"
	};

	moduleInsertLanguage(LANG_EN_US, LANG_NUM_STRINGS, langtable_en_us);
}

/*************************************************************************/

int mBanAppealHelp(User * u)
{
	moduleNoticeLang(s_ChanServ, u, CCHANBAN_HELP);

	return MOD_CONT;
}

int mMainSetHelp(User * u)
{
	moduleNoticeLang(s_ChanServ, u, CCHANBAN_HELP_CMD);
	return MOD_CONT;
}

/*************************************************************************/

/* EOF */
