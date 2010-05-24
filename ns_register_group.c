/***************************************************************************************/
/* Anope Module : ns_register_group.c : v1.x                                           */
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
#include <assert.h>

#define AUTHOR "Phil"
#define VERSION "1.0"

/* Language Stuff */
#define LANG_NUM_STRINGS   2

#define REGGROUP_REGISTER_ERROR  0
#define REGGROUP_GROUP_ERROR  1

/*************************************************************************/

int myDoRegister(User *u);
int myDoGroup(User *u);

void m_AddLanguages(void);

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

	c = createCommand("REGISTER", myDoRegister, NULL, -1, -1, -1, -1, -1);
	status = moduleAddCommand(NICKSERV, c, MOD_HEAD);
	
	c = createCommand("GROUP", myDoGroup, NULL, -1, -1, -1, -1, -1);
	status = moduleAddCommand(NICKSERV, c, MOD_HEAD);

	m_AddLanguages();

	return MOD_CONT;
}

/**
* Unload the module
**/
void AnopeFini(void) {}

/*************************************************************************/

/* Called when someone does /ns register */
int myDoRegister(User *u)
{
	char *text;
	char *pass;
	char *email;
	char *override;
	NickCore *nc;
	int stop = 0;
	int i = 0;
	
	text = moduleGetLastBuffer();
		
	if (text) {
		pass = myStrGetToken(text, ' ', 0);
	
		if (pass) {
			email = myStrGetToken(text, ' ', 1);
			
			if (email) {
				if (!findnick(u->nick)) {
					override = myStrGetToken(text, ' ', 2);
					
					if ((!override) || (stricmp(override, "YES"))) {					
						while ((stop == 0) && (i < 1024)) {
							for (nc = nclists[i]; nc; nc = nc->next) {
								if (nc->email) {
									if (!stricmp(email, nc->email)) {
										moduleNoticeLang(s_NickServ, u, REGGROUP_REGISTER_ERROR, email);
										alog("ns_register_group: %s!%s@%s was warned about multiple nickname groups", u->nick, u->username, u->host);
										// Signify a break from the while loop
										stop = 1;
										// Signify a break from the for loop
										break;
									}
								}
							}
							
							i++;
						}
					}
					else {
						alog("ns_register_group: %s!%s@%s ignored warning about multiple nickname groups", u->nick, u->username, u->host);
					}
					
					if (override)
						free(override);
				}				
				free(email);
			}			
			free(pass);
		}
	}

	if (stop == 0) {
		return MOD_CONT;
	}
	else {
		return MOD_STOP;
	}
}

/* Called when someone does /ns group */
int myDoGroup(User *u)
{
	char *text;
	char *nick;
	char *pass;
	int stop = 0;

	text = moduleGetLastBuffer();
		
	if (text) {
		nick = myStrGetToken(text, ' ', 0);
	
		if (nick) {
			pass = myStrGetToken(text, ' ', 1);
			
			if (pass) {		
				if (findnick(u->nick)) {
					moduleNoticeLang(s_NickServ, u, REGGROUP_GROUP_ERROR, nick, u->nick);
					stop = 1;
				}				
				free(pass);
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

/* Language stuffs */
void m_AddLanguages(void)
{
	char *langtable_en_us[] = {
		/* REGGROUP_REGISTER_ERROR */
		"The e-mail address you used is already in use on another nickname. This\n"
		"is not a problem however most users do not require more than one nickname\n"
		"group. Please read http://www.geekshed.net/grouping to learn how to group\n"
		"your nicknames and the reasons for doing so. If you are sure that you need\n"
		"multiple nickname groups, suffix the register command with YES. For example:\n"
		" \n"
		"/ns register yourpassword %s YES",
		/* REGGROUP_GROUP_ERROR */
		"The nickname you are currently using is registered. Grouping it with\n"
		"another nickname is more than likely not what you want as it will\n"
		"drop your nickname removing you from channel access lists. Instead\n"
		"you should change to the nickname you wish to group and do the group\n"
		"command again. Here is an example:\n"
		" \n"
		"/nick %s\n"
		"/ns group %s yourpassword"
	};
	
	char *langtable_de[] = {
		/* REGGROUP_REGISTER_ERROR */
		"Die von Ihnen benutzte Email-Adresse ist bereits benutzt für ein anderes\n"
		"Pseudonym (\"nickname\"). An sich ist das kein Problem, aber die meisten\n"
		"Benutzer brauchen nur eine \"nickname\"-Gruppe.  Bitte lesen Sie:\n"
		"http://www.geekshed.net/grouping für weitere Informationen wie Sie Ihre \"nicknames\"\n"
		"grupieren können und die daran verbundene Vorteile.  Falls Sie sicher sind dass Sie\n"
		"mehrfache \"nickname\"-Gruppen brauchen, markieren Sie bitte den Register-Auftrag mit\n"
		"\"YES\".  Zum beispiel:\n"
		" \n"
		"/ns register yourpassword %s YES",
		/* REGGROUP_GROUP_ERROR */
		"Das Pseudonym (\"nickname\") dass Sie gerade benutzen ist bereits registriert.  Es ist sehr\n"
		"unwahrscheinlich dass Sie dieser \"nickname\" grupieren möchten mit ein anderer \"nickname\".\n"
		"Diese Aktion würde Ihr \"nickname\" eliminieren und Ihnen entfernen von die Zugangsdateien der\n"
		"\"channels\". Sie sollten ändern zur \"nickname\" der Sie grupieren möchten und dann die\n"
		"Group-Auftrag wiederholen. Zum beispiel:\n"
		" \n"
		"/nick %s\n"
		"/ns group %s yourpassword"
	};
	
	char *langtable_fr[] = {
		/* REGGROUP_REGISTER_ERROR */
		"L'adresse email que vous avez utilisé est déjà pris par un autre alias (\"nickname\").\n"
		"Ceci n'est pas un problème, mais la majorité des utilisateurs n'a besoin que d'un groupe\n"
		"\"nickname\".  Veuillez lire http://www.geekshed.net/grouping pour apprendre comment grouper\n"
		"vos \"nicknames\"  et les raisons pour faire ceci.  Si quand-même vous êtes sûr que vous avez\n"
		"besoin d'utiliser plusiers groupes \"nicknames\", il faut spécifier la commande du régistre avec\n"
		"YES.  Par example:\n"
		" \n"
		"/ns register yourpassword %s YES",
		/* REGGROUP_GROUP_ERROR */
		"L'alias (\"nickname\") que vous utilisez actuellement est déjà enrégistré.  Il est\n"
		"très peu probable que vous voulez grouper ce \"nickname\" avec un autre \"nickname\",\n"
		"car ceci enlèvera votre \"nickname\" ce qui vous élimine des listes d'accès des chaînes.\n"
		"Pour éviter, vous devriez changer vers le \"nickname\" que vous voulez grouper et\n"
		"recommencer la commande de grouper.  Par example:\n"
		" \n"
		"/nick %s\n"
		"/ns group %s yourpassword"
	};
	
	char *langtable_nl[] = {
		/* REGGROUP_REGISTER_ERROR */
		"Het email adres dat U gebruikte, wordt al gebruikt voor een andere naam (nicknaam).\n"
		"Dit is op zich geen probleem, maar de meeste gebruikers hebben niet meerdere nicknaam\n"
		"groepen nodig. Meer informatie over het waarom en hoe de nicknamen te groeperen, is\n"
		"hier te vinden-> http://www.geekshed.net/grouping Indien meerdere nicknaam groepen toch\n"
		"gewenst zijn, bevestig dan de registratie opdracht met \"YES\". Bijvoorbeeld:\n"
		" \n"
		"/ns register yourpassword %s YES",
		/* REGGROUP_GROUP_ERROR */
		"Deze nicknaam is al geregistreerd. Het groeperen hiervan met een andere nicknaam is\n"
		"waarschijnlijk ongewenst omdat dit de naam zal laten vervallen evenals de vermelding\n"
		"op de toegangslijsten (accesslists) van de \"channels\". In plaats daarvan is het beter\n"
		"de te groeperen nicknaam te veranderen en de groeperings opdracht te herhalen. Hier is\n"
		"een voorbeeld:\n"
		" \n"
		"/nick %s\n"
		"/ns group %s yourpassword"
	};

	moduleInsertLanguage(LANG_EN_US, LANG_NUM_STRINGS, langtable_en_us);
	moduleInsertLanguage(LANG_DE, LANG_NUM_STRINGS, langtable_de);
	moduleInsertLanguage(LANG_FR, LANG_NUM_STRINGS, langtable_fr);
	moduleInsertLanguage(LANG_NL, LANG_NUM_STRINGS, langtable_nl);
}

/* EOF */