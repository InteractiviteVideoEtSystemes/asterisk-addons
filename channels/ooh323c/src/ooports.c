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


#include "ooports.h"
#include "ooh323ep.h"
#include "ootrace.h"

/** Global endpoint structure */
extern OOH323EndPoint gH323ep;

/* Get the next port of type TCP/UDP/RTP */
int ooGetNextPort (OOH323PortType type)
{
  int port = 0 ;
   if(type==OOTCP)
   {
     // Sig H.225 & Q931
      if(gH323ep.tcpPorts.current <= gH323ep.tcpPorts.max)
         port= gH323ep.tcpPorts.current++;
      else
      {
         gH323ep.tcpPorts.current = gH323ep.tcpPorts.start;
         port= gH323ep.tcpPorts.current++;
      }
   }
   if(type==OOUDP)
   {
     // GK
      if(gH323ep.udpPorts.current <= gH323ep.udpPorts.max)
         port = gH323ep.udpPorts.current++;
      else
      {
         gH323ep.udpPorts.current = gH323ep.udpPorts.start;
         port = gH323ep.udpPorts.current++;
      }
   }
   if(type==OORTP)
   {
      if(gH323ep.rtpPorts.current <= gH323ep.rtpPorts.max)
         port = gH323ep.rtpPorts.current++;
      else
      {
         gH323ep.rtpPorts.current = gH323ep.rtpPorts.start;
         port = gH323ep.rtpPorts.current++;
      }
   }
   if ( port )
   {
     OOTRACEAST(OOTRCLVLDBGA,"[H323] ooGetNextPort %s port %d \n",
                (type==OOTCP)?"TCP(H245)":(type==OOUDP)?"UDP":"RTP",port); 
     return port;
   }
   else
   {
     OOTRACEERR2("[H323] ooGetNextPort %s failed  \n",
                (type==OOTCP)?"TCP(H245)":(type==OOUDP)?"UDP":"RTP");  
     return OO_FAILED;
   }
}


int ooBindPort (OOH323PortType type, OOSOCKET psocket, char *ip)
{
   int initialPort, bindPort, ret;
   OOIPADDR ipAddrs;

   initialPort = ooGetNextPort (type);
   bindPort = initialPort;
   OOTRACEAST(OOTRCLVLDBGA,"[H323/H245] Select port %d \n",bindPort);   
   ret= ooSocketStrToAddr (ip, &ipAddrs);

   while(1)
   {
      if((ret=ooSocketBind(psocket, ipAddrs, bindPort))==0)
      {
         return bindPort;
      }
      else
      {
         bindPort = ooGetNextPort (type);
         if (bindPort == initialPort) return OO_FAILED;
      }
   }
}

#ifdef _WIN32        
int ooBindOSAllocatedPort(OOSOCKET psocket, char *ip)
{
   OOIPADDR ipAddrs;
   int size, ret;
   struct sockaddr_in name;
   size = sizeof(struct sockaddr_in);
   ret= ooSocketStrToAddr (ip, &ipAddrs);
   if((ret=ooSocketBind(psocket, ipAddrs, 
                     0))==ASN_OK)
   {
      ret = ooSocketGetSockName(psocket, &name, &size);
      if(ret == ASN_OK)
      {
         return name.sin_port;
         
      }
   }

   return OO_FAILED;
}
#endif
