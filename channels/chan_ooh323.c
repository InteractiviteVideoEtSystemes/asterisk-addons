/*
 * Copyright (C) 2004-2005 by Objective Systems, Inc.
 *
 * This software is furnished under an open source license and may be 
 * used and copied only in accordance with the terms of this license. 
 * The text of the license may generally be found in the root 
 * directory of this installation in the COPYING file.  It 
 * can also be viewed online at the following URL:
 *
 *   http://www.obj-sys.com/open/license.html
 *
 * Any redistributions of this file including modified versions must 
 * maintain this copyright notice.
 *
 *****************************************************************************/


#include "chan_ooh323.h"

/* Defaults */
#define DEFAULT_CONTEXT "default"
#define DEFAULT_H323ID "Asterisk PBX"
//#define DEFAULT_LOGFILE "/var/log/asterisk/h323_log"
#define DEFAULT_H323ACCNT "ast_h323"

/* Flags */
#define H323_SILENCESUPPRESSION (1<<0)
#define H323_GKROUTED           (1<<1)
#define H323_TUNNELING          (1<<2)
#define H323_FASTSTART          (1<<3)
#define H323_OUTGOING           (1<<4)
#define H323_ALREADYGONE        (1<<5)
#define H323_NEEDDESTROY        (1<<6)
#define H323_DISABLEGK          (1<<7)
#define H323_CAPSCONFIGURED     (1<<8)
#define H323_RTPCONFIGURED      (1<<9)
#define H323_HAVECHARGVECTOR    (1<<10)
#define H323_HAVETERMINFO       (1<<11)
#define H323_HAVERECVAUDIO      (1<<12)
#define H323_HAVERECVVIDEO      (1<<13)

/* Channel description */
static const char ooh323_chantype[] = "OOH323";
static const char tdesc[] = "Objective Systems H323 Channel Driver";
static const char config[] = "ooh323.conf";


/* Channel Definition */
static struct ast_channel *ooh323_request(const char *type, int format, 
                                        void *data, int *cause);
static int ooh323_digit_begin(struct ast_channel *ast, char digit);
static int ooh323_digit_end(struct ast_channel *ast, char digit, unsigned int duration);
static int ooh323_call(struct ast_channel *ast, char *dest, int timeout);
static int ooh323_hangup(struct ast_channel *ast);
static int ooh323_answer(struct ast_channel *ast);
static struct ast_frame *ooh323_read(struct ast_channel *ast);
static int ooh323_write(struct ast_channel *ast, struct ast_frame *f);
static int ooh323_indicate(struct ast_channel *ast, int condition, const void *data, size_t datalen);
static int ooh323_fixup
   (struct ast_channel *oldchan, struct ast_channel *newchan);

static enum ast_rtp_get_result ooh323_get_rtp_peer(struct ast_channel *chan, struct ast_rtp **rtp);
static enum ast_rtp_get_result ooh323_get_vrtp_peer(struct ast_channel *chan, struct ast_rtp **rtp);
static enum ast_rtp_get_result ooh323_get_trtp_peer(struct ast_channel *chan, struct ast_rtp **rtp);
static int ooh323_set_rtp_peer(struct ast_channel *chan, struct ast_rtp *rtp, struct ast_rtp *vrtp, struct ast_rtp *trtp,
    			       int codecs, int nat_active);

static int ooh323_get_codec(struct ast_channel *chan);

static int ooh323_set_capabilities(struct ast_channel *ast, struct ast_capabilities *chancaps);
static int ooh323_get_negociated_caps(struct ast_channel *ast, struct ast_capabilities *jcaps);
static void print_codec_to_cli(int fd, struct ast_codec_pref *pref);
#ifdef H323_EXIT
static void ast_ooh323c_exit(void);
#endif
static void do_setnat(struct ooh323_pvt *p, int natflags);
int ooh323_InitFlagCodec( ooCallData *call);

const char* ooH323AstGetChargVectorID( struct ast_channel *ast);
int ooH323AstSetChargVectorID( struct ast_channel *ast ,char* chargVectorID );
int ooH323AstSetPublicIP( struct ast_channel *ast ,char* publicIP );
int ooH323AstSetPrivateIP( struct ast_channel *ast ,char* privateIP );
int ooH323AstSetTerminalInfo( struct ast_channel* ast ,struct ooh323_pvt* pvt );
int ooh323_replace_InvalidChar4SipUri(char* nm2replace );

struct ooh323_peer *find_friend(const char *name, int port);

static const struct ast_channel_tech ooh323_tech = {
   .type = ooh323_chantype,
   .description = tdesc,
   .capabilities = -1,
   .requester = ooh323_request,
   .send_digit_begin = ooh323_digit_begin,
   .send_digit_end = ooh323_digit_end,
   .call = ooh323_call,
   .hangup = ooh323_hangup,
   .answer = ooh323_answer,
   .read = ooh323_read,
   .write = ooh323_write,
   .write_video = ooh323_write,   
   .exception = ooh323_read,
   .indicate = ooh323_indicate,
   .fixup = ooh323_fixup,
   .send_html = 0,
#ifdef VIDEOCAPS
   .set_capabilities = ooh323_set_capabilities,//from videocpas -----------
   .get_negociated_caps = ooh323_get_negociated_caps,//Fusion
#endif
   .bridge = ast_rtp_bridge,
};

static struct ast_rtp_protocol ooh323_rtp = {
   .type = ooh323_chantype,
   .get_rtp_info = ooh323_get_rtp_peer,
   .get_vrtp_info = ooh323_get_vrtp_peer,
   .get_trtp_info = ooh323_get_trtp_peer,
   .set_rtp_peer = ooh323_set_rtp_peer,
   .get_codec = ooh323_get_codec,
};

/* H.323 channel private structure */
static struct ooh323_pvt *iflist = NULL;

/* Protect the channel/interface list (ooh323_pvt) */
AST_MUTEX_DEFINE_STATIC(iflock);

/* Profile of H.323 user registered with PBX*/
struct ooh323_user{
   ast_mutex_t lock;
   char        name[256];
   char        context[AST_MAX_EXTENSION];
   int         incominglimit;
   unsigned    inUse;
   char        accountcode[20];
   int         amaflags;
   int         capability;
   struct ast_codec_pref prefs;
   int         dtmfmode;
   int         rtptimeout;
   int         mUseIP;        /* Use IP address or H323-ID to search user */
   char        mIP[20];
   struct ooh323_user *next;
};

/* Profile of valid asterisk peers */
struct ooh323_peer{
   ast_mutex_t lock;
   char        name[256];
   unsigned    outgoinglimit;
   unsigned    outUse;
   int         capability;
   struct ast_codec_pref prefs;
   char        accountcode[20];
   int         amaflags;
   int         dtmfmode;
   int         mFriend;    /* indicates defined as friend */
   char        ip[20];
   int         port;
   char        *h323id;    /* H323-ID alias, which asterisk will register with gk to reach this peer*/
   char        *email;     /* Email alias, which asterisk will register with gk to reach this peer*/
   char        *url;       /* url alias, which asterisk will register with gk to reach this peer*/
   char        *e164;      /* e164 alias, which asterisk will register with gk to reach this peer*/
   int         rtptimeout;
   struct ooh323_peer *next;
};


/* List of H.323 users known to PBX */
static struct ast_user_list {
   struct ooh323_user *users;
   ast_mutex_t lock;
} userl;

static struct ast_peer_list {
   struct ooh323_peer *peers;
  ast_mutex_t lock;
}peerl;

/* Mutex to protect H.323 reload process */
static int h323_reloading = 0;
AST_MUTEX_DEFINE_STATIC(h323_reload_lock);

/* Mutex to protect usage counter */
static int usecnt =0;
AST_MUTEX_DEFINE_STATIC(usecnt_lock);

AST_MUTEX_DEFINE_STATIC(ooh323c_cmd_lock);

/* stack callbacks */
int onAlerting(ooCallData *call);
int onNewCallCreated(ooCallData *call);
int onCallEstablished(ooCallData *call);
int onCallCleared(ooCallData *call);

static char gLogFile[256] = DEFAULT_TRACEFILE ;
static int  gPort = 1720;
static char gIP[20];
static char gPublicIP[20];
static char gCallerID[AST_MAX_EXTENSION] = DEFAULT_H323ID;
static struct ooAliases *gAliasList;
static int  gCapability = AST_FORMAT_ULAW;
static struct ast_codec_pref gPrefs;
static int  gDTMFMode = H323_DTMF_RFC2833|H323_DTMF_INBAND|H323_DTMF_RFC2833| H323_DTMF_Q931|H323_DTMF_H245ALPHANUMERIC|H323_DTMF_H245SIGNAL;;
  
static char gGatekeeper[100];
static enum RasGatekeeperMode gRasGkMode = RasNoGatekeeper;
static char gProductID[OO_PRODUCT_ID_SIZE] = {0} ;
static char gVersionID[OO_PRODUCT_ID_SIZE] = {0} ;
static int  gManufacturerCode = DEFAULT_MANUFACTURERCODE ; 
static int  gIsGateway = 0;
static int  gFastStart = 1;
static int  gTunneling = 1;
static int  gMediaWaitForConnect = 0;
static int  gVp200Info= 0 ;
static int  gBearerCapabilityMode=Q931TransferCircuitMode;
static int  gTOS = 0;
static int  gssrcfix = 0;
static int  gRTPTimeout = 60;
static char gAccountcode[80] = DEFAULT_H323ACCNT;
static int  gAMAFLAGS;
static char gContext[AST_MAX_EXTENSION] = DEFAULT_CONTEXT;
static int  gIncomingLimit = 4;
static int  gOutgoingLimit = 4;
OOBOOL gH323Debug = FALSE;

static struct ooh323_config
{
   int    mTCPPortStart;
   int    mTCPPortEnd;
   int    mUDPPortStart;
   int    mUDPPortEnd ;
   int    mRTPPortStart ;
   int    mRTPPortEnd ;
   time_t mFURMinTime ;
   
}ooconfig;

typedef struct 
{
  int s;
  struct ast_frame f;
  unsigned char rawdata[8192 + AST_FRIENDLY_OFFSET];
  unsigned int ssrc;              /*!< Synchronization source, RFC 3550, page 10. */
} my_ast_rtp;

/** Asterisk RTP stuff*/
static struct sched_context *sched;
static struct io_context *io;

/* Protect the monitoring thread, so only one process can kill or start it, 
   and not when it's doing something critical. */
AST_MUTEX_DEFINE_STATIC(monlock);


/* This is the thread for the monitor which checks for input on the channels
   which are not currently in use.  */
static pthread_t monitor_thread = AST_PTHREADT_NULL;


static struct ast_channel *ooh323_new(struct ooh323_pvt *i, int state,
                                      const char *host ) 
{
   struct ast_channel *ch = NULL;
   int fmt;
   char cformats[_STR_CODEC_SIZE] = {0};

   if ( option_debug > 2 )
     ast_log(LOG_DEBUG,"ooh323_new : state[%d]\n",state);

   /* Don't hold a h323 pvt lock while we allocate a channel */
   ast_mutex_unlock(&i->lock);
   ch = ast_channel_alloc(1, state, i->callerid_num, i->callerid_name, i->accountcode, i->exten, i->context, i->amaflags, "OOH323/%s-%04x", host, rand() & 0xffff);
   ast_mutex_lock(&i->lock);

   if(ch)
   {
      ast_mutex_lock(&ch->lock);
      ch->tech = &ooh323_tech;

      ch->nativeformats = i->capability;

      ast_log(LOG_DEBUG,"native codecs (nativeformats) [%s]\n", 
              ast_getformatname_multiple(cformats,_STR_CODEC_SIZE, ch->nativeformats));

      fmt = ast_best_codec(ch->nativeformats);
      
      //fmt |= (ch->nativeformats & AST_FORMAT_VIDEO_MASK);

      if ( option_debug > 2 )
        ast_log(LOG_DEBUG,"First prefered audio codec  [%s]\n", 
                ast_getformatname_multiple(cformats,_STR_CODEC_SIZE, fmt));

      ch->fds[0] = ast_rtp_fd(i->rtp);
      ch->fds[1] = ast_rtcp_fd(i->rtp);

      ch->fds[2] = ast_rtp_fd(i->vrtp);
      ch->fds[3] = ast_rtcp_fd(i->vrtp);

      if (state == AST_STATE_RING)
         ch->rings = 1;

      ch->adsicpe = AST_ADSI_UNAVAILABLE;
      ch->writeformat = fmt;
      ch->rawwriteformat = fmt;
      ch->readformat = fmt;
      ch->rawreadformat = fmt;
      ch->tech_pvt = i;
      i->owner = ch;

      /* Allocate dsp for in-band DTMF support */
      if(i->dtmfmode & H323_DTMF_INBAND) {
         i->vad = ast_dsp_new();
         ast_dsp_set_features(i->vad, DSP_FEATURE_DTMF_DETECT);
    		 //ast_dsp_digitmode(i->vad, DSP_DIGITMODE_DTMF | DSP_DIGITMODE_RELAXDTMF);         

         if(gH323Debug)
         ast_verbose("---   ooh323_new - dsp allocated\n");
      }

      ast_mutex_lock(&usecnt_lock);
      usecnt++;
      ast_mutex_unlock(&usecnt_lock);

      /* Notify the module monitors that use count for resource has changed*/
      ast_update_use_count();

      strncpy(ch->context, i->context, sizeof(ch->context)-1);
      ch->context[sizeof(ch->context)-1] = '\0';
      strncpy(ch->exten, i->exten, sizeof(ch->exten)-1);
      ch->exten[sizeof(ch->exten)-1] = '\0';

      ch->priority = 1;
      if (i->callerid_name)
      {
        ch->cid.cid_name = strdup(i->callerid_name);
      }else if (i->caller_h323id) {
        ch->cid.cid_name = strdup( i->caller_h323id);
      }

      if(i->callerid_num){
        ch->cid.cid_num = strdup(i->callerid_num);
      }else if ( i->caller_h323id ){
        ch->cid.cid_num = strdup(i->caller_h323id);
      }

      if(!ast_test_flag(i, H323_OUTGOING))
      {
      
         if (!ast_strlen_zero(i->caller_h323id)) {
            pbx_builtin_setvar_helper(ch, "_CALLER_H323ID", i->caller_h323id);

         }
         if (!ast_strlen_zero(i->caller_dialedDigits)) {
            pbx_builtin_setvar_helper(ch, "_CALLER_H323DIALEDDIGITS", 
                                                       i->caller_dialedDigits);
         }
         if (!ast_strlen_zero(i->caller_email)) {
            pbx_builtin_setvar_helper(ch, "_CALLER_H323EMAIL", 
                                                              i->caller_email);
         }
         if (!ast_strlen_zero(i->caller_url)) {
            pbx_builtin_setvar_helper(ch, "_CALLER_H323URL", i->caller_url);
         }
      }

      if (!ast_strlen_zero(i->accountcode))
         ast_string_field_set(ch, accountcode, i->accountcode);
      
      if (i->amaflags)
         ch->amaflags = i->amaflags;

      ast_setstate(ch, state);
      ast_log(LOG_DEBUG, "[H323] update ast_channel_state [%d]\n",state);
      if (state != AST_STATE_DOWN) {
         if (ast_pbx_start(ch)) {
            ast_log(LOG_WARNING, "Unable to start PBX on %s\n", ch->name);
            ast_mutex_unlock(&ch->lock);
            ast_hangup(ch);
            ch = NULL;
         }
      }
   } else
      ast_log(LOG_WARNING, "Unable to allocate channel structure\n");


   if(ch)
   {
     if ( ast_test_flag(i, H323_HAVECHARGVECTOR) )
     {
       ooH323AstSetChargVectorID( ch ,  i->ChargVectorID );
     }
     else
     {
       if ( option_debug > 2 )
         ast_log(LOG_DEBUG, "[H323] No Charging-Vector \n");
     }
     if ( ast_test_flag(i, H323_HAVETERMINFO ) )
     {
       ooH323AstSetTerminalInfo( ch ,  i );
     }
     ast_mutex_unlock(&ch->lock);

     if ( option_debug > 2 )
       ast_log(LOG_DEBUG, "[H323] setting owner (ast_channel) 0x%X\n",ch);
   }

   return ch;
}



static struct ooh323_pvt *ooh323_alloc(int callref, char *callToken) 
{
   struct ooh323_pvt *pvt = NULL;
   struct in_addr ipAddr;

   pvt = (struct ooh323_pvt*) malloc(sizeof(struct ooh323_pvt));
   if(!pvt)
   {
      ast_log(LOG_ERROR, "Couldn't allocate private ooh323 structure\n");
      return NULL;
   }

   memset(pvt, 0, sizeof(struct ooh323_pvt));
   ast_mutex_init(&pvt->lock);
   ast_mutex_lock(&pvt->lock);


   pvt->caller_h323id[0]='\0';
   pvt->caller_dialedDigits[0]='\0';
   pvt->caller_email[0]='\0';
   pvt->caller_url[0]='\0';
   pvt->vrtp = NULL;   
   pvt->callee_h323id[0]='\0';
   pvt->callee_dialedDigits[0]='\0';
   pvt->callee_email[0] = '\0';
   pvt->callee_url[0] = '\0';

   pvt->host = NULL;
   pvt->callToken = NULL;
   pvt->port = 0;
   pvt->vad = NULL;
   pvt->next = NULL;
   pvt->owner = NULL;
   pvt->callerid_name = NULL;
   pvt->callerid_num = NULL;
   pvt->flags = 0;
   pvt->ssrc  = 0;
   if(!inet_aton(gIP, &ipAddr))
   {
      ast_log(LOG_ERROR, "Invalid OOH323 driver ip address\n");
      ast_mutex_unlock(&pvt->lock);
      ast_mutex_destroy(&pvt->lock);
      free(pvt);
      return NULL;
   }
   

   pvt->rtp = ast_rtp_new_with_bindaddr(sched, io, 1, 0, ipAddr);

   if (!pvt->rtp) 
   {
      ast_log(LOG_WARNING, "Unable to create RTP session: %s\n", 
              strerror(errno));
      ast_mutex_unlock(&pvt->lock);
      ast_mutex_destroy(&pvt->lock);
      free(pvt);
      return NULL;
   }
 
   ast_rtp_settos(pvt->rtp, gTOS);

   pvt->vrtp = ast_rtp_new_with_bindaddr(sched, io, 1, 0, ipAddr);

   if (!pvt->vrtp) 
   {
      ast_log(LOG_WARNING, "Unable to create VRTP session: %s\n", 
              strerror(errno));
      ast_mutex_unlock(&pvt->lock);
      ast_mutex_destroy(&pvt->lock);
      free(pvt);
      return NULL;
   }
 
   ast_rtp_settos(pvt->vrtp, 0);
   ast_rtp_set_transparency(pvt->vrtp, 10);

   pvt->call_reference = callref;
   if(callToken)
      pvt->callToken = strdup(callToken);

   /* whether to use gk for this call */
   if(gRasGkMode == RasNoGatekeeper)
      OO_SETFLAG(pvt->flags, H323_DISABLEGK);

   pvt->dtmfmode = gDTMFMode;
   strncpy(pvt->context, gContext, sizeof(pvt->context)-1);
   pvt->exten[0]='\0';
   strncpy(pvt->accountcode, gAccountcode, sizeof(pvt->accountcode)-1);
   pvt->amaflags = gAMAFLAGS;
   pvt->capability = gCapability;
   memcpy(&pvt->prefs, &gPrefs, sizeof(struct ast_codec_pref));


  ast_mutex_unlock(&pvt->lock); 
  /* Add to interface list */
  ast_mutex_lock(&iflock);
  pvt->next = iflist;
  iflist = pvt;
  ast_mutex_unlock(&iflock);

  if ( option_debug > 2 )
  {
    ast_log(LOG_DEBUG, "ooh323_alloc: create ooh323_pvt* [0x%X] & RTP  \n",pvt);
  }
  // memset(pvt->ChargVectorID, 0 , OO_PCV_CID_SIZE );
  return pvt;
}


/*
   Possible data values - peername, exten/peername, exten@ip
 */
static struct ast_channel *ooh323_request(const char *type, int format, 
                                          void *data, int *cause)
{
   struct ast_channel *chan=NULL;
   struct ooh323_pvt *p = NULL;
   struct ooh323_peer *peer = NULL;
   char *dest = NULL; 
   char *ext = NULL;
   char tmp[256];
   char formats[_STR_CODEC_SIZE];
   int oldformat;
   int port = 0;

   if ( option_debug > 2 )
     ast_log(LOG_DEBUG,"ooh323_request: data %s format[%s]\n", 
             (char*)data,  
             ast_getformatname_multiple(formats,_STR_CODEC_SIZE,format) );

   oldformat = format;
   format &= ((AST_FORMAT_MAX_AUDIO << 1) - 1);
   if (!format) {
      ast_log(LOG_NOTICE, "Asked to get a channel of unsupported format "
                          "'%d'\n", format);
      return NULL;
   }

   p = ooh323_alloc(0,0); /* Initial callRef is zero */

   if(!p)
   {
      ast_log(LOG_WARNING, "Unable to build pvt data for '%s'\n", (char*)data);
      return NULL;
   }
   ast_mutex_lock(&p->lock);

   /* This is an outgoing call, since ooh323_request is called */
   ast_set_flag(p, H323_OUTGOING);


   strncpy(tmp, (char*)data, sizeof(tmp)-1);

   dest = strchr(tmp, '/');

   if(dest)
   {  
      *dest = '\0';
      dest++;
      ext = tmp;
   }
   else if((dest = strchr(tmp, '@'))) {
      *dest = '\0';
      dest++;
      ext = tmp;
   }
   else {
      dest = tmp;
      ext = NULL;
   }
   
   /*if((sport = strchr(dest, ':'))) {
      *sport = '\0';
      sport++;
      port = atoi(sport);
   }*/

   if(dest) {
      peer = find_peer(dest, port);
   }
   else{
      ast_log(LOG_ERROR, "Destination format is not supported\n");
      return NULL;
   }

   if(peer)
   {
      p->username = strdup(peer->name);
      p->host = strdup(peer->ip);
      p->port = peer->port;
      /* Disable gk as we are going to call a known peer*/
      OO_SETFLAG(p->flags, H323_DISABLEGK);

      if(ext)
         strncpy(p->exten, ext, sizeof(p->exten)-1);

      if(peer->capability & format)
         p->capability = peer->capability & format;
      else{
         p->capability = peer->capability;
      }
      memcpy(&p->prefs, &peer->prefs, sizeof(struct ast_codec_pref));
      p->dtmfmode = peer->dtmfmode;
      strncpy(p->accountcode, peer->accountcode, sizeof(p->accountcode)-1);
      p->amaflags = peer->amaflags;
   }
   else {
      p->dtmfmode = gDTMFMode;
      p->capability = gCapability;

      memcpy(&p->prefs, &gPrefs, sizeof(struct ast_codec_pref));
      p->username = strdup(dest);
     
      p->host = strdup(dest);
      if(port > 0) {
         p->port = port;
      }
      if(ext) {
         strncpy(p->exten, ext, sizeof(p->exten)-1);
      }
   }


   chan = ooh323_new(p, AST_STATE_DOWN, p->username);
   
   ast_mutex_unlock(&p->lock);

   if (!chan)
   {
      ast_mutex_lock(&iflock);
      ooh323_destroy(p);
      ast_mutex_unlock(&iflock);
   }

   restart_monitor();
   if(gH323Debug)
      ast_verbose("+++   ooh323_request\n");

   return chan;

}


static struct ooh323_pvt* find_call(ooCallData *call)
{
   struct ooh323_pvt *p=NULL;

   p = iflist;
   ast_mutex_lock(&iflock);

   while(p)
   {
      if(p->callToken && !strcmp(p->callToken, call->callToken))
      {
         break;
      }
      p = p->next;
      
   }
   ast_mutex_unlock(&iflock);

   if ( option_debug > 2 )
     ast_log(LOG_DEBUG,"[H323]find call : 0x%X\n",p);

   return p;
}

struct ooh323_pvt* ooh323_find_call(ooCallData *call)
{
    return find_call(call);
}

struct ooh323_user *find_user(const char * name, const char* ip)
{
   struct ooh323_user *user=NULL;

   if (ip) ast_log(LOG_DEBUG,"[H323]find user : ip=%s\n",ip);
   user = userl.users;
   ast_mutex_lock(&userl.lock);
   while(user)
   {
      if(ip && user->mUseIP && !strcmp(user->mIP, ip)) {
         break;
      }
      if(name && !strcmp(user->name, name)) {
         break;
      }
      user = user->next;
   }
   ast_mutex_unlock(&userl.lock);

   if ( option_debug > 2 )
     ast_log(LOG_DEBUG,"[H323]find user : 0x%X\n",user);

   return user;      
}

struct ooh323_peer *find_friend(const char *name, int port)
{
   struct ooh323_peer *peer=NULL;  

   if(gH323Debug)
      ast_verbose("---   find_friend \"%s\"\n", name);


   peer = peerl.peers;
   ast_mutex_lock(&peerl.lock);
   while(peer)
   {
      if(gH323Debug) {
         ast_verbose("      comparing with \"%s\"\n", peer->ip);
      }
      if(!strcmp(peer->ip, name)) {
         if(port > 0 && peer->port == port) { break; }
         else if (port <= 0) { break; }
      }
      peer = peer->next;
   }
   ast_mutex_unlock(&peerl.lock);

   if(gH323Debug) {
      if(peer) {
         ast_verbose("      found matching friend\n");
      }
      ast_verbose("+++   find_friend \"%s\"\n", name);
   }

   return peer;      
}


struct ooh323_peer *find_peer(const char * name, int port)
{
   struct ooh323_peer *peer=NULL;  

   if(gH323Debug)
      ast_verbose("---   find_peer \"%s\"\n", name);


   peer = peerl.peers;
   ast_mutex_lock(&peerl.lock);
   while(peer)
   {
      if(gH323Debug) {
         ast_verbose("      comparing with \"%s\"\n", peer->ip);
      }
      if(!strcasecmp(peer->name, name))
         break;
      if(peer->h323id && !strcasecmp(peer->h323id, name))
         break;
      if(peer->e164 && !strcasecmp(peer->e164, name))
         break;
      /*
      if(!strcmp(peer->ip, name)) {
         if(port > 0 && peer->port == port) { break; }
         else if (port <= 0) { break; }
      }
      */
      peer = peer->next;
   }
   ast_mutex_unlock(&peerl.lock);

   if(gH323Debug) {
      if(peer) {
         ast_verbose("      found matching peer\n");
      }
      ast_verbose("+++   find_peer \"%s\"\n", name);
   }

   return peer;      
}

static int ooh323_digit_begin(struct ast_channel *chan, char digit)
{
   char dtmf[2];
   struct ooh323_pvt *p = (struct ooh323_pvt *) chan->tech_pvt;
   
   if(gH323Debug)
      ast_verbose("---   ooh323_digit_begin\n");

   if(!p){
      ast_log(LOG_ERROR, "No private structure for call\n");
      return -1;
   }
   ast_mutex_lock(&p->lock);
   if (p->rtp && (p->dtmfmode & H323_DTMF_RFC2833)) {
      ast_rtp_senddigit_begin(p->rtp, digit);
   }
   if (((p->dtmfmode & H323_DTMF_Q931) ||
                   (p->dtmfmode & H323_DTMF_H245ALPHANUMERIC) ||
                   (p->dtmfmode & H323_DTMF_H245SIGNAL))){
      dtmf[0]= digit;
      dtmf[1]='\0';
      ast_mutex_lock(&ooh323c_cmd_lock);
      ooSendDTMFDigit(p->callToken, dtmf);
      ast_mutex_unlock(&ooh323c_cmd_lock);
   }
   else if (p->rtp && (p->dtmfmode & H323_DTMF_RFC2833)) {
      ast_rtp_senddigit_begin(p->rtp, digit);
   }
   ast_mutex_unlock(&p->lock);
   if(gH323Debug)
      ast_verbose("+++   ooh323_digit_begin\n");

   return 0;
}

static int ooh323_digit_end(struct ast_channel *chan, char digit, unsigned int duration)
{
   struct ooh323_pvt *p = (struct ooh323_pvt *) chan->tech_pvt;

   if(gH323Debug)
      ast_verbose("---   ooh323_digit_end\n");

   if(!p){
      ast_log(LOG_ERROR, "No private structure for call\n");
      return -1;
   }
   ast_mutex_lock(&p->lock);
   if (p->rtp && (p->dtmfmode & H323_DTMF_RFC2833)) 
      ast_rtp_senddigit_end(p->rtp, digit);

   ast_mutex_unlock(&p->lock);
   if(gH323Debug)
      ast_verbose("+++   ooh323_digit_end\n");

   return 0;
}


static int ooh323_call(struct ast_channel *ast, char *dest, int timeout)
{
   struct ooh323_pvt *p = ast->tech_pvt;
   char destination[256];
   int res=0;
   const char *val=NULL;
   ooCallOptions opts = {
     .fastStart = gFastStart?TRUE:FALSE,
     .tunneling = gTunneling?TRUE:FALSE,
     .disableGk = (gRasGkMode == RasNoGatekeeper)?TRUE:FALSE,
     .callMode = OO_CALLMODE_AUDIOCALL
   };

   if ( option_debug > 2 )
     ast_log(LOG_DEBUG, "ooh323_call- %s\n", dest);

   if (ast->nativeformats & AST_FORMAT_VIDEO_MASK)
   {
     opts.callMode = OO_CALLMODE_VIDEOCALL;
   }

   if ((ast->_state != AST_STATE_DOWN) && (ast->_state != AST_STATE_RESERVED)) 
   {
      ast_log(LOG_WARNING, "ooh323_call called on %s, neither down nor "
                           "reserved\n", ast->name);
      return -1;
   }
   ast_mutex_lock(&p->lock);
   ast_set_flag(p, H323_OUTGOING);
   if (ast->cid.cid_num) {
     if(p->callerid_num)   free(p->callerid_num);
      p->callerid_num = strdup(ast->cid.cid_num);
   }

   if(ast->cid.cid_name) {
      if(p->callerid_name)   free(p->callerid_name);
      p->callerid_name = strdup(ast->cid.cid_name);
   }
   else{
      ast->cid.cid_name = strdup(gCallerID);
      if(p->callerid_name)   free(p->callerid_name);
      p->callerid_name = strdup(ast->cid.cid_name);
   }

   /* Retrieve vars */
   val = pbx_builtin_getvar_helper(ast, "CALLER_H323ID");
   if(val){
     strncpy(p->caller_h323id, val, sizeof(p->caller_h323id)-1);
   }
   
   val = pbx_builtin_getvar_helper(ast, "CALLER_H323DIALEDDIGITS");
   if(val){
      strncpy(p->caller_dialedDigits, val, sizeof(p->caller_dialedDigits)-1);
      if(!p->callerid_num)   p->callerid_num = strdup(val);
     
   }

   val = pbx_builtin_getvar_helper(ast, "CALLER_H323EMAIL");
   if(val)
     strncpy(p->caller_email, val, sizeof(p->caller_email)-1);
   
   val = pbx_builtin_getvar_helper(ast, "CALLER_H323URL");
   if(val)
     strncpy(p->caller_url, val, sizeof(p->caller_url)-1);  


   p->callToken = (char*)malloc(AST_MAX_EXTENSION);
   if(!p->callToken)
   {
      ast_mutex_unlock(&p->lock);
      ast_log(LOG_ERROR, "Failed to allocate memory for callToken\n");
      return -1; /* TODO: need to clean/hangup?? */
   }      

   if(p->host && p->port != 0)
      sprintf(destination, "%s:%d", p->host, p->port);
   else if(p->host)
      sprintf(destination, "%s", p->host);
   else
     strncpy(destination, dest, sizeof(destination)-1);

   destination[sizeof(destination)-1]='\0';

   ast_mutex_lock(&ooh323c_cmd_lock);
   //if(OO_TESTFLAG(p->flags, H323_DISABLEGK))
   res = ooMakeCall(destination, p->callToken, ooH323AstGetChargVectorID( ast ) , (size_t)AST_MAX_EXTENSION, &opts);
   //else
   //  res = ooMakeCall(destination, p->callToken, (char*)ooH323AstGetChargVectorID( ast ) ,AST_MAX_EXTENSION, NULL);
   ast_mutex_unlock(&ooh323c_cmd_lock);
   ast_mutex_unlock(&p->lock);

   if ( option_debug > 2 )
     ast_log(LOG_DEBUG, "[H323/CMD] Waiting call creation %s\n",p->callToken);   
   ooSyncLock();
   if ( option_debug > 2 )
     ast_log(LOG_DEBUG, "[H323/CMD] Ok call creation %s\n",p->callToken);


   if(res != OO_OK)
   {
      ast_log(LOG_ERROR,"Failed to make call\n");
      return -1; /* ToDO: cleanup */
   }

  return 0;
}

static int ooh323_hangup(struct ast_channel *ast)
{
   struct ooh323_pvt *p = ast->tech_pvt;

   if(gH323Debug)
      ast_verbose("---   ooh323_hangup\n");

   if(p)
   {
      ast_mutex_lock(&p->lock);

      if(gH323Debug)
	ast_verbose("    hanging %s\n", p->username);
      ast->tech_pvt = NULL; 
      if(!ast_test_flag(p, H323_ALREADYGONE)) {
         ast_mutex_lock(&ooh323c_cmd_lock);
         ooHangCall(p->callToken, 
             ooh323_convert_hangupcause_asteriskToH323(p->owner->hangupcause));
         ast_mutex_unlock(&ooh323c_cmd_lock);
         ast_set_flag(p, H323_ALREADYGONE);
         /* ast_mutex_unlock(&p->lock); */
      }
      else {
         ast_set_flag(p, H323_NEEDDESTROY);
      }
      /* detach channel here */
      if(p->owner)
      {
         p->owner->tech_pvt = NULL;
         p->owner = NULL;
      }

      ast_mutex_unlock(&p->lock);
      ast_mutex_lock(&usecnt_lock);
      usecnt--;
      ast_mutex_unlock(&usecnt_lock);

      /* Notify the module monitors that use count for resource has changed */
      ast_update_use_count();
     
   }


   else {
      ast_log(LOG_ERROR, "No call to hangup\n" );
      return -1;
   }
   
   if(gH323Debug)
      ast_verbose("+++   ooh323_hangup\n");

  return 0;
}

static int ooh323_answer(struct ast_channel *ast)
{
   struct ooh323_pvt *p = ast->tech_pvt;
   char cformats[_STR_CODEC_SIZE] = {0};
   if ( option_debug > 2 )
     ast_log(LOG_DEBUG, "ooh323_answer (%s) \"Answser\" ast_channel_state [%d]\n", 
             ast->name,ast->_state );

#if 0
   //ast_mutex_lock(&p->lock);
   if(ast->_state != AST_STATE_UP)
   {
     //ast_mutex_lock(&ast->lock);
      ast_setstate(ast, AST_STATE_UP);
      if ( option_debug > 2 )
        ast_log(LOG_DEBUG, "ooh323_answer (%s) update ast_channel_state [%d]\n", 
                ast->name,ast->_state );
      //ast_mutex_unlock(&ast->lock);
      ast_mutex_lock(&ooh323c_cmd_lock);
      ooAnswerCall(p->callToken);
      ast_mutex_unlock(&ooh323c_cmd_lock);
   }
   //ast_mutex_unlock(&p->lock);

   if ( option_debug > 2 )
     ast_log(LOG_DEBUG, "[H323/CMD] >> Waiting answers creation %s\n",p->callToken);   
   ooSyncLock();
   if ( option_debug > 2 )
     ast_log(LOG_DEBUG, "[H323/CMD] Ok answers creation %s\n",p->callToken);
#else
   if(ast->_state != AST_STATE_UP)
   {
      ast_mutex_lock(&ooh323c_cmd_lock);
      ooAnswerCall(p->callToken);
      ast_mutex_unlock(&ooh323c_cmd_lock);
      if ( option_debug > 2 )
        ast_log(LOG_DEBUG, "[H323/CMD] >> Waiting answers creation %s\n",p->callToken);   
      ooSyncLock();
      if ( option_debug > 2 )
        ast_log(LOG_DEBUG, "[H323/CMD] Ok answers creation %s\n",p->callToken);
      if ( option_debug > 2 )
        ast_log(LOG_DEBUG, "ooh323_answer (%s) update ast_channel_state [%d]\n", 
                ast->name,ast->_state );
   }
#endif

   if ( option_debug > 2 )
     ast_log(LOG_DEBUG,"native codecs (nativeformats) [%s]\n", 
             ast_getformatname_multiple(cformats,_STR_CODEC_SIZE, ast->nativeformats));

   return 0;
}

static struct ast_frame *ooh323_read(struct ast_channel *ast)
{
   struct ast_frame *fr;
   static struct ast_frame null_frame = { AST_FRAME_NULL, };
   struct ooh323_pvt *p = ast->tech_pvt;

   ast_mutex_lock(&p->lock);
   if(p->rtp||p->vrtp)
      fr = ooh323_rtp_read(ast, p);
   else
      fr = &null_frame;
   /* time(&p->lastrtprx); */
   ast_mutex_unlock(&p->lock);
   return fr;
}

static int ooh323_write(struct ast_channel *ast, struct ast_frame *f)
{
   struct ooh323_pvt *p = ast->tech_pvt;
   int res = 0;

   if(f->frametype == AST_FRAME_VOICE)
   {
     if (option_debug > 4 ) 
       ast_log(LOG_DEBUG, "ooh323_write Send : RTP Voice \n");
   
      if(!(f->subclass & ast->nativeformats))
      {
        char formats1[_STR_CODEC_SIZE] = {0};
        char formats2[_STR_CODEC_SIZE] = {0};
        char formats3[_STR_CODEC_SIZE] = {0};
        char formats4[_STR_CODEC_SIZE] = {0};
        ast_log(LOG_WARNING, "[%s] Asked to transmit audio frame type %s, while native "
                "formats is %s (read/write = %s/%s)\n", ast->name ,
                ast_getformatname_multiple(formats1,_STR_CODEC_SIZE, f->subclass),
                ast_getformatname_multiple(formats2,_STR_CODEC_SIZE, ast->nativeformats),
                ast_getformatname_multiple(formats3,_STR_CODEC_SIZE, ast->readformat),
                ast_getformatname_multiple(formats4,_STR_CODEC_SIZE, ast->writeformat));

         return 0;
      }
      if(p){
         ast_mutex_lock(&p->lock);
         if(p->rtp)
            res = ast_rtp_write(p->rtp, f);
         ast_mutex_unlock(&p->lock);
      }
   }
   else if(f->frametype == AST_FRAME_VIDEO  )
   {
     if (option_debug > 4 ) 
       ast_log(LOG_DEBUG, "ooh323_write Send : RTP Video \n");
   
      if(!( f->subclass  & ast->nativeformats))
      {
        char formats1[_STR_CODEC_SIZE] = {0};
        char formats2[_STR_CODEC_SIZE] = {0};
        char formats3[_STR_CODEC_SIZE] = {0};
        char formats4[_STR_CODEC_SIZE] = {0};
         ast_log(LOG_WARNING, "[%s] Asked to transmit video frame type %s, while native "
                 "formats is %s (read/write = %s/%s)\n", ast->name ,
                 ast_getformatname_multiple(formats1,_STR_CODEC_SIZE, f->subclass),
                 ast_getformatname_multiple(formats2,_STR_CODEC_SIZE, ast->nativeformats),
                 ast_getformatname_multiple(formats3,_STR_CODEC_SIZE, ast->readformat),
                 ast_getformatname_multiple(formats4,_STR_CODEC_SIZE, ast->writeformat));
         return 0;
      }
      if (p)
      {
        if ( gssrcfix )
        {
          my_ast_rtp* chg_ssrc = (my_ast_rtp*)p->vrtp ;
          if ( p->ssrc == 0 )
          {
            p->ssrc = chg_ssrc->ssrc ;
            if ( option_debug > 2 )
              ast_log(LOG_DEBUG, "New ssrc [0x%X]\n", p->ssrc);
          }
          else
          {
            if ( option_debug > 4 )
              ast_log(LOG_DEBUG, "Replace ssrc  [0x%X] -> [0x%X]\n", chg_ssrc->ssrc,p->ssrc );
            chg_ssrc->ssrc = p->ssrc ;
          }
        }
        ast_mutex_lock(&p->lock);
        if(p->vrtp)
        {
          res = ast_rtp_write(p->vrtp, f);
        }
        else
        {
          if ( option_debug > 2 )
            ast_log(LOG_DEBUG, "No RTP Video \n");
        }
        ast_mutex_unlock(&p->lock);
      }
   }
   else if (f->frametype == AST_FRAME_IMAGE) {
      return 0;
   }
   else {
      ast_log(LOG_WARNING, "Can't send %d type frames with SIP write\n", 
                            f->frametype);
      return 0;
   }

   return res;
}

static int ooh323_indicate(struct ast_channel *ast, int condition, const void *data, size_t datalen)
{

   struct ooh323_pvt *p = (struct ooh323_pvt *) ast->tech_pvt;
   char *callToken = (char *)NULL;

   ast_mutex_lock(&p->lock);
   callToken = (p->callToken ? strdup(p->callToken) : NULL);
   ast_mutex_unlock(&p->lock);

   if(!callToken)
   {
      if(gH323Debug)
         ast_verbose("   ooh323_indicate - No callToken\n");
      return -1;
   }

   if(gH323Debug)
      ast_verbose("----- ooh323_indicate %d on call %s\n", condition, callToken);
    

   switch(condition) {
      case AST_CONTROL_CONGESTION:
         if(!ast_test_flag(p, H323_ALREADYGONE)){
            ast_mutex_lock(&ooh323c_cmd_lock);
            ooHangCall(callToken, OO_REASON_LOCAL_CONGESTED);
            ast_mutex_unlock(&ooh323c_cmd_lock);
            ast_set_flag(p, H323_ALREADYGONE);
         }
         break;
      case AST_CONTROL_BUSY:
         if(!ast_test_flag(p, H323_ALREADYGONE)){
            ast_mutex_lock(&ooh323c_cmd_lock);
            ooHangCall(callToken, OO_REASON_LOCAL_BUSY);
            ast_mutex_unlock(&ooh323c_cmd_lock);
            ast_set_flag(p, H323_ALREADYGONE);
         }
         break;
      case AST_CONTROL_HOLD:
         ast_moh_start(ast, data, NULL);      
         break;
      case AST_CONTROL_UNHOLD:
	     ast_moh_stop(ast);
         break;
#ifdef VIDEOCAPS
      case AST_CONTROL_CAPABILITY:
         if (datalen == sizeof(struct ast_capabilities)) 
         {
              struct ast_capabilities *jcaps = (struct ast_capabilities*)data;
              if ( option_debug > 2 )
                ast_log(LOG_DEBUG, "[H323/CODEC] Setting prefcodec from jcaps->cap\n");
              ast_resolve_capabilities(&ast->channelcaps, &ast->channelcaps, jcaps, (option_verbose > 3) );
              //  p->prefcodec = jcaps->cap;
              //if ( option_debug > 2 )
              //  ast_log(LOG_DEBUG,"chan %s - setting preferred codecs to %08x.\n", ast->name, p->prefcodec);
         }
         /* TODO - handle the case when peer channel has no caps */
         if (! ast_test_flag(p, H323_CAPSCONFIGURED) )
         {
           ooCallData *call = call = ooFindCallByToken(p->callToken);
           if ( call )
           {
             ooh323c_set_capability_for_call(call, &p->prefs, p->capability,p->dtmfmode, &ast->channelcaps);
             configure_local_rtp(p, call);
             ast_set_flag(p,H323_CAPSCONFIGURED);
           }
           else
           {
             ast_log(LOG_ERROR, "No call\n");
           }
         }
         else
         {
           if ( option_debug > 2 )
             ast_log(LOG_DEBUG, "chan_ooh323 capabilities already configured for %s.\n", ast->name);
         }
         break;
#endif
         //
     case AST_CONTROL_RINGING:
         ast_mutex_lock(&ooh323c_cmd_lock);
         ooManualRingback(callToken);
         ast_mutex_unlock(&ooh323c_cmd_lock);
	 break;

   // IVeS : Add video fast update 
     case AST_CONTROL_VIDUPDATE:
       if ( option_debug > 2 )
         ast_log(LOG_DEBUG, "chan_ooh323 forward fast update request  %s.\n", ast->name);
       ooSendFastUpdateRequest( callToken , -1 , -1 );
       break ;

      case AST_CONTROL_SRCCHANGE:
	if (p->rtp)
		ast_rtp_change_source(p->rtp);

	if (p->vrtp)
		ast_rtp_change_source(p->vrtp);
	break;

      case AST_CONTROL_PROCEEDING:
      case AST_CONTROL_PROGRESS:
      case -1:
         break;
      default:
         ast_log(LOG_WARNING,"Don't know how to indicate condition %d on %s\n",
                              condition, callToken);
   }

   if(gH323Debug)
      ast_verbose("++++  ooh323_indicate %d on %s\n", condition, callToken);


   return -1;
}

static int ooh323_fixup
   (struct ast_channel *oldchan, struct ast_channel *newchan)
{
   struct ooh323_pvt *p = newchan->tech_pvt;

   if(gH323Debug)
      ast_verbose("--- ooh323c ooh323_fixup\n");

   ast_mutex_lock(&p->lock);
   if (p->owner != oldchan) {
      ast_log(LOG_WARNING, "Old channel wasn't %p but was %p\n", oldchan, p->owner);
      ast_mutex_unlock(&p->lock);
      return -1;
   }

   if (p->owner == oldchan) {
      p->owner = newchan;
   }
   else {
      p->owner = oldchan;
   }

   ast_mutex_unlock(&p->lock);

   if(gH323Debug)
      ast_verbose("+++ ooh323c ooh323_fixup \n");

   return 0;
}


void ooh323_set_write_format(ooCallData *call, int fmt)
{
#if 0
   struct ooh323_pvt *p = NULL;
   char formats[_STR_CODEC_SIZE];
   
   p = find_call(call);
   if(!p)
   {
      ast_log(LOG_ERROR, "No matching call found for %s\n", call->callToken);
      return;
   }

   //ast_mutex_lock(&p->lock);

   // IVes p->writeformat = fmt;
   //ast_mutex_unlock(&p->lock);
       ast_log(LOG_DEBUG,"ooh323_set_write_format fmt=[%s]\n", 
               ast_getformatname_multiple(formats1,_STR_CODEC_SIZE, fmt) );
   if(p->owner){
     printf("Writeformat before update %s\n", 
              ast_getformatname_multiple(formats,_STR_CODEC_SIZE, p->owner->writeformat));
     ast_set_write_format(p->owner, fmt);
   }
   else
     ast_log(LOG_ERROR, "No owner found\n");


#else
  struct ooh323_pvt *p               = NULL;
  char              formats1[_STR_CODEC_SIZE]     = {0};
  char              formats2[_STR_CODEC_SIZE]     = {0};
  char              formats3[_STR_CODEC_SIZE]     = {0};
  int               fmt2             = 0 ;
  int               fmtOld           = 0 ;
  int               oldNativeFormats = 0 ;

  /* udpate native formats */
  if ( option_debug > 2 )
    ast_log(LOG_DEBUG,">> ooh323_set_write_format fmt=[%s]\n",  ast_getformatname_multiple(formats1,_STR_CODEC_SIZE, fmt) );


  p = find_call(call);
  if(!p)
  {
    ast_log(LOG_ERROR, "ooh323_set_write_format No matching call found for %s\n", call->callToken);
    return;
  }

  if ( !p->owner)
  {
    ast_log(LOG_ERROR, "ooh323_set_write_format p->owner null for call found for %s\n", call->callToken);
    return;
  }
  fmtOld = p->owner->nativeformats ;
  fmt2   =  (fmt & AST_FORMAT_AUDIO_MASK) ;
  if ( fmt2 > 0  ) 
  {
    oldNativeFormats = p->owner->nativeformats ;
    ast_mutex_lock(&p->lock);
    p->owner->nativeformats |= fmt2 ;
    ast_mutex_unlock(&p->lock);
 
    ast_log(LOG_DEBUG,"ooh323_set_write_format Update nativeformats (audio) [%s] -> [%s]  and write format[%s]    \n", 
            ast_getformatname_multiple(formats1,_STR_CODEC_SIZE,oldNativeFormats& AST_FORMAT_AUDIO_MASK ),
            ast_getformatname_multiple(formats2,_STR_CODEC_SIZE, p->owner->nativeformats& AST_FORMAT_AUDIO_MASK),
            ast_getformatname_multiple(formats3,_STR_CODEC_SIZE,fmt ));

    /* update audio transcoders */
    //ast_set_read_format(p->owner, p->owner->readformat);
    //ast_set_write_format(p->owner, p->owner->writeformat);
    ast_set_write_format(p->owner, fmt);
    //ast_set_write_format(p->owner, p->owner->writeformat);
  }



  fmt2 =  (fmt & AST_FORMAT_VIDEO_MASK) ;
  if ( fmt2 > 0 )
  {
    oldNativeFormats = p->owner->nativeformats ;
    ast_mutex_lock(&p->lock);
    p->owner->nativeformats |= fmt2 ;
    ast_mutex_unlock(&p->lock);

    ast_log(LOG_DEBUG,"ooh323_set_write_format Update nativeformats (video) [%s] -> [%s]  and write format[%s]    \n", 
            ast_getformatname_multiple(formats1,_STR_CODEC_SIZE,oldNativeFormats& AST_FORMAT_VIDEO_MASK ),
            ast_getformatname_multiple(formats2,_STR_CODEC_SIZE, p->owner->nativeformats& AST_FORMAT_VIDEO_MASK),
            ast_getformatname_multiple(formats3,_STR_CODEC_SIZE,fmt ));
    ast_set_write_format(p->owner, fmt);
  }


  fmt2 =  (fmt & AST_FORMAT_TEXT_MASK) ;
  if ( fmt2 > 0 )
  {
    oldNativeFormats = p->owner->nativeformats ;
    ast_mutex_lock(&p->lock);
    p->owner->nativeformats |= fmt2 ;

    ast_mutex_unlock(&p->lock);
    ast_log(LOG_DEBUG,"ooh323_set_write_format Update nativeformats (text) [%s] -> [%s]  and write format[%s]    \n", 
            ast_getformatname_multiple(formats1,_STR_CODEC_SIZE,oldNativeFormats& AST_FORMAT_TEXT_MASK ),
            ast_getformatname_multiple(formats2,_STR_CODEC_SIZE, p->owner->nativeformats& AST_FORMAT_TEXT_MASK),
            ast_getformatname_multiple(formats3,_STR_CODEC_SIZE,fmt ));
    ast_set_write_format(p->owner, fmt);
  }
  ast_log(LOG_DEBUG,"<< ooh323_set_write_format nativeformats [%s] -> [%s]\n", 
          ast_getformatname_multiple(formats1,_STR_CODEC_SIZE,fmtOld ),
          ast_getformatname_multiple(formats2,_STR_CODEC_SIZE, p->owner->nativeformats));
#endif
}


void ooh323_set_read_format(ooCallData *call, int fmt)
{
  struct ooh323_pvt *p               = NULL;
  char              formats1[_STR_CODEC_SIZE]     = {0};
  char              formats2[_STR_CODEC_SIZE]     = {0};
  char              formats3[_STR_CODEC_SIZE]     = {0};
  int               fmt2             = 0 ;
  int               fmtOld           = 0 ;
  int               oldNativeFormats = 0 ;

  /* udpate native formats */
  if ( option_debug > 2 )
    ast_log(LOG_DEBUG,">> ooh323_set_read_format fmt=[%s]\n",  ast_getformatname_multiple(formats1,_STR_CODEC_SIZE, fmt) );


  p = find_call(call);
  if(!p)
  {
    ast_log(LOG_ERROR, "ooh323_set_read_format No matching call found for %s\n", call->callToken);
    return;
  }

  if ( !p->owner)
  {
    ast_log(LOG_ERROR, "ooh323_set_read_format p->owner null for call found for %s\n", call->callToken);
    return;
  }
  fmtOld = p->owner->nativeformats ;
  fmt2   =  (fmt & AST_FORMAT_AUDIO_MASK) ;
  if ( fmt2 > 0  ) 
  {
    oldNativeFormats = p->owner->nativeformats ;
    ast_mutex_lock(&p->lock);
    p->owner->nativeformats |= fmt2 ;
    ast_mutex_unlock(&p->lock);
 
    ast_log(LOG_DEBUG,"ooh323_set_read_format Update nativeformats (audio) [%s] -> [%s]  and read format[%s]    \n", 
            ast_getformatname_multiple(formats1,_STR_CODEC_SIZE,oldNativeFormats& AST_FORMAT_AUDIO_MASK ),
            ast_getformatname_multiple(formats2,_STR_CODEC_SIZE, p->owner->nativeformats& AST_FORMAT_AUDIO_MASK),
            ast_getformatname_multiple(formats3,_STR_CODEC_SIZE,fmt ));

    /* update audio transcoders */
    //ast_set_read_format(p->owner, p->owner->readformat);
    //ast_set_write_format(p->owner, p->owner->writeformat);
    ast_set_read_format(p->owner, fmt);
    //ast_set_write_format(p->owner, p->owner->writeformat);
  }



  fmt2 =  (fmt & AST_FORMAT_VIDEO_MASK) ;
  if ( fmt2 > 0 )
  {
    oldNativeFormats = p->owner->nativeformats ;
    ast_mutex_lock(&p->lock);
    p->owner->nativeformats |= fmt2 ;
    ast_mutex_unlock(&p->lock);

    ast_log(LOG_DEBUG,"ooh323_set_read_format Update nativeformats (video) [%s] -> [%s]  and read format[%s]    \n", 
            ast_getformatname_multiple(formats1,_STR_CODEC_SIZE,oldNativeFormats& AST_FORMAT_VIDEO_MASK ),
            ast_getformatname_multiple(formats2,_STR_CODEC_SIZE, p->owner->nativeformats& AST_FORMAT_VIDEO_MASK),
            ast_getformatname_multiple(formats3,_STR_CODEC_SIZE,fmt ));
    ast_set_read_format(p->owner, fmt);
  }


  fmt2 =  (fmt & AST_FORMAT_TEXT_MASK) ;
  if ( fmt2 > 0 )
  {
    oldNativeFormats = p->owner->nativeformats ;
    ast_mutex_lock(&p->lock);
    p->owner->nativeformats |= fmt2 ;

    ast_mutex_unlock(&p->lock);
    ast_log(LOG_DEBUG,"ooh323_set_read_format Update nativeformats (text) [%s] -> [%s]  and read format[%s]    \n", 
            ast_getformatname_multiple(formats1,_STR_CODEC_SIZE,oldNativeFormats& AST_FORMAT_TEXT_MASK ),
            ast_getformatname_multiple(formats2,_STR_CODEC_SIZE, p->owner->nativeformats& AST_FORMAT_TEXT_MASK),
            ast_getformatname_multiple(formats3,_STR_CODEC_SIZE,fmt ));
    ast_set_read_format(p->owner, fmt);
  }
  ast_log(LOG_DEBUG,"<< ooh323_set_read_format nativeformats [%s] -> [%s]\n", 
          ast_getformatname_multiple(formats1,_STR_CODEC_SIZE,fmtOld ),
          ast_getformatname_multiple(formats2,_STR_CODEC_SIZE, p->owner->nativeformats));
}

int onAlerting(ooCallData *call)
{
   struct ooh323_pvt *p=NULL;
   struct ast_channel *c = NULL;

   if(gH323Debug)
      ast_verbose("--- onAlerting %s\n", call->callToken);

   p = find_call(call);

   if(!p)
   {
      ast_log(LOG_ERROR, "No matching call found\n");
      return -1;
   }  
   ast_mutex_lock(&p->lock);
   if(!ast_test_flag(p, H323_OUTGOING))
   {
      c = ooh323_new(p, AST_STATE_RING, p->username);
      if(!c)
      {
         ast_mutex_unlock(&p->lock);
         ast_log(LOG_ERROR, "Could not create ast_channel\n");
         return -1;
      }
      ast_mutex_unlock(&p->lock);
   }
   else {
      if (!p->owner) {
         ast_mutex_unlock(&p->lock);
         ast_log(LOG_ERROR, "Channel has no owner\n");
         return 0;
      }
      c = p->owner;
      ast_mutex_unlock(&p->lock);
      ast_mutex_lock(&c->lock);
      ast_setstate(c, AST_STATE_RINGING);
      ast_mutex_unlock(&c->lock);
      ast_queue_control(c, AST_CONTROL_RINGING);
   }

   // IVeS : Send ConfID => p-CV
   // ooH323AstSetChargVectorID( call );
   

   if(gH323Debug)
      ast_verbose("+++ onAlerting %s\n", call->callToken);

   return OO_OK;
}

/**
  * Callback for sending digits from H.323 up to asterisk
  *
  */
int ooh323_onReceivedDigit(OOH323CallData *call, const char* digit)
{
   struct ooh323_pvt *p=NULL;
   struct ast_frame f;
   int res;

   if ( option_debug > 2 )
     ast_log(LOG_DEBUG, "Received Digit/Dtmf: %c\n", digit);
   p = find_call(call);
   if(!p) {
      ast_log(LOG_ERROR, "Failed to find a matching call.\n");
      return -1;
   }
   if(!p->owner) {
      ast_log(LOG_ERROR, "Channel has no owner\n");
      return -1;
   }
   ast_mutex_lock(&p->lock);
   memset(&f, 0, sizeof(f));
   f.frametype = AST_FRAME_DTMF;
   f.subclass = digit[0];
   f.datalen = 0;
   f.samples = 800;
   f.offset = 0;
   f.data = NULL;
   f.mallocd = 0;
   f.src = "SEND_DIGIT";
   ast_mutex_unlock(&p->lock);
   res = ast_queue_frame(p->owner, &f);
   return res;
}

int ooh323_onReceivedSetup(ooCallData *call, Q931Message *pmsg)
{
   struct ooh323_pvt *p=NULL;
   struct ooh323_user *user=NULL;
   ooAliases *alias=NULL;
   char *at=NULL;
   char number [OO_MAX_NUMBER_LENGTH];
   struct ast_channel* ast = NULL ;
   char cformats[_STR_CODEC_SIZE] = {0};

   if ( option_debug > 2 )
     ast_log(LOG_DEBUG,"ooh323_onReceivedSetup %s\n", call->callToken);

   p = ooh323_alloc(call->callReference, call->callToken);
   if(!p)
   {
      ast_log(LOG_ERROR, "Failed to create a new call.\n");
      return -1;
   }
   ast = p->owner;
   ast_mutex_lock(&p->lock);
   ast_clear_flag(p, H323_OUTGOING);

   // IVes : Init flag Codec
   ooh323_InitFlagCodec( call ) ;
  
   if ( ast ) 
   {
     if ( option_debug > 2 )
       ast_log(LOG_DEBUG,"ooh323_onReceivedSetup capability[%s] (%s)\n", 
               ast_getformatname_multiple(cformats,_STR_CODEC_SIZE, 
                                          ast->nativeformats),call->callToken);

   }

   if(call->remoteDisplayName){
      p->callerid_name = strdup(call->remoteDisplayName);
   }

   if(ooCallGetCallingPartyNumber(call, number, OO_MAX_NUMBER_LENGTH) == OO_OK)
   {
      p->callerid_num = strdup(number);
   }

   if(call->remoteAliases)
   {
      alias = call->remoteAliases;         
      while(alias)
      {      
         if(alias->type == T_H225AliasAddress_dialedDigits)
         {
            if(!p->callerid_num)
               p->callerid_num = strdup(alias->value);
            strncpy(p->caller_dialedDigits, alias->value, sizeof(p->caller_dialedDigits)-1);
         }
         else if(alias->type == T_H225AliasAddress_h323_ID)
         {
            if(!p->callerid_name){
               p->callerid_name = strdup(alias->value);
            }
            strncpy(p->caller_h323id, alias->value, sizeof(p->caller_h323id)-1);

            if (!p->callerid_num)
               p->callerid_num = strdup(alias->value);
         }  
         else if(alias->type == T_H225AliasAddress_url_ID)
         {
            strncpy(p->caller_url, alias->value, sizeof(p->caller_url)-1);
            if (!p->callerid_num)
               p->callerid_num = strdup(alias->value);
         }
         else if(alias->type == T_H225AliasAddress_email_ID)
         {
            strncpy(p->caller_email, alias->value, sizeof(p->caller_email)-1);
            if (!p->callerid_num)
               p->callerid_num = strdup(alias->value);
         }

         alias = alias->next;
      }
   }

   number[0]='\0';
   if(ooCallGetCalledPartyNumber(call, number, OO_MAX_NUMBER_LENGTH)== OO_OK)
   {
      strncpy(p->exten, number, sizeof(p->exten)-1);
   }
   else{
      update_our_aliases(call, p);
      if(!ast_strlen_zero(p->callee_dialedDigits)){
         strncpy(p->exten, p->callee_dialedDigits, sizeof(p->exten)-1);
      }
      else if(!ast_strlen_zero(p->callee_h323id)){
         strncpy(p->exten, p->callee_h323id, sizeof(p->exten)-1);
      }
      else if ( !ast_strlen_zero(p->callee_url) ){
          strncpy(p->exten, p->callee_url, sizeof(p->exten)-1);
      }
      else if(!ast_strlen_zero(p->callee_email)){
         strncpy(p->exten, p->callee_email, sizeof(p->exten)-1);
         if((at=strchr(p->exten, '@'))) {
            *at='\0';
         }
      }
   }

   /* if no extension found, set to default 's' */
   if(ast_strlen_zero(p->exten))
   {     
      p->exten[0]='s';
      p->exten[1]='\0';
   }

   if(!p->callerid_name)
   {
      p->callerid_name = strdup(call->remoteIP);
   }
   
   if(p->callerid_name)
   {
      user = find_user(p->callerid_name, call->remoteIP);
      if(user)
      {
         if(gH323Debug)
         ast_verbose("user found %s \n", user->name);

         ast_mutex_lock(&user->lock);
         p->username = strdup(user->name);
         strncpy(p->context, user->context, sizeof(p->context)-1);
         strncpy(p->accountcode, user->accountcode, sizeof(p->accountcode)-1);
         p->amaflags = user->amaflags;
         p->capability = user->capability;
         memcpy(&p->prefs, &user->prefs, sizeof(struct ast_codec_pref));
         p->dtmfmode = user->dtmfmode;
         /* Since, call is coming from a pbx user, no need to use gk */
         OO_SETFLAG(p->flags, H323_DISABLEGK);
         OO_SETFLAG(call->flags, OO_M_DISABLEGK);
         ast_mutex_unlock(&user->lock);
      }
      else
      {
         if (p->callerid_num == NULL)
         {
             char *unknown = "Unknown";
             ast_verbose("p->callerid_num=null, setting to %s\n", unknown);
             p->callerid_num = strdup(unknown);
         }
         ast_verbose("User not found username=callerid_num=%s\n", p->callerid_num);
         p->username = strdup(p->callerid_num);
      }
   }

   ooh323_replace_InvalidChar4SipUri(p->username);
   ooh323_replace_InvalidChar4SipUri(p->callerid_num);
   ast_log(LOG_DEBUG, "ooh323_onReceivedSetup - callerid_name[%s] username[%s] callerid_num[%s] call[%s]",
           p->callerid_name?p->callerid_name:"",p->username?p->username:"",p->callerid_num?p->callerid_num:"",p->exten?p->exten:"");

   /* IVeS - support du RINGING */
   OO_SETFLAG(call->flags,OO_M_MANUALRINGBACK);


#ifndef VIDEOCAPS
   /* in case of videocaps support, capabilites are on ooh323_set_capabilites()
    */
   ooh323c_set_capability_for_call(call, &p->prefs, p->capability,p->dtmfmode);
   configure_local_rtp(p, call);
#else
   if ( !strcmp(call->callType, "outgoing") )
   {
     if ( ! ast_test_flag(p, H323_CAPSCONFIGURED) )
     {
       ooh323c_set_capability_for_call(call, &p->prefs, p->capability,p->dtmfmode, &ast->channelcaps);
       configure_local_rtp(p, call);
       ast_set_flag(p,H323_CAPSCONFIGURED);
     }
     else
     {
       if ( option_debug > 2 )
         ast_log(LOG_DEBUG, "chan_ooh323 capabilities already configured for %s.\n", ast->name);
     }
   }
   else
   {
     configure_local_rtp(p, call);
   }
#endif

   ast_mutex_unlock(&p->lock);

   /* IVeS copy call Vector Id for futur use */
   memcpy(p->ChargVectorID,call->chargVectorID , OO_PCV_CID_SIZE );
   ast_set_flag(p, H323_HAVECHARGVECTOR);

   /* IVeS copy terminal Info */
   p->manufacturerCode = call->manufacturerCode ;
   memcpy(p->productId ,call->productId  ,OO_PRODUCT_ID_SIZE );
   memcpy(p->versionId ,call->versionId  ,OO_PRODUCT_ID_SIZE );
   ast_set_flag(p, H323_HAVETERMINFO);   

   /* IVeS : Limit burst of  fast update request */
   call->lastFUR = 0L ;

   if ( option_debug > 2 )
     ast_log(LOG_DEBUG, "ooh323_onReceivedSetup - Determined context %s, "
             "extension %s\n", p->context, p->exten);

   return OO_OK;
}

// IVeS : Miscellaneous
int ooh323_onReceivedMiscellaneous(  struct OOH323CallData *call, OOH323MiscCmd type ,void* data )
{
  int Status = TRUE ;

   if ( option_debug > 2 )
     ast_log(LOG_DEBUG, "Received H323 miscellaneous Command %d\n",type);

   switch (type)
   {
     case OO_MISC_CMD_VIDUPDATE:
       Status = ooh323_onReceivedVideoFastUdpate( call , data );
       break ;
     case OO_MISC_CMD_CAPABILITY:
       Status =ooh323_onReceivedCapability( call , data );
       break;
     case OO_MISC_CMD_UNKNOWN:
     default:
      ast_log(LOG_ERROR, "ooh323_onReceivedMiscellaneous: Unknown stack "
              "miscellaneous Command %d\n",type);
   }
  
  return Status ;
} 


// IVes : Add video fast update management
// Todo use data for gob info
int ooh323_onReceivedVideoFastUdpate(struct OOH323CallData *call, void * data)
{
   struct ooh323_pvt *p=NULL;
   int                res;
   time_t             currTime  = 0L ;
   char*              callToken = (char *)NULL;

   if ( option_debug > 2 )
     ast_log(LOG_DEBUG, "Received H323 VideoFastUpdate\n");

   p = find_call(call);   
   if(!p) 
   {
      ast_log(LOG_ERROR, "Failed to find a matching call.\n");
      return -1;
   }
   
   if(!p->owner) 
   {
      ast_log(LOG_ERROR, "Channel has no owner\n");
      return -1;
   }

   ast_mutex_lock(&p->lock);
   callToken = (p->callToken ? strdup(p->callToken) : NULL);
   ast_mutex_unlock(&p->lock);

   time(&currTime);
   if (ooconfig.mFURMinTime != 0  && 
       ( ( call->lastFUR + ooconfig.mFURMinTime) < currTime ) ||
       call->lastFUR > currTime )
   {
     res = ast_queue_control(p->owner,AST_CONTROL_VIDUPDATE); 
     if(callToken)
     {
       if ( option_debug > 2 )
         ast_log(LOG_DEBUG, "Temporary ACK for (real fix on plg) chan_ooh323 forward fast update request  \n");
       ooSendFastUpdateRequest( callToken , -1 , -1 );
     }
     call->lastFUR = currTime ;
   }
   else
   {
     if (  ooconfig.mFURMinTime )
     {
       ast_log(LOG_WARNING , "Ignoring video fast update request:last FUR < %ld s "
               "( last[%ld] curr[%ld] )\n", 
               ooconfig.mFURMinTime , call->lastFUR , currTime);
     }
     else
     {
        ast_log(LOG_WARNING , "Ignoring all video fast update request:config FURMinTime==0\n" );     
     }
   }
   return res;
}

int ooh323_onReceivedCapability(struct OOH323CallData *call, void * data)
{
   struct ooh323_pvt *p=NULL;
   int                res;
   struct ast_capabilities caps;
   if ( option_debug > 2 )
     ast_log(LOG_DEBUG, "Received H323 Capabilty\n");
   if( !data ) 
   {
      ast_log(LOG_ERROR, "Invalid data\n");
      return -1;
   }
   p = find_call(call);   
   if(!p) 
   {
      ast_log(LOG_ERROR, "Failed to find a matching call.\n");
      return -1;
   }
   
   if(!p->owner) 
   {
      ast_log(LOG_ERROR, "Channel has no owner\n");
      return -1;
   }

   // Capability check only if video h264 

   struct ast_channel *chan = p->owner ;
   struct ast_channel* bridged = ast_bridged_channel(chan);
   if ( bridged ){

  // Update chan 
     OOH264CapParams *params = (OOH264CapParams *)data;
     chan->channelcaps.maxvideobitrate = params->maxBitRate*100;
     chan->channelcaps.h264.profile =params->profile ; 
     chan->channelcaps.h264.level =params->level ;
     chan->channelcaps.h264.constraint =  params->constraint ;

     ast_copy_capabilities(&caps, &chan->channelcaps);
     if ( ast_get_negociated_caps(chan, &caps) == 0 ) {
       ast_log(LOG_DEBUG, "Call to ast_get_negociated_caps, end point[%d] bridged[%d]\n",
               chan->channelcaps.maxvideobitrate ,bridged->channelcaps.maxvideobitrate);
       ast_indicate_data(bridged, AST_CONTROL_CAPABILITY, &caps, sizeof(caps));
     }
   }

   return res;
}

int onNewCallCreated(ooCallData *call)
{
   struct ooh323_pvt *p = NULL;
   int i=0;
   struct ast_channel* ast = NULL ;
  if ( option_debug > 2 )
    ast_log(LOG_DEBUG,">> [H323] onNewCallCreated (%s, %s)\n", call->callType, call->callToken );

   if(!strcmp(call->callType, "outgoing"))
   {
      p = find_call(call);
      if(!p)
      {
         ast_log(LOG_ERROR, "No matching call found for outgoing call\n");
         return -1;
      }
      ast_mutex_lock(&p->lock);
      if(p->callerid_name){
         ooCallSetCallerId(call, p->callerid_name);
      }
      if(p->callerid_num)
      {
         i=0;
         while(*(p->callerid_num+i) != '\0')
         {
            if(!isdigit(*(p->callerid_num+i))) { break; }
            i++;
         }
         if(*(p->callerid_num+i) == '\0')
            ooCallSetCallingPartyNumber(call, p->callerid_num, 1,gVp200Info );
         else{
            if(!p->callerid_name)
               ooCallSetCallerId(call, p->callerid_num);
         }
      }
      if ( p->owner ){
        ast = p->owner ;
      }
      ooCallSetBearerCapabilityCircuitMode(call,gBearerCapabilityMode);
      
      // H323-ID
      if(!ast_strlen_zero(p->caller_h323id)){
        ooCallAddAliasH323ID(call, p->caller_h323id);
      }else if ( !ast_strlen_zero(p->caller_dialedDigits)){
         ooCallAddAliasH323ID(call,  p->caller_dialedDigits);
      }
      else if  ( !ast_strlen_zero(p->callerid_name) ){
         ooCallAddAliasH323ID(call, p->callerid_name);
      }else if (!ast_strlen_zero(p->callerid_num) ) {
        if(!ast_strlen_zero(p->callerid_num) && ooIsDailedDigit(p->callerid_num)) {
          ooCallAddAliasH323ID(call, p->callerid_num);
        }
      }

      // DialedDigits
      if(!ast_strlen_zero(p->caller_dialedDigits)){
         if(gH323Debug) {
            ast_verbose("Setting dialed digits %s\n", p->caller_dialedDigits);
         }
         ooCallAddAliasDialedDigits(call, p->caller_dialedDigits);
      }
      else if(p->callerid_num) {
         if(ooIsDailedDigit(p->callerid_num)) {
            if(gH323Debug) {
               ast_verbose("setting callid number %s\n", p->callerid_num);
            }
            ooCallAddAliasDialedDigits(call, p->callerid_num);
         }
      }


      if(!ast_strlen_zero(p->exten))  
      {
        if(ooIsDailedDigit(p->exten)) {
          ooCallSetCalledPartyNumber(call, p->exten);
          ooCallAddRemoteAliasDialedDigits(call, p->exten);
        }
        else {
          ooCallAddRemoteAliasH323ID(call, p->exten);
        }
      }



      // fuck facility AlternativeAdresse !!!
      if ( ! ast_test_flag(p, H323_CAPSCONFIGURED))
      {
        if ( ast )
        {
          ooh323c_set_capability_for_call(call, &p->prefs, p->capability,p->dtmfmode, &ast->channelcaps);
          configure_local_rtp(p, call);
          ast_set_flag(p, H323_CAPSCONFIGURED);
        }
        else
        {
          ast_log(LOG_WARNING, "onNewCallCreated owner not found !!! .\n");
        }
      }
      else
      {
        ast_log(LOG_WARNING, "onNewCallCreated Capabilites already configured for chan %s.\n", ast->name);
        struct sockaddr_in us;
        struct sockaddr_in vus;
        OOMediaInfo mediaInfo;
        OOMediaInfo vmediaInfo;
        int x, format=0;     
        char cformats[_STR_CODEC_SIZE] = {0};
        if (p->rtp)
        {
          ast_rtp_get_us(p->rtp, &us);
          if ( strlen(gPublicIP) != 0 )
            strncpy(mediaInfo.lMediaIP, gPublicIP, sizeof(mediaInfo.lMediaIP)-1);
          else
            strncpy(mediaInfo.lMediaIP, ast_inet_ntoa(us.sin_addr), sizeof(mediaInfo.lMediaIP)-1);
          mediaInfo.lMediaPort = (int)ntohs(us.sin_port);

          mediaInfo.lMediaCntrlPort = mediaInfo.lMediaPort +1;

          for(x=0; 0 != (format=ast_codec_pref_index(&p->prefs, x)); x++)
          {
            if (format & AST_FORMAT_AUDIO_MASK)
            {
              strcpy(mediaInfo.dir, "transmit");
              mediaInfo.cap = ooh323_convertAsteriskCapToH323Cap(format& AST_FORMAT_AUDIO_MASK);
              mediaInfo.lMediaPort = (int)ntohs(us.sin_port);
              mediaInfo.lMediaCntrlPort = mediaInfo.lMediaPort +1;
              if ( option_debug > 2 )
                ast_log(LOG_DEBUG,"[H323/RTP] configure_local_rtp :Open Audio RTP recv [%s] ==> %s:%d (%s, %s)\n",
                        ast_getformatname_multiple(cformats,_STR_CODEC_SIZE,format & AST_FORMAT_AUDIO_MASK ),
                        mediaInfo.lMediaIP , mediaInfo.lMediaPort, call->callType, call->callToken );

              if ( mediaInfo.cap != -1 )
              {
                ooAddMediaInfo(call, mediaInfo);
                strcpy(mediaInfo.dir, "receive");
                ooAddMediaInfo(call, mediaInfo);
              }
            }
          }
        }
        if (p->vrtp)
        {
          int vformats[] = { AST_FORMAT_H263, AST_FORMAT_H264 };

          ast_rtp_get_us(p->vrtp, &vus);
          if ( strlen(gPublicIP) != 0 )
            strncpy(vmediaInfo.lMediaIP, gPublicIP, sizeof(vmediaInfo.lMediaIP)-1);
          else
            strncpy(vmediaInfo.lMediaIP, ast_inet_ntoa(vus.sin_addr), sizeof(vmediaInfo.lMediaIP)-1);
          vmediaInfo.lMediaPort =(int) ntohs(vus.sin_port);
          vmediaInfo.lMediaCntrlPort = vmediaInfo.lMediaPort +1;
 
          /* figure out our local RTP port and tell the H.323 stack about it*/
          for(x=0; x < 2; x++)
          {
            if (p->capability & vformats[x])
            {

              if ( option_debug > 2 )
                ast_log(LOG_DEBUG,"[H323/RTP] configure_local_rtp :Open Video RTP recv [%s] ==> %s:%d (%s, %s)\n",
                        ast_getformatname_multiple(cformats,_STR_CODEC_SIZE,vformats[x]  ),
                        vmediaInfo.lMediaIP ,vmediaInfo.lMediaPort , call->callType, call->callToken );
              // Dynamic paylaod :  ast_rtp_set_m_type(p->vrtp, payload);
              format = vformats[x];
              strcpy(vmediaInfo.dir, "transmit");
              vmediaInfo.cap = ooh323_convertAsteriskCapToH323Cap(format);
              vmediaInfo.rtpPayloadType = ast_rtp_lookup_code(p->vrtp, 1, format);

              if ( vmediaInfo.cap != -1)
              {
                ooAddMediaInfo(call, vmediaInfo);
                strcpy(vmediaInfo.dir, "receive");
                vmediaInfo.rtpPayloadType = 0;
                ooAddMediaInfo(call, vmediaInfo);
              }
            }
          }
        }

      }
      ast_mutex_unlock(&p->lock);
      if(gH323Debug)
      {
        char prefsBuf[256];
        ast_codec_pref_string(&p->prefs, prefsBuf, 256);
        ast_verbose(" Outgoing call %s(%s) - Codec prefs - %s\n", 
                    p->username?p->username:"NULL", call->callToken, prefsBuf);
      }
   }

    ast_log(LOG_DEBUG,"<<[H323] onNewCallCreated (%s, %s)\n",
            call->callType, call->callToken );

   return OO_OK;
}

int onCallEstablished(ooCallData *call)
{
  struct ooh323_pvt *p = NULL;


  if ( option_debug > 2 )
    ast_log(LOG_DEBUG,"[H323] =======  OnCallEstablished (%s, %s) ============== \n",
            call->callType, call->callToken );

   p = find_call(call);
   if(!p) {
      ast_log(LOG_ERROR, "Failed to find a matching call.\n");
      return -1;
   }
   ast_mutex_lock(&p->lock);
   if (!p->owner) {
      ast_mutex_unlock(&p->lock);
      ast_log(LOG_ERROR, "Channel has no owner\n");
      return -1;
   }
   
   while(ast_mutex_trylock(&p->owner->lock))
   {
     if ( option_debug > 2 )
       ast_log(LOG_DEBUG,"onCallEstablished: Failed to grab lock, trying again\n");
      ast_mutex_unlock(&p->lock);
      usleep(1);
      ast_mutex_lock(&p->lock);
   }    
   if(p->owner->_state != AST_STATE_UP)
   {
      ast_setstate(p->owner, AST_STATE_UP);
   }
   ast_mutex_unlock(&p->owner->lock);
   if(ast_test_flag(p, H323_OUTGOING)) {
      struct ast_channel* c = p->owner;
      ast_mutex_unlock(&p->lock);
      ast_queue_control(c, AST_CONTROL_ANSWER);
   }
   else { ast_mutex_unlock(&p->lock); }

  return OO_OK;
}

int onCallCleared(ooCallData *call)
{
   struct ooh323_pvt *p = NULL;
   int ownerLock = 0;

   if(gH323Debug)
      ast_verbose("---   onCallCleared %s \n", call->callToken);

   p = find_call(call); 
   if (!p) { return 0; }
   ast_mutex_lock(&p->lock);
  
   while(p->owner) {
      if(ast_mutex_trylock(&p->owner->lock)) {

        if ( option_debug > 2 )
          ast_log(LOG_DEBUG,"onCallCleared: Failed to grab lock, trying again\n");
         ast_mutex_unlock(&p->lock);
         usleep(1);
         ast_mutex_lock(&p->lock);
      }
      else {
         ownerLock = 1; break;
      }
   }

   if(ownerLock) {
      if(!ast_test_flag(p, H323_ALREADYGONE)) { 

         /* NOTE: Channel is not detached yet */
         ast_set_flag(p, H323_ALREADYGONE);
         p->owner->hangupcause = 
            ooh323_convert_hangupcause_h323ToAsterisk(call->callEndReason);
         p->owner->_softhangup |= AST_SOFTHANGUP_DEV;
         ast_mutex_unlock(&p->owner->lock);
         ast_queue_hangup(p->owner);
         ast_mutex_unlock(&p->lock);
         return OO_OK;
      }
      ast_mutex_unlock(&p->owner->lock);
   }
   ast_set_flag(p, H323_NEEDDESTROY);
   ast_mutex_unlock(&p->lock);

   if(gH323Debug)
      ast_verbose("+++   onCallCleared\n");

   return OO_OK;
}

static void ooh323_delete_user(struct ooh323_user *user)
{
   struct ooh323_user *prev=NULL, *cur=NULL;

   if(gH323Debug)
      ast_verbose("---   ooh323_delete_user\n");

   if(user)
   {   
      cur = userl.users;
      ast_mutex_lock(&userl.lock);
      while(cur)
      {
         if(cur==user) break;
         prev = cur;
         cur = cur->next;
      }

      if(cur)
      {
         if(prev)
            prev->next = cur->next;
         else
            userl.users = cur->next;
      }
      ast_mutex_unlock(&userl.lock);

      free(user);
   }  

   if(gH323Debug)
      ast_verbose("+++   ooh323_delete_user\n");

}

void ooh323_delete_peer(struct ooh323_peer *peer)
{
   struct ooh323_peer *prev=NULL, *cur=NULL;

   if(gH323Debug)
      ast_verbose("---   ooh323_delete_peer\n");

   if(peer)
   {   
      cur = peerl.peers;
      ast_mutex_lock(&peerl.lock);
      while(cur)
      {
         if(cur==peer) break;
         prev = cur;
         cur = cur->next;
      }

      if(cur)
      {
         if(prev)
            prev->next = cur->next;
         else
            peerl.peers = cur->next;
      }
      ast_mutex_unlock(&peerl.lock);

      if(peer->h323id)   free(peer->h323id);
      if(peer->email)    free(peer->email);
      if(peer->url)      free(peer->url);
      if(peer->e164)     free(peer->e164);

      free(peer);
   }  

   if(gH323Debug)
      ast_verbose("+++   ooh323_delete_peer\n");

}



static struct ooh323_user *build_user(const char *name, struct ast_variable *v)
{
   struct ooh323_user *user=NULL;

   if(gH323Debug)
      ast_verbose("---   build_user\n");

   user = (struct ooh323_user *)malloc(sizeof(struct ooh323_user));
   if (user) {
      memset(user, 0, sizeof(struct ooh323_user));
      ast_mutex_init(&user->lock);
      strncpy(user->name, name, sizeof(user->name)-1);
      user->capability = gCapability;
      memcpy(&user->prefs, &gPrefs, sizeof(struct ast_codec_pref));
      user->rtptimeout = gRTPTimeout;
      user->dtmfmode = gDTMFMode;
      /* set default context */
      strncpy(user->context, gContext, sizeof(user->context)-1);
      strncpy(user->accountcode, gAccountcode, sizeof(user->accountcode)-1);
      user->amaflags = gAMAFLAGS;

      while(v) {
         if (!strcasecmp(v->name, "context")) {
            strncpy(user->context, v->value, sizeof(user->context) - 1);
         }
         else if (!strcasecmp(v->name, "incominglimit")) {
            user->incominglimit = atoi(v->value);
            if (user->incominglimit < 0)
               user->incominglimit = 0;
         }
         else if (!strcasecmp(v->name, "accountcode")) {
            strncpy(user->accountcode, v->value, sizeof(user->accountcode)-1);
         }
         else if (!strcasecmp(v->name, "rtptimeout")) {
            user->rtptimeout = atoi(v->value);
            if(user->rtptimeout < 0)
               user->rtptimeout = gRTPTimeout;
         }
         else if (!strcasecmp(v->name, "disallow")) {
            ast_parse_allow_disallow(&user->prefs, &user->capability, 
                                     v->value, 0);
         }
         else if (!strcasecmp(v->name, "allow")) {
            const char* tcodecs = v->value;
            if(!strcasecmp(v->value, "all")) {
               tcodecs = "ulaw,alaw,g729,g723,gsm";
            }
	    ast_log(LOG_NOTICE, "user %s pref allow = %s\n", user->name, tcodecs );
            ast_parse_allow_disallow(&user->prefs, &user->capability, 
                                     tcodecs, 1);
         }
         else if (!strcasecmp(v->name, "amaflags")) {
            user->amaflags = ast_cdr_amaflags2int(v->value);
         }
         else if (!strcasecmp(v->name, "ip")) {
            strncpy(user->mIP, v->value, sizeof(user->mIP)-1);
            user->mUseIP = 1;
         }
         else if (!strcasecmp(v->name, "dtmfmode")) {
            if(!strcasecmp(v->value, "rfc2833"))
               user->dtmfmode = H323_DTMF_RFC2833;
            else if(!strcasecmp(v->value, "q931keypad"))
               user->dtmfmode = H323_DTMF_Q931;
            else if(!strcasecmp(v->value, "h245alphanumeric"))
               user->dtmfmode = H323_DTMF_H245ALPHANUMERIC;
            else if(!strcasecmp(v->value, "h245signal"))
               user->dtmfmode = H323_DTMF_H245SIGNAL;
         } 
         v = v->next;
      }
   }

   if(gH323Debug)
      ast_verbose("+++   build_user\n");

   return user;
}

static struct ooh323_peer *build_peer(const char *name, struct ast_variable *v, int friend_type)
{
   struct ooh323_peer *peer=NULL;

   if(gH323Debug)
      ast_verbose("---   build_peer\n");

   peer = (struct ooh323_peer *)malloc(sizeof(struct ooh323_peer));
   if (peer) {
      memset(peer, 0, sizeof(struct ooh323_peer));
      ast_mutex_init(&peer->lock);
      strncpy(peer->name, name, sizeof(peer->name)-1);
      peer->capability = gCapability;
      memcpy(&peer->prefs, &gPrefs, sizeof(struct ast_codec_pref));
      peer->rtptimeout = gRTPTimeout;
      strncpy(peer->accountcode, gAccountcode, sizeof(peer->accountcode)-1);
      peer->amaflags = gAMAFLAGS;
      peer->dtmfmode = gDTMFMode;
      if(0 == friend_type) {
         peer->mFriend = 1;
      }

      while(v) {
         if (!strcasecmp(v->name, "h323id")) {
            peer->h323id = (char*)malloc(strlen(v->value)+1);
            if(!peer->h323id)
            {
               ast_log(LOG_ERROR, "Could not allocate memory for h323id of "
                                  "peer %s\n", name);
               ooh323_delete_peer(peer);
               return NULL;
            }
            strcpy(peer->h323id, v->value);
         } 
         else if (!strcasecmp(v->name, "e164")) {
            peer->e164 = (char*)malloc(strlen(v->value)+1);
            if(!peer->e164)
            {
               ast_log(LOG_ERROR, "Could not allocate memory for e164 of "
                                  "peer %s\n", name);
               ooh323_delete_peer(peer);
               return NULL;
            }
            strcpy(peer->e164, v->value);
         } 
         else  if (!strcasecmp(v->name, "email")) {
            peer->email = (char*)malloc(strlen(v->value)+1);
            if(!peer->email)
            {
               ast_log(LOG_ERROR, "Could not allocate memory for email of "
                                  "peer %s\n", name);
               ooh323_delete_peer(peer);
               return NULL;
            }
            strcpy(peer->email, v->value);
         } 
         else if (!strcasecmp(v->name, "url")) {
            peer->url = (char*)malloc(strlen(v->value)+1);
            if(!peer->url)
            {
               ast_log(LOG_ERROR, "Could not allocate memory for h323id of "
                                  "peer %s\n", name);
               ooh323_delete_peer(peer);
               return NULL;
            }
            strcpy(peer->url, v->value);
         }
         else if (!strcasecmp(v->name, "port")) {
            peer->port = atoi(v->value);
         } 
         else if (!strcasecmp(v->name, "ip")) {
            strncpy(peer->ip, v->value, sizeof(peer->ip)-1);
         } 
         else if (!strcasecmp(v->name, "outgoinglimit")) {
            peer->outgoinglimit = atoi(v->value);
            if (peer->outgoinglimit < 0)
               peer->outgoinglimit = 0;
         }
         else if (!strcasecmp(v->name, "accountcode")) {
            strncpy(peer->accountcode, v->value, sizeof(peer->accountcode)-1);
         }
         else if (!strcasecmp(v->name, "rtptimeout")) {
            peer->rtptimeout = atoi(v->value);
            if(peer->rtptimeout < 0)
               peer->rtptimeout = gRTPTimeout;
         }
         else if (!strcasecmp(v->name, "disallow")) {
            ast_parse_allow_disallow(&peer->prefs, &peer->capability, 
                                     v->value, 0); 
         }
         else if (!strcasecmp(v->name, "allow")) {
            const char* tcodecs = v->value;
            if(!strcasecmp(v->value, "all")) {
               tcodecs = "ulaw,alaw,g729,g723,gsm,h263,h264";
            }
	    ast_log(LOG_NOTICE, "peer %s pref allow = %s\n", peer->name, tcodecs );
            ast_parse_allow_disallow(&peer->prefs, &peer->capability, 
                                     tcodecs, 1);             
         }
         else if (!strcasecmp(v->name,  "amaflags")) {
            peer->amaflags = ast_cdr_amaflags2int(v->value);
         }
         else if (!strcasecmp(v->name, "dtmfmode")) {
            if(!strcasecmp(v->value, "rfc2833"))
               peer->dtmfmode = H323_DTMF_RFC2833;
            else if(!strcasecmp(v->value, "q931keypad"))
               peer->dtmfmode = H323_DTMF_Q931;
            else if(!strcasecmp(v->value, "h245alphanumeric"))
               peer->dtmfmode = H323_DTMF_H245ALPHANUMERIC;
            else if(!strcasecmp(v->value, "h245signal"))
               peer->dtmfmode = H323_DTMF_H245SIGNAL;
         }
         v = v->next;
      }
   }

   if(gH323Debug)
      ast_verbose("+++   build_peer\n");

   return peer;
}

static int ooh323_do_reload(void)
{
   if(gH323Debug) {
      ast_verbose("---   ooh323_do_reload\n");
   }

   delete_users();
   delete_peers();
   reload_config();

   if(gH323Debug) {
      ast_verbose("+++   ooh323_do_reload\n");
   }

   return 0;
}

/*--- h323_reload: Force reload of module from cli ---*/
static int ooh323_reload(int fd, int argc, char *argv[])
{

   if(gH323Debug)
      ast_verbose("---   ooh323_reload\n");

   ast_mutex_lock(&h323_reload_lock);
   if (h323_reloading) {
      ast_verbose("Previous OOH323 reload not yet done\n");
   } 
   else {
      h323_reloading = 1;
   }
   ast_mutex_unlock(&h323_reload_lock);
   restart_monitor();

   if(gH323Debug)
      ast_verbose("+++   ooh323_reload\n");

   return 0;
}

static int reload(void *mod)
{
   return ooh323_reload(0, 0, NULL);
}

int reload_config(void)
{
   int format;
   struct ooAliases  *pNewAlias = NULL;
   struct ast_config *cfg;
   struct ast_variable *v;
   struct ooh323_user *user = NULL;
   struct ooh323_peer *peer = NULL;
   char *cat;
   const char *utype;

   if(gH323Debug)
      ast_verbose("---   reload_config\n");

   cfg = ast_config_load(config);

   /* We *must* have a config file otherwise stop immediately */
   if (!cfg) {
      ast_log(LOG_NOTICE, "Unable to load config %s, OOH323 disabled\n",config);
      return 1;
   }

   /* Inintialize everything to default */
   strcpy(gLogFile, DEFAULT_TRACEFILE);
   gPort = 1720;
   gIP[0]='\0';
   gPublicIP[0]='\0';
   strncpy(gCallerID, DEFAULT_H323ID,AST_MAX_EXTENSION);
   strncpy(gProductID, DEFAULT_PRODUCTID,OO_PRODUCT_ID_SIZE);
   strncpy(gVersionID, OOH323C_VERSION,OO_PRODUCT_ID_SIZE);
   gCapability = AST_FORMAT_ULAW;
   memset(&gPrefs, 0, sizeof(struct ast_codec_pref));
   gDTMFMode = H323_DTMF_RFC2833|H323_DTMF_INBAND|H323_DTMF_RFC2833| H323_DTMF_Q931|H323_DTMF_H245ALPHANUMERIC|H323_DTMF_H245SIGNAL;;
   gRasGkMode = RasNoGatekeeper;
   gGatekeeper[0] = '\0';
   gRTPTimeout = 60;
   strcpy(gAccountcode, DEFAULT_H323ACCNT);
   gFastStart = 0;
   gTunneling = 0;
   gVp200Info= 0 ;
   gBearerCapabilityMode=Q931TransferCircuitMode;
   gTOS = 0;
   strcpy(gContext, DEFAULT_CONTEXT);
   gAliasList = NULL;
   gMediaWaitForConnect = 0;
   ooconfig.mTCPPortStart = TCPPORTSSTART;
   ooconfig.mTCPPortEnd = TCPPORTSEND;
   ooconfig.mUDPPortStart = UDPPORTSSTART;
   ooconfig.mUDPPortEnd = UDPPORTSEND;
   ooconfig.mRTPPortStart = RTPPORTSSTART;
   ooconfig.mRTPPortEnd = RTPPORTSEND;
   ooconfig.mFURMinTime = _TIME_TO_SEND_FUR ;

   v = ast_variable_browse(cfg, "general");
   while(v) {
   
      if (!strcasecmp(v->name, "port")) {
         gPort = (int)strtol(v->value, NULL, 10);
      } 
      else if (!strcasecmp(v->name, "bindaddr")) {
         strncpy(gIP, v->value, sizeof(gIP)-1);
      } 
      else if (!strcasecmp(v->name, "publicIp")) {
         strncpy(gPublicIP, v->value, sizeof(gPublicIP)-1);
      } 
      else if ((!strcasecmp(v->name, "h225portrange"))||!strcasecmp(v->name, "H245PortRange")) {
         char* endlimit = 0;
         char temp[512];
         strncpy(temp, v->value, sizeof(temp) - 1);	
      	/* char *temp = ast_strdupa(v->value); */
         endlimit = strchr(temp, ',');
         if (endlimit) {
            *endlimit = '\0';
            endlimit++;
            ooconfig.mTCPPortStart = atoi(temp);
            ooconfig.mTCPPortEnd = atoi(endlimit);

            if(ooH323EpSetTCPPortRange(ooconfig.mTCPPortStart, 
                                       ooconfig.mTCPPortEnd) == OO_FAILED) {
               ast_log(LOG_ERROR, "h225portrange: Failed to set range\n");
            }
         } 
         else {
            ast_log(LOG_ERROR, "h225portrange: Invalid format, separate port range with \",\"\n");
         }
      }
      else if (!strcasecmp(v->name, "GKportrange")) {
         char* endlimit = 0;
         char temp[512];
         strncpy(temp, v->value, sizeof(temp) - 1);	
      	/* char *temp = ast_strdupa(v->value); */
         endlimit = strchr(temp, ',');
         if (endlimit) {
            *endlimit = '\0';
            endlimit++;
            ooconfig.mUDPPortStart = atoi(temp);
            ooconfig.mUDPPortEnd = atoi(endlimit);

            if(ooH323EpSetUDPPortRange(ooconfig.mUDPPortStart, 
                                       ooconfig.mUDPPortEnd) == OO_FAILED) {
               ast_log(LOG_ERROR, "GKportrange: Failed to set range\n");
            }
         } 
         else {
            ast_log(LOG_ERROR, "GKportrange: Invalid format, separate port range with \",\"\n");
         }
      }
      else if (!strcasecmp(v->name, "rtpportrange")) {
         char* endlimit = 0;
         char temp[512];
         strncpy(temp, v->value, sizeof(temp) - 1);	
      	/* char *temp = ast_strdupa(v->value); */
         endlimit = strchr(temp, ',');
         if (endlimit) {
            *endlimit = '\0';
            endlimit++;
            ooconfig.mRTPPortStart = atoi(temp);
            ooconfig.mRTPPortEnd = atoi(endlimit);

            if(ooH323EpSetRTPPortRange(ooconfig.mRTPPortStart, 
                                       ooconfig.mRTPPortEnd) == OO_FAILED) {
               ast_log(LOG_ERROR, "rtpportrange: Failed to set range\n");
            }
         } 
         else {
            ast_log(LOG_ERROR, "rtpportrange: Invalid format, separate port range with \",\"\n");
         }
      }
      else if (!strcasecmp(v->name, "FURMinTime")) 
      {
         char temp[512];
         int  iTemp = 0 ;
         strncpy(temp, v->value, sizeof(temp) - 1);	
         iTemp = atoi(temp);
         if ( (iTemp <= _MAX_TIME_TO_SEND_FUR ) && (iTemp >= _MIN_TIME_TO_SEND_FUR ))
         {
           ooconfig.mFURMinTime = atoi(temp);
           ast_log(LOG_WARNING, "Min time between two video fast update : %ds\n", ooconfig.mFURMinTime);
         } 
         else 
         {
           ast_log(LOG_ERROR, "FURMinTime: Invalid value %d ( min=%d , max=%d) \n",
                   iTemp ,_MIN_TIME_TO_SEND_FUR  , _MAX_TIME_TO_SEND_FUR  );
         }
      }
      else if (!strcasecmp(v->name, "gateway")) {
         gIsGateway = ast_true(v->value);
      } 
      else if (!strcasecmp(v->name, "ssrcFix")) {
         gssrcfix = ast_true(v->value);
      } 
      else if (!strcasecmp(v->name, "msdmode")) {
        ooH245setMsdMode(v->value);
      } 
      else if (!strcasecmp(v->name, "CheckOLC")) {
        ooH245SetCheckOLC(ast_true(v->value));
      } 
      else if (!strcasecmp(v->name, "termtype")) {
        ooH323EpSetTermType( atoi(v->value));
      } 
      else if (!strcasecmp(v->name, "faststart")) {
         gFastStart = ast_true(v->value);
         if(gFastStart)
            ooH323EpEnableFastStart();
         else
            ooH323EpDisableFastStart();
      } 
      else if (!strcasecmp(v->name, "mediawaitforconnect")) {
         gMediaWaitForConnect = ast_true(v->value);
         if(gMediaWaitForConnect)
            ooH323EpEnableMediaWaitForConnect();
         else 
            ooH323EpDisableMediaWaitForConnect();
      } 
      else if (!strcasecmp(v->name, "h245tunneling")) {
         gTunneling = ast_true(v->value);
         if(gTunneling)
            ooH323EpEnableH245Tunneling();
         else
            ooH323EpDisableH245Tunneling();
      }
      else if (!strcasecmp(v->name, "addVp200Info")) {
         gVp200Info = ast_true(v->value);
      } 
      else if (!strcasecmp(v->name, "BearerCapabilityCircuitMode" )){
        gBearerCapabilityMode= ast_true(v->value)?Q931TransferCircuitMode:Q931TransferPacketMode ;
      }
      else if (!strcasecmp(v->name, "h323id")) {
         pNewAlias = malloc(sizeof(struct ooAliases));
         if(!pNewAlias){
            ast_log(LOG_ERROR, "Failed to allocate memory for h323id alias\n");
            return 1;
         }
         pNewAlias->type =  T_H225AliasAddress_h323_ID;
         pNewAlias->value = strdup(v->value);
         pNewAlias->next = gAliasList;
         gAliasList = pNewAlias;
         pNewAlias = NULL;
      }
      else if (!strcasecmp(v->name, "e164")) {
         pNewAlias = malloc(sizeof(struct ooAliases));
         if(!pNewAlias){
            ast_log(LOG_ERROR, "Failed to allocate memory for e164 alias\n");
            return 1;
         }
         pNewAlias->type =  T_H225AliasAddress_dialedDigits;
         pNewAlias->value = strdup(v->value);
         pNewAlias->next = gAliasList;
         gAliasList = pNewAlias;
         pNewAlias = NULL;
      }
      else if (!strcasecmp(v->name, "email")) {
         pNewAlias = malloc(sizeof(struct ooAliases));
         if(!pNewAlias){
            ast_log(LOG_ERROR, "Failed to allocate memory for email alias\n");
            return 1;
         }
         pNewAlias->type =  T_H225AliasAddress_email_ID;
         pNewAlias->value = strdup(v->value);
         pNewAlias->next = gAliasList;
         gAliasList = pNewAlias;
         pNewAlias = NULL;
      }
      else if (!strcasecmp(v->name, "callerid")) {
         strncpy(gCallerID, v->value, sizeof(gCallerID)-1);
      }
      else if (!strcasecmp(v->name, "H225_productid")) {
         strncpy(gProductID, v->value, sizeof(gProductID)-1);
      }
      else if (!strcasecmp(v->name, "H225_versionid")) {
         strncpy(gVersionID, v->value, sizeof(gVersionID)-1);
      }
      else if (!strcasecmp(v->name, "H225_manufacturercode")) {
        gManufacturerCode = atoi(v->value);
      }      
      else if (!strcasecmp(v->name, "incominglimit")) {
         gIncomingLimit = atoi(v->value);
      }
      else if (!strcasecmp(v->name, "outgoinglimit")) {
         gOutgoingLimit = atoi(v->value);
      }
      else if (!strcasecmp(v->name, "gatekeeper")) {
         if (!strcasecmp(v->value, "DISABLE")) {
            gRasGkMode = RasNoGatekeeper;
         } 
         else if (!strcasecmp(v->value, "DISCOVER")) {
            gRasGkMode = RasDiscoverGatekeeper;
         } 
         else {
            gRasGkMode = RasUseSpecificGatekeeper;
            strncpy(gGatekeeper, v->value, sizeof(gGatekeeper)-1);
         }
      }
      else if (!strcasecmp(v->name, "logfile")) {
         strncpy(gLogFile, v->value, sizeof(gLogFile)-1);
      }
      else if (!strcasecmp(v->name, "context")) {
         strncpy(gContext, v->value, sizeof(gContext)-1);
         ast_verbose(VERBOSE_PREFIX_3 "  == Setting default context to %s\n", 
                                                      gContext);
      }
      else if (!strcasecmp(v->name, "rtptimeout")) {
         gRTPTimeout = atoi(v->value);
         if(gRTPTimeout <= 0)
            gRTPTimeout = 60;
      }
      else if (!strcasecmp(v->name, "tos")) {
         if (sscanf(v->value, "%i", &format) == 1)
            gTOS = format & 0xff;
         else if (!strcasecmp(v->value, "lowdelay"))
            gTOS = IPTOS_LOWDELAY;
         else if (!strcasecmp(v->value, "throughput"))
            gTOS = IPTOS_THROUGHPUT;
         else if (!strcasecmp(v->value, "reliability"))
            gTOS = IPTOS_RELIABILITY;
         else if (!strcasecmp(v->value, "mincost"))
            gTOS = IPTOS_MINCOST;
         else if (!strcasecmp(v->value, "none"))
            gTOS = 0;
         else
            ast_log(LOG_WARNING, "Invalid tos value at line %d, should be "
                                 "'lowdelay', 'throughput', 'reliability', "
                                 "'mincost', or 'none'\n", v->lineno);
      }
      else if(!strcasecmp(v->name, "amaflags")) {
         gAMAFLAGS = ast_cdr_amaflags2int(v->value);
      }
      else if(!strcasecmp(v->name, "accountcode")) {
         strncpy(gAccountcode, v->value, sizeof(gAccountcode)-1);
      }
      else if (!strcasecmp(v->name, "disallow")) {
         ast_parse_allow_disallow(&gPrefs, &gCapability, v->value, 0);
      }
      else if (!strcasecmp(v->name, "allow")) {
         const char* tcodecs = v->value;
         if(!strcasecmp(v->value, "all")) {
            tcodecs = "ulaw,alaw,g729,g723,gsm";
         }
	 ast_log(LOG_NOTICE, "ooh323 gpref allow = %s\n", tcodecs );
         ast_parse_allow_disallow(&gPrefs, &gCapability, tcodecs, 1);
      }
      else if (!strcasecmp(v->name, "dtmfmode")) {
         if (!strcasecmp(v->value, "inband"))
            gDTMFMode=H323_DTMF_INBAND;
         else if (!strcasecmp(v->value, "rfc2833"))
            gDTMFMode = H323_DTMF_RFC2833;
         else if (!strcasecmp(v->value, "q931keypad"))
            gDTMFMode = H323_DTMF_Q931;
         else if (!strcasecmp(v->value, "h245alphanumeric"))
            gDTMFMode = H323_DTMF_H245ALPHANUMERIC;
         else if (!strcasecmp(v->value, "h245signal"))
            gDTMFMode = H323_DTMF_H245SIGNAL;
         else {
            ast_log(LOG_WARNING, "Unknown dtmf mode '%s', using rfc2833\n", 
                                                                    v->value);
            gDTMFMode = H323_DTMF_RFC2833;
         }
      }  
      v = v->next;
   }
	
   cat = ast_category_browse(cfg, NULL);
   while(cat) 
   {
      if(strcasecmp(cat, "general")) 
      {
         int friend_type = 0;
         utype = ast_variable_retrieve(cfg, cat, "type");
         if(utype)
         {
            friend_type = strcasecmp(utype, "friend");
            if(!strcmp(utype, "user") || 0 == friend_type)
            {
               user = build_user(cat, ast_variable_browse(cfg, cat));
               if (user) 
               {
                  ast_mutex_lock(&userl.lock);
                  user->next = userl.users;
                  userl.users = user;
                  ast_mutex_unlock(&userl.lock);
               }
               else {
                  ast_log(LOG_WARNING, "Failed to build user %s\n", cat);
               }
            }
            if(!strcasecmp(utype, "peer") || 0 == friend_type)
            {
               peer = build_peer(cat, ast_variable_browse(cfg, cat), friend_type);
               if(peer)
               {
                  ast_mutex_lock(&peerl.lock);
                  peer->next = peerl.peers;
                  peerl.peers = peer;
                  ast_mutex_unlock(&peerl.lock);
               }
               else {
                  ast_log(LOG_WARNING, "Failed to build peer %s\n", cat);
               }
            }
         }
      }
      cat = ast_category_browse(cfg, cat);
   }
   ast_config_destroy(cfg);


   /* Determine ip address if neccessary */
   if(!strlen(gIP))
   {
      ooGetLocalIPAddress(gIP);
      if(!strcmp(gIP, "127.0.0.1"))
      {
         ast_log(LOG_NOTICE, "Failed to determine local ip address. Please "
                            "specify it in ooh323.conf. OOH323 Disabled\n");
         return 1;
      }
   }

   if(gH323Debug)
      ast_verbose("+++   reload_config\n");

   return 0;

}


static int ooh323_show_peer(int fd, int argc, char *argv[])
{
   struct ooh323_peer *prev=NULL, *peer=NULL;
   char ip_port[30];
   if(argc !=  4)
      return RESULT_SHOWUSAGE;
 
 
   ast_mutex_lock(&peerl.lock);
   peer = peerl.peers;
   while(peer)
   {
      ast_mutex_lock(&peer->lock);
      if(!strcmp(peer->name, argv[3]))
         break;
      else{
         prev = peer;
         peer = peer->next;
         ast_mutex_unlock(&prev->lock);
      }
   }

   if(peer)
   {
      sprintf(ip_port, "%s:%d", peer->ip, peer->port);
      ast_cli(fd, "%-15.15s%s\n", "Name: ", peer->name);
      ast_cli(fd, "%-15.15s%s", "Format Prefs: ", "(");
      print_codec_to_cli(fd, &peer->prefs);
      ast_cli(fd, ")\n");
      ast_cli(fd, "%-15.15s", "DTMF Mode: ");
      if(peer->dtmfmode & H323_DTMF_RFC2833)
         ast_cli(fd, "%s\n", "rfc2833");
      else if(peer->dtmfmode & H323_DTMF_Q931)
         ast_cli(fd, "%s\n", "q931keypad");
      else if(peer->dtmfmode & H323_DTMF_H245ALPHANUMERIC)
         ast_cli(fd, "%s\n", "h245alphanumeric");
      else if(peer->dtmfmode & H323_DTMF_H245SIGNAL)
         ast_cli(fd, "%s\n", "h245signal");
      else
         ast_cli(fd, "%s\n", "unknown");
      ast_cli(fd, "%-15.15s%s\n", "AccountCode: ", peer->accountcode);
      ast_cli(fd, "%-15.15s%s\n", "AMA flags: ", 
                                            ast_cdr_flags2str(peer->amaflags));
      ast_cli(fd, "%-15.15s%s\n", "IP:Port: ", ip_port);
      ast_cli(fd, "%-15.15s%d\n", "OutgoingLimit: ", peer->outgoinglimit);
      ast_cli(fd, "%-15.15s%d\n", "rtptimeout: ", peer->rtptimeout);
      ast_mutex_unlock(&peer->lock);
   }
   else{
     ast_cli(fd, "Peer %s not found\n", argv[3]);
     ast_cli(fd, "\n");
   }
   ast_mutex_unlock(&peerl.lock);

   return RESULT_SUCCESS;
}

static int ooh323_show_peers(int fd, int argc, char *argv[])
{
   struct ooh323_peer *prev = NULL, *peer=NULL;
   char formats[_STR_CODEC_SIZE];
   char ip_port[30];
#define FORMAT  "%-15.15s  %-15.15s  %-23.23s  %-s\n"

   if (argc != 3)
      return RESULT_SHOWUSAGE;

   ast_cli(fd, FORMAT, "Name", "Accountcode", "ip:port", "Formats");
   
   ast_mutex_lock(&peerl.lock);
   peer = peerl.peers;
   while(peer)
   {
     ast_mutex_lock(&peer->lock);
     sprintf(ip_port, "%s:%d", peer->ip, peer->port);
     ast_cli(fd, FORMAT, peer->name, 
                 peer->accountcode,
                 ip_port,
                 ast_getformatname_multiple(formats,_STR_CODEC_SIZE,peer->capability));
     prev = peer;
     peer = peer->next;
     ast_mutex_unlock(&prev->lock);

   }
   ast_mutex_unlock(&peerl.lock);
#undef FORMAT
   return RESULT_SUCCESS;
}

/*! \brief Print codec list from preference to CLI/manager */
static void print_codec_to_cli(int fd, struct ast_codec_pref *pref)
{
	int x, codec;

	for(x = 0; x < 32 ; x++) {
		codec = ast_codec_pref_index(pref, x);
		if (!codec)
			break;
		ast_cli(fd, "%s", ast_getformatname(codec));
		ast_cli(fd, ":%d", pref->framing[x]);
		if (x < 31 && ast_codec_pref_index(pref, x + 1))
			ast_cli(fd, ",");
		}
	if (!x)
		ast_cli(fd, "none");
}

static int ooh323_show_user(int fd, int argc, char *argv[])
{
   struct ooh323_user *prev=NULL, *user=NULL;

   if(argc !=  4)
      return RESULT_SHOWUSAGE;
 
 
   ast_mutex_lock(&userl.lock);
   user = userl.users;
   while(user)
   {
      ast_mutex_lock(&user->lock);
      if(!strcmp(user->name, argv[3])) {
         break;
      }
      else {
         prev = user;
         user = user->next;
         ast_mutex_unlock(&prev->lock);
      }
   }

   if(user)
   {
      ast_cli(fd, "%-15.15s%s\n", "Name: ", user->name);
      ast_cli(fd, "%-15.15s%s", "Format Prefs: ", "(");
      print_codec_to_cli(fd, &user->prefs);
      ast_cli(fd, ")\n");
      ast_cli(fd, "%-15.15s", "DTMF Mode: ");
      if(user->dtmfmode & H323_DTMF_RFC2833)
         ast_cli(fd, "%s\n", "rfc2833");
      else if(user->dtmfmode & H323_DTMF_Q931)
         ast_cli(fd, "%s\n", "q931keypad");
      else if(user->dtmfmode & H323_DTMF_H245ALPHANUMERIC)
         ast_cli(fd, "%s\n", "h245alphanumeric");
      else if(user->dtmfmode & H323_DTMF_H245SIGNAL)
         ast_cli(fd, "%s\n", "h245signal");
      else
         ast_cli(fd, "%s\n", "unknown");
      ast_cli(fd, "%-15.15s%s\n", "AccountCode: ", user->accountcode);
      ast_cli(fd, "%-15.15s%s\n", "AMA flags: ", 
                                            ast_cdr_flags2str(user->amaflags));
      ast_cli(fd, "%-15.15s%s\n", "Context: ", user->context);
      ast_cli(fd, "%-15.15s%d\n", "IncomingLimit: ", user->incominglimit);
      ast_cli(fd, "%-15.15s%d\n", "rtptimeout: ", user->rtptimeout);
      if (user->mIP)
      	   ast_cli(fd, "%-15.15s%s\n", "IP: ", user->mIP);
      ast_mutex_unlock(&user->lock);
   }
   else{
     ast_cli(fd, "User %s not found\n", argv[3]);
     ast_cli(fd, "\n");
   }
   ast_mutex_unlock(&userl.lock);

   return RESULT_SUCCESS;
}

static int ooh323_show_users(int fd, int argc, char *argv[])
{
   struct ooh323_user *prev = NULL, *user=NULL;
   char formats[_STR_CODEC_SIZE];
#define FORMAT1  "%-15.15s  %-15.15s  %-15.15s  %-s\n"

   if (argc != 3)
      return RESULT_SHOWUSAGE;

   ast_cli(fd, FORMAT1, "Username", "Accountcode", "Context", "Formats");
   
   ast_mutex_lock(&userl.lock);
   user = userl.users;
   while(user)
   {
     ast_mutex_lock(&user->lock);
     ast_cli(fd, FORMAT1, user->name, 
                 user->accountcode, user->context,
                 ast_getformatname_multiple(formats,_STR_CODEC_SIZE,user->capability));
     prev = user;
     user = user->next;
     ast_mutex_unlock(&prev->lock);

   }
   ast_mutex_unlock(&userl.lock);
#undef FORMAT1
   return RESULT_SUCCESS;

}

static int ooh323_do_debug(int fd, int argc, char *argv[])
{
   if (argc != 2)
      return RESULT_SHOWUSAGE;

   gH323Debug = TRUE;
   ast_cli(fd, "OOH323 Debugging Enabled\n");
   return RESULT_SUCCESS;
}

static int ooh323_no_debug(int fd, int argc, char *argv[])

{
   if (argc != 3)
      return RESULT_SHOWUSAGE;
   gH323Debug = FALSE;
   ast_cli(fd, "OOH323 Debugging Disabled\n");
   return RESULT_SUCCESS;
}

static int ooh323_show_channels(int fd, int argc, char *argv[])
{
   return RESULT_SUCCESS;
}

static int ooh323_show_config(int fd, int argc, char *argv[])
{
   char value[_STR_CODEC_SIZE];
   ooAliases *pAlias = NULL, *pAliasNext=NULL;
   ast_cli(fd, "\nObjective Open H.323 Value for config  :\n");
   ast_cli(fd, "[general]\n");
   ast_cli(fd, "%-30s%s\n", "port= ","The port asterisk should listen for incoming H323 connections.");
   ast_cli(fd, "%-30s%s\n", "bindaddr= ","The IP address, asterisk should listen on for incoming H323 connections");
   ast_cli(fd, "%s\n", "\tDefault - 0.0.0.0: tries to find out local ip address on it's own");
   ast_cli(fd, "%-30s%s\n", "publicIp= ","The public IP address for rtp stream ");
   ast_cli(fd, "%-30s%s\n", "h225portrange= min-max ","H225 tcp port range ");
   ast_cli(fd, "%-30s%s\n", "H245PortRange= min-max ","H245 tcp port range ");
   ast_cli(fd, "%-30s%s\n", "GKportrange=min-max ","gatekeeper tcp port range ");
   ast_cli(fd, "%-30s%s\n", "rtpportrange=min-max ","rtp udp port range");
   ast_cli(fd, "%-30s%s\n", "FURMinTime= ","minimum interval value in second between two video fast update request ");
   ast_cli(fd, "%-30s%s\n", "gateway=yes/no ","This parameter indicates whether channel driver should register with ");
   ast_cli(fd, "%s\n", "\tgatekeeper as a gateway or an endpoint.h (Default - no)");
   ast_cli(fd, "%-30s%s\n", "ssrcFix=yes/no ","Allow ssrc change during call ");
   ast_cli(fd, "%-30s%s\n", "msdmode=master/slave ","Default mode for Master or Slave Determination");
   ast_cli(fd, "%-30s%s\n", "CheckOLC=yes/no  ","Check if OpenLogicalChannel is ok with MSD (recommanded no)");
   ast_cli(fd, "%-30s%s\n", "termtype=[50 - 240]","value of termtype recommanded  Terminal=50 Gateway=90 Gatekeeper=130 MCU=190 default=50");
   ast_cli(fd, "%-30s%s\n", "mediawaitforconnect=yes/no ","Whether media wait for connect for fast start call (Default - no)");
   ast_cli(fd, "%-30s%s\n", "h245tunneling=yes/no ","Whether asterisk should use  tunneling for H323 connections.");
   ast_cli(fd, "%-30s%s\n", "faststart=yes/no ","Whether asterisk should use  fast start for H323 connections.");
   ast_cli(fd, "%-30s%s\n", "addVp200Info=yes/no ","Add US VRS vp200 information on setup ");
   ast_cli(fd, "%-30s%s\n", "BearerCapabilityCircuitMode=yes/no ","Transfer Mode is circuit on setup (default yes) no = transfert mode in packet mode");
   ast_cli(fd, "%-30s%s\n", "h323id=  ","Alias name on setup ");
   ast_cli(fd, "%-30s%s\n", "email= ","Alias email on setup");
   ast_cli(fd, "%-30s%s\n", "callerid= ","caller id on setup");
   ast_cli(fd, "%-30s%s\n", "H225_productid= ","vendor identification on setup.");
   ast_cli(fd, "%-30s%s\n", "H225_versionid= ","version identification on setup.");
   ast_cli(fd, "%-30s%s\n", "H225_manufacturercode= ","manufactured code on setup.");
   ast_cli(fd, "%-30s%s\n", "incominglimit= ","max simultaneous incomming call");
   ast_cli(fd, "%-30s%s\n", "outgoinglimit= ","max simultaneous outgoing call");
   ast_cli(fd, "%-30s%s\n", "gatekeeper=DISABLE/DISCOVER/a.b.c.d ","Whether this asterisk server will use gatekeeper.(Default - DISABLE)");
   ast_cli(fd, "%-30s%s\n", "logfile= ","Location for H323 stack log file (Default - /var/log/asterisk/h323_log)");
   ast_cli(fd, "%-30s%s\n", "context= ","Sets default context all clients will be placed in.Default - default");
   ast_cli(fd, "%-30s%s\n", "rtptimeout= ","Max time without rtp before  hangup call ");
   ast_cli(fd, "%-30s%s\n", "tos=lowdelay/thoughput/reliability/mincost/non ","Type of Service.(Default - none)");
   ast_cli(fd, "%-30s%s\n", "amaflags=","Asterisk amaflags");
   ast_cli(fd, "%-30s%s\n", "disallow=all/ulaw/alaw/g729/g723/gsm/h263/h264  ","deactive codec  ");
   ast_cli(fd, "%-30s%s\n", "allow=all/ulaw/alaw/g729/g723/gsm/h263/h264  ","active codec ");
   ast_cli(fd, "%-30s%s\n", "dtmfmode=rfc2833/q931keypad/h245alphanumeric/h245signal  "," mode to be used by default for all clients.(Default all)");
   ast_cli(fd, "\n");
   ast_cli(fd, "\n");
   ast_cli(fd, "\nObjective Open H.323 Channel Driver's Config :\n");
   sprintf(value, "%s:%d", gIP, gPort);
   ast_cli(fd, "%-30s%s\n", "IP:Port: ", value);
   ast_cli(fd, "%-30s%s\n", "Public IP: ", gPublicIP?gPublicIP:"");
   ast_cli(fd, "%-30s%d-%d\n", "H.225/H.245 port range: ", 
           ooconfig.mTCPPortStart, ooconfig.mTCPPortEnd);
   ast_cli(fd, "%-30s%d-%d\n", "GK port range: ", 
      ooconfig.mUDPPortStart, ooconfig.mUDPPortEnd);
   ast_cli(fd, "%-30s%d-%d\n", "RTP port range: ", 
      ooconfig.mRTPPortStart, ooconfig.mRTPPortEnd);
   ast_cli(fd, "%-30s%s\n", "FastStart", gFastStart?"yes":"no");
   ast_cli(fd, "%-30s%s\n", "Tunneling", gTunneling?"yes":"no");
   ast_cli(fd, "%-30s%s\n", "CallerId", gCallerID);
   ast_cli(fd, "%-30s%s\n", "MediaWaitForConnect", 
      gMediaWaitForConnect?"yes":"no");
   ast_cli(fd, "%-30s%ld\n", "Min FUR:",ooconfig.mFURMinTime );
   ast_cli(fd, "%-30s%s\n", "addVp200Info:",gVp200Info?"yes":"no" );
   ast_cli(fd, "%-30s%s\n", "BearerCapabilityCircuitMode:",gBearerCapabilityMode==Q931TransferCircuitMode?"yes":"no" );
#if (0)
   extern OOH323EndPoint gH323ep;
   ast_cli(fd, "%-30s%s\n", "FASTSTART", 
      (OO_TESTFLAG(gH323ep.flags, OO_M_FASTSTART) != 0) ? "yes" : "no");
   ast_cli(fd, "%-30s%s\n", "TUNNELING", 
      (OO_TESTFLAG(gH323ep.flags, OO_M_TUNNELING) != 0) ? "yes" : "no");
   ast_cli(fd, "%-30s%s\n", "MEDIAWAITFORCONN",
      (OO_TESTFLAG(gH323ep.flags, OO_M_MEDIAWAITFORCONN) != 0) ? "yes" : "no");
#endif

   if(gRasGkMode == RasNoGatekeeper)
      sprintf(value, "%s", "No Gatekeeper");
   else if(gRasGkMode == RasDiscoverGatekeeper)
      sprintf(value, "%s", "Discover");
   else
      sprintf(value, "%s", gGatekeeper);

   ast_cli(fd,  "%-30s%s\n", "Gatekeeper:", value);
   ast_cli(fd,  "%-30s%s\n", "H.323 LogFile:", gLogFile);   
   ast_cli(fd,  "%-30s%s\n", "Context:", gContext);
   ast_cli(fd,  "%-30s%s\n", "Capability:", 
           ast_getformatname_multiple(value,_STR_CODEC_SIZE,gCapability));
           
   //print_codec_to_cli(fd, &gPrefs);
           

   ast_cli(fd, "%-30s", "DTMF Mode: ");
   if(gDTMFMode & H323_DTMF_RFC2833)
      ast_cli(fd, "%s\n", "rfc2833");
   else if (gDTMFMode & H323_DTMF_Q931)
      ast_cli(fd, "%s\n", "q931keypad");
   else if(gDTMFMode & H323_DTMF_H245ALPHANUMERIC)
      ast_cli(fd, "%s\n", "h245alphanumeric");
   else if(gDTMFMode & H323_DTMF_H245SIGNAL)
      ast_cli(fd, "%s\n", "h245signal");
   else
      ast_cli(fd, "%s\n", "unknown");

   ast_cli(fd, "%-30s%s\n", "AccountCode: ", gAccountcode);

   ast_cli(fd, "%-30s%s\n", "AMA flags: ", ast_cdr_flags2str(gAMAFLAGS));

   pAlias = gAliasList;
   if(pAlias)
   {
     ast_cli(fd, "%-30s\n", "Aliases: ");
   }
   while(pAlias)
   {
      pAliasNext = pAlias->next;
      if(pAliasNext)
      {
         ast_cli(fd,"\t%-30s\t%-30s\n",pAlias->value, pAliasNext->value);
         pAlias = pAliasNext->next;
      }
      else{
         ast_cli(fd,"\t%-30s\n",pAlias->value);
         pAlias = pAlias->next;
      }
   }

   ast_cli(fd,"----------------- H323 STACK CONFIG --------------------\n");
   ooH323EpPrintConfig();
   return RESULT_SUCCESS;
}

static char show_config_usage[]=
"Usage: ooh323 show config\n"
"       Shows global configuration of H.323 channel driver\n";

static char debug_usage[] = 
"Usage: ooh323 debug\n"
"       Enables debugging of OOH323 channel driver\n";

static struct ast_cli_entry  cli_debug =
   { { "ooh323", "debug", NULL }, ooh323_do_debug, "Enable OOH323 debugging", debug_usage };

static char reload_usage[] =
"Usage: ooh323 reload\n"
"       Reload configuration of OOH323 channel driver\n";

static struct ast_cli_entry  cli_reload =
{ { "ooh323", "reload", NULL }, ooh323_reload, "Reload OOH323 configuration", reload_usage };

static char no_debug_usage[] = 
"Usage: ooh323 no debug\n"
"       Disables debugging of OOH323 channel driver\n";

static struct ast_cli_entry  cli_no_debug =
   { { "ooh323", "no", "debug", NULL }, ooh323_no_debug, "Disable OOH323 debugging", no_debug_usage };

static char show_users_usage[] = 
"Usage: ooh323 show users \n"
"       Lists all known OOH323 users.\n";

static struct ast_cli_entry  cli_show_users = 
   { { "ooh323", "show", "users", NULL }, ooh323_show_users, "Show defined OOH323 users", show_users_usage };

static char show_user_usage[] = 
"Usage: ooh323 show user <name> \n"
"       List details of specific OOH323 user.\n";

static struct ast_cli_entry  cli_show_user =
   { { "ooh323", "show", "user", NULL }, ooh323_show_user, "Show details on specific OOH323 user", show_user_usage };

static char show_peers_usage[] = 
"Usage: ooh323 show peers \n"
"       Lists all known OOH323 peers.\n";

static struct ast_cli_entry  cli_show_peers = 
   { { "ooh323", "show", "peers", NULL }, ooh323_show_peers, "Show defined OOH323 peers", show_peers_usage };

static char show_peer_usage[] = 
"Usage: ooh323 show peer <name> \n"
"       List details of specific OOH323 peer.\n";

static struct ast_cli_entry  cli_show_peer =
   { { "ooh323", "show", "peer", NULL }, ooh323_show_peer, "Show details on specific OOH323 peer", show_peer_usage };

static struct ast_cli_entry cli_show_config =
   { { "ooh323", "show", "config", NULL }, ooh323_show_config, "Show details on global configuration of H.323 channel driver", show_config_usage };         



static int load_module(void)
{
   int res;
   struct ooAliases * pNewAlias=NULL;
   struct ooh323_peer *peer=NULL;
   OOH225MsgCallbacks h225Callbacks = {0, 0, 0, 0};

   OOH323CALLBACKS h323Callbacks ={
     .onNewCallCreated = onNewCallCreated,
     .onAlerting = onAlerting,
     .onIncomingCall = NULL,
     .onOutgoingCall = NULL,
     .onCallEstablished = onCallEstablished,
     .onCallCleared = onCallCleared,
     .openLogicalChannels= NULL,
     .onReceivedDTMF = &ooh323_onReceivedDigit ,
     .onReceivedMiscellaneous = ooh323_onReceivedMiscellaneous 
   };

  if (0)
	ast_log(LOG_NOTICE, 
		"---------------------------------------------------------------------------------\n"
		"---  ******* IMPORTANT NOTE ***********\n"
		"---\n"
		"---  This module is currently unsupported.  Use it at your own risk.\n"
		"---\n"
		"---------------------------------------------------------------------------------\n");

   h225Callbacks.onReceivedSetup = &ooh323_onReceivedSetup;

   userl.users = NULL;
   ast_mutex_init(&userl.lock);
   peerl.peers = NULL;
   ast_mutex_init(&peerl.lock);
#ifdef H323_EXIT        
   ast_register_atexit(&ast_ooh323c_exit); 
#endif

   sched = sched_context_create();
   if (!sched) {
      ast_log(LOG_WARNING, "Unable to create schedule context\n");
   }
   io = io_context_create();
   if (!io) {
      ast_log(LOG_WARNING, "Unable to create I/O context\n");
   }


   /* fire up the H.323 Endpoint */       
   if(OO_OK != ooH323EpInitialize(OO_CALLMODE_VIDEOCALL, gLogFile))
     //if(OO_OK != ooH323EpInitialize(OO_CALLMODE_AUDIOCALL, gLogFile))
   {
     ast_log(LOG_ERROR, "Failed to initialize OOH323 endpoint-"
             "OOH323 Disabled\n");
     return 1;
   }

   res = reload_config();
   if (!res) {
      /* Make sure we can register our OOH323 channel type */
      if (ast_channel_register(&ooh323_tech)) {
         ast_log(LOG_ERROR, "Unable to register channel class %s\n", ooh323_chantype);
         return 0;
      }
      ast_rtp_proto_register(&ooh323_rtp);
      ast_cli_register(&cli_show_users);
      ast_cli_register(&cli_show_user);
      ast_cli_register(&cli_show_peers);
      ast_cli_register(&cli_show_peer);
      ast_cli_register(&cli_show_config);
      ast_cli_register(&cli_debug);
      ast_cli_register(&cli_reload);
      ast_cli_register(&cli_no_debug);

      if(gIsGateway)
         ooH323EpSetAsGateway();

      ooH323EpDisableAutoAnswer();
      ooH323EpSetH225MsgCallbacks(h225Callbacks);
      ooH323EpSetTraceLevel(OOTRCLVLDBGC);
      ooH323EpSetLocalAddress(gIP, gPort, (strlen(gPublicIP) != 0)?gPublicIP:NULL,NULL);
      ooH323EpSetCallerID(gCallerID);
      ooH323EpSetVersionID(gVersionID);
      ooH323EpSetProductID(gProductID);
      /* Set aliases if any */
      pNewAlias = gAliasList;
      while(pNewAlias)
      {
         switch(pNewAlias->type)
         {
         case T_H225AliasAddress_h323_ID:
            ooH323EpAddAliasH323ID(pNewAlias->value);
            break;
         case T_H225AliasAddress_dialedDigits:   
            ooH323EpAddAliasDialedDigits(pNewAlias->value);
            break;
         case T_H225AliasAddress_email_ID:   
            ooH323EpAddAliasEmailID(pNewAlias->value);
            break;
         default:
            ;
         }
         pNewAlias = pNewAlias->next;
      }

      ast_mutex_lock(&peerl.lock);
      peer = peerl.peers;
      while(peer)
      {
         if(peer->h323id) ooH323EpAddAliasH323ID(peer->h323id);
         if(peer->email)  ooH323EpAddAliasEmailID(peer->email);
         if(peer->e164)   ooH323EpAddAliasDialedDigits(peer->e164);
         if(peer->url)    ooH323EpAddAliasURLID(peer->url);
         peer = peer->next;
      }
      ast_mutex_unlock(&peerl.lock);
   

      if(gMediaWaitForConnect)
         ooH323EpEnableMediaWaitForConnect();
      else 
         ooH323EpDisableMediaWaitForConnect();

      /* Fast start and tunneling options */
      if(gFastStart)
         ooH323EpEnableFastStart();
      else
         ooH323EpDisableFastStart();

      if(!gTunneling)
         ooH323EpDisableH245Tunneling();

      /* Gatekeeper */
      if(gRasGkMode == RasUseSpecificGatekeeper)
         ooGkClientInit(gRasGkMode, gGatekeeper, 0);
      else if(gRasGkMode == RasDiscoverGatekeeper)
         ooGkClientInit(gRasGkMode, 0, 0);

      // Manufacter
      if ( gManufacturerCode )
        ooH323EpSetmanufacturerCode( gManufacturerCode );

      /* Register callbacks */
      ooH323EpSetH323Callbacks(h323Callbacks);

      /* Add endpoint capabilities */
      if(ooh323c_set_capability(&gPrefs, gCapability, gDTMFMode)<0) {
         ast_log(LOG_ERROR, "Capabilities failure for OOH323. OOH323 Disabled.\n");
         return 1;
      }

      /* Create H.323 listener */
      if(ooCreateH323Listener()!= OO_OK)
      {
         ast_log(LOG_ERROR, "OOH323 Listener Creation failure. "
                            "OOH323 DISABLED\n");
      
         ooH323EpDestroy();
         return 1;
      }
      
      if(ooh323c_start_stack_thread()  <0)
      {
         ast_log(LOG_ERROR, "Failed to start OOH323 stack thread. "
                            "OOH323 DISABLED\n");
         ooH323EpDestroy();
         return 1;
      }

      /* And start the monitor for the first time */
      restart_monitor();
   }

   return 0;
}


static void *do_monitor(void *data)
{
   int res;
   int reloading;
   struct ooh323_pvt *h323 = NULL;
   time_t t;

   for(;;) {
      struct ooh323_pvt *h323_next;
      /* Check for a reload request */
      ast_mutex_lock(&h323_reload_lock);
      reloading = h323_reloading;
      h323_reloading = 0;
      ast_mutex_unlock(&h323_reload_lock);
      if (reloading) {
         if (option_verbose > 0) {
            ast_verbose(VERBOSE_PREFIX_1 "Reloading H.323\n");
         }
         ooh323_do_reload();
      }

      /* Check for interfaces needing to be killed */
      ast_mutex_lock(&iflock);
      time(&t);
      h323 = iflist;
      while(h323) {
         h323_next = h323->next;

         /*TODO: Need to add rtptimeout keepalive support */
         if(ast_test_flag(h323, H323_NEEDDESTROY)) {
            ooh323_destroy (h323);
         }
         h323 = h323_next;
      }
      ast_mutex_unlock(&iflock);
      pthread_testcancel();

      /* Wait for sched or io */
      res = ast_sched_wait(sched);
      if ((res < 0) || (res > 1000)) {
         res = 1000;
      }
      res = ast_io_wait(io, res);
      pthread_testcancel();
      ast_mutex_lock(&monlock);
      if (res >= 0) {
         ast_sched_runq(sched);
      }
      ast_mutex_unlock(&monlock);
   }
   /* Never reached */
   return NULL;
}

int restart_monitor(void)
{
   pthread_attr_t attr;

   /* If we're supposed to be stopped -- stay stopped */
   if (monitor_thread == AST_PTHREADT_STOP)
      return 0;
   if (ast_mutex_lock(&monlock)) {
      ast_log(LOG_WARNING, "Unable to lock monitor\n");
      return -1;
   }
   if (monitor_thread == pthread_self()) {
      ast_mutex_unlock(&monlock);
      ast_log(LOG_WARNING, "Cannot kill myself\n");
      return -1;
   }
   if (monitor_thread != AST_PTHREADT_NULL) {
      /* Wake up the thread */
      pthread_kill(monitor_thread, SIGURG);
   } 
   else {
      pthread_attr_init(&attr);
      pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
      /* Start a new monitor */
      if (ast_pthread_create(&monitor_thread, &attr, do_monitor, NULL) < 0) {
         ast_mutex_unlock(&monlock);
         ast_log(LOG_ERROR, "Unable to start monitor thread.\n");
         return -1;
      }
   }
   ast_mutex_unlock(&monlock);
   return 0;
}



int ooh323_destroy(struct ooh323_pvt *p)
{
   /* NOTE: Assumes iflock already acquired */
   struct ooh323_pvt *prev=NULL,*cur = NULL;


   if(gH323Debug)
   {
      ast_verbose("---   ooh323_destroy \n");

      if(p)
         ast_verbose(" Destroying %s\n", p->username);
   }

   cur = iflist;
   while(cur)
   {
      if(cur == p) { break; }
      prev = cur;
      cur = cur->next;
   }

   if(cur)
   {
      ast_mutex_lock(&cur->lock);
      if(prev)
         prev->next = cur->next;
      else
         iflist = cur->next;

      if(cur->callToken) {
         free(cur->callToken);
         cur->callToken = 0;
      }

      if(cur->username) {
         free(cur->username);
         cur->username = 0;
      }

      if(cur->host) {
         free(cur->host);
         cur->host = 0;
      }

      if(cur->callerid_name) {
         free(cur->callerid_name);
         cur->callerid_name = 0;
      }
      
      if(cur->callerid_num) {
         free(cur->callerid_num);
         cur->callerid_num = 0;
      }


      if (cur->rtp) {
         ast_rtp_destroy(cur->rtp);
         cur->rtp = 0;
      }

      if (cur->vrtp) {
         ast_rtp_destroy(cur->vrtp);
         cur->vrtp = 0;
      }
	
      /* Unlink us from the owner if we have one */
      if (cur->owner) {
         while(ast_mutex_trylock(&cur->owner->lock))
         {
           if ( option_debug > 2 )
             ast_log(LOG_DEBUG,"ooh323_destroy : Failed to grab lock, trying again\n");
           ast_mutex_unlock(&cur->lock);
           usleep(1);
           ast_mutex_lock(&cur->lock);
         }           
         if ( option_debug > 2 )
           ast_log(LOG_DEBUG, "Detaching from %s\n", cur->owner->name);
         cur->owner->tech_pvt = NULL;
         ast_mutex_unlock(&cur->owner->lock);
         cur->owner = NULL;
      }
  
      if (cur->vad) {
         ast_dsp_free(cur->vad);
         cur->vad = NULL;
      }
      ast_mutex_unlock(&cur->lock);
      ast_mutex_destroy(&cur->lock);

      free(cur);
   }

   if(gH323Debug)
      ast_verbose("+++   ooh323_destroy\n");

   return 0;
}

int delete_peers()
{
   struct ooh323_peer *cur=NULL, *prev=NULL;
   ast_mutex_lock(&peerl.lock);
   cur = peerl.peers;
   while(cur)
   {
      prev = cur;
      cur = cur->next;

      ast_mutex_destroy(&prev->lock);
      if(prev->h323id)   free(prev->h323id);
      if(prev->email)    free(prev->email);
      if(prev->url)      free(prev->url);
      if(prev->e164)     free(prev->e164);
      free(prev);

      if(cur == peerl.peers) { break; }
   }
   peerl.peers = NULL;
   ast_mutex_unlock(&peerl.lock);
   return 0;
}

int delete_users()
{
   struct ooh323_user *cur=NULL, *prev=NULL;
   ast_mutex_lock(&userl.lock);
   cur = userl.users;
   while(cur)
   {
      prev = cur;
      cur = cur->next;
      ast_mutex_destroy(&prev->lock);
      free(prev);
      if(cur == userl.users) { break; }
   }
   userl.users = NULL;
   ast_mutex_unlock(&userl.lock);
   return 0;
}
  
static int unload_module(void)
{
   struct ooh323_pvt *p;
   struct ooAliases *cur=NULL, *prev=NULL;

   if(gH323Debug) {
      ast_verbose("--- ooh323  unload_module \n");
   }
   /* First, take us out of the channel loop */
   ast_cli_unregister(&cli_show_users);
   ast_cli_unregister(&cli_show_user);
   ast_cli_unregister(&cli_show_peers);
   ast_cli_unregister(&cli_show_peer);
   ast_cli_unregister(&cli_show_config);
   ast_cli_unregister(&cli_debug);
   ast_cli_unregister(&cli_reload);
   ast_cli_unregister(&cli_no_debug);
   ast_rtp_proto_unregister(&ooh323_rtp);
   ast_channel_unregister(&ooh323_tech);
#ifdef H323_EXIT
   ast_unregister_atexit(&ast_ooh323c_exit);
#endif

   if(gH323Debug) {
      ast_verbose("  unload_module - hanging up all interfaces\n");
   }
   if (!ast_mutex_lock(&iflock)) {
      /* Hangup all interfaces if they have an owner */
      p = iflist;
      while(p) {
         if (p->owner) {
            ast_softhangup(p->owner, AST_SOFTHANGUP_APPUNLOAD);
         }
         p = p->next;
      }
      iflist = NULL;
      ast_mutex_unlock(&iflock);
   } 
   else {
      ast_log(LOG_WARNING, "Unable to lock the interface list\n");
      return -1;
   }


   if(gH323Debug) {
      ast_verbose("  unload_module - stopping monitor thread\n");
   }  
   if (monitor_thread != AST_PTHREADT_NULL)
   {
     if(!ast_mutex_lock(&monlock)) {
        if (monitor_thread && (monitor_thread != AST_PTHREADT_STOP)) {
            pthread_cancel(monitor_thread);
            pthread_kill(monitor_thread, SIGURG);
            pthread_join(monitor_thread, NULL);
         }
         monitor_thread = AST_PTHREADT_STOP;
         ast_mutex_unlock(&monlock);
      } 
      else {
         ast_log(LOG_WARNING, "Unable to lock the monitor\n");
         return -1;
      }
   }


   if(gH323Debug) {
     ast_verbose("   unload_module - stopping stack thread\n");
   }
   ooh323c_stop_stack_thread();


   if(gH323Debug) {
      ast_verbose("  unload_module - freeing up memory used by interfaces\n");
   }
   if (!ast_mutex_lock(&iflock)) {
      struct ooh323_pvt *pl;

      /* Destroy all the interfaces and free their memory */
      p = iflist;
      while(p) {
         pl = p;
         p = p->next;
         /* Free associated memory */
         ooh323_destroy(pl);
      }
      iflist = NULL;
      ast_mutex_unlock(&iflock);
   } 
   else {
      ast_log(LOG_WARNING, "Unable to lock the interface list\n");
      return -1;
   }
 

   if(gH323Debug) {
      ast_verbose("  unload_module - deleting users\n");
   }
   delete_users();


   if(gH323Debug) {
      ast_verbose("  unload_module - deleting peers\n");
   }
   delete_peers();


   if(gH323Debug) {
      ast_verbose("  unload_module - Freeing up alias list\n");
   }
   cur = gAliasList;
   while(cur)
   {
     prev = cur;
     cur = cur->next;
     free(prev->value);
     free(prev);
   }
   gAliasList = NULL;

   if(gH323Debug) {
      ast_verbose("   unload_module- destroying OOH323 endpoint \n");
   }
   ooH323EpDestroy();

   if(gH323Debug) {
      ast_verbose("+++ ooh323  unload_module \n");   
   }

   return 0;
}



static enum ast_rtp_get_result ooh323_get_rtp_peer(struct ast_channel *chan, struct ast_rtp **rtp)
{
   struct ooh323_pvt *p = NULL;
   enum ast_rtp_get_result res = AST_RTP_TRY_PARTIAL;

   if (!(p = (struct ooh323_pvt *) chan->tech_pvt))
	return AST_RTP_GET_FAILED;

   *rtp = p->rtp;

   if (!(p->rtp)) {
	return AST_RTP_GET_FAILED;
   }
   res = AST_RTP_TRY_PARTIAL;

   return res;
}

static enum ast_rtp_get_result ooh323_get_vrtp_peer(struct ast_channel *chan, struct ast_rtp **rtp)
{
   struct ooh323_pvt *p = NULL;
   enum ast_rtp_get_result res = AST_RTP_TRY_PARTIAL;

   if (!(p = (struct ooh323_pvt *) chan->tech_pvt))
        return AST_RTP_GET_FAILED;

   *rtp = p->vrtp;

   if (!(p->vrtp)) {
        return AST_RTP_GET_FAILED;
   }
   res = AST_RTP_TRY_PARTIAL;

   return res;
}

static enum ast_rtp_get_result ooh323_get_trtp_peer(struct ast_channel *chan, struct ast_rtp **rtp)
{
   struct ooh323_pvt *p = NULL;
   enum ast_rtp_get_result res = AST_RTP_TRY_PARTIAL;

   if (!(p = (struct ooh323_pvt *) chan->tech_pvt))
        return AST_RTP_GET_FAILED;

   *rtp = p->trtp;

   if (!(p->trtp)) {
        return AST_RTP_GET_FAILED;
   }
   res = AST_RTP_TRY_PARTIAL;

   return res;
}

static int ooh323_get_codec(struct ast_channel *chan)
{
        struct ooh323_pvt *p = (struct ooh323_pvt *) chan->tech_pvt;

	return (p) ? p->capability : 0;
}

int ooh323_update_capPrefsOrderForCall
   (ooCallData *call, struct ast_codec_pref *prefs)
{
   int i=0;
   int codec = ast_codec_pref_index(prefs, i);

   ooResetCapPrefs(call);
   while(codec)
   {
      ooAppendCapToCapPrefs(call, ooh323_convertAsteriskCapToH323Cap(codec));
      codec = ast_codec_pref_index(prefs, ++i);
   }

   return 0;
}


int ooh323_convertAsteriskCapToH323Cap(int cap)
{
  char formats[_STR_CODEC_SIZE]={0};
   switch (cap) {
      case AST_FORMAT_ULAW:
         return OO_G711ULAW64K;
      case AST_FORMAT_ALAW:
         return OO_G711ALAW64K;
      case AST_FORMAT_GSM:
         return OO_GSMFULLRATE;
      case AST_FORMAT_G729A:
         return OO_G729A;
      case AST_FORMAT_G723_1:
         return OO_G7231;
      case AST_FORMAT_H263:
         return OO_H263VIDEO;
      case AST_FORMAT_H264:
         return OO_GENERICVIDEO;
      default:
         ast_log(LOG_NOTICE, "Don't know how to deal with mode %s\n", 
                        ast_getformatname_multiple(formats,_STR_CODEC_SIZE,cap));
         return -1;
   }
}

static int ooh323_set_rtp_peer
   (struct ast_channel *chan, struct ast_rtp *rtp, struct ast_rtp *vrtp, struct ast_rtp *trtp,
    int codecs, int nat_active)
{
   /* XXX Deal with Video */
   struct ooh323_pvt *p;
   struct sockaddr_in them;
   struct sockaddr_in us;
   int mode;

   if(gH323Debug)
      ast_verbose("---   ooh323_set_peer - %s\n", chan->name);

   if (!rtp) {
      return 0;
   }

   mode = ooh323_convertAsteriskCapToH323Cap(chan->writeformat); 
   p = (struct ooh323_pvt *) chan->tech_pvt;
   if (!p) {
      ast_log(LOG_ERROR, "No Private Structure, this is bad\n");
      return -1;
   }
   ast_rtp_get_peer(rtp, &them);
   ast_rtp_get_us(rtp, &us);
   
   if (vrtp)
   {
    ast_rtp_get_peer(vrtp, &them);
    ast_rtp_get_us(vrtp, &us);
   }   
   
   return 0;
}


int configure_local_rtp(struct ooh323_pvt *p, ooCallData *call)
{
  struct sockaddr_in us;
  struct sockaddr_in vus;
  OOMediaInfo mediaInfo;
  OOMediaInfo vmediaInfo;
  int x, format=0;     
  char cformats[_STR_CODEC_SIZE] = {0};
  ast_log(LOG_DEBUG,"[H323/RTP] configure_local_rtp\n");
  if ( !strcmp(call->callType, "incoming") )
  {
    struct ooh323_pvt *pvt = NULL;
    pvt = find_call(call);
    if(!pvt)
    {
      ast_log(LOG_ERROR, "configure_local_rtp:No matching call found for %s , PUBLIC and PRIVATE IP not update\n", call->callToken);
    }
    else
    {
      if ( pvt->owner )
      {
        ast_mutex_lock(&pvt->lock);
        ooH323AstSetPublicIP( pvt->owner ,call->PublicIP );
        ooH323AstSetPrivateIP( pvt->owner ,call->PrivateIP );
        ast_mutex_unlock(&pvt->lock);
      }
      else
      {
        ast_log(LOG_ERROR, "configure_local_rtp:No ast channel found for %s PUBLIC and PRIVATE IP not update\n", call->callToken);
      }
    }
  }

  if (! ast_test_flag(p, H323_RTPCONFIGURED) )
  {
    if (p->rtp)
    {
      ast_rtp_codec_setpref(p->rtp, &p->prefs);
    }
   
    /* figure out our local RTP port and tell the H.323 stack about it*/
    if (p->rtp)
    {
      ast_rtp_get_us(p->rtp, &us);
      if ( strlen(gPublicIP) != 0 )
        strncpy(mediaInfo.lMediaIP, gPublicIP, sizeof(mediaInfo.lMediaIP)-1);
      else
        strncpy(mediaInfo.lMediaIP, ast_inet_ntoa(us.sin_addr), sizeof(mediaInfo.lMediaIP)-1);
      mediaInfo.lMediaPort = (int)ntohs(us.sin_port);

      mediaInfo.lMediaCntrlPort = mediaInfo.lMediaPort +1;

      for(x=0; 0 != (format=ast_codec_pref_index(&p->prefs, x)); x++)
      {
        if (format & AST_FORMAT_AUDIO_MASK)
        {
          strcpy(mediaInfo.dir, "transmit");
          mediaInfo.cap = ooh323_convertAsteriskCapToH323Cap(format& AST_FORMAT_AUDIO_MASK);
          mediaInfo.lMediaPort = (int)ntohs(us.sin_port);
          mediaInfo.lMediaCntrlPort = mediaInfo.lMediaPort +1;
          if ( option_debug > 2 )
            ast_log(LOG_DEBUG,"[H323/RTP] configure_local_rtp :Open Audio RTP recv [%s] ==> %s:%d (%s, %s)\n",
                    ast_getformatname_multiple(cformats,_STR_CODEC_SIZE,format & AST_FORMAT_AUDIO_MASK ),
                    mediaInfo.lMediaIP , mediaInfo.lMediaPort, call->callType, call->callToken );

          if ( mediaInfo.cap != -1 )
          {
            ooAddMediaInfo(call, mediaInfo);
            strcpy(mediaInfo.dir, "receive");
            ooAddMediaInfo(call, mediaInfo);
          }
        }
      }
    }

    /* figure out our local RTP port and tell the H.323 stack about it*/
    if (p->vrtp)
    {
      int vformats[] = { AST_FORMAT_H263, AST_FORMAT_H264 };

      ast_rtp_get_us(p->vrtp, &vus);
      if ( strlen(gPublicIP) != 0 )
        strncpy(vmediaInfo.lMediaIP,gPublicIP, sizeof(vmediaInfo.lMediaIP)-1);
      else
        strncpy(vmediaInfo.lMediaIP, ast_inet_ntoa(vus.sin_addr), sizeof(vmediaInfo.lMediaIP)-1);
      vmediaInfo.lMediaPort =(int) ntohs(vus.sin_port);
      vmediaInfo.lMediaCntrlPort = vmediaInfo.lMediaPort +1;
 
      /* figure out our local RTP port and tell the H.323 stack about it*/
      for(x=0; x < 2; x++)
      {
        if (p->capability & vformats[x])
        {

          if ( option_debug > 2 )
            ast_log(LOG_DEBUG,"[H323/RTP] configure_local_rtp :Open Video RTP recv [%s] ==> %s:%d (%s, %s)\n",
                    ast_getformatname_multiple(cformats,_STR_CODEC_SIZE,vformats[x]  ),
                    vmediaInfo.lMediaIP ,vmediaInfo.lMediaPort , call->callType, call->callToken );

          format = vformats[x];
          strcpy(vmediaInfo.dir, "transmit");
          vmediaInfo.cap = ooh323_convertAsteriskCapToH323Cap(format);
          vmediaInfo.rtpPayloadType = ast_rtp_lookup_code(p->vrtp, 1, format);

          if ( vmediaInfo.cap != -1)
          {
            ooAddMediaInfo(call, vmediaInfo);
            strcpy(vmediaInfo.dir, "receive");
            vmediaInfo.rtpPayloadType = 0;
            ooAddMediaInfo(call, vmediaInfo);
          }
        }
      }
    }
    ast_set_flag(p,H323_RTPCONFIGURED);
  }
  else
  {
    if ( option_debug > 2 )
      ast_log(LOG_DEBUG,"[H323/RTP] configure_local_rtp :already configured\n" );    
  }
   
  return 1;
}


void setup_rtp_connection(ooCallData *call, const char *remoteIp, 
                          int remotePort, int fmt)
{
   struct ooh323_pvt *p = NULL;
   struct sockaddr_in them;
   char cformats[_STR_CODEC_SIZE] = {0};

   /* Find the call or allocate a private structure if call not found */
   p = find_call(call); 

   if (!p) {
      ast_log(LOG_ERROR, "No call , setup rtp failed\n");
      return;
   }
   ast_log(LOG_DEBUG,"[H323/RTP] setup_rtp_connection fmt:0x%x\n",fmt);
  if ( !strcmp(call->callType, "incoming") )
  {
    if ( p->owner )
    {
      ast_mutex_lock(&p->lock);
      ooH323AstSetPublicIP( p->owner ,call->PublicIP );
      ooH323AstSetPrivateIP( p->owner ,call->PrivateIP );
      ast_mutex_unlock(&p->lock);
    }
    else
    {
      ast_log(LOG_ERROR, "setup_rtp_connection:No ast channel found for %s PUBLIC and PRIVATE IP not update\n", call->callToken);
    }
  }

  them.sin_family = AF_INET;
  them.sin_addr.s_addr = inet_addr(remoteIp); /* only works for IPv4 */
  them.sin_port = htons(remotePort);

   if (fmt & AST_FORMAT_VIDEO_MASK)
   {
     if ( option_debug > 2 )
       ast_log(LOG_DEBUG,"[H323/RTP] Open Video RTP send [%s] ==> %s:%d (%s, %s)\n",
               ast_getformatname_multiple(cformats,_STR_CODEC_SIZE, fmt& AST_FORMAT_VIDEO_MASK),
               remoteIp , remotePort ,  call->callType, call->callToken );
     ast_rtp_set_peer(p->vrtp, &them);
     call->haveVideoSendPort = TRUE ;
     //do_setnat( p , TRUE ); see rtp read , active nat after recieved !
   }
   else if (fmt & AST_FORMAT_AUDIO_MASK)
   {
     if ( option_debug > 2 )
       ast_log(LOG_DEBUG,"[H323/RTP] Open Audio RTP send [%s] ==> %s:%d (%s, %s)\n",
               ast_getformatname_multiple(cformats,_STR_CODEC_SIZE, fmt ),
               remoteIp , remotePort ,  call->callType, call->callToken );
     ast_rtp_set_peer(p->rtp, &them);
     call->haveAudioSendPort = TRUE ;
     //do_setnat( p , TRUE );see rtp read , active nat after recieved !
   } 
   else 
   {
       ast_log(LOG_ERROR,"[H323/RTP] Cannot Open NOT supported stream  RTP send [%s] ==> %s:%d (%s, %s)\n",
               ast_getformatname_multiple(cformats,_STR_CODEC_SIZE, fmt ),
               remoteIp , remotePort ,  call->callType, call->callToken );
   }
  
#ifndef ESTABLISH_TO_CONNECT
   onCallEstablished(call);
#endif
   return;
}

void close_rtp_connection(ooCallData *call)
{
   struct ooh323_pvt *p = NULL;
   struct sockaddr_in us;
   char   lIP[_STR_CODEC_SIZE] = {0};

   p = find_call(call);
   if(!p)
   {
      ast_log(LOG_ERROR, "Couldn't find matching call to close rtp "
                         "connection\n");
      return;
   }

   if(p->rtp)
   {
    strncpy( lIP , ast_inet_ntoa(us.sin_addr),_STR_CODEC_SIZE);
     ast_rtp_get_us(p->rtp, &us);
     if ( option_debug > 2 )
       ast_log(LOG_DEBUG,"[H323/RTP] Close audio RTP close ==> %s:%d (%s, %s)\n",
               lIP ,us.sin_port , 
               call->callType, call->callToken );
     ast_mutex_lock(&p->lock);
     ast_rtp_stop(p->rtp);
     ast_mutex_unlock(&p->lock);
   }

   if(p->vrtp)
   {
    strncpy( lIP , ast_inet_ntoa(us.sin_addr),_STR_CODEC_SIZE);
     ast_rtp_get_us(p->vrtp, &us);
     if ( option_debug > 2 )
       ast_log(LOG_DEBUG,"[H323/RTP] Close video  RTP close ==> %s:%d (%s, %s)\n",
               lIP ,us.sin_port , 
               call->callType, call->callToken );
     ast_mutex_lock(&p->lock);
     ast_rtp_stop(p->vrtp);
     ast_mutex_unlock(&p->lock);
   }

   return;
}


int update_our_aliases(ooCallData *call, struct ooh323_pvt *p)
{
   int updated = -1;
   ooAliases *psAlias=NULL;
   
   if(!call->ourAliases)
      return updated;
   psAlias = call->ourAliases;
   while(psAlias)
   {
      if(psAlias->type == T_H225AliasAddress_h323_ID)
      {
         strncpy(p->callee_h323id, psAlias->value, sizeof(p->callee_h323id)-1);
         updated = 1;
      }
      if(psAlias->type == T_H225AliasAddress_dialedDigits)
      {
         strncpy(p->callee_dialedDigits, psAlias->value, 
                                        sizeof(p->callee_dialedDigits)-1);
         updated = 1;
      }
      if(psAlias->type == T_H225AliasAddress_url_ID)
      {
         strncpy(p->callee_url, psAlias->value, sizeof(p->callee_url)-1);
         updated = 1;
      }
      if(psAlias->type == T_H225AliasAddress_email_ID)
      {
         strncpy(p->callee_email, psAlias->value, sizeof(p->callee_email)-1);
         updated = 1;
      }
      psAlias = psAlias->next;
   }
   return updated;
}

struct ast_frame *ooh323_rtp_read (struct ast_channel *ast, struct ooh323_pvt *p)
{
   /* Retrieve audio/etc from channel.  Assumes p->lock is already held. */
   struct        ast_frame *f         = NULL ;
   static struct ast_frame null_frame = { AST_FRAME_NULL, };
   char          formats1[_STR_CODEC_SIZE]         = {0};
   char          formats2[_STR_CODEC_SIZE]         = {0};
  
   if ( option_debug > 4 )
     ast_log(LOG_DEBUG, "RTP fdno =%d\n", ast->fdno);

   switch(ast->fdno) {
     case 0:
       if (! ast_test_flag(p, H323_HAVERECVAUDIO) ){
         ast_set_flag(p,H323_HAVERECVAUDIO);
         if (p->rtp) {
           if ( option_debug > 2 )
             ast_log(LOG_DEBUG, "ooh323_rtp_read Setting NAT on audio RTP \n");
           ast_rtp_setnat(p->rtp, TRUE);
         }
       }
       f = ast_rtp_read(p->rtp);   /* RTP Audio */
       break;
     case 1:
       f = ast_rtcp_read(p->rtp);   /* RTCP Control Channel */
       break;
     case 2:
       if (! ast_test_flag(p, H323_HAVERECVVIDEO) ){
         ast_set_flag(p,H323_HAVERECVVIDEO);
         if (p->vrtp) {
           if ( option_debug > 2 )
             ast_log(LOG_DEBUG, "ooh323_rtp_read Setting NAT on video RTP \n");
           ast_rtp_setnat(p->vrtp, TRUE);
         }
       }
       f = ast_rtp_read(p->vrtp);   /* RTP Video */
       break;
     case 3:
       f = ast_rtcp_read(p->vrtp);   /* RTCP Control Channel for video */
       break;
     default:
       f = &null_frame;
   }
   /* Don't send RFC2833 if we're not supposed to */
   if (f && (f->frametype == AST_FRAME_DTMF) && 
       !(p->dtmfmode & H323_DTMF_RFC2833))
   {
      ast_log(LOG_ERROR, "Receive : DTMF but noy on 2833\n");
      return &null_frame;
   }
   if (p->owner) 
   {

      /* We already hold the channel lock */
      if (f->frametype == AST_FRAME_VOICE) 
      {
        if ( option_debug > 4 )
          ast_log(LOG_DEBUG, "RTP read audio : %d %x\n", f->subclass, p->owner->nativeformats);

         if (f->subclass != (p->owner->nativeformats & AST_FORMAT_AUDIO_MASK)) 
         {
               if ( option_debug > 2 )
                 ast_log(LOG_DEBUG, "Oooh, format changed Audio Nativeformats  [%s] -> [%s]\n", 
                         ast_getformatname_multiple(formats1,_STR_CODEC_SIZE, p->owner->nativeformats) ,
                         ast_getformatname_multiple(formats2,_STR_CODEC_SIZE,f->subclass) );
            p->owner->nativeformats = f->subclass | (p->owner->nativeformats & AST_FORMAT_VIDEO_MASK);
            ast_set_read_format(p->owner, p->owner->readformat);
            ast_set_write_format(p->owner, p->owner->writeformat);
         }
         if ((p->dtmfmode & H323_DTMF_INBAND) && p->vad) 
         {
            f = ast_dsp_process(p->owner, p->vad, f);
            
            if (f && (f->frametype == AST_FRAME_DTMF)) 
              if ( option_debug > 2 )
                ast_log(LOG_DEBUG, "* Detected inband DTMF '%c'\n",f->subclass);
         }
      }

      /* We already hold the channel lock */
      if (f->frametype == AST_FRAME_VIDEO) 
      {
        if ( option_debug > 4 )
          ast_log(LOG_DEBUG, "RTP read video : %d\n", f->subclass);

         if ((f->subclass & ~0x1) != (p->owner->nativeformats & AST_FORMAT_VIDEO_MASK))  
         {
           if ( option_debug > 2 )
             ast_log(LOG_DEBUG, "Oooh, format changed Video Nativeformats  [%s] -> [%s] \n", 
                     ast_getformatname_multiple(formats1,_STR_CODEC_SIZE, p->owner->nativeformats) ,
                     ast_getformatname_multiple(formats2,_STR_CODEC_SIZE,f->subclass) );
            p->owner->nativeformats = f->subclass | (p->owner->nativeformats & AST_FORMAT_AUDIO_MASK);
            ast_set_read_format(p->owner, p->owner->readformat);
            ast_set_write_format(p->owner, p->owner->writeformat);
         }
      }
   }
   return f;
}


int ooh323_convert_hangupcause_asteriskToH323(int cause)
{
   switch(cause)
   {
      case AST_CAUSE_CALL_REJECTED:
         return OO_REASON_REMOTE_REJECTED;
      case AST_CAUSE_UNALLOCATED:
         return OO_REASON_NOUSER;
      case AST_CAUSE_BUSY:
         return OO_REASON_REMOTE_BUSY;
      case AST_CAUSE_BEARERCAPABILITY_NOTAVAIL:
         return OO_REASON_NOCOMMON_CAPABILITIES;
      case AST_CAUSE_CONGESTION:
         return OO_REASON_REMOTE_BUSY;
      case AST_CAUSE_NO_ANSWER:
         return OO_REASON_REMOTE_NOANSWER;
      case AST_CAUSE_NORMAL:
         return OO_REASON_REMOTE_CLEARED;
      case AST_CAUSE_FAILURE:
      default:
         return OO_REASON_UNKNOWN;
   }

   return 0;


}

int ooh323_convert_hangupcause_h323ToAsterisk(int cause)
{
   switch(cause) {
      case OO_REASON_REMOTE_REJECTED:
         return AST_CAUSE_CALL_REJECTED;
      case OO_REASON_NOUSER: 
         return AST_CAUSE_UNALLOCATED;
      case OO_REASON_REMOTE_BUSY:
      case OO_REASON_LOCAL_BUSY:
         return AST_CAUSE_BUSY;
      case OO_REASON_NOCOMMON_CAPABILITIES:   /* No codecs approved */
         return AST_CAUSE_BEARERCAPABILITY_NOTAVAIL;
      case OO_REASON_REMOTE_CONGESTED:
      case OO_REASON_LOCAL_CONGESTED:
         return AST_CAUSE_CONGESTION;
      case OO_REASON_REMOTE_NOANSWER:
         return AST_CAUSE_NO_ANSWER;
      case OO_REASON_UNKNOWN: 
      case OO_REASON_INVALIDMESSAGE:
      case OO_REASON_TRANSPORTFAILURE:
         return AST_CAUSE_FAILURE;
      case OO_REASON_REMOTE_CLEARED:
         return AST_CAUSE_NORMAL;
      default:
         return AST_CAUSE_NORMAL;
   }
   /* Never reached */
   return 0;
}

#ifdef H323_EXIT
void ast_ooh323c_exit()
{
   ooGkClientDestroy();
}
#endif

#ifdef VIDEOCAPS
/* IVeS - videocaps support for chan_ooh323 */
static int ooh323_set_capabilities(struct ast_channel *ast, struct ast_capabilities *chancaps)
{
  ooCallData * call = NULL;
  struct ooh323_pvt *p = ast->tech_pvt;

  if (p == NULL) return -1;

  if (ast_are_caps_zero(&ast->channelcaps))
	{
    ast_copy_capabilities(&ast->channelcaps, chancaps);
  }
	else 
  {
    ast_resolve_capabilities(&ast->channelcaps, &ast->channelcaps, chancaps, (option_verbose > 2) );
	}

  call = ooFindCallByToken(p->callToken);
  if (call == NULL)
  {
    ast_log(LOG_ERROR, "Could not find OOH323 call data associated with chan %s.\n", ast->name);
    return -2;
  }

  OOTRACEAST(OOTRCLVLDBGA,"[H323/CODEC] ooh323_set_capabilities (%s, %s)\n", call->callType,
             call->callToken);

  if ( ! ast_test_flag(p, H323_CAPSCONFIGURED))
  {
    ooh323c_set_capability_for_call(call, &p->prefs, p->capability,p->dtmfmode, &ast->channelcaps);
    configure_local_rtp(p, call);
    ast_set_flag(p, H323_CAPSCONFIGURED);
  }
  else
  {
    ast_log(LOG_WARNING, "Capabilites already configured for chan %s.\n", ast->name);
  }
	return 0;
}

static int ooh323_get_negociated_caps(struct ast_channel *ast, struct ast_capabilities *jcaps)
{
	struct ooh323_pvt *p = ast->tech_pvt;
        memcpy(jcaps, &ast->channelcaps, sizeof(ast->channelcaps));
	jcaps->cap = p->capability;

        return p->capability; 
}

#endif

/*! \brief Set nat mode on the various data sockets */
static void do_setnat(struct ooh323_pvt *p, int natflags )
{
	const char *mode = natflags ? "On" : "Off";

	if (p->rtp) {
    if ( option_debug > 2 )
      ast_log(LOG_DEBUG, "do_setnat Setting NAT on RTP to %s\n", mode);
		ast_rtp_setnat(p->rtp, natflags);
	}
	if (p->vrtp) {
    if ( option_debug > 2 )
      ast_log(LOG_DEBUG, "do_setnat Setting NAT on VRTP to %s\n", mode);
		ast_rtp_setnat(p->vrtp, natflags);
	}
}

int ooh323_InitFlagCodec( ooCallData *call)
{
  int Status = TRUE ;
  call->CallHaveH263 = FALSE ;
  call->CallHaveH264 = FALSE ;
  call->CallHaveAlaw = FALSE ;
  call->CallHaveUlaw = FALSE ;
  call->CallHaveGsm  = FALSE ;
  call->CallHaveG729 = FALSE ;
  call->CallHaveG723 = FALSE ;

  return Status ;
}

int ooH323AstSetChargVectorID( struct ast_channel *ast ,char* chargVectorID )
{
  int    Status        = TRUE ;


  if ( !ast )
  {
    ast_log(LOG_ERROR, "ooH323AstSetChargVectorID:Charging-Vector ,  ast_channel ==  NULL \n");
    Status = FALSE ;
  } 

  if ( Status )
  {
    pbx_builtin_setvar_helper( ast , _STR_PBX_SETVAR_P_CV_CID , chargVectorID );
    if ( option_debug > 2 )
    {
      OOTRACEAST(OOTRCLVLDBGA,"ooH323AstSetChargVectorID Charging-Vector ast_channel[0x%X] %s[%s] \n",
                 ast, _STR_PBX_SETVAR_P_CV_CID,chargVectorID);
    }
  }
  return Status ;
}

const char* ooH323AstGetChargVectorID( struct ast_channel *ast)
{
  if ( ast )
  {
    if ( option_debug > 2 )
    {
      OOTRACEAST(OOTRCLVLDBGA,"ooH323AstGetChargVectorID Charging-Vector "
                 "ast_channel[0x%X] %s[%s]\n",
                 ast , _STR_PBX_GETVAR_P_CV_CID,
                 pbx_builtin_getvar_helper( ast , _STR_PBX_GETVAR_P_CV_CID ));
    }
    return pbx_builtin_getvar_helper( ast , _STR_PBX_GETVAR_P_CV_CID  );
  }
  else
  {
    ast_log(LOG_ERROR, "ooH323AstGetChargVectorID:Charging-Vector failed "
            ", ast_channel ==  NULL \n");   
    return NULL ;
  }
}


int ooH323AstSetPrivateIP( struct ast_channel *ast ,char* privateIP )
{
  int    Status        = TRUE ;


  if ( !ast )
  {
    ast_log(LOG_ERROR, "ooH323AstSetPrivateIP:ast_channel ==  NULL \n");
    Status = FALSE ;
  } 

  if ( Status )
  {
    pbx_builtin_setvar_helper( ast , _STR_PBX_SETVAR_CALLER_PRIVATE_IP , privateIP );
    if ( option_debug > 2 )
    {
      OOTRACEAST(OOTRCLVLDBGA,"ooH323AstSetPrivateIP  ast_channel[0x%X] %s[%s] \n",
                 ast, _STR_PBX_SETVAR_CALLER_PRIVATE_IP ,privateIP);
    }
  }
  return Status ;
}


int ooH323AstSetPublicIP( struct ast_channel *ast ,char* publicIP )
{
  int    Status        = TRUE ;

  if ( !ast )
  {
    ast_log(LOG_ERROR, "ooH323AstSetPublicIP: ast_channel ==  NULL \n");
    Status = FALSE ;
  } 

  if ( Status )
  {
    pbx_builtin_setvar_helper( ast , _STR_PBX_SETVAR_CALLER_PUBLIC_IP , publicIP );
    if ( option_debug > 2 )
    {
      OOTRACEAST(OOTRCLVLDBGA,"ooH323AstSetPublicIP  ast_channel[0x%X] %s[%s] \n",
                 ast, _STR_PBX_SETVAR_CALLER_PUBLIC_IP ,publicIP);
    }
  }
  return Status ;
}

int  ooH323AstSetTerminalInfo( struct ast_channel* ast ,struct ooh323_pvt* pvt )
{
  int    Status        = TRUE ;
  char   locCode[OO_PRODUCT_ID_SIZE] = {0};
  if ( !ast )
  {
    ast_log(LOG_ERROR, "ooH323AstSetTerminalInfo: ast_channel ==  NULL \n");
    Status = FALSE ;
  } 

  if ( Status )
  {
    snprintf( locCode , OO_PRODUCT_ID_SIZE , "0x%X",pvt->manufacturerCode);
    pbx_builtin_setvar_helper( ast , _STR_PBX_SETVAR_H225_MANUFACTURERCODE , locCode );
    if ( option_debug > 2 )
    {
      OOTRACEAST(OOTRCLVLDBGA,"ooH323AstSetTerminalInfo  ast_channel[0x%X] %s[%d] \n",
                 ast, _STR_PBX_SETVAR_H225_MANUFACTURERCODE  , pvt->manufacturerCode);
    }
    pbx_builtin_setvar_helper( ast , _STR_PBX_SETVAR_H225_PRODUCTID , pvt->productId );
    if ( option_debug > 2 )
    {
      OOTRACEAST(OOTRCLVLDBGA,"ooH323AstSetTerminalInfo  ast_channel[0x%X] %s[%s] \n",
                 ast, _STR_PBX_SETVAR_H225_PRODUCTID   , pvt->productId);
    }
    pbx_builtin_setvar_helper( ast , _STR_PBX_SETVAR_H225_VERSIONID , pvt->versionId );
    if ( option_debug > 2 )
    {
      OOTRACEAST(OOTRCLVLDBGA,"ooH323AstSetTerminalInfo  ast_channel[0x%X] %s[%s] \n",
                 ast, _STR_PBX_SETVAR_H225_VERSIONID   , pvt->versionId);
    }
  }
  return Status ;
}


int ooh323_replace_InvalidChar4SipUri(char* nm2replace ){
  int    Status        = FALSE ;
  if ( nm2replace ){
    size_t lg = strlen( nm2replace ) ;
    int i = 0 ;
    for ( i = 0 ; i < lg ; i++ ){
      int ch = nm2replace[i] ;
      if (( ch ==  0x2E ) || // Case "."
          ( ch >= 0x30 && ch <= 0x3A )|| // Case "[0-9:]" 
          ( ch >= 0x40 && ch <= 0x5A ) ||// Case "[@A-Z]"
          ( ch >= 0x61 && ch <= 0x7A ))// Case "[a-z]"
          continue;
      nm2replace[i] = '_' ;
    }
  }
  return Status ;
}
AST_MODULE_INFO_STANDARD(ASTERISK_GPL_KEY, "Objective Systems H323 Channel");
