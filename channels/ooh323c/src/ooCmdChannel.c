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

#include "ooStackCmds.h"
#include "ootrace.h"
#include "ooq931.h"
#include "ooh245.h"
#include "ooh323ep.h"
#include "oochannels.h"
#include "ooCalls.h"
#include "ooCmdChannel.h"

/** Global endpoint structure */
extern OOH323EndPoint gH323ep;

OOSOCKET gCmdChan = 0;
pthread_mutex_t gCmdChanLock;
pthread_mutex_t thSync;
int             nbthSync = 0 ;

int ooCreateCmdConnection()
{
   int ret = 0;
   int thePipe[2];
   OOTRACEAST(OOTRCLVLDBGA,"[H323] Create command file \n");
   if ((ret = pipe(thePipe)) == -1) {
      return OO_FAILED;
   }
   pthread_mutex_init(&gCmdChanLock, NULL);
   pthread_mutex_init(&thSync, NULL);
   ooSyncLock();

   gH323ep.cmdSock = dup(thePipe[0]);
   close(thePipe[0]);
   gCmdChan = dup(thePipe[1]);
   close(thePipe[1]);
   return OO_OK;
}

int ooSyncLock()
{
  nbthSync ++ ;
  OOTRACEAST(OOTRCLVLDBGA,"[H323] Synchro Lock %d \n",nbthSync);
  if ( nbthSync > 2 )
  {
    OOTRACEERR2("Potentiel dead lock on H323 cmd [%d]\n",nbthSync);
  }
	pthread_mutex_lock(&thSync);
}

int ooSyncUnLock()
{
   pthread_mutex_unlock(&thSync);
   nbthSync-- ;
   OOTRACEAST(OOTRCLVLDBGA,"[H323] Synchro Unlock %d \n",nbthSync);
}

int ooCloseCmdConnection()
{
   OOTRACEAST(OOTRCLVLDBGA,"[H323] Destroy command file \n");

   close(gH323ep.cmdSock);
   gH323ep.cmdSock = 0;
   close(gCmdChan);
   gCmdChan = 0;
   pthread_mutex_destroy(&gCmdChanLock);
   pthread_mutex_destroy(&thSync);
   return OO_OK;
}

int ooWriteStackCommand(OOStackCommand *cmd)
{
  OOTRACEAST(OOTRCLVLDBGA,"Send stack command %d \n",cmd->type);
	pthread_mutex_lock(&gCmdChanLock);
  if (write(gCmdChan, (char*)cmd, sizeof(OOStackCommand)) == -1) {
		pthread_mutex_unlock(&gCmdChanLock);
		return OO_FAILED;
	}
	pthread_mutex_unlock(&gCmdChanLock);
   
  return OO_OK;
}


int ooReadAndProcessStackCommand()
{
  OOH323CallData *pCall = NULL;   
  unsigned char buffer[MAXMSGLEN];
  int i, recvLen = 0;
  OOStackCommand cmd;
  memset(&cmd, 0, sizeof(OOStackCommand));
  recvLen = read(gH323ep.cmdSock, buffer, MAXMSGLEN);

  if(recvLen <= 0)
  {
    OOTRACEERR1("Error:Failed to read CMD message\n");
    return OO_FAILED;
  }

  for(i=0; (int)(i+sizeof(OOStackCommand)) <= recvLen; i += sizeof(OOStackCommand))
  {
    memcpy(&cmd, buffer+i, sizeof(OOStackCommand));

    OOTRACEAST(OOTRCLVLDBGA,"Recv stack command %d \n",cmd.type);
    if(cmd.type == OO_CMD_NOOP)
      continue;

    if(gH323ep.gkClient && gH323ep.gkClient->state != GkClientRegistered)
    {
      OOTRACEINFO1("Ignoring stack command as Gk Client is not registered"
                   " yet\n");
    }
    else 
    {
      switch(cmd.type) 
      {
        case OO_CMD_MAKECALL: 
          OOTRACEAST(OOTRCLVLDBGA,"[H323/CMD] << Receiv Command MakeCall %s (%s) CV[%s] opts[0x%x]\n",
                     (cmd.param1)?(char*)cmd.param1:"NULL",
                     (cmd.param2)?(char*)cmd.param2:"NULL",
                     (cmd.param3)?(char*)cmd.param3:"NULL",
                     cmd.param4 );
 
          ooH323MakeCall ((char*)cmd.param1, (char*)cmd.param2, (char*)cmd.param3, 
                          (ooCallOptions*)cmd.param4);

          break;

        case OO_CMD_MANUALRINGBACK:
          OOTRACEAST(OOTRCLVLDBGA,"[H323/CMD] << Receiv Command ManualRingBack(%s)\n",
                     (cmd.param1)?(char*)cmd.param1:"NULL");
          if(OO_TESTFLAG(gH323ep.flags, OO_M_MANUALRINGBACK))
          {
            pCall = ooFindCallByToken((char*)cmd.param1);
            if(!pCall) {
              OOTRACEINFO2("Call \"%s\" does not exist\n",
                           (char*)cmd.param1);
              OOTRACEINFO1("Call migth be cleared/closed\n");
            }
            else {
              ooSendAlerting(ooFindCallByToken((char*)cmd.param1));
              if(OO_TESTFLAG(gH323ep.flags, OO_M_AUTOANSWER)) {
                ooSendConnect(ooFindCallByToken((char*)cmd.param1));
              }
            }
          }
          break;
 
        case OO_CMD_ANSCALL:
          OOTRACEAST(OOTRCLVLDBGA,"[H323/CMD] << Receiv Command AnswerCall (%s) \n",
                     (cmd.param1)?(char*)cmd.param1:"NULL");
          pCall = ooFindCallByToken((char*)cmd.param1);
          if(!pCall) 
          {
            OOTRACEERR2("[H323/CMD] << Receiv Command AnswerCall, "
                        "Call \"%s\" does not exist\n",
                        (char*)cmd.param1);
            OOTRACEERR1("[H323/CMD] << Receiv Command AnswerCall,"
                        "Call might be cleared/closed\n");
          }
          else 
          {
            ooSendConnect(pCall);
          }
          OOTRACEAST(OOTRCLVLDBGA,"[H323/CMD] <<  answers call send Delock Sync.(%s)\n",
                     (cmd.param1)?(char*)cmd.param1:"NULL" );   
          // Delock 
          ooSyncUnLock();
          break;

        case OO_CMD_FWDCALL:
          OOTRACEAST(OOTRCLVLDBGA,"[H323/CMD] << Receiv Command Forward Call (%s) \n",
                     (cmd.param1)?(char*)cmd.param1:"NULL");
          OOTRACEINFO3("Forwarding call %s to %s\n", (char*)cmd.param1,
                       (char*)cmd.param2);
          ooH323ForwardCall((char*)cmd.param1, (char*)cmd.param2);
          break;

        case OO_CMD_HANGCALL: 
          OOTRACEAST(OOTRCLVLDBGA,"[H323/CMD] << Receiv Command HangCall (%s) \n",
                     (cmd.param1)?(char*)cmd.param1:"NULL");

          ooH323HangCall((char*)cmd.param1, 
                         *(OOCallClearReason*)cmd.param2);
          break;
          
        case OO_CMD_SENDDIGIT:
          OOTRACEAST(OOTRCLVLDBGA,"[H323/CMD] << Received Command SendDigit (%s) \n",
                     (cmd.param1)?(char*)cmd.param1:"NULL");

          pCall = ooFindCallByToken((char*)cmd.param1);
          if(!pCall) 
          {
            OOTRACEERR2("ERROR:Invalid calltoken %s\n",
                        (char*)cmd.param1);
            break;
          }
          if(pCall->jointDtmfMode & OO_CAP_DTMF_H245_alphanumeric) 
          {
            ooSendH245UserInputIndication_alphanumeric(
              pCall, (const char*)cmd.param2);
          }
          else if(pCall->jointDtmfMode & OO_CAP_DTMF_H245_signal) 
          {
            ooSendH245UserInputIndication_signal(
              pCall, (const char*)cmd.param2);
          }
          else {
            ooQ931SendDTMFAsKeyPadIE(pCall, (const char*)cmd.param2);
          }

          break;

        case OO_CMD_FASTUPDATEREQ: /* IVES - Video Update Request icmd */
          OOTRACEAST(OOTRCLVLDBGA,"[H323/CMD] << Received Command FUR (%s) \n",
                     (cmd.param1)?(char*)cmd.param1:"NULL");

          pCall = ooFindCallByToken((char*)cmd.param1);
          if(!pCall) 
          {
            OOTRACEERR2("ERROR:Invalid calltoken %s\n",
                        (char*)cmd.param1);
          }
          else
          {
            ooSendH245VideoUpdateRequest( pCall , -1 , -1 );
          }
          break ;

        case OO_CMD_STOPMONITOR: 
          OOTRACEAST(OOTRCLVLDBGA,"[H323/CMD] << Receiv Command Stop Monitor \n");
          ooStopMonitorCalls();
          break;
        case OO_CMD_NOOP:
        default: OOTRACEERR1("ERROR:Unknown command\n");
      }
    }
    if(cmd.param1) free(cmd.param1);
    if(cmd.param2) free(cmd.param2);
    if(cmd.param3) free(cmd.param3);
    if(cmd.param4) free(cmd.param4);
  }


  return OO_OK;
}
   
