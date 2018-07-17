/*
  tr.act.h
  
  Tracing macros for activity functions
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


#ifndef ACTTRH
#define ACTTRH

#ifdef debug
#include "dbsim.h"
/************************************************************************\
*                           ACTIVITY tracing statements                                *
\**************************************************************************************/

#define TRACE_ACTIVITY_setarg  \
   if (TraceLevel >= MAXDBLEVEL-2)  { \
      sprintf(YS__prbpkt,"    Setting argument for %s[%d]\n", \
      aptr->name,YS__ActId(aptr));  \
      YS__SendPrbPkt(TEXTPROBE,aptr->name,YS__prbpkt); \
   }

#define TRACE_ACTIVITY_getarg   \
   if (TraceLevel >= MAXDBLEVEL-2) { \
      sprintf(YS__prbpkt,"    Getting argument from %s[%d]\n", \
      aptr->name,YS__ActId(aptr)); \
      YS__SendPrbPkt(TEXTPROBE,aptr->name,YS__prbpkt); \
   }

#define TRACE_ACTIVITY_fork  \
   if (TraceLevel >= MAXDBLEVEL-2) { \
      sprintf(YS__prbpkt,"    - Forking schedule, parent process is %s[%d]\n",  \
             aptr->parent->name,YS__ActId(aptr->parent)); \
      YS__SendPrbPkt(TEXTPROBE,aptr->name,YS__prbpkt); \
   }

#define TRACE_ACTIVITY_block  \
   if (TraceLevel >= MAXDBLEVEL-2) { \
      sprintf(YS__prbpkt,"    - Blocking schedule, parent process %s[%d] suspends\n", \
             aptr->parent->name,YS__ActId(aptr->parent)); \
      YS__SendPrbPkt(TEXTPROBE,aptr->name,YS__prbpkt); \
   }

#define TRACE_ACTIVITY_schedule1  \
   if (TraceLevel >= MAXDBLEVEL-2)  { \
      sprintf(YS__prbpkt,"    Scheduling activity %s[%d] to occur in %g time units\n", \
              aptr->name,YS__ActId(aptr),timeinc); \
      YS__SendPrbPkt(TEXTPROBE,aptr->name,YS__prbpkt); \
   }

#define TRACE_ACTIVITY_schedule2  \
   if (TraceLevel >= MAXDBLEVEL-2) { \
      sprintf(YS__prbpkt, \
         "    Scheduling activity %s[%d] to wait for semaphore %s[%d]\n",  \
         aptr->name,YS__ActId(aptr),sptr->name,YS__ActId(sptr)); \
      YS__SendPrbPkt(TEXTPROBE,aptr->name,YS__prbpkt); \
   }

#define TRACE_ACTIVITY_schedule3 \
   if (TraceLevel >= MAXDBLEVEL-2) { \
      sprintf(YS__prbpkt, \
         "    - Semaphore %s[%d] = %d; %s is decremented and %s is activated\n", \
         sptr->name,YS__ActId(sptr),sptr->val,sptr->name,aptr->name); \
      YS__SendPrbPkt(TEXTPROBE,sptr->name,YS__prbpkt); \
   }

#define TRACE_ACTIVITY_schedule4  \
   if (TraceLevel >= MAXDBLEVEL-2) { \
      sprintf(YS__prbpkt,"    - Semaphore %s[%d] = %d; %s waits\n",  \
              sptr->name,YS__ActId(sptr),sptr->val,aptr->name); \
      YS__SendPrbPkt(TEXTPROBE,sptr->name,YS__prbpkt); \
   }

#define TRACE_ACTIVITY_schedule5  \
   if (TraceLevel >= MAXDBLEVEL-2) { \
        sprintf(YS__prbpkt, \
           "    Scheduling activity %s[%d] to wait for condition %s[%d]\n", \
           aptr->name,YS__ActId(aptr),cptr->name,YS__QeId(cptr)); \
      YS__SendPrbPkt(TEXTPROBE,aptr->name,YS__prbpkt); \
  }

#define TRACE_ACTIVITY_schedule6  \
   if (TraceLevel >= MAXDBLEVEL-2) { \
        sprintf(YS__prbpkt, \
           "    Scheduling activity %s[%d] to use resource %s[%d] for %g time units\n", \
           aptr->name,YS__ActId(aptr),rptr->name,YS__QeId(rptr),timeinc); \
      YS__SendPrbPkt(TEXTPROBE,aptr->name,YS__prbpkt); \
  }

#define TRACE_ACTIVITY_schedule7  \
   if (TraceLevel >= MAXDBLEVEL-2)  { \
      if (aptr->type == EVTYPE || aptr->type==OSEVTYPE)  \
         sprintf(YS__prbpkt,"    Scheduling event %s[%d] to occur immediately\n", \
                 aptr->name,YS__ActId(aptr)); \
      else if (aptr->type == PROCTYPE || aptr->type==OSPRTYPE || aptr->type==USRPRTYPE) \
         sprintf(YS__prbpkt, \
            "    Scheduling process %s[%d] to start immediately\n",  \
            aptr->name,YS__ActId(aptr));  \
      YS__SendPrbPkt(TEXTPROBE,aptr->name,YS__prbpkt); \
   }

#define TRACE_ACTIVITY_schedule8 \
   if (TraceLevel >= MAXDBLEVEL-2) { \
        sprintf(YS__prbpkt,"    Scheduling activity %s[%d] to wait for flag %s[%d]\n", \
                aptr->name,YS__ActId(aptr),fptr->name,YS__QeId(fptr)); \
      YS__SendPrbPkt(TEXTPROBE,aptr->name,YS__prbpkt); \
  }

#define TRACE_ACTIVITY_schedule11  \
   if (TraceLevel >= MAXDBLEVEL-2)  { \
      sprintf(YS__prbpkt,"    - Condition false, activity scheduled on condition\n"); \
      YS__SendPrbPkt(TEXTPROBE,aptr->name,YS__prbpkt); \
   }

#define TRACE_ACTIVITY_schedule12  \
   if (TraceLevel >= MAXDBLEVEL-2)  { \
      sprintf(YS__prbpkt,"    - Resource ");  \
      if (rptr->status != PENDING) sprintf(YS__prbpkt+strlen(YS__prbpkt), \
         "notified\n");  \
      else sprintf(YS__prbpkt+strlen(YS__prbpkt),"already pending\n");  \
      YS__SendPrbPkt(TEXTPROBE,aptr->name,YS__prbpkt); \
   }

#define TRACE_ACTIVITY_schedule13  \
   if (TraceLevel >= MAXDBLEVEL-2) {  \
      sprintf(YS__prbpkt,"    - Flag not set,activity scheduled to wait on flag\n"); \
      YS__SendPrbPkt(TEXTPROBE,fptr->name,YS__prbpkt); \
   }

#define TRACE_ACTIVITY_schedule14  \
   if (TraceLevel >= MAXDBLEVEL-2) {  \
      sprintf(YS__prbpkt,"    - Flag is set, activity scheduled now\n"); \
      YS__SendPrbPkt(TEXTPROBE,fptr->name,YS__prbpkt); \
   }

#define TRACE_ACTIVITY_schedule15  \
   if (TraceLevel >= MAXDBLEVEL-2)  { \
      sprintf(YS__prbpkt,"    - Condition true, activity scheduled now\n"); \
      YS__SendPrbPkt(TEXTPROBE,aptr->name,YS__prbpkt); \
   }


/**************************************************************************************\
*                             EVENT Tracing Statements                                 *
\**************************************************************************************/

#define TRACE_EVENT_event  \
   if (TraceLevel >= MAXDBLEVEL-2)  { \
      sprintf(YS__prbpkt,"    Creating event %s[%d]\n",eptr->name,YS__ActId(eptr)); \
      YS__SendPrbPkt(TEXTPROBE,eptr->name,YS__prbpkt); \
   }

#define TRACE_EVENT_reschedule1  \
   if (TraceLevel >= MAXDBLEVEL-2)  { \
      sprintf(YS__prbpkt,"    Rescheduling event %s[%d] to occur in %g time units\n", \
              YS__ActEvnt->name,YS__ActId(YS__ActEvnt),timeinc); \
      YS__SendPrbPkt(TEXTPROBE,YS__ActEvnt->name,YS__prbpkt); \
   }

#define TRACE_EVENT_reschedule2  \
   if (TraceLevel >= MAXDBLEVEL-2) { \
      sprintf(YS__prbpkt, \
         "    Rescheduling event %s[%d] to wait for semaphore %s[%d]\n",  \
         YS__ActEvnt->name,YS__ActId(YS__ActEvnt),sptr->name,YS__ActId(sptr)); \
      YS__SendPrbPkt(TEXTPROBE,YS__ActEvnt->name,YS__prbpkt); \
   }

#define TRACE_EVENT_reschedule3 \
   if (TraceLevel >= MAXDBLEVEL-2) { \
      sprintf(YS__prbpkt, \
         "    - Semaphore %s[%d] = %d; %s is decremented and %s is activated\n", \
         sptr->name,YS__ActId(sptr),sptr->val,sptr->name,YS__ActEvnt->name); \
      YS__SendPrbPkt(TEXTPROBE,sptr->name,YS__prbpkt); \
   }

#define TRACE_EVENT_reschedule4  \
   if (TraceLevel >= MAXDBLEVEL-2) { \
      sprintf(YS__prbpkt,"    - Semaphore %s[%d] = %d; %s waits\n",  \
              sptr->name,YS__ActId(sptr),sptr->val,YS__ActEvnt->name); \
      YS__SendPrbPkt(TEXTPROBE,sptr->name,YS__prbpkt); \
   }

#define TRACE_EVENT_reschedule5 \
   if (TraceLevel >= MAXDBLEVEL-2) { \
     sprintf(YS__prbpkt,"    Rescheduling event %s[%d] to wait for flag %s[%d]\n", \
             YS__ActEvnt->name,YS__ActId(YS__ActEvnt),fptr->name,YS__QeId(fptr)); \
      YS__SendPrbPkt(TEXTPROBE,YS__ActEvnt->name,YS__prbpkt); \
  }

#define TRACE_EVENT_reschedule6  \
   if (TraceLevel >= MAXDBLEVEL-2)  { \
      sprintf(YS__prbpkt,"    - Flag ");  \
      if (fptr->status != PENDING) sprintf(YS__prbpkt+strlen(YS__prbpkt), \
         "notified\n");  \
      else sprintf(YS__prbpkt+strlen(YS__prbpkt),"already pending\n");  \
      YS__SendPrbPkt(TEXTPROBE,YS__ActEvnt->name,YS__prbpkt); \
   }

#define TRACE_EVENT_reschedule7  \
   if (TraceLevel >= MAXDBLEVEL-2) { \
     sprintf(YS__prbpkt, \
        "    Scheduling event %s[%d] to wait for condition %s[%d]\n", \
        YS__ActEvnt->name,YS__ActId(YS__ActEvnt),cptr->name,YS__QeId(cptr)); \
      YS__SendPrbPkt(TEXTPROBE,YS__ActEvnt->name,YS__prbpkt); \
  }

#define TRACE_EVENT_reschedule8  \
   if (TraceLevel >= MAXDBLEVEL-2)  { \
      sprintf(YS__prbpkt,"    - Condition ");  \
      if (cptr->status != PENDING) sprintf(YS__prbpkt+strlen(YS__prbpkt), \
         "notified\n");  \
      else sprintf(YS__prbpkt+strlen(YS__prbpkt),"already pending\n");  \
      YS__SendPrbPkt(TEXTPROBE,YS__ActEvnt->name,YS__prbpkt); \
   }

#define TRACE_EVENT_reschedule9  \
   if (TraceLevel >= MAXDBLEVEL-2) { \
     sprintf(YS__prbpkt, \
        "    Rescheduling event %s[%d] to use resource %s[%d] for %g time units\n", \
        YS__ActEvnt->name,YS__ActId(YS__ActEvnt),rptr->name,YS__QeId(rptr),timeinc); \
     YS__SendPrbPkt(TEXTPROBE,YS__ActEvnt->name,YS__prbpkt); \
  }

#define TRACE_EVENT_reschedule10  \
   if (TraceLevel >= MAXDBLEVEL-2)  { \
      sprintf(YS__prbpkt,"    - Resource ");  \
      if (rptr->status != PENDING) sprintf(YS__prbpkt+strlen(YS__prbpkt), \
         "notified\n");  \
      else sprintf(YS__prbpkt+strlen(YS__prbpkt),"already pending\n");  \
      YS__SendPrbPkt(TEXTPROBE,YS__ActEvnt->name,YS__prbpkt); \
   }

#define TRACE_EVENT_reschedule11  \
   if (TraceLevel >= MAXDBLEVEL-2)  { \
      sprintf(YS__prbpkt,"    Rescheduling event %s[%d]\n", \
              YS__ActEvnt->name,YS__ActId(YS__ActEvnt)); \
      YS__SendPrbPkt(TEXTPROBE,YS__ActEvnt->name,YS__prbpkt); \
   }

#define TRACE_EVENT_settype  \
   if (TraceLevel >= MAXDBLEVEL-2)  { \
      sprintf(YS__prbpkt,"    Setting type of event %s[%d] to %d\n", \
         eptr->name,YS__ActId(eptr),etype); \
      YS__SendPrbPkt(TEXTPROBE,eptr->name,YS__prbpkt); \
   }

#define TRACE_EVENT_setdelflg  \
   if (TraceLevel >= MAXDBLEVEL-2)  { \
      sprintf(YS__prbpkt,"    Event %s[%d] set to deleting mode\n", \
         YS__ActEvnt->name,YS__ActId(YS__ActEvnt)); \
      YS__SendPrbPkt(TEXTPROBE,YS__ActEvnt->name,YS__prbpkt); \
   }

#else  /*******************************************************************************/

#define TRACE_ACTIVITY_setarg
#define TRACE_ACTIVITY_getarg
#define TRACE_ACTIVITY_fork
#define TRACE_ACTIVITY_block
#define TRACE_ACTIVITY_schedule1
#define TRACE_ACTIVITY_schedule2
#define TRACE_ACTIVITY_schedule3
#define TRACE_ACTIVITY_schedule4
#define TRACE_ACTIVITY_schedule5
#define TRACE_ACTIVITY_schedule6
#define TRACE_ACTIVITY_schedule7
#define TRACE_ACTIVITY_schedule8
#define TRACE_ACTIVITY_schedule9
#define TRACE_ACTIVITY_schedule11
#define TRACE_ACTIVITY_schedule12

#define TRACE_PROCESS_process
#define TRACE_PROCESS_transfer
#define TRACE_PROCESS_suspend1
#define TRACE_PROCESS_suspend2
#define TRACE_PROCESS_suspend3
#define TRACE_PROCESS_join1
#define TRACE_PROCESS_join2
#define TRACE_PROCESS_join3
#define TRACE_PROCESS_delay
#define TRACE_PROCESS_nice
#define TRACE_PROCESS_waitcpu
#define TRACE_PROCESS_sendmsg1
#define TRACE_PROCESS_msgrecv1
#define TRACE_PROCESS_msgrecv2
#define TRACE_PROCESS_msgrecv3
#define TRACE_PROCESS_msgrecv4
#define TRACE_PROCESS_msgrecv5
#define TRACE_PROCESS_msgrecv6
#define TRACE_PROCESS_msgrecv2
#define TRACE_PROCESS_msgchk1
#define TRACE_PROCESS_msgchk2
#define TRACE_PROCESS_msgchk3

#define TRACE_EVENT_event
#define TRACE_EVENT_reschedule1
#define TRACE_EVENT_reschedule2
#define TRACE_EVENT_reschedule3
#define TRACE_EVENT_reschedule4
#define TRACE_EVENT_reschedule5
#define TRACE_EVENT_reschedule6
#define TRACE_EVENT_reschedule7
#define TRACE_EVENT_reschedule8
#define TRACE_EVENT_reschedule9
#define TRACE_EVENT_reschedule10
#define TRACE_EVENT_reschedule11
#define TRACE_EVENT_settype
#define TRACE_EVENT_setdelflg

#endif  /******************************************************************************/

#endif

