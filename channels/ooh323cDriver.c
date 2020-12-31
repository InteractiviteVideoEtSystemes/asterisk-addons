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

#include "ooh323cDriver.h"

#include <asterisk/pbx.h>
#include <asterisk/logger.h>

extern OOBOOL gH323Debug;
/* ooh323c stack thread. */
static pthread_t ooh323c_thread = AST_PTHREADT_NULL;
static int grxframes = 240;

static int gtxframes = 20;

int ooh323c_start_receive_channel(ooCallData *call, ooLogicalChannel *pChannel);
int ooh323c_start_transmit_channel(ooCallData *call, ooLogicalChannel *pChannel);
int ooh323c_stop_receive_channel(ooCallData *call, ooLogicalChannel *pChannel);
int ooh323c_stop_transmit_channel(ooCallData *call, ooLogicalChannel *pChannel);

void* ooh323c_stack_thread(void* dummy)
{

  ooMonitorChannels();
  return dummy;
}

int ooh323c_start_stack_thread()
{
   if(ast_pthread_create(&ooh323c_thread, NULL, ooh323c_stack_thread, NULL) < 0)
   {
      ast_log(LOG_ERROR, "Unable to start ooh323c thread.\n");
      return -1;
   }
   return 0;
}

int ooh323c_stop_stack_thread(void)
{
  if(ooh323c_thread !=  AST_PTHREADT_NULL)
  {
    ooStopMonitor();
    pthread_join(ooh323c_thread, NULL);
    ooh323c_thread =  AST_PTHREADT_NULL;
  }
  return 0;
}

int ooh323c_set_capability(struct ast_codec_pref *prefs, int capability, int dtmf)
{
  int ret, x, format=0;
  if ( option_debug > 2 )
    ast_log(LOG_DEBUG,"[H323/CODEC]Adding capabilities to H323 endpoint\n");
   
  for(x=0; 0 != (format=ast_codec_pref_index(prefs, x)); x++)
  {
    if(format & AST_FORMAT_ULAW)
    {
      if ( option_debug > 2 )
        ast_log(LOG_DEBUG,"[H323/CODEC]Adding g711 ulaw capability to H323 endpoint\n");
      ret= ooH323EpAddG711Capability(OO_G711ULAW64K, gtxframes, grxframes, 
                                     OORXANDTX, &ooh323c_start_receive_channel,
                                     &ooh323c_start_transmit_channel,
                                     &ooh323c_stop_receive_channel, 
                                     &ooh323c_stop_transmit_channel);
    }
    if(format & AST_FORMAT_ALAW)
    {
      if ( option_debug > 2 )
        ast_log(LOG_DEBUG,"[H323/CODEC]Adding g711 alaw capability to H323 endpoint\n");
      ret= ooH323EpAddG711Capability(OO_G711ALAW64K, gtxframes, grxframes, 
                                     OORXANDTX, &ooh323c_start_receive_channel,
                                     &ooh323c_start_transmit_channel,
                                     &ooh323c_stop_receive_channel, 
                                     &ooh323c_stop_transmit_channel);
    }

    if(format & AST_FORMAT_G729A)
    {
      if ( option_debug > 2 )
        ast_log(LOG_DEBUG,"[H323/CODEC]Adding g729A capability to H323 endpoint\n");
      ret = ooH323EpAddG729Capability(OO_G729A, 2, 24, 
                                      OORXANDTX, &ooh323c_start_receive_channel,
                                      &ooh323c_start_transmit_channel,
                                      &ooh323c_stop_receive_channel, 
                                      &ooh323c_stop_transmit_channel);

      if ( option_debug > 2 )
        ast_log(LOG_DEBUG,"[H323/CODEC]Adding g729 capability to H323 endpoint\n");
      ret |= ooH323EpAddG729Capability(OO_G729, 2, 24, 
                                       OORXANDTX, &ooh323c_start_receive_channel,
                                       &ooh323c_start_transmit_channel,
                                       &ooh323c_stop_receive_channel, 
                                       &ooh323c_stop_transmit_channel);
    }

    if(format & AST_FORMAT_G723_1)
    {
      if ( option_debug > 2 )
        ast_log(LOG_DEBUG,"[H323/CODEC]Adding g7231 capability to H323 endpoint\n");
      ret = ooH323EpAddG7231Capability(OO_G7231, 4, 7, FALSE, 
                                       OORXANDTX, &ooh323c_start_receive_channel,
                                       &ooh323c_start_transmit_channel,
                                       &ooh323c_stop_receive_channel, 
                                       &ooh323c_stop_transmit_channel);

    }

    if(format & AST_FORMAT_GSM)
    {
      if ( option_debug > 2 )
        ast_log(LOG_DEBUG,"[H323/CODEC]Adding gsm capability to H323 endpoint\n");
      ret = ooH323EpAddGSMCapability(OO_GSMFULLRATE, 4, FALSE, FALSE, 
                                     OORXANDTX, &ooh323c_start_receive_channel,
                                     &ooh323c_start_transmit_channel,
                                     &ooh323c_stop_receive_channel, 
                                     &ooh323c_stop_transmit_channel);

    }
  }

  if(capability & AST_FORMAT_H264)
  {
    if ( option_debug > 2 )
      ast_log(LOG_DEBUG,"[H323/CODEC]Adding h264 capability to H323 endpoint\n");
    ret = ooH323EpAddH264VideoCapability(OO_GENERICVIDEO,  OO_DEFAULT_H264_PROFILE, 0, OO_DEFAULT_H264_LEVEL, OO_DEFAULT_H264_MAXBR,
                                         OORXANDTX, &ooh323c_start_receive_channel,
                                         &ooh323c_start_transmit_channel,
                                         &ooh323c_stop_receive_channel,
                                         &ooh323c_stop_transmit_channel);

  }

  if(capability & AST_FORMAT_H263)
  {
    if ( option_debug > 2 )
      ast_log(LOG_DEBUG,"[H323/CODEC]Adding h263 capability to H323 endpoint\n");
    ret = ooH323EpAddH263VideoCapability(OO_H263VIDEO, 1, 1, 1, 1, 1, OO_DEFAULT_H264_MAXBR, 
                                         OORXANDTX, &ooh323c_start_receive_channel,
                                         &ooh323c_start_transmit_channel,
                                         &ooh323c_stop_receive_channel, 
                                         &ooh323c_stop_transmit_channel);
  }
   
  if(dtmf & H323_DTMF_RFC2833)
    ret |= ooH323EpEnableDTMFRFC2833(0);
  else if(dtmf & H323_DTMF_H245ALPHANUMERIC)
    ret |= ooH323EpEnableDTMFH245Alphanumeric();
  else if(dtmf & H323_DTMF_H245SIGNAL)
    ret |= ooH323EpEnableDTMFH245Signal();

  return ret;
}

#ifdef VIDEOCAPS
int ooh323c_set_capability_for_call
(ooCallData *call, struct ast_codec_pref *prefs, int capability, int dtmf, struct ast_capabilities *caps)
#else
  int ooh323c_set_capability_for_call
(ooCallData *call, struct ast_codec_pref *prefs, int capability, int dtmf)
#endif
{
  int ret, x, txframes, rxframes;
  int format=0;
  char cformats[_STR_CODEC_SIZE] = {0};
  if ( !call )
  {
    ast_log(LOG_ERROR, "call is null\n");
    return;
  }

  if ( option_debug > 2 )
    ast_log(LOG_DEBUG,"[H323/CODEC]Adding capabilities %s to call(%s, %s)\n", 
            ast_getformatname_multiple(cformats,_STR_CODEC_SIZE,capability),
            call->callType, 
            call->callToken);

  if(dtmf & H323_DTMF_RFC2833)
    ret |= ooCallEnableDTMFRFC2833(call,0);
  else if(dtmf & H323_DTMF_H245ALPHANUMERIC)
    ret |= ooCallEnableDTMFH245Alphanumeric(call);
  else if(dtmf & H323_DTMF_H245SIGNAL)
    ret |= ooCallEnableDTMFH245Signal(call);


  for(x=0; 0 !=(format=ast_codec_pref_index(prefs, x)); x++)
  {
    if ( option_debug > 2 )
      ast_log(LOG_DEBUG,"[H323/CODEC]Format = %x\n", format); 
   
    if(format & AST_FORMAT_ULAW)
    {
      if ( option_debug > 2 )
        ast_log(LOG_DEBUG,"[H323/CODEC]Adding g711 ulaw capability to call(%s, %s)\n", 
                call->callType, call->callToken);
      txframes = prefs->framing[x];
      ret= ooCallAddG711Capability(call, OO_G711ULAW64K, txframes, 
                                   grxframes, OORXANDTX, 
                                   &ooh323c_start_receive_channel,
                                   &ooh323c_start_transmit_channel,
                                   &ooh323c_stop_receive_channel, 
                                   &ooh323c_stop_transmit_channel);
    }
    if(format & AST_FORMAT_ALAW)
    {
      if ( option_debug > 2 )
        ast_log(LOG_DEBUG,"[H323/CODEC]Adding g711 alaw capability to call(%s, %s)\n",
                call->callType, call->callToken);
      txframes = prefs->framing[x];
      ret= ooCallAddG711Capability(call, OO_G711ALAW64K, txframes, 
                                   grxframes, OORXANDTX, 
                                   &ooh323c_start_receive_channel,
                                   &ooh323c_start_transmit_channel,
                                   &ooh323c_stop_receive_channel, 
                                   &ooh323c_stop_transmit_channel);
    }

    if(format & AST_FORMAT_G729A)
    {
      if ( option_debug > 2 )
        ast_log(LOG_DEBUG,"[H323/CODEC]Adding g729A capability to call(%s, %s)\n",
                call->callType, call->callToken);
      txframes = (prefs->framing[x])/10;
      ret= ooCallAddG729Capability(call, OO_G729A, txframes, 24, 
                                   OORXANDTX, &ooh323c_start_receive_channel,
                                   &ooh323c_start_transmit_channel,
                                   &ooh323c_stop_receive_channel, 
                                   &ooh323c_stop_transmit_channel);

      if ( option_debug > 2 )
        ast_log(LOG_DEBUG,"[H323/CODEC]Adding g729 capability to call(%s, %s)\n",
                call->callType, call->callToken);
      ret|= ooCallAddG729Capability(call, OO_G729, txframes, 24, 
                                    OORXANDTX, &ooh323c_start_receive_channel,
                                    &ooh323c_start_transmit_channel,
                                    &ooh323c_stop_receive_channel, 
                                    &ooh323c_stop_transmit_channel);
    }

    if(format & AST_FORMAT_G723_1)
    {
      if ( option_debug > 2 )
        ast_log(LOG_DEBUG,"[H323/CODEC]Adding g7231 capability to call (%s, %s)\n",
                call->callType, call->callToken);
      ret = ooCallAddG7231Capability(call, OO_G7231, 4, 7, FALSE, 
                                     OORXANDTX, &ooh323c_start_receive_channel,
                                     &ooh323c_start_transmit_channel,
                                     &ooh323c_stop_receive_channel, 
                                     &ooh323c_stop_transmit_channel);

    }

    if(format & AST_FORMAT_GSM)
    {
      if ( option_debug > 2 )
        ast_log(LOG_DEBUG,"[H323/CODEC]Adding gsm capability to call(%s, %s)\n", 
                call->callType, call->callToken);
      ret = ooCallAddGSMCapability(call, OO_GSMFULLRATE, 4, FALSE, FALSE, 
                                   OORXANDTX, &ooh323c_start_receive_channel,
                                   &ooh323c_start_transmit_channel,
                                   &ooh323c_stop_receive_channel, 
                                   &ooh323c_stop_transmit_channel);
    }
  }

  if(capability & AST_FORMAT_H263)
  {
    if ( option_debug > 2 )
      ast_log(LOG_DEBUG,"[H323/CODEC]Adding h263 capability to H323 call (%s, %s)\n",
              call->callType, call->callToken);
    ret = ooCallAddH263VideoCapability(call, OO_H263VIDEO, 0, 0, 1, 0, 0, 192400U, 
                                       OORXANDTX, &ooh323c_start_receive_channel,
                                       &ooh323c_start_transmit_channel,
                                       &ooh323c_stop_receive_channel, 
                                       &ooh323c_stop_transmit_channel);

  }

  if(capability & AST_FORMAT_H264)
  {
    if ( option_debug > 2 )
      ast_log(LOG_DEBUG,"[H323/CODEC]Adding h264 capability to H323 call maxvideobitrate[%d] maxcallbitrate[%d] h264.maxbr[%d]  (%s, %s)\n\n", 
	caps->maxvideobitrate  , caps->maxcallbitrate  , caps->h264.maxbr , call->callType, call->callToken);
#ifdef VIDEOCAPS	 
    /* IVeS - here the capabilities are supposed to be available and already negociated because
     * peer SIP channel has already negociated them. We should add a flag in the pvt to ensure
     * that we add cpas only once ...
     */ 

    if (! caps->h264.valid )
    {
      ast_log(LOG_WARNING,"[H323/CODEC] Invalid  h264 capability use default  (%s, %s)\n\n", call->callType, call->callToken);
      caps->h264.profile = OO_DEFAULT_H264_PROFILE;
      caps->h264.constraint = 0;
      caps->h264.level = OO_DEFAULT_H264_LEVEL;
      caps->h264.maxbr = OO_DEFAULT_H264_MAXBR ;
    }


    if( caps->maxvideobitrate > 0  ){
      ast_log(LOG_DEBUG,"[H323/CODEC]  h264 use maxvideobitrate[%d]  capability use default  (%s, %s)\n\n", caps->maxvideobitrate,call->callType, call->callToken);
      caps->h264.maxbr = caps->maxvideobitrate ;
    }else if ( caps->maxcallbitrate > 0 ) {
      ast_log(LOG_DEBUG,"[H323/CODEC]  h264 use maxcallbitrate[%d]  capability use default  (%s, %s)\n\n", caps->maxcallbitrate,call->callType, call->callToken);
      caps->h264.maxbr = caps->maxcallbitrate ;
    }
    
    if ( caps->h264.maxbr == 0) {
      ast_log(LOG_DEBUG,"[H323/CODEC]  h264 use default bitrate[%d]  capability use default  (%s, %s)\n\n", OO_DEFAULT_H264_MAXBR,call->callType, call->callToken);
      caps->h264.maxbr = OO_DEFAULT_H264_MAXBR ;
    }

    ast_dump_h264_video_cap(&caps->h264);

    if ( option_debug > 2 )
      ast_log(LOG_DEBUG,"[H323/CODEC] H264 br[%d] profile[0x%x] Level[0x%x] \n",caps->h264.maxbr,caps->h264.profile,caps->h264.level);
    ret = ooCallAddH264VideoCapability(call, OO_GENERICVIDEO,  caps->h264.profile, caps->h264.constraint,
                                       caps->h264.level, caps->h264.maxbr,
                                       OORXANDTX, &ooh323c_start_receive_channel,
                                       &ooh323c_start_transmit_channel,
                                       &ooh323c_stop_receive_channel,
                                       &ooh323c_stop_transmit_channel);
#else
    ret = ooCallAddH264VideoCapability(call, OO_GENERICVIDEO,  OO_DEFAULT_H264_PROFILE, 0, OO_DEFAULT_H264_LEVEL, OO_DEFAULT_H264_MAXBR ,
                                       OORXANDTX, &ooh323c_start_receive_channel,
                                       &ooh323c_start_transmit_channel,
                                       &ooh323c_stop_receive_channel,
                                       &ooh323c_stop_transmit_channel);
#endif
  }

}

int ooh323c_set_aliases(ooAliases * aliases)
{
   ooAliases *cur = aliases;
   while(cur)
   {
      switch(cur->type)
      { 
      case T_H225AliasAddress_dialedDigits:
         ooH323EpAddAliasDialedDigits(cur->value);
         break;
      case T_H225AliasAddress_h323_ID:
         ooH323EpAddAliasH323ID(cur->value);
         break;
      case T_H225AliasAddress_url_ID:
         ooH323EpAddAliasURLID(cur->value);
         break;
      case T_H225AliasAddress_email_ID:
         ooH323EpAddAliasEmailID(cur->value);
         break;
      default:
        if ( option_debug > 2 )
          ast_log(LOG_DEBUG, "Ignoring unknown alias type\n");
      }
      cur = cur->next;
   }
   return 1;
}
   
int ooh323c_start_receive_channel(ooCallData *call, ooLogicalChannel *pChannel)
{
   struct ooh323_pvt *p = NULL;
   int fmt=-1;

   if ( option_debug > 2 )
     ast_log(LOG_DEBUG, "[H323/CODEC] Logical channel ID %d RX found with cap %d.\n",
             pChannel->channelNo, pChannel->chanCap->cap );

   p = ooh323_find_call(call);
   if(!p) {
      ast_log(LOG_ERROR, "LogicalChannelsOpened(): Failed to find a matching call.\n");
      return -1;
   }


   fmt = convertH323CapToAsteriskCap(pChannel->chanCap->cap);
   if(fmt>0)
   {
       ooh323_set_read_format(call, fmt);
#ifdef VIDEOCAPS
       /* IVeS TODO : udapte videocaps structure. We need to */
       /* translate the whole structure, with satck info pChannel */
       if (p->owner)
       {
         p->owner->channelcaps.cap |= fmt;
       }
#endif
   }
   else{
     ast_log(LOG_ERROR, "Invalid capability type for receive channel %s\n",
                                                          call->callToken);
     return -1;
   }

   if (pChannel->rtpPayloadType >= 96 && pChannel->rtpPayloadType <= 127)
   {
     if ( option_debug > 2 )
       ast_log(LOG_DEBUG, "Dynamic payload found %d.\n", pChannel->rtpPayloadType);
        switch(pChannel->chanCap->cap)
        {
            case OO_GENERICVIDEO:
              ast_log(LOG_DEBUG, "Set dynamic payload for GENERICVIDEO %d.\n", pChannel->rtpPayloadType);
               ast_rtp_set_rtpmap_type(p->vrtp, pChannel->rtpPayloadType, "video", "h264", 0);
               break;

            default:
                 ast_log(LOG_WARNING, "Dynamic payload not handled for cap = %d.\n", pChannel->chanCap->cap);
                 break;
        }
   }

   return 1;
}

int ooh323c_start_transmit_channel(ooCallData *call, ooLogicalChannel *pChannel)
{
  int fmt=-1;
  fmt = convertH323CapToAsteriskCap(pChannel->chanCap->cap);
  if(fmt>0)
    ooh323_set_write_format(call, fmt);
  else{
    ast_log(LOG_ERROR, "Invalid capability type for receive channel %s\n",
            call->callToken);
    return -1;
  }

  if ( option_debug > 2 )
    ast_log(LOG_DEBUG, "Start RTP %x, %d\n", fmt, pChannel->remoteMediaPort);

  setup_rtp_connection(call, pChannel->remoteIP, pChannel->remoteMediaPort, fmt);
  return 1;
}

int ooh323c_stop_receive_channel(ooCallData *call, ooLogicalChannel *pChannel)
{
   return 1;
}

int ooh323c_stop_transmit_channel(ooCallData *call, ooLogicalChannel *pChannel)
{
   close_rtp_connection(call);
   return 1;
}

int convertH323CapToAsteriskCap(int cap)
{

  switch(cap)
  {
    case OO_G711ULAW64K:
      return AST_FORMAT_ULAW;
    case OO_G711ALAW64K:
      return AST_FORMAT_ALAW;
    case OO_GSMFULLRATE:
      return AST_FORMAT_GSM;
    case OO_G729:
      return AST_FORMAT_G729A;
    case OO_G729A:
      return AST_FORMAT_G729A;
    case OO_G7231:
      return AST_FORMAT_G723_1;
    case OO_H263VIDEO:
      return AST_FORMAT_H263;
    case OO_GENERICVIDEO:
      return AST_FORMAT_H264;

    default:
      if ( option_debug > 2 )
        ast_log(LOG_DEBUG, "Cap %d is not supported by driver yet\n");
      return -1;
  }

  return -1;
}

 
