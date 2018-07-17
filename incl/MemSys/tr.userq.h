/*
  tr.userq.h

  Queue management statistics

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

  
#ifndef UQUEUESTRH
#define UQUEUESTRH

#ifdef debug
#include "dbsim.h"
/*****************************************************************************\
*                                QUEUE tracing statements                    *
\*****************************************************************************/

#define TRACE_QUEUE_gethead1  \
   if (TraceLevel >= MAXDBLEVEL-1) { \
      sprintf(YS__prbpkt,"        Getting %s[%d] from head of queue %s[%d]\n",  \
              (qeptr)->name,YS__QeId(qptr),qptr->name,YS__QeId(qptr)); \
      YS__SendPrbPkt(TEXTPROBE,qptr->name,YS__prbpkt); \
   }

#define TRACE_QUEUE_gethead2  \
   if (TraceLevel >= MAXDBLEVEL-1) { \
      sprintf(YS__prbpkt, \
         "        Queue %s[%d] empty\n",qptr->name,YS__QeId(qptr)); \
      YS__SendPrbPkt(TEXTPROBE,qptr->name,YS__prbpkt); \
   }

#define TRACE_QUEUE_gettail1  \
   if (TraceLevel >= MAXDBLEVEL-1) { \
      sprintf(YS__prbpkt,"        Getting %s[%d] from tail of queue %s[%d]\n",  \
              qeptr->name,YS__QeId(qeptr),qptr->name,YS__QeId(qptr)); \
      YS__SendPrbPkt(TEXTPROBE,qptr->name,YS__prbpkt); \
   }

#define TRACE_QUEUE_gettail2  \
   if (TraceLevel >= MAXDBLEVEL-1) { \
      sprintf(YS__prbpkt, \
         "        Queue %s[%d] empty\n",qptr->name,YS__QeId(qptr)); \
      YS__SendPrbPkt(TEXTPROBE,qptr->name,YS__prbpkt); \
   }

#define TRACE_QUEUE_puttail  \
   if (TraceLevel >= MAXDBLEVEL-1) { \
      sprintf(YS__prbpkt,"        Putting %s[%d] on the tail of queue %s[%d]\n",  \
              qeptr->name,YS__QeId(qeptr),qptr->name,YS__QeId(qptr)); \
      YS__SendPrbPkt(TEXTPROBE,qptr->name,YS__prbpkt); \
   }

#define TRACE_QUEUE_puthead  \
   if (TraceLevel >= MAXDBLEVEL-1) { \
      sprintf(YS__prbpkt,"        Putting %s[%d] at the head of queue %s[%d]\n", \
              qeptr->name,YS__QeId(qeptr),qptr->name,YS__QeId(qptr)); \
      YS__SendPrbPkt(TEXTPROBE,qptr->name,YS__prbpkt); \
   }

#define TRACE_QUEUE_takeout  \
   if (TraceLevel >= MAXDBLEVEL-1) { \
      sprintf(YS__prbpkt,"        Taking element %s[%d] from queue %s[%d]\n",  \
              qeptr->name,YS__QeId(qeptr),qptr->name,YS__QeId(qptr)); \
      YS__SendPrbPkt(TEXTPROBE,qptr->name,YS__prbpkt); \
   }

#define TRACE_QUEUE_checkelem1  \
   if (TraceLevel >= MAXDBLEVEL-1) { \
      sprintf(YS__prbpkt,"        Element %s[%d] in %s[%d]\n",  \
           qeptr->name,YS__QeId(qeptr),qptr->name,YS__QeId(qptr)); \
      YS__SendPrbPkt(TEXTPROBE,qptr->name,YS__prbpkt); \
   }

#define TRACE_QUEUE_checkelem2  \
   if (TraceLevel >= MAXDBLEVEL-1) { \
      sprintf(YS__prbpkt,"        Element %s[%d] not in %s[%d]\n",  \
           qeptr->name,YS__QeId(qeptr),qptr->name,YS__QeId(qptr)); \
      YS__SendPrbPkt(TEXTPROBE,qptr->name,YS__prbpkt); \
   }

#define TRACE_QUEUE_next  \
   if (TraceLevel >= MAXDBLEVEL-1)  \
   {  \
      if (qeptr) {  \
         sprintf(YS__prbpkt,"        The element after %s[%d] in queue %s[%d] is "  \
                 ,qeptr->name,YS__QeId(qeptr),qptr->name,YS__QeId(qptr));  \
         if (eptr == NULL) sprintf(YS__prbpkt+strlen(YS__prbpkt),"NULL\n");  \
         else sprintf(YS__prbpkt+strlen(YS__prbpkt), \
                  "%s[%d]\n",eptr->name,YS__QeId(eptr));  \
      }  \
      else {  \
         sprintf(YS__prbpkt,"        The element at the head of queue %s[%d] is ",  \
                 qptr->name,YS__QeId(qptr));  \
         if (eptr == NULL) sprintf(YS__prbpkt+strlen(YS__prbpkt),"NULL\n");  \
         else sprintf(YS__prbpkt+strlen(YS__prbpkt), \
                 "%s[%d]\n",eptr->name,YS__QeId(eptr));  \
      }  \
      YS__SendPrbPkt(TEXTPROBE,qptr->name,YS__prbpkt); \
   }

#define TRACE_QUEUE_prev  \
   if (TraceLevel >= MAXDBLEVEL-1)  \
   {  \
      if (qeptr) {  \
         sprintf(YS__prbpkt,"        The element before %s[%d] in queue %s[%d] is "  \
                 ,qeptr->name,YS__QeId(qeptr),qptr->name,YS__QeId(qptr));  \
         if (eptr == NULL) sprintf(YS__prbpkt+strlen(YS__prbpkt),"NULL\n");  \
         else sprintf(YS__prbpkt+strlen(YS__prbpkt), \
                  "%s[%d]\n",eptr->name,YS__QeId(eptr));  \
      }  \
      else {  \
         sprintf(YS__prbpkt,"        The element at the tail of queue %s[%d] is ",  \
                 qptr->name,YS__QeId(qptr));  \
         if (eptr == NULL) sprintf(YS__prbpkt+strlen(YS__prbpkt),"NULL\n");  \
         else sprintf(YS__prbpkt+strlen(YS__prbpkt), \
                 "%s[%d]\n",eptr->name,YS__QeId(eptr));  \
      }  \
      YS__SendPrbPkt(TEXTPROBE,qptr->name,YS__prbpkt); \
   }

#define TRACE_QUEUE_show(ptr)  \
   if (TraceLevel >= MAXDBLEVEL-1) YS__QueuePrint(ptr); 

#define TRACE_QUEUE_headvalue  \
   if (TraceLevel >= MAXDBLEVEL-1) { \
      sprintf(YS__prbpkt,"        Value of head of queue %s[%d] = %g\n",  \
              qptr->name,YS__QeId(qptr),retval); \
      YS__SendPrbPkt(TEXTPROBE,qptr->name,YS__prbpkt); \
   }

#define TRACE_QUEUE_insert  \
   if (TraceLevel >= MAXDBLEVEL-1) { \
      sprintf(YS__prbpkt,"        Inserting %s[%d] with time %g into queue %s[%d]\n", \
              aptr->name,YS__QeId(aptr),aptr->time,qptr->name,YS__QeId(qptr)); \
      YS__SendPrbPkt(TEXTPROBE,qptr->name,YS__prbpkt); \
   }

#define TRACE_QUEUE_enter  \
   if (TraceLevel >= MAXDBLEVEL-1) { \
      sprintf(YS__prbpkt,"        Entering %s[%d] with time %g into queue %s[%d]\n", \
              aptr->name,YS__QeId(aptr),aptr->time,qptr->name,YS__QeId(qptr)); \
      YS__SendPrbPkt(TEXTPROBE,qptr->name,YS__prbpkt); \
   }

#define TRACE_QUEUE_prinsert  \
   if (TraceLevel >= MAXDBLEVEL-1) { \
      sprintf(YS__prbpkt, \
         "        Inserting %s[%d] with resource priority %g into queue %s[%d]\n",\
              aptr->name,YS__QeId(aptr), \
              aptr->priority, \
              qptr->name,YS__QeId(qptr)); \
      YS__SendPrbPkt(TEXTPROBE,qptr->name,YS__prbpkt); \
   }

#define TRACE_QUEUE_prenter  \
   if (TraceLevel >= MAXDBLEVEL-1) { \
      sprintf(YS__prbpkt, \
         "        Entering %s[%d] with resource priority %g into queue %s[%d]\n", \
              aptr->name,YS__QeId(aptr), \
              aptr->priority, \
              qptr->name,YS__QeId(qptr)); \
      YS__SendPrbPkt(TEXTPROBE,qptr->name,YS__prbpkt); \
   }

#define TRACE_QUEUE_reset  \
   if (TraceLevel >= MAXDBLEVEL-1) { \
      sprintf(YS__prbpkt, \
         "        Clearing queue %s[%d]\n",qptr->name,YS__QeId(qptr)); \
      YS__SendPrbPkt(TEXTPROBE,qptr->name,YS__prbpkt); \
   }

#define TRACE_PRQUEUE_new  \
   if (TraceLevel >= MAXDBLEVEL-1)  { \
      sprintf(YS__prbpkt, \
         "        Creating queue %s[%d]\n",prqptr->name,YS__QeId(prqptr)); \
      YS__SendPrbPkt(TEXTPROBE,prqptr->name,YS__prbpkt); \
   }

/************************************************************************\
*                            SEMAPHORE tracing statements                              *
\**************************************************************************************/

#define TRACE_SEMAPHORE_new  \
   if (TraceLevel >= MAXDBLEVEL-2) { \
      sprintf(YS__prbpkt,"    Creating semaphore %s[%d] with value %d\n",  \
              semptr->name,YS__QeId(semptr),semptr->val); \
      YS__SendPrbPkt(TEXTPROBE,semptr->name,YS__prbpkt); \
   }

#define TRACE_SEMAPHORE_init  \
   if (TraceLevel >= MAXDBLEVEL-2) { \
      sprintf(YS__prbpkt,"    Initializing semaphore %s[%d]; new value = %d\n",  \
              sptr->name,YS__QeId(sptr),i); \
      YS__SendPrbPkt(TEXTPROBE,sptr->name,YS__prbpkt); \
   }

#define TRACE_SEMAPHORE_p1  \
   if (TraceLevel >= MAXDBLEVEL-2)  { \
      YS__SendPrbPkt(TEXTPROBE,sptr->name,YS__prbpkt); \
   }

#define TRACE_SEMAPHORE_p2  \
   if (TraceLevel >= MAXDBLEVEL-2) { \
      YS__SendPrbPkt(TEXTPROBE,sptr->name,YS__prbpkt); \
   }


#define TRACE_SEMAPHORE_v1p  \
   if (TraceLevel >= MAXDBLEVEL-2)  { \
      if (YS__ActEvnt != NULL) { \
         sprintf(YS__prbpkt,"    Event %s[%d] signalling semaphore %s[%d]\n", \
                 YS__ActEvnt->name,YS__ActId(YS__ActEvnt),sptr->name,YS__QeId(sptr)); \
         sprintf(YS__prbpkt+strlen(YS__prbpkt), \
            "    - No processes or events waiting; new semaphore value = %d\n", \
                 sptr->val); \
      }  \
      if (YS__ActEvnt == NULL) { \
         sprintf(YS__prbpkt,"    User routine signalling semaphore %s[%d]\n", \
                 sptr->name,YS__QeId(sptr)); \
         sprintf(YS__prbpkt+strlen(YS__prbpkt), \
            "    - No processes or events waiting; new semaphore value = %d\n", \
                 sptr->val); \
      }  \
      YS__SendPrbPkt(TEXTPROBE,sptr->name,YS__prbpkt); \
   }

#define TRACE_SEMAPHORE_v2p  \
   if (TraceLevel >= MAXDBLEVEL-2) { \
      if (YS__ActEvnt != NULL)  \
         sprintf(YS__prbpkt,"    Event %s[%d] signalling semaphore %s[%d]\n",  \
                 YS__ActEvnt->name,YS__ActId(YS__ActEvnt),sptr->name,YS__QeId(sptr)); \
      if (YS__ActEvnt == NULL)  \
         sprintf(YS__prbpkt,"    User routine signalling semaphore %s[%d]\n",  \
                 sptr->name,YS__QeId(sptr)); \
      YS__SendPrbPkt(TEXTPROBE,sptr->name,YS__prbpkt); \
   }

#define TRACE_SEMAPHORE_v3  \
   if (TraceLevel >= MAXDBLEVEL-2) { \
      sprintf(YS__prbpkt, \
         "    - Activity %s[%d] released; semaphore value unchanged\n", \
              aptr->name,YS__ActId(aptr)); \
      YS__SendPrbPkt(TEXTPROBE,aptr->name,YS__prbpkt); \
   }

#define TRACE_SEMAPHORE_s1p  \
   if (TraceLevel >= MAXDBLEVEL-2)  { \
      if (YS__ActEvnt != NULL) { \
         sprintf(YS__prbpkt,"    Event %s[%d] setting semaphore %s[%d]\n", \
                 YS__ActEvnt->name,YS__ActId(YS__ActEvnt),sptr->name,YS__QeId(sptr)); \
         sprintf(YS__prbpkt+strlen(YS__prbpkt), \
            "    - No processes or events waiting; new semaphore value = %d\n", \
                 sptr->val); \
      }  \
      if (YS__ActEvnt == NULL) { \
         sprintf(YS__prbpkt,"    User routine setting semaphore %s[%d]\n", \
                 sptr->name,YS__QeId(sptr)); \
         sprintf(YS__prbpkt+strlen(YS__prbpkt), \
            "    - No processes or events waiting; new semaphore value = %d\n", \
                 sptr->val); \
      }  \
      YS__SendPrbPkt(TEXTPROBE,sptr->name,YS__prbpkt); \
   }

#define TRACE_SEMAPHORE_s2p  \
   if (TraceLevel >= MAXDBLEVEL-2) { \
      if (YS__ActEvnt != NULL)  \
         sprintf(YS__prbpkt,"    Event %s[%d] setting semaphore %s[%d]\n",  \
                 YS__ActEvnt->name,YS__ActId(YS__ActEvnt),sptr->name,YS__QeId(sptr)); \
      if (YS__ActEvnt == NULL)  \
         sprintf(YS__prbpkt,"    User routine setting semaphore %s[%d]\n",  \
                 sptr->name,YS__QeId(sptr)); \
      YS__SendPrbPkt(TEXTPROBE,sptr->name,YS__prbpkt); \
   }





#else  /*******************************************************************************/

#define TRACE_QUEUE_gethead1
#define TRACE_QUEUE_gethead2
#define TRACE_QUEUE_gettail1
#define TRACE_QUEUE_gettail2
#define TRACE_QUEUE_puttail
#define TRACE_QUEUE_puthead
#define TRACE_QUEUE_takeout
#define TRACE_QUEUE_checkelem1
#define TRACE_QUEUE_checkelem2
#define TRACE_QUEUE_next
#define TRACE_QUEUE_prev
#define TRACE_QUEUE_show(ptr)
#define TRACE_QUEUE_headvalue
#define TRACE_QUEUE_insert
#define TRACE_QUEUE_enter
#define TRACE_QUEUE_prinsert
#define TRACE_QUEUE_prenter
#define TRACE_QUEUE_reset
#define TRACE_PRQUEUE_new

#define TRACE_SEMAPHORE_new
#define TRACE_SEMAPHORE_init
#define TRACE_SEMAPHORE_reset
#define TRACE_SEMAPHORE_p1
#define TRACE_SEMAPHORE_p2
#define TRACE_SEMAPHORE_v1p
#define TRACE_SEMAPHORE_v2p
#define TRACE_SEMAPHORE_v3
#define TRACE_SEMAPHORE_s1p
#define TRACE_SEMAPHORE_s2p


#endif  /******************************************************************************/


#endif

