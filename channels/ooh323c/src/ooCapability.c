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
#include "ooCapability.h"
#include "ootrace.h"
#include "ooCalls.h"
#include "ooh323ep.h"
#include "ooUtils.h"
/** Global endpoint structure */
extern OOH323EndPoint gH323ep;

static int giDynamicRTPPayloadType = 101;
static void ooExtractH264Parameters(DList * collapsing, unsigned * p_profile, unsigned * p_level);


static unsigned char ooProfileSIP2H241(unsigned char sipProfile);
static unsigned char ooProfileH2412SIP(unsigned char h241Profile);
static unsigned char ooLevelSIP2H241( unsigned char sipLevel );
static unsigned char ooLevelH2412SIP( unsigned char h241Level );

/*! The h.264 profile level is the profile (eg 1.1 x 10) */
typedef enum
{
        level_1   = 10, /* 0x0A */
        level_1_1 = 11, /* 0x0B */
        level_1_2 = 12, /* 0x0C */
        level_1_3 = 13, /* 0x0D */
        level_2   = 20, /* 0x14 */
        level_2_1 = 21, /* 0x15 */
        level_2_2 = 22, /* 0x16 */
        level_3   = 30, /* 0x1E */
        level_3_1 = 31, /* 0x1F */
        level_3_2 = 32, /* 0x20 */
        level_4   = 40, /* 0x28 */
        level_4_1 = 41, /* 0x29 */
        level_4_2 = 42, /* 0x2A */
        level_5   = 50, /* 0x32 */
        level_5_1 = 51, /* 0x33 */
} ast_h264_level;

typedef enum
{
        h264_profile_calvac44 = 44,
        h264_profile_baseline = 66,
        h264_profile_main = 77,
        h264_profile_extended = 88,
        h264_profile_high = 100,
        h264_profile_high10 = 110,
        h264_profile_high422 = 122,
        h264_profile_high444 = 244,
} ast_h264_profile;

int ooCapabilityEnableDTMFRFC2833
   (OOH323CallData *call, int dynamicRTPPayloadType)
{
   if(!call)
   {
      gH323ep.dtmfmode |= OO_CAP_DTMF_RFC2833;
      OOTRACEINFO1("Enabled RFC2833 DTMF capability for end-point\n");
   }
   else{
      call->dtmfmode |= OO_CAP_DTMF_RFC2833;
      OOTRACEINFO3("Enabled RFC2833 DTMF capability for (%s, %s) \n", 
                   call->callType, call->callToken);
   }

   /*Dynamic RTP payload type range is from 96 - 127 */
   if(dynamicRTPPayloadType >= 96 && dynamicRTPPayloadType <= 127)
      giDynamicRTPPayloadType = dynamicRTPPayloadType;

   return OO_OK;
}



int ooCapabilityDisableDTMFRFC2833(OOH323CallData *call)
{
   if(!call){
      gH323ep.dtmfmode ^= OO_CAP_DTMF_RFC2833;
      OOTRACEINFO1("Disabled RFC2833 DTMF capability for end-point\n");
   }
   else{
      call->dtmfmode ^= OO_CAP_DTMF_RFC2833;
      OOTRACEINFO3("Disabled RFC2833 DTMF capability for (%s, %s)\n", 
                    call->callType, call->callToken);
   }

   return OO_OK;
}

int ooCapabilityEnableDTMFH245Alphanumeric(OOH323CallData *call)
{
   if(!call){
      gH323ep.dtmfmode |= OO_CAP_DTMF_H245_alphanumeric;
      OOTRACEINFO1("Dtmf mode set to H.245(alphanumeric) for endpoint\n");
   }
   else {
      call->dtmfmode |= OO_CAP_DTMF_H245_alphanumeric;
      OOTRACEINFO3("Dtmf mode set to H.245(alphanumeric) for (%s, %s)\n", 
                    call->callType, call->callToken);
   }
   return OO_OK;
}

int ooCapabilityDisableDTMFH245Alphanumeric(OOH323CallData *call)
{
   if(!call){
      gH323ep.dtmfmode ^= OO_CAP_DTMF_H245_alphanumeric;
      OOTRACEINFO1("Dtmf mode H.245(alphanumeric) disabled for endpoint\n");
   }
   else {
      call->dtmfmode ^= OO_CAP_DTMF_H245_alphanumeric;
      OOTRACEINFO3("Dtmf mode H.245(alphanumeric) disabled for (%s, %s)\n", 
                    call->callType, call->callToken);
   }
   return OO_OK;
}

int ooCapabilityEnableDTMFH245Signal(OOH323CallData *call)
{
   if(!call){
      gH323ep.dtmfmode |= OO_CAP_DTMF_H245_signal;
      OOTRACEINFO1("Dtmf mode set to H.245(signal) for endpoint\n");
   }
   else {
      call->dtmfmode |= OO_CAP_DTMF_H245_signal;
      OOTRACEINFO3("Dtmf mode set to H.245(signal) for (%s, %s)\n", 
                    call->callType, call->callToken);
   }
   return OO_OK;
}

int ooCapabilityDisableDTMFH245Signal(OOH323CallData *call)
{
   if(!call){
      gH323ep.dtmfmode ^= OO_CAP_DTMF_H245_signal;
      OOTRACEINFO1("Dtmf mode H.245(signal) disabled for endpoint\n");
   }
   else {
      call->dtmfmode ^= OO_CAP_DTMF_H245_signal;
      OOTRACEINFO3("Dtmf mode H.245(signal) disabled for (%s, %s)\n", 
                    call->callType, call->callToken);
   }
   return OO_OK;
}

int ooCapabilityEnableDTMFQ931Keypad(struct OOH323CallData *call)
{
   if(!call){
      gH323ep.dtmfmode |= OO_CAP_DTMF_Q931;
      OOTRACEINFO1("Dtmf mode set to Q.931(keypad) for the endpoint\n");
   }
   else {
      call->dtmfmode |= OO_CAP_DTMF_Q931;
      OOTRACEINFO3("Dtmf mode set to Q.931(keypad) for the call (%s, %s)\n", 
                    call->callType, call->callToken);
   }
   return OO_OK;
}

int ooCapabilityDisableDTMFQ931Keypad(struct OOH323CallData *call)
{
   if(!call){
      gH323ep.dtmfmode ^= OO_CAP_DTMF_Q931;
      OOTRACEINFO1("Dtmf mode Q.931(keypad) disabled for the endpoint\n");
   }
   else {
      call->dtmfmode ^= OO_CAP_DTMF_Q931;
      OOTRACEINFO3("Dtmf mode Q.931(keypad) disabled for the call (%s, %s)\n", 
                    call->callType, call->callToken);
   }
   return OO_OK;
}

int ooCapabilityAddH263VideoCapability(ooCallData *call, 
                              unsigned sqcifMPI, unsigned qcifMPI, 
                              unsigned cifMPI, unsigned cif4MPI, 
                              unsigned cif16MPI, unsigned maxBitRate, int dir, 
                              cb_StartReceiveChannel startReceiveChannel,
                              cb_StartTransmitChannel startTransmitChannel,
                              cb_StopReceiveChannel stopReceiveChannel,
                              cb_StopTransmitChannel stopTransmitChannel, 
                              OOBOOL remote)
{
   int ret = OO_OK;
   if(sqcifMPI>0)
   {
      ret = ooCapabilityAddH263VideoCapability_helper(call, sqcifMPI, 0, 
                                 0, 0, 0, maxBitRate, dir, startReceiveChannel,
                                 startTransmitChannel, stopReceiveChannel,
                                 stopTransmitChannel, remote);
      if(ret != OO_OK)
      {
         OOTRACEERR1("Error: Failed to add H263 sqcifMPI capability\n");
         return OO_FAILED;
      }
   }
   if(qcifMPI>0)
   {
      ret = ooCapabilityAddH263VideoCapability_helper(call, 0, qcifMPI, 0,
                                 0, 0, maxBitRate, dir, startReceiveChannel,
                                 startTransmitChannel, stopReceiveChannel,
                                 stopTransmitChannel, remote);
      if(ret != OO_OK)
      {
         OOTRACEERR1("Error: Failed to add H263 qcifMPI capability\n");
         return OO_FAILED;
      }
   }
   if(cifMPI>0)
   {
      ret = ooCapabilityAddH263VideoCapability_helper(call, 0, 0, cifMPI, 
                                 0, 0, maxBitRate, dir, startReceiveChannel,
                                 startTransmitChannel, stopReceiveChannel,
                                 stopTransmitChannel, remote);
      if(ret != OO_OK)
      {
         OOTRACEERR1("Error: Failed to add H263 cifMPI capability\n");
         return OO_FAILED;
      }
   }
   if(cif4MPI>0)
   {
      ret = ooCapabilityAddH263VideoCapability_helper(call, 0, 0, 0, 
                                 cif4MPI, 0, maxBitRate, dir, 
                                 startReceiveChannel,
                                 startTransmitChannel, stopReceiveChannel,
                                 stopTransmitChannel, remote);
      if(ret != OO_OK)
      {
         OOTRACEERR1("Error: Failed to add H263 cif4MPI capability\n");
         return OO_FAILED;
      }
   }
   if(cif16MPI>0)
   {
      ret = ooCapabilityAddH263VideoCapability_helper(call, dir, 0, 0, 0, 0, 
                                 cif16MPI, maxBitRate, startReceiveChannel,
                                 startTransmitChannel, stopReceiveChannel,
                                 stopTransmitChannel, remote);
      if(ret != OO_OK)
      {
         OOTRACEERR1("Error: Failed to add H263 cif16MPI capability\n");
         return OO_FAILED;
      }
   }
   return OO_OK;

}

int ooCapabilityAddH263VideoCapability_helper(ooCallData *call,
                              unsigned sqcifMPI, unsigned qcifMPI, 
                              unsigned cifMPI, unsigned cif4MPI, 
                              unsigned cif16MPI, unsigned maxBitRate, int dir, 
                              cb_StartReceiveChannel startReceiveChannel,
                              cb_StartTransmitChannel startTransmitChannel,
                              cb_StopReceiveChannel stopReceiveChannel,
                              cb_StopTransmitChannel stopTransmitChannel, 
                              OOBOOL remote)
{

   ooH323EpCapability *epCap = NULL, *cur=NULL;
   OOH263CapParams *params=NULL;   
   OOCTXT *pctxt=NULL;
   char *pictureType = NULL;
   int cap = OO_H263VIDEO;

 
   OOTRACEAST(OOTRCLVLDBGA,"[H323/CODEC]  :"
              "Adding endpoint H264 capability call(0x%X)\n", call);

   if(!call) pctxt = &gH323ep.ctxt;
   else pctxt = call->pctxt;

   epCap = (ooH323EpCapability*)memAllocZ(pctxt, sizeof(ooH323EpCapability));
   params = (OOH263CapParams*) memAllocZ(pctxt, sizeof(OOH263CapParams));
   if(!epCap || !params)
   {
      OOTRACEERR1("[H323/CODEC] Error:Memory - ooCapabilityAddH263Capability - epCap/params"
                  ".\n");
      return OO_FAILED;
   }
   
   if(sqcifMPI>0)
   {
      params->MPI = sqcifMPI;
      params->picFormat = OO_PICFORMAT_SQCIF;
      pictureType = "SQCIF";
   }
   if(qcifMPI>0)
   {
      params->MPI = qcifMPI;
      params->picFormat =  OO_PICFORMAT_QCIF;
      pictureType = "QCIF";
   }
   if(cifMPI>0)
   {
      params->MPI = cifMPI;
      params->picFormat = OO_PICFORMAT_CIF;
      pictureType = "CIF";
   }
   if(cif4MPI>0)
   {
      params->MPI = cif4MPI;
      params->picFormat = OO_PICFORMAT_CIF4;
      pictureType = "CIF4";
   }
   if(cif16MPI>0)
   {
      params->MPI = cif16MPI;
      params->picFormat = OO_PICFORMAT_CIF16;
      pictureType = "CIF16";
   }

   params->maxBitRate = maxBitRate/100;


   if(dir & OORXANDTX)
   {
      epCap->dir = OORX;
      epCap->dir |= OOTX;
   }
   else
      epCap->dir = dir;
   
   epCap->cap = OO_H263VIDEO;
   epCap->capType = OO_CAP_TYPE_VIDEO;
   epCap->params = (void*)params;
   epCap->startReceiveChannel = startReceiveChannel;
   epCap->startTransmitChannel = startTransmitChannel;
   epCap->stopReceiveChannel = stopReceiveChannel;
   epCap->stopTransmitChannel = stopTransmitChannel;
   
   epCap->next = NULL;

   if(!call)
   {
     /*Add as local capability */
      OOTRACEAST(OOTRCLVLDBGA,"[H323/CODEC]  :"
                   "Adding endpoint capability %s. %s\n", 
                   ooGetCapTypeText(epCap->cap),pictureType);
      if(!gH323ep.myCaps)
      {
        OOTRACEAST(OOTRCLVLDBGA,"[H323/CODEC]  :"
                   "Global capability %s. %s\n", 
                   ooGetCapTypeText(epCap->cap),pictureType);
         gH323ep.myCaps = epCap;
      }
      else{
        OOTRACEAST(OOTRCLVLDBGA,"[H323/CODEC]  :"
                   "Add to global capability %s. %s\n", 
                   ooGetCapTypeText(epCap->cap),pictureType);
         cur = gH323ep.myCaps;
         while(cur->next) cur = cur->next;
         cur->next = epCap;
      }
      ooAppendCapToCapPrefs(NULL, cap);
      gH323ep.noOfCaps++;
   }
   else
   {
     if(remote)
     {
       OOTRACEAST(OOTRCLVLDBGA,"[H323/CODEC]  :"
                    "Adding call specific capability %s. %s (%s, %s) to remote\n", 
                    ooGetCapTypeText(epCap->cap), pictureType ,call->callType, 
                    call->callToken);
       /*Add as remote capability */
       if(!call->remoteCaps)
       {           
         call->remoteCaps = epCap;
       }
       else
       {
         cur = call->remoteCaps;
         while(cur->next) cur = cur->next;
         cur->next = epCap;
       }
     }
     else
     {
       OOTRACEAST(OOTRCLVLDBGA,"[H323/CODEC]  :"
                    "Adding call specific capability %s. %s(%s, %s) \n", 
                    ooGetCapTypeText(epCap->cap), pictureType ,call->callType, 
                    call->callToken);
       if(!call->ourCaps)
       {
         call->ourCaps = epCap;
         ooResetCapPrefs(call);
       }
       else
       {
         cur = call->ourCaps;
         while(cur->next) cur = cur->next;
         cur->next = epCap;
       }
       call->CallHaveH263 = TRUE ;
       ooAppendCapToCapPrefs(call, cap);
     }
   }

   return OO_OK;
}

/* add H.264 Capability */
int ooCapabilityAddH264VideoCapability(ooCallData *call,
                              unsigned profile, unsigned constraint,
			      unsigned level, unsigned maxBitRate, int dir,
                              cb_StartReceiveChannel startReceiveChannel,
                              cb_StartTransmitChannel startTransmitChannel,
                              cb_StopReceiveChannel stopReceiveChannel,
                              cb_StopTransmitChannel stopTransmitChannel,
                              OOBOOL remote)
{
   ooH323EpCapability *epCap = NULL, *cur=NULL;
   OOH264CapParams *params=NULL;
   OOCTXT *pctxt=NULL;
   char *pictureType = NULL;
   int cap = OO_GENERICVIDEO;

   if(!call) pctxt = &gH323ep.ctxt;
   else pctxt = call->pctxt;

   epCap = (ooH323EpCapability*)memAllocZ(pctxt, sizeof(ooH323EpCapability));
   params = (OOH264CapParams*) memAllocZ(pctxt, sizeof(OOH264CapParams));
   if(!epCap || !params)
   {
      OOTRACEERR1("Error:Memory - ooCapabilityAddH264Capability - epCap/params \n");
      return OO_FAILED;
   }

   /* Check values here */
   params->maxBitRate = maxBitRate/100;
   params->level = level ;
   params->profile = profile ;

   if(dir & OORXANDTX)
   {
      epCap->dir = OORX;
      epCap->dir |= OOTX;
   }
   else
      epCap->dir = dir;

   epCap->cap = OO_GENERICVIDEO;
   epCap->capType = OO_CAP_TYPE_VIDEO;
   epCap->params = (void*)params;
   epCap->startReceiveChannel = startReceiveChannel;
   epCap->startTransmitChannel = startTransmitChannel;
   epCap->stopReceiveChannel = stopReceiveChannel;
   epCap->stopTransmitChannel = stopTransmitChannel;
   epCap->next = NULL;

   /*Add as local capability */
   OOTRACEAST(OOTRCLVLDBGA,"[H323/CODEC]  :"
              "Adding endpoint H264 capability (call[0x%X] )\n", call);

   if(!call)
   {
     /*Add as local capability */
     OOTRACEAST(OOTRCLVLDBGA,"[H323/CODEC]  :"
                "Adding endpoint H264 capability %s.\n", 
                ooGetCapTypeText(epCap->cap));
      if(!gH323ep.myCaps)
      {
        OOTRACEAST(OOTRCLVLDBGA,"[H323/CODEC]  :"
                   "Global capability H264  %s. \n", 
                   ooGetCapTypeText(epCap->cap));
         gH323ep.myCaps = epCap;
      }
      else
      {
        OOTRACEAST(OOTRCLVLDBGA,"[H323/CODEC]  :"
                   "Add H264  to global capability %s. \n", 
                   ooGetCapTypeText(epCap->cap));
         cur = gH323ep.myCaps;
         while(cur->next) cur = cur->next;
         cur->next = epCap;
      }
      ooAppendCapToCapPrefs(NULL, cap);
      gH323ep.noOfCaps++;
   }
   else
   {
     OOTRACEAST(OOTRCLVLDBGA,"[H323/CODEC]  :"
                "Adding endpoint H264 capability to call(0x%X)\n", call);
      if(remote)
      {
        OOTRACEAST(OOTRCLVLDBGA,"[H323/CODEC]  :"
                   "Adding call specific capability %s. %s (%s, %s) to remote\n", 
                   ooGetCapTypeText(epCap->cap), pictureType ,call->callType, 
                   call->callToken);
         /*Add as remote capability */
         if(!call->remoteCaps)
            call->remoteCaps = epCap;
         else{
            cur = call->remoteCaps;
            while(cur->next) cur = cur->next;
            cur->next = epCap;
         }
      }
      else
      {
        /*Add as our capability */
        OOTRACEAST(OOTRCLVLDBGA,"[H323/CODEC]  :Adding call specific H264 video capability with profile 0x%02x"
                   " level 0x%02x. "
                   "(%s, %s)\n", profile, level, call->callType, call->callToken);
        if(!call->ourCaps)
        {
           call->ourCaps = epCap;
           ooResetCapPrefs(call);
        }
        else
        {
           cur = call->ourCaps;
           while(cur->next) cur = cur->next;
           cur->next = epCap;
        }
        call->CallHaveH264 = TRUE ;
        ooAppendCapToCapPrefs(call, cap);
     }
   }
   return OO_OK;

}
/* Used for g711 ulaw/alaw, g728, g729 and g7231 */
int ooCapabilityAddSimpleCapability
   (OOH323CallData *call, int cap, int txframes, 
    int rxframes, OOBOOL silenceSuppression, int dir, 
    cb_StartReceiveChannel startReceiveChannel,
    cb_StartTransmitChannel startTransmitChannel,
    cb_StopReceiveChannel stopReceiveChannel,
    cb_StopTransmitChannel stopTransmitChannel,
    OOBOOL remote)
{
   ooH323EpCapability *epCap = NULL, *cur=NULL;
   OOCapParams *params=NULL;   
   OOCTXT *pctxt=NULL;
   if(!call) pctxt = &gH323ep.ctxt;
   else pctxt = call->pctxt;

   epCap = (ooH323EpCapability*)memAlloc(pctxt, sizeof(ooH323EpCapability));
   params = (OOCapParams*) memAlloc(pctxt, sizeof(OOCapParams));
   if(!epCap || !params)
   {
      OOTRACEERR1("ooCapabilityAddSimpleCapability:ERROR: Memory - ooCapabilityAddSimpleCapability - "
                  "epCap/params\n");
      return OO_FAILED;
   }


   params->txframes = txframes;
   params->rxframes = rxframes;
   /* Ignore silence suppression parameter unless cap is g7231 */
   if(cap == OO_G7231)
      params->silenceSuppression = silenceSuppression;
   else
      params->silenceSuppression = FALSE; /* Set to false for g711 and g729*/

   if(dir & OORXANDTX) {
      epCap->dir = OORX;
      epCap->dir |= OOTX;
   }
   else {
      epCap->dir = dir;
   }
   
   epCap->cap = cap;
   epCap->capType = OO_CAP_TYPE_AUDIO;
   epCap->params = (void*)params;
   epCap->startReceiveChannel = startReceiveChannel;
   epCap->startTransmitChannel = startTransmitChannel;
   epCap->stopReceiveChannel = stopReceiveChannel;
   epCap->stopTransmitChannel = stopTransmitChannel;
   epCap->next = NULL;

   if(!call)
   {
      /* Add as local capability */
      OOTRACEAST(OOTRCLVLDBGA,"ooCapabilityAddSimpleCapability:"
                   "Adding endpoint capability %s. \n", 
                     ooGetCapTypeText(epCap->cap));
      if(!gH323ep.myCaps) {
               OOTRACEAST(OOTRCLVLDBGA,"ooCapabilityAddSimpleCapability:"
                   "Update global capability %s. \n",
                     ooGetCapTypeText(epCap->cap));

         gH323ep.myCaps = epCap;
      }
      else{
         cur = gH323ep.myCaps;
         while(cur->next) cur = cur->next;
         cur->next = epCap;
      }
      ooAppendCapToCapPrefs(NULL, cap);
      gH323ep.noOfCaps++;
   }
   else{
      if(remote)
      {
         /* Add as remote capability */
         OOTRACEAST(OOTRCLVLDBGA,"ooCapabilityAddSimpleCapability:"
                      "Adding call specific capability %s. (%s, %s) to remote\n", 
                      ooGetCapTypeText(epCap->cap), call->callType, 
                      call->callToken);
         switch ( epCap->cap )
         {
           case OO_G711ALAW64K:
           case OO_G711ALAW56K:
             call->CallHaveAlaw = TRUE ;
            break ;
           case OO_G711ULAW64K:
           case OO_G711ULAW56K:
             call->CallHaveUlaw = TRUE ;
           break ;
           case OO_G729:
           case OO_G729A:
           case OO_G729B:
           case OO_G729AB:
             call->CallHaveG729 = TRUE ;
           break ;
           case OO_G7231:
             call->CallHaveG723 = TRUE ;
            break ;
         }
         if(!call->remoteCaps) {
            call->remoteCaps = epCap;
         }
         else{
            cur = call->remoteCaps;
            while(cur->next) cur = cur->next;
            cur->next = epCap;
         }
      }
      else{
         /* Add as our capability */
         OOTRACEAST(OOTRCLVLDBGA,"ooCapabilityAddSimpleCapability:"
                      "Adding call specific capability %s. (%s, %s)\n", 
                      ooGetCapTypeText(epCap->cap), call->callType, 
                      call->callToken);
         switch ( epCap->cap )
         {
           case OO_G711ALAW64K:
           case OO_G711ALAW56K:
             call->CallHaveAlaw = TRUE ;
            break ;
           case OO_G711ULAW64K:
           case OO_G711ULAW56K:
             call->CallHaveUlaw = TRUE ;
           break ;
           case OO_G729:
           case OO_G729A:
           case OO_G729B:
           case OO_G729AB:
             call->CallHaveG729 = TRUE ;
           break ;
           case OO_G7231:
             call->CallHaveG723 = TRUE ;
            break ;
         }
         if(!call->ourCaps){
            call->ourCaps = epCap;
            ooResetCapPrefs(call);
         }
         else{
            cur = call->ourCaps;
            while(cur->next) cur = cur->next;
            cur->next = epCap;
         }
         
         ooAppendCapToCapPrefs(call, cap);
      }
   }
           
   return OO_OK;
}


int ooCapabilityAddGSMCapability(OOH323CallData *call, int cap, 
                                unsigned framesPerPkt, OOBOOL comfortNoise,
                                OOBOOL scrambled, int dir, 
                                cb_StartReceiveChannel startReceiveChannel,
                                cb_StartTransmitChannel startTransmitChannel,
                                cb_StopReceiveChannel stopReceiveChannel,
                                cb_StopTransmitChannel stopTransmitChannel, 
                                OOBOOL remote)
{

   ooH323EpCapability *epCap = NULL, *cur=NULL;
   OOGSMCapParams *params=NULL;   
   OOCTXT *pctxt = NULL;
 
   if(!call) pctxt = &gH323ep.ctxt;
   else pctxt = call->pctxt;

   epCap = (ooH323EpCapability*)memAlloc(pctxt, sizeof(ooH323EpCapability));
   params = (OOGSMCapParams*) memAlloc(pctxt, sizeof(OOGSMCapParams));
   if(!epCap || !params)
   {
      OOTRACEERR1("Error:Memory - ooCapabilityAddGSMCapability - "
                  "epCap/params\n");
      return OO_FAILED;
   }


   params->rxframes = framesPerPkt;
   params->txframes = framesPerPkt;
   params->comfortNoise = comfortNoise;
   params->scrambled = scrambled;
   if(dir & OORXANDTX)
   {
      epCap->dir = OORX;
      epCap->dir |= OOTX;
   }
   else
      epCap->dir = dir;

   epCap->cap = cap;
   epCap->capType = OO_CAP_TYPE_AUDIO;
   epCap->params = (void*)params;
   epCap->startReceiveChannel = startReceiveChannel;
   epCap->startTransmitChannel = startTransmitChannel;
   epCap->stopReceiveChannel = stopReceiveChannel;
   epCap->stopTransmitChannel = stopTransmitChannel;
   
   epCap->next = NULL;
   /* Add as local capability */
   if(!call)
   {
      if(!gH323ep.myCaps)
         gH323ep.myCaps = epCap;
      else{
         cur = gH323ep.myCaps;
         while(cur->next) cur = cur->next;
         cur->next = epCap;
      }
      ooAppendCapToCapPrefs(NULL, cap);
      gH323ep.noOfCaps++;
   }
   else{
      if(remote)
      {
         /*Add as remote capability */
         if(!call->remoteCaps)
            call->remoteCaps = epCap;
         else{
            cur = call->remoteCaps;
            while(cur->next) cur = cur->next;
            cur->next = epCap;
         }
      }
      else{
         OOTRACEDBGC4("Adding call specific capability %s. (%s, %s)\n", 
                     ooGetCapTypeText(epCap->cap), call->callType, 
                     call->callToken);
         /*Add as our capability */
         if(!call->ourCaps){
            call->ourCaps = epCap;
            ooResetCapPrefs(call);
         }
         else{
            cur = call->ourCaps;
            while(cur->next) cur = cur->next;
            cur->next = epCap;
         }
          call->CallHaveG729 = TRUE ;
         ooAppendCapToCapPrefs(call, cap);
      }
   }

   return OO_OK;
}




struct H245VideoCapability* ooCapabilityCreateVideoCapability
      (ooH323EpCapability *epCap, OOCTXT *pctxt, int dir)
{

   if(!epCap)
   {
     OOTRACEERR1("Error:Invalid capability parameter passed to "
                 "ooCapabilityCreateVideoCapability.\n");
     return NULL;
   }
   
   if(!(epCap->dir & dir))
   {
      OOTRACEERR1("Error:Failed to create capability due to direction "
                  "mismatch.\n");
      return NULL;
   }

   switch(epCap->cap)
   {
   case OO_H263VIDEO:
     return ooCapabilityCreateH263VideoCapability(epCap, pctxt, dir);

   case OO_GENERICVIDEO: /* = H.264 */
     return ooCapabilityCreateGenericVideoCapability(epCap, pctxt, dir);

   case OO_NONSTDVIDEO:
   case OO_H261VIDEO:
   case OO_H262VIDEO:
   case OO_IS11172VIDEO:
   case OO_EXTELEMVIDEO:
   default:
      OOTRACEERR2("ERROR: Don't know how to create video capability %s\n",
                  ooGetCapTypeText(epCap->cap));
   }
   return NULL;
}


   
struct H245AudioCapability* ooCapabilityCreateAudioCapability
      (ooH323EpCapability *epCap, OOCTXT *pctxt, int dir)
{

   if(!epCap)
   {
     OOTRACEERR1("Error:Invalid capability parameter passed to "
                 "ooCapabilityCreateAudioCapability.\n");
     return NULL;
   }
   
   if(!(epCap->dir & dir))
   {
      OOTRACEERR1("Error:Failed to create capability due to direction "
                  "mismatch.\n");
      return NULL;
   }

   switch(epCap->cap)
   {
     case OO_G711ALAW64K:
     case OO_G711ULAW56K:
     case OO_G711ALAW56K:
     case OO_G728:
     case OO_G729A:
     case OO_G7231:
     case OO_G711ULAW64K:
     case OO_G729:
     return ooCapabilityCreateSimpleCapability(epCap, pctxt, dir);
   case OO_GSMFULLRATE:
      return ooCapabilityCreateGSMFullRateCapability(epCap, pctxt, dir);
   default:
      OOTRACEERR2("ERROR: Don't know how to create audio capability %d\n",
                  epCap->cap);
   }
   return NULL;
}



void* ooCapabilityCreateDTMFCapability(int cap, OOCTXT *pctxt)
{
   H245AudioTelephonyEventCapability *pATECap=NULL;
   H245UserInputCapability *userInput = NULL;
   char *events=NULL;
   switch(cap)
   {
   case OO_CAP_DTMF_RFC2833:
      pATECap = (H245AudioTelephonyEventCapability*)memAlloc(pctxt, 
                                   sizeof(H245AudioTelephonyEventCapability));
      if(!pATECap)
      {
         OOTRACEERR1("Error:Memory - ooCapabilityCreateDTMFCapability - pATECap\n");
         return NULL;
      }
      memset(pATECap, 0, sizeof(H245AudioTelephonyEventCapability));
      pATECap->dynamicRTPPayloadType = giDynamicRTPPayloadType;
      events = (char*)memAlloc(pctxt, strlen("0-16")+1);
      if(!events)
      {
         OOTRACEERR1("Error:Memory - ooCapabilityCreateDTMFCapability - events\n");
         memFreePtr(pctxt, pATECap);
         return NULL;
      }
      strncpy(events, "0-16", strlen("0-16"));
      pATECap->audioTelephoneEvent = events;
      return pATECap;
   case OO_CAP_DTMF_H245_alphanumeric:
      userInput = (H245UserInputCapability*)memAllocZ(pctxt, 
                                          sizeof(H245UserInputCapability));
      if(!userInput)
      {
         OOTRACEERR1("Error:Memory - ooCapabilityCreateDTMFCapability - "
                     "userInput\n");
         return NULL;
      }
      userInput->t = T_H245UserInputCapability_basicString;
      return userInput;
   case OO_CAP_DTMF_H245_signal:
      userInput = (H245UserInputCapability*)memAllocZ(pctxt, 
                                          sizeof(H245UserInputCapability));
      if(!userInput)
      {
         OOTRACEERR1("Error:Memory - ooCapabilityCreateDTMFCapability - "
                     "userInput\n");
         return NULL;
      }
      userInput->t = T_H245UserInputCapability_dtmf;
      return userInput;
   default:
     OOTRACEERR1("Error:unknown dtmf capability type\n");
   }
   return NULL;
}



struct H245VideoCapability* ooCapabilityCreateH263VideoCapability
   (ooH323EpCapability *epCap, OOCTXT* pctxt, int dir)
{
   H245VideoCapability *pVideo=NULL;
   OOH263CapParams *params=NULL;
   H245H263VideoCapability *pH263Cap=NULL;

   if(!epCap || !epCap->params)
   {
     OOTRACEERR1("Error:Invalid capability parameters to "
                 "ooCapabilityCreateH263VideoCapability.\n");
     return NULL;
   }
   params =(OOH263CapParams*)epCap->params;

   pVideo = (H245VideoCapability*)memAllocZ(pctxt, 
                                                  sizeof(H245VideoCapability));
   pH263Cap = (H245H263VideoCapability*) memAllocZ(pctxt, 
                                             sizeof(H245H263VideoCapability));
   if(!pVideo || !pH263Cap)
   {
      OOTRACEERR1("ERROR:Memory - ooCapabilityCreateH263VideoCapability - "
                  "pVideo/pH263Cap\n");
      return NULL;
   }

   pVideo->t = T_H245VideoCapability_h263VideoCapability;
   pVideo->u.h263VideoCapability = pH263Cap;


   if(params->picFormat ==  OO_PICFORMAT_SQCIF) {
      pH263Cap->m.sqcifMPIPresent = TRUE;
      pH263Cap->sqcifMPI = params->MPI;
   }
   else if(params->picFormat == OO_PICFORMAT_QCIF) {
      pH263Cap->m.qcifMPIPresent = TRUE;
      pH263Cap->qcifMPI = params->MPI;
   }
   else if(params->picFormat == OO_PICFORMAT_CIF) {
      pH263Cap->m.cifMPIPresent = TRUE;
      pH263Cap->cifMPI = params->MPI;
   }
   else if(params->picFormat == OO_PICFORMAT_CIF4) {
      pH263Cap->m.cif4MPIPresent  = TRUE;
      pH263Cap->cif4MPI = params->MPI;
   }
   else if(params->picFormat == OO_PICFORMAT_CIF16) {
      pH263Cap->m.cif16MPIPresent = TRUE;
      pH263Cap->cif16MPI = params->MPI;
   }

   pH263Cap->m.errorCompensationPresent = TRUE;
   pH263Cap->maxBitRate = params->maxBitRate;
   pH263Cap->unrestrictedVector = FALSE;
   pH263Cap->arithmeticCoding = FALSE;
   pH263Cap->advancedPrediction = FALSE;
   pH263Cap->pbFrames = FALSE;
   pH263Cap->temporalSpatialTradeOffCapability = FALSE;
   pH263Cap->hrd_B = 0;
   pH263Cap->bppMaxKb = 0;
   pH263Cap->slowSqcifMPI = FALSE;
   pH263Cap->slowQcifMPI = FALSE;
   pH263Cap->slowCifMPI = FALSE;
   pH263Cap->slowCif4MPI = FALSE;
   pH263Cap->slowCif16MPI = FALSE;
   pH263Cap->errorCompensation = FALSE;
   return pVideo;
}

static ASN1OBJID h241_h264 =
{
       7, /* nb de composants de l'OID */
       { 0, 0, 8, 241, 0, 0, 1 }
};


/* IVeS - ajout H.264 */
struct H245VideoCapability* ooCapabilityCreateGenericVideoCapability
   (ooH323EpCapability *epCap, OOCTXT* pctxt, int dir)
{
   H245VideoCapability *pVideo=NULL;
   OOH264CapParams *params=NULL;
   H245GenericCapability *pH264Cap=NULL;

   if(!epCap || !epCap->params)
   {
     OOTRACEERR1("Error:Invalid capability parameters to "
                 "ooCapabilityCreateGenericVideoCapability.\n");
     return NULL;
   }
   params =(OOH264CapParams*)epCap->params;

   pVideo = (H245VideoCapability*)memAllocZ(pctxt,
                                                  sizeof(H245VideoCapability));
   pH264Cap = (H245GenericCapability*) memAllocZ(pctxt,
                                             sizeof(H245GenericCapability));
   if(!pVideo || !pH264Cap)
   {
      OOTRACEERR1("ERROR:Memory - ooCapabilityCreateH264VideoCapability - "
                  "pVideo/pH264Cap\n");
      return NULL;
   }
   /* H.241 - h.264 identifier */
   pH264Cap->capabilityIdentifier.t = T_H245CapabilityIdentifier_standard;
   pH264Cap->capabilityIdentifier.u.standard = memAllocZ(pctxt, sizeof(h241_h264));
   memcpy( pH264Cap->capabilityIdentifier.u.standard, &h241_h264, sizeof(h241_h264));

   /* max bitrate */
   pH264Cap->m.maxBitRatePresent = TRUE;
   pH264Cap->maxBitRate = params->maxBitRate;

   /* Ajouter level et profils sous forme de collapsing parameters */
   pH264Cap->m.collapsingPresent = TRUE;
   dListInit(&pH264Cap->collapsing);

   H245GenericParameter * prof = memAllocZ(pctxt, sizeof(H245GenericParameter) );
   prof->parameterIdentifier.t = T_H245ParameterIdentifier_standard;
   prof->parameterIdentifier.u.standard = 41 ; /* profile */
   prof->parameterValue.t = T_H245ParameterValue_booleanArray ;

    /* normaliser profile */
   prof->parameterValue.u.booleanArray = ooProfileSIP2H241(params->profile);
   OOTRACEERR2("Adding standart parameter H.264 profile = 0x%x.\n", params->profile);
   dListAppend(pctxt, &pH264Cap->collapsing, prof);

   /* level */
   H245GenericParameter * level = memAllocZ(pctxt, sizeof(H245GenericParameter) );
   level->parameterIdentifier.t = T_H245ParameterIdentifier_standard;
   level->parameterIdentifier.u.standard = 42; /* level */
   level->parameterValue.t = T_H245ParameterValue_unsignedMin ;
   level->parameterValue.u.unsignedMin = ooLevelSIP2H241(params->level);
   OOTRACEERR2("Adding standart parameter H.264 level = 0x%x.\n", params->level);
   dListAppend(pctxt, &pH264Cap->collapsing, level);

   pVideo->t = T_H245VideoCapability_genericVideoCapability;
   pVideo->u.genericVideoCapability = pH264Cap;

   return pVideo;
}

struct H245AudioCapability* ooCapabilityCreateGSMFullRateCapability
   (ooH323EpCapability *epCap, OOCTXT* pctxt, int dir)
{
   H245AudioCapability *pAudio=NULL;
   H245GSMAudioCapability *pGSMCap=NULL;
   if(!epCap || !epCap->params)
   {
     OOTRACEERR1("Error:Invalid capability parameters to "
                 "ooCapabilityCreateGSMFullRateCapability.\n");
     return NULL;
   }

   pAudio = (H245AudioCapability*)memAlloc(pctxt, 
                                                sizeof(H245AudioCapability));
   pGSMCap = (H245GSMAudioCapability*)memAlloc(pctxt, 
                                              sizeof(H245GSMAudioCapability));
   if(!pAudio || !pGSMCap)
   {
      OOTRACEERR1("ERROR:Memory - ooCapabilityCreateGSMFullRateCapability - "
                  "pAudio/pGSMCap\n");
      return NULL;
   }
   
   pAudio->t = T_H245AudioCapability_gsmFullRate;
   pAudio->u.gsmFullRate = pGSMCap;
   if(dir & OORX)
      pGSMCap->audioUnitSize = ((OOGSMCapParams*)epCap->params)->rxframes*OO_GSMFRAMESIZE;
   else
      pGSMCap->audioUnitSize = ((OOGSMCapParams*)epCap->params)->txframes*OO_GSMFRAMESIZE;
 
   pGSMCap->comfortNoise = ((OOGSMCapParams*)epCap->params)->comfortNoise;
   pGSMCap->scrambled = ((OOGSMCapParams*)epCap->params)->scrambled;

   return pAudio;
}

/* This is used for g711 ulaw/alaw, g728, g729, g729A, g7231*/
struct H245AudioCapability* ooCapabilityCreateSimpleCapability
   (ooH323EpCapability *epCap, OOCTXT* pctxt, int dir)
{
   H245AudioCapability *pAudio=NULL;
   OOCapParams *params;
   if(!epCap || !epCap->params)
   {
     OOTRACEERR1("Error:Invalid capability parameters to "
                 "ooCapabilityCreateSimpleCapability.\n");
     return NULL;
   }
   params =(OOCapParams*)epCap->params;
   pAudio = (H245AudioCapability*)memAlloc(pctxt, 
                                                sizeof(H245AudioCapability));
   if(!pAudio)
   {
      OOTRACEERR1("ERROR:Memory - ooCapabilityCreateSimpleCapability - pAudio\n");
      return NULL;
   }

   
   switch(epCap->cap)
   {
   case OO_G711ALAW64K:
      pAudio->t = T_H245AudioCapability_g711Alaw64k;
      if(dir & OORX)
         pAudio->u.g711Alaw64k = params->rxframes;
      else
         pAudio->u.g711Alaw64k = params->txframes;
      
      return pAudio;
   case OO_G711ULAW64K:
      pAudio->t = T_H245AudioCapability_g711Ulaw64k;
      if(dir & OORX)
         pAudio->u.g711Ulaw64k = params->rxframes;
      else
         pAudio->u.g711Ulaw64k = params->txframes;
      return pAudio;

   case OO_G711ALAW56K:
      pAudio->t = T_H245AudioCapability_g711Alaw56k;
      if(dir & OORX)
         pAudio->u.g711Alaw56k = params->rxframes;
      else
         pAudio->u.g711Alaw56k = params->txframes; 
      return pAudio;

   case OO_G711ULAW56K:
      pAudio->t = T_H245AudioCapability_g711Ulaw56k;
      if(dir & OORX)
         pAudio->u.g711Ulaw56k = params->rxframes;
      else
         pAudio->u.g711Ulaw64k = params->txframes;
      return pAudio;
   /*case OO_G726:
      pAudio->t = T_H245AudioCapability_g726;
      if(dir & OORX)
         pAudio->u.g726 = params->rxframes;
      else
         pAudio->u.g726 = params->txframes;
      return pAudio;*/
   case OO_G728:
      pAudio->t = T_H245AudioCapability_g728;
      if(dir & OORX)
         pAudio->u.g728 = params->rxframes;
      else
         pAudio->u.g728 = params->txframes;
      return pAudio;
   case OO_G729A:
      pAudio->t = T_H245AudioCapability_g729AnnexA;
      if(dir & OORX)
         pAudio->u.g729AnnexA = params->rxframes;
      else
         pAudio->u.g729AnnexA = params->txframes;
      return pAudio;

   case OO_G729:
      pAudio->t = T_H245AudioCapability_g729;
      if(dir & OORX)
         pAudio->u.g729 = params->rxframes;
      else
         pAudio->u.g729 = params->txframes;
      return pAudio;

   case OO_G7231:
      pAudio->t = T_H245AudioCapability_g7231;
      pAudio->u.g7231 = (H245AudioCapability_g7231*)memAlloc(pctxt, 
                                           sizeof(H245AudioCapability_g7231));
      if(!pAudio->u.g7231)
      {
         OOTRACEERR1("Error:Memory - ooCapabilityCreateSimpleCapability - g7231\n");
         memFreePtr(pctxt, pAudio);
         return NULL;
      }
      pAudio->u.g7231->silenceSuppression = params->silenceSuppression;
      if(dir & OORX)
         pAudio->u.g7231->maxAl_sduAudioFrames = params->rxframes;
      else
         pAudio->u.g7231->maxAl_sduAudioFrames = params->txframes;
      return pAudio;

   default:
      OOTRACEERR2("ERROR: Don't know how to create audio capability %d\n",
                   epCap->cap);
   }
   return NULL;
}

/* Used for g711 ulaw/alaw, g728, g729, g729a, g7231 */
ASN1BOOL ooCapabilityCheckCompatibility_Simple
   (OOH323CallData *call, ooH323EpCapability* epCap, 
    H245AudioCapability* audioCap, int dir)
{
   int noofframes=0, cap;

   OOTRACEDBGC2("Comparing channel with codec type: %d\n", audioCap->t);

   switch(audioCap->t)
   {
   case T_H245AudioCapability_g711Ulaw56k:
      cap = OO_G711ULAW56K;
      noofframes = audioCap->u.g711Ulaw56k;
      break;
   case T_H245AudioCapability_g711Ulaw64k:
      cap = OO_G711ULAW64K;
      noofframes = audioCap->u.g711Ulaw64k;
      break;
   case T_H245AudioCapability_g711Alaw64k:
      cap = OO_G711ALAW64K;
      noofframes = audioCap->u.g711Alaw64k;
      break;
   case T_H245AudioCapability_g711Alaw56k:
      cap = OO_G711ALAW56K;
      noofframes = audioCap->u.g711Alaw56k;
      break;
   /*case T_H245AudioCapability_g726:
      cap = OO_G726;
      noofframes = audioCap->u.g726;
      break;*/
   case T_H245AudioCapability_g728:
      cap = OO_G728;
      noofframes = audioCap->u.g728;
      break;
   case T_H245AudioCapability_g729:
      cap = OO_G729;
      noofframes = audioCap->u.g729;
      break;
   case T_H245AudioCapability_g729AnnexA:
      cap = OO_G729A;
      noofframes = audioCap->u.g729AnnexA;
      break;   
   case T_H245AudioCapability_g7231:
     cap = OO_G7231;
     noofframes = audioCap->u.g7231->maxAl_sduAudioFrames;
     break;
   default:
      return FALSE;
   }

   OOTRACEDBGC3("Comparing codecs: current=%d, requested=%d\n", 
      epCap->cap, cap);
   if(cap != epCap->cap) { return FALSE; }

   /* Can we receive this capability */
   if(dir & OORX)
   {
      OOTRACEDBGC3("Comparing RX frame rate: channel's=%d, requested=%d\n",
         ((OOCapParams*)epCap->params)->rxframes, noofframes);
      if(((OOCapParams*)epCap->params)->rxframes >= noofframes) {
         return TRUE;
      }
      //else {
      //  not supported, as already told other ep our max. receive rate
      //  our ep can't receive more rate than it
      //  return FALSE;
      //}
   }

   /* Can we transmit compatible stream */
   if(dir & OOTX)
   {
      OOTRACEDBGC3("Comparing TX frame rate: channel's=%d, requested=%d\n",
         ((OOCapParams*)epCap->params)->txframes, noofframes);
      if(((OOCapParams*)epCap->params)->txframes <= noofframes) {
         return TRUE;
      }
      //else {
      //   TODO: reduce our ep transmission rate, as peer EP has low receive
      //   cap, than return TRUE
      //}
   }
   return FALSE;

}


OOBOOL ooCapabilityCheckCompatibility_GSM
   (OOH323CallData *call, ooH323EpCapability* epCap, 
    H245AudioCapability* audioCap, int dir)
{
   unsigned noofframes=0, cap;
   switch(audioCap->t)
   {
   case T_H245AudioCapability_gsmFullRate:
      cap = OO_GSMFULLRATE;
      noofframes = (audioCap->u.gsmFullRate->audioUnitSize)/OO_GSMFRAMESIZE;
      break;
   case T_H245AudioCapability_gsmHalfRate:
      cap = OO_GSMHALFRATE;
      noofframes = (audioCap->u.gsmHalfRate->audioUnitSize)/OO_GSMFRAMESIZE;
      break;
   case T_H245AudioCapability_gsmEnhancedFullRate:
      cap = OO_GSMENHANCEDFULLRATE;
      noofframes = (audioCap->u.gsmEnhancedFullRate->audioUnitSize)/OO_GSMFRAMESIZE;
      break;
   default:
      return FALSE;
   }

   /* can we receive this capability */
   if(dir & OORX)
   {
      if(((OOGSMCapParams*)epCap->params)->rxframes >= noofframes)
         return TRUE;
   }

   /* Make sure we transmit compatible stream */
   if(dir & OOTX)
   {
      if(((OOGSMCapParams*)epCap->params)->txframes > noofframes){
         OOTRACEDBGA5("Reducing txframes for GSM from %d to %d to match "
                      "receive capability of remote end.(%s, %s)\n", 
                     ((OOGSMCapParams*)epCap->params)->txframes, noofframes, 
                     call->callType, call->callToken);
         ((OOGSMCapParams*)epCap->params)->txframes = noofframes;
      }
      return TRUE;
   }
   return FALSE;

}


OOBOOL ooCapabilityCheckCompatibility_H263Video
   (struct OOH323CallData *call, ooH323EpCapability *epCap, 
    H245VideoCapability *pVideoCap, int dir)
{
   H245H263VideoCapability *pH263Cap = NULL;

   OOH263CapParams *params = epCap->params;
   if(!pVideoCap->u.h263VideoCapability)  
   {
      OOTRACEERR3("Error:No H263 video capability present in video capability"
                 "structure. (%s, %s)\n", call->callType, call->callToken);
      return FALSE;
   }
   pH263Cap = pVideoCap->u.h263VideoCapability;
   
   /* can we receive/transmit this capability */
   if(OORX & dir)
   {
      if(pH263Cap->m.sqcifMPIPresent)
      {
         if(params->picFormat != OO_PICFORMAT_SQCIF)
         {
            return FALSE;
         }
         else{
            if(pH263Cap->sqcifMPI >= params->MPI)
               return TRUE;
            else
               return FALSE;
         }
      }
      if(pH263Cap->m.qcifMPIPresent)
      {
         if(params->picFormat != OO_PICFORMAT_QCIF)
         {
            return FALSE;
         }
         else{
            if(pH263Cap->qcifMPI >= params->MPI)
               return TRUE;
            else
               return FALSE;
         }
      }
      if(pH263Cap->m.cifMPIPresent)
      {
         if(params->picFormat != OO_PICFORMAT_CIF)
         {
            return FALSE;
         }
         else{
            if(pH263Cap->cifMPI >= params->MPI)
               return TRUE;
            else
               return FALSE;
         }
      }
      if(pH263Cap->m.cif4MPIPresent)
      {
         if(params->picFormat != OO_PICFORMAT_CIF4)
         {
            return FALSE;
         }
         else{
            if(pH263Cap->cif4MPI >= params->MPI)
               return TRUE;
            else
               return FALSE;
         }
      }
      if(pH263Cap->m.cif16MPIPresent)
      {
         if(params->picFormat != OO_PICFORMAT_CIF16)
         {
            return FALSE;
         }
         else{
            if(pH263Cap->cif16MPI >= params->MPI)
               return TRUE;
            else
               return FALSE;
         }
      }
   }

   /* Can we transmit */
   if(OOTX & dir)
   {
       if(pH263Cap->m.sqcifMPIPresent)
      {
         if(params->picFormat != OO_PICFORMAT_SQCIF)
         {
            return FALSE;
         }
         else{
            if(pH263Cap->sqcifMPI <= params->MPI)
               return TRUE;
            else
               return FALSE;
         }
      }
      if(pH263Cap->m.qcifMPIPresent)
      {
         if(params->picFormat != OO_PICFORMAT_QCIF)
         {
            return FALSE;
         }
         else{
            if(pH263Cap->qcifMPI <= params->MPI)
               return TRUE;
            else
               return FALSE;
         }
      }
      if(pH263Cap->m.cifMPIPresent)
      {
         if(params->picFormat != OO_PICFORMAT_CIF)
         {
            return FALSE;
         }
         else{
            if(pH263Cap->cifMPI <= params->MPI)
               return TRUE;
            else
               return FALSE;
         }
      }
      if(pH263Cap->m.cif4MPIPresent)
      {
         if(params->picFormat != OO_PICFORMAT_CIF4)
         {
            return FALSE;
         }
         else{
            if(pH263Cap->cif4MPI <= params->MPI)
               return TRUE;
            else
               return FALSE;
         }
      }
      if(pH263Cap->m.cif16MPIPresent)
      {
         if(params->picFormat != OO_PICFORMAT_CIF16)
         {
            return FALSE;
         }
         else{
            if(pH263Cap->cif16MPI <= params->MPI)
               return TRUE;
            else
               return FALSE;
         }
      }
   }
   
   return FALSE;

}

/* IVeS - ajout H.264 */
OOBOOL ooCapabilityCheckCompatibility_H264Video(struct OOH323CallData *call, ooH323EpCapability *epCap,
    H245VideoCapability *pVideoCap, int dir)
{
   H245GenericCapability *pH264Cap = NULL;

   OOH264CapParams *params = epCap->params;

   if (!pVideoCap->u.genericVideoCapability )
   {
      OOTRACEERR3("Error:No H26r43 video capability present in video capability"
                 "structure. (%s, %s)\n", call->callType, call->callToken);
      return FALSE;
   }

   pH264Cap = pVideoCap->u.genericVideoCapability;

   /* IVeS - comparer les OID */
   if ( pH264Cap->capabilityIdentifier.t == T_H245CapabilityIdentifier_standard
        && 
        ooOidCompare(&h241_h264, pH264Cap->capabilityIdentifier.u.standard ))
   {
       /* ici, on devrait comparer profile-level avec ceux passes
        * dans epCap
        */

       return TRUE; 
   }
}

OOBOOL ooCapabilityCheckCompatibility_Audio
   (OOH323CallData *call, ooH323EpCapability* epCap, 
    H245AudioCapability* audioCap, int dir)
{

   switch(audioCap->t)
   {
   case T_H245AudioCapability_g711Ulaw56k:
   case T_H245AudioCapability_g711Ulaw64k:
   case T_H245AudioCapability_g711Alaw64k:
   case T_H245AudioCapability_g711Alaw56k:
   /*case T_H245AudioCapability_g726:*/
   case T_H245AudioCapability_g728:
   case T_H245AudioCapability_g729:
   case T_H245AudioCapability_g729AnnexA:
   case T_H245AudioCapability_g7231:
      return ooCapabilityCheckCompatibility_Simple(call, epCap, audioCap, dir);
   case T_H245AudioCapability_gsmFullRate:
      return ooCapabilityCheckCompatibility_GSM(call, epCap, audioCap, dir);
   default:
      return FALSE;
   }

   return FALSE;  
}

OOBOOL ooCapabilityCheckCompatibility_Video
   (OOH323CallData *call, ooH323EpCapability* epCap, 
    H245VideoCapability* videoCap, int dir)
{
   switch(videoCap->t)
   {
   case T_H245VideoCapability_h263VideoCapability:
      return ooCapabilityCheckCompatibility_H263Video(call, epCap, 
                                                              videoCap, dir);
   case T_H245VideoCapability_genericVideoCapability:
      return ooCapabilityCheckCompatibility_H264Video(call, epCap, 
                                                              videoCap, dir);
   default:
     OOTRACEDBGC3("ooCapabilityCheckCompatibility_Video - Unsupported video "
                  "capability. (%s, $s)\n", call->callType, call->callToken);
   }
   return FALSE;
}
/*
   Note: In faststart if we sent transmit rate (x>y) and remote
         can receive only y, then we can't reduce our transmit rate
*/
OOBOOL ooCapabilityCheckCompatibility
   (struct OOH323CallData *call, ooH323EpCapability* epCap, 
    H245DataType* dataType, int dir)
{
   switch(dataType->t)
   {
   case T_H245DataType_audioData:
      if(epCap->capType == OO_CAP_TYPE_AUDIO)
         return ooCapabilityCheckCompatibility_Audio(call, epCap, 
                                                   dataType->u.audioData, dir);
      break;
   case T_H245DataType_videoData:
      if(epCap->capType == OO_CAP_TYPE_VIDEO)
         return ooCapabilityCheckCompatibility_Video(call, epCap, 
                                                   dataType->u.videoData, dir);
      break;
   case T_H245DataType_data:
   default:
      OOTRACEDBGC3("ooCapabilityCheckCompatibility - Unsupported  "
                  "capability. (%s, $s)\n", call->callType, call->callToken);
   }

   return FALSE;
}

#if 0
/**
  TODO: If txCap is local and number of txframes is greater than remote can
  receive, we should automatically decrease it. And logical channel cap should
  be the one where it should be decreased. The start logical channel will
  indicate the application that it is supposed to tx and a reduced rate.
 */
ASN1BOOL ooCheckCompatibility
   (OOH323CallData *call, ooH323EpCapability *txCap, ooH323EpCapability *rxCap)
{

   if(txCap->cap != rxCap->cap) return FALSE;

   if(!(txCap->dir & OOTX)) return FALSE;
   
   if(!(rxCap->dir & OORX)) return FALSE;

   switch(txCap->cap)
   {   
   case OO_G711ALAW64K:
   case OO_G711ALAW56K:
   case OO_G711ULAW64K:
   case OO_G711ULAW56K:
   /*case OO_G726:*/
   case OO_G728:
   case OO_G729:
   case OO_G729A:
   case OO_G7231:
     if(((OOCapParams*)txCap->params)->txframes <= 
                                ((OOCapParams*)rxCap->params)->rxframes)
       return TRUE;
     else{
       OOTRACEDBGA4("Simple caps %s are not compatible.(%s, %s)\n", 
                    ooGetCapTypeText(txCap->cap), call->callType, 
                    call->callToken);
       return FALSE;
     }
   case OO_GSMFULLRATE:
   case OO_GSMHALFRATE:
   case OO_GSMENHANCEDFULLRATE:
     if(((OOGSMCapParams*)txCap->params)->txframes <= 
                                ((OOGSMCapParams*)rxCap->params)->rxframes)
       return TRUE;
     else{
       OOTRACEDBGA3("GSM caps are not compatible. (%s, %s)\n", call->callType,
                     call->callToken);
       return FALSE;
     }  
   default:
     OOTRACEWARN3("WARN: Unsupported capabilities being compared. (%s, %s)\n",
                   call->callType, call->callToken);
   }
   return FALSE;

}

#endif

ooH323EpCapability* ooIsAudioDataTypeGSMSupported
   (OOH323CallData *call, H245AudioCapability* audioCap, int dir)
{
   unsigned framesPerPkt=0;
   int cap=0;
   ooH323EpCapability *cur = NULL, *epCap=NULL;
   OOGSMCapParams *params = NULL;

   switch(audioCap->t)
   {
   case T_H245AudioCapability_gsmFullRate:
      framesPerPkt = (audioCap->u.gsmFullRate->audioUnitSize)/OO_GSMFRAMESIZE;
      cap = OO_GSMFULLRATE;
      break;
   case T_H245AudioCapability_gsmHalfRate:
      framesPerPkt = (audioCap->u.gsmHalfRate->audioUnitSize)/OO_GSMFRAMESIZE;
      cap = OO_GSMHALFRATE;
      break;
   case T_H245AudioCapability_gsmEnhancedFullRate:
      framesPerPkt = (audioCap->u.gsmEnhancedFullRate->audioUnitSize)/OO_GSMFRAMESIZE;
      cap = OO_GSMENHANCEDFULLRATE;
      break;
   default:
      OOTRACEERR3("Error:Invalid GSM capability type.(%s, %s)\n", 
                                           call->callType, call->callToken);
     return NULL;
   }

   OOTRACEDBGC4("Determined audio data type to be of type %d. Searching"
                " for matching capability.(%s, %s)\n", cap, call->callType, 
                call->callToken);

   /* If we have call specific caps then we use them, otherwise we use
      general endpoint caps*/
   if(call->ourCaps)
   {
     if (call->masterSlaveState==OO_MasterSlave_Master )
     {
       if ( ooH245GetCheckOLC() && cap  < OO_CAP_VIDEO_BASE && call->localAudioChoice )
       {
         OOTRACEAST(OOTRCLVLDBGA,"ooIsAudioDataTypeGSMSupported Using Audio OLC caps  \n");
         cur = call->localAudioChoice ;
       }
       else
       {
         OOTRACEAST(OOTRCLVLDBGA,"ooIsAudioDataTypeGSMSupported Using call caps  \n");
         cur = call->ourCaps;
       }
     }
     else
     {
       OOTRACEAST(OOTRCLVLDBGA,"ooIsAudioDataTypeGSMSupported Using call caps  \n");
       cur = call->ourCaps;
     }
   }
   else
   {
     OOTRACEAST(OOTRCLVLDBGA,"ooIsAudioDataTypeGSMSupported Using global caps.\n");
     cur = gH323ep.myCaps;
   }

   while(cur)
   {
      OOTRACEDBGC4("Local cap being compared %d. (%s, %s)\n", cur->cap,
                     call->callType, call->callToken);
      
      if(cur->cap == cap && (cur->dir & dir))
         break;
      cur = cur->next;
   }
   
   if(!cur) return NULL;
   
   OOTRACEDBGC4("Found matching audio capability type %d. Comparing"
                " other parameters. (%s, %s)\n", cap, call->callType, 
                call->callToken);
   
   /* can we receive this capability */
   if(dir & OORX)
   {
      if(((OOGSMCapParams*)cur->params)->rxframes < framesPerPkt)
         return NULL;
      else{
         epCap = (ooH323EpCapability*)memAlloc(call->pctxt, 
                                                 sizeof(ooH323EpCapability));
         params =(OOGSMCapParams*)memAlloc(call->pctxt,sizeof(OOGSMCapParams));
         if(!epCap || !params)
         {
            OOTRACEERR3("Error:Memory - ooIsAudioDataTypeGSMSupported - "
                        "epCap/params (%s, %s)\n", call->callType, 
                        call->callToken);
            return NULL;
         }
         epCap->params = params;
         epCap->cap = cur->cap;
         epCap->dir = cur->dir;
         epCap->capType = cur->capType;
         epCap->startReceiveChannel = cur->startReceiveChannel;
         epCap->startTransmitChannel= cur->startTransmitChannel;
         epCap->stopReceiveChannel = cur->stopReceiveChannel;
         epCap->stopTransmitChannel = cur->stopTransmitChannel;
         epCap->next = NULL;
         memcpy(epCap->params, cur->params, sizeof(OOGSMCapParams));
         return epCap;
      }
   }

   /* Can we transmit compatible stream */
   if(dir & OOTX)
   {
      epCap = (ooH323EpCapability*)memAlloc(call->pctxt, 
                                                sizeof(ooH323EpCapability));
      params =(OOGSMCapParams*)memAlloc(call->pctxt,sizeof(OOGSMCapParams));
      if(!epCap || !params)
      {
         OOTRACEERR3("Error:Memory - ooIsAudioDataTypeGSMSupported - "
                     "epCap/params (%s, %s)\n", call->callType, 
                     call->callToken);
         return NULL;
      }
      epCap->params = params;
      epCap->cap = cur->cap;
      epCap->dir = cur->dir;
      epCap->capType = cur->capType;
      epCap->startReceiveChannel = cur->startReceiveChannel;
      epCap->startTransmitChannel= cur->startTransmitChannel;
      epCap->stopReceiveChannel = cur->stopReceiveChannel;
      epCap->stopTransmitChannel = cur->stopTransmitChannel;
      epCap->next = NULL;
      memcpy(epCap->params, cur->params, sizeof(OOGSMCapParams));
      if(params->txframes > framesPerPkt)
      {
         OOTRACEINFO5("Reducing framesPerPkt for transmission of GSM "
                      "capability from %d to %d to match receive capability of"
                      " remote endpoint.(%s, %s)\n", params->txframes, 
                      framesPerPkt, call->callType, call->callToken);
         params->txframes = framesPerPkt;
      }

      return epCap;

   }
   return NULL;

}

/* used for g711 ulaw/alaw, g728, g729, g729a, g7231 */
ooH323EpCapability* ooIsAudioDataTypeSimpleSupported
   (OOH323CallData *call, H245AudioCapability* audioCap, int dir)
{
   int cap, framesPerPkt=0;
   ooH323EpCapability *cur=NULL, *epCap=NULL;
   OOCapParams * params= NULL;

   /* Find similar capability */
   switch(audioCap->t)
   {
      case T_H245AudioCapability_g711Alaw64k:
         framesPerPkt = audioCap->u.g711Alaw64k;
         cap = OO_G711ALAW64K;
         break;
      case T_H245AudioCapability_g711Alaw56k:
         framesPerPkt = audioCap->u.g711Alaw56k;
         cap = OO_G711ALAW56K;
         break;
      case T_H245AudioCapability_g711Ulaw56k:
         framesPerPkt = audioCap->u.g711Ulaw56k;
         cap = OO_G711ULAW56K;
         break;
      case T_H245AudioCapability_g711Ulaw64k:
         framesPerPkt = audioCap->u.g711Ulaw64k;
         cap = OO_G711ULAW64K;
         break;

/*      case T_H245AudioCapability_g726:
         framesPerPkt = audioCap->u.g726;
         cap = OO_G726;
         break;
*/
      case T_H245AudioCapability_g728:
         framesPerPkt = audioCap->u.g728;
         cap = OO_G728;
         break;

      case T_H245AudioCapability_g729:
         framesPerPkt = audioCap->u.g729;
         cap = OO_G729;
         break;
      case T_H245AudioCapability_g729AnnexA:
         framesPerPkt = audioCap->u.g729AnnexA;
         cap = OO_G729A;
         break;
      case T_H245AudioCapability_g7231:
         framesPerPkt = audioCap->u.g7231->maxAl_sduAudioFrames;
         cap = OO_G7231;
         break;
      default:
         return NULL;
   }

   OOTRACEDBGC4("Determined Simple audio data type to be of type %s. Searching"
                " for matching capability.(%s, %s)\n", 
                ooGetCapTypeText(cap), call->callType, call->callToken);

   /* If we have call specific caps, we use them; otherwise use general
      endpoint caps
   */   
   if(call->ourCaps)
   {
     if (call->masterSlaveState==OO_MasterSlave_Master )
     {
       if ( ooH245GetCheckOLC() && cap  < OO_CAP_VIDEO_BASE && call->localAudioChoice )
       {
         OOTRACEAST(OOTRCLVLDBGA,"ooIsAudioDataTypeGSMSupported Using Audio OLC caps  \n");
         cur = call->localAudioChoice ;
       }
       else
       {
         OOTRACEAST(OOTRCLVLDBGA,"ooIsAudioDataTypeGSMSupported Using call caps  \n");
         cur = call->ourCaps;
       }
     }
     else
     {
       OOTRACEAST(OOTRCLVLDBGA,"ooIsAudioDataTypeGSMSupported Using call caps  \n");
       cur = call->ourCaps;
     }
   }
   else
   {
     OOTRACEAST(OOTRCLVLDBGA,"ooIsAudioDataTypeGSMSupported Using global caps.\n");
     cur = gH323ep.myCaps;
   }

   while(cur)
   {
     OOTRACEAST(OOTRCLVLDBGA,"Local cap being compared %s. (%s, %s)\n", 
                ooGetCapTypeText(cur->cap),call->callType, call->callToken);
      
      if(cur->cap == cap && (cur->dir & dir))
         break;
      cur = cur->next;
   }
   
   if(!cur) 
   {
     OOTRACEAST(OOTRCLVLDBGA,"ooIsAudioDataTypeSimpleSupported return NULL\n");
     return NULL ;
   }
   
   OOTRACEAST(OOTRCLVLDBGA,"Found matching simple audio capability type "
              "%s. Comparing other parameters. (%s, %s)\n",
              ooGetCapTypeText(cap), 
              call->callType, call->callToken);
   
   /* can we receive this capability */
   if(dir & OORX)
   {
     if(((OOCapParams*)cur->params)->rxframes < framesPerPkt)
         return NULL;
     else{
        OOTRACEAST(OOTRCLVLDBGA,"We can receive Simple capability %s. "
                   "(%s, %s)\n", 
                   ooGetCapTypeText(cur->cap), call->callType, 
                   call->callToken);
        epCap = (ooH323EpCapability*)memAlloc(call->pctxt, 
                                                 sizeof(ooH323EpCapability));
        params=(OOCapParams*)memAlloc(call->pctxt,sizeof(OOCapParams));
        if(!epCap || !params)
        {
           OOTRACEERR3("Error:Memory - ooIsAudioDataTypeSimpleSupported - "
                       "epCap/params (%s, %s)\n", call->callType, 
                       call->callToken);
           return NULL;
        }
        epCap->params = params;
        epCap->cap = cur->cap;
        epCap->dir = cur->dir;
        epCap->capType = cur->capType;
        epCap->startReceiveChannel = cur->startReceiveChannel;
        epCap->startTransmitChannel= cur->startTransmitChannel;
        epCap->stopReceiveChannel = cur->stopReceiveChannel;
        epCap->stopTransmitChannel = cur->stopTransmitChannel;
        epCap->next = NULL;
        memcpy(epCap->params, cur->params, sizeof(OOCapParams));
        OOTRACEAST(OOTRCLVLDBGA,"Returning copy of matched receive"
                   " capability %s. "
                   "(%s, %s)\n", 
                   ooGetCapTypeText(cur->cap), call->callType, 
                   call->callToken);
        return epCap;
     }
   }

   /* Can we transmit compatible stream */
   if(dir & OOTX)
   {
      OOTRACEAST(OOTRCLVLDBGA,"We can transmit Simple capability "
                 "%s. (%s, %s)\n", 
                 ooGetCapTypeText(cur->cap), call->callType, 
                 call->callToken);
      epCap = (ooH323EpCapability*)memAlloc(call->pctxt, 
                                                sizeof(ooH323EpCapability));
      params =(OOCapParams*)memAlloc(call->pctxt,sizeof(OOCapParams));
      if(!epCap || !params)
      {
         OOTRACEERR3("Error:Memory - ooIsAudioDataTypeSimpleSupported - "
                     "epCap/params (%s, %s)\n", call->callType, 
                     call->callToken);
         return NULL;
      }
      epCap->params = params;
      epCap->cap = cur->cap;
      epCap->dir = cur->dir;
      epCap->capType = cur->capType;
      epCap->startReceiveChannel = cur->startReceiveChannel;
      epCap->startTransmitChannel= cur->startTransmitChannel;
      epCap->stopReceiveChannel = cur->stopReceiveChannel;
      epCap->stopTransmitChannel = cur->stopTransmitChannel;
      epCap->next = NULL;
      memcpy(epCap->params, cur->params, sizeof(OOCapParams));
      if(params->txframes > framesPerPkt)
      {
         OOTRACEINFO5("Reducing framesPerPkt for transmission of Simple "
                      "capability from %d to %d to match receive capability of"
                      " remote endpoint.(%s, %s)\n", params->txframes, 
                      framesPerPkt, call->callType, call->callToken);
         params->txframes = framesPerPkt;
      }
      OOTRACEAST(OOTRCLVLDBGA,"Returning copy of matched transmit "
                 "capability %s."
                 "(%s, %s)\n", 
                 ooGetCapTypeText(cur->cap), call->callType, 
                 call->callToken);
      return epCap;
   }
   return NULL;
}



ooH323EpCapability* ooIsAudioDataTypeSupported
   (OOH323CallData *call, H245AudioCapability* audioCap, int dir)
{
   /* Find similar capability */
   switch(audioCap->t)
   {
      case T_H245AudioCapability_g711Alaw64k:
      case T_H245AudioCapability_g711Alaw56k:
      case T_H245AudioCapability_g711Ulaw56k:
      case T_H245AudioCapability_g711Ulaw64k:
      /*case T_H245AudioCapability_g726:*/
      case T_H245AudioCapability_g728:
      case T_H245AudioCapability_g729:
      case T_H245AudioCapability_g729AnnexA:
      case T_H245AudioCapability_g7231:
         return ooIsAudioDataTypeSimpleSupported(call, audioCap, dir);
      case T_H245AudioCapability_gsmFullRate:
      case T_H245AudioCapability_gsmHalfRate:
      case T_H245AudioCapability_gsmEnhancedFullRate:
         return ooIsAudioDataTypeGSMSupported(call, audioCap, dir);   
      default:
         return NULL;
   }   
}


ooH323EpCapability* ooIsVideoDataTypeH263Supported
   (OOH323CallData *call, H245H263VideoCapability* pH263Cap, int dir, 
    OOPictureFormat picFormat)
{
   int cap;
   ooH323EpCapability *cur=NULL, *epCap=NULL;
   ooH323EpCapability *best=NULL ;
   OOH263CapParams *params= NULL;   
   char *pictureType=NULL;
   unsigned mpi=0;
   cap = OO_H263VIDEO;

   if(picFormat == OO_PICFORMAT_SQCIF && pH263Cap->m.sqcifMPIPresent)
   {
      pictureType = "SQCIF";
      mpi = pH263Cap->sqcifMPI;
   }
   if(picFormat == OO_PICFORMAT_QCIF && pH263Cap->m.qcifMPIPresent)
   {
      pictureType = "QCIF";
      mpi = pH263Cap->qcifMPI;
   }
   if(picFormat == OO_PICFORMAT_CIF && pH263Cap->m.cifMPIPresent)
   {
      pictureType = "CIF";
      mpi = pH263Cap->cifMPI;
   }
   if(picFormat == OO_PICFORMAT_CIF4 && pH263Cap->m.cif4MPIPresent)
   {
      pictureType = "CIF4";
      mpi = pH263Cap->cif4MPI;
   }
   if(picFormat == OO_PICFORMAT_CIF16 && pH263Cap->m.cif16MPIPresent)
   {
      pictureType = "CIF16";
      mpi = pH263Cap->cif16MPI;
   }
   

   OOTRACEAST(OOTRCLVLDBGA,"Looking for H263 video capability(%s). (%s, %s)\n", 
              pictureType, call->callType, call->callToken);

  /* If we have call specific caps, we use them; otherwise use general
      endpoint caps
   */   
   if(call->ourCaps)
   {
     if (call->masterSlaveState==OO_MasterSlave_Master )
     {
       if ( ooH245GetCheckOLC() && call->localVideoChoice )
       {
         OOTRACEAST(OOTRCLVLDBGA,"ooIsAudioDataTypeGSMSupported Using Video OLC caps  \n");
         cur = call->localVideoChoice ;
       }
       else
       {
         OOTRACEAST(OOTRCLVLDBGA,"ooIsAudioDataTypeGSMSupported Using call caps  \n");
         cur = call->ourCaps;
       }
     }
     else
     {
       OOTRACEAST(OOTRCLVLDBGA,"ooIsAudioDataTypeGSMSupported Using call caps  \n");
       cur = call->ourCaps;
     }
   }
   else
   {
     OOTRACEAST(OOTRCLVLDBGA,"ooIsAudioDataTypeGSMSupported Using global caps.\n");
     cur = gH323ep.myCaps;
   }

   while(cur)
   {
      OOTRACEAST(OOTRCLVLDBGA,"Local cap being compared %s. (%s, %s)\n", 
                 ooGetCapTypeText(cur->cap),call->callType, call->callToken);
      
      if(cur->cap == cap && (cur->dir & dir))
      {
         if(((OOH263CapParams*)cur->params)->picFormat == picFormat)
         {
            break;
         }
         else
         {
           if ( !best ) 
           {
             best = cur ;
           }
         }
      }
      cur = cur->next;
   }
   
#if 0 // Phv on rejete tt a caause du sqcif , TODO : Lost H264 !!!
   if(!cur) 
   {
     OOTRACEAST(OOTRCLVLDBGA,"ooIsVideoDataTypeH263Supported return NULL\n");
     return NULL;
   }
#else
   if(!cur ) 
   {
// PHV test tandberg h264
     int H263config = 0 ;
     if (  call->CallHaveH263  && best )
     {
       OOTRACEAST(OOTRCLVLDBGA,"ooIsVideoDataTypeH263Supported %s not found use best\n",
                  pictureType  );
       cur = best ;
       // Todo tester best !!!!
     }
     else
     {
       OOTRACEAST(OOTRCLVLDBGA,"ooIsVideoDataTypeH263Supported return NULL\n");
       return NULL;       
     }
   }
#endif

   OOTRACEAST(OOTRCLVLDBGA,"Found matching H.263 video capability type %s. Comparing"
              " other parameters. (%s, %s)\n", ooGetCapTypeText(cap), 
              call->callType, call->callToken);   
   if(dir & OORX)
   {
      if(mpi < ((OOH263CapParams*)cur->params)->MPI)
         return NULL;
      else{
         epCap = (ooH323EpCapability*)memAlloc(call->pctxt, 
                                                   sizeof(ooH323EpCapability));
         params = (OOH263CapParams*) memAlloc(call->pctxt, 
                                                      sizeof(OOH263CapParams));
         if(!epCap || !params)
         {
            OOTRACEERR3("Error:Memory - ooIsVideoDataTypeH263Supported - "
                       "epCap/params. (%s, %s)\n", call->callType, 
                        call->callToken);
            return NULL;
         }
         epCap->params = params;
         epCap->cap = cur->cap;
         epCap->dir = cur->dir;
         epCap->capType = cur->capType;
         epCap->startReceiveChannel = cur->startReceiveChannel;
         epCap->startTransmitChannel= cur->startTransmitChannel;
         epCap->stopReceiveChannel = cur->stopReceiveChannel;
         epCap->stopTransmitChannel = cur->stopTransmitChannel;
         epCap->next = NULL;
         memcpy(epCap->params, cur->params, sizeof(OOH263CapParams));
         OOTRACEAST(OOTRCLVLDBGA,"Returning copy of matched receive capability %s. "
                     "(%s, %s)\n", ooGetCapTypeText(cur->cap), call->callType, 
                     call->callToken);
         return epCap;
      }
   }
   if(dir & OOTX)
   {
      epCap = (ooH323EpCapability*)memAlloc(call->pctxt, 
                                                  sizeof(ooH323EpCapability));
      params = (OOH263CapParams*) memAlloc(call->pctxt, 
                                                      sizeof(OOH263CapParams));
      if(!epCap || !params)
      {
         OOTRACEERR3("Error:Memory - ooIsVideoDataTypeH263Supported - "
                     "epCap/params. (%s, %s)\n", call->callType, 
                     call->callToken);
         return NULL;
      }
      epCap->params = params;
      epCap->cap = cur->cap;
      epCap->dir = cur->dir;
      epCap->capType = cur->capType;
      epCap->startReceiveChannel = cur->startReceiveChannel;
      epCap->startTransmitChannel= cur->startTransmitChannel;
      epCap->stopReceiveChannel = cur->stopReceiveChannel;
      epCap->stopTransmitChannel = cur->stopTransmitChannel;
      epCap->next = NULL;
      memcpy(epCap->params, cur->params, sizeof(OOH263CapParams));
      if(params->MPI < mpi)
      {
         OOTRACEINFO5("Increasing minimum picture interval for transmission of"
                      " H263 video capability from %d to %d to match receive "
                      "capability of remote endpoint.(%s, %s)\n", params->MPI, 
                      mpi, call->callType, call->callToken);
         params->MPI = mpi;
      }
      OOTRACEAST(OOTRCLVLDBGA,"Returning copy of matched receive capability %s. "
                 "(%s, %s)\n", ooGetCapTypeText(cur->cap), call->callType, 
                 call->callToken);
      return epCap;
   }
   return NULL;

}

ooH323EpCapability *ooIsVideoDataTypeH264Supported(OOH323CallData *call, H245GenericCapability *pH264Cap, int dir)
{
  int cap;
  ooH323EpCapability *cur=NULL, *epCap=NULL;
  OOH264CapParams *params= NULL;   
  char *pictureType=NULL;
  unsigned profile, level ,maxBitRate =0 ;
  cap = OO_GENERICVIDEO;

  /* to be done check H.264 OID here */
  if ( pH264Cap->m.collapsingPresent )
  {
    //OOTRACEAST(OOTRCLVLDBGA,"ooIsVideoDataTypeH264Supported() :Find h264 profile and level \n" );
    ooExtractH264Parameters(&pH264Cap->collapsing, &profile, &level);
  }
  else
  {
    //OOTRACEAST(OOTRCLVLDBGA,"ooIsVideoDataTypeH264Supported() :fix h264 profile and level \n" );
    profile = OO_DEFAULT_H264_PROFILE;
    level = OO_DEFAULT_H264_LEVEL;
  }

  if ( pH264Cap->m.maxBitRatePresent ){
    maxBitRate = pH264Cap->maxBitRate ;
  }else{
    maxBitRate = OO_DEFAULT_H264_MAXBR ;
  }

  maxBitRate = maxBitRate / 100 ;
  OOTRACEAST(OOTRCLVLDBGA,"ooIsVideoDataTypeH264Supported() : H.264 profile[0x%x] level[0x%x] maxbr[%d] *100 kb/s(%s, %s) \n", 
             profile, level, maxBitRate, call->callType, call->callToken);
 
  /* If we have call specific caps, we use them; otherwise use general
     endpoint caps
  */   
  if (call->ourCaps)
  {
    if (call->masterSlaveState==OO_MasterSlave_Master )
    {
      if ( ooH245GetCheckOLC() &&  call->localVideoChoice )
      {
        OOTRACEAST(OOTRCLVLDBGA,"ooIsVideoDataTypeH264Supported MASTER Using Video OLC caps  \n");
        cur = call->localVideoChoice ;
      }
      else
      {
        OOTRACEAST(OOTRCLVLDBGA,"ooIsVideoDataTypeH264Supported MASTER Using call caps  \n");
        cur = call->ourCaps;
      }
    }
    else
    {
      OOTRACEAST(OOTRCLVLDBGA,"ooIsVideoDataTypeH264Supported SLAVE Using call caps  \n");
      cur = call->ourCaps;
    }
  }
  else
  {
    OOTRACEAST(OOTRCLVLDBGA,"ooIsVideoDataTypeH264Supported Using global caps.\n");
    cur = gH323ep.myCaps;
  }

  while (cur)
  {
    OOTRACEDBGC4("Local cap being compared %s. (%s, %s)\n", 
                 ooGetCapTypeText(cur->cap),call->callType, call->callToken);
      
    if(cur->cap == cap && (cur->dir & dir))
    {
      break;
    }
    cur = cur->next;
  }
   
  if (!cur)
  {
    OOTRACEDBGC1("Did not found any H.264 in local caps. Sorry. Not supported.\n");
    return NULL;
  }

  OOTRACEDBGC4("Found matching H.264 video capability type %s. Comparing"
               " other parameters. (%s, %s)\n", ooGetCapTypeText(cap), 
               call->callType, call->callToken); 
  
  if (dir & OORX)
  {
    if (profile > ((OOH264CapParams*)cur->params)->profile)
    {
      OOTRACEERR3("Error:incompatible H.264 profiles: local profile = 0x%x < remote profile = 0x%x.\n",
                  ((OOH264CapParams*)cur->params)->profile, profile );
      return NULL;
    }

    /* test level here */
    epCap = (ooH323EpCapability*) memAlloc(call->pctxt, 
                                           sizeof(ooH323EpCapability));
    params = (OOH264CapParams*) memAlloc(call->pctxt, 
                                         sizeof(OOH263CapParams));
    if (!epCap || !params)
    {
      OOTRACEERR3("Error:Memory - ooIsVideoDataTypeH264Supported - "
                  "epCap/params. (%s, %s)\n", call->callType, 
                  call->callToken);
      return NULL;
    }

    epCap->params = params;
    epCap->cap = cur->cap;
    epCap->dir = cur->dir;
    epCap->capType = cur->capType;
    epCap->startReceiveChannel = cur->startReceiveChannel;
    epCap->startTransmitChannel= cur->startTransmitChannel;
    epCap->stopReceiveChannel = cur->stopReceiveChannel;
    epCap->stopTransmitChannel = cur->stopTransmitChannel;
    epCap->next = NULL;
    memcpy(epCap->params, cur->params, sizeof(OOH264CapParams));
    OOTRACEDBGC4("Returning copy of matched receive capability %s. "
                 "(%s, %s)\n", ooGetCapTypeText(cur->cap), call->callType, 
                 call->callToken);
    return epCap;
  }

  if (dir & OOTX)
  {
    epCap = (ooH323EpCapability*)memAlloc(call->pctxt, 
                                          sizeof(ooH323EpCapability));
    params = (OOH264CapParams*) memAlloc(call->pctxt, 
                                         sizeof(OOH263CapParams));
    if (!epCap || !params)
    {
      OOTRACEERR3("Error:Memory - ooIsVideoDataTypeH264Supported - "
                  "epCap/params. (%s, %s)\n", call->callType, 
                  call->callToken);
      return NULL;
    }

    epCap->params = params;
    epCap->cap = cur->cap;
    epCap->dir = cur->dir;
    epCap->capType = cur->capType;
    epCap->startReceiveChannel = cur->startReceiveChannel;
    epCap->startTransmitChannel= cur->startTransmitChannel;
    epCap->stopReceiveChannel = cur->stopReceiveChannel;
    epCap->stopTransmitChannel = cur->stopTransmitChannel;
    epCap->next = NULL;
    memcpy(epCap->params, cur->params, sizeof(OOH264CapParams));


    OOTRACEAST(OOTRCLVLDBGA,"ooIsVideoDataTypeH264Supported remote endpoint profile[0x%x] level[0x%x] maxbr[%d] call profile[0x%x] level[0x%x] maxbr[%d] .(%s, %s)\n",
               profile, level, maxBitRate, 
               params->profile, params->level, params->maxBitRate,
               call->callType, call->callToken);



    if (params->profile > profile)
    {
      OOTRACEAST(OOTRCLVLERR,"adjusting maximum profile for transmission of"
                   " H264 video capability from 0x%x to 0x%x to match receive "
                   "capability of remote endpoint.(%s, %s)\n", params->profile, 
                   profile, call->callType, call->callToken);
      params->profile = profile;
    }

    if (params->level > level)
    {
      OOTRACEAST(OOTRCLVLERR,"adjusting maximum level for transmission of"
                   " H264 video capability from 0x%X to 0x%X to match receive "
                   "capability of remote endpoint.(%s, %s)\n", params->level, 
                   level, call->callType, call->callToken);
      params->level = level;
    }

    if (params->maxBitRate > maxBitRate )
    {
      OOTRACEAST(OOTRCLVLERR,"adjusting maximum bandwidth for transmission of"
                   " H264 video capability from %d to %d to match receive "
                   "capability of remote endpoint.(%s, %s)\n", params->maxBitRate, 
                   maxBitRate, call->callType, call->callToken);
      params->maxBitRate = maxBitRate;
    }

    OOTRACEDBGC4("Returning copy of matched receive capability %s. "
                 "(%s, %s)\n", ooGetCapTypeText(cur->cap), call->callType, 
                 call->callToken);
    return epCap;
  }
  return NULL;
}

ooH323EpCapability* ooIsVideoDataTypeSupported
(OOH323CallData *call, H245VideoCapability* pVideoCap, int dir)
{

  switch(pVideoCap->t)   
  {
    case T_H245VideoCapability_h263VideoCapability:
      if ( call->CallHaveH263 )
      {
        if(pVideoCap->u.h263VideoCapability->m.sqcifMPIPresent)
          return ooIsVideoDataTypeH263Supported(call, 
                                                pVideoCap->u.h263VideoCapability, dir, OO_PICFORMAT_SQCIF);
        else if(pVideoCap->u.h263VideoCapability->m.qcifMPIPresent)
          return ooIsVideoDataTypeH263Supported(call, 
                                                pVideoCap->u.h263VideoCapability, dir, OO_PICFORMAT_QCIF);
        else if(pVideoCap->u.h263VideoCapability->m.cifMPIPresent)
          return ooIsVideoDataTypeH263Supported(call, 
                                                pVideoCap->u.h263VideoCapability, dir, OO_PICFORMAT_CIF);
        else if(pVideoCap->u.h263VideoCapability->m.cif4MPIPresent)
          return ooIsVideoDataTypeH263Supported(call, 
                                                pVideoCap->u.h263VideoCapability, dir, OO_PICFORMAT_CIF4);
        else if(pVideoCap->u.h263VideoCapability->m.cif16MPIPresent)
          return ooIsVideoDataTypeH263Supported(call, 
                                                pVideoCap->u.h263VideoCapability, dir, OO_PICFORMAT_CIF16);
      }
      break;  

    case T_H245VideoCapability_genericVideoCapability:
      if ( call->CallHaveH264 )
      {
        /* H.264 - IVèS - check the OID recevied in the function called below */
        return ooIsVideoDataTypeH264Supported(call, pVideoCap->u.genericVideoCapability, dir );
      }
      break ;

    case T_H245VideoCapability_nonStandard:
    case T_H245VideoCapability_h261VideoCapability:
    case T_H245VideoCapability_h262VideoCapability:
    case T_H245VideoCapability_is11172VideoCapability:
    case T_H245VideoCapability_extElem1:
    default:
      OOTRACEDBGA1("Unsupported video capability type in "
                   "ooIsVideoDataTypeSupported\n");
      return NULL;
  }
  return NULL;
}

ooH323EpCapability* ooIsDataTypeSupported
(OOH323CallData *call, H245DataType *data, int dir)
{
  OOTRACEAST(OOTRCLVLDBGA,"Looking for data type support. [%d] (%s, %s)\n", 
             data->t,call->callType, 
             call->callToken);
    

  switch(data->t)
  {
    case T_H245DataType_nonStandard:
      OOTRACEERR3("NonStandard data type not supported.(%s, %s)\n", 
                  call->callType, call->callToken);
      return NULL;
    case T_H245DataType_nullData:
      OOTRACEERR3("Null data type not supported.(%s, %s)\n", 
                  call->callType, call->callToken);
      return NULL;
    case T_H245DataType_videoData:
      OOTRACEAST(OOTRCLVLDBGA,"Looking for video dataType support. (%s, %s)\n",
                 call->callType, call->callToken);
      return ooIsVideoDataTypeSupported(call, data->u.videoData, dir);
    case T_H245DataType_audioData:
      OOTRACEAST(OOTRCLVLDBGA,"Looking for audio dataType support. (%s, %s)\n",
                 call->callType, call->callToken);
      return ooIsAudioDataTypeSupported(call, data->u.audioData, dir);
    case T_H245DataType_data:
      OOTRACEERR3("Data type not supported.(%s, %s)\n", 
                  call->callType, call->callToken);
      return NULL;
    case T_H245DataType_encryptionData:
      OOTRACEERR3("Encryption data type not supported.(%s, %s)\n", 
                  call->callType, call->callToken);
      return NULL;
    case T_H245DataType_h235Control:
      OOTRACEERR3("h235 control data type not supported.(%s, %s)\n", 
                  call->callType, call->callToken);
      return NULL;
    case T_H245DataType_h235Media:
      OOTRACEERR3("h235 media type not supported.(%s, %s)\n", 
                  call->callType, call->callToken);
      return NULL;
    case T_H245DataType_multiplexedStream:
      OOTRACEERR3("Multiplexe Stream type not supported.(%s, %s)\n", 
                  call->callType, call->callToken);
      return NULL;
    default:
      OOTRACEERR3("Unknown data type not supported.(%s, %s)\n", 
                  call->callType, call->callToken);

  }
   return NULL;
}

int ooResetCapPrefs(OOH323CallData *call)
{
   OOCapPrefs *capPrefs=NULL;
   if(call)
      capPrefs = &call->capPrefs;
   else
      capPrefs = &gH323ep.capPrefs;
   memset(capPrefs, 0, sizeof(OOCapPrefs));
   return OO_OK;
}

int ooRemoveCapFromCapPrefs(OOH323CallData *call, int cap)
{
   int i=0, j=0;
   OOCapPrefs *capPrefs=NULL, oldPrefs;
   if( call )
   {
     OOTRACEAST(OOTRCLVLDBGA,"ooRemoveCapFromCapPrefs:"
                  "Remove capability %s. (%s, %s) to call \n", 
                  ooGetCapTypeText(cap), call->callType, 
                  call->callToken);
      capPrefs = &call->capPrefs;
   }
   else
   {
     OOTRACEAST(OOTRCLVLDBGA,"ooRemoveCapFromCapPrefs:"
                  "Remove capability %s. to global \n", 
                  ooGetCapTypeText(cap) );
      capPrefs = &gH323ep.capPrefs;
   }

   memcpy(&oldPrefs, capPrefs, sizeof(OOCapPrefs));
   memset(capPrefs, 0, sizeof(OOCapPrefs));
   for(i=0; i<oldPrefs.index; i++)
   {  
      if(oldPrefs.order[i] != cap) 
         capPrefs->order[j++] = oldPrefs.order[i];
   }
   capPrefs->index = j;
   return OO_OK;
}

 
int ooAppendCapToCapPrefs(OOH323CallData *call, int cap)
{
   OOCapPrefs *capPrefs=NULL;

   if(call)
   {
     OOTRACEAST(OOTRCLVLDBGA,"ooAppendCapToCapPrefs:"
                  "Adding capability %s. (%s, %s) to call \n", 
                  ooGetCapTypeText(cap), call->callType, 
                  call->callToken);
      capPrefs = &call->capPrefs;
   }
   else
   {
     OOTRACEAST(OOTRCLVLDBGA,"ooAppendCapToCapPrefs:"
                  "Adding capability %s. to global \n", 
                  ooGetCapTypeText(cap) );
      capPrefs = &gH323ep.capPrefs;
   }
   capPrefs->order[capPrefs->index++] = cap;
   return OO_OK;
}

int ooChangeCapPrefOrder(OOH323CallData *call, int cap, int pos)
{
   int i=0, j=0;
   OOCapPrefs *capPrefs = NULL;

   /* Whether to change prefs for call or for endpoint as a whole */
   if(call)
   {
     OOTRACEAST(OOTRCLVLDBGA,"ooChangeCapPrefOrder:"
                  "Change capability %s to order %d. (%s, %s) to call \n", 
                  ooGetCapTypeText(cap), pos, call->callType, 
                  call->callToken);
     capPrefs = &call->capPrefs;
   }
   else
   {
     OOTRACEAST(OOTRCLVLDBGA,"ooChangeCapPrefOrder:"
                  "Change capability %s to order %d. to global \n", 
                  ooGetCapTypeText(cap), pos );
     capPrefs = &gH323ep.capPrefs;
   }

   /* check whether cap exists, cap must exist */
   for(i=0; i<capPrefs->index; i++)
   {
      if(capPrefs->order[i] == cap)
         break;
   }
   if(i == capPrefs->index) return OO_FAILED;

   if(i==pos) return OO_OK; /* No need to change */

   /* Decrease Pref order */
   if(i < pos)
   {
      for( ; i<pos; i++) 
         capPrefs->order[i] = capPrefs->order[i+1];
      capPrefs->order[i]=cap;
      return OO_OK;
   }
   /* Increase Pref order */
   if(i>pos)
   {
     for(j=i; j>pos; j--)
       capPrefs->order[j] = capPrefs->order[j-1];
     capPrefs->order[j] = cap;
     return OO_OK;
   }

   return OO_FAILED;

}

int ooPreppendCapToCapPrefs(OOH323CallData *call, int cap)
{
   int i=0, j=0;
   OOCapPrefs *capPrefs=NULL, oldPrefs;
   if(call)
   {
     OOTRACEAST(OOTRCLVLDBGA,"ooPreppendCapToCapPrefs:"
                  "Adding capability %s. (%s, %s) to call \n", 
                  ooGetCapTypeText(cap), call->callType, 
                  call->callToken);
      capPrefs = &call->capPrefs;
   }
   else
   {
     OOTRACEAST(OOTRCLVLDBGA,"ooPreppendCapToCapPrefs:"
                  "Adding capability %s to global\n", 
                  ooGetCapTypeText(cap));
      capPrefs = &gH323ep.capPrefs;
   }

   memcpy(&oldPrefs, capPrefs, sizeof(OOCapPrefs));

 
   capPrefs->order[j++] = cap;

   for(i=0; i<oldPrefs.index; i++)
   {  
      if(oldPrefs.order[i] != cap) 
         capPrefs->order[j++] = oldPrefs.order[i];
   }
   capPrefs->index = j;
   return OO_OK;
}

       
int ooAddRemoteCapability(OOH323CallData *call, H245Capability *cap)
{
  OOTRACEAST(OOTRCLVLDBGA,"[H323/CODEC] ooAddRemoteCapability %d (%s, %s)\n",
             cap->t, call->callType, call->callToken);
   switch(cap->t)
   {
   case T_H245Capability_receiveVideoCapability:
     return ooAddRemoteVideoCapability(call, cap->u.receiveVideoCapability,
                                       OORX);
   case T_H245Capability_transmitVideoCapability:
     return ooAddRemoteVideoCapability(call, cap->u.receiveVideoCapability,
                                       OOTX);
   case T_H245Capability_receiveAndTransmitVideoCapability:
     return ooAddRemoteVideoCapability(call, cap->u.receiveVideoCapability,
                                       OORXTX);
   case T_H245Capability_receiveAudioCapability:
     return ooAddRemoteAudioCapability(call, cap->u.receiveAudioCapability, 
                                       OORX);
   case T_H245Capability_transmitAudioCapability:
     return ooAddRemoteAudioCapability(call, cap->u.transmitAudioCapability, 
                                       OOTX);
   case T_H245Capability_receiveAndTransmitAudioCapability:
     return ooAddRemoteAudioCapability(call, 
                             cap->u.receiveAndTransmitAudioCapability, OORXTX);

   case T_H245Capability_receiveAndTransmitDataApplicationCapability:
   case T_H245Capability_receiveDataApplicationCapability:
   case T_H245Capability_transmitDataApplicationCapability:
      /* IVeS -- add realtime text here */
     OOTRACEDBGA4("Unsupported cap of type DataApplication(%d). Ignoring. (%s, %s)\n", 
                   cap->t, call->callType, call->callToken);
     break;

   default:
     OOTRACEDBGA4("Unsupported cap type encountered %d. Ignoring. (%s, %s)\n", 
                   cap->t, call->callType, call->callToken);
   }
   return OO_OK;
}

int ooAddRemoteAudioCapability(OOH323CallData *call, 
                               H245AudioCapability *audioCap,
                               int dir)
{
   int rxframes=0, txframes=0;
  
   switch(audioCap->t)
   {
   case T_H245AudioCapability_g711Alaw64k:
      if(dir&OOTX) txframes = audioCap->u.g711Alaw64k;
      else if(dir&OORX) rxframes = audioCap->u.g711Alaw64k;
      else{ 
         txframes = audioCap->u.g711Alaw64k; 
         rxframes = audioCap->u.g711Alaw64k; 
      }
      return ooCapabilityAddSimpleCapability(call, OO_G711ALAW64K, txframes, 
                            rxframes, FALSE, dir, NULL, NULL, NULL, NULL,TRUE);
   case T_H245AudioCapability_g711Alaw56k:
      if(dir&OOTX) txframes = audioCap->u.g711Alaw56k;
      else if(dir&OORX) rxframes = audioCap->u.g711Alaw56k;
      else{ 
         txframes = audioCap->u.g711Alaw56k; 
         rxframes = audioCap->u.g711Alaw56k; 
      }
      return ooCapabilityAddSimpleCapability(call, OO_G711ALAW56K, txframes, 
                           rxframes, FALSE, dir, NULL, NULL, NULL, NULL, TRUE);
   case T_H245AudioCapability_g711Ulaw64k:
      if(dir&OOTX) txframes = audioCap->u.g711Ulaw64k;
      else if(dir&OORX) rxframes = audioCap->u.g711Ulaw64k;
      else{ 
         txframes = audioCap->u.g711Ulaw64k; 
         rxframes = audioCap->u.g711Ulaw64k; 
      }
      return ooCapabilityAddSimpleCapability(call, OO_G711ULAW64K, txframes, 
                           rxframes, FALSE, dir, NULL, NULL, NULL, NULL, TRUE);
   case T_H245AudioCapability_g711Ulaw56k:
      if(dir&OOTX) txframes = audioCap->u.g711Ulaw56k;
      else if(dir&OORX) rxframes = audioCap->u.g711Ulaw56k;
      else{ 
         txframes = audioCap->u.g711Ulaw56k; 
         rxframes = audioCap->u.g711Ulaw56k; 
      }
      return ooCapabilityAddSimpleCapability(call, OO_G711ULAW56K, txframes, 
                           rxframes, FALSE, dir, NULL, NULL, NULL, NULL, TRUE);

/*   case T_H245AudioCapability_g726:
      if(dir&OOTX) txframes = audioCap->u.g726;
      else if(dir&OORX) rxframes = audioCap->u.g726;
      else{ 
         txframes = audioCap->u.g726; 
         rxframes = audioCap->u.g726; 
      }
      return ooCapabilityAddSimpleCapability(call, OO_G726, txframes, 
                           rxframes, FALSE, dir, NULL, NULL, NULL, NULL, TRUE);
*/
   case T_H245AudioCapability_g728:
      if(dir&OOTX) txframes = audioCap->u.g728;
      else if(dir&OORX) rxframes = audioCap->u.g728;
      else{ 
         txframes = audioCap->u.g728; 
         rxframes = audioCap->u.g728; 
      }
      return ooCapabilityAddSimpleCapability(call, OO_G728, txframes, 
                           rxframes, FALSE, dir, NULL, NULL, NULL, NULL, TRUE);

   case T_H245AudioCapability_g729:
      if(dir&OOTX) txframes = audioCap->u.g729;
      else if(dir&OORX) rxframes = audioCap->u.g729;
      else{ 
         txframes = audioCap->u.g729; 
         rxframes = audioCap->u.g729; 
      }
      return ooCapabilityAddSimpleCapability(call, OO_G729, txframes, 
                           rxframes, FALSE, dir, NULL, NULL, NULL, NULL, TRUE);

   case T_H245AudioCapability_g729AnnexA:
      if(dir&OOTX) txframes = audioCap->u.g729AnnexA;
      else if(dir&OORX) rxframes = audioCap->u.g729AnnexA;
      else{ 
         txframes = audioCap->u.g729AnnexA; 
         rxframes = audioCap->u.g729AnnexA; 
      }
      return ooCapabilityAddSimpleCapability(call, OO_G729A, txframes, 
                           rxframes, FALSE, dir, NULL, NULL, NULL, NULL, TRUE);

   case T_H245AudioCapability_g7231:
      if(dir&OOTX) txframes = audioCap->u.g7231->maxAl_sduAudioFrames;
      else if(dir&OORX) rxframes = audioCap->u.g7231->maxAl_sduAudioFrames;
      else{ 
         txframes = audioCap->u.g7231->maxAl_sduAudioFrames; 
         rxframes = audioCap->u.g7231->maxAl_sduAudioFrames; 
      }
      return ooCapabilityAddSimpleCapability(call, OO_G7231, txframes,rxframes,
                                         audioCap->u.g7231->silenceSuppression,
                                         dir, NULL, NULL, NULL, NULL, TRUE); 
   case T_H245AudioCapability_gsmFullRate:
      return ooCapabilityAddGSMCapability(call, OO_GSMFULLRATE, 
            (unsigned)(audioCap->u.gsmFullRate->audioUnitSize/OO_GSMFRAMESIZE),
                                        audioCap->u.gsmFullRate->comfortNoise,
                                        audioCap->u.gsmFullRate->scrambled, 
                                        dir, NULL, NULL, NULL, NULL, TRUE);
   case T_H245AudioCapability_gsmHalfRate:
      return ooCapabilityAddGSMCapability(call, OO_GSMHALFRATE,
            (unsigned)(audioCap->u.gsmHalfRate->audioUnitSize/OO_GSMFRAMESIZE),
                                        audioCap->u.gsmHalfRate->comfortNoise,
                                        audioCap->u.gsmHalfRate->scrambled, 
                                        dir, NULL, NULL, NULL, NULL, TRUE);
   case T_H245AudioCapability_gsmEnhancedFullRate:
      return ooCapabilityAddGSMCapability(call, OO_GSMENHANCEDFULLRATE, 
   (unsigned)(audioCap->u.gsmEnhancedFullRate->audioUnitSize/OO_GSMFRAMESIZE),
                                audioCap->u.gsmEnhancedFullRate->comfortNoise,
                                audioCap->u.gsmEnhancedFullRate->scrambled, 
                                dir, NULL, NULL, NULL, NULL, TRUE);

   default:
     OOTRACEDBGA1("Unsupported audio capability type\n");
   
   }

   return OO_OK;
}

static void ooExtractH264Parameters(DList * collapsing, unsigned * p_profile, unsigned * p_level)
{
  DListNode * gparams;
  H245GenericParameter * p ;

  /* Iterate on all parameters and extract
   * profile, level and constraint
   */
  gparams = collapsing->head;
  if ( !gparams )
  {
    OOTRACEAST(OOTRCLVLERR," ooExtractH264Parameters: no  generic param.\n" );
  }

  while (gparams != NULL)
  {
    p = (H245GenericParameter *) gparams->data;
    if ( p->parameterIdentifier.t == T_H245ParameterIdentifier_standard )
    {
      OOTRACEAST(OOTRCLVLDBGA,"[H323/CODEC]  ooExtractH264Parameters:Find profile & level in generic param. [%d] \n",
         p->parameterIdentifier.u.standard );
	    switch ( p->parameterIdentifier.u.standard )
	    {
        case 41: /* profile */
          *p_profile = ooProfileH2412SIP(p->parameterValue.u.booleanArray);
          OOTRACEAST(OOTRCLVLDBGA,"[H323/CODEC]  ooExtractH264Parameters:in collapsing: std param H.264 profile = 0x%x.\n", *p_profile);
          break;

        case 42: /* level */
          *p_level = ooLevelH2412SIP(p->parameterValue.u.unsignedMin);
          OOTRACEAST(OOTRCLVLDBGA,"[H323/CODEC]  ooExtractH264Parameters:in collapsing: std param H.264 level = 0x%x.\n", *p_level);
          break;

        default:
          OOTRACEAST(OOTRCLVLDBGA,"[H323/CODEC]  ooExtractH264Parameters:in collapsing: unsupported standard parameter ID %d.\n", p->parameterIdentifier.u.standard);
          break;
	    }
    }
    else
    {
      OOTRACEAST(OOTRCLVLERR,"[H323/CODEC]  ooExtractH264Parameters:in collapsing: ignoring generic parameter (non standard) of type %d.\n", p->parameterIdentifier.t);
    }
    gparams = gparams->next;
  }

  /* default values */
  
  if (*p_profile == 0) *p_profile = OO_DEFAULT_H264_PROFILE;
  if (*p_level == 0) *p_level = OO_DEFAULT_H264_LEVEL;
}

int ooAddRemoteVideoCapability(struct OOH323CallData *call,
                               H245VideoCapability *videoCap,
                               int dir)
{
  H245H263VideoCapability * h263;
  H245GenericCapability * h264;  /* generic video = h264 */
  unsigned profile = 0, level = 0, constraint = 0;
   
  switch(videoCap->t)
  {
    case T_H245VideoCapability_h263VideoCapability:
	    OOTRACEAST(OOTRCLVLDBGA,"[H323/CODEC] ooAddRemoteVideoCapability : H.263 \n" );
	    h263 = videoCap->u.h263VideoCapability;
	    return ooCapabilityAddH263VideoCapability(call,
                                                h263->m.sqcifMPIPresent ? h263->sqcifMPI : 0,
                                                h263->m.qcifMPIPresent ? h263->qcifMPI : 0,	
                                                h263->m.cifMPIPresent ? h263->cifMPI : 0,	
                                                h263->m.cif4MPIPresent ? h263->cif4MPI : 0,	
                                                h263->m.cif16MPIPresent ? h263->cif16MPI : 0,	
                                                h263->maxBitRate, dir,
                                                NULL, NULL, NULL, NULL, TRUE);


    case T_H245VideoCapability_genericVideoCapability:
	    h264 = videoCap->u.genericVideoCapability;
	    /* to be done check H.264 OID here */
	    if ( h264->m.collapsingPresent )
	    {
        ooExtractH264Parameters(&h264->collapsing, &profile, &level);
	    }
	    else    
      {
        profile = OO_DEFAULT_H264_PROFILE;
        level = OO_DEFAULT_H264_LEVEL;
      }

	    OOTRACEAST(OOTRCLVLDBGA,"[H323/CODEC] ooAddRemoteVideoCapability : H.264 profile = 0x%x level = 0x%x.\n", 
                 profile, level );
	    return ooCapabilityAddH264VideoCapability(call,
                                                profile, constraint,
                                                level, h264->maxBitRate, dir,
                                                NULL, NULL, NULL, NULL, TRUE);

    case T_H245VideoCapability_h261VideoCapability:
	    OOTRACEERR1("ooAddRemoteVideoCapability(): ooAddRemoteVideoCapability  Unsupported h261VideoCapability. Ignoring.\n");
	    break;

    default:
	    OOTRACEERR2("ooAddRemoteVideoCapability(): ooAddRemoteVideoCapability Unsupported video "
                  "capability type %d\n", videoCap->t );
	    break;
  }
  return OO_OK;
}



int ooCapabilityUpdateJointCapabilities
   (OOH323CallData* call, H245Capability *cap)
{
   ooH323EpCapability * epCap = NULL, *cur = NULL;

   OOTRACEDBGC3("checking whether we need to add cap to joint capabilities"
                "(%s, %s)\n", call->callType, call->callToken);
            
   switch(cap->t)
   {
   case T_H245Capability_receiveAudioCapability:
      epCap= ooIsAudioDataTypeSupported(call, cap->u.receiveAudioCapability, 
                                        OOTX);
      break;
   case T_H245Capability_transmitAudioCapability:
      epCap = ooIsAudioDataTypeSupported(call, cap->u.transmitAudioCapability,
                                        OORX);
      break;
   case T_H245Capability_receiveAndTransmitAudioCapability:
      //epCap = NULL;
      epCap = ooIsAudioDataTypeSupported(call, cap->u.transmitAudioCapability,
                                        OOTX);
      break;
   case T_H245Capability_receiveVideoCapability:
      return ooCapabilityUpdateJointCapabilitiesVideo(call, 
                                          cap->u.receiveVideoCapability, OOTX);
   case T_H245Capability_transmitVideoCapability:
      return ooCapabilityUpdateJointCapabilitiesVideo(call, 
                                         cap->u.transmitVideoCapability, OORX);
   case T_H245Capability_receiveAndTransmitVideoCapability:
      return ooCapabilityUpdateJointCapabilitiesVideo(call, 
                                          cap->u.receiveVideoCapability, OOTX);                                            
   case T_H245Capability_receiveUserInputCapability:
      if((cap->u.receiveUserInputCapability->t == 
                                 T_H245UserInputCapability_basicString) &&
         (call->dtmfmode & OO_CAP_DTMF_H245_alphanumeric))
      {
         call->jointDtmfMode |= OO_CAP_DTMF_H245_alphanumeric;
         return OO_OK;
      }
      else if((cap->u.receiveUserInputCapability->t ==
               T_H245UserInputCapability_dtmf) &&
               (call->dtmfmode & OO_CAP_DTMF_H245_signal))
      {
         call->jointDtmfMode |= OO_CAP_DTMF_H245_signal;
         return OO_OK;
      }
      //break;
   default:
     OOTRACEDBGA4("Unsupported cap type encountered %d. Ignoring. (%s, %s)\n", 
                   cap->t, call->callType, call->callToken);
   }

   if(epCap)
   {
      OOTRACEDBGC3("Adding cap to joint capabilities(%s, %s)\n",call->callType,
                   call->callToken);
      /* Note:we add jointCaps in remote endpoints preference order.*/
      if(!call->jointCaps)
         call->jointCaps = epCap;
      else {
         cur = call->jointCaps;
         while(cur->next) cur = cur->next;
         cur->next = epCap;
      }

      return OO_OK;
   }

   OOTRACEDBGC3("Not adding to joint capabilities. (%s, %s)\n", call->callType,
                call->callToken);
   return OO_OK;
}



int ooCapabilityUpdateJointCapabilitiesVideo
   (OOH323CallData *call, H245VideoCapability *videoCap, int dir)
{
   switch(videoCap->t)
   {
   case T_H245VideoCapability_h263VideoCapability:
      return ooCapabilityUpdateJointCapabilitiesVideoH263(call, 
                                        videoCap->u.h263VideoCapability, dir);

   case T_H245VideoCapability_genericVideoCapability: /* IVeS - H.264 */
      return ooCapabilityUpdateJointCapabilitiesVideoH264(call,
                                        videoCap->u.genericVideoCapability, dir);

   case T_H245VideoCapability_h261VideoCapability:
      OOTRACEERR4("ooCapabilityUpdateJointCapabilitiesVideo - Unsupported"
                   "capability h261VideoCapability(%d) h261. (%s, %s)\n", videoCap->t, call->callType, 
                   call->callToken);
      break;

   default:
      OOTRACEERR4("ooCapabilityUpdateJointCapabilitiesVideo - Unsupported"
                   "capability type %d. (%s, %s)\n", videoCap->t, call->callType, 
                   call->callToken);
   }
   return OO_OK;
}


int ooCapabilityUpdateJointCapabilitiesVideoH263
   (OOH323CallData *call, H245H263VideoCapability *pH263Cap, int dir)
{
   ooH323EpCapability *epCap = NULL, *cur = NULL;
   if(pH263Cap->m.sqcifMPIPresent)
   {
       epCap =  ooIsVideoDataTypeH263Supported(call, pH263Cap, dir, 
                                                          OO_PICFORMAT_SQCIF);
      if(epCap)
      {
         OOTRACEDBGC3("Adding H263-SQCIF to joint capabilities(%s, %s)\n",
                      call->callType, call->callToken);
         /* Note:we add jointCaps in remote endpoints preference order.*/
         if(!call->jointCaps)
            call->jointCaps = epCap;
         else {
            cur = call->jointCaps;
            while(cur->next) cur = cur->next;
            cur->next = epCap;
         }

      }     
   }

   epCap = NULL;

   if(pH263Cap->m.qcifMPIPresent)
   {
      epCap =  ooIsVideoDataTypeH263Supported(call, pH263Cap, dir, 
                                                          OO_PICFORMAT_QCIF);
      if(epCap)
      {
         OOTRACEDBGC3("Adding H263-QCIF to joint capabilities(%s, %s)\n",
                      call->callType, call->callToken);
         /* Note:we add jointCaps in remote endpoints preference order.*/
         if(!call->jointCaps)
            call->jointCaps = epCap;
         else {
            cur = call->jointCaps;
            while(cur->next) cur = cur->next;
            cur->next = epCap;
         }

      }     
   }

   epCap = NULL;

   if(pH263Cap->m.cifMPIPresent)
   {
      epCap =  ooIsVideoDataTypeH263Supported(call, pH263Cap, dir, 
                                                          OO_PICFORMAT_CIF);
      if(epCap)
      {
         OOTRACEDBGC3("Adding H263-CIF to joint capabilities(%s, %s)\n",
                      call->callType, call->callToken);
         /* Note:we add jointCaps in remote endpoints preference order.*/
         if(!call->jointCaps)
            call->jointCaps = epCap;
         else {
            cur = call->jointCaps;
            while(cur->next) cur = cur->next;
            cur->next = epCap;
         }

      }     
   }

   epCap = NULL;

   if(pH263Cap->m.cif4MPIPresent)
   {
      epCap =  ooIsVideoDataTypeH263Supported(call, pH263Cap, dir, 
                                                          OO_PICFORMAT_CIF4);
      if(epCap)
      {
         OOTRACEDBGC3("Adding H263-CIF4 to joint capabilities(%s, %s)\n",
                      call->callType, call->callToken);
         /* Note:we add jointCaps in remote endpoints preference order.*/
         if(!call->jointCaps)
            call->jointCaps = epCap;
         else {
            cur = call->jointCaps;
            while(cur->next) cur = cur->next;
            cur->next = epCap;
         }
      }     
   }

   epCap = NULL;

   if(pH263Cap->m.cif16MPIPresent)
   {
      epCap =  ooIsVideoDataTypeH263Supported(call, pH263Cap, dir, 
                                                          OO_PICFORMAT_CIF16);
      if(epCap)
      {
         OOTRACEDBGC3("Adding H263-CIF16 to joint capabilities(%s, %s)\n",
                      call->callType, call->callToken);
         /* Note:we add jointCaps in remote endpoints preference order.*/
         if(!call->jointCaps)
            call->jointCaps = epCap;
         else {
            cur = call->jointCaps;
            while(cur->next) cur = cur->next;
            cur->next = epCap;
         }

      }     
   }

   return OO_OK;
}

int ooCapabilityUpdateJointCapabilitiesVideoH264(OOH323CallData *call, H245GenericCapability *pH264Cap, int dir)
{
   ooH323EpCapability *epCap = NULL, *cur = NULL;
   /* IV�S - OID should be checked here */
   epCap = ooIsVideoDataTypeH264Supported(call, pH264Cap, dir);
   if (epCap)
   {
       OOTRACEDBGC3("Adding H264 to joint capabilities(%s, %s)\n",
                      call->callType, call->callToken);
       /* Note:we add jointCaps in remote endpoints preference order.*/
       if(!call->jointCaps)
	   call->jointCaps = epCap;
       else {
            cur = call->jointCaps;
            while(cur->next) cur = cur->next;
            cur->next = epCap;
       }

   }     
   return OO_OK;
}

const char* ooGetCapTypeText (OOCapabilities cap)
{
   static const char *capTypes[]={
      "unknown",
      "OO_NONSTANDARD",
      "OO_G711ALAW64K",
      "OO_G711ALAW56K",
      "OO_G711ULAW64K",
      "OO_G711ULAW56K",
      "OO_G72264K",
      "OO_G72256K",
      "OO_G72248K",
      "OO_G7231",
      "OO_G728",
      "OO_G729",
      "OO_G729ANNEXA",
      "OO_IS11172AUDIO",
      "OO_IS13818AUDIO",
      "OO_G729WANNEXB",
      "OO_G729ANNEXAWANNEXB",
      "OO_G7231ANNEXC",
      "OO_GSMFULLRATE",
      "OO_GSMHALFRATE",
      "OO_GSMENHANCEDFULLRATE",
      "OO_GENERICAUDIO",
      "OO_G729EXTENSIONS",
      "OO_VBD",
      "OO_AUDIOTELEPHONYEVENT",
      "OO_AUDIOTONE",
      "OO_EXTELEM1",
      "OO_VIDEO_BASE",
      "OO_NONSTDVIDEO",
      "OO_H261VIDEO",
      "OO_H262VIDEO",
      "OO_H263VIDEO",
      "OO_IS11172VIDEO",  /* mpegi4 part 2 */
      "OO_GENERICVIDEO",  /* H.264 mostly */
      "OO_EXTELEMVIDEO"
   };
   return ooUtilsGetText (cap, capTypes, OONUMBEROF(capTypes));
}

// Table 8-2 − ITU-T H.264 capability parameter – Profile
// This parameter is a Boolean array.
// If bit 2 (value 64) is 1, this indicates the Baseline Profile.
// If bit 3 (value 32) is 1, this indicates the Main Profile.
// If bit 4 (value 16) is 1, this indicates the Extended Profile.
// If bit 5 (value 8) is 1, this indicates the High Profile.
// If bit 6 (value 4) is 1, this indicates the High 10 Profile.
// If bit 7 (value 2) is 1, this indicates the High 4:2:2 Profile.
// If bit 8 (value 1) is 1, this indicates the High 4:4:4 Profile.
//All other bits are reserved, shall be set to 0, and shall be ignored
unsigned char ooProfileSIP2H241(unsigned char sipProfile){
  unsigned char H241Profile = OO_DEFAULT_H264_PROFILE ;
    switch ( sipProfile ){
      case h264_profile_calvac44 :
      case h264_profile_baseline:
        H241Profile = 0x40 ;
        break ;
      case h264_profile_main :
        H241Profile = 0x20 ;
        break ;
      case h264_profile_extended :
        H241Profile = 0x10 ;
        break ;
      case h264_profile_high :
        H241Profile = 0x8 ;
        break ;
      case h264_profile_high10 :
        H241Profile = 0x4 ;
        break ;
      case h264_profile_high422:
        H241Profile = 0x2 ;
        break ;
      case h264_profile_high444:
        H241Profile = 0x1 ;
        break ;
      default:
        ast_log(LOG_ERROR,"[H323/H241] Warning profile =0x%x Unknown => reajust to 0x%x\n",sipProfile,OO_DEFAULT_H264_PROFILE);
        H241Profile = OO_DEFAULT_H264_PROFILE ;
        break ; 
    }
    ast_log(LOG_DEBUG,"[H323/H241] sip profile[0x%X] = h241 profile[0x%x] \n",sipProfile,H241Profile);
    return H241Profile;
}


unsigned char ooProfileH2412SIP(unsigned char h241Profile){
  unsigned char SipProfile = h264_profile_baseline ;
    switch ( h241Profile ){
      case 0x40:
        SipProfile = h264_profile_baseline ;
        break ;
      case 0x20 :
        SipProfile = h264_profile_main ;
        break ;
      case 0x10 :
        SipProfile = h264_profile_extended ;
        break ;
      case 0x8 :
        SipProfile = h264_profile_high ;
        break ;
      case 0x4 :
        SipProfile = h264_profile_high10 ;
        break ;
      case 0x2:
        SipProfile = h264_profile_high422 ;
        break ;
      case 0x1:
        SipProfile = h264_profile_high444 ;
        break ;
      default:
        ast_log(LOG_ERROR,"[H323/H241] Warning profile =0x%x Unknown => reajust to 0x%x\n",h241Profile,SipProfile);
        break ; 
    }
    ast_log(LOG_DEBUG,"[H323/H241] h214 profile[0x%X] = sip profile[0x%x] \n",h241Profile,SipProfile);
    return SipProfile;
}

/*     
Table 8-4 − Level parameter values
Level parameter value ITU-T H.264 Level number
15 1
19 1b
22 1.1
29 1.2
36 1.3
43 2
50 2.1
57 2.2
64 3
71 3.1
78 3.2
85 4
92 4.1
99 4.2
106 5
113 5.1
120 5.2
*/
#define LIMIT_22 0
unsigned char ooLevelSIP2H241( unsigned char sipLevel ){
  unsigned char h241Level = OO_DEFAULT_H264_LEVEL ;
  switch ( sipLevel ){
    case 10:
      h241Level = 15;
      break ;
    case 14:
      h241Level = 19;
      break ;
    case 11:
      h241Level = 22;
      break ;
    case 12:
      h241Level = 29;
      break ;
    case 13:
      h241Level = 36;
      break ;
    case 20:
      h241Level = 43;
      break ;
    case 21:
      h241Level = 50;
      break ;
#if LIMIT_22
    default:
      h241Level = 57;
      break ;
#else
    case 22:
      h241Level = 57;
      break ;
    case 30:
      h241Level = 64;
      break ;
    case 31:
      h241Level = 71;
      break ;
    case 32:
      h241Level = 78;
      break ;
    case 40:
      h241Level = 85;
      break ;
    case 41:
      h241Level = 92;
      break ;
    case 42:
      h241Level = 99;
      break ;
   case 50:
      h241Level = 106;
      break ;
   case 51:
      h241Level = 113;
      break ;
   case 52:
      h241Level = 120;
      break ;
   default :   ast_log(LOG_ERROR,"[H323/H241] Warning sip level =0x%x Unknown => reajust to 0x%x\n",sipLevel,h241Level);
        break ; 
#endif
    }
    ast_log(LOG_DEBUG,"[H323/H241] sip level[0x%X] = h241 level[0x%x] \n",sipLevel,h241Level);
    return h241Level;
}

unsigned char ooLevelH2412SIP( unsigned char h241Level ){
  unsigned char sipLevel = 71 ;
  switch ( h241Level ){
    case 15:
      sipLevel = 10;
      break ;
    case 19:
      sipLevel = 14;
      break ;
    case 22:
      sipLevel = 11;
      break ;
    case 29:
      sipLevel = 12;
      break ;
    case 36:
      sipLevel = 13;
      break ;
    case 43:
      sipLevel = 20;
      break ;
    case 50:
      sipLevel = 21;
      break ;
    case 57:
      sipLevel = 22;
      break ;
    case 64:
      sipLevel = 30;
      break ;
    case 71:
      sipLevel = 31;
      break ;
    case 78:
      sipLevel = 32;
      break ;
    case 85:
      sipLevel = 40;
      break ;
    case 92:
      sipLevel = 41;
      break ;
    case 99:
      sipLevel = 42;
      break ;
   case 106:
      sipLevel = 50;
      break ;
   case 113:
      sipLevel = 51;
      break ;
   case 120:
      sipLevel = 52;
      break ;
   default :   ast_log(LOG_ERROR,"[H323/H241] Warning h241 level =0x%x Unknown => reajust to 0x%x\n",h241Level,sipLevel);
        break ; 
    }
    ast_log(LOG_DEBUG,"[H323/H241] h241 level[0x%X] = sip level[0x%x] \n",h241Level,sipLevel);
    return sipLevel;
}
