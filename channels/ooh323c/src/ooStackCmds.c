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
#include "ooh323ep.h"
#include "ooCalls.h"
#include "ooCmdChannel.h"

extern OOSOCKET gCmdChan;

int ooGenerateOutgoingCallToken (char *callToken, size_t size)
{
   static int counter = 1;
   char aCallToken[200];
   int  ret = 0;


   sprintf (aCallToken, "ooh323c_o_%d", counter++);

   if (counter > OO_MAX_CALL_TOKEN)
      counter = 1;

   if ((strlen(aCallToken)+1) < size)
      strcpy (callToken, aCallToken);
   else {
      ret = OO_FAILED;
   }

   return ret;
}


OOStkCmdStat ooMakeCall 
   (const char* dest, char* callToken, const char* chargVectorID, size_t bufsiz, ooCallOptions *opts)
{
   OOStackCommand cmd;
   OOTRACEAST(OOTRCLVLDBGA,"[H323/CMD] >>  Send Command MakeCall %s  CV[%s]\n",
              (dest)?dest:"NULL", (chargVectorID)?chargVectorID:"NULL" );

   if(!callToken)
      return OO_STKCMD_INVALIDPARAM;
  

   /* Generate call token*/
   if (ooGenerateOutgoingCallToken (callToken, bufsiz) != OO_OK){
      return OO_STKCMD_INVALIDPARAM;
   }

   if(gCmdChan == 0)
   {
      if(ooCreateCmdConnection() != OO_OK)
         return OO_STKCMD_CONNECTIONERR;
   }

   memset(&cmd, 0, sizeof(OOStackCommand));
   cmd.type = OO_CMD_MAKECALL;
   cmd.param1 = (void*) malloc(strlen(dest)+1);
   if(!cmd.param1)
   {
      return OO_STKCMD_MEMERR;
   }
   strcpy((char*)cmd.param1, dest);

  
   cmd.param2 = (void*) malloc(strlen(callToken)+1);
   if(!cmd.param2)
   {
      free(cmd.param1);
      return OO_STKCMD_MEMERR;
   }
   
   strcpy((char*)cmd.param2, callToken);

   if ( chargVectorID )
   {
     cmd.param3 = (void*) malloc(OO_PCV_CID_SIZE);
     if(!cmd.param3)
     {
       free(cmd.param1);
       free(cmd.param2);
       return OO_STKCMD_MEMERR;
     }
   
     memcpy((char*)cmd.param3, chargVectorID,OO_PCV_CID_SIZE);
   }
   else
   {
     cmd.param3 = NULL ; 
   }

   if(!opts)
   {
      cmd.param4 = 0;
   }
   else {
      cmd.param4 = (void*) malloc(sizeof(ooCallOptions));
      if(!cmd.param4)
      {
         free(cmd.param1);
         free(cmd.param2);
         free(cmd.param3);
         return OO_STKCMD_MEMERR;
      }
      memcpy((void*)cmd.param4, opts, sizeof(ooCallOptions));
   }

   if(ooWriteStackCommand(&cmd) != OO_OK)
   {
      free(cmd.param1);
      free(cmd.param2);
      if(cmd.param3) free(cmd.param3);
      return OO_STKCMD_WRITEERR;
   }

   return OO_STKCMD_SUCCESS;
}


OOStkCmdStat ooManualRingback(const char *callToken)
{
   OOStackCommand cmd;

   OOTRACEAST(OOTRCLVLDBGA,"[H323/CMD] >>  Send Command Manual RingBack (%s) \n",
              (callToken)?callToken:"NULL");

   if(!callToken)
   {
      return OO_STKCMD_INVALIDPARAM;
   }

   if(gCmdChan == 0)
   {
      if(ooCreateCmdConnection() != OO_OK)
         return OO_STKCMD_CONNECTIONERR;
   }

   memset(&cmd, 0, sizeof(OOStackCommand));
   cmd.type = OO_CMD_MANUALRINGBACK;
   cmd.param1 = (void*) malloc(strlen(callToken)+1);
   if(!cmd.param1)
   {
      return OO_STKCMD_MEMERR;
   }
   strcpy((char*)cmd.param1, callToken);
   
   if(ooWriteStackCommand(&cmd) != OO_OK)
   {
      free(cmd.param1);
      return OO_STKCMD_WRITEERR;
   }

   return OO_STKCMD_SUCCESS;
}

OOStkCmdStat ooAnswerCall(const char *callToken)
{
   OOStackCommand cmd;

   OOTRACEAST(OOTRCLVLDBGA,"[H323/CMD] >>  Send Command AnswerCall (%s) \n",
              (callToken)?callToken:"NULL");

   if(!callToken)
   {
      return OO_STKCMD_INVALIDPARAM;
   }

   if(gCmdChan == 0)
   {
      if(ooCreateCmdConnection() != OO_OK)
         return OO_STKCMD_CONNECTIONERR;
   }

   memset(&cmd, 0, sizeof(OOStackCommand));
   cmd.type = OO_CMD_ANSCALL;

   cmd.param1 = (void*) malloc(strlen(callToken)+1);
   if(!cmd.param1)
   {
      return OO_STKCMD_MEMERR;
   }
   strcpy((char*)cmd.param1, callToken);
   
   if(ooWriteStackCommand(&cmd) != OO_OK)
   {
      free(cmd.param1);
      return OO_STKCMD_WRITEERR;
   }

   return OO_STKCMD_SUCCESS;
}

OOStkCmdStat ooForwardCall(const char* callToken, char *dest)
{
   OOStackCommand cmd;

   OOTRACEAST(OOTRCLVLDBGA,"[H323/CMD] >>  Send Command Forward Call (%s) \n",
              (callToken)?callToken:"NULL");
   
   if(!callToken || !dest)
   {
      return OO_STKCMD_INVALIDPARAM;
   }

   if(gCmdChan == 0)
   {
      if(ooCreateCmdConnection() != OO_OK)
         return OO_STKCMD_CONNECTIONERR;
   }

   memset(&cmd, 0, sizeof(OOStackCommand));
   cmd.type = OO_CMD_FWDCALL;

   cmd.param1 = (void*) malloc(strlen(callToken)+1);
   cmd.param2 = (void*) malloc(strlen(dest)+1);
   if(!cmd.param1 || !cmd.param2)
   {
      if(cmd.param1)   free(cmd.param1);  /* Release memory */
      if(cmd.param2)   free(cmd.param2);
      return OO_STKCMD_MEMERR;
   }
   strcpy((char*)cmd.param1, callToken);
   strcpy((char*)cmd.param2, dest);

   if(ooWriteStackCommand(&cmd) != OO_OK)
   {
      free(cmd.param1);
      free(cmd.param2);
      return OO_STKCMD_WRITEERR;
   }

   return OO_STKCMD_SUCCESS;
}


OOStkCmdStat ooHangCall(const char* callToken, OOCallClearReason reason)
{
   OOStackCommand cmd;

   OOTRACEAST(OOTRCLVLDBGA,"[H323/CMD] >>  Send Command Hangup Call (%s) \n",
              (callToken)?callToken:"NULL");

   if(!callToken)
   {
      return OO_STKCMD_INVALIDPARAM;
   }

   if(gCmdChan == 0)
   {
      if(ooCreateCmdConnection() != OO_OK)
         return OO_STKCMD_CONNECTIONERR;
   }

   memset(&cmd, 0, sizeof(OOStackCommand));
   cmd.type = OO_CMD_HANGCALL;
   cmd.param1 = (void*) malloc(strlen(callToken)+1);
   cmd.param2 = (void*) malloc(sizeof(OOCallClearReason));
   if(!cmd.param1 || !cmd.param2)
   {
      if(cmd.param1)   free(cmd.param1); /* Release memory */
      if(cmd.param2)   free(cmd.param2);
      return OO_STKCMD_MEMERR;
   }
   strcpy((char*)cmd.param1, callToken);
   *((OOCallClearReason*)cmd.param2) = reason;

   if(ooWriteStackCommand(&cmd) != OO_OK)
   {
      free(cmd.param1);
      free(cmd.param2);
      return OO_STKCMD_WRITEERR;
   }
   
   return OO_STKCMD_SUCCESS;
}


OOStkCmdStat ooStopMonitor()
{
   OOStackCommand cmd;

   OOTRACEAST(OOTRCLVLDBGA,"[H323/CMD] >>  Send Command Stop Monitor \n");

   if(gCmdChan == 0)
   {
      if(ooCreateCmdConnection() != OO_OK)
         return OO_STKCMD_CONNECTIONERR;
   }

   memset(&cmd, 0, sizeof(OOStackCommand));
   cmd.type = OO_CMD_STOPMONITOR;
   
   if(ooWriteStackCommand(&cmd) != OO_OK)
      return OO_STKCMD_WRITEERR;

   return OO_STKCMD_SUCCESS;
}

OOStkCmdStat ooSendDTMFDigit(const char *callToken, const char* dtmf)
{
   OOStackCommand cmd;

   OOTRACEAST(OOTRCLVLDBGA,"[H323/CMD] >>  Send Command DTMF (%s) \n",
              (callToken)?callToken:"NULL");
   
   if(!callToken)
   {
      return OO_STKCMD_INVALIDPARAM;
   }

   if(gCmdChan == 0)
   {
      if(ooCreateCmdConnection() != OO_OK)
         return OO_STKCMD_CONNECTIONERR;
   }

   memset(&cmd, 0, sizeof(OOStackCommand));
   cmd.type = OO_CMD_SENDDIGIT;

   cmd.param1 = (void*) malloc(strlen(callToken)+1);
   cmd.param2 = (void*) malloc(strlen(dtmf)+1);
   if(!cmd.param1 || !cmd.param2)
   {
      if(cmd.param1)   free(cmd.param1); /* Release memory */
      if(cmd.param2)   free(cmd.param2);
      return OO_STKCMD_MEMERR;
   }
   strcpy((char*)cmd.param1, callToken);
   strcpy((char*)cmd.param2, dtmf);
   
   if(ooWriteStackCommand(&cmd) != OO_OK)
   {
      free(cmd.param1);
      free(cmd.param2);
      return OO_STKCMD_WRITEERR;
   }

   return OO_STKCMD_SUCCESS;
}

OOStkCmdStat ooSendFastUpdateRequest(const char *callToken, int first, int nbgob)
{
   OOStackCommand cmd;

   OOTRACEAST(OOTRCLVLDBGA,"[H323/CMD] >>  call (%s) Send fast update request CMD.\n",
              (callToken)?callToken:"NULL");

   if(!callToken)
   {
      return OO_STKCMD_INVALIDPARAM;
   }

   if(gCmdChan == 0)
   {
      if(ooCreateCmdConnection() != OO_OK)
         return OO_STKCMD_CONNECTIONERR;
   }

   memset(&cmd, 0, sizeof(OOStackCommand));
   cmd.type = OO_CMD_FASTUPDATEREQ;

   cmd.param1 = (void*) malloc(strlen(callToken)+1);
   cmd.param2 = (void*) malloc(sizeof ( int ) );
   cmd.param3 = (void*) malloc(sizeof ( int ) );

   if(!cmd.param1 || !cmd.param2 || !cmd.param3 )
   {
      if(cmd.param1)   free(cmd.param1); /* Release memory */
      if(cmd.param2)   free(cmd.param2); /* Release memory */
      if(cmd.param3)   free(cmd.param3); /* Release memory */
     return OO_STKCMD_MEMERR;
   }

   strcpy((char*) cmd.param1, callToken);
   (* (int *)cmd.param2) = first;
   (* (int *)cmd.param3) = nbgob;

   if(ooWriteStackCommand(&cmd) != OO_OK)
   {
      return OO_STKCMD_WRITEERR;
   }

   return OO_STKCMD_SUCCESS;
}
const char* ooGetStkCmdStatusCodeTxt(OOStkCmdStat stat)
{
   switch(stat)
   {
      case OO_STKCMD_SUCCESS:
         return "Stack command - successfully issued";

      case OO_STKCMD_MEMERR:
         return "Stack command - Memory allocation error";

      case OO_STKCMD_INVALIDPARAM:
         return "Stack command - Invalid parameter";

      case OO_STKCMD_WRITEERR:
         return "Stack command - write error";

      case OO_STKCMD_CONNECTIONERR: 
         return "Stack command - Failed to create command channel";

      default:
         return "Invalid status code";
   }
}

