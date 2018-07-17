/*
  tr.evlst.h

  Event-list tracing

  */
/*****************************************************************************/
/* This file is part of the RSIM Simulator, and is based on earlier code     */
/* from RPPT: The Rice Parallel Processing Testbed.                          */
/*                                                                           */
/******************************************************************************/
/* University of Illinois/NCSA Open Source License                            */
/*                                                                            */
/* Copyright (c) 2002 The Board of Trustees of the University of Illinois and */
/* William Marsh Rice University                                              */
/*                                                                            */
/* All rights reserved.                                                       */
/*                                                                            */
/* Developed by: Professor Sarita Adve's RSIM research group                  */
/*               University of Illinois at Urbana-Champaign and William Marsh */
/*                 Rice University                                            */
/*               http://www.cs.uiuc.edu/rsim and                              */
/*                 http://www.ece.rice.edu/~rsim/dist.html                    */
/*                                                                            */
/* Permission is hereby granted, free of charge, to any person obtaining a    */
/* copy of this software and associated documentation files (the "Software"), */
/* to deal with the Software without restriction, including without           */
/* limitation the rights to use, copy, modify, merge, publish, distribute,    */
/* sublicense, and/or sell copies of the Software, and to permit persons to   */
/* whom the Software is furnished to do so, subject to the following          */
/* conditions:                                                                */
/*                                                                            */
/*     * Redistributions of source code must retain the above copyright       */
/* notice, this list of conditions and the following disclaimers.             */
/*                                                                            */
/*     * Redistributions in binary form must reproduce the above copyright    */
/* notice, this list of conditions and the following disclaimers in the       */
/* documentation and/or other materials provided with the distribution.       */
/*                                                                            */
/*     * Neither the names of Professor Sarita Adve's RSIM research group,    */
/* the University of Illinois at Urbana-Champaign, William Marsh Rice         */
/* University, nor the names of its contributors may be used to endorse or    */
/* promote products derived from this Software without specific prior written */
/* permission.                                                                */
/*                                                                            */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR */
/* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,   */
/* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL    */
/* THE CONTRIBUTORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR  */
/* OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,      */
/* ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR      */
/* OTHER DEALINGS WITH THE SOFTWARE.                                          */
/******************************************************************************/


#ifndef EVLSTTRH
#define EVLSTTRH

#ifdef debug
#include "dbsim.h"
/************************************************************************\
*                           EVENT LIST tracing statements                              *
\**************************************************************************************/

#define TRACE_EVLST_gethead1  \
   if (TraceLevel >= MAXDBLEVEL) { \
      sprintf(YS__prbpkt, \
         "        Event List empty\n"); \
      YS__SendPrbPkt(TEXTPROBE,"EventList",YS__prbpkt); \
   }

#define TRACE_EVLST_gethead2  \
   if (TraceLevel >= MAXDBLEVEL) { \
      sprintf(YS__prbpkt,"        Getting %s[%d] from the head of the Event List\n",  \
              retptr->name,YS__QeId(retptr)); \
      YS__SendPrbPkt(TEXTPROBE,"EventList",YS__prbpkt); \
   }

#define TRACE_EVLST_headval  \
   if (TraceLevel >= MAXDBLEVEL) { \
      sprintf(YS__prbpkt,"        Value of the Event List head = %g\n",  \
              retval); \
      YS__SendPrbPkt(TEXTPROBE,"EventList",YS__prbpkt); \
   }

#define TRACE_EVLST_delete  \
   if (TraceLevel >= MAXDBLEVEL) { \
      sprintf(YS__prbpkt,"        Deleting element %s[%d] from the Event List\n", \
              aptr->name,YS__QeId(aptr)); \
      YS__SendPrbPkt(TEXTPROBE,"EventList",YS__prbpkt); \
   }

#define TRACE_EVLST_show  \
   if (TraceLevel >= MAXDBLEVEL)  \
      YS__EventListPrint();

#define TRACE_EVLST_init1  \
   if (TraceLevel >= MAXDBLEVEL) { \
      sprintf(YS__prbpkt,"Using calendar queue implementation for the event list\n"); \
      YS__SendPrbPkt(TEXTPROBE,"EventList",YS__prbpkt); \
   }

#define TRACE_EVLST_init2  \
   if (TraceLevel >= MAXDBLEVEL) { \
      sprintf(YS__prbpkt, \
         "Using simple linear search implementation for the event list\n"); \
      YS__SendPrbPkt(TEXTPROBE,"EventList",YS__prbpkt); \
   }

#define TRACE_EVLST_insert  \
   if (TraceLevel >= MAXDBLEVEL) { \
      sprintf(YS__prbpkt,"        Inserting %s[%d] with time %g into the Event List\n", \
              aptr->name,YS__QeId(aptr),aptr->time); \
      YS__SendPrbPkt(TEXTPROBE,"EventList",YS__prbpkt); \
   }


#else  /*******************************************************************************/

#define TRACE_EVLST_gethead1
#define TRACE_EVLST_gethead2
#define TRACE_EVLST_headval
#define TRACE_EVLST_delete
#define TRACE_EVLST_show
#define TRACE_EVLST_init1
#define TRACE_EVLST_init2
#define TRACE_EVLST_insert

#endif  /******************************************************************************/


#endif


