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
#ifndef _OO_H323CDRIVER_H_
#define __OO_H323DRIVER_H_
#include "chan_ooh323.h"
#include "ootypes.h"
#include "ooh323ep.h"
#include "oochannels.h"
#include "ooCalls.h"
#include "ooCapability.h"
#include "ooStackCmds.h"
#define H323_DTMF_RFC2833          (1 << 0)
#define H323_DTMF_Q931             (1 << 1)
#define H323_DTMF_H245ALPHANUMERIC (1 << 2)
#define H323_DTMF_H245SIGNAL       (1 << 3)
#define H323_DTMF_INBAND           (1 << 4)

int ooh323c_start_stack_thread(void);
int ooh323c_stop_stack_thread(void);
int ooh323c_set_capability
   (struct ast_codec_pref *prefs, int capability, int dtmf);
int convertH323CapToAsteriskCap(int cap);
#ifdef VIDEOCAPS
int ooh323c_set_capability_for_call
   (ooCallData *call, struct ast_codec_pref *prefs, int capability, int dtmf,
    struct ast_capabilities * caps);
#else
int ooh323c_set_capability_for_call
   (ooCallData *call, struct ast_codec_pref *prefs, int capability, int dtmf);
#endif

#endif
