#include "module.h"
#define AUTHOR "SGR"
#define VERSION "1.01"

  /* -----------------------------------------------------------
   * Name: os_qakill
   * Author: SGR <Alex_SGR@ntlworld.com>
   * Date: 04/10/2003
   * -----------------------------------------------------------
   * Functions: do_qakill_some_lameass(User * u);
   * Limitations: optional expiry not yet possible
   * Tested: Ultimate(2.8.x), Unreal(3.2), Viagra
   * -----------------------------------------------------------
   *
   *  This modules has 1 configurable option: DISABLE_LOWER_KILL
   *  if this IS defined then services admins will not be able 
   *  to use the command on the nicks of other identified 
   *  services admins or roots.
   *
   * Thanks to dengel, Rob and Certus for all there support. 
   * 
   * -----------------------------------------------------------
   * Changes: 
   *
   *    1: Added Anope-1.7.x support [370 and later]
   *    
   * -----------------------------------------------------------
   */

/* ---------------------------------------------------------------------- */
/* START OF CONFIGURATION BLOCK - please read the comments :)             */
/* ---------------------------------------------------------------------- */

/* If you are using Anope-1.7.x or later, leave this alone. 
 * Otherwise change it to '#undef ANOPE17x' (no quotes). 
 *
 * THIS IS IMPORTANT
 *
 */
#define ANOPE17x

/*  DISABLE_LOWER_KILL -  if this IS defined then services admins will
 *  not be able to use the command on the nicks of other identified 
 *  services admins or roots. 
 */
   
#define DISABLE_LOWER_QAKILL



/* ---------------------------------------------------------------------- */
/* DO NOT EDIT BELOW THIS LINE UNLESS YOU KNOW WHAT YOU ARE DOING         */
/* ---------------------------------------------------------------------- */

static int do_qakill_some_lameass(User * u);
int SGR_Module_Help_OPERSERV_QAKILL_FULL(User *u);
void SGR_Module_Help_OPERSERV_QAKILL(User *u);

int AnopeInit(int argc, char **argv) {
        Command *c;
        c = createCommand("QAKILL", do_qakill_some_lameass, is_services_admin,-1,-1,-1,-1,-1);
        alog("Loading module os_qakill.so [Status: %d]", moduleAddCommand(OPERSERV,c,MOD_HEAD));
	moduleAddHelp(c,SGR_Module_Help_OPERSERV_QAKILL_FULL);
        moduleSetOperHelp(SGR_Module_Help_OPERSERV_QAKILL);
        alog("[os_qakill] New command: /msg operserv QAKILL [nick] [reason]");
        alog("[os_qakill] For information see: /msg operserv HELP QAKILL");
        alog("[os_qakill] Yayness!(tm) - MODULE LOADED AND ACTIVE");
        moduleAddAuthor(AUTHOR);
        moduleAddVersion(VERSION);
        return MOD_CONT; 
}

void AnopeFini(void) {
        alog("Unloading os_qakill.so");
}

static int do_qakill_some_lameass(User * u)
{
    char *to_be_akilled, *reason;
    char reasonx[512];
    char mask[USERMAX + HOSTMAX + 2];
    User *target;

    to_be_akilled = strtok(NULL, " ");
    reason = strtok(NULL, "");

    if (!is_services_admin(u)) { 
        notice(s_OperServ, u->nick, "Access Denied");
	return MOD_STOP;
    }
    if (to_be_akilled) {
       if (!reason) {
           reason = "You have been AKILLED";
       }
       if (AddAkiller) {
           snprintf(reasonx, sizeof(reasonx), "[%s] %s", u->nick, reason);
       }
       if ((target = finduser(to_be_akilled))) {
            sprintf(mask, "*@%s", target->host);
            #ifdef DISABLE_LOWER_QAKILL
            if ((is_services_admin(target)) && (!is_services_root(u))) {
               notice(s_OperServ, u->nick, "Permission Denied");
               #ifndef ANOPE17x
	       wallops(s_OperServ, "%s attempted to QAKILL %s (%s)", u->nick, target->nick, reasonx);
               #else
	       anope_cmd_global(s_OperServ, "%s attempted to QAKILL %s (%s)", u->nick, target->nick, reasonx);
               #endif
               return MOD_STOP;
	    }
            #endif
            add_akill(u, mask, u->nick, time(NULL)+AutokillExpiry, reason);
            if (WallOSAkill) {
                #ifndef ANOPE17x
                wallops(s_OperServ, "%s used QAKILL on %s (%s)", u->nick, target->nick, reasonx);
                #else
                anope_cmd_global(s_OperServ, "%s used QAKILL on %s (%s)", u->nick, target->nick, reasonx);
                #endif
            }
	    if (!AkillOnAdd) {
                kill_user(s_OperServ, target->nick, reasonx);
	    }
       }
       else {
            notice_lang(s_OperServ, u, NICK_X_NOT_IN_USE, to_be_akilled);
       }
    } 
    else {
          notice(s_OperServ, u->nick, "See /msg OperServ HELP QAKILL for more info.");
    }
    return MOD_CONT;
}

void SGR_Module_Help_OPERSERV_QAKILL(User *u)
{
   if (is_services_admin(u)) {
       notice(s_OperServ, u->nick, "    QAKILL      AKILL a user by nick.");
   }
   return;
}


int SGR_Module_Help_OPERSERV_QAKILL_FULL(User *u)
{
    if (is_services_admin(u)) {
        notice(s_OperServ, u->nick, "-----------------------------------------------------------------------");
        notice(s_OperServ, u->nick, " Syntax: QAKILL Nick [Reason]");
	notice(s_OperServ, u->nick, "   ");
 	notice(s_OperServ, u->nick, " Allows Services Admins to add an akill with just a nick. A reason");
	notice(s_OperServ, u->nick, " is not mandatory, however it is often useful for you put a valid one.");
        notice(s_OperServ, u->nick, " The akill placed will be in the form *@full.host.mask for the default");
	notice(s_OperServ, u->nick, " AKILL expiry time.");
        #ifdef DISABLE_LOWER_QAKILL
        notice(s_OperServ, u->nick, " You will not be able to QAKILL identifed services admins or services");
        notice(s_OperServ, u->nick, " roots unless you are a services root.");
	#endif
	notice(s_OperServ, u->nick, "-----------------------------------------------------------------------");
	return MOD_CONT;
   }
   return MOD_CONT;
}

