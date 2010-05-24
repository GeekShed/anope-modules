/***************************************************************************************/
/* Anope Module : ns_qdbban.c : v1.x                                                   */
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
#define VERSION "1.0.0"

/* Default database name */
#define DEFAULT_DB_NAME "qdbban.db"
#define DEFAULT_XML_DB_NAME "qdbban.xml"

/* Multi-language stuff */
#define LANG_NUM_STRINGS		6

#define QDBBAN_ADD_SUCCESS		0
#define QDBBAN_DEL_SUCCESS		1
#define QDBBAN_HELP    		    2
#define QDBBAN_HELP_CMD 	    3
#define QDBBAN_NS_HELP			4
#define QDBBAN_NS_HELP_QLIST	5

/*************************************************************************/

char *qdbBanDBName = NULL;
char *qdbBanXMLDBName = NULL;

int myAddQDBBan(User * u);
int doModCont(User * u);
int myNickInfo(User * u);
int mQList(User * u);

int mQDBBanHelp(User * u);
int mMainSetHelp(User * u);
void nsHelp(User * u);
int nsHelpQList(User * u);
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

	alog("ns_qdbban: Loading configuration directives...");
	if (mLoadConfig()) {
		return MOD_STOP;
	}

	c = createCommand("SASET", myAddQDBBan, is_services_oper, -1, -1, -1, -1, -1);
	status = moduleAddCommand(NICKSERV, c, MOD_HEAD);

	c = createCommand("QLIST", mQList, is_services_oper, -1, -1, -1, -1, -1);
	moduleAddOperHelp(c, nsHelpQList);
	moduleSetNickHelp(nsHelp);
	status = moduleAddCommand(NICKSERV, c, MOD_HEAD);

	/*
	Add the SASET command again for the purpose of adding stuff to /ns help saset
	calls doModCont as using NULL causes Anope to crash. This is due to a (possible)
	bug in 1.8.4.
	*/
	c = createCommand("SASET", doModCont, NULL, -1, -1, -1, -1, -1);
	moduleAddOperHelp(c, mMainSetHelp);
	status = moduleAddCommand(NICKSERV, c, MOD_TAIL);

	c = createCommand("SASET QDBBAN", NULL, NULL, -1, -1, -1, -1, -1);
	moduleAddOperHelp(c, mQDBBanHelp);
	moduleAddCommand(NICKSERV, c, MOD_HEAD);

	c = createCommand("INFO", myNickInfo, NULL, -1, -1, -1, -1, -1);
	status = moduleAddCommand(NICKSERV, c, MOD_TAIL);

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

	if (qdbBanDBName)
		free(qdbBanDBName);
	if (qdbBanXMLDBName)
		free(qdbBanXMLDBName);
}

/*************************************************************************/

/**
* Provide the user interface to add/remove a qdb ban from a nickname
* @param u The user who executed this command
* @return MOD_CONT if we want to process other commands in this command
* stack, MOD_STOP if we dont
**/
int myAddQDBBan(User * u)
{
	char *text;
	char *nick;
	char *command;
	char *setting;
	int stop = 0;
	NickCore *nc;
	NickAlias *na;

	text = moduleGetLastBuffer();
	if (text) {
		nick = myStrGetToken(text, ' ', 0);
		if (nick) {
			command = myStrGetToken(text, ' ', 1);
			if (command) {
				if (stricmp(command, "QDBBAN") == 0) {
					setting = myStrGetToken(text, ' ', 2);
					if (setting) {
						if (!(na = findnick(nick))) {
							notice_lang(s_NickServ, u, NICK_X_NOT_REGISTERED, nick);
						}
						else {
							nc = na->nc;

							if (stricmp(setting, "ON") == 0) {
								moduleAddData(&nc->moduleData, "qdbban", "on");
								moduleNoticeLang(s_NickServ, u, QDBBAN_ADD_SUCCESS, nc->display);
								alog("ns_qdbban: %s!%s@%s banned %s from using the QDB", u->nick, u->username, u->host, nc->display);
							}
							else if (stricmp(setting, "OFF") == 0) {
								moduleDelData(&nc->moduleData, "qdbban");
								moduleNoticeLang(s_NickServ, u, QDBBAN_DEL_SUCCESS, nc->display);
								alog("ns_qdbban: %s!%s@%s unbanned %s from using the QDB", u->nick, u->username, u->host, nc->display);
							}
							else {
								moduleNoticeLang(s_NickServ, u, QDBBAN_HELP);
							}
						}
						free(setting);
					}
					else {
						moduleNoticeLang(s_NickServ, u, QDBBAN_HELP);
					}

					stop = 1;
				}
				free(command);
			}
			free(nick);
		}
	}

	if (stop == 0) {
		return MOD_CONT;
	}
	else {
		return MOD_STOP;
	}
}

/*************************************************************************/

/**
* Called after a user does /ns saset
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
* Called after an svsadmin does a /ns info nick
* @param u The user who requested info
* @return MOD_CONT to continue processing commands or MOD_STOP to stop
**/
int myNickInfo(User * u)
{
	char *text;
	char *nick;
	char *data;
	NickCore *nc;
	NickAlias *na;

	if (is_services_oper(u)) {
		text = moduleGetLastBuffer();
		if (text) {
			nick = myStrGetToken(text, ' ', 0);
			if (nick) {
				if ((na = findnick(nick))) {
					nc = na->nc;

					if ((data = moduleGetData(&nc->moduleData, "qdbban"))) {
						notice_user(s_NickServ, u, "%s is banned from submitting to the QDB", na->nick);
					}

					if (data)
						free(data);
				}
				free(nick);
			}
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
	FILE *in;

	char *nick = NULL;
	int len = 0;

	NickCore *nc = NULL;

	/* will _never_ be this big thanks to the 512 limit of a message */
	char buffer[2000];
	if ((in = fopen(qdbBanDBName, "r")) == NULL) {
		alog("ns_qdbban: WARNING: can not open the %s database file! (it might not exist, this is not fatal)", qdbBanDBName);
		return 1;
	} else {
		while (fgets(buffer, 1500, in)) {
			nick = myStrGetToken(buffer, ' ', 0);
			if (nick) {
				len = strlen(nick);
				/* Take the \n from the end of the nick */
				nick[len - 1] = '\0';
				if ((nc = findcore(nick))) {
					moduleAddData(&nc->moduleData, "qdbban", "on");
				}
				free(nick);
			}
		}
		return 0;
	}
	return 0;
}

/**
* Save all our data to our db file
* @return 0 for success
**/
int mSaveData(int argc, char **argv)
{
	NickCore *nc = NULL;
	int i = 0;
	int ret = 0;
	FILE *out;
	FILE *xmlout;
	char *data;

	if (argc >= 1) {
		if (!stricmp(argv[0], EVENT_START)) {
			/* Regular Database */
			if ((out = fopen(qdbBanDBName, "w")) == NULL) {
				alog("ns_qdbban: ERROR: can not open the regular database file!");
				anope_cmd_global(s_OperServ, "ns_qdbban: ERROR: can not open the regular database file!");
				ret = 1;
			} else {
				/* XML Database */
				if ((xmlout = fopen(qdbBanXMLDBName, "w")) == NULL) {
					alog("ns_qdbban: ERROR: can not open the XML database file!");
					anope_cmd_global(s_OperServ, "ns_qdbban.c: ERROR: can not open the XML database file!");
					ret = 1;
				} else {
					fprintf(xmlout, "<?xml version=\"1.0\" encoding=\"ISO-8859-1\" standalone=\"no\" ?>\n");
					fprintf(xmlout, "<qdbbanlist>\n");

					for (i = 0; i < 1024; i++) {
						for (nc = nclists[i]; nc; nc = nc->next) {
							/* If we have any info on this user */
							if ((data = moduleGetData(&nc->moduleData, "qdbban"))) {
								fprintf(out, "%s\n", nc->display);
								fprintf(xmlout, "\t<qdbban>\n");

								char *nickEnt = html_entities(nc->display);

								fprintf(xmlout, "\t\t<nick>%s</nick>\n", nickEnt);
								fprintf(xmlout, "\t</qdbban>\n");

								free(nickEnt);
								free(data);
							}
						}
					}

					fprintf(xmlout, "</qdbbanlist>\n");

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
		ModuleDatabaseBackup(qdbBanDBName);

	return MOD_CONT;
}

/**
* Load the configuration directives from Services configuration file.
* @return 0 for success
**/
int mLoadConfig(void)
{
	loadConfParam("qdbBanDBName", DEFAULT_DB_NAME, &qdbBanDBName);
	loadConfParam("qdbBanXMLDBName", DEFAULT_XML_DB_NAME, &qdbBanXMLDBName);

	return MOD_CONT;
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

	alog("ns_qdbban: Directive %s loaded (%s)...", name, *ptr);

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
			alog("ns_qdbban: Reloading configuration directives...");
			ret = mLoadConfig();
		} else {
			/* Nothing for now */
		}
	}

	if (ret)
		alog("ns_qdbban: ERROR: An error has occured while reloading the configuration file");

	return MOD_CONT;
}

/*************************************************************************/

/**
* manages the multilanguage stuff
**/
void m_AddLanguages(void)
{
	char *langtable_en_us[] = {
		/* QDBBAN_ADD_SUCCESS */
		"%s has been banned from submitting to the QDB",
		/* QDBBAN_DEL_SUCCESS */
		"%s has been allowed to submit to the QDB",
		/* QDBBAN_HELP */
		"Syntax: SASET nick QDBBAN {ON|OFF}\n"
		"Sets whether the given nickname will be banned from submitting to the QDB.\n"
		"Set to ON to prevent the nickname posting to the QDB. Set to OFF\n"
		"to allow the nickname to post to the QDB. Default is OFF.\n",
		/* QDBBAN_HELP_CMD */
		" \n"
		"Commands added by ns_qdbban:\n"
		"    QDBBAN         Ban a nickname from posting to the QDB",
		/* QDBBAN_NS_HELP */
		"    QLIST       List QDB bans",
		/* QDBBAN_NS_HELP_QLIST */
		"Syntax: QLIST\n"
		" \n"
		"Lists users who are banned from submitting to the QDB"
	};

	moduleInsertLanguage(LANG_EN_US, LANG_NUM_STRINGS, langtable_en_us);
}

/*************************************************************************/

/**
* Called when user does /ns qlist
**/
int mQList(User * u) {
	NickCore *nc = NULL;
	int count = 0;
	char *data;
	int i;

	for (i = 0; i < 1024; i++) {
		for (nc = nclists[i]; nc; nc = nc->next) {
			/* If we have any info on this user */
			if ((data = moduleGetData(&nc->moduleData, "qdbban"))) {
				if (count == 0) {
					notice_user(s_NickServ, u, "Users who are banned from submitting to the QDB:");
				}

				notice_user(s_NickServ, u, nc->display);

				count++;
				free(data);
			}
		}
	}

	if (count == 0) {
		notice_user(s_NickServ, u, "No users are banned from submitting to the QDB");
	}

	return MOD_CONT;
}

/*************************************************************************/

/**
* Called when user does /ns help
**/
void nsHelp(User * u) {
	if (is_services_oper(u))
		moduleNoticeLang(s_NickServ, u, QDBBAN_NS_HELP);
}

/**
* Called when user does /ns help qlist
**/
int nsHelpQList(User * u) {
	moduleNoticeLang(s_NickServ, u, QDBBAN_NS_HELP_QLIST);

	return MOD_CONT;
}

/**
* Called when user does /ns help saset qdbban
**/
int mQDBBanHelp(User * u)
{
	moduleNoticeLang(s_NickServ, u, QDBBAN_HELP);

	return MOD_CONT;
}

/**
* Called when user does /ns help saset
**/
int mMainSetHelp(User * u)
{
	moduleNoticeLang(s_NickServ, u, QDBBAN_HELP_CMD);

	return MOD_STOP;
}

/*************************************************************************/

/* EOF */
