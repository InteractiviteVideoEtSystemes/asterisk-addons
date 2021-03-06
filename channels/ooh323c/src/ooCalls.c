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

#include "ootrace.h"
#include "ootypes.h"
#include "ooCalls.h"
#include "ooUtils.h"
#include "ooports.h"
#include "oochannels.h"
#include "ooh245.h"
#include "ooCapability.h"
#include "ooGkClient.h"
#include "ooh323ep.h"
#include "ooCalls.h"

/** Global endpoint structure */
extern OOH323EndPoint gH323ep;

#define IVeS_isallowed(c) (isalnum(c)||(c=='_')||(c=='@')||(c=='.')||(c=='-'))

void IVeS_strcpy(char *p_dst, const char *p_src)
{
  char *s;
  char *d;

  s = (char *)p_src;
  d = p_dst;
  while ((*s) != '\0')
  {
    if (IVeS_isallowed(*s))
    {
      *d = *s;
      d++;
    }
    s++;
  }
  *d = '\0';
}

OOH323CallData* ooCreateCall(char* type, char*callToken)
{
   OOH323CallData *call=NULL;
   OOCTXT *pctxt=NULL;

   OOTRACEAST(OOTRCLVLDBGA,"[H323]  Create new call :%s\n",type);   

   pctxt = newContext();
   if(!pctxt)
   {
      OOTRACEERR1("ERROR:Failed to create OOCTXT for new call\n");
      return NULL;
   }
   call = (OOH323CallData*)memAlloc(pctxt, sizeof(OOH323CallData));
   if(!call)
   {
      OOTRACEERR1("ERROR:Memory - ooCreateCall - call\n");
      return NULL;
   } 
   /*   memset(call, 0, sizeof(OOH323CallData));*/
   call->pctxt = pctxt;
   call->callMode = gH323ep.callMode;
   sprintf(call->callToken, "%s", callToken);
   sprintf(call->callType, "%s", type);
   call->callReference = 0;
   if(gH323ep.callerid) {
     strncpy(call->ourCallerId, gH323ep.callerid, sizeof(call->ourCallerId)-1);
     call->ourCallerId[sizeof(call->ourCallerId)-1] = '\0';
   }
   else {
      call->ourCallerId[0] = '\0';
   }
   
   memset(&call->callIdentifier, 0, sizeof(H225CallIdentifier));
   memset(&call->confIdentifier, 0, sizeof(H225ConferenceIdentifier));
   memset(&call->chargVectorID,0 , OO_PCV_CID_SIZE );
   call->flags = 0;
   if (OO_TESTFLAG(gH323ep.flags, OO_M_TUNNELING))
      OO_SETFLAG (call->flags, OO_M_TUNNELING);

   if(gH323ep.gkClient)
   {
      if(OO_TESTFLAG(gH323ep.flags, OO_M_GKROUTED))
      {
         OO_SETFLAG(call->flags, OO_M_GKROUTED);
      }
   }

   if (OO_TESTFLAG(gH323ep.flags, OO_M_FASTSTART))
      OO_SETFLAG (call->flags, OO_M_FASTSTART);

   if (OO_TESTFLAG(gH323ep.flags, OO_M_MEDIAWAITFORCONN))
      OO_SETFLAG (call->flags, OO_M_MEDIAWAITFORCONN);
   
   call->callState = OO_CALL_CREATED;
   call->callEndReason = OO_REASON_UNKNOWN;
   call->pCallFwdData = NULL;

   if(!strcmp(call->callType, "incoming"))
   {
      call->callingPartyNumber = NULL;
   }
   else{      
      if(ooUtilsIsStrEmpty(gH323ep.callingPartyNumber))
      {
         call->callingPartyNumber = NULL;
      }
      else{
         call->callingPartyNumber = (char*) memAlloc(call->pctxt, 
                                         strlen(gH323ep.callingPartyNumber)+1);
         if(call->callingPartyNumber)
         {
            strcpy(call->callingPartyNumber, gH323ep.callingPartyNumber);
         }
         else{
            OOTRACEERR3("Error:Memory - ooCreateCall - callingPartyNumber"
                        ".(%s, %s)\n", call->callType, call->callToken);
            freeContext(pctxt);
            return NULL;
         }
      }
   }

   call->calledPartyNumber = NULL;
   call->h245ConnectionAttempts = 0;
   call->h245SessionState = OO_H245SESSION_IDLE;
   call->dtmfmode = gH323ep.dtmfmode;
   call->mediaInfo = NULL;
   strcpy(call->localIP, gH323ep.signallingIP);
   call->pH225Channel = NULL;
   call->pH245Channel = NULL;
   call->h245listener = NULL;
   call->h245listenport = NULL;
   call->remoteIP[0] = '\0';
   call->remotePort = 0;
   call->remoteH245Port = 0;
   call->remoteDisplayName = NULL;
   call->remoteAliases = NULL;
   call->ourAliases = NULL;
   call->masterSlaveState = OO_MasterSlave_Idle;
   call->statusDeterminationNumber = 0;
   call->localTermCapState = OO_LocalTermCapExchange_Idle;
   call->remoteTermCapState = OO_RemoteTermCapExchange_Idle; 
   call->ourCaps = NULL;
   call->remoteCaps = NULL;
   call->jointCaps = NULL;
   call->localAudioChoice = NULL;
   call->localVideoChoice = NULL;
   dListInit(&call->remoteFastStartOLCs);
   call->remoteTermCapSeqNo =0;
   call->localTermCapSeqNo = 0;
   memcpy(&call->capPrefs, &gH323ep.capPrefs, sizeof(OOCapPrefs));    
   call->logicalChans = NULL;
   // call->haveSendOLC = FALSE ;
   call->noOfLogicalChannels = 0;
   call->logicalChanNoBase = 1;
   call->logicalChanNoMax = 100;
   call->logicalChanNoCur = 1;
   call->nextSessionID = 4; /* 1,2,3 are reserved for audio, video and data */
   dListInit(&call->timerList);
   call->msdRetries = 0;
   call->pFastStartRes = NULL;
   call->usrData = NULL;
   OOTRACEINFO3("Created a new call (%s, %s)\n", call->callType, 
                 call->callToken);
   /* Add new call to calllist */
   ooAddCallToList (call);
   if(gH323ep.h323Callbacks.onNewCallCreated)
     gH323ep.h323Callbacks.onNewCallCreated(call);
   return call;
}

int ooAddCallToList(OOH323CallData *call)
{
   if(!gH323ep.callList)
   {
      OOTRACEAST(OOTRCLVLDBGA,"ooAddCallToList : add first call [0x%X]\n",call);
      gH323ep.callList = call;
      call->next = NULL;
      call->prev = NULL;
   }
   else{
      OOTRACEAST(OOTRCLVLDBGA,"ooAddCallToList : insert call [0x%X]\n",call);
      call->next = gH323ep.callList;
      call->prev = NULL;
      gH323ep.callList->prev = call;
      gH323ep.callList = call;
   }
   return OO_OK;
}


int ooEndCall(OOH323CallData *call)
{
   OOTRACEDBGA4("In ooEndCall call state is - %s (%s, %s)\n", 
                 ooGetCallStateText(call->callState), call->callType, 
                 call->callToken);

   if(call->callState == OO_CALL_CLEARED)
   {
      ooCleanCall(call); 
      return OO_OK;
   }

   if(call->logicalChans)
   {
      OOTRACEINFO3("Clearing all logical channels. (%s, %s)\n", call->callType,
                    call->callToken);
      ooClearAllLogicalChannels(call);
   }

   if(!OO_TESTFLAG(call->flags, OO_M_ENDSESSION_BUILT))
   {
      if(call->h245SessionState == OO_H245SESSION_ACTIVE ||
         call->h245SessionState == OO_H245SESSION_ENDRECVD)
      {
         ooSendEndSessionCommand(call);
         OO_SETFLAG(call->flags, OO_M_ENDSESSION_BUILT);
      }
   }


   if(!call->pH225Channel || call->pH225Channel->sock ==0)
   {
     OOTRACEAST(OOTRCLVLDBGA,"ooEndCall upd callState[CLEARED]\n");
     call->callState = OO_CALL_CLEARED;
   }
   else{
      if(!OO_TESTFLAG(call->flags, OO_M_RELEASE_BUILT))   
      {
         if(call->callState == OO_CALL_CLEAR || 
            call->callState == OO_CALL_CLEAR_RELEASERECVD)
         {
            ooSendReleaseComplete(call);
            OO_SETFLAG(call->flags, OO_M_RELEASE_BUILT);
         }
      }
   }
      
   return OO_OK;
}



int ooRemoveCallFromList (OOH323CallData *call)
{
   if(!call)
      return OO_OK;

   OOTRACEAST(OOTRCLVLDBGA,"Remove call 0x%X \n",call);
   if(call == gH323ep.callList)
   {
      if(!call->next)
         gH323ep.callList = NULL;
      else{
         call->next->prev = NULL;
         gH323ep.callList = call->next;
      }
   }
   else{
      call->prev->next = call->next;
      if(call->next)
         call->next->prev = call->prev;
   }
   return OO_OK;
}

int ooCleanCall(OOH323CallData *call)
{
   OOCTXT *pctxt;

   OOTRACEAST(OOTRCLVLDBGA,"Cleaning Call (%s, %s)- reason:%s\n", 
                 call->callType, call->callToken, 
                 ooGetReasonCodeText (call->callEndReason));

   /* First clean all the logical channels, if not already cleaned. */
   if(call->logicalChans)
      ooClearAllLogicalChannels(call);
   
   /* Close H.245 connection, if not already closed */
   if(call->h245SessionState != OO_H245SESSION_CLOSED)
      ooCloseH245Connection(call);
   else{
      if(call->pH245Channel && call->pH245Channel->outQueue.count > 0)
      {
         dListFreeAll(call->pctxt, &(call->pH245Channel->outQueue));
         memFreePtr(call->pctxt, call->pH245Channel);
      }
   }

   /* Close H.245 listener, if not already closed */
   if(call->h245listener)
   {
      ooCloseH245Listener(call);
   }
   
   /* Close H225 connection, if not already closed. */
   if (0 != call->pH225Channel && 0 != call->pH225Channel->sock)
   {
      ooCloseH225Connection(call);
   }

   /* Clean timers */
   if(call->timerList.count > 0)
   {
      dListFreeAll(call->pctxt, &(call->timerList));
   }

   if(gH323ep.gkClient && !OO_TESTFLAG(call->flags, OO_M_DISABLEGK))
   {
      ooGkClientCleanCall(gH323ep.gkClient, call);
   }

   ooRemoveCallFromList (call);
   OOTRACEINFO3("Removed call (%s, %s) from list\n", call->callType, 
                 call->callToken);

   if(call->pCallFwdData && call->pCallFwdData->fwdedByRemote)
   {

      if(gH323ep.h323Callbacks.onCallForwarded)
         gH323ep.h323Callbacks.onCallForwarded(call);

      if(ooH323HandleCallFwdRequest(call)!= OO_OK)
      {
         OOTRACEERR3("Error:Failed to forward call (%s, %s)\n", call->callType,
                     call->callToken);
      }
   }
   else {
      if(gH323ep.h323Callbacks.onCallCleared)
         gH323ep.h323Callbacks.onCallCleared(call);
   }

   pctxt = call->pctxt;
   freeContext(pctxt);
   ASN1CRTFREE0(pctxt);
   return OO_OK;
}


int ooCallSetCallerId(OOH323CallData* call, const char* callerid)
{
   if(!call || !callerid) return OO_FAILED;
   strncpy(call->ourCallerId, callerid, sizeof(call->ourCallerId)-1);
   call->ourCallerId[sizeof(call->ourCallerId)-1]='\0';
   return OO_OK;
}

int ooCallSetBearerCapabilityCircuitMode(OOH323CallData *call, const int CircuitMode)
{
   if(!call ) return OO_FAILED;
   call->BearerCapabilityTransferMode=(CircuitMode==Q931TransferCircuitMode)?Q931TransferCircuitMode:Q931TransferPacketMode;
   return OO_OK;
}

int ooCallSetCallingPartyNumber(OOH323CallData *call, const char *number, int callingPartyNumberType, int addVp200)
{
   if(call->callingPartyNumber) 
      memFreePtr(call->pctxt, call->callingPartyNumber);

   call->callingPartyNumber = (char*) memAlloc(call->pctxt, strlen(number)+1);
   if(call->callingPartyNumber)
   {
     /* IVeS: remove non alphanum digits if any (may corrupt SIP messages) */
     /* strcpy(call->callingPartyNumber, number); */
     IVeS_strcpy(call->callingPartyNumber, number);
   }
   else{
      OOTRACEERR3("Error:Memory - ooCallSetCallingPartyNumber - "
                  "callingPartyNumber.(%s, %s)\n", call->callType, 
                  call->callToken);
      return OO_FAILED;
   }
   call->callingPartyNumberType = callingPartyNumberType;
   call->addVp200Info = addVp200;
   /* Set dialed digits alias */
   /*   if(!strcmp(call->callType, "outgoing"))
   {
      ooCallAddAliasDialedDigits(call, number);
   }*/
   return OO_OK;
}

int ooCallGetCallingPartyNumber(OOH323CallData *call, char *buffer, int len)
{
   if(call->callingPartyNumber)
   {
      if(len>(int)strlen(call->callingPartyNumber))
      {
         strcpy(buffer, call->callingPartyNumber);
         return OO_OK;
      }
   }
   
   return OO_FAILED;
}


int ooCallSetCalledPartyNumber(OOH323CallData *call, const char *number)
{
   if(call->calledPartyNumber) 
      memFreePtr(call->pctxt, call->calledPartyNumber);

   call->calledPartyNumber = (char*) memAlloc(call->pctxt, strlen(number)+1);
   if(call->calledPartyNumber)
   {
     /* IVeS: remove non alphanum digits if any (may corrupt SIP messages) */
     /* strcpy(call->calledPartyNumber, number); */
     IVeS_strcpy(call->calledPartyNumber, number);
   }
   else{
      OOTRACEERR3("Error:Memory - ooCallSetCalledPartyNumber - "
                  "calledPartyNumber.(%s, %s)\n", call->callType, 
                  call->callToken);
      return OO_FAILED;
   }
   return OO_OK;
}

int ooCallGetCalledPartyNumber(OOH323CallData *call, char *buffer, int len)
{
   if(call->calledPartyNumber)
   {
      if(len>(int)strlen(call->calledPartyNumber))
      {
         strcpy(buffer, call->calledPartyNumber);
         return OO_OK;
      }
   }
   
   return OO_FAILED;
}

int ooCallClearAliases(OOH323CallData *call)
{
   if(call->ourAliases)
      memFreePtr(call->pctxt, call->ourAliases);
   call->ourAliases = NULL;
   return OO_OK;
}

int ooCallAddAlias
   (OOH323CallData *call, int aliasType, const char *value, OOBOOL local)
{
   ooAliases * psNewAlias=NULL;
   psNewAlias = (ooAliases*)memAlloc(call->pctxt, sizeof(ooAliases));
   if(!psNewAlias)
   {
      OOTRACEERR3("Error:Memory - ooCallAddAlias - psNewAlias"
                  "(%s, %s)\n", call->callType, call->callToken);
      return OO_FAILED;
   }
   psNewAlias->type = aliasType;
   psNewAlias->value = (char*) memAlloc(call->pctxt, strlen(value)+1);
   if(!psNewAlias->value)
   {
      OOTRACEERR3("Error:Memory - ooCallAddAlias - psNewAlias->value"
                  " (%s, %s)\n", call->callType, call->callToken);
      memFreePtr(call->pctxt, psNewAlias);
      return OO_FAILED;
   }
   /* IVeS: remove non alphanum digits if any (may corrupt SIP messages) */
   /* strcpy(psNewAlias->value, value); */
   IVeS_strcpy(psNewAlias->value, value);

   if(local)
   {
      psNewAlias->next = call->ourAliases;
      call->ourAliases = psNewAlias;
   }
   else {
     psNewAlias->next = call->remoteAliases;
     call->remoteAliases = psNewAlias;
   }

   OOTRACEDBGC5("Added %s alias %s to call. (%s, %s)\n", 
              local?"local":"remote", value, call->callType, call->callToken);
   return OO_OK;
}

int ooCallAddAliasH323ID(OOH323CallData *call, const char* h323id)
{
   return ooCallAddAlias(call, T_H225AliasAddress_h323_ID, h323id, TRUE);
}


int ooCallAddAliasDialedDigits(OOH323CallData *call, const char* dialedDigits)
{
   return ooCallAddAlias
               (call, T_H225AliasAddress_dialedDigits, dialedDigits, TRUE);
}


int ooCallAddAliasEmailID(OOH323CallData *call, const char* email)
{
   return ooCallAddAlias(call, T_H225AliasAddress_email_ID, email, TRUE);
}


int ooCallAddAliasURLID(OOH323CallData *call, const char* url)
{
   return ooCallAddAlias(call, T_H225AliasAddress_url_ID, url, TRUE);
} 
 

int ooCallAddRemoteAliasH323ID(OOH323CallData *call, const char* h323id)
{
   return ooCallAddAlias(call, T_H225AliasAddress_h323_ID, h323id, FALSE);
}

int ooCallAddRemoteAliasDialedDigits
   (OOH323CallData *call, const char* dialedDigits)
{
   return ooCallAddAlias
              (call, T_H225AliasAddress_dialedDigits, dialedDigits, FALSE);
}



/* Used to override global end point capabilities and add call specific 
   capabilities */

int ooCallAddG7231Capability(OOH323CallData *call, int cap, int txframes, 
                            int rxframes, OOBOOL silenceSuppression, int dir,
                            cb_StartReceiveChannel startReceiveChannel,
                            cb_StartTransmitChannel startTransmitChannel,
                            cb_StopReceiveChannel stopReceiveChannel,
                            cb_StopTransmitChannel stopTransmitChannel)
{
   return ooCapabilityAddSimpleCapability(call, cap, txframes, rxframes, 
                                silenceSuppression, dir, startReceiveChannel, 
                                startTransmitChannel, stopReceiveChannel, 
                                stopTransmitChannel, FALSE);
}



int ooCallAddG729Capability(OOH323CallData *call, int cap, int txframes, 
                            int rxframes, int dir,
                            cb_StartReceiveChannel startReceiveChannel,
                            cb_StartTransmitChannel startTransmitChannel,
                            cb_StopReceiveChannel stopReceiveChannel,
                            cb_StopTransmitChannel stopTransmitChannel)
{
   return ooCapabilityAddSimpleCapability(call, cap, txframes, rxframes, FALSE,
                          dir, startReceiveChannel, startTransmitChannel, 
                          stopReceiveChannel, stopTransmitChannel, FALSE);
}

/*
int ooCallAddG726Capability(OOH323CallData *call, int cap, int txframes, 
                            int rxframes, int dir,
                            cb_StartReceiveChannel startReceiveChannel,
                            cb_StartTransmitChannel startTransmitChannel,
                            cb_StopReceiveChannel stopReceiveChannel,
                            cb_StopTransmitChannel stopTransmitChannel)
{
   return ooCapabilityAddSimpleCapability(call, cap, txframes, rxframes, FALSE,
                          dir, startReceiveChannel, startTransmitChannel, 
                          stopReceiveChannel, stopTransmitChannel, FALSE);
}
*/

int ooCallAddG728Capability(OOH323CallData *call, int cap, int txframes, 
                            int rxframes, int dir,
                            cb_StartReceiveChannel startReceiveChannel,
                            cb_StartTransmitChannel startTransmitChannel,
                            cb_StopReceiveChannel stopReceiveChannel,
                            cb_StopTransmitChannel stopTransmitChannel)
{
   return ooCapabilityAddSimpleCapability(call, cap, txframes, rxframes, FALSE,
                          dir, startReceiveChannel, startTransmitChannel, 
                          stopReceiveChannel, stopTransmitChannel, FALSE);
}

int ooCallAddG711Capability(OOH323CallData *call, int cap, int txframes, 
                            int rxframes, int dir,
                            cb_StartReceiveChannel startReceiveChannel,
                            cb_StartTransmitChannel startTransmitChannel,
                            cb_StopReceiveChannel stopReceiveChannel,
                            cb_StopTransmitChannel stopTransmitChannel)
{
   return ooCapabilityAddSimpleCapability(call, cap, txframes, rxframes, FALSE,
                            dir, startReceiveChannel, startTransmitChannel, 
                            stopReceiveChannel, stopTransmitChannel, FALSE);
}

int ooCallAddGSMCapability
   (OOH323CallData* call, int cap, ASN1USINT framesPerPkt, 
    OOBOOL comfortNoise, OOBOOL scrambled, int dir,
    cb_StartReceiveChannel startReceiveChannel,
    cb_StartTransmitChannel startTransmitChannel,
    cb_StopReceiveChannel stopReceiveChannel,
    cb_StopTransmitChannel stopTransmitChannel)
{
   return ooCapabilityAddGSMCapability(call, cap, framesPerPkt, comfortNoise, 
                                     scrambled, dir, startReceiveChannel, 
                                     startTransmitChannel, stopReceiveChannel,
                                     stopTransmitChannel, FALSE);
}


int ooCallAddH263VideoCapability
   (OOH323CallData *call, int cap, unsigned sqcifMPI, unsigned qcifMPI, 
    unsigned cifMPI, unsigned cif4MPI, unsigned cif16MPI, unsigned maxBitRate, 
    int dir, cb_StartReceiveChannel startReceiveChannel,
    cb_StartTransmitChannel startTransmitChannel,
    cb_StopReceiveChannel stopReceiveChannel,
    cb_StopTransmitChannel stopTransmitChannel)
{

   return ooCapabilityAddH263VideoCapability(call, sqcifMPI, qcifMPI, cifMPI,
                                     cif4MPI, cif16MPI, maxBitRate,dir,
                                     startReceiveChannel, startTransmitChannel,
                                     stopReceiveChannel, stopTransmitChannel, 
                                     FALSE);

}

int ooCallAddH264VideoCapability
   (OOH323CallData *call, int cap, unsigned profile, unsigned constraint,
    unsigned level, unsigned maxBitRate,
    int dir, cb_StartReceiveChannel startReceiveChannel,
    cb_StartTransmitChannel startTransmitChannel,
    cb_StopReceiveChannel stopReceiveChannel,
    cb_StopTransmitChannel stopTransmitChannel)
{
   return ooCapabilityAddH264VideoCapability(call,
                profile, constraint,
                level, maxBitRate, dir,
                startReceiveChannel, startTransmitChannel,
                stopReceiveChannel, stopTransmitChannel,
                FALSE);
}


int ooCallEnableDTMFRFC2833(OOH323CallData *call, int dynamicRTPPayloadType)
{
   return ooCapabilityEnableDTMFRFC2833(call, dynamicRTPPayloadType);
}

int ooCallDisableDTMFRFC2833(OOH323CallData *call)
{
  return ooCapabilityDisableDTMFRFC2833(call);
}


int ooCallEnableDTMFH245Alphanumeric(OOH323CallData *call)
{
   return ooCapabilityEnableDTMFH245Alphanumeric(call);
}

int ooCallDisableDTMFH245Alphanumeric(OOH323CallData *call)
{
   return ooCapabilityDisableDTMFH245Alphanumeric(call);
}

int ooCallEnableDTMFH245Signal(OOH323CallData *call)
{
   return ooCapabilityEnableDTMFH245Signal(call);
}

int ooCallDisableDTMFH245Signal(OOH323CallData *call)
{
   return ooCapabilityDisableDTMFH245Signal(call);
}

int ooCallEnableDTMFQ931Keypad(OOH323CallData *call)
{
   return ooCapabilityEnableDTMFQ931Keypad(call);
}

int ooCallDisableDTMFQ931Keypad(OOH323CallData *call)
{
   return ooCapabilityDisableDTMFQ931Keypad(call);
}


OOH323CallData* ooFindCallByToken(char *callToken)
{
   OOH323CallData *call;
   if(!callToken)
   {
      OOTRACEERR1("ERROR:Invalid call token passed - ooFindCallByToken\n");
      return NULL;
   }
   if(!gH323ep.callList)
   {
      OOTRACEERR1("ERROR: Empty calllist - ooFindCallByToken failed\n");
      return NULL;
   }
   call = gH323ep.callList;
   while(call)
   {
      if(!strcmp(call->callToken, callToken))
         break;
      else
         call = call->next;
   }
   
   if(!call)
   {
      OOTRACEERR2("ERROR:Call with token %s not found\n", callToken);
      return NULL;
   }
   return call;
}



/* Checks whether session with suplied ID and direction is already active*/
ASN1BOOL ooIsSessionEstablished(OOH323CallData *call, int sessionID, char* dir)
{
   OOLogicalChannel * temp = NULL;
   temp = call->logicalChans;
   while(temp)
   {
      if(temp->sessionID == sessionID              &&
         temp->state == OO_LOGICALCHAN_ESTABLISHED && 
         !strcmp(temp->dir, dir)                     )
         return TRUE;
      temp = temp->next;
   }
   return FALSE;
}

int ooAddMediaInfo(OOH323CallData *call, OOMediaInfo mediaInfo)
{
   OOMediaInfo *newMediaInfo=NULL;
  OOTRACEAST(OOTRCLVLDBGA,">> ooAddMediaInfo\n");
   if(!call)
   {
     OOTRACEERR3("Error:Invalid 'call' param for ooAddMediaInfo. call=0x%X \n",call,call);
     return OO_FAILED;
   }
   newMediaInfo = (OOMediaInfo*) memAlloc(call->pctxt, sizeof(OOMediaInfo));
   if(!newMediaInfo)
   {
      OOTRACEERR3("Error:Memory - ooAddMediaInfo - newMediaInfo. "
                  "(%s, %s)\n", call->callType, call->callToken);
      return OO_FAILED;
   }

   memcpy (newMediaInfo, &mediaInfo, sizeof(OOMediaInfo));

   OOTRACEAST(OOTRCLVLDBGA,"ooAddMediaInfo Configured mediainfo for cap %s [%s:%d] (%s, %s)\n", 
              ooGetCapTypeText(mediaInfo.cap),newMediaInfo->lMediaIP ,newMediaInfo->lMediaPort,
                call->callType, call->callToken);
   if(!call->mediaInfo) {
       
      newMediaInfo->next = NULL;
      call->mediaInfo = newMediaInfo;
   }
   else {
      newMediaInfo->next = call->mediaInfo;
      call->mediaInfo = newMediaInfo;
   }
   return OO_OK;
}

unsigned ooCallGenerateSessionID
   (OOH323CallData *call, OOCapType type, char *dir)
{
   unsigned sessionID=0;

   if(type == OO_CAP_TYPE_AUDIO)
   {
      if(!ooGetLogicalChannel(call, 1, dir))
      {
         sessionID = 1;
      }
      else{
         if(call->masterSlaveState == OO_MasterSlave_Master)
            sessionID = call->nextSessionID++;
         else{
            OOTRACEDBGC4("Session id for %s channel of type audio has to be "
                        "provided by remote.(%s, %s)\n", dir, call->callType, 
                         call->callToken);
            sessionID = 0; /* Will be assigned by remote */
         }
      }
   }

   if(type == OO_CAP_TYPE_VIDEO)
   {
      if(!ooGetLogicalChannel(call, 2, dir))
      {
         sessionID = 2;
      }
      else{
         if(call->masterSlaveState == OO_MasterSlave_Master)
            sessionID = call->nextSessionID++;
         else{
            sessionID = 0; /* Will be assigned by remote */
            OOTRACEDBGC4("Session id for %s channel of type video has to be "
                        "provided by remote.(%s, %s)\n", dir, call->callType, 
                         call->callToken);
         }
      }
   }
   return sessionID;

}


int ooCallH245ConnectionRetryTimerExpired(void *data)
{
   ooTimerCallback *cbData = (ooTimerCallback*) data;
   OOH323CallData *call = cbData->call;

   OOTRACEINFO3("H245 connection retry timer expired. (%s, %s)\n", 
                                            call->callType, call->callToken); 
   memFreePtr(call->pctxt, cbData);

   call->h245ConnectionAttempts++;

   ooCreateH245Connection(call);

   return OO_OK;
}

const char* ooGetReasonCodeText (OOUINT32 code)
{
   static const char* reasonCodeText[] = {
      "OO_REASON_UNKNOWN", 
      "OO_REASON_INVALIDMESSAGE",
      "OO_REASON_TRANSPORTFAILURE", 
      "OO_REASON_NOROUTE",
      "OO_REASON_NOUSER",
      "OO_REASON_NOBW",
      "OO_REASON_GK_NOCALLEDUSER",
      "OO_REASON_GK_NOCALLERUSER",
      "OO_REASON_GK_NORESOURCES",
      "OO_REASON_GK_UNREACHABLE",
      "OO_REASON_GK_CLEARED",
      "OO_REASON_NOCOMMON_CAPABILITIES",
      "OO_REASON_REMOTE_FWDED",   
      "OO_REASON_LOCAL_FWDED",
      "OO_REASON_REMOTE_CLEARED", 
      "OO_REASON_LOCAL_CLEARED", 
      "OO_REASON_REMOTE_BUSY",
      "OO_REASON_LOCAL_BUSY",
      "OO_REASON_REMOTE_NOANSWER",
      "OO_REASON_LOCAL_NOTANSWERED",
      "OO_REASON_REMOTE_REJECTED",
      "OO_REASON_LOCAL_REJECTED",
      "OO_REASON_REMOTE_CONGESTED",
      "OO_REASON_LOCAL_CONGESTED"
   };
   return ooUtilsGetText (code, reasonCodeText, OONUMBEROF(reasonCodeText));
}

const char* ooGetCallStateText (OOCallState callState)
{
   static const char* callStateText[] = {
      "OO_CALL_CREATED",
      "OO_CALL_WAITING_ADMISSION",
      "OO_CALL_CONNECTING",
      "OO_CALL_CONNECTED",
      "OO_CALL_PAUSED",
      "OO_CALL_CLEAR",
      "OO_CALL_CLEAR_RELEASERECVD",
      "OO_CALL_CLEAR_RELEASESENT",
      "OO_CALL_CLEARED"
   };
   return ooUtilsGetText (callState, callStateText, OONUMBEROF(callStateText));
}

