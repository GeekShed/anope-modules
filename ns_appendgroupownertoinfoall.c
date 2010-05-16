#include "module.h"
#define AUTHOR "SGR" 
#define VERSION "1.00"

  /* ----------------------------------------------------------- 
   * Name: ns_appendgroupownertoinfoall
   * Author: SGR <Alex_SGR@ntlworld.com> 
   * Date: 12/11/2003 
   * ----------------------------------------------------------- 
   * Functions: m_ns_append_groupowner.
   * Limitations: only 10 can be checked at a time.
   * Tested: Ultimate(2.8.x), Unreal(3.2), Bahamut
   * ----------------------------------------------------------- 
   * This version has been tested on Ultimate2.8.6, Unreal and 
   * Bahamut. All IRCd's should be compatible with this module. 
   * 
   * There are 0 configurabe options in this module.
   * 
   * This module will cause the "group owner" to be displayed
   * to a nick owner or services admin when a /NickServ INFO 
   * <nick> ALL command is issued.
   *
   * ----------------------------------------------------------- 
   *  ChangeLog
   *
   *  1) Works
   * -----------------------------------------------------------*/ 
    
/* ---------------------------------------------------------------------- */ 
/* START OF CONFIGURATION BLOCK - please read the comments :)             */ 
/* ---------------------------------------------------------------------- */ 


/* ---------------------------------------------------------------------- */
/* DO NOT EDIT BELOW THIS LINE UNLESS YOU KNOW WHAT YOU ARE DOING         */
/* ---------------------------------------------------------------------- */

int m_ns_append_groupowner(User * u);

int AnopeInit(int argc, char **argv)
{ 
   Command *c; 
   alog("Loading module ns_appendgrouptoinfoall.so"); 
   c = createCommand("INFO", m_ns_append_groupowner, NULL,-1,-1,-1,-1,-1); 
   moduleAddCommand(NICKSERV, c, MOD_TAIL);
   alog("[ns_appendgrouptoinfoall] Command UPDATED: /msg %s INFO <nick> ALL", s_NickServ); 
   alog("[ns_appendgrouptoinfoall] Yayness!(tm) - MODULE LOADED AND ACTIVE"); 
   return MOD_CONT;
} 

int m_ns_append_groupowner(User * u)
{
    char *nick = strtok(moduleGetLastBuffer(), " ");
    char *all = strtok(NULL, " ");
    NickAlias *na;

    if (!nick || !all) {
        // Exit if stuff is missing
        return MOD_CONT;
    }
    if (!(stricmp("all", all) == 0)) { 
        // Exit if the ALL parameter was not given.
        return MOD_CONT;
    }
    if (!is_services_admin(u) || !nick_identified(u)) { 
        // Exit if not an SA or the Nick owner.
        return MOD_CONT;
    }
    if ((!nick ? !(na = u->na) : !(na = findnick(nick)))) {
         // Exit if no nick provided or if we can't find a
	 // registered nick matching the one provided.
         return MOD_CONT;
    } 
    if (na->status & NS_VERBOTEN) {
        // Exit if the nick is forbidden.
        return MOD_CONT;
    }
    else {
          if (na->nc->aliases.count < 2) { 
              notice(s_NickServ, u->nick, "      Group Owner: Ungrouped (Singular)");
          }
	  else {
                notice(s_NickServ, u->nick, "      Group Owner: %s", na->nc->display);
          }
          return MOD_CONT;
    }
}
