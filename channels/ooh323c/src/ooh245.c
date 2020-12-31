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

#include "ooh245.h"
#include "ooCalls.h"
#include "printHandler.h"
#include "ooh323ep.h"
#include "ooCapability.h"
#include "ooTimer.h"
#ifdef _WIN32
#include <stdlib.h>
#include <process.h>
#define getpid _getpid
#endif
#include <time.h>

/** Global endpoint structure */
extern ooEndPoint gH323ep;

static ASN1OBJID gh245ProtocolID = {
  6, { 0, 0, 8, 245, 0, 8 }
};

#ifdef  CONFIG_CUSTOM
static int SendMSD  = TRUE ;   // Send MSD after TCS
static int WaitOLC  = FALSE ; // if Mode Slave , waiting Master OLC for to send my OLC
#endif
static int CheckOLC = FALSE ; // Check if mode master and slave send OLC correct

#define STR_MSD_MODE_SIZE     20

static char msd_mode2str[][30]={
  "undef",
  "master",
  "slave"
};

typedef enum
{
  MSD_MODE_UNDEF = 0,
  MSD_MODE_MASTER,
  MSD_MODE_SLAVE
}msd_mode;

msd_mode defMSDMode = MSD_MODE_UNDEF ;

int ooGetVideoLogicalChannel(OOH323CallData *call,ooH323EpCapability** EpCap){
  OOLogicalChannel * pChannel = NULL;
  pChannel = call->logicalChans;
  while(pChannel){
    if ( pChannel->type == OO_CAP_TYPE_VIDEO   && strcmp( pChannel->dir, "receive") == 0 )
    {
      return pChannel->sessionID;
    }
    pChannel = pChannel->next;
  }
  return 0 ;
}

int ooShowMSDConfig(void)
{
#ifdef  CONFIG_CUSTOM
  OOTRACEINFO2("\tSend MSD                 : %s\n", (SendMSD)?"true":"false");
  OOTRACEINFO2("\tWait OLC on Slave Mode   : %s\n", (WaitOLC)?"true":"false");
#endif
  OOTRACEINFO2("\tCheck OLC on master mode : %s\n", (CheckOLC)?"true":"false");
}

int ooCreateH245Message(H245Message **pph245msg, int type)
{
   OOCTXT* pctxt = &gH323ep.msgctxt;

   *pph245msg = (H245Message*) memAlloc (pctxt, sizeof(H245Message));

   if(!(*pph245msg))
   {
      OOTRACEERR1("ERROR:Failed to allocate memory for h245 message\n");
      return OO_FAILED;
   }
   else
   {
      (*pph245msg)->h245Msg.t = type;
      (*pph245msg)->logicalChannelNo = 0;
      switch(type)
      {
         case  T_H245MultimediaSystemControlMessage_request:
            (*pph245msg)->h245Msg.u.request = (H245RequestMessage*) 
            memAllocZ (pctxt, sizeof(H245RequestMessage));

            /*Check for successful mem allocation, and if successful initialize
              mem to zero*/
            if(!(*pph245msg)->h245Msg.u.request)
            {
               OOTRACEERR1("ERROR:Memory allocation for H.245 request"
                                     " message failed\n");
               return OO_FAILED;
            }
            break;

         case T_H245MultimediaSystemControlMessage_response:
            (*pph245msg)->h245Msg.u.response = (H245ResponseMessage*)
            memAllocZ (pctxt, sizeof(H245ResponseMessage));

            /*Check for successful mem allocation, and if successful initialize
              mem to zero*/
            if(!(*pph245msg)->h245Msg.u.response)
            {
               OOTRACEERR1("ERROR:Memory allocation for H.245 response"
                                     " message failed\n");
               return OO_FAILED;
            }
            break;

         case T_H245MultimediaSystemControlMessage_command:
            (*pph245msg)->h245Msg.u.command = (H245CommandMessage*)
            memAllocZ (pctxt, sizeof(H245CommandMessage));

            /*Check for successful mem allocation, and if successful initialize
              mem to zero*/
            if(!(*pph245msg)->h245Msg.u.command)
            {
               OOTRACEERR1("ERROR:Memory allocation for H.245 command"
                                     " message failed\n");
               return OO_FAILED;
            }
            break;

        case T_H245MultimediaSystemControlMessage_indication:
          (*pph245msg)->h245Msg.u.indication = (H245IndicationMessage*)
            memAllocZ (pctxt, sizeof(H245IndicationMessage));

          /*Check for successful mem allocation, and if successful initialize
            mem to zero*/
          if(!(*pph245msg)->h245Msg.u.indication)
          {
            OOTRACEERR1("ERROR:Memory allocation for H.245 indication"
                        " message failed\n");
            return OO_FAILED;
          }
          break;


        default:
          OOTRACEERR1("ERROR: H245 message type not supported\n");
      }

      return OO_OK;
   }
}

int ooFreeH245Message(OOH323CallData *call, H245Message *pmsg)
{
  /* In case of tunneling, memory is freed when corresponding Q931 message is freed.*/
   OOTRACEDBGC1("msgCtxt Reset?");
   if (0 != pmsg) {
     if(!OO_TESTFLAG (call->flags, OO_M_TUNNELING)){
         memReset (&gH323ep.msgctxt);
         OOTRACEDBGC3(" Done (%s, %s)\n", call->callType, call->callToken);
         return OO_OK;
     }
   }
   OOTRACEDBGC3("Not Done (%s, %s)\n", call->callType, call->callToken);
   return OO_OK;
}

#ifndef _COMPACT
static void ooPrintH245Message 
   (OOH323CallData* call, ASN1OCTET* msgbuf, ASN1UINT msglen)
{
   OOCTXT ctxt;
   H245MultimediaSystemControlMessage mmMsg;
   int ret;

   initContext (&ctxt);

   setPERBuffer (&ctxt, msgbuf, msglen, TRUE);

   initializePrintHandler(&printHandler, "Sending H.245 Message");

   /* Set event handler */
   setEventHandler (&ctxt, &printHandler);

   ret = asn1PD_H245MultimediaSystemControlMessage(&ctxt, &mmMsg);
   if(ret != ASN_OK)
   {
      OOTRACEERR3("Error decoding H245 message (%s, %s)\n", 
                  call->callType, call->callToken);
      OOTRACEERR1 (errGetText (&ctxt));
   }
   finishPrint();
   freeContext(&ctxt);   
}
#endif

int ooEncodeH245Message
   (OOH323CallData *call, H245Message *ph245Msg, char *msgbuf, int size)
{
   int len=0, encodeLen=0, i=0;
   int stat=0;
   ASN1OCTET* encodePtr=NULL;
   H245MultimediaSystemControlMessage *multimediaMsg;
   OOCTXT *pctxt = &gH323ep.msgctxt;
   multimediaMsg = &(ph245Msg->h245Msg);

   if(!msgbuf || size<200)
   {
      OOTRACEERR3("Error: Invalid message buffer/size for "
                  "ooEncodeH245Message. (%s, %s)\n", 
                   call->callType, call->callToken);
      return OO_FAILED;
   }

   msgbuf[i++] = ph245Msg->msgType;
   msgbuf[i++] = (ph245Msg->logicalChannelNo>>8);
   msgbuf[i++] = ph245Msg->logicalChannelNo;
   /* This will contain the total length of the encoded message */
   msgbuf[i++] = 0;
   msgbuf[i++] = 0;
   
   if(!OO_TESTFLAG (call->flags, OO_M_TUNNELING))
   {
      /* Populate message buffer to be returned */
      len =  4;
      msgbuf[i++] = 3; /* TPKT version */
      msgbuf[i++] = 0; /* TPKT resevred */
      /* 1st octet of length, will be populated once len is determined */
      msgbuf[i++] = 0; 
      /* 2nd octet of length, will be populated once len is determined */
      msgbuf[i++] = 0;
   }
   
   setPERBuffer (pctxt, msgbuf+i, (size-i), TRUE);

   stat = asn1PE_H245MultimediaSystemControlMessage (&gH323ep.msgctxt, 
                                                            multimediaMsg);

   if (stat != ASN_OK) {
      OOTRACEERR3 ("ERROR: H245 Message encoding failed (%s, %s)\n", 
                   call->callType, call->callToken);
      OOTRACEERR1 (errGetText (&gH323ep.msgctxt));
      return OO_FAILED;
   }
   
   encodePtr = encodeGetMsgPtr(pctxt, &encodeLen);
   len +=encodeLen;
   msgbuf[3] = (len>>8);
   msgbuf[4] = len;
   if(!OO_TESTFLAG (call->flags, OO_M_TUNNELING))
   {
      msgbuf[7] = len>>8;
      msgbuf[8] = len;
   }
#ifndef _COMPACT
   ooPrintH245Message (call, encodePtr, encodeLen);
#endif
   return OO_OK;
}

int ooSendH245Msg(OOH323CallData *call, H245Message *msg)
{
   int iRet=0,  len=0, msgType=0, logicalChannelNo = 0;
   ASN1OCTET * encodebuf;


   if(!call)
      return OO_FAILED;

   encodebuf = (ASN1OCTET*) memAlloc (call->pctxt, MAXMSGLEN);
   if(!encodebuf)
   {
      OOTRACEERR3("Error:Failed to allocate memory for encoding H245 "
                  "message(%s, %s)\n", call->callType, call->callToken);
      return OO_FAILED;
   }
   iRet = ooEncodeH245Message(call, msg, encodebuf, MAXMSGLEN);

   // phv
   ooTraceH245Msg( call , msg , NULL ) ;

   if(iRet != OO_OK)
   {
      OOTRACEERR3("Error:Failed to encode H245 message. (%s, %s)\n", 
                                             call->callType, call->callToken);
      memFreePtr (call->pctxt, encodebuf);
      return OO_FAILED;
   }
   if(!call->pH245Channel)
   {
      call->pH245Channel =  
              (OOH323Channel*) memAllocZ (call->pctxt, sizeof(OOH323Channel));
      if(!call->pH245Channel)
      {
         OOTRACEERR3("Error:Failed to allocate memory for H245Channel "
                     "structure. (%s, %s)\n", call->callType, call->callToken);
         memFreePtr (call->pctxt, encodebuf);
         return OO_FAILED;
      }
   }

   /* We need to send EndSessionCommand immediately.*/      
   if(!OO_TESTFLAG(call->flags, OO_M_TUNNELING)){
      if(encodebuf[0]== OOEndSessionCommand) /* High priority message */
      {
         dListFreeAll(call->pctxt, &call->pH245Channel->outQueue);
         dListAppend (call->pctxt, &call->pH245Channel->outQueue, encodebuf);
         ooSendMsg(call, OOH245MSG);
      }
      else{
         dListAppend (call->pctxt, &call->pH245Channel->outQueue, encodebuf);
         OOTRACEDBGC4("Queued H245 messages %d. (%s, %s)\n", 
         call->pH245Channel->outQueue.count, 
         call->callType, call->callToken);   
      }
   }
   else{
      msgType = encodebuf[0];

      logicalChannelNo = encodebuf[1];
      logicalChannelNo = logicalChannelNo << 8;
      logicalChannelNo = (logicalChannelNo | encodebuf[2]);

      len = encodebuf[3];
      len = len<<8;
      len = (len | encodebuf[4]);

      iRet = ooSendAsTunneledMessage
         (call, encodebuf+5,len,msgType, logicalChannelNo);

      if(iRet != OO_OK)
      {
         memFreePtr (call->pctxt, encodebuf);
         OOTRACEERR3("ERROR:Failed to tunnel H.245 message (%s, %s)\n",
                      call->callType, call->callToken);
         if(call->callState < OO_CALL_CLEAR)
         {
            call->callEndReason = OO_REASON_INVALIDMESSAGE;
            call->callState = OO_CALL_CLEAR;
         }
         return OO_FAILED;
      }
      memFreePtr (call->pctxt, encodebuf);
      return OO_OK;
   }

   return OO_OK;
}

int ooSendTermCapMsg(OOH323CallData *call)
{
  int ret;
  H245RequestMessage *request=NULL;
  OOCTXT *pctxt=NULL;
  ooH323EpCapability *epCap=NULL;
  H245TerminalCapabilitySet *termCap=NULL;
  H245AudioCapability *audioCap=NULL;
  H245AudioTelephonyEventCapability *ateCap=NULL;
  H245UserInputCapability *userInputCap = NULL;
  H245CapabilityTableEntry *entry=NULL;
  H245AlternativeCapabilitySet *altSet=NULL;
  H245CapabilityDescriptor *capDesc=NULL;
  H245Message *ph245msg=NULL;
  H245VideoCapability *videoCap=NULL;

  int i=0, j=0, k=0;
  if(call->localTermCapState == OO_LocalTermCapSetSent)
  {
    OOTRACEINFO3("TerminalCapabilitySet exchange procedure already in progress. (%s, %s)\n", call->callType, call->callToken);
    return OO_OK;
  }
 
  ret = ooCreateH245Message(&ph245msg, T_H245MultimediaSystemControlMessage_request);
 
  if(ret == OO_FAILED)
  {
    OOTRACEERR3("Error:Failed to create H245 message for Terminal CapabilitySet (%s, %s)\n", call->callType,call->callToken);
    return OO_FAILED;
  }

  /* Set request type as TerminalCapabilitySet */
  request = ph245msg->h245Msg.u.request;
  pctxt = &gH323ep.msgctxt;
  ph245msg->msgType = OOTerminalCapabilitySet;
  memset(request, 0, sizeof(H245RequestMessage));
  if(request == NULL)
  {
    OOTRACEERR3("ERROR: No memory allocated for request message (%s, %s)\n",
                call->callType, call->callToken);
    return OO_FAILED;
  }
   
  request->t = T_H245RequestMessage_terminalCapabilitySet;
  request->u.terminalCapabilitySet = (H245TerminalCapabilitySet*)
    memAlloc(pctxt, sizeof(H245TerminalCapabilitySet)); 
  termCap = request->u.terminalCapabilitySet;
  memset(termCap, 0, sizeof(H245TerminalCapabilitySet));
  termCap->m.multiplexCapabilityPresent = 0;
  termCap->m.capabilityTablePresent = 1;
  termCap->m.capabilityDescriptorsPresent = 1;
  termCap->sequenceNumber = ++(call->localTermCapSeqNo);  
  termCap->protocolIdentifier = gh245ProtocolID; /* protocol id */

  /* Add audio Capabilities */
  OOTRACEAST(OOTRCLVLDBGA,"[H323/CODEC] ooSendTermCapMsg : nb caps[%d]\n",call->capPrefs.index);

  dListInit(&(termCap->capabilityTable));
  for(k=0; k<(int)call->capPrefs.index; k++)
  {
    if(call->ourCaps)
    {
      OOTRACEAST(OOTRCLVLDBGA,"[H323/CODEC] ooSendTermCapMsg : Using call caps \n");
      epCap = call->ourCaps;
    }
    else
    {
      OOTRACEAST(OOTRCLVLDBGA,"[H323/CODEC] ooSendTermCapMsg : Using global caps \n");
      epCap = gH323ep.myCaps;
    }
     
    while(epCap) 
    {
      OOTRACEAST(OOTRCLVLDBGA,"[H323/CODEC] ooSendTermCapMsg :check order %s \n",ooGetCapTypeText(epCap->cap));
     
      if(epCap->cap == call->capPrefs.order[k]) 
        break;
      epCap = epCap->next;
    }

    if(!epCap)
    {
      OOTRACEWARN4("WARN:Preferred capability %d not supported.(%s, %s)\n", call->capPrefs.order[k],call->callType, call->callToken);
      continue;
    }

    if(epCap->capType == OO_CAP_TYPE_AUDIO)
    {

      OOTRACEAST(OOTRCLVLDBGA,"[H323/CODEC] ooSendTermCapMsg :Add audio caps.\n");

      /* Create audio capability. If capability supports receive, we only 
         add it as receive capability in TCS. However, if it supports only 
         transmit, we add it as transmit capability in TCS.
      */
      if((epCap->dir & OORX))
      {

        OOTRACEDBGC4("Sending receive capability %s in TCS.(%s, %s)\n",
                     ooGetCapTypeText(epCap->cap), call->callType, call->callToken);

        audioCap = ooCapabilityCreateAudioCapability(epCap, pctxt, OORX);
        if(!audioCap)
        {
          OOTRACEWARN4("WARN:Failed to create audio capability %s (%s, %s)\n", 
                       ooGetCapTypeText(epCap->cap),  call->callType, call->callToken);
          continue;
        }
      }
      else if(epCap->dir & OOTX)
      {
        OOTRACEDBGC4("Sending transmit capability %s in TCS.(%s, %s)\n",
                     ooGetCapTypeText(epCap->cap), call->callType, call->callToken);
        audioCap = ooCapabilityCreateAudioCapability(epCap, pctxt, OOTX);
        if(!audioCap)
        {
          OOTRACEWARN4("WARN:Failed to create audio capability %s "
                       "(%s, %s)\n", ooGetCapTypeText(epCap->cap),
                       call->callType, call->callToken);
          continue;
        }     
      }
      else{
        OOTRACEWARN3("Warn:Capability is not RX/TX/RXANDTX. Symmetric "
                     "capabilities are not supported.(%s, %s)\n", 
                     call->callType, call->callToken);
        continue;
      }
      /* Add  Capabilities to Capability Table */
      entry = (H245CapabilityTableEntry*) memAlloc(pctxt,
                                                   sizeof(H245CapabilityTableEntry));
      if(!entry)
      {
        OOTRACEERR3("Error:Memory - ooSendTermCapMsg - entry(audio Cap)."
                    "(%s, %s)\n", call->callType, call->callToken);
        return OO_FAILED;
      }
      memset(entry, 0, sizeof(H245CapabilityTableEntry));
      entry->m.capabilityPresent = 1;
      if((epCap->dir & OORX))
      {
        entry->capability.t = T_H245Capability_receiveAudioCapability;
        entry->capability.u.receiveAudioCapability = audioCap;
      }
      else{
        entry->capability.t = T_H245Capability_transmitAudioCapability;
        entry->capability.u.transmitAudioCapability = audioCap;
      }
      entry->capabilityTableEntryNumber = i+1;
      dListAppend(pctxt , &(termCap->capabilityTable), entry);
      i++;
    }
    else if(epCap->capType == OO_CAP_TYPE_VIDEO)
    {
      OOTRACEAST(OOTRCLVLDBGA,"[H323/CODEC] ooSendTermCapMsg :Add video caps.\n");
      if((epCap->dir & OORX))
      {
        OOTRACEDBGC4("Sending receive capability %s in TCS.(%s, %s)\n",
                     ooGetCapTypeText(epCap->cap), call->callType, call->callToken);
        videoCap = ooCapabilityCreateVideoCapability(epCap, pctxt, OORX);
        if(!videoCap)
        {
          OOTRACEWARN4("WARN:Failed to create Video capability %s "
                       "(%s, %s)\n", ooGetCapTypeText(epCap->cap),
                       call->callType, call->callToken);
          continue;
        }
        else
        {
          switch(epCap->cap)
          {
            case OO_H263VIDEO:
              call->CallHaveH263 = TRUE ;
              break; 
            case OO_GENERICVIDEO: /* = H.264 */
              call->CallHaveH264 = TRUE ;
              break ; 
          }
        }
      }
      else if(epCap->dir & OOTX)
      {
        OOTRACEDBGC4("Sending transmit capability %s in TCS.(%s, %s)\n",
                     ooGetCapTypeText(epCap->cap), call->callType, call->callToken);
        videoCap = ooCapabilityCreateVideoCapability(epCap, pctxt, OOTX);
        if(!videoCap)
        {
          OOTRACEWARN4("WARN:Failed to create video capability %s "
                       "(%s, %s)\n", ooGetCapTypeText(epCap->cap),
                       call->callType, call->callToken);
          continue;
        } 
        else
        {
          switch(epCap->cap)
          {
            case OO_H263VIDEO:
              call->CallHaveH263 = TRUE ;
              break; 
            case OO_GENERICVIDEO: /* = H.264 */
              call->CallHaveH264 = TRUE ;
              break ; 
          }
        }   
      }
      else{
        OOTRACEWARN3("Warn:Capability is not RX/TX/RXANDTX. Symmetric "
                     "capabilities are not supported.(%s, %s)\n", 
                     call->callType, call->callToken);
        continue;
      }
      /* Add Video capabilities to Capability Table */
      entry = (H245CapabilityTableEntry*) memAlloc(pctxt,
                                                   sizeof(H245CapabilityTableEntry));
      if(!entry)
      {
        OOTRACEERR3("Error:Memory - ooSendTermCapMsg - entry(video Cap)."
                    "(%s, %s)\n", call->callType, call->callToken);
        return OO_FAILED;
      }
      memset(entry, 0, sizeof(H245CapabilityTableEntry));
      entry->m.capabilityPresent = 1;
      if((epCap->dir & OORX))
      {
        entry->capability.t = T_H245Capability_receiveVideoCapability;
        entry->capability.u.receiveVideoCapability = videoCap;
      }
      else{
        entry->capability.t = T_H245Capability_transmitVideoCapability;
        entry->capability.u.transmitVideoCapability = videoCap;
      }
      entry->capabilityTableEntryNumber = i+1;
      dListAppend(pctxt , &(termCap->capabilityTable), entry);
      i++;
    }
  }

  OOTRACEAST(OOTRCLVLDBGA,"[H323/CODEC] ooSendTermCapMsg : dtmf mode [%d] \n",call->dtmfmode);
  /* Add dtmf capability, if any */
  if(call->dtmfmode & OO_CAP_DTMF_RFC2833)
  {
    OOTRACEAST(OOTRCLVLDBGA,"[H323/CODEC] ooSendTermCapMsg : %d send dtmf 2833\n",i);  
    ateCap = (H245AudioTelephonyEventCapability*)
      ooCapabilityCreateDTMFCapability(OO_CAP_DTMF_RFC2833, pctxt);
    if(!ateCap)
    {
      OOTRACEWARN3("WARN:Failed to add RFC2833 cap to TCS(%s, %s)\n",
                   call->callType, call->callToken);
    }
    else {
      entry = (H245CapabilityTableEntry*) memAlloc(pctxt,
                                                   sizeof(H245CapabilityTableEntry));
      if(!entry)
      {
        OOTRACEERR3("Error:Failed to allocate memory for new capability "
                    "table entry. (%s, %s)\n", call->callType, 
                    call->callToken);
        ooFreeH245Message(call, ph245msg);
        return OO_FAILED;
      }
            
      memset(entry, 0, sizeof(H245CapabilityTableEntry));
      entry->m.capabilityPresent = 1;

      entry->capability.t = T_H245Capability_receiveRTPAudioTelephonyEventCapability;
      entry->capability.u.receiveRTPAudioTelephonyEventCapability = ateCap;
      
      entry->capabilityTableEntryNumber = i+1;
      dListAppend(pctxt , &(termCap->capabilityTable), entry);

      i++;
    }
  }

  if(call->dtmfmode & OO_CAP_DTMF_H245_alphanumeric)
  {
    OOTRACEAST(OOTRCLVLDBGA,"[H323/CODEC] ooSendTermCapMsg : %d send dtmf alpha\n",i);  
    userInputCap = (H245UserInputCapability*)ooCapabilityCreateDTMFCapability
      (OO_CAP_DTMF_H245_alphanumeric, pctxt);
    if(!userInputCap)
    {
      OOTRACEWARN3("WARN:Failed to add H245(alphanumeric) cap to "
                   "TCS(%s, %s)\n", call->callType, call->callToken);
    }
    else {
      entry = (H245CapabilityTableEntry*) memAlloc(pctxt,
                                                   sizeof(H245CapabilityTableEntry));
      if(!entry)
      {
        OOTRACEERR3("Error:Failed to allocate memory for new capability "
                    "table entry. (%s, %s)\n", call->callType, 
                    call->callToken);
        ooFreeH245Message(call, ph245msg);
        return OO_FAILED;
      }
            
      memset(entry, 0, sizeof(H245CapabilityTableEntry));
      entry->m.capabilityPresent = 1;

      entry->capability.t = T_H245Capability_receiveUserInputCapability;
      entry->capability.u.receiveUserInputCapability = userInputCap;
      
      entry->capabilityTableEntryNumber = i+1;
      dListAppend(pctxt , &(termCap->capabilityTable), entry);

      i++;
    }
  }
  userInputCap = NULL;
  if(call->dtmfmode & OO_CAP_DTMF_H245_signal)
  {
    OOTRACEAST(OOTRCLVLDBGA,"[H323/CODEC] ooSendTermCapMsg : %d send dtmf signal\n",i);  
    userInputCap = (H245UserInputCapability*)ooCapabilityCreateDTMFCapability
      (OO_CAP_DTMF_H245_signal, pctxt);
    if(!userInputCap)
    {
      OOTRACEWARN3("WARN:Failed to add H245(signal) cap to "
                   "TCS(%s, %s)\n", call->callType, call->callToken);
    }
    else {
      entry = (H245CapabilityTableEntry*) memAlloc(pctxt,
                                                   sizeof(H245CapabilityTableEntry));
      if(!entry)
      {
        OOTRACEERR3("Error:Failed to allocate memory for new capability "
                    "table entry. (%s, %s)\n", call->callType, 
                    call->callToken);
        ooFreeH245Message(call, ph245msg);
        return OO_FAILED;
      }
            
      memset(entry, 0, sizeof(H245CapabilityTableEntry));
      entry->m.capabilityPresent = 1;

      entry->capability.t = T_H245Capability_receiveUserInputCapability;
      entry->capability.u.receiveUserInputCapability = userInputCap;
      
      entry->capabilityTableEntryNumber = i+1;
      dListAppend(pctxt , &(termCap->capabilityTable), entry);

      i++;
    }
  }

          
  /*TODO:Add Video and Data capabilities, if required*/
  if(i==0)
  {
    OOTRACEERR3("Error:No capabilities found to send in TCS message."
                " (%s, %s)\n", call->callType, call->callToken);
    ooFreeH245Message(call,ph245msg);
    return OO_FAILED;
  }
      
  /* Define capability descriptior */
  capDesc = (H245CapabilityDescriptor*)
    memAlloc(pctxt, sizeof(H245CapabilityDescriptor));
  memset(capDesc, 0, sizeof(H245CapabilityDescriptor));
  capDesc->m.simultaneousCapabilitiesPresent = 1;
  capDesc->capabilityDescriptorNumber = 1;
  dListInit(&(capDesc->simultaneousCapabilities));
  /* Add Alternative Capability Set.
     TODO: Right now all capabilities are added in separate
     alternate capabilities set. Need a way for application
     developer to specify the alternative capability sets.
  */
  OOTRACEAST(OOTRCLVLDBGA,"[H323/CODEC] ooSendTermCapMsg : %d codec build list\n",i);  
  for(j=0; j<i; j++)
  {
    altSet = (H245AlternativeCapabilitySet*)
      memAlloc(pctxt, sizeof(H245AlternativeCapabilitySet));
    memset(altSet, 0, sizeof(H245AlternativeCapabilitySet));
    altSet->n = 1;
    altSet->elem[0] = j+1;
   
    dListAppend(pctxt, &(capDesc->simultaneousCapabilities), altSet);
  }

  dListInit(&(termCap->capabilityDescriptors));
  dListAppend(pctxt, &(termCap->capabilityDescriptors), capDesc);

  OOTRACEAST(OOTRCLVLDBGA,"Built terminal capability send message (%s, %s)\n", 
             call->callType, call->callToken);
  ret = ooSendH245Msg(call, ph245msg);
  if(ret != OO_OK)
  {
    OOTRACEERR3("Error:Failed to enqueue TCS message to outbound queue. "
                "(%s, %s)\n", call->callType, call->callToken);
  }
  else {
    call->localTermCapState = OO_LocalTermCapSetSent;
  }

  ooFreeH245Message(call,ph245msg);

  return ret;
}


ASN1UINT ooGenerateStatusDeterminationNumber()
{
  ASN1UINT statusDeterminationNumber;
  ASN1UINT random_factor = getpid();
#ifdef _WIN32
  SYSTEMTIME systemTime;
  GetLocalTime(&systemTime);
  srand((systemTime.wMilliseconds ^ systemTime.wSecond) + random_factor);
#else
  struct timeval tv;
  gettimeofday(&tv, NULL);
  srand((tv.tv_usec ^ tv.tv_sec) + random_factor );
#endif
  statusDeterminationNumber = rand()%16777215;
  OOTRACEAST(OOTRCLVLDBGA,"[MSD] ooGenerateStatusDeterminationNumber default mode[%s] statusDeterminationNumber[%d] \n",
             msd_mode2str[defMSDMode],statusDeterminationNumber);
  return statusDeterminationNumber;
}

/* TODO: Should Send MasterSlave Release when no response from 
   Remote endpoint after MasterSlaveDetermination sent within
   timeout.
*/
int ooHandleMasterSlave(OOH323CallData *call, void * pmsg, 
                          int msgType)
{
   H245MasterSlaveDetermination *masterSlave;
   H245MasterSlaveDeterminationAck *masterSlaveAck;
   ASN1UINT statusDeterminationNumber;

   switch(msgType)
   {
      case OOMasterSlaveDetermination:
        /* 
           http://web.stsproject.net/index.php/cisco/h323/42-h245-masterslave-determination
           The determination principle is: 
           the endpoint with larger terminaltype value is "Master".
           If the terminaltype values are the same, the endpoint with larger StatusDeterminationNumber is Master.
        */
        masterSlave = (H245MasterSlaveDetermination*)pmsg;
        OOTRACEAST(OOTRCLVLDBGA,"[MSD] Master Slave Determination received  type (distant/local) [%d/%d]  number[%d/%d] MSD aldready sent[%s] default mode[%s]  (%s, %s)\n",
                   masterSlave->terminalType , gH323ep.termType,
                   masterSlave->statusDeterminationNumber , (call->masterSlaveState >= OO_MasterSlave_DetermineSent)?call->statusDeterminationNumber:0,
                   (call->masterSlaveState >= OO_MasterSlave_DetermineSent)?"true":"false",
                   msd_mode2str[defMSDMode],
                   call->callType, call->callToken);
         
         if(masterSlave->terminalType < gH323ep.termType)
         {
            ooSendMasterSlaveDeterminationAck(call, "slave");
            call->masterSlaveState =  OO_MasterSlave_AckSent;
            OOTRACEINFO3("[MSD] MasterSlaveDetermination by type -distant[slave] local[master](%s, %s)\n", call->callType, call->callToken);
            return OO_OK;
         }
         else if(masterSlave->terminalType > gH323ep.termType)
         {
           ooSendMasterSlaveDeterminationAck(call, "master");
           call->masterSlaveState =  OO_MasterSlave_AckSent;
           OOTRACEINFO3("[MSD] MasterSlaveDetermination by type -distant[master] local[slave]  (%s, %s)\n", call->callType, call->callToken);
           return OO_OK;
         }


         /* Since term types are same, master slave determination will
            be done based on statusdetermination number
         */
         OOTRACEINFO3("[MSD] Determining master-slave based on StatusDetermination"
                      "Number (%s, %s)\n", call->callType, call->callToken);
         if(call->masterSlaveState == OO_MasterSlave_DetermineSent){
           statusDeterminationNumber = call->statusDeterminationNumber;
         } else {
           statusDeterminationNumber = ooGenerateStatusDeterminationNumber();
         }
         call->statusDeterminationNumber = statusDeterminationNumber ; 
   
#if 0
         if(masterSlave->statusDeterminationNumber >  statusDeterminationNumber)
         {
           ooSendMasterSlaveDeterminationAck(call, "master");
           call->masterSlaveState = OO_MasterSlave_AckSent;
           OOTRACEINFO3("[MSD] MasterSlaveDetermination send ACK - distant[master] local[slave] (%s, %s)\n", call->callType, call->callToken);
           return OO_OK;
         }
         else if(masterSlave->statusDeterminationNumber < statusDeterminationNumber)
         {
           ooSendMasterSlaveDeterminationAck(call, "slave");
           call->masterSlaveState = OO_MasterSlave_AckSent ;
           OOTRACEINFO3("[MSD] MasterSlaveDetermination send ACK - distant[slave] local[master] (%s, %s)\n", call->callType, call->callToken);
           return OO_OK;
         }
         else
         {
           ooSendMasterSlaveDeterminationReject (call);
           OOTRACEERR3("[MSD] ERROR:MasterSlaveDetermination failed- identical "
                       "numbers (%s, %s)\n", call->callType, call->callToken);
         }
#else
         /* According to the the H.245 Spec (05/2006) Figure C4, the calculation is as follows:
            MAX_SDN = (2^24)-1
            SDN_DIFF = (remoteSDN - localSDN)
            if ((SDN_DIFF MOD MAX_SND ) < 2^23 ) //  2^23 == 0xFFFFFF
            {
            MASTER
            } else {
            SLAVE
            } 
         */
         ASN1UINT moduloDiff = (masterSlave->statusDeterminationNumber - statusDeterminationNumber)&0xffffff;
         if (moduloDiff == 0 || moduloDiff == 0x800000){
           ooSendMasterSlaveDeterminationReject (call);
           OOTRACEERR3("[MSD] ERROR:MasterSlaveDetermination failed- identical "
                       "numbers (%s, %s)\n", call->callType, call->callToken);
         }
         else if (moduloDiff < 0x800000){
           ooSendMasterSlaveDeterminationAck(call, "slave");
           call->masterSlaveState = OO_MasterSlave_AckSent ;
           OOTRACEINFO3("[MSD] MasterSlaveDetermination send ACK - distant[slave] local[master] (%s, %s)\n", call->callType, call->callToken);
           return OO_OK;
         }
         else{
           ooSendMasterSlaveDeterminationAck(call, "master");
           call->masterSlaveState = OO_MasterSlave_AckSent;
           OOTRACEINFO3("[MSD] MasterSlaveDetermination send ACK - distant[master] local[slave] (%s, %s)\n", call->callType, call->callToken);
           return OO_OK;
         }
#endif
         break;

     case OOMasterSlaveAck:
       masterSlaveAck = (H245MasterSlaveDeterminationAck*)pmsg;
       OOTRACEAST(OOTRCLVLDBGA,"[MSD] Master Slave ACK received  Distant[%s]  MSD aldready sent[%s] default mode[%s]  (%s, %s)\n",
                  (masterSlaveAck->decision.t ==  T_H245MasterSlaveDeterminationAck_decision_master)?"slave":"master" ,
                  (call->masterSlaveState >= OO_MasterSlave_DetermineSent)?"true":"false",
                  msd_mode2str[defMSDMode],
                  call->callType, call->callToken);

       if(call->masterSlaveState == OO_MasterSlave_AckSent)   
       {   
         if(masterSlaveAck->decision.t == T_H245MasterSlaveDeterminationAck_decision_master) {
           OOTRACEINFO3("[MSD] MasterSlaveDetermination ACK aldready sent - distant[slave] local[master] (%s, %s)\n", call->callType, call->callToken); 
           call->masterSlaveState = OO_MasterSlave_Master;   
         }else{
           OOTRACEINFO3("[MSD] MasterSlaveDetermination ACK aldready sent - distant[master] local[slave] (%s, %s)\n", call->callType, call->callToken); 
           call->masterSlaveState = OO_MasterSlave_Slave;   
           return OO_OK;   
         }
       }   

       if(call->masterSlaveState == OO_MasterSlave_DetermineSent)   
       {   
         if(masterSlaveAck->decision.t ==  T_H245MasterSlaveDeterminationAck_decision_master)   
         {   
           ooSendMasterSlaveDeterminationAck(call, "slave");  
           OOTRACEINFO3("[MSD] MasterSlaveDetermination send ACK  - distant[slave] local[master] (%s, %s)\n", call->callType, call->callToken);
           call->masterSlaveState =  OO_MasterSlave_Master;   
           return OO_OK;   
         }   
         else   
         {   
           ooSendMasterSlaveDeterminationAck(call, "master");   
           OOTRACEINFO3("[MSD] MasterSlaveDetermination send ACK  - distant[master] local[slave] (%s, %s)\n", call->callType, call->callToken);
           call->masterSlaveState = OO_MasterSlave_Slave;   
           return OO_OK;   
         }   
       }

       //phv send first OLC if master mode      
       if( call->localTermCapState == OO_LocalTermCapSetAckRecvd &&
           call->remoteTermCapState == OO_RemoteTermCapSetAckSent && 
           call->masterSlaveState ==  OO_MasterSlave_Master ) 
       {
         /*Since Cap exchange and MasterSlave Procedures are done */
         OOTRACEAST(OOTRCLVLDBGA,"[MSD] MasterSlaveDetermination : Open Logical Channels[%s] (%s, %s) \n", 
                    (call->masterSlaveState == OO_MasterSlave_Master)?"master":"slave",
                    call->callType, call->callToken );
         if(!call->logicalChans){
           if(!gH323ep.h323Callbacks.openLogicalChannels)
             ooOpenLogicalChannels(call);
           else
             gH323ep.h323Callbacks.openLogicalChannels(call);
         }
       }
       else
         OOTRACEAST(OOTRCLVLDBGA,"[MSD] MasterSlaveDetermination :Not opening logical channels now MSD[%s] localTermCapStateAck[%s] remoteTermCapState[%s] (%s, %s) \n",
                    (call->masterSlaveState == OO_MasterSlave_Master)?"master":"slave",
                    (call->localTermCapState == OO_LocalTermCapSetAckRecvd)?"true":"false" ,
                    (call->remoteTermCapState == OO_RemoteTermCapSetAckSent)?"true":"false", 
                    call->callType, call->callToken);
       break;
     default:
       OOTRACEWARN3("Warn:Unhandled Master Slave message received - %s - "
                    "%s\n", call->callType, call->callToken);
   }
   return OO_OK;      
}

#ifdef  CONFIG_CUSTOM
int ooH245SetSendMSD(int value)
{

  OOTRACEAST(OOTRCLVLDBGA,"ooH245SetSendMSD :"
             "[%s] \n", 
             (value)?"true":"false" );
  SendMSD=value;
  return SendMSD ;
}
#endif

void ooH245setMsdMode( char* defaultMode )
{
  int Status = TRUE ;
  if ( strncmp(msd_mode2str[MSD_MODE_MASTER], defaultMode , STR_MSD_MODE_SIZE ) == 0 )
  {
    defMSDMode=MSD_MODE_MASTER ;
  }
  else if ( strncmp(msd_mode2str[MSD_MODE_SLAVE], defaultMode , STR_MSD_MODE_SIZE ) == 0 )
  {
    defMSDMode=MSD_MODE_SLAVE ;
  }
  else
  {
    defMSDMode=MSD_MODE_UNDEF ; 
  }
  OOTRACEAST(OOTRCLVLDBGA,"Default MSD mode  :[%s] \n",msd_mode2str[defMSDMode]  );
}

int ooH245SetCheckOLC(int value)
{

  OOTRACEAST(OOTRCLVLDBGA,"ooH245SetCheckOLC"
             "[%s] \n", 
             (value)?"true":"false" );
  CheckOLC=value;
  return CheckOLC ;
}

int ooH245GetCheckOLC()
{

  OOTRACEAST(OOTRCLVLDBGA,"ooH245GetCheckOLC"
             "[%s] \n", 
             (CheckOLC)?"true":"false" );
  return CheckOLC ;
}
#ifdef  CONFIG_CUSTOM
int ooH245SetWaitOLC(int value)
{

  OOTRACEAST(OOTRCLVLDBGA,"ooH245SetWaitOLC:"
             "[%s] \n", 
             (value)?"true":"false" );
  WaitOLC=value;
  return WaitOLC ;
}


#endif
int ooSendMasterSlaveDetermination(OOH323CallData *call, int force )
{
   int ret;
   H245Message* ph245msg=NULL;
   H245RequestMessage *request;
   OOCTXT *pctxt=&gH323ep.msgctxt;
   H245MasterSlaveDetermination* pMasterSlave;
#ifdef  CONFIG_CUSTOM
   if ( !SendMSD && !force )
   {
      OOTRACEAST(OOTRCLVLDBGA,"MasterSlave determination configuration : not send MSD (%s, %s)\n",
                   call->callType, call->callToken);
      return OO_OK;     
   }
#endif
   /* Check whether Master Slave Determination already in progress */
   if(call->masterSlaveState != OO_MasterSlave_Idle)
   {
      OOTRACEINFO3("MasterSlave determination already in progress (%s, %s)\n",
                   call->callType, call->callToken);
      return OO_OK;
   }

   ret = ooCreateH245Message(&ph245msg,
                   T_H245MultimediaSystemControlMessage_request);
   if(ret != OO_OK)
   {
      OOTRACEERR3("Error: creating H245 message - MasterSlave Determination "
                  "(%s, %s)\n", call->callType, call->callToken);
      return OO_FAILED;
   }
   ph245msg->msgType = OOMasterSlaveDetermination;
   request = ph245msg->h245Msg.u.request;
   request->t = T_H245RequestMessage_masterSlaveDetermination;
   request->u.masterSlaveDetermination = (H245MasterSlaveDetermination*)
            ASN1MALLOC(pctxt, sizeof(H245MasterSlaveDetermination));

   
   pMasterSlave = request->u.masterSlaveDetermination;
   memset(pMasterSlave, 0, sizeof(H245MasterSlaveDetermination));   
   pMasterSlave->terminalType = gH323ep.termType; 
   pMasterSlave->statusDeterminationNumber = ooGenerateStatusDeterminationNumber();
   call->statusDeterminationNumber = pMasterSlave->statusDeterminationNumber;

   OOTRACEDBGA3("Built MasterSlave Determination (%s, %s)\n", call->callType,
                 call->callToken); 
   ret = ooSendH245Msg(call, ph245msg);

   if(ret != OO_OK)
   {
      OOTRACEERR3("Error:Failed to enqueue MasterSlaveDetermination message to"
                  " outbound queue. (%s, %s)\n", call->callType, 
                  call->callToken);
   }
   else
      call->masterSlaveState = OO_MasterSlave_DetermineSent;
   
   ooFreeH245Message(call, ph245msg);

   return ret;
}

int ooSendMasterSlaveDeterminationAck(OOH323CallData* call,
                                      char * status)
{
   int ret=0;
   H245ResponseMessage * response=NULL;
   H245Message *ph245msg=NULL;
   OOCTXT *pctxt=&gH323ep.msgctxt;

   ret = ooCreateH245Message(&ph245msg, 
                      T_H245MultimediaSystemControlMessage_response);
   if(ret != OO_OK)
   {
      OOTRACEERR3("Error:H245 message creation failed for - MasterSlave "
                  "Determination Ack (%s, %s)\n",call->callType, 
                  call->callToken);
      return OO_FAILED;
   }
   ph245msg->msgType = OOMasterSlaveAck;
   response = ph245msg->h245Msg.u.response;
   memset(response, 0, sizeof(H245ResponseMessage));
   response->t = T_H245ResponseMessage_masterSlaveDeterminationAck;
   response->u.masterSlaveDeterminationAck = (H245MasterSlaveDeterminationAck*)
                   ASN1MALLOC(pctxt, sizeof(H245MasterSlaveDeterminationAck));
   memset(response->u.masterSlaveDeterminationAck, 0, 
                             sizeof(H245MasterSlaveDeterminationAck));
   if(!strcmp("master", status))
      response->u.masterSlaveDeterminationAck->decision.t = 
                         T_H245MasterSlaveDeterminationAck_decision_master;
   else
      response->u.masterSlaveDeterminationAck->decision.t = 
                         T_H245MasterSlaveDeterminationAck_decision_slave;
   
   OOTRACEDBGA3("Built MasterSlave determination Ack (%s, %s)\n", 
                call->callType, call->callToken);
   ret = ooSendH245Msg(call, ph245msg);
   if(ret != OO_OK)
   {
      OOTRACEERR3("Error:Failed to enqueue MasterSlaveDeterminationAck message"
                  " to outbound queue. (%s, %s)\n", call->callType, 
                  call->callToken);
   }
   
   ooFreeH245Message(call, ph245msg);
   return ret;
}

int ooSendFlowControlAck(OOH323CallData* call)
{
    int ret;
   H245CommandMessage * command;
   OOCTXT *pctxt;
   H245Message *ph245msg=NULL;
#if 1
   ret = ooCreateH245Message(&ph245msg, 
                      T_H245MultimediaSystemControlMessage_command);
   if(ret != OO_OK)
   {
      OOTRACEERR3("Error: H245 message creation failed for - Send Flow Control Ack "
                  "Command (%s, %s)\n", call->callType, call->callToken);
      return OO_FAILED;
   }
   ph245msg->msgType = OOflowControlCommand ;

   command = ph245msg->h245Msg.u.command;
   pctxt = &gH323ep.msgctxt;
   memset(command, 0, sizeof(H245CommandMessage));
   command->t = T_H245CommandMessage_flowControlCommand ;
   command->u.flowControlCommand = (H245FlowControlCommand*) ASN1MALLOC(pctxt,
                                  sizeof(H245FlowControlCommand ));
   memset(command->u.flowControlCommand, 0, sizeof(H245FlowControlCommand));

   command->u.flowControlCommand->scope.t = (ASN1UINT)T_H245FlowControlCommand_scope_logicalChannelNumber ;
   ooH323EpCapability* EpCap = NULL ; 
   command->u.flowControlCommand->scope.u.logicalChannelNumber = ooGetVideoLogicalChannel(call , &EpCap );
   /* todo get ooH323EpCapability->cap */
   /* OOH264CapParams *params=epCap->params ou OOCapParams *params=epCap->params ou OOH263CapParams *params=epCap->params */
   /* and get bitrate */
   command->u.flowControlCommand->restriction.t = (ASN1UINT)T_H245FlowControlCommand_restriction_noRestriction ; 

/*
  typedef struct EXTERN H245FlowControlCommand {
  H245FlowControlCommand_scope scope;
  H245FlowControlCommand_restriction restriction;
  } H245FlowControlCommand;
  
  typedef struct EXTERN H245FlowControlCommand_scope {
  int t;
  union {
  // t = 1 
  H245LogicalChannelNumber logicalChannelNumber;
  // t = 2 
  ASN1USINT resourceID;
  // t = 3 
  } u;
  } H245FlowControlCommand_scope;

  typedef struct EXTERN H245FlowControlCommand_restriction {
  int t;
  union {
  // t = 1 
  ASN1UINT maximumBitRate;
  // t = 2 
  } u;
  } H245FlowControlCommand_restriction;
*/
   OOTRACEDBGA3("Built FlowControl Command (%s, %s)\n", call->callType,
                call->callToken);
   ret = ooSendH245Msg(call, ph245msg);
   if(ret != OO_OK)
   {
      OOTRACEERR3("Error:Failed to enqueue FlowControl message to outbound "
                  "queue.(%s, %s)\n", call->callType, call->callToken);
   }
   ooFreeH245Message(call, ph245msg);
#endif
   return ret;
}

int ooSendMasterSlaveDeterminationReject (OOH323CallData* call)
{
   int ret=0;
   H245ResponseMessage* response=NULL;
   H245Message *ph245msg=NULL;
   OOCTXT *pctxt=&gH323ep.msgctxt;

   ret = ooCreateH245Message
      (&ph245msg, T_H245MultimediaSystemControlMessage_response);

   if (ret != OO_OK) {
      OOTRACEERR3("Error:H245 message creation failed for - MasterSlave "
                  "Determination Reject (%s, %s)\n",call->callType, 
                  call->callToken);
      return OO_FAILED;
   }
   ph245msg->msgType = OOMasterSlaveReject;
   response = ph245msg->h245Msg.u.response;

   response->t = T_H245ResponseMessage_masterSlaveDeterminationReject;

   response->u.masterSlaveDeterminationReject = 
      (H245MasterSlaveDeterminationReject*)
      memAlloc (pctxt, sizeof(H245MasterSlaveDeterminationReject));

   response->u.masterSlaveDeterminationReject->cause.t =
      T_H245MasterSlaveDeterminationReject_cause_identicalNumbers;

   OOTRACEDBGA3 ("Built MasterSlave determination reject (%s, %s)\n", 
                 call->callType, call->callToken);

   ret = ooSendH245Msg (call, ph245msg);

   if (ret != OO_OK) {
      OOTRACEERR3 
         ("Error:Failed to enqueue MasterSlaveDeterminationReject "
          "message to outbound queue.(%s, %s)\n", call->callType, 
          call->callToken);
   }
   
   ooFreeH245Message (call, ph245msg);

   return ret;
}

int ooSendMasterSlaveDeterminationRelease(OOH323CallData * call)
{
   int ret=0;
   H245IndicationMessage* indication=NULL;
   H245Message *ph245msg=NULL;
   OOCTXT *pctxt=&gH323ep.msgctxt;

   ret = ooCreateH245Message
      (&ph245msg, T_H245MultimediaSystemControlMessage_indication);

   if (ret != OO_OK) {
      OOTRACEERR3("Error:H245 message creation failed for - MasterSlave "
                  "Determination Release (%s, %s)\n",call->callType, 
                  call->callToken);
      return OO_FAILED;
   }
   ph245msg->msgType = OOMasterSlaveRelease;
   indication = ph245msg->h245Msg.u.indication;

   indication->t = T_H245IndicationMessage_masterSlaveDeterminationRelease;

   indication->u.masterSlaveDeterminationRelease = 
      (H245MasterSlaveDeterminationRelease*)
      memAlloc (pctxt, sizeof(H245MasterSlaveDeterminationRelease));

   if(!indication->u.masterSlaveDeterminationRelease)
   {
      OOTRACEERR3("Error: Failed to allocate memory for MSDRelease message."
                  " (%s, %s)\n", call->callType, call->callToken);
      ooFreeH245Message(call, ph245msg);
      return OO_FAILED;
   }
   OOTRACEDBGA3 ("Built MasterSlave determination Release (%s, %s)\n", 
                 call->callType, call->callToken);

   ret = ooSendH245Msg (call, ph245msg);

   if (ret != OO_OK) {
      OOTRACEERR3 
        ("Error:Failed to enqueue MasterSlaveDeterminationRelease "
        "message to outbound queue.(%s, %s)\n", call->callType, 
        call->callToken);
   }
   
   ooFreeH245Message (call, ph245msg);
   return ret;
}

int ooHandleMasterSlaveReject
   (OOH323CallData *call, H245MasterSlaveDeterminationReject* reject)
{
   if(call->msdRetries < DEFAULT_MAX_RETRIES)
   {
      call->msdRetries++;
      OOTRACEDBGA3("Retrying MasterSlaveDetermination. (%s, %s)\n", 
                    call->callType, call->callToken);
      call->masterSlaveState = OO_MasterSlave_Idle;
      ooSendMasterSlaveDetermination(call,TRUE);
      return OO_OK;
   }
   OOTRACEERR3("Error:Failed to complete MasterSlaveDetermination - "
               "Ending call. (%s, %s)\n", call->callType, call->callToken);
   if(call->callState < OO_CALL_CLEAR)
   {
      call->callEndReason = OO_REASON_LOCAL_CLEARED;
      call->callState = OO_CALL_CLEAR;
   }
   return OO_OK;
}


int ooHandleOpenLogicalChannel(OOH323CallData* call, 
                               H245OpenLogicalChannel *olc)
{
  H245OpenLogicalChannel_forwardLogicalChannelParameters *flcp = &(olc->forwardLogicalChannelParameters);
   

  /* Check whether channel type is supported. Only supported channel 
     type for now is g711ulaw audio channel.
  */
  switch(flcp->dataType.t)
  {
    case T_H245DataType_nonStandard:
      OOTRACEWARN3("Warn:Media channel data type "
                   "'T_H245DataType_nonStandard' not supported (%s, %s)\n",
                   call->callType, call->callToken);
      ooSendOpenLogicalChannelReject(call, olc->forwardLogicalChannelNumber,
                                     T_H245OpenLogicalChannelReject_cause_dataTypeNotSupported);
      break;
    case T_H245DataType_nullData:
      OOTRACEWARN3("Warn:Media channel data type "
                   "'T_H245DataType_nullData' not supported (%s, %s)\n",
                   call->callType, call->callToken);
      ooSendOpenLogicalChannelReject(call, olc->forwardLogicalChannelNumber,
                                     T_H245OpenLogicalChannelReject_cause_dataTypeNotSupported);
      break;
    case T_H245DataType_videoData:
    case T_H245DataType_audioData:
      ooHandleOpenLogicalChannel_helper(call, olc);
      break;
    case T_H245DataType_data:
      OOTRACEWARN3("Warn:Media channel data type "
                   "'T_H245DataType_data' not supported (%s, %s)\n",
                   call->callType, call->callToken);
      ooSendOpenLogicalChannelReject(call, olc->forwardLogicalChannelNumber,
                                     T_H245OpenLogicalChannelReject_cause_dataTypeNotSupported);
      break;
    case T_H245DataType_encryptionData:
      OOTRACEWARN3("Warn:Media channel data type "
                   "'T_H245DataType_encryptionData' not supported (%s, %s)\n",
                   call->callType, call->callToken);
      ooSendOpenLogicalChannelReject(call, olc->forwardLogicalChannelNumber,
                                     T_H245OpenLogicalChannelReject_cause_dataTypeNotSupported);
      break;
    case T_H245DataType_h235Control:
      OOTRACEWARN3("Warn:Media channel data type "
                   "'T_H245DataType_h235Control' not supported (%s, %s)\n",
                   call->callType, call->callToken);
      ooSendOpenLogicalChannelReject(call, olc->forwardLogicalChannelNumber,
                                     T_H245OpenLogicalChannelReject_cause_dataTypeNotSupported);
      break;
    case T_H245DataType_h235Media:
      OOTRACEWARN3("Warn:Media channel data type "
                   "'T_H245DataType_h235Media' not supported (%s, %s)\n",
                   call->callType, call->callToken);
      ooSendOpenLogicalChannelReject(call, olc->forwardLogicalChannelNumber,
                                     T_H245OpenLogicalChannelReject_cause_dataTypeNotSupported);
      break;
    case T_H245DataType_multiplexedStream:
      OOTRACEWARN3("Warn:Media channel data type "
                   "'T_H245DataType_multiplexedStream' not supported(%s, %s)\n",
                   call->callType, call->callToken);
      ooSendOpenLogicalChannelReject(call, olc->forwardLogicalChannelNumber,
                                     T_H245OpenLogicalChannelReject_cause_dataTypeNotSupported);
      break;
    case T_H245DataType_redundancyEncoding:
      OOTRACEWARN3("Warn:Media channel data type "
                   "'T_H245DataType_redundancyEncoding' not supported (%s, %s)\n",
                   call->callType, call->callToken);
      ooSendOpenLogicalChannelReject(call, olc->forwardLogicalChannelNumber,
                                     T_H245OpenLogicalChannelReject_cause_dataTypeNotSupported);
      break;
    case T_H245DataType_multiplePayloadStream:
      OOTRACEWARN3("Warn:Media channel data type "
                   "'T_H245DataType_multiplePayloadStream' not supported (%s, %s)\n",
                   call->callType, call->callToken);
      ooSendOpenLogicalChannelReject(call, olc->forwardLogicalChannelNumber,
                                     T_H245OpenLogicalChannelReject_cause_dataTypeNotSupported);
      break;
    case T_H245DataType_fec:
      OOTRACEWARN3("Warn:Media channel data type 'T_H245DataType_fec' not "
                   "supported (%s, %s)\n", call->callType, call->callToken);
      ooSendOpenLogicalChannelReject(call, olc->forwardLogicalChannelNumber,
                                     T_H245OpenLogicalChannelReject_cause_dataTypeNotSupported);
      break;
    default:
      OOTRACEERR3("ERROR:Unknown media channel data type (%s, %s)\n", 
                  call->callType, call->callToken);
      ooSendOpenLogicalChannelReject(call, olc->forwardLogicalChannelNumber,
                                     T_H245OpenLogicalChannelReject_cause_dataTypeNotSupported);
  }
  return OO_OK;
}       

/*TODO: Need to clean logical channel in case of failure after creating one */
int ooHandleOpenLogicalChannel_helper(OOH323CallData *call, 
                                      H245OpenLogicalChannel*olc)
{
   int ret=0;
   H245Message *ph245msg=NULL;
   H245ResponseMessage *response;
   H245OpenLogicalChannelAck *olcAck;
   ooH323EpCapability *epCap=NULL;
   H245H2250LogicalChannelAckParameters *h2250lcap=NULL;
   OOCTXT *pctxt;
   H245UnicastAddress *unicastAddrs, *unicastAddrs1;
   H245UnicastAddress_iPAddress *iPAddress, *iPAddress1;
   ooLogicalChannel *pLogicalChannel = NULL;
   H245H2250LogicalChannelParameters *h2250lcp=NULL;
   H245OpenLogicalChannel_forwardLogicalChannelParameters *flcp =
    &(olc->forwardLogicalChannelParameters);

   if(!flcp || flcp->multiplexParameters.t != T_H245OpenLogicalChannel_forwardLogicalChannelParameters_multiplexParameters_h2250LogicalChannelParameters)
   {
      OOTRACEERR3("Error:ooHandleOpenLogicalChannel_helper - invalid forward "
                 "logical channel parameters. (%s, %s)\n", call->callType, 
                 call->callToken);
      ooSendOpenLogicalChannelReject(call, olc->forwardLogicalChannelNumber,
          T_H245OpenLogicalChannelReject_cause_unspecified);
      return OO_FAILED;
   }

   h2250lcp = flcp->multiplexParameters.u.h2250LogicalChannelParameters;

   if(!(epCap=ooIsDataTypeSupported(call, &flcp->dataType, OORX)))
   {
      OOTRACEERR3("ERROR:HandleOpenLogicalChannel_helper - capability not "
                  "supported (%s, %s)\n", call->callType, call->callToken);

      ooSendOpenLogicalChannelReject(call, olc->forwardLogicalChannelNumber,
          T_H245OpenLogicalChannelReject_cause_dataTypeNotSupported);
      return OO_FAILED;
   }
   /* Generate an Ack for the open channel request */
   ret = ooCreateH245Message(&ph245msg,
                             T_H245MultimediaSystemControlMessage_response);
   if(ret != OO_OK)
   {
      OOTRACEERR3("Error: H245 message creation failed for - "
                  "OpenLogicalChannel Ack (%s, %s)\n", call->callType, 
                  call->callToken);
      memFreePtr(call->pctxt, epCap);
      epCap = NULL;
      return OO_FAILED;
   }

   SetRemoteCapChoice( call , epCap );

   ph245msg->msgType = OOOpenLogicalChannelAck;
   ph245msg->logicalChannelNo = olc->forwardLogicalChannelNumber;
   response = ph245msg->h245Msg.u.response;
   pctxt = &gH323ep.msgctxt;
   memset(response, 0, sizeof(H245ResponseMessage));
   response->t = T_H245ResponseMessage_openLogicalChannelAck;
   response->u.openLogicalChannelAck = (H245OpenLogicalChannelAck*)
                   memAlloc(pctxt, sizeof(H245OpenLogicalChannelAck));   
   olcAck = response->u.openLogicalChannelAck;
   memset(olcAck, 0, sizeof(H245OpenLogicalChannelAck));
   olcAck->forwardLogicalChannelNumber = olc->forwardLogicalChannelNumber;

   olcAck->m.forwardMultiplexAckParametersPresent = 1;
   olcAck->forwardMultiplexAckParameters.t = 
     T_H245OpenLogicalChannelAck_forwardMultiplexAckParameters_h2250LogicalChannelAckParameters;
   olcAck->forwardMultiplexAckParameters.u.h2250LogicalChannelAckParameters = 
                      (H245H2250LogicalChannelAckParameters*)ASN1MALLOC(pctxt, 
                      sizeof(H245H2250LogicalChannelAckParameters));
   h2250lcap = 
      olcAck->forwardMultiplexAckParameters.u.h2250LogicalChannelAckParameters;
   memset(h2250lcap, 0, sizeof(H245H2250LogicalChannelAckParameters));

   h2250lcap->m.mediaChannelPresent = 1;
   h2250lcap->m.mediaControlChannelPresent = 1;
   h2250lcap->m.sessionIDPresent = 1;

   if(h2250lcp->sessionID == 0)
      h2250lcap->sessionID = ooCallGenerateSessionID(call, epCap->capType, "receive");
   else
      h2250lcap->sessionID = h2250lcp->sessionID;
   
   h2250lcap->mediaChannel.t = 
                         T_H245TransportAddress_unicastAddress;
   h2250lcap->mediaChannel.u.unicastAddress =  (H245UnicastAddress*)
                         ASN1MALLOC(pctxt, sizeof(H245UnicastAddress));

   unicastAddrs = h2250lcap->mediaChannel.u.unicastAddress;
   memset(unicastAddrs, 0, sizeof(H245UnicastAddress));
   unicastAddrs->t = T_H245UnicastAddress_iPAddress;
   unicastAddrs->u.iPAddress = (H245UnicastAddress_iPAddress*)
               memAlloc(pctxt, sizeof(H245UnicastAddress_iPAddress));
   iPAddress = unicastAddrs->u.iPAddress;
   memset(iPAddress, 0, sizeof(H245UnicastAddress_iPAddress));

   pLogicalChannel = ooAddNewLogicalChannel(call, 
                        olc->forwardLogicalChannelNumber, h2250lcap->sessionID,
                        "receive", epCap);
   if(!pLogicalChannel)
   {
      OOTRACEERR3("ERROR:Failed to add new logical channel entry to call " 
                  "(%s, %s)\n", call->callType, call->callToken);
      return OO_FAILED;
   }
   if ( h2250lcp->m.dynamicRTPPayloadTypePresent )
   {
      pLogicalChannel->rtpPayloadType = h2250lcp->dynamicRTPPayloadType;
      OOTRACEERR2("Got dynamic RTP payload type %d.\n", pLogicalChannel->rtpPayloadType );
   }
   ooSocketConvertIpToNwAddr(call->localIP, iPAddress->network.data);

   iPAddress->network.numocts = 4;
   iPAddress->tsapIdentifier = pLogicalChannel->localRtpPort;

   /* media contrcol channel */
   h2250lcap->mediaControlChannel.t = 
                                 T_H245TransportAddress_unicastAddress;
   h2250lcap->mediaControlChannel.u.unicastAddress =  (H245UnicastAddress*)
                         ASN1MALLOC(pctxt, sizeof(H245UnicastAddress));

   unicastAddrs1 = h2250lcap->mediaControlChannel.u.unicastAddress;
   memset(unicastAddrs1, 0, sizeof(H245UnicastAddress));
   unicastAddrs1->t = T_H245UnicastAddress_iPAddress;
   unicastAddrs1->u.iPAddress = (H245UnicastAddress_iPAddress*)
               memAlloc(pctxt, sizeof(H245UnicastAddress_iPAddress));
   iPAddress1 = unicastAddrs1->u.iPAddress;
   memset(iPAddress1, 0, sizeof(H245UnicastAddress_iPAddress));

   ooSocketConvertIpToNwAddr(call->localIP, iPAddress1->network.data);

   iPAddress1->network.numocts = 4;
   iPAddress1->tsapIdentifier = pLogicalChannel->localRtcpPort;

   OOTRACEDBGA3("Built OpenLogicalChannelAck (%s, %s)\n", call->callType, 
                 call->callToken);
   ret = ooSendH245Msg(call, ph245msg);
   if(ret != OO_OK)
   {
      OOTRACEERR3("Error:Failed to enqueue OpenLogicalChannelAck message to "
                  "outbound queue. (%s, %s)\n", call->callType, 
                  call->callToken);
   }
   ooFreeH245Message(call, ph245msg);

   if(epCap->startReceiveChannel)
   {
      epCap->startReceiveChannel(call, pLogicalChannel);      
      OOTRACEINFO6("Receive channel of type %s started at %s:%d(%s, %s)\n", 
                    ooGetCapTypeText(epCap->cap), call->localIP, 
                    pLogicalChannel->localRtpPort, call->callType, 
                    call->callToken);
   }
   else{
      OOTRACEERR3("ERROR:No callback registered to start receive audio "
                  "channel (%s, %s)\n", call->callType, call->callToken);
      return OO_FAILED;
   }
   pLogicalChannel->state = OO_LOGICALCHAN_ESTABLISHED;
   // Send olc on slave mode 
   if (  call->masterSlaveState == OO_MasterSlave_Slave ){
       if (  epCap->cap < OO_CAP_VIDEO_BASE ){
         OOTRACEAST(OOTRCLVLDBGA,"ooHandleOpenLogicalChannel :  Open Audio Logical Channels[%s] (%s, %s) \n", 
                    (call->masterSlaveState == OO_MasterSlave_Master)?"master":"slave",
                    call->callType, call->callToken );
 
         ret = ooOpenLogicalChannel(call, OO_CAP_TYPE_AUDIO);
         if(ret != OO_OK)
         {
           OOTRACEERR3("ERROR:ooHandleOpenLogicalChannel Failed to open audio channel. Clearing call.(%s, %s)\n", call->callType, call->callToken);
           if(call->callState < OO_CALL_CLEAR)
           {
             call->callEndReason = OO_REASON_LOCAL_CLEARED;
             call->callState = OO_CALL_CLEAR;
           }
           return ret;
         }
       }else{
       OOTRACEAST(OOTRCLVLDBGA,"ooHandleOpenLogicalChannel :  Open Video Logical Channels[%s] (%s, %s) \n", 
                    (call->masterSlaveState == OO_MasterSlave_Master)?"master":"slave",
                    call->callType, call->callToken );
 
         ret = ooOpenLogicalChannel(call, OO_CAP_TYPE_VIDEO);
         if(ret != OO_OK)
         {
           OOTRACEERR3("ERROR:ooHandleOpenLogicalChannel Failed to open video channel. Clearing call."
                       "(%s, %s)\n", call->callType, call->callToken);
           if(call->callState < OO_CALL_CLEAR)
           {
             call->callEndReason = OO_REASON_LOCAL_CLEARED;
             call->callState = OO_CALL_CLEAR;
           }
           return ret;
         }
       }
   }else{
     if(!call->logicalChans){
       OOTRACEAST(OOTRCLVLDBGA,"ooHandleOpenLogicalChannel : master mode but olc not send : Open Logical Channels[%s] (%s, %s) \n", 
                  (call->masterSlaveState == OO_MasterSlave_Master)?"master":"slave",
                  call->callType, call->callToken );
      if(!gH323ep.h323Callbacks.openLogicalChannels)
        ooOpenLogicalChannels(call);
      else
        gH323ep.h323Callbacks.openLogicalChannels(call);
     }
   }
   return ret;
}

int ooSendOpenLogicalChannelReject
   (OOH323CallData *call, ASN1UINT channelNum, ASN1UINT cause)
{
   int ret=0;
   H245ResponseMessage* response=NULL;
   H245Message *ph245msg=NULL;
   OOCTXT *pctxt=&gH323ep.msgctxt;

   ret = ooCreateH245Message
      (&ph245msg, T_H245MultimediaSystemControlMessage_response);

   if (ret != OO_OK) {
      OOTRACEERR3("Error:H245 message creation failed for - OpenLogicalChannel"
                  "Reject (%s, %s)\n",call->callType, 
                  call->callToken);
      return OO_FAILED;
   }
   ph245msg->msgType = OOOpenLogicalChannelReject;
   response = ph245msg->h245Msg.u.response;

   response->t = T_H245ResponseMessage_openLogicalChannelReject;

   response->u.openLogicalChannelReject = 
      (H245OpenLogicalChannelReject*)
      memAlloc (pctxt, sizeof(H245OpenLogicalChannelReject));

   if(!response->u.openLogicalChannelReject)
   {
      OOTRACEERR3("Error: Failed to allocate memory for OpenLogicalChannel"
                  "Reject message. (%s, %s)\n", call->callType, 
                  call->callToken);
      ooFreeH245Message(call, ph245msg);
      return OO_FAILED;
   }
   response->u.openLogicalChannelReject->forwardLogicalChannelNumber = 
                                                                 channelNum;
   response->u.openLogicalChannelReject->cause.t = cause;

   OOTRACEDBGA3 ("Built OpenLogicalChannelReject (%s, %s)\n", 
                 call->callType, call->callToken);

   ret = ooSendH245Msg (call, ph245msg);

   if (ret != OO_OK) {
      OOTRACEERR3 
         ("Error:Failed to enqueue OpenLogicalChannelReject "
         "message to outbound queue.(%s, %s)\n", call->callType, 
         call->callToken);
   }
   
   ooFreeH245Message (call, ph245msg);

   return ret;
}


int ooOnReceivedOpenLogicalChannelAck(OOH323CallData *call,
                                      H245OpenLogicalChannelAck *olcAck)
{
   char remoteip[20];
   ooLogicalChannel *pLogicalChannel;
   H245H2250LogicalChannelAckParameters *h2250lcap;
   H245UnicastAddress *unicastAddr;
   H245UnicastAddress_iPAddress *iPAddress;
   H245UnicastAddress *unicastAddr1;
   H245UnicastAddress_iPAddress *iPAddress1;

   if(!((olcAck->m.forwardMultiplexAckParametersPresent == 1) &&
        (olcAck->forwardMultiplexAckParameters.t == 
         T_H245OpenLogicalChannelAck_forwardMultiplexAckParameters_h2250LogicalChannelAckParameters)))
   {
      OOTRACEERR3("Error: Processing open logical channel ack - LogicalChannel"
                  "Ack parameters absent (%s, %s)\n", call->callType, 
                  call->callToken);
      return OO_OK;  /* should send CloseLogicalChannel request */
   }

   h2250lcap = 
      olcAck->forwardMultiplexAckParameters.u.h2250LogicalChannelAckParameters;
   /* Extract media channel address */
   if(h2250lcap->m.mediaChannelPresent != 1)
   { 
      OOTRACEERR3("Error: Processing OpenLogicalChannelAck - media channel "
                  "absent (%s, %s)\n", call->callType, call->callToken);
      return OO_FAILED;
   }
   if(h2250lcap->mediaChannel.t != T_H245TransportAddress_unicastAddress)
   {
      OOTRACEERR3("Error: Processing OpenLogicalChannelAck - media channel "
                  "address type is not unicast (%s, %s)\n", call->callType, 
                  call->callToken);
      return OO_FAILED;
   }
   
   unicastAddr = h2250lcap->mediaChannel.u.unicastAddress;
   if(unicastAddr->t != T_H245UnicastAddress_iPAddress)
   {
      OOTRACEERR3("Error: Processing OpenLogicalChannelAck - media channel "
                  "address type is not IP (%s, %s)\n", call->callType, 
                   call->callToken);
      return OO_FAILED;
   }
   iPAddress = unicastAddr->u.iPAddress;
   
   sprintf(remoteip,"%d.%d.%d.%d", iPAddress->network.data[0],
                                  iPAddress->network.data[1], 
                                  iPAddress->network.data[2], 
                                  iPAddress->network.data[3]);
   
   /* Extract media control channel address */
   if(h2250lcap->m.mediaControlChannelPresent != 1)
   { 
      OOTRACEERR3("Error: Processing OpenLogicalChannelAck - Missing media "
                "control channel (%s, %s)\n", call->callType, call->callToken);
      return OO_FAILED;
   }
   if(h2250lcap->mediaControlChannel.t != 
                                     T_H245TransportAddress_unicastAddress)
   {
      OOTRACEERR3("Error: Processing OpenLogicalChannelAck - media control "
                  "channel addres type is not unicast (%s, %s)\n", 
                   call->callType, call->callToken);
      return OO_FAILED;
   }
   
   unicastAddr1 = h2250lcap->mediaControlChannel.u.unicastAddress;
   if(unicastAddr1->t != T_H245UnicastAddress_iPAddress)
   {
      OOTRACEERR3("Error: Processing OpenLogicalChannelAck - media control "
                  "channel address type is not IP (%s, %s)\n", call->callType, 
                   call->callToken);
      return OO_FAILED;
   }
   iPAddress1 = unicastAddr1->u.iPAddress;

   /* Set remote destination address for rtp session */
   //   strcpy(call->remoteIP, remoteip);
   
   /* Start channel here */
   pLogicalChannel = ooFindLogicalChannelByLogicalChannelNo(call,olcAck->forwardLogicalChannelNumber);
   if(!pLogicalChannel)
   {
      OOTRACEERR4("ERROR:Logical channel %d not found in the channel list for "
                  "call (%s, %s)\n", olcAck->forwardLogicalChannelNumber, 
                  call->callType, call->callToken);
      return OO_FAILED;
   }

   /* Update session id if we were waiting for remote to assign one and remote 
      did assign one. */
   if(pLogicalChannel->sessionID == 0 && h2250lcap->m.sessionIDPresent)
      pLogicalChannel->sessionID = h2250lcap->sessionID;   

   /* Populate ports &ip  for channel */
   strcpy(pLogicalChannel->remoteIP, remoteip);   
   pLogicalChannel->remoteMediaPort = iPAddress->tsapIdentifier;
   pLogicalChannel->remoteMediaControlPort = iPAddress1->tsapIdentifier;

   OOTRACEAST(OOTRCLVLDBGA,"[H323/H245] RECV <-- OLCAck:[%d] %s:%d <=[ %s ] =>%s:%d "
              " (%s, %s)\n", olcAck->forwardLogicalChannelNumber,
              (pLogicalChannel->localIP)?pLogicalChannel->localIP:"Null", 
              pLogicalChannel->localRtpPort,
              ooGetCapTypeText(pLogicalChannel->chanCap->cap),
              (pLogicalChannel->remoteIP)?pLogicalChannel->remoteIP:"Null" ,
              pLogicalChannel->remoteMediaPort, 
              call->callType, call->callToken);

   if(pLogicalChannel->chanCap->startTransmitChannel)
   {
      pLogicalChannel->chanCap->startTransmitChannel(call, pLogicalChannel);
      OOTRACEINFO4("TransmitLogical Channel of type %s started (%s, %s)\n", 
                   ooGetCapTypeText(pLogicalChannel->chanCap->cap),
                   call->callType, call->callToken);
      // send if i received 
      // ooSendH245MIlogicalchannelActive(call,olcAck->forwardLogicalChannelNumber);
   }
   else{
      OOTRACEERR3("ERROR:No callback registered for starting transmit channel "
                  "(%s, %s)\n", call->callType, call->callToken);
      return OO_FAILED;
   }
   pLogicalChannel->state = OO_LOGICALCHAN_ESTABLISHED;
   return OO_OK;
}

int ooOnReceivedOpenLogicalChannelRejected(OOH323CallData *call, 
                                     H245OpenLogicalChannelReject *olcReject)
{
   switch(olcReject->cause.t)
   {
   case T_H245OpenLogicalChannelReject_cause_unspecified:
      OOTRACEINFO4("Open logical channel %d rejected - unspecified (%s, %s)\n",
                   olcReject->forwardLogicalChannelNumber, call->callType, 
                   call->callToken);
      break;
   case T_H245OpenLogicalChannelReject_cause_unsuitableReverseParameters:
      OOTRACEINFO4("Open logical channel %d rejected - "
                   "unsuitableReverseParameters (%s, %s)\n", 
                   olcReject->forwardLogicalChannelNumber, call->callType, 
                   call->callToken);
      break;
   case T_H245OpenLogicalChannelReject_cause_dataTypeNotSupported:
      OOTRACEINFO4("Open logical channel %d rejected - dataTypeNotSupported"
                   "(%s, %s)\n", olcReject->forwardLogicalChannelNumber, 
                   call->callType, call->callToken);
      break;
   case T_H245OpenLogicalChannelReject_cause_dataTypeNotAvailable:
      OOTRACEINFO4("Open logical channel %d rejected - dataTypeNotAvailable"
                   "(%s, %s)\n", olcReject->forwardLogicalChannelNumber, 
                   call->callType, call->callToken);
      break;
   case T_H245OpenLogicalChannelReject_cause_unknownDataType:
      OOTRACEINFO4("Open logical channel %d rejected - unknownDataType"
                   "(%s, %s)\n", olcReject->forwardLogicalChannelNumber, 
                   call->callType, call->callToken);
      break;
   case T_H245OpenLogicalChannelReject_cause_dataTypeALCombinationNotSupported:
      OOTRACEINFO4("Open logical channel %d rejected - "
                   "dataTypeALCombinationNotSupported(%s, %s)\n", 
                   olcReject->forwardLogicalChannelNumber, 
                   call->callType, call->callToken);
      break;
   case T_H245OpenLogicalChannelReject_cause_multicastChannelNotAllowed:
       OOTRACEINFO4("Open logical channel %d rejected - "
                    "multicastChannelNotAllowed (%s, %s)\n", 
                    olcReject->forwardLogicalChannelNumber, 
                    call->callType, call->callToken);
      break;
   case T_H245OpenLogicalChannelReject_cause_insufficientBandwidth:
      OOTRACEINFO4("Open logical channel %d rejected - insufficientBandwidth"
                   "(%s, %s)\n", olcReject->forwardLogicalChannelNumber, 
                   call->callType, call->callToken);
      break;
   case T_H245OpenLogicalChannelReject_cause_separateStackEstablishmentFailed:
      OOTRACEINFO4("Open logical channel %d rejected - "
                    "separateStackEstablishmentFailed (%s, %s)\n", 
                    olcReject->forwardLogicalChannelNumber, 
                    call->callType, call->callToken);
      break;
   case T_H245OpenLogicalChannelReject_cause_invalidSessionID:
      OOTRACEINFO4("Open logical channel %d rejected - "
                    "invalidSessionID (%s, %s)\n", 
                    olcReject->forwardLogicalChannelNumber, 
                    call->callType, call->callToken);
      break;
   case T_H245OpenLogicalChannelReject_cause_masterSlaveConflict:
      OOTRACEINFO4("Open logical channel %d rejected - "
                    "invalidSessionID (%s, %s)\n", 
                    olcReject->forwardLogicalChannelNumber, 
                    call->callType, call->callToken);
      break;
   case T_H245OpenLogicalChannelReject_cause_waitForCommunicationMode:
      OOTRACEINFO4("Open logical channel %d rejected - "
                    "waitForCommunicationMode (%s, %s)\n", 
                    olcReject->forwardLogicalChannelNumber, 
                    call->callType, call->callToken);
      break;
   case T_H245OpenLogicalChannelReject_cause_invalidDependentChannel:
      OOTRACEINFO4("Open logical channel %d rejected - "
                    "invalidDependentChannel (%s, %s)\n", 
                    olcReject->forwardLogicalChannelNumber, 
                    call->callType, call->callToken);
      break;
   case T_H245OpenLogicalChannelReject_cause_replacementForRejected:
      OOTRACEINFO4("Open logical channel %d rejected - "
                    "replacementForRejected (%s, %s)\n", 
                    olcReject->forwardLogicalChannelNumber, 
                    call->callType, call->callToken);
      break;
   default:
      OOTRACEERR4("Error: OpenLogicalChannel %d rejected - "
                  "invalid cause(%s, %s)\n",
                   olcReject->forwardLogicalChannelNumber, 
                    call->callType, call->callToken);
   }
   if(call->callState < OO_CALL_CLEAR)
   {
      call->callState = OO_CALL_CLEAR;
      call->callEndReason = OO_REASON_LOCAL_CLEARED;
   }
   return OO_OK;
}

/**
 * Currently only disconnect end session command is supported.
 **/
int ooSendEndSessionCommand(OOH323CallData *call)
{
   int ret;
   H245CommandMessage * command;
   OOCTXT *pctxt;
   H245Message *ph245msg=NULL;
   ret = ooCreateH245Message(&ph245msg, 
                      T_H245MultimediaSystemControlMessage_command);
   if(ret != OO_OK)
   {
      OOTRACEERR3("Error: H245 message creation failed for - End Session "
                  "Command (%s, %s)\n", call->callType, call->callToken);
      return OO_FAILED;
   }
   ph245msg->msgType = OOEndSessionCommand;

   command = ph245msg->h245Msg.u.command;
   pctxt = &gH323ep.msgctxt;
   memset(command, 0, sizeof(H245CommandMessage));
   command->t = T_H245CommandMessage_endSessionCommand;
   command->u.endSessionCommand = (H245EndSessionCommand*) ASN1MALLOC(pctxt,
                                  sizeof(H245EndSessionCommand));
   memset(command->u.endSessionCommand, 0, sizeof(H245EndSessionCommand));
   command->u.endSessionCommand->t = T_H245EndSessionCommand_disconnect;
   OOTRACEDBGA3("Built EndSession Command (%s, %s)\n", call->callType,
                call->callToken);
   ret = ooSendH245Msg(call, ph245msg);
   if(ret != OO_OK)
   {
      OOTRACEERR3("Error:Failed to enqueue EndSession message to outbound "
                  "queue.(%s, %s)\n", call->callType, call->callToken);
   }
   ooFreeH245Message(call, ph245msg);
   return ret;
}


int ooHandleH245Command(OOH323CallData *call, 
                        H245CommandMessage *command)
{
   ASN1UINT i;
   DListNode *pNode = NULL;
   OOTimer *pTimer = NULL;
   OOTRACEDBGC3("Handling H.245 command message. (%s, %s)\n", call->callType,
                 call->callToken);
   switch(command->t)
   {
      case T_H245CommandMessage_endSessionCommand:
         OOTRACEINFO3("Received EndSession command (%s, %s)\n", 
                       call->callType, call->callToken);
         if(call->h245SessionState == OO_H245SESSION_ENDSENT)
         {
            /* Disable Session timer */
            for(i = 0; i<call->timerList.count; i++)
            {
               pNode = dListFindByIndex(&call->timerList, i);
               pTimer = (OOTimer*)pNode->data;
               if(((ooTimerCallback*)pTimer->cbData)->timerType & 
                                                            OO_SESSION_TIMER)
               {
                  ASN1MEMFREEPTR(call->pctxt, pTimer->cbData);
                  ooTimerDelete(call->pctxt, &call->timerList, pTimer);
                  OOTRACEDBGC3("Deleted Session Timer. (%s, %s)\n", 
                                call->callType, call->callToken);
                  break;
               }
            }
            ooCloseH245Connection(call);
         }
         else{

            call->h245SessionState = OO_H245SESSION_ENDRECVD;
#if 0
            if(call->callState < OO_CALL_CLEAR)
               call->callState = OO_CALL_CLEAR;
#else 
            if(call->logicalChans)
            {
               OOTRACEINFO3("In response to received EndSessionCommand - "
                            "Clearing all logical channels. (%s, %s)\n", 
                            call->callType, call->callToken);
               ooClearAllLogicalChannels(call);
            }
            ooSendEndSessionCommand(call);
#endif
         }
            
            
         break;
      case T_H245CommandMessage_sendTerminalCapabilitySet:
         OOTRACEWARN3("Warning: Received command Send terminal capability set "
                      "- Not handled (%s, %s)\n", call->callType, 
                      call->callToken);
         break;
      case T_H245CommandMessage_flowControlCommand:
         OOTRACEWARN3("Warning: Flow control command received "
                      "(%s, %s)\n", call->callType, call->callToken);
         ooSendFlowControlAck( call);
         break;

     case T_H245CommandMessage_miscellaneousCommand:
       if ( command->u.miscellaneousCommand->type.t == 
            T_H245MiscellaneousCommand_type_videoFastUpdatePicture)
       {
         OOTRACEWARN4("Warning: Video Fast Update received [%x] "
                      "(%s, %s)\n", gH323ep.h323Callbacks.onReceivedMiscellaneous,
                      call->callType, call->callToken );
         if(gH323ep.h323Callbacks.onReceivedMiscellaneous)
           gH323ep.h323Callbacks.onReceivedMiscellaneous(call,OO_MISC_CMD_VIDUPDATE,NULL);
       }
       else
       {
         OOTRACEWARN4("Warning: Unhandled H245 command message received %d "
                      "(%s, %s)\n", command->t,call->callType, call->callToken);         
       }    
       break;     
      default:
         OOTRACEWARN4("Warning: Unhandled H245 command message received %d "
                      "(%s, %s)\n", command->t,call->callType, call->callToken);
   }
   OOTRACEDBGC3("Handling H.245 command message done. (%s, %s)\n", 
                 call->callType, call->callToken);   
   return OO_OK;
}


int ooOnReceivedTerminalCapabilitySetAck(OOH323CallData* call)
{
  call->localTermCapState = OO_LocalTermCapSetAckRecvd;

  if(call->remoteTermCapState != OO_RemoteTermCapSetAckSent)
    return OO_OK;

  if( call->localTermCapState == OO_LocalTermCapSetAckRecvd &&
      call->remoteTermCapState == OO_RemoteTermCapSetAckSent && 
      call->masterSlaveState ==  OO_MasterSlave_Master) 
  {
    /*Since Cap exchange and MasterSlave Procedures are done */
    OOTRACEAST(OOTRCLVLDBGA,"ooOnReceivedTerminalCapabilitySetAck : Open Logical Channels[%s] (%s, %s) \n", 
               (call->masterSlaveState == OO_MasterSlave_Master)?"master":"slave",
               call->callType, call->callToken );
    if(!call->logicalChans){
      if(!gH323ep.h323Callbacks.openLogicalChannels)
        ooOpenLogicalChannels(call);
      else
        gH323ep.h323Callbacks.openLogicalChannels(call);
    }
  }
  else
    OOTRACEAST(OOTRCLVLDBGA,"ooOnReceivedTerminalCapabilitySetAck Not opening logical channels now MSD[%s] localTermCapStateAck[%s] remoteTermCapState[%s] (%s, %s) \n",
                 (call->masterSlaveState == OO_MasterSlave_Master)?"master":"slave",
                 (call->localTermCapState == OO_LocalTermCapSetAckRecvd)?"true":"false" ,
                 (call->remoteTermCapState == OO_RemoteTermCapSetAckSent)?"true":"false", 
                 call->callType, call->callToken);
      
  return OO_OK;
}

int ooCloseAllLogicalChannels(OOH323CallData *call)
{
   ooLogicalChannel *temp;

   temp = call->logicalChans;
   while(temp)
   {
      if(temp->state == OO_LOGICALCHAN_ESTABLISHED)
      {
         /* Sending closelogicalchannel only for outgoing channels*/
         if(!strcmp(temp->dir, "transmit"))
         {
            ooSendCloseLogicalChannel(call, temp);
         }
         else{
            ooSendRequestCloseLogicalChannel(call, temp);
         }
      }
      temp = temp->next;
   }
   return OO_OK;
}

int ooSendCloseLogicalChannel(OOH323CallData *call, ooLogicalChannel *logicalChan)
{
   int ret = OO_OK, error=0;
   H245Message *ph245msg = NULL;
   OOCTXT *pctxt;
   H245RequestMessage *request;
   H245CloseLogicalChannel* clc;
   
   ret = ooCreateH245Message(&ph245msg, 
                             T_H245MultimediaSystemControlMessage_request);
   if(ret != OO_OK)
   {
      OOTRACEERR3("ERROR:Failed to create H245 message for closeLogicalChannel"
                  " message (%s, %s)\n", call->callType, call->callToken);
      return OO_FAILED;
   }
   ph245msg->msgType = OOCloseLogicalChannel;
   ph245msg->logicalChannelNo = logicalChan->channelNo;
   pctxt = &gH323ep.msgctxt;
   request = ph245msg->h245Msg.u.request;

   request->t = T_H245RequestMessage_closeLogicalChannel;
   request->u.closeLogicalChannel = (H245CloseLogicalChannel*)ASN1MALLOC(pctxt,
                                     sizeof(H245CloseLogicalChannel));
   if(!request->u.closeLogicalChannel)
   {
      OOTRACEERR3("ERROR:Memory allocation for CloseLogicalChannel failed "
                  "(%s, %s)\n", call->callType, call->callToken);
      ooFreeH245Message(call, ph245msg);
      return OO_FAILED;
   }
   clc = request->u.closeLogicalChannel;
   memset(clc, 0, sizeof(H245CloseLogicalChannel));

   clc->forwardLogicalChannelNumber = logicalChan->channelNo;
   clc->source.t = T_H245CloseLogicalChannel_source_lcse;
   clc->m.reasonPresent = 1;
   clc->reason.t = T_H245CloseLogicalChannel_reason_unknown;

   OOTRACEDBGA4("Built close logical channel for %d (%s, %s)\n", 
                 logicalChan->channelNo, call->callType, call->callToken);
   ret = ooSendH245Msg(call, ph245msg);
   if(ret != OO_OK)
   {
     OOTRACEERR3("Error:Failed to enqueue CloseLogicalChannel to outbound queue.(%s, %s)\n", call->callType,
                 call->callToken);
     error++;
   }
   ooFreeH245Message(call, ph245msg);
   
   /* Stop the media transmission */
   OOTRACEINFO4("Closing logical channel %d (%s, %s)\n", 
                clc->forwardLogicalChannelNumber, call->callType, 
                call->callToken);
   ret = ooClearLogicalChannel(call, clc->forwardLogicalChannelNumber);
   if(ret != OO_OK)
   {
      OOTRACEERR4("ERROR:Failed to close logical channel %d (%s, %s)\n",
         clc->forwardLogicalChannelNumber, call->callType, call->callToken);
      return OO_FAILED;
   }
   if(error) return OO_FAILED;
   return ret;
}

/*TODO: Need to pass reason as a parameter */
int ooSendRequestCloseLogicalChannel(OOH323CallData *call, 
                                     ooLogicalChannel *logicalChan)
{
   int ret = OO_OK;
   H245Message *ph245msg = NULL;
   OOCTXT *pctxt;
   H245RequestMessage *request;
   H245RequestChannelClose *rclc;

   ret = ooCreateH245Message(&ph245msg, 
                             T_H245MultimediaSystemControlMessage_request);
   if(ret != OO_OK)
   {
      OOTRACEERR3("ERROR:Failed to create H245 message for "
                  "requestCloseLogicalChannel message (%s, %s)\n", 
                   call->callType, call->callToken);
      return OO_FAILED;
   }
   ph245msg->msgType = OORequestChannelClose;
   ph245msg->logicalChannelNo = logicalChan->channelNo;
   pctxt = &gH323ep.msgctxt;
   request = ph245msg->h245Msg.u.request;

   request->t = T_H245RequestMessage_requestChannelClose;
   request->u.requestChannelClose = (H245RequestChannelClose*)ASN1MALLOC(pctxt,
                                     sizeof(H245RequestChannelClose));
   if(!request->u.requestChannelClose)
   {
      OOTRACEERR3("ERROR:Memory allocation for RequestCloseLogicalChannel "
                  " failed (%s, %s)\n", call->callType, call->callToken);
      ooFreeH245Message(call, ph245msg);
      return OO_FAILED;
   }

   rclc = request->u.requestChannelClose;
   memset(rclc, 0, sizeof(H245RequestChannelClose));
   rclc->forwardLogicalChannelNumber = logicalChan->channelNo;
   
   rclc->m.reasonPresent = 1;
   rclc->reason.t = T_H245RequestChannelClose_reason_unknown;

   OOTRACEDBGA4("Built RequestCloseChannel for %d (%s, %s)\n", 
                 logicalChan->channelNo, call->callType, call->callToken);
   ret = ooSendH245Msg(call, ph245msg);
   if(ret != OO_OK)
   {
     OOTRACEERR3("Error:Failed to enqueue the RequestCloseChannel to outbound"
                 " queue (%s, %s)\n", call->callType,
                 call->callToken);
   }
   ooFreeH245Message(call, ph245msg);

   return ret;
}

int ooSendRequestChannelCloseRelease(OOH323CallData *call, int channelNum)
{
   int ret = OO_OK;
   H245Message *ph245msg = NULL;
   OOCTXT *pctxt;
   H245IndicationMessage *indication;

   ret = ooCreateH245Message(&ph245msg, 
                             T_H245MultimediaSystemControlMessage_indication);
   if(ret != OO_OK)
   {
      OOTRACEERR3("ERROR:Failed to create H245 message for "
                  "RequestChannelCloseRelease message (%s, %s)\n", 
                   call->callType, call->callToken);
      return OO_FAILED;
   }
   ph245msg->msgType = OORequestChannelCloseRelease;
   ph245msg->logicalChannelNo = channelNum;
   pctxt = &gH323ep.msgctxt;
   indication = ph245msg->h245Msg.u.indication;
   indication->t = T_H245IndicationMessage_requestChannelCloseRelease;
   indication->u.requestChannelCloseRelease = (H245RequestChannelCloseRelease*)
                     ASN1MALLOC(pctxt, sizeof(H245RequestChannelCloseRelease));
   if(!indication->u.requestChannelCloseRelease)
   {
      OOTRACEERR3("Error:Failed to allocate memory for "
                  "RequestChannelCloseRelease message. (%s, %s)\n", 
                   call->callType, call->callToken);
      ooFreeH245Message(call, ph245msg);
   }

   indication->u.requestChannelCloseRelease->forwardLogicalChannelNumber = 
                                                                channelNum;

   OOTRACEDBGA4("Built RequestChannelCloseRelease for %d (%s, %s)\n", 
                channelNum, call->callType, call->callToken);
   ret = ooSendH245Msg(call, ph245msg);
   if(ret != OO_OK)
   {
     OOTRACEERR3("Error:Failed to enqueue the RequestChannelCloseRelease to "
                 "outbound queue (%s, %s)\n", call->callType, call->callToken);
   }
   ooFreeH245Message(call, ph245msg);

   return ret;
}


   
int ooOnReceivedRequestChannelClose(OOH323CallData *call, 
                                    H245RequestChannelClose *rclc)
{
   int ret=0, error=0;
   H245Message *ph245msg=NULL;
   H245ResponseMessage *response = NULL;
   OOCTXT *pctxt=NULL;
   H245RequestChannelCloseAck *rclcAck;
   ooLogicalChannel * lChannel=NULL;
   /* Send Ack: TODO: Need to send reject, if doesn't exist
   */
   lChannel = ooFindLogicalChannelByLogicalChannelNo(call, 
                                        rclc->forwardLogicalChannelNumber);
   if(!lChannel)
   {
      OOTRACEERR4("ERROR:Channel %d requested to be closed not found "
                  "(%s, %s)\n", rclc->forwardLogicalChannelNumber,
                  call->callType, call->callToken);
      return OO_FAILED;
   }
   else{
      if(strcmp(lChannel->dir, "transmit"))
      {
         OOTRACEERR4("ERROR:Channel %d requested to be closed, Not a forward "
                     "channel (%s, %s)\n", rclc->forwardLogicalChannelNumber,
                     call->callType, call->callToken);
         return OO_FAILED;
      }
   }
   ret = ooCreateH245Message(&ph245msg, 
                             T_H245MultimediaSystemControlMessage_response);
   if(ret != OO_OK)
   {
      OOTRACEERR3("ERROR:Memory allocation for RequestChannelCloseAck message "
                  "failed (%s, %s)\n", call->callType, call->callToken);
      return OO_FAILED;
   }
   pctxt = &gH323ep.msgctxt;
   ph245msg->msgType = OORequestChannelCloseAck;
   ph245msg->logicalChannelNo = rclc->forwardLogicalChannelNumber;
   response = ph245msg->h245Msg.u.response;
   response->t = T_H245ResponseMessage_requestChannelCloseAck;
   response->u.requestChannelCloseAck = (H245RequestChannelCloseAck*)ASN1MALLOC
                                   (pctxt, sizeof(H245RequestChannelCloseAck));
   if(!response->u.requestChannelCloseAck)
   {
      OOTRACEERR3("ERROR:Failed to allocate memory for RequestChannelCloseAck "
                  "message (%s, %s)\n", call->callType, call->callToken);
      return OO_FAILED;
   }
   rclcAck = response->u.requestChannelCloseAck;
   memset(rclcAck, 0, sizeof(H245RequestChannelCloseAck));
   rclcAck->forwardLogicalChannelNumber = rclc->forwardLogicalChannelNumber;

   OOTRACEDBGA3("Built RequestCloseChannelAck message (%s, %s)\n", 
                 call->callType, call->callToken);
   ret = ooSendH245Msg(call, ph245msg);
   if(ret != OO_OK)
   {
      OOTRACEERR3("Error:Failed to enqueue RequestCloseChannelAck to outbound queue. (%s, %s)\n", call->callType,
                  call->callToken);
      error++;
   }

   ooFreeH245Message(call, ph245msg);
   
   /* Send Close Logical Channel*/
   ret = ooSendCloseLogicalChannel(call, lChannel);
   if(ret != OO_OK)
   {
      OOTRACEERR3("ERROR:Failed to build CloseLgicalChannel message(%s, %s)\n",
                   call->callType, call->callToken);
      return OO_FAILED;
   }

   if(error) return OO_FAILED;

   return ret;
}

/*
  We clear channel here. Ideally the remote endpoint should send 
  CloseLogicalChannel and then the channel should be cleared. But there's no
  timer for this and if remote endpoint misbehaves, the call will keep waiting
  for CloseLogicalChannel and hence, wouldn't be cleared. In case when remote
  endpoint sends CloseLogicalChannel, we call ooClearLogicalChannel again,
  which simply returns OO_OK as channel was already cleared. Other option is
  to start a timer for call cleanup and if call is not cleaned up within 
  timeout, we clean call forcefully. Note, no such timer is defined in 
  standards.
*/
int ooOnReceivedRequestChannelCloseAck
                       (OOH323CallData *call, H245RequestChannelCloseAck *rccAck)
{
   int ret=OO_OK;
   /* Remote endpoint is ok to close channel. So let's do it */
   ret = ooClearLogicalChannel(call, rccAck->forwardLogicalChannelNumber);
   if(ret != OO_OK)
   {
      OOTRACEERR4("Error:Failed to clear logical channel %d. (%s, %s)\n", 
                   rccAck->forwardLogicalChannelNumber, call->callType, 
                   call->callToken);
   }

   return ret;
}

int ooOnReceivedRequestChannelCloseReject
   (OOH323CallData *call, H245RequestChannelCloseReject *rccReject)
{
   int ret =0;
   switch(rccReject->cause.t)
   {
   case T_H245RequestChannelCloseReject_cause_unspecified:
      OOTRACEDBGA4("Remote endpoint has rejected request to close logical "
                   "channel %d - cause unspecified. (%s, %s)\n", 
                   rccReject->forwardLogicalChannelNumber, call->callType, 
                   call->callToken);
     break;
   case T_H245RequestChannelCloseReject_cause_extElem1:
      OOTRACEDBGA4("Remote endpoint has rejected request to close logical "
                   "channel %d - cause propriatory. (%s, %s)\n", 
                   rccReject->forwardLogicalChannelNumber, call->callType, 
                   call->callToken);   
      break;
   default:
      OOTRACEDBGA4("Remote endpoint has rejected request to close logical "
                   "channel %d - cause INVALID. (%s, %s)\n", 
                   rccReject->forwardLogicalChannelNumber, call->callType, 
                   call->callToken);
   }
   OOTRACEDBGA4("Clearing logical channel %d. (%s, %s)\n", 
                 rccReject->forwardLogicalChannelNumber, call->callType, 
                 call->callToken);
   ret = ooClearLogicalChannel(call, rccReject->forwardLogicalChannelNumber);
   if(ret != OO_OK)
   {
      OOTRACEERR4("Error: failed to clear logical channel %d.(%s, %s)\n", 
                   rccReject->forwardLogicalChannelNumber, call->callType, 
                   call->callToken);
   }
   return ret;
}

/****/
int ooOnReceivedCloseLogicalChannel(OOH323CallData *call, 
                                    H245CloseLogicalChannel* clc)
{
   int ret=0;
   H245Message *ph245msg = NULL;
   OOCTXT *pctxt = NULL;
   H245CloseLogicalChannelAck * clcAck;
   H245ResponseMessage *response;
   
   OOTRACEINFO4("Closing logical channel number %d (%s, %s)\n",
      clc->forwardLogicalChannelNumber, call->callType, call->callToken);
   
   ret = ooClearLogicalChannel(call, clc->forwardLogicalChannelNumber);
   if(ret != OO_OK)
   {
      OOTRACEERR4("ERROR:Failed to close logical channel %d (%s, %s)\n",
         clc->forwardLogicalChannelNumber, call->callType, call->callToken);
      return OO_FAILED;
   }

   ret = ooCreateH245Message(&ph245msg, 
                              T_H245MultimediaSystemControlMessage_response);
   if(ret != OO_OK)
   {
      OOTRACEERR3("ERROR:Failed to create H245 message for "
                  "closeLogicalChannelAck (%s, %s)\n", call->callType, 
                  call->callToken);
      return OO_FAILED;
   }
   pctxt = &gH323ep.msgctxt;
   ph245msg->msgType = OOCloseLogicalChannelAck;
   ph245msg->logicalChannelNo = clc->forwardLogicalChannelNumber;
   response = ph245msg->h245Msg.u.response;
   response->t = T_H245ResponseMessage_closeLogicalChannelAck;
   response->u.closeLogicalChannelAck = (H245CloseLogicalChannelAck*)
                         ASN1MALLOC(pctxt, sizeof(H245CloseLogicalChannelAck));
   clcAck = response->u.closeLogicalChannelAck;
   if(!clcAck)
   {
      OOTRACEERR3("ERROR:Failed to allocate memory for closeLogicalChannelAck "
                  "(%s, %s)\n", call->callType, call->callToken);
      return OO_OK;
   }
   memset(clcAck, 0, sizeof(H245CloseLogicalChannelAck));
   clcAck->forwardLogicalChannelNumber = clc->forwardLogicalChannelNumber;

   OOTRACEDBGA3("Built CloseLogicalChannelAck message (%s, %s)\n",
                 call->callType, call->callToken);
   ret = ooSendH245Msg(call, ph245msg);
   if(ret != OO_OK)
   {
     OOTRACEERR3("Error:Failed to enqueue CloseLogicalChannelAck message to "
                 "outbound queue.(%s, %s)\n", call->callType, call->callToken);
   }

   ooFreeH245Message(call, ph245msg);
   return ret;
}

int ooOnReceivedCloseChannelAck(OOH323CallData* call, 
                                H245CloseLogicalChannelAck* clcAck)
{
   int ret = OO_OK;
   return ret;
}

int ooHandleH245Message(OOH323CallData *call, H245Message * pmsg)
{
   ASN1UINT i;
   DListNode *pNode = NULL;
   OOTimer *pTimer = NULL;
   H245Message *pH245 = (H245Message*)pmsg;
   /* There are four major types of H.245 messages that can be received.
      Request/Response/Command/Indication. Each one of them need to be 
      handled separately.
   */   
   H245RequestMessage *request = NULL;
   H245ResponseMessage *response = NULL;
   H245CommandMessage *command = NULL;
   H245IndicationMessage *indication = NULL;
   
   OOTRACEDBGC3("Handling H245 message. (%s, %s)\n", call->callType, 
                 call->callToken);
   
   ooTraceH245Msg(call , NULL , pmsg );  

   switch(pH245->h245Msg.t)
   {
      /* H.245 Request message is received */
      case (T_H245MultimediaSystemControlMessage_request):
         request = pH245->h245Msg.u.request;
         switch(request->t)
         {
           case T_H245RequestMessage_terminalCapabilitySet:
               /* If session isn't marked active yet, do it. possible in case of 
                  tunneling */
               if(call->h245SessionState == OO_H245SESSION_IDLE)
                  call->h245SessionState = OO_H245SESSION_ACTIVE; 

               ooOnReceivedTerminalCapabilitySet(call, pH245);
               if(call->localTermCapState == OO_LocalTermCapExchange_Idle)
                  ooSendTermCapMsg(call);
               break;
            case T_H245RequestMessage_masterSlaveDetermination:
               ooHandleMasterSlave(call, 
                                     request->u.masterSlaveDetermination, 
                                     OOMasterSlaveDetermination);
               break;
            case T_H245RequestMessage_openLogicalChannel:
               ooHandleOpenLogicalChannel(call, 
                                          request->u.openLogicalChannel);
               break;
            case T_H245RequestMessage_closeLogicalChannel:
               OOTRACEINFO4("Received close logical Channel - %d (%s, %s)\n",
                  request->u.closeLogicalChannel->forwardLogicalChannelNumber, 
                  call->callType, call->callToken);
               ooOnReceivedCloseLogicalChannel(call, 
                                               request->u.closeLogicalChannel);
               break;
            case T_H245RequestMessage_requestChannelClose:
               OOTRACEINFO4("Received RequestChannelClose - %d (%s, %s)\n",
                  request->u.requestChannelClose->forwardLogicalChannelNumber, 
                  call->callType, call->callToken);
               ooOnReceivedRequestChannelClose(call, 
                                               request->u.requestChannelClose);
               break;

           case T_H245RequestMessage_roundTripDelayRequest:
               OOTRACEINFO4("Received Request roundTripDelayRequest - %d (%s, %s)\n",
                  request->u.roundTripDelayRequest->sequenceNumber, 
                  call->callType, call->callToken);
               ooOnReceivedRequestRoundTripDelayRequest(call, request->u.roundTripDelayRequest->sequenceNumber);
           case T_H245RequestMessage_conferenceRequest:
               OOTRACEINFO4("Received Request T_H245RequestMessage_conferenceRequest %d (%s, %s)\n",
                  request->u.conferenceRequest->t, 
                  call->callType, call->callToken);
               ooOnReceivedConferenceRequest(call, request->u.roundTripDelayRequest->sequenceNumber);
               break ;
            default:
              OOTRACEERR4("[H323/H245] Error:Ignore request %d .(%s, %s)\n",
                          request->t ,call->callType, call->callToken);
               ;
         } /* End of Request Message */
         break;
      /* H.245 Response message is received */ 
      case (T_H245MultimediaSystemControlMessage_response):
         response = pH245->h245Msg.u.response;
         switch(response->t)
         {
            case T_H245ResponseMessage_masterSlaveDeterminationAck:
               /* Disable MSD timer */
               for(i = 0; i<call->timerList.count; i++)
               {
                  pNode = dListFindByIndex(&call->timerList, i);
                  pTimer = (OOTimer*)pNode->data;
                  if(((ooTimerCallback*)pTimer->cbData)->timerType & OO_MSD_TIMER)
                  {
                     ASN1MEMFREEPTR(call->pctxt, pTimer->cbData);
                     ooTimerDelete(call->pctxt, &call->timerList, pTimer);
                     OOTRACEDBGC3("Deleted MSD Timer. (%s, %s)\n", call->callType,
                   call->callToken);
                     break;
                  }
               }

               ooHandleMasterSlave(call, 
                                   response->u.masterSlaveDeterminationAck, 
                                   OOMasterSlaveAck);
               break;
            case T_H245ResponseMessage_masterSlaveDeterminationReject:
               /* Disable MSD timer */
               for(i = 0; i<call->timerList.count; i++)
               {
                  pNode = dListFindByIndex(&call->timerList, i);
                  pTimer = (OOTimer*)pNode->data;
                  if(((ooTimerCallback*)pTimer->cbData)->timerType & OO_MSD_TIMER)
                  {
                     ASN1MEMFREEPTR(call->pctxt, pTimer->cbData);
                     ooTimerDelete(call->pctxt, &call->timerList, pTimer);
                     OOTRACEDBGC3("Deleted MSD Timer. (%s, %s)\n", call->callType,
                   call->callToken);
                     break;
                  }
               }
               ooHandleMasterSlaveReject(call, 
                                  response->u.masterSlaveDeterminationReject);
               break;
            case T_H245ResponseMessage_terminalCapabilitySetAck:
               /* Disable TCS timer */
               for(i = 0; i<call->timerList.count; i++)
               {
                  pNode = dListFindByIndex(&call->timerList, i);
                  pTimer = (OOTimer*)pNode->data;
                  if(((ooTimerCallback*)pTimer->cbData)->timerType & OO_TCS_TIMER)
                  {
                     ASN1MEMFREEPTR(call->pctxt, pTimer->cbData);
                     ooTimerDelete(call->pctxt, &call->timerList, pTimer);
                     OOTRACEDBGC3("Deleted TCS Timer. (%s, %s)\n", call->callType,
                        call->callToken);
                     break;
                  }
               }
               ooOnReceivedTerminalCapabilitySetAck(call);
               break;
            case T_H245ResponseMessage_terminalCapabilitySetReject:
               OOTRACEINFO3("TerminalCapabilitySetReject message received."
                            " (%s, %s)\n", call->callType, call->callToken);
               if(response->u.terminalCapabilitySetReject->sequenceNumber != 
                  call->localTermCapSeqNo)
               {
                  OOTRACEINFO5("Ignoring TCSReject with mismatched seqno %d "
                              "(local - %d). (%s, %s)\n", 
                          response->u.terminalCapabilitySetReject->sequenceNumber,
                        call->localTermCapSeqNo, call->callType, call->callToken);
                  break;
               }
               /* Disable TCS timer */
               for(i = 0; i<call->timerList.count; i++)
               {
                  pNode = dListFindByIndex(&call->timerList, i);
                  pTimer = (OOTimer*)pNode->data;
                  if(((ooTimerCallback*)pTimer->cbData)->timerType & OO_TCS_TIMER)
                  {
                     ASN1MEMFREEPTR(call->pctxt, pTimer->cbData);
                     ooTimerDelete(call->pctxt, &call->timerList, pTimer);
                     OOTRACEDBGC3("Deleted TCS Timer. (%s, %s)\n", call->callType,
                        call->callToken);
                     break;
                  }
               }
               if(call->callState < OO_CALL_CLEAR)
               {
                  call->callState = OO_CALL_CLEAR;
                  call->callEndReason = OO_REASON_NOCOMMON_CAPABILITIES;
               }
               break;
            case T_H245ResponseMessage_openLogicalChannelAck:
               for(i = 0; i<call->timerList.count; i++)
               {
                  pNode = dListFindByIndex(&call->timerList, i);
                  pTimer = (OOTimer*)pNode->data;
                  if((((ooTimerCallback*)pTimer->cbData)->timerType & OO_OLC_TIMER)                                            && 
                      ((ooTimerCallback*)pTimer->cbData)->channelNumber == 
                   response->u.openLogicalChannelAck->forwardLogicalChannelNumber)
                  {

                     memFreePtr(call->pctxt, pTimer->cbData);
                     ooTimerDelete(call->pctxt, &call->timerList, pTimer);
                     OOTRACEDBGC3("Deleted OpenLogicalChannel Timer. (%s, %s)\n", 
                                   call->callType, call->callToken);
                     break;
                  }
               }
               ooOnReceivedOpenLogicalChannelAck(call, 
                                              response->u.openLogicalChannelAck);
               break;
            case T_H245ResponseMessage_openLogicalChannelReject:
               OOTRACEINFO3("Open Logical Channel Reject received (%s, %s)\n",
                             call->callType, call->callToken);
               for(i = 0; i<call->timerList.count; i++)
               {
                  pNode = dListFindByIndex(&call->timerList, i);
                  pTimer = (OOTimer*)pNode->data;
                  if((((ooTimerCallback*)pTimer->cbData)->timerType & OO_OLC_TIMER)                                            && 
                      ((ooTimerCallback*)pTimer->cbData)->channelNumber == 
                   response->u.openLogicalChannelAck->forwardLogicalChannelNumber)
                  {

                     ASN1MEMFREEPTR(call->pctxt, pTimer->cbData);
                     ooTimerDelete(call->pctxt, &call->timerList, pTimer);
                     OOTRACEDBGC3("Deleted OpenLogicalChannel Timer. (%s, %s)\n", 
                                   call->callType, call->callToken);
                     break;
                  }
               }
               ooOnReceivedOpenLogicalChannelRejected(call, 
                                        response->u.openLogicalChannelReject);
               break;
            case T_H245ResponseMessage_closeLogicalChannelAck:
               OOTRACEINFO4("CloseLogicalChannelAck received for %d (%s, %s)\n",
                  response->u.closeLogicalChannelAck->forwardLogicalChannelNumber,
                  call->callType, call->callToken);
               for(i = 0; i<call->timerList.count; i++)
               {
                  pNode = dListFindByIndex(&call->timerList, i);
                  pTimer = (OOTimer*)pNode->data;
                  if((((ooTimerCallback*)pTimer->cbData)->timerType & OO_CLC_TIMER)                                            && 
                      ((ooTimerCallback*)pTimer->cbData)->channelNumber == 
                  response->u.closeLogicalChannelAck->forwardLogicalChannelNumber)
                  {

                     ASN1MEMFREEPTR(call->pctxt, pTimer->cbData);
                     ooTimerDelete(call->pctxt, &call->timerList, pTimer);
                     OOTRACEDBGC3("Deleted CloseLogicalChannel Timer. (%s, %s)\n",
                                   call->callType, call->callToken);
                     break;
                  }
               }
               ooOnReceivedCloseChannelAck(call, 
                                           response->u.closeLogicalChannelAck);
               break;
            case T_H245ResponseMessage_requestChannelCloseAck:
                OOTRACEINFO4("RequestChannelCloseAck received - %d (%s, %s)\n",
                  response->u.requestChannelCloseAck->forwardLogicalChannelNumber,
                  call->callType, call->callToken);
                for(i = 0; i<call->timerList.count; i++)
                {
                  pNode = dListFindByIndex(&call->timerList, i);
                  pTimer = (OOTimer*)pNode->data;
                  if((((ooTimerCallback*)pTimer->cbData)->timerType & OO_RCC_TIMER)                                            && 
                      ((ooTimerCallback*)pTimer->cbData)->channelNumber == 
                  response->u.requestChannelCloseAck->forwardLogicalChannelNumber)
                  {

                     ASN1MEMFREEPTR(call->pctxt, pTimer->cbData);
                     ooTimerDelete(call->pctxt, &call->timerList, pTimer);
                     OOTRACEDBGC3("Deleted RequestChannelClose Timer. (%s, %s)\n",
                                   call->callType, call->callToken);
                     break;
                  }
                }
                ooOnReceivedRequestChannelCloseAck(call, 
                                             response->u.requestChannelCloseAck);
                break;
            case T_H245ResponseMessage_requestChannelCloseReject:
               OOTRACEINFO4("RequestChannelCloseReject received - %d (%s, %s)\n",
               response->u.requestChannelCloseReject->forwardLogicalChannelNumber,
                 call->callType, call->callToken);
                for(i = 0; i<call->timerList.count; i++)
                {
                  pNode = dListFindByIndex(&call->timerList, i);
                  pTimer = (OOTimer*)pNode->data;
                  if((((ooTimerCallback*)pTimer->cbData)->timerType & OO_RCC_TIMER) && 
                      ((ooTimerCallback*)pTimer->cbData)->channelNumber == 
                      response->u.requestChannelCloseReject->forwardLogicalChannelNumber)
                  {

                     ASN1MEMFREEPTR(call->pctxt, pTimer->cbData);
                     ooTimerDelete(call->pctxt, &call->timerList, pTimer);
                     OOTRACEDBGC3("Deleted RequestChannelClose Timer. (%s, %s)\n",
                                   call->callType, call->callToken);
                     break;
                  }
               }
               ooOnReceivedRequestChannelCloseReject(call, 
                                           response->u.requestChannelCloseReject);
               break;
            default:
              OOTRACEERR4("[H323/H245] Error:Ignore respons %d .(%s, %s)\n",
                          response->t ,call->callType, call->callToken);
               ;
         }
         break;
      /* H.245 command message is received */
      case (T_H245MultimediaSystemControlMessage_command):
         command = pH245->h245Msg.u.command;
         ooHandleH245Command(call, command);
         break;
      /* H.245 Indication message received */
      case (T_H245MultimediaSystemControlMessage_indication):
         indication = pH245->h245Msg.u.indication;
         switch(indication->t)
         {
            case T_H245IndicationMessage_userInput:
               ooOnReceivedUserInputIndication(call, indication->u.userInput);
               break;
           case T_H245IndicationMessage_miscellaneousIndication:
             ooSendH245MIlogicalchannelActive(call, indication->u.miscellaneousIndication->logicalChannelNumber);
             break ;
           default:
             OOTRACEWARN3("Unhandled indication message received.(%s, %s)\n",call->callType, call->callToken);
         }
         break;
      default:
        ;
   }
   OOTRACEDBGC3("Finished handling H245 message. (%s, %s)\n", 
                 call->callType, call->callToken);
   return OO_OK;
}


int ooOnReceivedUserInputIndication
   (OOH323CallData *call, H245UserInputIndication *indication)
{
   if((indication->t == T_H245UserInputIndication_alphanumeric) && 
      (call->dtmfmode & OO_CAP_DTMF_H245_alphanumeric))
   {
      if(gH323ep.h323Callbacks.onReceivedDTMF)
         gH323ep.h323Callbacks.onReceivedDTMF(call,indication->u.alphanumeric);
   }
   else if((indication->t == T_H245UserInputIndication_signal) && 
           (call->dtmfmode & OO_CAP_DTMF_H245_signal)) {
      if(gH323ep.h323Callbacks.onReceivedDTMF)
         gH323ep.h323Callbacks.onReceivedDTMF(call, 
                                             indication->u.signal->signalType);
   }
   else {
      OOTRACEINFO3("Unsupported userInput message type received - ignoring."
                   "(%s, %s)\n", call->callType, call->callToken);
   }
   return OO_OK;
}

int ooOnReceivedTerminalCapabilitySet(OOH323CallData *call, H245Message *pmsg)
{
   int ret = 0,k;
   H245TerminalCapabilitySet *tcs=NULL;
   DListNode *pNode=NULL;
   H245CapabilityTableEntry *capEntry = NULL;
   
   OOTRACEAST(OOTRCLVLDBGA,"[H323/CODEC] ooOnReceivedTerminalCapabilitySet"
              " (%s, %s)\n", call->callType, call->callToken);

   tcs =  pmsg->h245Msg.u.request->u.terminalCapabilitySet;
   if(call->remoteTermCapSeqNo >= tcs->sequenceNumber)
   {
      OOTRACEINFO4("Rejecting TermCapSet message with SeqNo %d, as already "
                   "acknowledged message with this SeqNo (%s, %s)\n", 
                   call->remoteTermCapSeqNo, call->callType, call->callToken);
      ooSendTerminalCapabilitySetReject(call, tcs->sequenceNumber, 
                         T_H245TerminalCapabilitySetReject_cause_unspecified);
      return OO_OK;
   }
  
   if(!tcs->m.capabilityTablePresent)
   {
      // OOTRACEWARN3("Warn:Ignoring TCS as no capability table present(%s, %s)\n",
      OOTRACEWARN3("Empty TCS found.  Pausing call...(%s, %s)\n",
                    call->callType, call->callToken);
      call->h245SessionState = OO_H245SESSION_PAUSED;
      //ooSendTerminalCapabilitySetReject(call, tcs->sequenceNumber, 
      //                   T_H245TerminalCapabilitySetReject_cause_unspecified);
      //return OO_OK;
   }
   call->remoteTermCapSeqNo = tcs->sequenceNumber;

   if(tcs->m.capabilityTablePresent) {
      for(k=0; k<(int)tcs->capabilityTable.count; k++)
      {
         pNode = dListFindByIndex(&tcs->capabilityTable, k);
         if(pNode)
         {
            OOTRACEDBGC4("Processing CapabilityTable Entry %d (%s, %s)\n", 
                          k, call->callType, call->callToken);
            capEntry = (H245CapabilityTableEntry*) pNode->data;
            if(capEntry->m.capabilityPresent){
               ret =  ooAddRemoteCapability(call, &capEntry->capability);
               if(ret != OO_OK)
               {
                  OOTRACEERR4("Error:Failed to process remote capability in "
                              "capability table at index %d. (%s, %s)\n", 
                               k, call->callType, call->callToken);
               }
               ooCapabilityUpdateJointCapabilities(call, &capEntry->capability);
            }
         }
         pNode = NULL;
         capEntry=NULL;
      }
   }

   
   /* Update remoteTermCapSetState */
   call->remoteTermCapState = OO_RemoteTermCapSetRecvd;

   ooH245AcknowledgeTerminalCapabilitySet(call);   

   /* If we haven't yet send TCS then send it now */
   if(call->localTermCapState == OO_LocalTermCapExchange_Idle)
   {
      ret = ooSendTermCapMsg(call);
      if(ret != OO_OK)
      {
         OOTRACEERR3("ERROR:Sending Terminal capability message (%s, %s)\n",
                      call->callType, call->callToken);
         return ret;
      }
   }

   if(call->remoteTermCapState != OO_RemoteTermCapSetAckSent ||
      call->localTermCapState  != OO_LocalTermCapSetAckRecvd)
      return OO_OK;

   if(call->masterSlaveState == OO_MasterSlave_Idle)
   {
      OOTRACEINFO3("ooOnReceivedTerminalCapabilitySet:MasterSlave not determinated (%s, %s)\n",
                   call->callType, call->callToken);
      ooSendMasterSlaveDetermination(call,TRUE);
      return OO_OK;
   }

  if( call->localTermCapState == OO_LocalTermCapSetAckRecvd &&
      call->remoteTermCapState == OO_RemoteTermCapSetAckSent && 
      call->masterSlaveState ==  OO_MasterSlave_Master) 
  {
    /*Since Cap exchange and MasterSlave Procedures are done */
    OOTRACEAST(OOTRCLVLDBGA,"ooOnReceivedTerminalCapabilitySet : Open Logical Channels[%s] (%s, %s) \n", 
               (call->masterSlaveState == OO_MasterSlave_Master)?"master":"slave",
               call->callType, call->callToken );
    if(!call->logicalChans){
      if(!gH323ep.h323Callbacks.openLogicalChannels)
        ooOpenLogicalChannels(call);
      else
        gH323ep.h323Callbacks.openLogicalChannels(call);
    }
  }
  else
    OOTRACEAST(OOTRCLVLDBGA,"ooOnReceivedTerminalCapabilitySet Not opening logical channels now MSD[%s] localTermCapStateAck[%s] remoteTermCapState[%s] (%s, %s) \n",
                 (call->masterSlaveState == OO_MasterSlave_Master)?"master":"slave",
                 (call->localTermCapState == OO_LocalTermCapSetAckRecvd)?"true":"false" ,
                 (call->remoteTermCapState == OO_RemoteTermCapSetAckSent)?"true":"false", 
                 call->callType, call->callToken);

   return OO_OK;
}

int ooSendTerminalCapabilitySetReject
                        (OOH323CallData *call, int seqNo, ASN1UINT cause)
{
   H245Message *ph245msg=NULL;
   H245ResponseMessage * response=NULL;
   OOCTXT *pctxt=NULL;
   int ret = ooCreateH245Message(&ph245msg, 
                      T_H245MultimediaSystemControlMessage_response);
   if(ret != OO_OK)
   {
      OOTRACEERR1("ERROR:H245 message creation failed for - "
                           "TerminalCapabilitySetReject\n");
      return OO_FAILED;
   }
   ph245msg->msgType = OOTerminalCapabilitySetReject;
   response = ph245msg->h245Msg.u.response;
   memset(response, 0, sizeof(H245ResponseMessage));
   pctxt = &gH323ep.msgctxt;
   response->t = T_H245ResponseMessage_terminalCapabilitySetReject;
   
   response->u.terminalCapabilitySetReject = (H245TerminalCapabilitySetReject*)
                   ASN1MALLOC(pctxt, sizeof(H245TerminalCapabilitySetReject));

   memset(response->u.terminalCapabilitySetReject, 0, 
                                 sizeof(H245TerminalCapabilitySetReject));
   response->u.terminalCapabilitySetReject->sequenceNumber = seqNo;
   response->u.terminalCapabilitySetReject->cause.t = cause;

   OOTRACEDBGA3("Built TerminalCapabilitySetReject (%s, %s)\n", 
                 call->callType, call->callToken);
 
   ret = ooSendH245Msg(call, ph245msg);
   if(ret != OO_OK)
   {
     OOTRACEERR3("Error:Failed to enqueue TCSReject to outbound queue. "
                 "(%s, %s)\n", call->callType, call->callToken);
   }
   else
      call->remoteTermCapState = OO_RemoteTermCapExchange_Idle;

   ooFreeH245Message(call, ph245msg);
   return ret;
}

int ooH245AcknowledgeTerminalCapabilitySet(OOH323CallData *call)
{
   H245Message *ph245msg=NULL;
   H245ResponseMessage * response=NULL;
   OOCTXT *pctxt=NULL;
   int ret = ooCreateH245Message(&ph245msg, 
                      T_H245MultimediaSystemControlMessage_response);
   if(ret != OO_OK)
   {
      OOTRACEERR1("ERROR:H245 message creation failed for - "
                           "TerminalCapability Set Ack\n");
      return OO_FAILED;
   }
   ph245msg->msgType = OOTerminalCapabilitySetAck;
   response = ph245msg->h245Msg.u.response;
   memset(response, 0, sizeof(H245ResponseMessage));
   pctxt = &gH323ep.msgctxt;
   response->t = T_H245ResponseMessage_terminalCapabilitySetAck;
   
   response->u.terminalCapabilitySetAck = (H245TerminalCapabilitySetAck*)
                   ASN1MALLOC(pctxt, sizeof(H245TerminalCapabilitySetAck));

   memset(response->u.terminalCapabilitySetAck, 0, 
                                 sizeof(H245TerminalCapabilitySetAck));
   response->u.terminalCapabilitySetAck->sequenceNumber = call->remoteTermCapSeqNo;

   OOTRACEDBGA3("Built TerminalCapabilitySet Ack (%s, %s)\n", 
                 call->callType, call->callToken);
   ret = ooSendH245Msg(call, ph245msg);

   if(ret != OO_OK)
   {
     OOTRACEERR3("Error:Failed to enqueue TCSAck to outbound queue. (%s, %s)\n", call->callType, call->callToken);
   }
   else
      call->remoteTermCapState = OO_RemoteTermCapSetAckSent;

   ooFreeH245Message(call, ph245msg);
   return ret;
}


int ooSendTerminalCapabilitySetRelease(OOH323CallData * call)
{
   int ret=0;
   H245IndicationMessage* indication=NULL;
   H245Message *ph245msg=NULL;
   OOCTXT *pctxt=&gH323ep.msgctxt;

   ret = ooCreateH245Message
      (&ph245msg, T_H245MultimediaSystemControlMessage_indication);

   if (ret != OO_OK) {
      OOTRACEERR3("Error:H245 message creation failed for - Terminal"
                  "CapabilitySetRelease (%s, %s)\n",call->callType, 
                  call->callToken);
      return OO_FAILED;
   }
   ph245msg->msgType = OOTerminalCapabilitySetRelease;
   indication = ph245msg->h245Msg.u.indication;

   indication->t = T_H245IndicationMessage_terminalCapabilitySetRelease;

   indication->u.terminalCapabilitySetRelease = 
      (H245TerminalCapabilitySetRelease*)
      memAlloc (pctxt, sizeof(H245TerminalCapabilitySetRelease));

   if(!indication->u.terminalCapabilitySetRelease)
   {
      OOTRACEERR3("Error: Failed to allocate memory for TCSRelease message."
                  " (%s, %s)\n", call->callType, call->callToken);
      ooFreeH245Message(call, ph245msg);
      return OO_FAILED;
   }
   OOTRACEDBGA3 ("Built TerminalCapabilitySetRelease (%s, %s)\n", 
                 call->callType, call->callToken);

   ret = ooSendH245Msg (call, ph245msg);

   if (ret != OO_OK) {
      OOTRACEERR3 
         ("Error:Failed to enqueue TerminalCapabilitySetRelease "
         "message to outbound queue.(%s, %s)\n", call->callType, 
         call->callToken);
   }
   
   ooFreeH245Message (call, ph245msg);
   return ret;
}


int ooSendH245UserInputIndication_alphanumeric
   (OOH323CallData *call, const char *data)
{
   int ret=0;
   H245IndicationMessage* indication=NULL;
   H245Message *ph245msg=NULL;
   OOCTXT *pctxt=&gH323ep.msgctxt;

   ret = ooCreateH245Message
      (&ph245msg, T_H245MultimediaSystemControlMessage_indication);

   if (ret != OO_OK) {
      OOTRACEERR3("Error:H245 message creation failed for - H245UserInput"
                  "Indication_alphanumeric (%s, %s)\n",call->callType, 
                  call->callToken);
      return OO_FAILED;
   }
   ph245msg->msgType = OOUserInputIndication;
   indication = ph245msg->h245Msg.u.indication;

   indication->t = T_H245IndicationMessage_userInput;
   indication->u.userInput = 
      (H245UserInputIndication*)
      memAllocZ (pctxt, sizeof(H245UserInputIndication));

   if(!indication->u.userInput)
   {
      OOTRACEERR3("Error: Memory - ooH245UserInputIndication_alphanumeric - "
                  " userInput (%s, %s)\n", call->callType, call->callToken);
      ooFreeH245Message(call, ph245msg);
      return OO_FAILED;
   }
   indication->u.userInput->t = T_H245UserInputIndication_alphanumeric;
   indication->u.userInput->u.alphanumeric = (ASN1GeneralString)
                                              memAlloc(pctxt, strlen(data)+1);
   if(!indication->u.userInput->u.alphanumeric)
   {
      OOTRACEERR3("Error: Memory - ooH245UserInputIndication-alphanumeric - "
                  "alphanumeric (%s, %s).\n", call->callType, call->callToken);
      ooFreeH245Message(call, ph245msg);
      return OO_FAILED;
   }
   strcpy((char*)indication->u.userInput->u.alphanumeric, data);
   OOTRACEDBGA3 ("Built UserInputIndication_alphanumeric (%s, %s)\n", 
                 call->callType, call->callToken);

   ret = ooSendH245Msg (call, ph245msg);

   if (ret != OO_OK) {
      OOTRACEERR3 
         ("Error:Failed to enqueue UserInputIndication_alphanumeric "
          "message to outbound queue.(%s, %s)\n", call->callType, 
          call->callToken);
   }
   
   ooFreeH245Message (call, ph245msg);
   return ret;
}

int ooSendH245UserInputIndication_signal
   (OOH323CallData *call, const char *data)
{
   int ret=0;
   H245IndicationMessage* indication=NULL;
   H245Message *ph245msg=NULL;
   OOCTXT *pctxt=&gH323ep.msgctxt;

   ret = ooCreateH245Message
      (&ph245msg, T_H245MultimediaSystemControlMessage_indication);

   if (ret != OO_OK) {
      OOTRACEERR3("Error:H245 message creation failed for - H245UserInput"
                  "Indication_signal (%s, %s)\n",call->callType, 
                  call->callToken);
      return OO_FAILED;
   }
   ph245msg->msgType = OOUserInputIndication;
   indication = ph245msg->h245Msg.u.indication;

   indication->t = T_H245IndicationMessage_userInput;
   indication->u.userInput = 
      (H245UserInputIndication*)
      memAllocZ (pctxt, sizeof(H245UserInputIndication));

   if(!indication->u.userInput)
   {
      OOTRACEERR3("Error: Memory - ooH245UserInputIndication_signal - "
                  " userInput (%s, %s)\n", call->callType, call->callToken);
      ooFreeH245Message(call, ph245msg);
      return OO_FAILED;
   }
   indication->u.userInput->t = T_H245UserInputIndication_signal;
   indication->u.userInput->u.signal = (H245UserInputIndication_signal*)
                      memAllocZ(pctxt, sizeof(H245UserInputIndication_signal));
   indication->u.userInput->u.signal->signalType = (ASN1IA5String)
                                              memAlloc(pctxt, strlen(data)+1);
   if(!indication->u.userInput->u.signal ||
      !indication->u.userInput->u.signal->signalType)
   {
      OOTRACEERR3("Error: Memory - ooH245UserInputIndication_signal - "
                  "signal (%s, %s).\n", call->callType, call->callToken);
      ooFreeH245Message(call, ph245msg);
      return OO_FAILED;
   }
   strcpy((char*)indication->u.userInput->u.signal->signalType, data);
   OOTRACEDBGA3 ("Built UserInputIndication_signal (%s, %s)\n", 
                 call->callType, call->callToken);

   ret = ooSendH245Msg (call, ph245msg);

   if (ret != OO_OK) {
      OOTRACEERR3 
         ("Error:Failed to enqueue UserInputIndication_signal "
          "message to outbound queue.(%s, %s)\n", call->callType, 
          call->callToken);
   }
   
   ooFreeH245Message (call, ph245msg);
   return ret;
}



/* IVeS */
int ooSendH245VideoUpdateRequest(OOH323CallData *call, int firstgob, int nbgobs)
{
  int ret = 0;
  H245CommandMessage* cmd=NULL;
  H245Message *ph245msg=NULL;
  OOCTXT *pctxt=&gH323ep.msgctxt;
  OOLogicalChannel * pChannel = NULL;
  pChannel = call->logicalChans;
  while(pChannel)
  {
    OOTRACEAST(OOTRCLVLDBGA,"ooSendH245VideoUpdateRequest :have OLC video received  (%s, %s)"
               "find  on logical chan %d [%s] %s.\n",
               call->callType, call->callToken,pChannel->sessionID,
               pChannel->dir, ooGetCapTypeText(pChannel->chanCap->cap) );

    if ( pChannel->type == OO_CAP_TYPE_VIDEO   && strcmp( pChannel->dir, "receive") == 0 )
    {
      OOTRACEDBGA4 ("ooSendH245VideoUpdateRequest :have OLC video received  (%s, %s)"
                    " on logical chan %d.\n",
                    call->callType, call->callToken,pChannel->sessionID );

      ret = ooCreateH245Message
        (&ph245msg, T_H245MultimediaSystemControlMessage_command);

      if (ret != OO_OK) {
        OOTRACEERR3("Error:H245 message creation failed for - H245MiscellaneousCommand"
                    "(%s, %s)\n",call->callType, call->callToken);
        return OO_FAILED;
      }
      ph245msg->msgType = OOMiscellaneousCommand; 
      cmd = ph245msg->h245Msg.u.command;
      cmd->t =  T_H245CommandMessage_miscellaneousCommand;

      cmd->u.miscellaneousCommand =
        (H245MiscellaneousCommand*) memAllocZ (pctxt, sizeof(H245MiscellaneousCommand));

      if( !cmd->u.miscellaneousCommand )
      {
        OOTRACEERR3("Error: Memory - ooSendH245VideoUpdateRequest "
                    " (%s, %s)\n", call->callType, call->callToken);
        ooFreeH245Message(call, ph245msg);
        return OO_FAILED;
      }

      cmd->u.miscellaneousCommand->logicalChannelNumber  = pChannel->sessionID;
      cmd->u.miscellaneousCommand->type.t = T_H245MiscellaneousCommand_type_videoFastUpdatePicture;
      /* IVeS - a ameliorer */
      cmd->u.miscellaneousCommand->type.u.videoFastUpdateGOB = NULL ;
      OOTRACEDBGA4 ("Built VIdeoUpdateRquest (%s, %s) on logical chan %d.\n",
                    call->callType, call->callToken, cmd->u.miscellaneousCommand->logicalChannelNumber);   

      ret = ooSendH245Msg (call, ph245msg);

      if (ret != OO_OK) {
        OOTRACEERR3
          ("Error:Failed to enqueue VideoUpdateRequest "
           "message to outbound queue.(%s, %s)\n", call->callType,
           call->callToken);

      }
      ooFreeH245Message (call, ph245msg);
    }
    pChannel = pChannel->next;
  }
}

int ooSendH245MIlogicalchannelActive(OOH323CallData *call, H245LogicalChannelNumber LogicalChannelNumber)
{
  int ret = 0;
  H245IndicationMessage* indication=NULL;
  H245Message *ph245msg=NULL;
  OOCTXT *pctxt=&gH323ep.msgctxt;
  OOLogicalChannel * pChannel = NULL;
  pChannel = call->logicalChans;
#if 0
  while(pChannel)
  {
    OOTRACEAST(OOTRCLVLDBGA,"ooSendH245MIlogicalchannelActive :  (%s, %s)"
               "find  on logical chan %d [%s] %s.\n",
               call->callType, call->callToken,pChannel->sessionID,
               pChannel->dir, ooGetCapTypeText(pChannel->chanCap->cap) );

    if ( strcmp( pChannel->dir, "receive") == 0 )
    {
      OOTRACEDBGA4 ("ooSendH245MIlogicalchannelActive :have OLC received  (%s, %s)"
                    " on logical chan %d.\n",
                    call->callType, call->callToken,pChannel->sessionID );

      ret = ooCreateH245Message(&ph245msg, T_H245MultimediaSystemControlMessage_indication);

      if (ret != OO_OK) {
        OOTRACEERR3("ooSendH245MIlogicalchannelActive Error:H245 message creation failed for - H245miscellaneousIndication "
                    "(%s, %s)\n",call->callType, call->callToken);
        return OO_FAILED;
      }
      ph245msg->msgType = OOMiscellaneousIndication; 
      indication = ph245msg->h245Msg.u.indication;
      indication->t = T_H245IndicationMessage_miscellaneousIndication;
      indication->u.miscellaneousIndication =(H245MiscellaneousIndication*)memAllocZ (pctxt, sizeof(H245MiscellaneousIndication));

   
      if( !indication->u.miscellaneousIndication )
      {
        OOTRACEERR3("ooSendH245MIlogicalchannelActive Error: Memory -  "
                    " (%s, %s)\n", call->callType, call->callToken);
        ooFreeH245Message(call, ph245msg);
        return OO_FAILED;
      }

      indication->u.miscellaneousIndication->logicalChannelNumber  = pChannel->sessionID;
      indication->u.miscellaneousIndication->type.t = T_H245MiscellaneousIndication_type_logicalChannelActive;
      OOTRACEDBGA4 ("ooSendH245MIlogicalchannelActive Built logicalchannelActive (%s, %s) on logical chan %d.\n",
                    call->callType, call->callToken, indication->u.miscellaneousIndication->logicalChannelNumber);   

      ret = ooSendH245Msg (call, ph245msg);

      if (ret != OO_OK) {
        OOTRACEERR3
          ("ooSendH245MIlogicalchannelActive Error:Failed to enqueue logicalchannelActive "
           "message to outbound queue.(%s, %s)\n", call->callType,
           call->callToken);

      }
      ooFreeH245Message (call, ph245msg);
    }
    pChannel = pChannel->next;
  }
#else
  OOTRACEAST(OOTRCLVLDBGA,"ooSendH245MIlogicalchannelActive :logical chan %d  (%s, %s).\n",LogicalChannelNumber,
                call->callType, call->callToken );
  ret = ooCreateH245Message(&ph245msg, T_H245MultimediaSystemControlMessage_indication);

  if (ret != OO_OK) {
    OOTRACEERR3("ooSendH245MIlogicalchannelActive Error:H245 message creation failed for - H245miscellaneousIndication "
                "(%s, %s)\n",call->callType, call->callToken);
    return OO_FAILED;
  }
  ph245msg->msgType = OOMiscellaneousIndication; 
  indication = ph245msg->h245Msg.u.indication;
  indication->t = T_H245IndicationMessage_miscellaneousIndication;
  indication->u.miscellaneousIndication =(H245MiscellaneousIndication*)memAllocZ (pctxt, sizeof(H245MiscellaneousIndication));

   
  if( !indication->u.miscellaneousIndication )
  {
    OOTRACEERR3("ooSendH245MIlogicalchannelActive Error: Memory -  "
                " (%s, %s)\n", call->callType, call->callToken);
    ooFreeH245Message(call, ph245msg);
    return OO_FAILED;
  }

  indication->u.miscellaneousIndication->logicalChannelNumber  = LogicalChannelNumber;
  indication->u.miscellaneousIndication->type.t = T_H245MiscellaneousIndication_type_logicalChannelActive;
  OOTRACEAST(OOTRCLVLDBGA,"ooSendH245MIlogicalchannelActive Built logicalchannelActive (%s, %s) on logical chan %d.\n",
                call->callType, call->callToken, indication->u.miscellaneousIndication->logicalChannelNumber);   

  ret = ooSendH245Msg (call, ph245msg);

  if (ret != OO_OK) {
    OOTRACEERR3
      ("ooSendH245MIlogicalchannelActive Error:Failed to enqueue logicalchannelActive "
       "message to outbound queue.(%s, %s)\n", call->callType,
       call->callToken);

  }
  ooFreeH245Message (call, ph245msg);
#endif
}

int ooOnReceivedRequestRoundTripDelayRequest(OOH323CallData *call, H245SequenceNumber sequenceNumber)
{
  int ret = 0;
  H245ResponseMessage *response = NULL;
  H245Message *ph245msg=NULL;
  OOCTXT *pctxt=&gH323ep.msgctxt;

  OOTRACEAST(OOTRCLVLDBGA,"ooOnReceivedRequestRoundTripDelayRequest %d  (%s, %s)\n",
             sequenceNumber,call->callType, call->callToken) ;

  ret = ooCreateH245Message
    (&ph245msg, T_H245MultimediaSystemControlMessage_response);

   if(ret != OO_OK)
   {
      OOTRACEERR3("ERROR:Memory allocation for RoundTripDelayRespons message "
                  "failed (%s, %s)\n", call->callType, call->callToken);
      return OO_FAILED;
   }

   ph245msg->msgType = OORoundTripDelayRespons; 
   response = ph245msg->h245Msg.u.response;
   memset(response, 0, sizeof(H245ResponseMessage));
   response->t = T_H245ResponseMessage_roundTripDelayResponse ;
   response->u.roundTripDelayResponse = (H245RoundTripDelayResponse*)
                   ASN1MALLOC(pctxt, sizeof(H245RoundTripDelayResponse));
   memset(response->u.roundTripDelayResponse, 0, sizeof(H245RoundTripDelayResponse));
   response->u.roundTripDelayResponse->sequenceNumber = sequenceNumber ;

  ret = ooSendH245Msg(call, ph245msg);
   if(ret != OO_OK)
   {
      OOTRACEERR3("Error:Failed to enqueue RoundTripDelayRespons message"
                  " to outbound queue. (%s, %s)\n", call->callType, 
                  call->callToken);
   }
   
   ooFreeH245Message(call, ph245msg);
   return ret;
}

int ooOnReceivedConferenceRequest(OOH323CallData *call, int request)
{
  int ret = 0;
  H245ResponseMessage *response = NULL;
  H245Message *ph245msg=NULL;
  OOCTXT *pctxt=&gH323ep.msgctxt;
  int send = FALSE ;

  OOTRACEAST(OOTRCLVLDBGA,"ooOnReceivedConferenceRequest %d  (%s, %s)\n",
             request,call->callType, call->callToken) ;

  ret = ooCreateH245Message
    (&ph245msg, T_H245MultimediaSystemControlMessage_response);

   if(ret != OO_OK)
   {
      OOTRACEERR3("ERROR:Memory allocation for ConferenceRespons message "
                  "failed (%s, %s)\n", call->callType, call->callToken);
      return OO_FAILED;
   }

   ph245msg->msgType = OOConferenceRequestRespons; 
   response = ph245msg->h245Msg.u.response;
   memset(response, 0, sizeof(H245ResponseMessage));
   response->t = T_H245ResponseMessage_conferenceResponse ;
   response->u.conferenceResponse = (H245ConferenceResponse*)ASN1MALLOC(pctxt, sizeof(H245ConferenceResponse));
   memset(response->u.conferenceResponse, 0, sizeof(H245ConferenceResponse));
   switch ( request ){
     case T_H245ConferenceRequest_terminalListRequest:
       OOTRACEAST(OOTRCLVLDBGA,"ooOnReceivedConferenceRequest , build terminalListResponse  (%s, %s)\n",call->callType, call->callToken) ;
       response->u.conferenceResponse->t = T_H245ConferenceResponse_terminalListResponse ;
       response->u.conferenceResponse->u.terminalListResponse =  (H245ConferenceResponse_terminalListResponse*)ASN1MALLOC(pctxt, sizeof(H245ConferenceResponse_terminalListResponse));
       H245ConferenceResponse_terminalListResponse* terminalListResponse = response->u.conferenceResponse->u.terminalListResponse ;
       dListInit(terminalListResponse);
       send = TRUE ;
       break ; 
     

     case T_H245ConferenceRequest_enterH243TerminalID:
/* 
   typedef struct EXTERN H245ConferenceResponse_mCTerminalIDResponse {
   H245TerminalLabel terminalLabel;
   H245TerminalID terminalID;
   } H245ConferenceResponse_mCTerminalIDResponse;

   typedef struct EXTERN H245ConferenceResponse_terminalIDResponse {
   H245TerminalLabel terminalLabel;
   H245TerminalID terminalID;
   } H245ConferenceResponse_terminalIDResponse;

   typedef struct EXTERN H245TerminalLabel {
   H245McuNumber mcuNumber;
   H245TerminalNumber terminalNumber;
   } H245TerminalLabel;

   typedef struct EXTERN H245TerminalID {
   ASN1UINT numocts;
   ASN1OCTET data[128];
   } H245TerminalID;

*/
       OOTRACEAST(OOTRCLVLDBGA,"ooOnReceivedConferenceRequest , build terminalIDResponse  (%s, %s)\n",call->callType, call->callToken) ;
       response->u.conferenceResponse->t = T_H245ConferenceResponse_terminalIDResponse ;
       response->u.conferenceResponse->u.terminalIDResponse =  (H245ConferenceResponse_terminalIDResponse*)ASN1MALLOC(pctxt, sizeof(H245ConferenceResponse_terminalIDResponse));
       memset( response->u.conferenceResponse->u.terminalIDResponse , 0 , sizeof(H245ConferenceResponse_terminalIDResponse));
       response->u.conferenceResponse->u.terminalIDResponse->terminalLabel.mcuNumber = 1 ;
       response->u.conferenceResponse->u.terminalIDResponse->terminalLabel.terminalNumber = 1 ;
       if(gH323ep.callerid) {
         strncpy((char*)response->u.conferenceResponse->u.terminalIDResponse->terminalID.data, gH323ep.callerid, 128);
       }
       else {
         response->u.conferenceResponse->u.terminalIDResponse->terminalID.data[0] = '\0';
       }

       response->u.conferenceResponse->u.terminalIDResponse->terminalID.numocts = strlen( (char*)response->u.conferenceResponse->u.terminalIDResponse->terminalID.data);
       OOTRACEAST(OOTRCLVLDBGA,"ooOnReceivedConferenceRequest , terminalIDResponse  terminalID[%s/%d]  (%s, %s)\n",
                  (char*)response->u.conferenceResponse->u.terminalIDResponse->terminalID.data,response->u.conferenceResponse->u.terminalIDResponse->terminalID.numocts,
                  call->callType, call->callToken) ;
       send = TRUE ;
       break ; 
     default :
       OOTRACEERR4("Error:Failed to make ConferenceRespons for request %d (%s, %s)\n", 
                   request,call->callType,call->callToken);
       break ; 
   }

   if ( send ){
     ret = ooSendH245Msg(call, ph245msg);
     if(ret != OO_OK)
     {
       OOTRACEERR3("Error:Failed to enqueue ConferenceRequestRespons message"
                   " to outbound queue. (%s, %s)\n", call->callType, 
                   call->callToken);
     }
   }
   ooFreeH245Message(call, ph245msg);
   return ret;
}

int ooOpenLogicalChannels(OOH323CallData *call)
{
   int ret=0;

   OOTRACEAST(OOTRCLVLDBGA,"ooOpenLogicalChannels[%s] (%s, %s) \n", 
              (call->masterSlaveState==OO_MasterSlave_Master)?"master":"slave",
              call->callType, call->callToken );

   /* Audio channels */
   if(gH323ep.callMode == OO_CALLMODE_AUDIOCALL ||
      gH323ep.callMode == OO_CALLMODE_AUDIOTX)
   {
      //if (!OO_TESTFLAG (call->flags, OO_M_AUDIOSESSION))
      //{
         ret = ooOpenLogicalChannel(call, OO_CAP_TYPE_AUDIO);
         if(ret != OO_OK)
         {
            OOTRACEERR3("ERROR:Failed to open audio channels. Clearing call."
                        "(%s, %s)\n", call->callType, call->callToken);
            if(call->callState < OO_CALL_CLEAR)
            {
               call->callEndReason = OO_REASON_LOCAL_CLEARED;
               call->callState = OO_CALL_CLEAR;
            }
            return ret;
         }
      // }
   }

   if(gH323ep.callMode == OO_CALLMODE_VIDEOCALL)
   {
     /*      if (!OO_TESTFLAG (call->flags, OO_M_AUDIOSESSION))
             {*/
     ret = ooOpenLogicalChannel(call, OO_CAP_TYPE_AUDIO);
     if(ret != OO_OK)
     {
       OOTRACEERR3("ERROR:Failed to open audio channel. Clearing call."
                   "(%s, %s)\n", call->callType, call->callToken);
       if(call->callState < OO_CALL_CLEAR)
       {
         call->callEndReason = OO_REASON_LOCAL_CLEARED;
         call->callState = OO_CALL_CLEAR;
       }
       return ret;
     }
     //}
     /*      if(!OO_TESTFLAG(call->flags, OO_M_VIDEOSESSION))
             {*/
     ret = ooOpenLogicalChannel(call, OO_CAP_TYPE_VIDEO);
     if(ret != OO_OK)
     {
       OOTRACEERR3("ERROR:Failed to open video channel. Clearing call."
                   "(%s, %s)\n", call->callType, call->callToken);
       if(call->callState < OO_CALL_CLEAR)
       {
         call->callEndReason = OO_REASON_LOCAL_CLEARED;
         call->callState = OO_CALL_CLEAR;
       }
       return ret;
     }
     //}
   }
  
   return OO_OK;
}

/* CapType indicates whether to Open Audio or Video channel */
int ooOpenLogicalChannel(OOH323CallData *call, enum OOCapType capType )
{
  ooH323EpCapability *epCap=NULL;
  int k=0;

  OOTRACEAST(OOTRCLVLDBGA,"ooOpenLogicalChannel [%s] (%s, %s) \n", 
             (call->masterSlaveState==OO_MasterSlave_Master)?"master":"slave",
             call->callType, call->callToken );

  /* Check whether local endpoint has audio capability */
  if(gH323ep.myCaps == 0 && call->ourCaps == 0)
  {
    OOTRACEERR3("ERROR:Local endpoint does not have any audio capabilities"
                " (%s, %s)\n", call->callType, call->callToken);
    return OO_FAILED;
  }
   
  /* Go through local endpoints capabilities sequentially, and find out the
     first one which has a match in the remote endpoints receive capabilities.
  */
  switch (capType)
  {
    case OO_CAP_TYPE_AUDIO:
      OOTRACEINFO3("Looking for matching AUDIO capabilities for call (%s, %s)\n", 
                   call->callType, call->callToken);
      break;

    case OO_CAP_TYPE_VIDEO:
      OOTRACEINFO3("Looking for matching VIDEO capabilities for call (%s, %s)\n",           
                   call->callType, call->callToken);
      break;
    default:
      OOTRACEINFO4("Looking for matching capabilities of type %d for call (%s, %s)\n", 
                   capType, call->callType, call->callToken);
      break;
  }

  if(call->masterSlaveState == OO_MasterSlave_Master)
  {
    for(k=0; k<call->capPrefs.index; k++)
    {
      OOTRACEAST(OOTRCLVLDBGA,"Master: choice Find item %d codec[%s] order[%d] \n",k , 
                 ooGetCapTypeText(call->capPrefs.order[k]), call->capPrefs.index );
      /*Search for audio caps only */
      if(capType == OO_CAP_TYPE_AUDIO && call->capPrefs.order[k] > OO_CAP_VIDEO_BASE)
        continue;
      /* Search for video caps only */
      if(capType == OO_CAP_TYPE_VIDEO && call->capPrefs.order[k] <= OO_CAP_VIDEO_BASE)
        continue;

      epCap = call->jointCaps;
      OOTRACEAST(OOTRCLVLDBGA,"Master: choice epCap [%s][%d] (%s, %s) \n", 
                 ooGetCapTypeText(epCap->cap),epCap->cap,call->callType, call->callToken );

      while(epCap){
        if(epCap->cap == call->capPrefs.order[k] && (epCap->dir & OOTX))
          break;
        epCap = epCap->next;
      }

      if(!epCap)
      {
        OOTRACEDBGA4("Prefereed capability %d is not a local transmit "
                     "capability(%s, %s)\n", call->capPrefs.order[k],
                     call->callType, call->callToken);
        continue;
      }
      OOTRACEAST(OOTRCLVLDBGA,"have common capability epCap [%s][%d] (%s, %s) \n", 
                 ooGetCapTypeText(epCap->cap),epCap->cap,call->callType, call->callToken );
      break ;
    }
    if(!epCap)
    {
      OOTRACEERR4("ERROR:Incompatible capabilities - Can not open " "%s channel (%s, %s)\n", 
                  (capType==OO_CAP_TYPE_AUDIO)?"audio":"video", call->callType,
                  call->callToken);
      return OO_FAILED;
    }
  }
  else if(call->masterSlaveState == OO_MasterSlave_Slave)
  {
    OOTRACEAST(OOTRCLVLDBGA,"Slave choice call (%s, %s) \n", 
               call->callType, call->callToken ); 
    epCap = call->jointCaps;

    while(epCap){
      OOTRACEAST(OOTRCLVLDBGA,"ooOpenLogicalChannel(): slave mode, local cap %s dir %d.\n",
                  ooGetCapTypeText(epCap->cap), epCap->dir );
      if(epCap->capType == capType && epCap->dir & OOTX) { break; }
      epCap = epCap->next;
    }
    if(!epCap)
    {
      OOTRACEERR4("ERROR:Incompatible capabilities (in slave mode)- Can not open "
                  "%s channel (%s, %s) \n", 
                  (capType==OO_CAP_TYPE_AUDIO)?"audio":"video", call->callType,
                  call->callToken);
      return OO_FAILED;
    }

    if ( epCap )
    {
      OOTRACEAST(OOTRCLVLDBGA,"Slave choice epCap [%s][%d] (%s, %s) \n", 
                 ooGetCapTypeText(epCap->cap),epCap->cap,call->callType, call->callToken );   
    }
  }

  if ( epCap->cap == OO_GENERICVIDEO && epCap->params ) {
    // Check for re invite 
    if(gH323ep.h323Callbacks.onReceivedMiscellaneous){
      struct OOH264CapParams caps;
      memcpy( &caps , epCap->params , sizeof(OOH264CapParams));
      gH323ep.h323Callbacks.onReceivedMiscellaneous(call,OO_MISC_CMD_CAPABILITY,&caps);
    }
  }
  switch(epCap->cap)
  {
    case OO_G711ALAW64K:
    case OO_G711ULAW64K:
    case OO_G711ALAW56K:
    case OO_G711ULAW56K:
      /*case OO_G726:*/
    case OO_G728:
    case OO_G729A:
    case OO_G729:
    case OO_G7231:
    case OO_GSMFULLRATE:
    case OO_H263VIDEO:
    case OO_GENERICVIDEO:
      ooOpenChannel(call, epCap);
      break;
    case OO_GSMHALFRATE:
    case OO_GSMENHANCEDFULLRATE:

    default:
      OOTRACEERR4("ERROR:Unknown/unsupported Capability %s for call (%s, %s)\n", 
                  ooGetCapTypeText(epCap->cap), call->callType, 
                  call->callToken);
  }
  return OO_OK;
}

int ooOpenChannel(OOH323CallData* call, ooH323EpCapability *epCap)
{
   int ret;
   H245Message *ph245msg = NULL;
   H245RequestMessage * request;
   OOCTXT *pctxt = NULL;
   H245OpenLogicalChannel_forwardLogicalChannelParameters *flcp = NULL;
   H245AudioCapability *audioCap = NULL;
   H245VideoCapability *videoCap = NULL;
   H245H2250LogicalChannelParameters *h2250lcp = NULL;
   H245UnicastAddress *unicastAddrs = NULL;
   H245UnicastAddress_iPAddress *iPAddress = NULL;
   unsigned session_id=0;
   ooLogicalChannel *pLogicalChannel = NULL;
   
   OOTRACEAST(OOTRCLVLDBGA,"ooOpenChannel [%s] epcap[%s][%d]  (%s, %s) \n", 
              (call->masterSlaveState==OO_MasterSlave_Master)?"master":"slave",
              ooGetCapTypeText(epCap->cap),epCap->cap,
              call->callType, call->callToken );

   
   if(call->masterSlaveState == OO_MasterSlave_Master)
   {
     SetLocalCapChoice( call , epCap ) ;
   }
   ret = ooCreateH245Message(&ph245msg,  T_H245MultimediaSystemControlMessage_request);
   if(ret != OO_OK)
   {
      OOTRACEERR4("Error: ooOpenChannel H245 message creation failed for - Open %s"
                  "channel (%s, %s)\n", ooGetCapTypeText(epCap->cap), 
                  call->callType, call->callToken);
      return OO_FAILED;
   }

   ph245msg->msgType = OOOpenLogicalChannel;
   ph245msg->logicalChannelNo =  call->logicalChanNoCur++;
   if(call->logicalChanNoCur > call->logicalChanNoMax)
      call->logicalChanNoCur = call->logicalChanNoBase; 

   request = ph245msg->h245Msg.u.request;
   pctxt = &gH323ep.msgctxt;
   memset(request, 0, sizeof(H245RequestMessage));

   request->t = T_H245RequestMessage_openLogicalChannel;
   request->u.openLogicalChannel = (H245OpenLogicalChannel*)
                     memAlloc(pctxt, sizeof(H245OpenLogicalChannel));
   if(!request->u.openLogicalChannel)
   {
      OOTRACEERR3("Error:Memory - ooOpenChannel - openLogicalChannel."
                  "(%s, %s)\n", call->callType, call->callToken);
      return OO_FAILED;

   }
   memset(request->u.openLogicalChannel, 0,  sizeof(H245OpenLogicalChannel));
   request->u.openLogicalChannel->forwardLogicalChannelNumber = 
                                                 ph245msg->logicalChannelNo;

   
   session_id = ooCallGenerateSessionID(call, epCap->capType, "transmit");


   pLogicalChannel = ooAddNewLogicalChannel(call, 
                   request->u.openLogicalChannel->forwardLogicalChannelNumber,
                   session_id, "transmit", epCap);
   
   if(!pLogicalChannel)
   {
      OOTRACEERR3("ERROR:Failed to add new logical channel entry (%s, %s)\n",
                  call->callType, call->callToken);
      ooFreeH245Message(call, ph245msg);
      return OO_FAILED;
   }
   /* Populate H245OpenLogicalChannel_ForwardLogicalChannel Parameters*/
   flcp = &(request->u.openLogicalChannel->forwardLogicalChannelParameters);
   flcp->m.portNumberPresent = 0;
   flcp->m.forwardLogicalChannelDependencyPresent = 0;
   flcp->m.replacementForPresent = 0;

   /* data type of channel */
   if(epCap->capType == OO_CAP_TYPE_AUDIO)
   {
      flcp->dataType.t = T_H245DataType_audioData;
      /* set audio capability for channel */
      audioCap = ooCapabilityCreateAudioCapability(epCap,pctxt, OOTX);
      if(!audioCap)
      {
         OOTRACEERR4("Error:Failed to create duplicate audio capability in "
                     "ooOpenChannel- %s (%s, %s)\n", 
                     ooGetCapTypeText(epCap->cap), call->callType, 
                     call->callToken);
         ooFreeH245Message(call, ph245msg);
         return OO_FAILED;
      }
   
      flcp->dataType.u.audioData = audioCap;
   }
   else if(epCap->capType == OO_CAP_TYPE_VIDEO)
   {
      flcp->dataType.t = T_H245DataType_videoData;      
      videoCap = ooCapabilityCreateVideoCapability(epCap, pctxt, OOTX);
      if(!videoCap)
      {
         OOTRACEERR4("Error:Failed to create duplicate video capability in "
                     "ooOpenChannel- %s (%s, %s)\n", 
                     ooGetCapTypeText(epCap->cap), call->callType, 
                     call->callToken);
         ooFreeH245Message(call, ph245msg);
         return OO_FAILED;
      }
   
      flcp->dataType.u.videoData = videoCap;
   }
   else{
      OOTRACEERR1("Error: Unhandled media type in ooOpenChannel\n");
      return OO_FAILED;
   }
  
   flcp->multiplexParameters.t = 
      T_H245OpenLogicalChannel_forwardLogicalChannelParameters_multiplexParameters_h2250LogicalChannelParameters;
   flcp->multiplexParameters.u.h2250LogicalChannelParameters = 
                         (H245H2250LogicalChannelParameters*)ASN1MALLOC(pctxt, 
                          sizeof(H245H2250LogicalChannelParameters));

   h2250lcp = flcp->multiplexParameters.u.h2250LogicalChannelParameters;
   memset(h2250lcp, 0, sizeof(H245H2250LogicalChannelParameters));

   h2250lcp->sessionID = session_id;

   h2250lcp->mediaGuaranteedDelivery = 0;
   h2250lcp->silenceSuppression = 0;
   h2250lcp->m.mediaControlChannelPresent = 1;
   
   h2250lcp->mediaControlChannel.t = 
                                 T_H245TransportAddress_unicastAddress;
   h2250lcp->mediaControlChannel.u.unicastAddress =  (H245UnicastAddress*)
                         ASN1MALLOC(pctxt, sizeof(H245UnicastAddress));

   unicastAddrs = h2250lcp->mediaControlChannel.u.unicastAddress;
   memset(unicastAddrs, 0, sizeof(H245UnicastAddress));
   unicastAddrs->t = T_H245UnicastAddress_iPAddress;
   unicastAddrs->u.iPAddress = (H245UnicastAddress_iPAddress*)
               ASN1MALLOC(pctxt, sizeof(H245UnicastAddress_iPAddress));
   iPAddress = unicastAddrs->u.iPAddress;
   memset(iPAddress, 0, sizeof(H245UnicastAddress_iPAddress));

   ooSocketConvertIpToNwAddr(pLogicalChannel->localIP,iPAddress->network.data);

   iPAddress->network.numocts = 4;
   iPAddress->tsapIdentifier = pLogicalChannel->localRtcpPort;
   pLogicalChannel->state = OO_LOGICALCHAN_PROPOSED; 

   /* IVeS - ajout dynamic payload type pour H.264 */
   if (epCap->cap == OO_GENERICVIDEO)
   {
      if (pLogicalChannel->rtpPayloadType >= 96 && pLogicalChannel->rtpPayloadType <= 127)
      {
         h2250lcp->m.dynamicRTPPayloadTypePresent = TRUE;
         h2250lcp->dynamicRTPPayloadType = pLogicalChannel->rtpPayloadType;
         OOTRACEAST(OOTRCLVLDBGA,"Dynamic RTP pt = %d for cap %s (%s, %s)\n", 
                      pLogicalChannel->rtpPayloadType, ooGetCapTypeText(epCap->cap), call->callType, 
                 call->callToken);
#if 1 // if have packetisation mode 0
         h2250lcp->m.mediaPacketizationPresent = TRUE ;
         h2250lcp->mediaPacketization.t = T_H245H2250LogicalChannelParameters_mediaPacketization_rtpPayloadType ;

         ASN1OBJID h241_h264 =
           {
             8, /* nb de composants de l'OID */
             { 0, 0, 8, 241, 0, 0, 0,0 }
           };

         h2250lcp->mediaPacketization.u.rtpPayloadType = (H245RTPPayloadType*) ASN1MALLOC(pctxt, sizeof(H245RTPPayloadType));
         H245RTPPayloadType* rtpPayloadType = h2250lcp->mediaPacketization.u.rtpPayloadType ;
         memset(rtpPayloadType, 0, sizeof(H245RTPPayloadType));
         rtpPayloadType->m.payloadTypePresent = TRUE ;
         rtpPayloadType->payloadDescriptor.t=T_H245RTPPayloadType_payloadDescriptor_oid;
         rtpPayloadType->payloadDescriptor.u.oid =  ALLOC_ASN1ELEM (pctxt, ASN1OBJID);
         memcpy(rtpPayloadType->payloadDescriptor.u.oid, &h241_h264, sizeof(h241_h264));
         rtpPayloadType->payloadType = pLogicalChannel->rtpPayloadType ;
#endif
      }
   }
   OOTRACEAST(OOTRCLVLDBGA,"[H323/H245]ooOpenChannel  SEND --> OLC:%s:%d <=[ %s ]  (%s, %s)\n",
              (pLogicalChannel->localIP)?pLogicalChannel->localIP:"Null", 
              pLogicalChannel->localRtpPort,
              ooGetCapTypeText(epCap->cap),
              call->callType, call->callToken);

   ret = ooSendH245Msg(call, ph245msg);
   // call->haveSendOLC = TRUE ;
   if(ret != OO_OK)
   {
      OOTRACEERR3("Error:Failed to enqueue OpenLogicalChannel to outbound "
                 "queue. (%s, %s)\n", call->callType,
                 call->callToken);
   }
   ooFreeH245Message(call, ph245msg);
  
    return ret;
}


/* Used to build  OLCs for fast connect. Keep in mind that forward and 
   reverse 
   are always with respect to the endpoint which proposes channels 
   TODO: Need to clean logical channel in case of failure.    */
int ooBuildFastStartOLC
   (OOH323CallData *call, H245OpenLogicalChannel *olc, 
    ooH323EpCapability *epCap, OOCTXT*pctxt, int dir)
{
   OOBOOL reverse=FALSE, forward=FALSE;
   unsigned sessionID=0;
   H245OpenLogicalChannel_forwardLogicalChannelParameters *flcp=NULL;
   H245OpenLogicalChannel_reverseLogicalChannelParameters *rlcp=NULL;
   H245H2250LogicalChannelParameters *pH2250lcp1=NULL, *pH2250lcp2=NULL;
   H245UnicastAddress *pUnicastAddrs=NULL, *pUniAddrs=NULL;
   H245UnicastAddress_iPAddress *pIpAddrs=NULL, *pUniIpAddrs=NULL;
   unsigned session_id = 0;
   ooLogicalChannel *pLogicalChannel = NULL;
   int outgoing=FALSE;

   if(!strcmp(call->callType, "outgoing"))   
      outgoing = TRUE;
   
   if(dir & OORX)
   {
      OOTRACEDBGA3("Building OpenLogicalChannel for Receive  Capability "
                   "(%s, %s)\n", call->callType, call->callToken);
      session_id = ooCallGenerateSessionID(call, epCap->capType, "receive");
      pLogicalChannel = ooAddNewLogicalChannel(call, 
                                 olc->forwardLogicalChannelNumber, session_id, 
                                 "receive", epCap);
      if(outgoing)
         reverse = TRUE;
      else
         forward = TRUE;
   }
   else if(dir & OOTX)
   {
      OOTRACEDBGA3("Building OpenLogicalChannel for transmit Capability "
                   "(%s, %s)\n", call->callType, call->callToken);
      session_id = ooCallGenerateSessionID(call, epCap->capType, "transmit");
      pLogicalChannel = ooAddNewLogicalChannel(call, 
                                  olc->forwardLogicalChannelNumber, session_id,
                                  "transmit", epCap);
      if(outgoing)
         forward = TRUE;
      else
         reverse = TRUE;
   }
   else if(dir & OORXTX)
   {
      OOTRACEDBGA3("Building OpenLogicalChannel for ReceiveAndTransmit  "
                   "Capability (%s, %s)\n", call->callType, call->callToken);
      reverse = 1;
      forward = 1;
      OOTRACEERR3("Symmetric capability is not supported as of now (%s, %s)\n",
                   call->callType, call->callToken);
      return OO_FAILED;
   }

   if(forward)
   {
      OOTRACEDBGC3("Building forward olc. (%s, %s)\n", call->callType, 
                    call->callToken);
      flcp = &(olc->forwardLogicalChannelParameters);
      memset(flcp, 0, 
             sizeof(H245OpenLogicalChannel_forwardLogicalChannelParameters));

      if(epCap->capType == OO_CAP_TYPE_AUDIO) {
         sessionID =1;
         flcp->dataType.t = T_H245DataType_audioData;
         flcp->dataType.u.audioData = ooCapabilityCreateAudioCapability(epCap, 
                                                                   pctxt, dir);
      }
      else if(epCap->capType == OO_CAP_TYPE_VIDEO) {
         sessionID = 2;
         flcp->dataType.t = T_H245DataType_videoData;
         flcp->dataType.u.videoData = ooCapabilityCreateVideoCapability(epCap,
                                                                   pctxt, dir);
      }
      flcp->multiplexParameters.t = T_H245OpenLogicalChannel_forwardLogicalChannelParameters_multiplexParameters_h2250LogicalChannelParameters;
      pH2250lcp1 = (H245H2250LogicalChannelParameters*)ASN1MALLOC(pctxt, 
                                    sizeof(H245H2250LogicalChannelParameters));
      memset(pH2250lcp1, 0, sizeof(H245H2250LogicalChannelParameters));
      flcp->multiplexParameters.t = T_H245OpenLogicalChannel_forwardLogicalChannelParameters_multiplexParameters_h2250LogicalChannelParameters;
      
      flcp->multiplexParameters.u.h2250LogicalChannelParameters = pH2250lcp1;
          
      pH2250lcp1->sessionID = sessionID;
      if(!outgoing)
      {
         pH2250lcp1->m.mediaChannelPresent = 1;
         pH2250lcp1->mediaChannel.t = 
                                    T_H245TransportAddress_unicastAddress;
         pUniAddrs = (H245UnicastAddress*) ASN1MALLOC(pctxt, 
                                                   sizeof(H245UnicastAddress));
         memset(pUniAddrs, 0, sizeof(H245UnicastAddress));
         pH2250lcp1->mediaChannel.u.unicastAddress =  pUniAddrs;
         pUniAddrs->t = T_H245UnicastAddress_iPAddress;
         pUniIpAddrs = (H245UnicastAddress_iPAddress*) ASN1MALLOC(pctxt, 
                                         sizeof(H245UnicastAddress_iPAddress));
         memset(pUniIpAddrs, 0, sizeof(H245UnicastAddress_iPAddress));
         pUniAddrs->u.iPAddress = pUniIpAddrs;
     
         ooSocketConvertIpToNwAddr(pLogicalChannel->localIP, 
                                                pUniIpAddrs->network.data);

         pUniIpAddrs->network.numocts = 4;
         pUniIpAddrs->tsapIdentifier = pLogicalChannel->localRtpPort;
      }
      pH2250lcp1->m.mediaControlChannelPresent = 1;
      pH2250lcp1->mediaControlChannel.t = 
                                 T_H245TransportAddress_unicastAddress;
      pUnicastAddrs = (H245UnicastAddress*) ASN1MALLOC(pctxt, 
                                                   sizeof(H245UnicastAddress));
      memset(pUnicastAddrs, 0, sizeof(H245UnicastAddress));
      pH2250lcp1->mediaControlChannel.u.unicastAddress =  pUnicastAddrs;
      pUnicastAddrs->t = T_H245UnicastAddress_iPAddress;
      pIpAddrs = (H245UnicastAddress_iPAddress*) ASN1MALLOC(pctxt, 
                                         sizeof(H245UnicastAddress_iPAddress));
      memset(pIpAddrs, 0, sizeof(H245UnicastAddress_iPAddress));
      pUnicastAddrs->u.iPAddress = pIpAddrs;
     
       ooSocketConvertIpToNwAddr(pLogicalChannel->localIP, 
                                                      pIpAddrs->network.data);

      pIpAddrs->network.numocts = 4;
      pIpAddrs->tsapIdentifier = pLogicalChannel->localRtcpPort;
      if(!outgoing)
      {
         if(epCap->startReceiveChannel)
         {   
            epCap->startReceiveChannel(call, pLogicalChannel);      
            OOTRACEINFO4("Receive channel of type %s started (%s, %s)\n", 
                        (epCap->capType == OO_CAP_TYPE_AUDIO)?"audio":"video",
                        call->callType, call->callToken);
         }
         else{
            OOTRACEERR4("ERROR:No callback registered to start receive %s"
                       " channel (%s, %s)\n", 
                        (epCap->capType == OO_CAP_TYPE_AUDIO)?"audio":"video", 
                        call->callType, call->callToken);
            return OO_FAILED;
         }
      }
   }

   if(reverse)
   {
      OOTRACEDBGC3("Building reverse olc. (%s, %s)\n", call->callType, 
                    call->callToken);
      olc->forwardLogicalChannelParameters.dataType.t = 
                                                      T_H245DataType_nullData;
      olc->forwardLogicalChannelParameters.multiplexParameters.t = 
         T_H245OpenLogicalChannel_forwardLogicalChannelParameters_multiplexParameters_none;
      olc->m.reverseLogicalChannelParametersPresent = 1;
      rlcp = &(olc->reverseLogicalChannelParameters);
      memset(rlcp, 0, sizeof(H245OpenLogicalChannel_reverseLogicalChannelParameters));
      if(epCap->capType == OO_CAP_TYPE_AUDIO) {
         sessionID = 1;
         rlcp->dataType.t = T_H245DataType_audioData;
  
         rlcp->dataType.u.audioData = ooCapabilityCreateAudioCapability(epCap, 
                                                                   pctxt, dir);
      }
      else if(epCap->capType == OO_CAP_TYPE_VIDEO)  {
         sessionID = 2;
         rlcp->dataType.t = T_H245DataType_videoData;
  
         rlcp->dataType.u.videoData = ooCapabilityCreateVideoCapability(epCap, 
                                                                   pctxt, dir);
      }

      rlcp->m.multiplexParametersPresent = 1;
      rlcp->multiplexParameters.t = T_H245OpenLogicalChannel_reverseLogicalChannelParameters_multiplexParameters_h2250LogicalChannelParameters;
      pH2250lcp2 = (H245H2250LogicalChannelParameters*) ASN1MALLOC(pctxt, sizeof(H245H2250LogicalChannelParameters));
      rlcp->multiplexParameters.u.h2250LogicalChannelParameters = pH2250lcp2;
      memset(pH2250lcp2, 0, sizeof(H245H2250LogicalChannelParameters));
      pH2250lcp2->sessionID = sessionID;

      if(outgoing)
      {
         pH2250lcp2->m.mediaChannelPresent = 1;

         pH2250lcp2->mediaChannel.t = 
                                    T_H245TransportAddress_unicastAddress;
         pUnicastAddrs = (H245UnicastAddress*) memAlloc(pctxt, 
                                                  sizeof(H245UnicastAddress));
         memset(pUnicastAddrs, 0, sizeof(H245UnicastAddress));
         pH2250lcp2->mediaChannel.u.unicastAddress =  pUnicastAddrs;
      
         pUnicastAddrs->t = T_H245UnicastAddress_iPAddress;
         pIpAddrs = (H245UnicastAddress_iPAddress*) memAlloc(pctxt, 
                                         sizeof(H245UnicastAddress_iPAddress));
         memset(pIpAddrs, 0, sizeof(H245UnicastAddress_iPAddress));
         pUnicastAddrs->u.iPAddress = pIpAddrs;      

         ooSocketConvertIpToNwAddr(pLogicalChannel->localIP, 
                                                       pIpAddrs->network.data);

         pIpAddrs->network.numocts = 4;
         pIpAddrs->tsapIdentifier = pLogicalChannel->localRtpPort;
      }
      pH2250lcp2->m.mediaControlChannelPresent = 1;
      pH2250lcp2->mediaControlChannel.t = 
                                 T_H245TransportAddress_unicastAddress;
      pUniAddrs = (H245UnicastAddress*) ASN1MALLOC(pctxt, sizeof(H245UnicastAddress));
      
      memset(pUniAddrs, 0, sizeof(H245UnicastAddress));
      pH2250lcp2->mediaControlChannel.u.unicastAddress =  pUniAddrs;

      
      pUniAddrs->t = T_H245UnicastAddress_iPAddress;
      pUniIpAddrs = (H245UnicastAddress_iPAddress*) ASN1MALLOC(pctxt, sizeof(H245UnicastAddress_iPAddress));
      memset(pUniIpAddrs, 0, sizeof(H245UnicastAddress_iPAddress));
      pUniAddrs->u.iPAddress = pUniIpAddrs; 

      ooSocketConvertIpToNwAddr(pLogicalChannel->localIP, 
                                                    pUniIpAddrs->network.data);
      pUniIpAddrs->network.numocts = 4;
      pUniIpAddrs->tsapIdentifier = pLogicalChannel->localRtcpPort;
          
      /*
         In case of fast start, the local endpoint need to be ready to
         receive all the media types proposed in the fast connect, before
         the actual call is established.
      */
      if(outgoing)
      {
         if(epCap->startReceiveChannel)
         {
            epCap->startReceiveChannel(call, pLogicalChannel);      
            OOTRACEINFO4("Receive channel of type %s started (%s, %s)\n",
                         (epCap->capType == OO_CAP_TYPE_AUDIO)?"audio":"video",
                          call->callType, call->callToken);
         }
         else{
            OOTRACEERR4("ERROR:No callback registered to start receive %s "
                        "channel (%s, %s)\n", 
                        (epCap->capType == OO_CAP_TYPE_AUDIO)?"audio":"video", 
                        call->callType, call->callToken);
            return OO_FAILED;
         }
      }
   }

   /* State of logical channel. for out going calls, as we are sending setup, 
      state of all channels are proposed, for incoming calls, state is 
      established. */
   if(!outgoing) {
      pLogicalChannel->state = OO_LOGICALCHAN_ESTABLISHED;
   }
   else {
      /* Calling other ep, with SETUP message */
      /* Call is "outgoing */
      pLogicalChannel->state = OO_LOGICALCHAN_PROPOSED;
   }
   
   return OO_OK;
}



int ooMSDTimerExpired(void *data)
{
   ooTimerCallback *cbData = (ooTimerCallback*)data;
   OOH323CallData *call = cbData->call;
   OOTRACEINFO3("MasterSlaveDetermination timeout. (%s, %s)\n", call->callType,
                 call->callToken);
   ASN1MEMFREEPTR(call->pctxt, cbData);
   ooSendMasterSlaveDeterminationRelease(call);
   if(call->callState < OO_CALL_CLEAR)
   {
      call->callState = OO_CALL_CLEAR;
      call->callEndReason = OO_REASON_LOCAL_CLEARED;
   }

   return OO_OK;
}
   
int ooTCSTimerExpired(void *data)
{
   ooTimerCallback *cbData = (ooTimerCallback*)data;
   OOH323CallData *call = cbData->call;
   OOTRACEINFO3("TerminalCapabilityExchange timeout. (%s, %s)\n", 
                 call->callType, call->callToken);
   ASN1MEMFREEPTR(call->pctxt, cbData);
   ooSendTerminalCapabilitySetRelease(call);
   if(call->callState < OO_CALL_CLEAR)
   {
      call->callState = OO_CALL_CLEAR;
      call->callEndReason = OO_REASON_LOCAL_CLEARED;
   }

   return OO_OK;
}

int ooOpenLogicalChannelTimerExpired(void *pdata)
{
   ooTimerCallback *cbData = (ooTimerCallback*)pdata;
   OOH323CallData *call = cbData->call;
   ooLogicalChannel *pChannel = NULL;
   OOTRACEINFO3("OpenLogicalChannelTimer expired. (%s, %s)\n", call->callType,
                 call->callToken);
   pChannel = ooFindLogicalChannelByLogicalChannelNo(call, 
                                               cbData->channelNumber);
   if(pChannel)
      ooSendCloseLogicalChannel(call, pChannel);
   
   if(call->callState < OO_CALL_CLEAR)
   {
      call->callState = OO_CALL_CLEAR;
      call->callEndReason = OO_REASON_LOCAL_CLEARED;
   }
   ASN1MEMFREEPTR(call->pctxt, cbData);
   return OO_OK;
}

int ooCloseLogicalChannelTimerExpired(void *pdata)
{
   ooTimerCallback *cbData = (ooTimerCallback*)pdata;
   OOH323CallData *call = cbData->call;

   OOTRACEINFO3("CloseLogicalChannelTimer expired. (%s, %s)\n", call->callType,
                 call->callToken);

   ooClearLogicalChannel(call, cbData->channelNumber);
   
   if(call->callState < OO_CALL_CLEAR)
   {
      call->callState = OO_CALL_CLEAR;
      call->callEndReason = OO_REASON_LOCAL_CLEARED;
   }
   ASN1MEMFREEPTR(call->pctxt, cbData);
   return OO_OK;
}

int ooRequestChannelCloseTimerExpired(void *pdata)
{
   int ret = 0;
   ooTimerCallback *cbData = (ooTimerCallback*)pdata;
   OOH323CallData *call = cbData->call;

   OOTRACEINFO3("OpenLogicalChannelTimer expired. (%s, %s)\n", call->callType,
                 call->callToken);
  
   ooSendRequestChannelCloseRelease(call, cbData->channelNumber);

   ret = ooClearLogicalChannel(call, cbData->channelNumber);
   if(ret != OO_OK)
   {
      OOTRACEERR4("Error:Failed to clear logical channel %d. (%s, %s)\n",
                   cbData->channelNumber, call->callType, call->callToken);
   } 

   if(call->callState < OO_CALL_CLEAR)
   {
      call->callState = OO_CALL_CLEAR;
      call->callEndReason = OO_REASON_LOCAL_CLEARED;
   }
   ASN1MEMFREEPTR(call->pctxt, cbData);
   return OO_OK;
}

int ooSessionTimerExpired(void *pdata)
{
   int ret = 0;
   ooTimerCallback *cbData = (ooTimerCallback*)pdata;
   OOH323CallData *call = cbData->call;

   OOTRACEINFO3("SessionTimer expired. (%s, %s)\n", call->callType,
                 call->callToken);

   if(call->h245SessionState != OO_H245SESSION_IDLE && 
      call->h245SessionState != OO_H245SESSION_CLOSED &&
      call->h245SessionState != OO_H245SESSION_PAUSED) {

      ret = ooCloseH245Connection(call);
   
      if(ret != OO_OK) {
         OOTRACEERR3("Error:Failed to close H.245 connection (%s, %s)\n",
                     call->callType, call->callToken);
      } 
   }

   memFreePtr(call->pctxt, cbData);

   if(call->callState == OO_CALL_CLEAR_RELEASESENT){
     OOTRACEAST(OOTRCLVLDBGA,"ooSessionTimerExpired upd callState[CLEARED]\n");
      call->callState = OO_CALL_CLEARED;
   }
   
   return OO_OK;
}


int ooGetIpPortFromH245TransportAddress
   (OOH323CallData *call, H245TransportAddress *h245Address, char *ip, 
    int *port)
{
   H245UnicastAddress *unicastAddress = NULL;
   H245UnicastAddress_iPAddress *ipAddress = NULL;

   if(h245Address->t != T_H245TransportAddress_unicastAddress)
   {
      OOTRACEERR3("ERROR:Unsupported H245 address type "
                           "(%s, %s)\n", call->callType, call->callToken);
      return OO_FAILED;
   } 
      
   unicastAddress = h245Address->u.unicastAddress;
   if(unicastAddress->t != T_H245UnicastAddress_iPAddress)
   {
      OOTRACEERR3("ERROR:H245 Address type is not IP"
                   "(%s, %s)\n", call->callType, call->callToken);
      return OO_FAILED;
   }
   ipAddress = unicastAddress->u.iPAddress;

   *port = ipAddress->tsapIdentifier;

   sprintf(ip, "%d.%d.%d.%d", ipAddress->network.data[0], 
                              ipAddress->network.data[1],
                              ipAddress->network.data[2],
                              ipAddress->network.data[3]);
   return OO_OK;
}


int ooPrepareFastStartResponseOLC
   (OOH323CallData *call, H245OpenLogicalChannel *olc, 
    ooH323EpCapability *epCap, OOCTXT*pctxt, int dir)
{
   OOBOOL reverse=FALSE, forward=FALSE;
   H245OpenLogicalChannel_forwardLogicalChannelParameters *flcp=NULL;
   H245OpenLogicalChannel_reverseLogicalChannelParameters *rlcp=NULL;
   H245H2250LogicalChannelParameters *pH2250lcp1=NULL, *pH2250lcp2=NULL;
   H245UnicastAddress *pUnicastAddrs=NULL, *pUniAddrs=NULL;
   H245UnicastAddress_iPAddress *pIpAddrs=NULL, *pUniIpAddrs=NULL;
   unsigned session_id = 0;
   ooLogicalChannel *pLogicalChannel = NULL;
   
   if(dir & OORX)
   {
      OOTRACEDBGA3("ooPrepareFastStartResponseOLC for Receive  Capability "
                   "(%s, %s)\n", call->callType, call->callToken);
      session_id = ooCallGenerateSessionID(call, epCap->capType, "receive");
      pLogicalChannel = ooAddNewLogicalChannel(call, 
                                 olc->forwardLogicalChannelNumber, session_id, 
                                 "receive", epCap);
      forward = TRUE;
   }
   else if(dir & OOTX)
   {
      OOTRACEDBGA3("ooPrepareFastStartResponseOLC for transmit Capability "
                   "(%s, %s)\n", call->callType, call->callToken);
      session_id = ooCallGenerateSessionID(call, epCap->capType, "transmit");
      pLogicalChannel = ooAddNewLogicalChannel(call, 
                                  olc->forwardLogicalChannelNumber, session_id,
                                  "transmit", epCap);
      reverse = TRUE;
   }
   else if(dir & OORXTX)
   {
      OOTRACEDBGA3("ooPrepareFastStartResponseOLC for ReceiveAndTransmit  "
                   "Capability (%s, %s)\n", call->callType, call->callToken);
      reverse = 1;
      forward = 1;
      OOTRACEERR3("Symmetric capability is not supported as of now (%s, %s)\n",
                   call->callType, call->callToken);
      return OO_FAILED;
   }

   if(forward)
   {
      OOTRACEDBGC3("Preparing olc for receive channel. (%s, %s)\n", 
                   call->callType, call->callToken);
      flcp = &(olc->forwardLogicalChannelParameters);

      pH2250lcp1 = flcp->multiplexParameters.u.h2250LogicalChannelParameters;
          

      pH2250lcp1->m.mediaChannelPresent = 1;
      pH2250lcp1->mediaChannel.t = T_H245TransportAddress_unicastAddress;
      pUniAddrs = (H245UnicastAddress*) memAlloc(pctxt, 
                                                   sizeof(H245UnicastAddress));
      pUniIpAddrs = (H245UnicastAddress_iPAddress*) memAlloc(pctxt, 
                                         sizeof(H245UnicastAddress_iPAddress));
      if(!pUniAddrs || !pUniIpAddrs)
      {
         OOTRACEERR3("Error:Memory - ooPrepareFastStartResponseOLC - pUniAddrs"
                     "/pUniIpAddrs (%s, %s)\n", call->callType, 
                     call->callToken);
         return OO_FAILED;
      }

      pH2250lcp1->mediaChannel.u.unicastAddress =  pUniAddrs;
      pUniAddrs->t = T_H245UnicastAddress_iPAddress;
      pUniAddrs->u.iPAddress = pUniIpAddrs;
     
      ooSocketConvertIpToNwAddr(pLogicalChannel->localIP, 
                                                    pUniIpAddrs->network.data);

      pUniIpAddrs->network.numocts = 4;
      pUniIpAddrs->tsapIdentifier = pLogicalChannel->localRtpPort;

      pH2250lcp1->m.mediaControlChannelPresent = 1;
      pH2250lcp1->mediaControlChannel.t = 
                                 T_H245TransportAddress_unicastAddress;
      pUnicastAddrs = (H245UnicastAddress*) memAlloc(pctxt, 
                                                   sizeof(H245UnicastAddress));
      pIpAddrs = (H245UnicastAddress_iPAddress*) memAlloc(pctxt, 
                                         sizeof(H245UnicastAddress_iPAddress));
      if(!pUnicastAddrs || !pIpAddrs)
      {
         OOTRACEERR3("Error:Memory - ooPrepareFastStartResponseOLC - "
                     "pUnicastAddrs/pIpAddrs (%s, %s)\n", call->callType, 
                     call->callToken);
         return OO_FAILED;
      }
      memset(pUnicastAddrs, 0, sizeof(H245UnicastAddress));
      pH2250lcp1->mediaControlChannel.u.unicastAddress =  pUnicastAddrs;
      pUnicastAddrs->t = T_H245UnicastAddress_iPAddress;
      
      pUnicastAddrs->u.iPAddress = pIpAddrs;
     
      ooSocketConvertIpToNwAddr(pLogicalChannel->localIP, 
                                                       pIpAddrs->network.data);

      pIpAddrs->network.numocts = 4;
      pIpAddrs->tsapIdentifier = pLogicalChannel->localRtcpPort;
   }

   if(reverse)
   {
      OOTRACEDBGC3("Building reverse olc. (%s, %s)\n", call->callType, 
                    call->callToken);

      rlcp = &(olc->reverseLogicalChannelParameters);

      pH2250lcp2 = rlcp->multiplexParameters.u.h2250LogicalChannelParameters;
      pH2250lcp2->m.mediaChannelPresent = 0;
      memset(&pH2250lcp2->mediaChannel, 0, sizeof(H245TransportAddress));

      pH2250lcp2->m.mediaControlChannelPresent = 1;
      pH2250lcp2->mediaControlChannel.t = 
                                 T_H245TransportAddress_unicastAddress;
      pUniAddrs = (H245UnicastAddress*) memAlloc(pctxt, 
                                                   sizeof(H245UnicastAddress));
      pUniIpAddrs = (H245UnicastAddress_iPAddress*) memAlloc(pctxt, 
                                         sizeof(H245UnicastAddress_iPAddress));
      if(!pUniAddrs || !pUniIpAddrs)
      {
         OOTRACEERR3("Error:Memory - ooPrepareFastStartResponseOLC - "
                    "pUniAddrs/pUniIpAddrs (%s, %s)\n", call->callType, 
                     call->callToken);
         return OO_FAILED;
      }

      pH2250lcp2->mediaControlChannel.u.unicastAddress =  pUniAddrs;
      
      pUniAddrs->t = T_H245UnicastAddress_iPAddress;

      pUniAddrs->u.iPAddress = pUniIpAddrs; 

      ooSocketConvertIpToNwAddr(pLogicalChannel->localIP, 
                                                    pUniIpAddrs->network.data);
      pUniIpAddrs->network.numocts = 4;
      pUniIpAddrs->tsapIdentifier = pLogicalChannel->localRtcpPort;
          
   }

   pLogicalChannel->state = OO_LOGICALCHAN_ESTABLISHED;

   return OO_OK;
}


void ooTraceH245Msg(OOH323CallData *call, H245Message *msgS , H245Message *msgR )
{
  if( msgS )
  {
     switch ( msgS->msgType )
   {
     // MSD
    case OOMasterSlaveDetermination:
       OOTRACEAST(OOTRCLVLDBGA,"[H323/H245] Send --> MSD ( Master Slave Determ. )"
                  " (%s, %s)\n", 
                  call->callType, call->callToken);
       break ;

     case OOMasterSlaveAck:
       OOTRACEAST(OOTRCLVLDBGA,"[H323/H245] Send --> MSDAck (Master Slave Determ. Ack )"
                  " (%s, %s)\n", 
                  call->callType, call->callToken);
       break ;

     case OOMasterSlaveReject:
       OOTRACEAST(OOTRCLVLDBGA,"[H323/H245] Send --> MSDReject  (Master Slave Determ. Reject )"
                  " (%s, %s)\n", 
                  call->callType, call->callToken);
       break ;

     case OOMasterSlaveRelease:
       OOTRACEAST(OOTRCLVLDBGA,"[H323/H245] Send --> MSR ( Master Slave Release )"
                  " (%s, %s)\n", 
                  call->callType, call->callToken);
       break ;

       // TCS
    case OOTerminalCapabilitySet:
       OOTRACEAST(OOTRCLVLDBGA,"[H323/H245] Send --> TCS ( Terminal Capability Set ) "
                  " (%s, %s)\n", 
                  call->callType, call->callToken);
       break ;

     case OOTerminalCapabilitySetReject:
       OOTRACEAST(OOTRCLVLDBGA,"[H323/H245] Send --> TCSReject ( Terminal Capability Set Reject ) "
                  " (%s, %s)\n", 
                  call->callType, call->callToken);
       break ;

     case OOTerminalCapabilitySetAck:
       OOTRACEAST(OOTRCLVLDBGA,"[H323/H245] Send --> TCSAck ( Terminal Capability Set Ack ) "
                  "  (%s, %s)\n", 
                  call->callType, call->callToken);
       break ;
     case OOTerminalCapabilitySetRelease:
       OOTRACEAST(OOTRCLVLDBGA,"[H323/H245] Send --> TCSRelease ( Terminal Capability Set Release ) "
                  "  (%s, %s)\n", 
                  call->callType, call->callToken);
       break ;

       // OLC
     case OOOpenLogicalChannel:
       OOTRACEAST(OOTRCLVLDBGA,"[H323/H245] Send --> OLC ( Open Logical Channel ) "
                  " (%s, %s)\n", 
                  call->callType, call->callToken);
       break ;
     case OOOpenLogicalChannelAck:
       OOTRACEAST(OOTRCLVLDBGA,"[H323/H245] Send --> OLCAck ( Open Logical Channel Ack ) "
                  " (%s, %s)\n", 
                  call->callType, call->callToken);
       break ;
     case OOOpenLogicalChannelReject:
       OOTRACEAST(OOTRCLVLDBGA,"[H323/H245] Send --> OLCReject ( Open Logical Channel Reject )"
                  " (%s, %s)\n", 
                  call->callType, call->callToken);
       break ;
     case OOOpenLogicalChannelRelease:
       OOTRACEAST(OOTRCLVLDBGA,"[H323/H245] Send --> OLCReject ( Open Logical Channel Release )"
                  " (%s, %s)\n", 
                  call->callType, call->callToken);
       break ;
     case OOOpenLogicalChannelConfirm:
       OOTRACEAST(OOTRCLVLDBGA,"[H323/H245] Send --> OLCConfirm ( Open Logical Channel Confirm )"
                  " (%s, %s)\n", 
                  call->callType, call->callToken);
       break ;

       // CLC
     case OOCloseLogicalChannel:
       OOTRACEAST(OOTRCLVLDBGA,"[H323/H245] Send --> CLC ( Close Logical Chanel )"
                  "  (%s, %s)\n", 
                  call->callType, call->callToken);
       break ;

     case OOCloseLogicalChannelAck:
       OOTRACEAST(OOTRCLVLDBGA,"[H323/H245] Send --> CLCAck ( Close Logical Chanel Ack. )"
                  "  (%s, %s)\n", 
                  call->callType, call->callToken);
       break ;

       // RCC
     case OORequestChannelClose:
       OOTRACEAST(OOTRCLVLDBGA,"[H323/H245] Send --> RCC ( Request Channel Close )"
                  "  (%s, %s)\n", 
                  call->callType, call->callToken);
       break ;

     case OORequestChannelCloseRelease:
       OOTRACEAST(OOTRCLVLDBGA,"[H323/H245] Send --> RCCRelease (Request Channel Close Release ) "
                  "  (%s, %s)\n", 
                  call->callType, call->callToken);
       break ;

     case OORequestChannelCloseAck:
       OOTRACEAST(OOTRCLVLDBGA,"[H323/H245] Send -->  RCCAck ( Request Channel Close Ack.)"
                  "  (%s, %s)\n", 
                  call->callType, call->callToken);
       break ;

     case OORequestChannelCloseReject:
       OOTRACEAST(OOTRCLVLDBGA,"[H323/H245] Send -->  RCCReject ( Request Channel Close Reject.)"
                  "  (%s, %s)\n", 
                  call->callType, call->callToken);
       break ;

     case OOUserInputIndication:
       OOTRACEAST(OOTRCLVLDBGA,"[H323/H245] Send --> UII ( User Input Indication) "
                  " (%s, %s)\n", 
                  call->callType, call->callToken);
       break ;

     case OOEndSessionCommand:
       OOTRACEAST(OOTRCLVLDBGA,"[H323/H245] Send --> ESC ( End Session Command )"
                  "  (%s, %s)\n", 
                  call->callType, call->callToken);
       break ;

     case OOflowControlCommand:
       OOTRACEAST(OOTRCLVLDBGA,"[H323/H245] Send --> FCC ( flow Control Command )"
                  "  (%s, %s)\n", 
                  call->callType, call->callToken);
       break ;

     case OOMiscellaneousCommand:
       OOTRACEAST(OOTRCLVLDBGA,"[H323/H245] Send --> MC (  Miscellaneous Command )"
                  "  (%s, %s)\n", 
                  call->callType, call->callToken);
       break ;

     case OORoundTripDelayRespons:
       OOTRACEAST(OOTRCLVLDBGA,"[H323/H245] Send --> RTDR Round Trip Delay Respons"
                  "  (%s, %s)\n", 
                  call->callType, call->callToken);
       break ;
     case OOMiscellaneousIndication:
       OOTRACEAST(OOTRCLVLDBGA,"[H323/H245] Send --> MI Miscellaneous Indication"
                  "  (%s, %s)\n", 
                  call->callType, call->callToken);
       break ;
     case OOConferenceRequestRespons:
       OOTRACEAST(OOTRCLVLDBGA,"[H323/H245] Send -->  Conference Request Respons"
                  "  (%s, %s)\n", 
                  call->callType, call->callToken);
       break ;
     default :
       OOTRACEAST(OOTRCLVLDBGA,"[H323/H245] Unknow Send %d (%s, %s)\n", 
                  msgS->msgType , call->callType, call->callToken);
       break ; 
       
   }
  }
  
  if ( msgR )
  {
    H245RequestMessage    *request         = msgR->h245Msg.u.request;
    H245ResponseMessage   *response        = msgR->h245Msg.u.response;
    H245CommandMessage    *command         = msgR->h245Msg.u.command;
    H245IndicationMessage *indication      = msgR->h245Msg.u.indication;
    int                   SwitchType       = request->t ; 
    int                   Status           = TRUE ;
    char                  traces[PATH_MAX] = { 0 };
    char                  head[PATH_MAX]   = { 0 };
    switch(msgR->h245Msg.t)
    {
      case T_H245MultimediaSystemControlMessage_request:
        snprintf( head , PATH_MAX , "Request " );
        switch (request->t )
        {
          case T_H245RequestMessage_nonStandard:
            snprintf( traces , PATH_MAX , "[H323/H245] RECV <--  Non standart " );
            break ;

          case T_H245RequestMessage_masterSlaveDetermination:
            snprintf( traces , PATH_MAX , "[H323/H245] RECV <--  MSD ( Master Slave Determ. )" );
            break ;

          case T_H245RequestMessage_terminalCapabilitySet:
            snprintf( traces , PATH_MAX , "[H323/H245] RECV <--  TCS ( Terminal Capability Set )" );
            break ;

          case T_H245RequestMessage_openLogicalChannel:
            snprintf( traces , PATH_MAX , "[H323/H245] RECV <-- OLC ( Open Logical Channel ) " );
            break ;

          case T_H245RequestMessage_closeLogicalChannel:
            snprintf( traces , PATH_MAX , "[H323/H245] RECV <-- CLC ( Close Logical Chanel ) " );
            break ;

          case T_H245RequestMessage_requestChannelClose:
            snprintf( traces , PATH_MAX , "[H323/H245] RECV <-- RCC ( Request Channel Close ) " );
            break ;

          case T_H245RequestMessage_multiplexEntrySend:
            snprintf( traces , PATH_MAX , "[H323/H245] RECV <-- MESend ( Multiplex. Entry Send ) " );
            break ;

          case T_H245RequestMessage_requestMultiplexEntry:
            snprintf( traces , PATH_MAX , "[H323/H245] RECV <-- RME ( Request Multiplex. Entry ) " );
            break ;

          case T_H245RequestMessage_requestMode:
            snprintf( traces , PATH_MAX , "[H323/H245] RECV <-- RMode " );
            break ;

          case T_H245RequestMessage_roundTripDelayRequest:
            snprintf( traces , PATH_MAX , "[H323/H245] RECV <-- Round Trip Delay Request  " );
            break ;

          case T_H245RequestMessage_maintenanceLoopRequest:
            snprintf( traces , PATH_MAX , "[H323/H245] RECV <-- Maintenance " );
            break ;

          case T_H245RequestMessage_communicationModeRequest:
            snprintf( traces , PATH_MAX , "[H323/H245] RECV <-- Communication Mode" );
            break ;

          case T_H245RequestMessage_conferenceRequest:
            snprintf( traces , PATH_MAX , "[H323/H245] RECV <-- Conference " );
            break ;

          case T_H245RequestMessage_multilinkRequest:
            snprintf( traces , PATH_MAX , "[H323/H245] RECV <-- Multilink " );
            break ;

          case T_H245RequestMessage_logicalChannelRateRequest:
            snprintf( traces , PATH_MAX , "[H323/H245] RECV <-- Channel Rate " );
            break ;

          case T_H245RequestMessage_extElem1:
            snprintf( traces , PATH_MAX , "[H323/H245] RECV <-- extended element " );
            break ;

          default:
            snprintf( traces , PATH_MAX , "[H323/H245] RECV <-- unknown %d",request->t  );
            break ;
        }
        break ;

      case T_H245MultimediaSystemControlMessage_response:
        snprintf( head , PATH_MAX , "Respons " );
        switch ( response->t )
        {
          case T_H245ResponseMessage_nonStandard:
            snprintf( traces , PATH_MAX , "[H323/H245] RECV <--  Non standart " );
            break ;

          case T_H245ResponseMessage_masterSlaveDeterminationAck:
            snprintf( traces , PATH_MAX , "[H323/H245] RECV <--  MSDAck" );
            break ;

          case T_H245ResponseMessage_masterSlaveDeterminationReject:
            snprintf( traces , PATH_MAX , "[H323/H245] RECV <--  MSDReject " );
            break ;

          case T_H245ResponseMessage_terminalCapabilitySetAck:
            snprintf( traces , PATH_MAX , "[H323/H245] RECV <-- TCSAsck " );
            break ;

          case T_H245ResponseMessage_terminalCapabilitySetReject:
            snprintf( traces , PATH_MAX , "[H323/H245] RECV <-- TCSReject " );
            break ;

          case T_H245ResponseMessage_openLogicalChannelAck:
            snprintf( traces , PATH_MAX , "[H323/H245] RECV <-- OLCAck " );
            break ;

          case T_H245ResponseMessage_openLogicalChannelReject:
            snprintf( traces , PATH_MAX , "[H323/H245] RECV <-- OLCReject " );
            break ;

          case T_H245ResponseMessage_closeLogicalChannelAck:
            snprintf( traces , PATH_MAX , "[H323/H245] RECV <-- CLCAck " );
            break ;

          case T_H245ResponseMessage_requestChannelCloseAck:
            snprintf( traces , PATH_MAX , "[H323/H245] RECV <--  RCCRequest" );
            break ;

          case T_H245ResponseMessage_requestChannelCloseReject:
            snprintf( traces , PATH_MAX , "[H323/H245] RECV <--  RCCReject" );
            break ;

          case T_H245ResponseMessage_multiplexEntrySendAck:
            snprintf( traces , PATH_MAX , "[H323/H245] RECV <--  MESAck " );
            break ;

          case T_H245ResponseMessage_multiplexEntrySendReject:
            snprintf( traces , PATH_MAX , "[H323/H245] RECV <--  MESReject " );
            break ;

          case T_H245ResponseMessage_requestMultiplexEntryAck:
            snprintf( traces , PATH_MAX , "[H323/H245] RECV <-- RMEAck  " );
            break ;

          case T_H245ResponseMessage_requestMultiplexEntryReject:
            snprintf( traces , PATH_MAX , "[H323/H245] RECV <--  RMEReject" );
            break ;

          case T_H245ResponseMessage_requestModeAck:
            snprintf( traces , PATH_MAX , "[H323/H245] RECV <-- RMAck " );
            break ;

          case T_H245ResponseMessage_requestModeReject:
            snprintf( traces , PATH_MAX , "[H323/H245] RECV <-- RMReject " );
            break ;

          case T_H245ResponseMessage_roundTripDelayResponse:
            snprintf( traces , PATH_MAX , "[H323/H245] RECV <-- Delay " );
            break ;

          case T_H245ResponseMessage_maintenanceLoopAck:
            snprintf( traces , PATH_MAX , "[H323/H245] RECV <-- Maintenance Ack " );
            break ;

          case T_H245ResponseMessage_maintenanceLoopReject:
            snprintf( traces , PATH_MAX , "[H323/H245] RECV <-- Maintenance Reject " );
            break ;

          case T_H245ResponseMessage_communicationModeResponse:
            snprintf( traces , PATH_MAX , "[H323/H245] RECV <-- Comunication Mode " );
            break ;

          case T_H245ResponseMessage_conferenceResponse:
            snprintf( traces , PATH_MAX , "[H323/H245] RECV <-- Conference Respons " );
            break ;

          case T_H245ResponseMessage_multilinkResponse:
            snprintf( traces , PATH_MAX , "[H323/H245] RECV <-- Multilink Respons " );
            break ;

          case T_H245ResponseMessage_logicalChannelRateAcknowledge:
            snprintf( traces , PATH_MAX , "[H323/H245] RECV <-- Chanel Rate Ack " );
            break ;

          case T_H245ResponseMessage_logicalChannelRateReject:
            snprintf( traces , PATH_MAX , "[H323/H245] RECV <-- Chanel Rate Reject " );
            break ;

          case T_H245ResponseMessage_extElem1:
            snprintf( traces , PATH_MAX , "[H323/H245] RECV <-- extended element  " );
            break ;

          default:
            snprintf( traces , PATH_MAX , "[H323/H245] RECV <-- unknown %d ",response->t );
            break ;

        }
        break ;

      case T_H245MultimediaSystemControlMessage_command:
        snprintf( head , PATH_MAX , " Command " );
        switch ( command->t )
        {
          case T_H245CommandMessage_nonStandard:
            snprintf( traces , PATH_MAX , "[H323/H245] RECV <--  Non standart " );
            break ;

          case T_H245CommandMessage_maintenanceLoopOffCommand:
            snprintf( traces , PATH_MAX , "[H323/H245] RECV <-- Maintenance  " );
            break ;

          case T_H245CommandMessage_sendTerminalCapabilitySet:
            snprintf( traces , PATH_MAX , "[H323/H245] RECV <-- TCS " );
            break ;

          case T_H245CommandMessage_encryptionCommand:
            snprintf( traces , PATH_MAX , "[H323/H245] RECV <-- Encryption " );
            break ;

          case T_H245CommandMessage_flowControlCommand:
            snprintf( traces , PATH_MAX , "[H323/H245] RECV <-- Flow Control " );
            break ;

          case T_H245CommandMessage_endSessionCommand:
            snprintf( traces , PATH_MAX , "[H323/H245] RECV <-- ESC " );
            break ;

          case T_H245CommandMessage_miscellaneousCommand:
            snprintf( traces , PATH_MAX , "[H323/H245] RECV <-- miscellaneous " );
            break ;

          case T_H245CommandMessage_communicationModeCommand:
            snprintf( traces , PATH_MAX , "[H323/H245] RECV <-- Communication " );
            break ;

          case T_H245CommandMessage_conferenceCommand:
            snprintf( traces , PATH_MAX , "[H323/H245] RECV <-- Conference " );
            break ;

          case T_H245CommandMessage_h223MultiplexReconfiguration:
            snprintf( traces , PATH_MAX , "[H323/H245] RECV <-- MultiplexReconfiguration " );
            break ;

          case T_H245CommandMessage_newATMVCCommand:
            snprintf( traces , PATH_MAX , "[H323/H245] RECV <-- ATMV " );
            break ;

          case T_H245CommandMessage_mobileMultilinkReconfigurationCommand:
            snprintf( traces , PATH_MAX , "[H323/H245] RECV <--  mobileMultilinkReconfiguration" );
            break ;

          case T_H245CommandMessage_extElem1:
            snprintf( traces , PATH_MAX , "[H323/H245] RECV <--  extended element" );
            break ;
            
          default:
            snprintf( traces , PATH_MAX , "[H323/H245] RECV <--  unknown %d ",command->t );
            break ;
        }
        break ;

      case T_H245MultimediaSystemControlMessage_indication:
        snprintf( head , PATH_MAX , " Indication " );
        switch (indication->t)
        {
          case T_H245IndicationMessage_nonStandard:
            snprintf( traces , PATH_MAX , "[H323/H245] RECV <--  Non standart " );
            break ;

          case T_H245IndicationMessage_functionNotUnderstood:
            snprintf( traces , PATH_MAX , "[H323/H245] RECV <-- function Not Understood " );
            break ;

          case T_H245IndicationMessage_masterSlaveDeterminationRelease:
            snprintf( traces , PATH_MAX , "[H323/H245] RECV <-- MSDRelease " );
            break ;

          case T_H245IndicationMessage_terminalCapabilitySetRelease:
            snprintf( traces , PATH_MAX , "[H323/H245] RECV <-- TCSRelease " );
            break ;

          case T_H245IndicationMessage_openLogicalChannelConfirm:
            snprintf( traces , PATH_MAX , "[H323/H245] RECV <-- OLCConfirm " );
            break ;

          case T_H245IndicationMessage_requestChannelCloseRelease:
            snprintf( traces , PATH_MAX , "[H323/H245] RECV <-- RCCRelease " );
            break ;

          case T_H245IndicationMessage_multiplexEntrySendRelease:
            snprintf( traces , PATH_MAX , "[H323/H245] RECV <-- MESRelease " );
            break ;

          case T_H245IndicationMessage_requestMultiplexEntryRelease:
            snprintf( traces , PATH_MAX , "[H323/H245] RECV <-- RMERelease " );
            break ;

          case T_H245IndicationMessage_requestModeRelease:
            snprintf( traces , PATH_MAX , "[H323/H245] RECV <-- RMRelease " );
            break ;

          case T_H245IndicationMessage_miscellaneousIndication:
            snprintf( traces , PATH_MAX , "[H323/H245] RECV <-- Misc. Indication " );
            break ;

          case T_H245IndicationMessage_jitterIndication:
            snprintf( traces , PATH_MAX , "[H323/H245] RECV <-- Jitter Indication " );
            break ;

          case T_H245IndicationMessage_h223SkewIndication:
            snprintf( traces , PATH_MAX , "[H323/H245] RECV <-- h223 Skew Indication " );
            break ;

          case T_H245IndicationMessage_newATMVCIndication:
            snprintf( traces , PATH_MAX , "[H323/H245] RECV <-- New ATm Indication " );
            break ;

          case T_H245IndicationMessage_userInput:
            snprintf( traces , PATH_MAX , "[H323/H245] RECV <-- User Input " );
            break ;

          case T_H245IndicationMessage_h2250MaximumSkewIndication:
            snprintf( traces , PATH_MAX , "[H323/H245] RECV <-- h2250 Max Skew Indication " );
            break ;

          case T_H245IndicationMessage_mcLocationIndication:
            snprintf( traces , PATH_MAX , "[H323/H245] RECV <-- mc Location Indication " );
            break ;

          case T_H245IndicationMessage_conferenceIndication:
            snprintf( traces , PATH_MAX , "[H323/H245] RECV <-- Conference Indiciation " );
            break ;

          case T_H245IndicationMessage_vendorIdentification:
            snprintf( traces , PATH_MAX , "[H323/H245] RECV <-- Vendor ID " );
            break ;

          case T_H245IndicationMessage_functionNotSupported:
            snprintf( traces , PATH_MAX , "[H323/H245] RECV <-- function Not Supported  " );
            break ;

          case T_H245IndicationMessage_multilinkIndication:
            snprintf( traces , PATH_MAX , "[H323/H245] RECV <-- MultiLink Indication " );
            break ;

          case T_H245IndicationMessage_logicalChannelRateRelease:
            snprintf( traces , PATH_MAX , "[H323/H245] RECV <-- LCRRelease " );
            break ;

          case T_H245IndicationMessage_flowControlIndication:
            snprintf( traces , PATH_MAX , "[H323/H245] RECV <-- Flow Control Indication " );
            break ;

          case T_H245IndicationMessage_mobileMultilinkReconfigurationIndication:
            snprintf( traces , PATH_MAX , "[H323/H245] RECV <-- Mobile Multi Link ReConf. " );
            break ;

          case T_H245IndicationMessage_extElem1:
            snprintf( traces , PATH_MAX , "[H323/H245] RECV <-- extended element " );
            break ;

          default:
            snprintf( traces , PATH_MAX , "[H323/H245] RECV <-- unknown %d ",indication->t );
            break ;
        }
        break ;
      case T_H245MultimediaSystemControlMessage_extElem1:
        OOTRACEAST(OOTRCLVLERR,"[H323/H245] RECV <-- MSC ( Multimedia System Control ) "
                   "not implemented (%s, %s)\n", 
                   call->callType, call->callToken);
        Status = FALSE  ;
        break ;
      default :
        OOTRACEAST(OOTRCLVLERR,"[H323/H245] Unknow RECV %d (%s, %s)\n", 
                   msgR->h245Msg.t , call->callType, call->callToken);
        Status = FALSE ;
        break ; 
    }
    if ( Status ) 
    {
      OOTRACEAST(OOTRCLVLDBGA,"%s %s (%s, %s)\n", 
                 traces,head,call->callType, call->callToken);
    }
  }
}

int SetLocalCapChoice( OOH323CallData* call, ooH323EpCapability *epCap)
{
  if ( epCap->cap < OO_CAP_VIDEO_BASE )
  {
    OOTRACEAST(OOTRCLVLDBGA,"SetLocalCapChoice localAudioChoice [%s] (%s, %s) \n",
               ooGetCapTypeText(epCap->cap),call->callType, call->callToken );
    call->localAudioChoice =  (ooH323EpCapability*)memAlloc(call->pctxt, 
                                                            sizeof(ooH323EpCapability));
    memcpy(call->localAudioChoice , epCap , sizeof(ooH323EpCapability));
    call->localAudioChoice->next = NULL ;
  }
  else
  {
    if ( epCap->cap > OO_CAP_VIDEO_BASE &&  epCap->cap <= OO_EXTELEMVIDEO )
    {
      OOTRACEAST(OOTRCLVLDBGA,"SetLocalCapChoice localVideoChoice [%s] (%s, %s) \n",
                 ooGetCapTypeText(epCap->cap),call->callType, call->callToken );
      call->localVideoChoice =  (ooH323EpCapability*)memAlloc(call->pctxt, 
                                                              sizeof(ooH323EpCapability));
      memcpy(call->localVideoChoice , epCap , sizeof(ooH323EpCapability));
      call->localVideoChoice->next = NULL ;       
    }
    else
    {
      OOTRACEERR4("SetLocalCapChoice : Master mode but unknown codec %d "
                  "channel (%s, %s)\n",epCap->cap, 
                  call->callType, call->callToken);
      return OO_FAILED;
    }
  }
}

int SetRemoteCapChoice( OOH323CallData* call, ooH323EpCapability *epCap)
{
  int Status = OO_OK ;
  if ( epCap->cap < OO_CAP_VIDEO_BASE )
  {
    OOTRACEAST(OOTRCLVLDBGA,"SetRemoteCapChoice RemoteAudioChoice [%s] (%s, %s) \n",
               ooGetCapTypeText(epCap->cap),call->callType, call->callToken );
    call->RemoteAudioChoice =  (ooH323EpCapability*)memAlloc(call->pctxt, 
                                                            sizeof(ooH323EpCapability));
    memcpy(call->RemoteAudioChoice , epCap , sizeof(ooH323EpCapability));
  }
  else
  {
    if ( epCap->cap > OO_CAP_VIDEO_BASE &&  epCap->cap <= OO_EXTELEMVIDEO )
    {
      OOTRACEAST(OOTRCLVLDBGA,"SetRemoteCapChoice RemoteVideoChoice [%s] (%s, %s) \n",
                 ooGetCapTypeText(epCap->cap),call->callType, call->callToken );
      call->RemoteVideoChoice =  (ooH323EpCapability*)memAlloc(call->pctxt, 
                                                              sizeof(ooH323EpCapability));
      memcpy(call->RemoteVideoChoice , epCap , sizeof(ooH323EpCapability));
      call->RemoteVideoChoice->next = NULL ;       
    }
    else
    {
      OOTRACEERR4("SetRemoteCapChoice : Slave mode but unknown codec %d "
                  "channel (%s, %s)\n",epCap->cap, 
                  call->callType, call->callToken);
      Status == OO_FAILED ;
    }
  }
}
