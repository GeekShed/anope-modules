#include "module.h"
#define AUTHOR "SGR"
#define VERSION "1.10"

  /* ----------------------------------------------------------- 
   * Name: ns_limitregonconnect 
   * Author: SGR <Alex_SGR@ntlworld.com> 
   * Date: 06/02/2004 
   * ----------------------------------------------------------- 
   * Functions: oper_only_NICK_reg. 
   * Limitations: NONE KNOWN. 
   * Tested: Ultimate(2.8.x), Unreal(3.2), Viagra, Bahamut.
   * ----------------------------------------------------------- 
   * This version has been tested on Ultimate2.8.6, Viagra 
   * Unreal and Bahamut. All IRCd's should be compatible with 
   * this module. 
   * 
   *  Please set the config options below. 
   * 
   *  To DISABLE the notices comment out the #DEFINE lines. 
   *  #define DENY_NICK_REG_AS_TOO_EARLY1-6 "Notice text". 
   *  i.e 
   *  #define DENY_NICK_REG_AS_TOO_EARLY1 "notice text" becomes 
   */  
        // #define DENY_NICK_REG_AS_TOO_EARLY1 "notice text"  
   /* ----------------------------------------------------------- 
   *  ChangeLog
   *
   *  1) Works
   *  2) I just _had_ to add the DROP option! :D :P   
   *  3) Fixed major bug preventing Nick registratrions
   * -----------------------------------------------------------*/ 
    
/* ---------------------------------------------------------------------- */ 
/* START OF CONFIGURATION BLOCK - please read the comments :)             */ 
/* ---------------------------------------------------------------------- */ 

          /* ----------------------------------------------*/ 
          /* ONLY NICKGE WHAT IS INSIDE THE "speach marks" */
          /* and limit each line to 400 characters.        */ 
          /* ----------------------------------------------*/ 


/* This is the number of seconds users will have to wait before they can 
 * register with NickServ after connecting to the Network. This value
 * should be somewhere between 3 and 900 seconds. 30 is recommended.
 * This setting is MANDATORY. This value should be an integer only.
 */
#define TIME_TO_DENY_NICKREG 15 // 15 secs.

/* If this is defined and a user /nicks's within the TIME_TO_DENY_NICKREG
 * time; the counter for them will be reset and they will have to wait the
 * full 30 seconds. If this not set, a user can simply /nick to bypass the 
 * TIME_TO_DENY_NICKREG lockout on them registering their nick.
 * This setting is MANDATORY. This setting should be an integer value only 
 * 
 * On  = 1
 * Off = 0
 * */
#define USE_NICK_TRACKING    1 

/* These are the noticed that will be sent to a user when they are rejected
 * from registering their Nick with NickServ due to them having not been
 * connected for longer than TIME_TO_DENY_NICKREG. Upto 6 notices can be
 * defined */
#define DENY_NICK_REG_AS_TOO_EARLY1 "You must of been connected for more than 30 seconds before you"  
#define DENY_NICK_REG_AS_TOO_EARLY2 "may REGISTER your nickname with NickServ. If you have changed " 
#define DENY_NICK_REG_AS_TOO_EARLY3 "your nick within 30 seconds the timeout will of been reset." 
#define DENY_NICK_REG_AS_TOO_EARLY4 "If you do not wish to wait, you can lift this temporary block "
#define DENY_NICK_REG_AS_TOO_EARLY5 "using \002/NickServ REGISTER UNBLOCK.\002"
#define DENY_NICK_REG_AS_TOO_EARLY6 "For more information please join #Services-Help"

/* Defining this as will enable users to "force expire" the timeout
 * phrobiting them from registering. This will allow users to issue the
 * command /NickServ REGISTER UNBLOCK. When this is done, the user will
 * be removed from the temporary 'deny list' before the 30 second timeout.
 * And will thus allow them to register early. To UNDEFINE this, add //
 * to the beginning of the line.
 */
#define ALLOW_SELF_REMOVAL 

/* ---------------------------------------------------------------------- 
 * Advanced settings.
 * ---------------------------------------------------------------------- */

/* This setting controls how many 'slots' to reserve for new clients connecting.
 * everytime a client connects, the are added to a free slot, after the timeout
 * they are removed from the slot. When a user tries to register their nick
 * we 1st check if they are in one of the slots, if so, the registration is 
 * rejected. It is NOT recommended you change this setting unless you are
 * running services with a small ammount of ram and on a small [<1000 user] or
 * large [>5000 user] network. [DISCOURAGED] [Must be an integer value]
 * *** MUST NOT BE LOWER THAN 80, ideally keep to a power of 2 *** */

#define NICKBUFFER 1024 // Here we assume that normally, there are no more
                        // than 1024 clients who have connected in the last
			// TIME_TO_DENY_NICKREG secs. Don't worry if there 
			// are sometimes more or less - but feel free to 
			// change this if you are reguraly seeing the
			// "Hash Table Full" notice.

/* ---------------------------------------------------------------------- */
/* DO NOT EDIT BELOW THIS LINE UNLESS YOU KNOW WHAT YOU ARE DOING         */
/* ---------------------------------------------------------------------- */


typedef struct connectnicks CN;

struct connectnicks {
 	CN *next, *prev;
 	char *nick;
};

CN *cnsh[NICKBUFFER];
int ncns    = 0;
int NSRLock = 0;

// I'm expecting to use these functions in other
// modules, so I'm definedin them all as statics.
// since its a module not the core - it shouldn't
// have too much of an impact.
static CN *find_entry(char *nick);
static CN *create_entry(char *nick);
static int del_entry(CN *cn);
static int rtncount(void);
static void add_entry(CN *cn);
static void m_regconnecttimeoutloghash(void);
int my_nickservreglock(char *source, int ac, char **av);
int m_ns_remove_deny_entry(int argc, char **argv);
int m_ns_check_recentconnection(User *u);
int m_ns_someuserquit(char *source, int ac, char **av);
int AnopeInit(int argc, char **argv)
{
   Command *c;
   Message *msg;
   alog("Loading module ns_limitregonconnect.so");
   #if defined(IRC_ULTIMATE3)
   msg = createMessage("CLIENT", my_nickservreglock);
   moduleAddMessage(msg, MOD_TAIL); 
   #endif
   msg = createMessage("NICK", my_nickservreglock);
   moduleAddMessage(msg, MOD_TAIL); 
   msg = createMessage("QUIT", m_ns_someuserquit);
   moduleAddMessage(msg, MOD_TAIL); 

   c = createCommand("REGISTER", m_ns_check_recentconnection, NULL,-1,-1,-1,-1,-1);
   moduleAddCommand(NICKSERV, c, MOD_HEAD); 
   alog("[ns_limitregonconnect] Yayness!(tm) - MODULE LOADED AND ACTIVE");
   return MOD_CONT;
}

int m_ns_check_recentconnection(User *u)
{
   CN *cn;
   #ifdef ALLOW_SELF_REMOVAL
   char *buffer = moduleGetLastBuffer();
   if (buffer) {
       buffer = myStrGetToken(buffer, ' ',0);
       if (!stricmp(buffer,"unblock")) {
           if ((cn = find_entry(u->nick))) {
                del_entry(cn);
                notice(s_NickServ, u->nick, "The timout preventing you from REGISTERing has been removed. You may");
                notice(s_NickServ, u->nick, "now register your nick. See \002/msg %s HELP REGISTER\002", s_NickServ);
                notice(s_NickServ, u->nick, "for further information on how to register.");
	   }
	   else {
                 notice(s_NickServ, u->nick, "No matching timeouts were found. You may now register your nick.");
	   }
	   return MOD_STOP;
       }
   }
   #endif
   if ((cn = find_entry(u->nick))) {
        #ifdef DENY_NICK_REG_AS_TOO_EARLY1
        notice(s_NickServ, u->nick, DENY_NICK_REG_AS_TOO_EARLY1);
	#endif
        #ifdef DENY_NICK_REG_AS_TOO_EARLY2
        notice(s_NickServ, u->nick, DENY_NICK_REG_AS_TOO_EARLY2);
	#endif
        #ifdef DENY_NICK_REG_AS_TOO_EARLY3
        notice(s_NickServ, u->nick, DENY_NICK_REG_AS_TOO_EARLY3);
	#endif
        #ifdef DENY_NICK_REG_AS_TOO_EARLY4
        notice(s_NickServ, u->nick, DENY_NICK_REG_AS_TOO_EARLY4);
	#endif
        #ifdef DENY_NICK_REG_AS_TOO_EARLY5
        notice(s_NickServ, u->nick, DENY_NICK_REG_AS_TOO_EARLY5);
	#endif
        #ifdef DENY_NICK_REG_AS_TOO_EARLY6
        notice(s_NickServ, u->nick, DENY_NICK_REG_AS_TOO_EARLY6);
	#endif
        return MOD_STOP;
   }
   return MOD_CONT;

}

int m_ns_remove_deny_entry(int argc, char **argv)
{ 
  char *peep = argv[0];
  CN *cn;
  if (peep) {
      //alog("entry for %s removed", peep);
      while((cn = find_entry(peep))) {
             del_entry(cn);
      }
  }
  return MOD_CONT;
}

int m_ns_someuserquit(char *source, int ac, char **av) 
{
  CN *cn;
  if (*source) {
      if ((cn = find_entry(source))) {
           del_entry(cn);
      }
  }
  return MOD_CONT;
}

  
int my_nickservreglock(char *source, int ac, char **av)
{
     char NSRLockX[16];
     char *argv[1];
     CN *cn;
     int XM = 0;

     if (ac < 2) {
        return MOD_CONT;
     }
     if (*source) { 
         if ((cn = find_entry(source))) {
	      del_entry(cn);
              #ifdef USE_NICK_TRACKING
	      XM = rtncount();
              if (XM >= NICKBUFFER) {
                  m_regconnecttimeoutloghash();
		  return MOD_CONT;
	      }
              if ((cn = create_entry(av[0]))) {
                   argv[0] = sstrdup(av[0]);
                   NSRLock = NSRLock + 1;
                   snprintf(NSRLockX, sizeof(NSRLockX), "CNRD-%d", NSRLock);
                   moduleAddCallback(NSRLockX, time(NULL)+TIME_TO_DENY_NICKREG, m_ns_remove_deny_entry,1,argv);
		  // alog("added callback for %s %d",av[0], TIME_TO_DENY_NICKREG);
		   free(argv[0]);
	      }
	      #endif
	 }
         return MOD_CONT;
     }
     if ((cn = find_entry(av[0]))) {
          del_entry(cn);
     }
     XM = rtncount();
     if (XM >= NICKBUFFER) {
         m_regconnecttimeoutloghash();
         return MOD_CONT;
     }
     if ((cn = create_entry(av[0]))) {
          argv[0] = sstrdup(av[0]);
          NSRLock = NSRLock + 1;
          snprintf(NSRLockX, sizeof(NSRLockX), "CNRD-%d", NSRLock);
          moduleAddCallback(NSRLockX, time(NULL)+TIME_TO_DENY_NICKREG, m_ns_remove_deny_entry,1,argv);
	  //alog("added callback for %s %d",av[0], TIME_TO_DENY_NICKREG);
          free(argv[0]);
     }
     if (NSRLock > 9999999) {
         NSRLock = 1;
     }
     return MOD_CONT;
}

static int del_entry(CN *cn)
{
    if (cn->next) {
        cn->next->prev = cn->prev;
    }
    if (cn->prev) {
        cn->prev->next = cn->next;
    }
    else {
          cnsh[tolower(*cn->nick)] = cn->next;
    }
    ncns--;
    free(cn->nick);
    free(cn);
    return 1;
}

static CN *find_entry(char *nick)
{
    CN *cn;
    if (!nick || !*nick) {
        return NULL;
    }
    for (cn = cnsh[tolower(*nick)]; cn; cn = cn->next) {
        if (!stricmp(nick, cn->nick)) {
            return cn;
	}
    }
    return NULL;
}

static void add_entry(CN * cn)
{
    CN *next, *prev;
    
    for (prev = NULL, next = cnsh[tolower(*cn->nick)]; next != NULL && stricmp(next->nick, cn->nick) < 0; prev = next, next = next->next);
    cn->prev = prev;
    cn->next = next;
    if (!prev) {
        cnsh[tolower(*cn->nick)] = cn;
    } 
    else {
          prev->next = cn;
    }
    if (next) {
        next->prev = cn;
    }
    return;
}

static CN *create_entry(char *nick)
{
    CN *cn;
    cn = scalloc(sizeof(CN), 1);
    cn->nick = sstrdup(nick);
    add_entry(cn);
    ncns++;
    return cn;
}

static int rtncount(void)
{
   int i=0;
   CN *cn;
   int count=0;
   
   if (!ncns) { 
       return 0;
   }
   for (i = 0; i < NICKBUFFER; i++) {
        for (cn = cnsh[i]; cn; cn = cn->next) {
             count++;
	     if (count > NICKBUFFER) {
		    //alog("ERROR: %d entries", count);
		    break;
	     }
       }
   }
   return count;
}

static void m_regconnecttimeoutloghash(void)
{
   int D = 0;
   D = TIME_TO_DENY_NICKREG * 4;
   alog("[ns_limitregonconnect] Hash Table Full - This isn't too much of a problem, but if you are getting");
   alog("[ns_limitregonconnect] this notice reguraly (and outside of %d secs of the 1st of these messages)", D);
   alog("[ns_limitregonconnect] considder recompiling the module with a larger NICKBUFFER setting.");
   alog("[ns_limitregonconnect] Note: this error is common when servers are relinking, or you are recieving");
   alog("[ns_limitregonconnect] many new client connections e.g. when a botnet joins your network.");
} 
