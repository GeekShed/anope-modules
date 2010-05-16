#include "module.h"
#define AUTHOR "SGR"
#define VERSION "1.26"

  /* -----------------------------------------------------------
   * Name: os_mkill
   * Author: SGR <Alex_SGR@ntlworld.com>
   * Date: 20/07/2003 [abouts]
   * -----------------------------------------------------------
   * Functions: do_killingspree
   * Limitations: Should not kill opers.
   * Tested: Ultimate(2.8.x), Unreal(3.2), Viagra, Bahamut
   * -----------------------------------------------------------
   *
   * For futher help consult the in-IRC help system accessed 
   * by /OperServ MKILL HELP on most networks, after
   * this module has been loaded.
   * 
   * -----------------------------------------------------------
   * 
   *  Change Log:
   *
   *   1: Works.
   *   2: Added Support for Anope-1.7.x (revision 370+)
   *
   * -----------------------------------------------------------
   */


/* ---------------------------------------------------------------------- */
/* START OF CONFIGURATION BLOCK - please read the comments :)             */
/* ---------------------------------------------------------------------- */

/* [OPTIONAL] If you are using Anope-1.7.x or later, leave this alone. 
 * Otherwise change it to '#undef ANOPE17x' (no quotes). 
 *
 * THIS IS IMPORTANT
 *
 */
#define ANOPE17x


/* [OPTIONAL] If you would like to limit number of users that can be
 * killed per use of this command, set the below to a numerical value.
 * 
 * I.e. if this were set to 100, only the 1st 100 matches of a pattern
 * given to MKILL will be killed. 
 * 
 * As an example,  If this were set to 90, and I did an MKILL on 
 * *@*aol.com, only the 1st 90 users matched to *@*aol.com would get 
 * killed. So if our network had 120 AOL users on, 30 would still be 
 * connected after the command had completed.
 *
 * This MUST be numerical.
 */

#define MAX_KILLS_PER_RUN 1024



/* ---------------------------------------------------------------------- */
/* DO NOT EDIT BELOW THIS LINE UNLESS YOU KNOW WHAT YOU ARE DOING         */
/* ---------------------------------------------------------------------- */


int do_killingspree(User * u);
int SGR_Module_Help_OPERSERV_MKILL_FULL(User *u);
void SGR_Module_Help_OPERSERV_MKILL(User *u);
/*
#ifdef ANOPE17x
#define GetHost(u)  common_get_vhost(u)
#endif
*/

int AnopeInit(int argc, char **argv)
{
        Command *c;
        c = createCommand("MKILL",do_killingspree,is_services_admin,-1,-1,-1,-1,-1);                  
	moduleAddHelp(c,SGR_Module_Help_OPERSERV_MKILL_FULL);
        moduleSetOperHelp(SGR_Module_Help_OPERSERV_MKILL);
        alog("Loading module os_mkill.so [Status: %d]",moduleAddCommand(OPERSERV,c,MOD_HEAD));
        alog("[os_mkill] New command: /msg %s MKILL", s_OperServ);
        alog("[os_mkill] For information see: /msg %s MKILL HELP", s_OperServ);
        alog("[os_mkill] Yayness!(tm) - MODULE LOADED AND ACTIVE");
	moduleAddAuthor(AUTHOR);
        moduleAddVersion(VERSION);
        return MOD_CONT;
}

void AnopeFini(void) {
        alog("Unloading os_mkill.so");
}

void SGR_Module_Help_OPERSERV_MKILL(User *u)
{
   if (is_services_admin(u)) {
       notice(s_OperServ, u->nick, "    MKILL       Mass-Kill based on chan or *!*@* host");
   }
   return;
}


int SGR_Module_Help_OPERSERV_MKILL_FULL(User *u)
{
    if (is_oper(u)) {
        notice(s_OperServ, u->nick, "-----------------------------------------------------------------------");
        notice(s_OperServ, u->nick, " Syntax: MKILL [Nick!Ident@Host | #Channel] [Reason]");
        notice(s_OperServ, u->nick, "   ");
        notice(s_OperServ, u->nick, " Allows opers to forcefully disconnect mutiple users via %s.", s_OperServ);
        notice(s_OperServ, u->nick, " A reason is not mandatory, however it is often useful for you put a");
        notice(s_OperServ, u->nick, " valid one. ");
        notice(s_OperServ, u->nick, " ");
  #ifdef MAX_KILLS_PER_RUN
        notice(s_OperServ, u->nick, " Note: only the first %d users matching the pattern will be killed", MAX_KILLS_PER_RUN);
        notice(s_OperServ, u->nick, " if a non-channel pattern is given");
  #endif
        notice(s_OperServ, u->nick, " ");
        notice(s_OperServ, u->nick, " This command will NOT kill any opered clients");
        notice(s_OperServ, u->nick, "-----------------------------------------------------------------------");
        return MOD_STOP;
   }
   return MOD_STOP;
}

int do_killingspree(User * u)
{
    char *pattern = strtok(NULL, " ");
    char *givenreason = strtok(NULL, "");
    int killed = 0; 
    Channel *c;

    if (!pattern) {
    /* lets not Kill everyone on the net 'eh ;) */
        notice(s_OperServ, u->nick, " ERROR: No host or channel specified.");
        notice(s_OperServ, u->nick, "Syntax: MKILL [Nick!Ident@Host | #Channel] [Reason]");
        return MOD_STOP;
    }
    

    if (givenreason) {
       if (strlen(givenreason) > 400) {
          /* lets make sure we don't send too much data */
          notice(s_OperServ, u->nick, "Kill message too long.");
          return MOD_STOP;
       }  
    }
    if (!(givenreason)) {
        givenreason = "NO REASON";
    }
 
    if (pattern && (c = findchan(pattern))) {
        struct c_userlist *cu, *next;
        notice(s_OperServ, u->nick, "Mass Killing all clients defined in range.");
        for (cu = c->users; cu; cu = next) {
             next = cu->next; 
             if (!is_oper(cu->user)) {
              /* sprintf(givenreason, "%s [%s]", givenreason, u->nick); */
              /* snprintf(reason, sizeof(reason + NICKMAX + 16 ), "MASS KILLED: %s [%s]", givenreason, u->nick); */
                kill_user(s_OperServ, cu->user->nick, givenreason);
		killed++;
             }
        }
        notice(s_OperServ, u->nick, "Killed %d matches", killed); 
        return MOD_STOP;
    }
    else 
    {
        /* Lets do some sanity checks on that host */
    char mask[BUFSIZE];
    int i;
    User *u2;
    User *next;
       if (strlen(pattern) < 5) {
          notice(s_OperServ, u->nick, "ERROR: Mask must be in the form Nick!Ident@Host");
          return MOD_CONT;
       } 
       if ( (!(strstr(pattern,"!"))) || (!(strstr(pattern,"@")))) {
          notice(s_OperServ, u->nick, "ERROR: Mask must be in the form Nick!Ident@Host");
          return MOD_CONT;
       } 
       if ((stricmp(pattern,"*") == 0) || (stricmp(pattern,"*!*@*") == 0)) {
          notice_lang(s_OperServ, u, USERHOST_MASK_TOO_WIDE, pattern);
          return MOD_CONT;
       } 
       if (pattern && strspn(pattern, "!*@*?") > strlen(pattern)) {
          notice_lang(s_OperServ, u, USERHOST_MASK_TOO_WIDE, pattern);
          return MOD_CONT;
       }
    #ifndef MAX_KILLS_PER_RUN
    #define MAX_KILLS_PER_RUN usercnt
    #endif
       for (i = 0; i < MAX_KILLS_PER_RUN; i++) {
            for (u2 = userlist[i]; u2; u2 = next) {
                next = u2->next;
                if (pattern) {
                    snprintf(mask, sizeof(mask), "%s!%s@%s", u2->nick, u2->username, GetHost(u2));
                    if (!match_wild_nocase(pattern, mask))
                        continue;
                     }
                     if (!is_oper(u2)) {
                         kill_user(s_OperServ, u2->nick, givenreason);
		         killed++;
                     }
                }
            }
       }
       notice(s_OperServ, u->nick, "Killed %d matches", killed);
       return MOD_CONT;
}
