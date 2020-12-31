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
#ifndef _OO_CHAN_H323_H_
#define _OO_CHAN_H323_H_

#include <asterisk.h>
#undef PACKAGE_NAME
#undef PACKAGE_TARNAME
#undef PACKAGE_VERSION
#undef PACKAGE_STRING
#undef PACKAGE_BUGREPORT
#define VIDEOCAPS

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/signal.h>
#include <pthread.h>

#include <asterisk/lock.h>
#include <asterisk/channel.h>
#include <asterisk/config.h>
#include <asterisk/logger.h>
#include <asterisk/module.h>
#include <asterisk/pbx.h>
#include <asterisk/utils.h>
#include <asterisk/options.h>
#include <asterisk/sched.h>
#include <asterisk/io.h>
#include <asterisk/causes.h>
#include <asterisk/rtp.h>
#include <asterisk/acl.h>
#include <asterisk/callerid.h>
#include <asterisk/file.h>
#include <asterisk/cli.h>
#include <asterisk/app.h>
#include <asterisk/musiconhold.h>
#include <asterisk/manager.h>
#include <asterisk/dsp.h>
#include <asterisk/features.h>
#include <asterisk/stringfields.h>
#ifdef VIDEOCAPS
#include <asterisk/capability.h>
#endif

#include "ootypes.h"
#include "ooCapability.h"
#include "oochannels.h"
#include "ooh323ep.h"
#include "ooh323cDriver.h"
#include "ooCalls.h"
#include "ooq931.h"
#include "ooStackCmds.h"
#include "ooCapability.h"
#include "ooGkClient.h"
#include "ooCmdChannel.h"
#include "ooUtils.h"
#include "ooh245.h"

#ifndef _STR_CODEC_SIZE
#define _STR_CODEC_SIZE         512
#endif

/* IVeS --- P-Charging-Vector support */
#ifndef _STR_PBX_SETVAR_P_CV_CID
#define _STR_PBX_SETVAR_P_CV_CID   "__CHARGING_VECTOR"
#endif

#ifndef _STR_PBX_SETVAR_CALLER_PUBLIC_IP
#define _STR_PBX_SETVAR_CALLER_PUBLIC_IP   "__CALLER_PUBLIC_IP"
#endif

#ifndef _STR_PBX_SETVAR_CALLER_PRIVATE_IP
#define _STR_PBX_SETVAR_CALLER_PRIVATE_IP   "__CALLER_PRIVATE_IP"
#endif


#ifndef _STR_PBX_SETVAR_H225_MANUFACTURERCODE
#define _STR_PBX_SETVAR_H225_MANUFACTURERCODE   "__H225_MANUFACTURERCODE"
#endif

#ifndef _STR_PBX_SETVAR_H225_PRODUCTID
#define _STR_PBX_SETVAR_H225_PRODUCTID   "__H225_PRODUCTID"
#endif

#ifndef _STR_PBX_SETVAR_H225_VERSIONID
#define _STR_PBX_SETVAR_H225_VERSIONID   "__H225_VERSIONID"
#endif

#ifndef _STR_PBX_GETVAR_P_CV_CID
#define _STR_PBX_GETVAR_P_CV_CID   "CHARGING_VECTOR"
#endif

#ifndef _TIME_TO_SEND_FUR
#define _TIME_TO_SEND_FUR            3L
#endif

#ifndef _MAX_TIME_TO_SEND_FUR
#define _MAX_TIME_TO_SEND_FUR        10L
#endif

#ifndef _MIN_TIME_TO_SEND_FUR
#define _MIN_TIME_TO_SEND_FUR        0L
#endif

struct ooh323_pvt {
   ast_mutex_t lock;            /* Channel private lock */
   struct ast_rtp *rtp;
   struct ast_rtp *vrtp; /* Placeholder for now */
   struct ast_rtp *trtp; /* Placeholder for now */
   struct ast_channel *owner;   /* Master Channel */
   unsigned int ssrc;
   time_t lastrtptx;
   time_t lastrtprx;
   unsigned int flags;
   unsigned int call_reference;
   char *callToken;
   char *username;
   char *host;
   char *callerid_name;
   char *callerid_num;
   char caller_h323id[AST_MAX_EXTENSION];
   char caller_dialedDigits[AST_MAX_EXTENSION];
   char caller_email[AST_MAX_EXTENSION];
   char caller_url[256];
   char callee_h323id[AST_MAX_EXTENSION];
   char callee_dialedDigits[AST_MAX_EXTENSION];
   char callee_email[AST_MAX_EXTENSION];
   char callee_url[AST_MAX_EXTENSION];
   char ChargVectorID[OO_PCV_CID_SIZE];
   int  manufacturerCode;
   char productId[OO_PRODUCT_ID_SIZE];
   char versionId[OO_PRODUCT_ID_SIZE];
   int port;
//   int readformat;    /* negotiated read format */
//   int writeformat;  /* negotiated write format */
   int capability;
   struct ast_codec_pref prefs;
   int dtmfmode;
   char exten[AST_MAX_EXTENSION];       /* Requested extension */
   char context[AST_MAX_EXTENSION];     /* Context where to start */
   char accountcode[256];       /* Account code */
   int nat;
   int amaflags;
   struct ast_dsp *vad;
   struct ooh323_pvt *next;     /* Next entity */
};

struct ooh323_user;
struct ooh323_peer;
/* Helper functions */
struct ooh323_user *find_user(const char * name, const char *ip);
struct ooh323_peer *find_peer(const char * name, int port);
void ooh323_delete_peer(struct ooh323_peer *peer);   

int delete_users(void);
int delete_peers(void);

int ooh323_destroy(struct ooh323_pvt *p);
int reload_config(void);
int restart_monitor(void);

int configure_local_rtp(struct ooh323_pvt *p, ooCallData* call);
void setup_rtp_connection(ooCallData *call, const char *remoteIp, 
                          int remotePort, int fmt);
void close_rtp_connection(ooCallData *call);
struct ast_frame *ooh323_rtp_read
         (struct ast_channel *ast, struct ooh323_pvt *p);

struct ooh323_pvt* ooh323_find_call(ooCallData *call);
void ooh323_set_write_format(ooCallData *call, int fmt);
void ooh323_set_read_format(ooCallData *call, int fmt);

int ooh323_update_capPrefsOrderForCall
   (ooCallData *call, struct ast_codec_pref *prefs);

int ooh323_convertAsteriskCapToH323Cap(int cap);

int ooh323_convert_hangupcause_asteriskToH323(int cause);
int ooh323_convert_hangupcause_h323ToAsterisk(int cause);
int update_our_aliases(ooCallData *call, struct ooh323_pvt *p);

/* h323 msg callbacks */
int ooh323_onReceivedSetup(ooCallData *call, Q931Message *pmsg);
int ooh323_onReceivedDigit(OOH323CallData *call, const char* digit);
int ooh323_onReceivedVideoFastUdpate( struct OOH323CallData *call, void * data);
int ooh323_onReceivedMiscellaneous( struct OOH323CallData *call, OOH323MiscCmd type ,void* data);
int ooh323_onReceivedVideoCapability(struct OOH323CallData *call, void * data);
#endif
