/**
 * -----------------------------------------------------------------------------
 * Name    : cs_sync
 * Author  : Viper <Viper@Absurd-IRC.net>
 * Date    : 02/02/2008 (Last update: 08/02/2008)
 * Version : 1.1
 * -----------------------------------------------------------------------------
 * Requires    : Anope-1.7.21
 * Tested      : Anope 1.7.21 + UnrealIRCd 3.2.6
 * -----------------------------------------------------------------------------
 * This module will synchronize a channels' userlist with the channels accesslist
 * giving/taking user privileges according to the access list.
 *
 * The SYNC command is restricted to channel founders.
 * -----------------------------------------------------------------------------
 *
 * Changelog:
 *
 *   1.1  -  Fixed a few minor typos in the different languages.
 *
 *   1.0  -  Initial Release
 *
 * -----------------------------------------------------------------------------
 **/


#include "module.h"
#define AUTHOR "Viper"
#define VERSION "1.1"


/* Language defines */
#define LANG_NUM_STRINGS 					5

#define LANG_SYNC_DESC						0
#define LANG_SYNC_SYNTAX					1
#define LANG_SYNC_SYNTAX_EXT				2
#define LANG_SYNC_NOCHAN					3
#define LANG_SYNC_DONE						4

/* Functions */
void do_help_list(User *u);
int do_help(User *u);
int do_sync(User *c);
void add_languages(void);

/* ------------------------------------------------------------------------------- */

/**
 * Create the command, and tell anope about it.
 * @param argc Argument count
 * @param argv Argument list
 * @return MOD_CONT to allow the module, MOD_STOP to stop it
 **/
int AnopeInit(int argc, char **argv) {
	Command *c;

	alog("[\002cs_sync\002] Loading module...");

	if (!moduleMinVersion(1,7,21,1341)) {
		alog("[\002cs_sync\002] Your version of Anope isn't supported. Please update to a newer release.");
		return MOD_STOP;
	}

	/* Create AJOIN command.. */
	c = createCommand("SYNC", do_sync, NULL, -1, -1, -1, -1, -1);
	if (moduleAddCommand(CHANSERV,c,MOD_HEAD) != MOD_ERR_OK) {
		alog("[\002cs_sync\002] Cannot create SYNC command...");
		return MOD_STOP;
	}
	moduleAddHelp(c,do_help);
	moduleSetChanHelp(do_help_list);

	add_languages();

	moduleAddAuthor(AUTHOR);
	moduleAddVersion(VERSION);

	alog("[\002cs_sync\002] Module loaded successfully...");

	return MOD_CONT;
}


/**
 * Unload the module
 **/
void AnopeFini(void) {
	alog("[\002cs_sync\002] Unloading module...");
}

/* ------------------------------------------------------------------------------- */

/**
 * Add the AJOIN command to the NickServ HELP listing.
 **/
void do_help_list(User *u) {
	moduleNoticeLang(s_ChanServ, u, LANG_SYNC_DESC);
}


/**
 * Show the extended help on the AJOIN command.
 **/
int do_help(User *u) {
	moduleNoticeLang(s_ChanServ, u, LANG_SYNC_SYNTAX_EXT);
	return MOD_CONT;
}

/* ------------------------------------------------------------------------------- */

int countModes(char *str) {
	int i, len;
	int count = 0;

	len = strlen(str);

	for (i = 0; i < len; i++) {
		if (str[i] != '+' && str[i] != '-') {
			count++;
		}
	}

	return count;
}

void chan_get_correct_modes(User * user, Channel * c, int give_modes, char *modebuf, char *userbuf) {
    char *tmp;
    int status;
    int add_modes = 0;
    int rem_modes = 0;
    ChannelInfo *ci;

    if (!c || !(ci = c->ci))
        return;

    if ((ci->flags & CI_VERBOTEN) || (*(c->name) == '+'))
        return;

    status = chan_get_user_status(c, user);

    if (debug)
        alog("debug: Setting correct user modes for %s on %s (current status: %d, %sgiving modes)", user->nick, c->name, status, (give_modes ? "" : "not "));

    /* Changed the second line of this if a bit, to make sure unregistered
     * users can always get modes (IE: they always have autoop enabled). Before
     * this change, you were required to have a registered nick to be able
     * to receive modes. I wonder who added that... *looks at Rob* ;) -GD
     */
    if (give_modes && (get_ignore(user->nick) == NULL)
        && (!user->na || !(user->na->nc->flags & NI_AUTOOP))) {
        if (ircd->owner && is_founder(user, ci))
            add_modes |= CUS_OWNER;
        else if ((ircd->protect || ircd->admin)
                 && check_access(user, ci, CA_AUTOPROTECT))
            add_modes |= CUS_PROTECT;
        if (check_access(user, ci, CA_AUTOOP))
            add_modes |= CUS_OP;
        else if (ircd->halfop && check_access(user, ci, CA_AUTOHALFOP))
            add_modes |= CUS_HALFOP;
        else if (check_access(user, ci, CA_AUTOVOICE))
            add_modes |= CUS_VOICE;
    }

    /* We check if every mode they have is legally acquired here, and remove
     * the modes that they're not allowed to have. But only if SECUREOPS is
     * on, because else every mode is legal. -GD
     * Unless the channel has just been created. -heinz
     *     Or the user matches CA_AUTODEOP... -GD
     */
    if (((ci->flags & CI_SECUREOPS) || (c->usercount == 1)
         || check_access(user, ci, CA_AUTODEOP))
        && !is_ulined(user->server->name)) {
        if (ircd->owner && (status & CUS_OWNER) && !is_founder(user, ci))
            rem_modes |= CUS_OWNER;
        if ((ircd->protect || ircd->admin) && (status & CUS_PROTECT)
            && !check_access(user, ci, CA_AUTOPROTECT)
            && !check_access(user, ci, CA_PROTECTME))
            rem_modes |= CUS_PROTECT;
        if ((status & CUS_OP) && !check_access(user, ci, CA_AUTOOP)
            && !check_access(user, ci, CA_OPDEOPME))
            rem_modes |= CUS_OP;
        if (ircd->halfop && (status & CUS_HALFOP)
            && !check_access(user, ci, CA_AUTOHALFOP)
            && !check_access(user, ci, CA_HALFOPME))
            rem_modes |= CUS_HALFOP;
    }

    /* No modes to add or remove, exit function -GD */
    if (!add_modes && !rem_modes)
        return;

    /* No need for strn* functions for modebuf, as every possible string
     * will always fit in. -GD
     */
    strcpy(modebuf, "");
    strcpy(userbuf, "");
    if (add_modes > 0) {
        strcat(modebuf, "+");
        if ((add_modes & CUS_OWNER) && !(status & CUS_OWNER)) {
            tmp = stripModePrefix(ircd->ownerset);
            strcat(modebuf, tmp);
            free(tmp);
            strcat(userbuf, " ");
            strcat(userbuf, GET_USER(user));
        } else {
            add_modes &= ~CUS_OWNER;
        }
        if ((add_modes & CUS_PROTECT) && !(status & CUS_PROTECT)) {
            tmp = stripModePrefix(ircd->adminset);
            strcat(modebuf, tmp);
            free(tmp);
            strcat(userbuf, " ");
            strcat(userbuf, GET_USER(user));
        } else {
            add_modes &= ~CUS_PROTECT;
        }
        if ((add_modes & CUS_OP) && !(status & CUS_OP)) {
            strcat(modebuf, "o");
            strcat(userbuf, " ");
            strcat(userbuf, GET_USER(user));
            rem_modes |= CUS_DEOPPED;
        } else {
            add_modes &= ~CUS_OP;
        }
        if ((add_modes & CUS_HALFOP) && !(status & CUS_HALFOP)) {
            strcat(modebuf, "h");
            strcat(userbuf, " ");
            strcat(userbuf, GET_USER(user));
            /* Halfops are ops too, having a halfop with CUS_DEOPPED is not good - Adam */
            rem_modes |= CUS_DEOPPED;
        } else {
            add_modes &= ~CUS_HALFOP;
        }
        if ((add_modes & CUS_VOICE) && !(status & CUS_VOICE)) {
            strcat(modebuf, "v");
            strcat(userbuf, " ");
            strcat(userbuf, GET_USER(user));
        } else {
            add_modes &= ~CUS_VOICE;
        }
    }
    if (rem_modes > 0) {
        strcat(modebuf, "-");
        if (rem_modes & CUS_OWNER) {
            tmp = stripModePrefix(ircd->ownerset);
            strcat(modebuf, tmp);
            free(tmp);
            strcat(userbuf, " ");
            strcat(userbuf, GET_USER(user));
        }
        if (rem_modes & CUS_PROTECT) {
            tmp = stripModePrefix(ircd->adminset);
            strcat(modebuf, tmp);
            free(tmp);
            strcat(userbuf, " ");
            strcat(userbuf, GET_USER(user));
        }
        if (rem_modes & CUS_OP) {
            strcat(modebuf, "o");
            strcat(userbuf, " ");
            strcat(userbuf, GET_USER(user));
            /* Do not mark a user as deopped if they are halfopd - Adam */
            if (!(add_modes & CUS_HALFOP) && !(status & CUS_HALFOP))
                add_modes |= CUS_DEOPPED;
        }
        if (rem_modes & CUS_HALFOP) {
            strcat(modebuf, "h");
            strcat(userbuf, " ");
            strcat(userbuf, GET_USER(user));
            /* Do not mark a user as deopped if they are opped - Adam */
            if (!(add_modes & CUS_OP) && !(status & CUS_OP))
                add_modes |= CUS_DEOPPED;
        }
    }

    /* Here, both can be empty again due to the "isn't it set already?"
     * checks above. -GD
     */
    if (!add_modes && !rem_modes)
        return;

    if (add_modes > 0)
        chan_set_user_status(c, user, add_modes);
    if (rem_modes > 0)
        chan_remove_user_status(c, user, rem_modes);
}

/* ------------------------------------------------------------------------------- */

int do_sync(User *u) {
	Channel *c;
	ChannelInfo *ci;
	char *buffer, *chan;
	char userbuff[BUFSIZE];
	char finalub[BUFSIZE];
	char modebuff[BUFSIZE];
	char finalmb[BUFSIZE];

	finalmb[0] = '\0';
	finalub[0] = '\0';

	buffer = moduleGetLastBuffer();
	chan = myStrGetToken(buffer, ' ', 0);

	if (!chan) {
		moduleNoticeLang(s_ChanServ, u, LANG_SYNC_NOCHAN);
		moduleNoticeLang(s_ChanServ, u, LANG_SYNC_SYNTAX);
		return MOD_CONT;
	}

	if (!(c = findchan(chan))) {
		notice_lang(s_ChanServ, u, CHAN_X_NOT_IN_USE, chan);
	} else if (!(ci = c->ci)) {
		notice_lang(s_ChanServ, u, CHAN_X_NOT_REGISTERED, chan);
	} else if (ci->flags & CI_VERBOTEN) {
		notice_lang(s_ChanServ, u, CHAN_X_FORBIDDEN, chan);
	} else if (ci->flags & CI_SUSPENDED) {
		notice_lang(s_ChanServ, u, CHAN_X_FORBIDDEN, chan);
	} else if ((!u || !check_access(u, ci, CA_OPDEOP)) && !is_services_admin(u)) {
		notice_lang(s_ChanServ, u, ACCESS_DENIED);
	} else {
		struct c_userlist *cu, *next;

		cu = c->users;
		while (cu) {
			uint32 flags = 0;

			/* We store the next user just in case current user ends up getting kicked off the channel. */
			next = cu->next;

			if (cu->user->na && cu->user->na->nc) {
				flags = cu->user->na->nc->flags;
				cu->user->na->nc->flags &= ~NI_AUTOOP;
			}

			userbuff[0] = '\0';
			modebuff[0] = '\0';

			chan_get_correct_modes(cu->user, c, 1, modebuff, userbuff);

			if ((strlen(finalmb) + strlen(finalub) + strlen(modebuff) + strlen(userbuff) + strlen(c->name) + 10) > BUFSIZE) {
				anope_cmd_mode(whosends(ci), c->name, "%s%s", finalmb, finalub);
				finalmb[0] = '\0';
				finalub[0] = '\0';
			}
			else if (countModes(finalmb) + countModes(modebuff) > 12) {
				anope_cmd_mode(whosends(ci), c->name, "%s%s", finalmb, finalub);
				finalmb[0] = '\0';
				finalub[0] = '\0';
			}

			strcat(finalmb, modebuff);
			strcat(finalub, userbuff);

			if (cu->user->na && cu->user->na->nc)
				cu->user->na->nc->flags = flags;

			cu = next;
		}

		if (countModes(finalmb) != 0) {
			anope_cmd_mode(whosends(ci), c->name, "%s%s", finalmb, finalub);
		}

		moduleNoticeLang(s_ChanServ, u, LANG_SYNC_DONE, ci->name);
	}

	free(chan);
	return MOD_CONT;
}

/* ------------------------------------------------------------------------------- */

void add_languages(void) {
	char *langtable_en_us[] = {
		/* LANG_SYNC_DESC */
		"    SYNC       Give all users the status the access list grants them.",
		/* LANG_SYNC_SYNTAX */
		" Syntax: SYNC [\037channel\037]",
		/* LANG_SYNC_SYNTAX_EXT */
		" Syntax: \002SYNC \037channel\037\002\n"
		" \n"
		" This command will give all users currently in the channel the level\n"
		" they are granted by the channels' access list. Users who have a level\n"
		" greater then the one they are supposed to have will be demoted.\n"
		" \n"
		" The use of this command is restricted to people with OPDEOP permissions.",
		/* LANG_SYNC_NOCHAN */
		" No channel specified.",
		/* LANG_SYNC_DONE */
		" Synchronized userlist with channel accesslist for %s.",
	};

	char *langtable_nl[] = {
		/* LANG_SYNC_DESC */
		" SYNC       Geef alle gebruikers de status die de access list hen toestaat.",
		/* LANG_SYNC_SYNTAX */
		" Gebruik: SYNC [\037kanaal\037]",
		/* LANG_SYNC_SYNTAX_EXT */
		" Gebruik: \002SYNC \037kanaal\037\002\n"
		" \n"
		" Dit commando geeft alle gebruikers die zich in het kanaal bevinden het niveau\n"
		" waartoe ze volgens de access list van het kanaal toegang hebben.\n"
		" Gebruikers die een niveau hebben dat hoger is dan hetgeen zo toegang hebben worden\n"
		" gedegradeerd tot het niveau waartoe ze recht hebben.\n"
		" \n"
		" Het gebruik van dit commando is gelimiteerd tot de Kanaal founder/stichter.",
		/* LANG_SYNC_NOCHAN */
		" Geen kanaal opgegeven.",
		/* LANG_SYNC_DONE */
		"Gebruikerslijst met toegangslijst gesynchroniseerd voor %s.",
	};

	/* Russian translation provided by Kein */
	char *langtable_ru[] = {
		/* LANG_SYNC_DESC */
		"    SYNC       ¬ыставл€ет статусы прописанным пользовател€м канала.",
		/* LANG_SYNC_SYNTAX */
		" —интаксис: SYNC [\037#канал\037]",
		/* LANG_SYNC_SYNTAX_EXT */
		" —интаксис: \002SYNC \037#канал\037\002\n"
		" \n"
		" »спользование данной команды позвол€ет автоматически выдать все статусы\n"
		" тем пользовател€м канала, которые присутствуют в его списке доступа.\n"
		" ѕосетители, текущий канальный статус которых превышает прописанный им\n"
		" в списке доступа канала, будут понижены в полномочи€х.\n"
		" \n"
		" ƒоступ к данной команде ограничен до владельца канала.",
		/* LANG_SYNC_NOCHAN */
		" ¬ы не указали канал.",
		/* LANG_SYNC_DONE */
		" —татусы пользователей канала %s успешно синхронизированы со списком доступа.",
	};

	moduleInsertLanguage(LANG_EN_US, LANG_NUM_STRINGS, langtable_en_us);
	moduleInsertLanguage(LANG_NL, LANG_NUM_STRINGS, langtable_nl);
	moduleInsertLanguage(LANG_RU, LANG_NUM_STRINGS, langtable_ru);
}

/* EOF */
