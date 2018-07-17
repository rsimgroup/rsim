/*
  act.c

  Functions related to event/activity manipulation

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


#include "MemSys/simsys.h"
#include "MemSys/cpu.h"
#include "MemSys/tr.act.h"
#include <string.h>
#include <malloc.h>

static char*  procstates[16] = { "LIMBO","READY","DELAYED","WAIT_SEMAPHORE",
                                 "WAIT_FLAG","WAIT_CONDITION",
                                 "WAIT_RESOURCE","USING_RESOURCE",
                                 "RUNNING","BLOCKED","WAIT_JOIN",
                                 "WAIT_MESSAGE","WAIT_BARRIER" };

/*****************************************************************************/
/* ACTIVITY Operations: In YACSIM, there are two types of activities,        */
/* processes and events (RSIM only uses events).  The activity operations    */
/* are those that are work the same on both types.  The descriptors for      */
/* both events and processes are obtained from the descriptors for           */
/* activities by simply adding fields.                                       */
/*****************************************************************************/

int YS__ActId(aptr)            /* Returns the system defined ID or 0 if TraceIDs is 0 */
ACTIVITY *aptr;                /* Pointer to the activity, or NULL                    */
{
   if (TraceIDs) {             /* TraceIDs must be set to get IDs printed in traces   */
      if (aptr) return aptr->id;                     /* aptr points to something */
      else if (YS__ActEvnt) return YS__ActEvnt->id;  /* Called from an active event   */
      else YS__errmsg("Null Activity Referenced");
      return 0;
   }
   else return 0;  
}

void ActivitySetArg(aptr,arptr,arsize) /* Sets the argument pointer of an activity    */
ACTIVITY *aptr;                        /* Pointer to the activity            */
void  *arptr;                          /* Pointer to the new argument        */
int arsize;                            /* Argument size                      */
{
   PSDELAY;

   if (aptr) {                                  /* aptr points to an activity*/
      TRACE_ACTIVITY_setarg;                    /* Setting argument for activity ...  */
      aptr->argptr = arptr;                     /* Set its argument          */
      aptr->argsize = arsize;
   }
   else if (YS__ActEvnt) {                      /* Called from an active event */
      YS__ActEvnt->argptr = arptr;              /* It is setting its own argument     */
      YS__ActEvnt->argsize = arsize;
   }
   else YS__errmsg("Null Activity Referenced");
}

/*****************************************************************************/

char *ActivityGetArg(aptr)           /* Returns the argument pointer of an activity   */
ACTIVITY *aptr;                      /* Pointer to the activity              */
{
   PSDELAY;

   if (aptr) {                                /* aptr points to an activity  */
      TRACE_ACTIVITY_getarg;                  /* Getting argument from activity ...   */
      return aptr->argptr;
   }
   else if (YS__ActEvnt) return YS__ActEvnt->argptr;    /* Active event or process    */
   else YS__errmsg("Null Activity Referenced");
   return NULL;
}

/******************************************************************************/

int ActivityArgSize(aptr)            /* Returns the size of an argument      */
ACTIVITY *aptr;                      /* Pointer to the activity              */
{
   PSDELAY;

   if (aptr) return aptr->argsize;                     /* aptr points to an activity  */
   else if (YS__ActEvnt) return YS__ActEvnt->argsize;  /* Active event or process     */
   else YS__errmsg("Null Activity Referenced");        /* own argument       */
   return 0;
}

/*****************************************************************************/

void ActivitySchedTime(actptr,timeinc,bflg) /* Schedules an activity in the future    */
ACTIVITY *actptr;                           /* Pointer to the activity       */
double timeinc;                             /* Time incremnt                 */
int bflg;                                   /* Block flag - INDEPENDENT,BLOCK,FORK    */
{
   ACTIVITY *aptr;

   PSDELAY;

   if (actptr) aptr = actptr;                 /* actptr points to an activity, use it */
   else if (YS__ActEvnt) {                    /* or an event is rescheduling itself   */
      if (YS__ActEvnt->deleteflag == DELETE)
         YS__errmsg("Can not reschedule a deleting event"); 
      aptr = (ACTIVITY*)YS__ActEvnt;
   }
   else YS__errmsg("Null Activity Referenced"); /* or there is a mistake     */

   TRACE_ACTIVITY_schedule1;  /* Scheduling activity to occur in "timeinc" time units */
   aptr->blkflg = bflg;

   if (aptr->type==PROCTYPE && aptr->status!=LIMBO)
      YS__errmsg("Processes can only be scheduled once");
   else if (aptr->type==EVTYPE && aptr->status!=RUNNING && aptr->status!=LIMBO)
      YS__errmsg("Can not reschedule a pending event");
    else if ((aptr->type==OSPRTYPE || aptr->type==USRPRTYPE) && aptr->status!=LIMBO)
      YS__errmsg("Processes can only be scheduled once");
   else if (aptr->type==OSEVTYPE && aptr->status!=RUNNING && aptr->status!=LIMBO)
      YS__errmsg("Can not reschedule a pending event");

   if (aptr->blkflg!=INDEPENDENT)
      YS__errmsg("Only Independent block-flag supported for ActivitySchedTime in this version");

   if (timeinc == 0.0)  {                                     /* Schedule immediately */
      aptr->time = YS__Simtime;
      YS__RdyListAppend(aptr);
   }
   else if (timeinc > 0.0)  {                               /* Schedule in the future */
      aptr->time = YS__Simtime+timeinc;
      aptr->status = DELAYED;
      if (aptr->statptr) 
         StatrecUpdate(aptr->statptr,(double)DELAYED,YS__Simtime);
      YS__EventListInsert(aptr);
   }
   else
       YS__errmsg("Can not schedule an activity to occur in the past");

}

/*****************************************************************************/

void ActivityCollectStats(aptr) /* Activates automatic statistics collection */
ACTIVITY *aptr;                 /* Pointer to the activity                   */
{
   ACTIVITY *actptr;

   PSDELAY;

   if (aptr) actptr = aptr;
   else if (YS__ActEvnt) actptr = (ACTIVITY*)YS__ActEvnt;
   else YS__errmsg("NULL Activity referenced");

   if (actptr->statptr == NULL) {
      actptr->statptr = NewStatrec(actptr->name,INTERVAL,NOMEANS,HIST,12,1.0,13.0);
      StatrecUpdate(actptr->statptr,(double)(actptr->status),YS__Simtime); 
   }                                                      /* Start the first interval */
   else YS__warnmsg("Process statistics collection already set");
}

/*****************************************************************************/

void ActivityStatRept(actptr)  /* Prints a report of an activity's statistics*/
ACTIVITY *actptr;              /* Pointer to the activity                    */
{
   ACTIVITY *aptr;
   STATREC *srptr;
   double total = 0.0;
   int i;

   PSDELAY;

   if (actptr) aptr = actptr;                 /* actptr points to an activity, use it */
   else if (YS__ActEvnt) aptr = (ACTIVITY*)YS__ActEvnt; /* or use the Active Event    */
   else YS__errmsg("Null Activity Referenced"); /* there is a mistake        */

   srptr = aptr->statptr;
   if (srptr) {                              /* Statistics collection activated       */
      TracePrintTag("statrpt","\nStatistics for activity %s ",aptr->name);
      total = srptr->time1 - srptr->time0;   /* Total = sampling interval    */
      if (total > 0.0) {
         TracePrintTag("statrpt","from time %g to time %g:\n",srptr->time0,srptr->time1);
         for (i=0; i<srptr->bins; i++) {
            if (srptr->hist[i] > 0.0) {
               TracePrintTag("statrpt","   %6.3f time units, %5.2f%s of the sampling interval, ",
                       srptr->hist[i],(srptr->hist[i]/total)*100,"%");
               TracePrintTag("statrpt","were spent in state %-10s\n",procstates[i]);
            }
         }
      }
      else TracePrintTag("statrpt","\n   Zero interval; no statistics available");
      TracePrintTag("statrpt","\n");
   }
   else YS__warnmsg("Statistics not collected; cannot print report");
}

/*****************************************************************************/

STATREC *ActivityStatPtr(aptr) /* Returns a pointer to an activity's  statrec*/
ACTIVITY *aptr;                /* Pointer to the activity                    */
{
   PSDELAY;

   return aptr->statptr;
}


/*****************************************************************************/

ACTIVITY *ActivityGetMyPtr()   /* Returns a pointer to the current activity  */
{
   PSDELAY;

   if (YS__ActEvnt) return (ACTIVITY*)(YS__ActEvnt);
   else YS__errmsg("ActivityGetMyPtr() must be called from within an activity");
   return NULL;
}

/*****************************************************************************/
/* EVENT Operations: Events do not have a stack.  As a result, they are      */
/* like subroutines must terminate and start again at the beginning          */
/* instead of suspending.  The rescheduling operations are an attempt to     */
/* give events some of the suspending properties of processes, but they      */
/* are limited in what they do.                                              */
/*****************************************************************************/

EVENT *NewEvent(ename,bodyname,dflag,etype) /* Creates & returns pointer to an event  */
char *ename;                                /* User defined name             */
func bodyname;                              /* Defining function of the event*/
int dflag;                                  /* DELETE or NODELETE            */
int etype;                                  /* User defined type             */
{
   EVENT *eptr;

   PSDELAY;

   eptr = (EVENT*)YS__PoolGetObj(&YS__EventPool);      /* Get the activity descriptor */
   eptr->id = YS__idctr++;                             /* System assigned unique ID   */
   strncpy(eptr->name,ename,31);                       /* Copy the name      */
   eptr->name[31] = '\0';                              /*   Limited to 31 characters  */
   eptr->type = EVTYPE;                                /* Initialize all fields       */
   eptr->next = NULL;
   eptr->argptr = NULL;
   eptr->argsize = 0;
   eptr->time = 0.0;
   eptr->status = LIMBO;
   eptr->blkflg = INDEPENDENT;
   eptr->statptr = NULL;

   eptr->priority = 0.0;
   eptr->timeleft = 0.0;
   eptr->enterque = 0.0;

   eptr->body = bodyname;
   eptr->deleteflag = dflag;
   eptr->evtype = etype;
   eptr->state = 0;
   TRACE_EVENT_event;                                   /* Creating event ...*/
   return eptr;
}

/*****************************************************************************/

void EventReschedTime(timeinc,st)  /* Reschedules an event to occur in the future     */
double timeinc;                    /* Time increment                         */
int st;                            /* Return state                           */
{
   PSDELAY;

   if (YS__ActEvnt == NULL)
      YS__errmsg("EventReschedule() can only be invoked from within an event body");
   if (YS__ActEvnt->deleteflag == DELETE)
      YS__errmsg("Can not reschedule a deleting event");

   if (timeinc < 0.0) YS__errmsg("Events can not be scheduled in the past");

   TRACE_EVENT_reschedule1; /* Rescheduling activity to occur in "timeinc" time units */
   YS__ActEvnt->state = st;

   if (timeinc == 0.0)  {                                  /* Schedule immediately */
     YS__ActEvnt->time = YS__Simtime;
     YS__RdyListAppend(YS__ActEvnt);
   }
   else {                                                /* Schedule in the future */
     YS__ActEvnt->time = YS__Simtime+timeinc;
     YS__ActEvnt->status = DELAYED;
     if (YS__ActEvnt->statptr)
       StatrecUpdate(YS__ActEvnt->statptr,(double)DELAYED,YS__Simtime);
     YS__EventListInsert(YS__ActEvnt);
   }
}

/*****************************************************************************/

void EventReschedSema(sptr,st)   /* Reschedules an event to wait for a semaphore      */
SEMAPHORE *sptr;                 /* Pointer to the semaphore                 */
int st;                          /* Return state                             */
{
   PSDELAY;

   if (YS__ActEvnt == NULL)
      YS__errmsg("EventReschedule() can only be invoked from within an event body");
   if (YS__ActEvnt->deleteflag == DELETE)
      YS__errmsg("Can not reschedule a deleting event");

   TRACE_EVENT_reschedule2;          /* Rescheduling event to wait for semaphore      */
   YS__ActEvnt->state = st;
   if (sptr->val > 0)  {
      TRACE_EVENT_reschedule3;       /* Semaphore decremented and activity activated  */
      sptr->val--;
      YS__RdyListAppend(YS__ActEvnt);
   }
   else  {
      TRACE_EVENT_reschedule4;       /* Semaphore <= 0, activity waits       */
      YS__ActEvnt->status = WAIT_SEM;
      if (YS__ActEvnt->statptr) 
         StatrecUpdate(YS__ActEvnt->statptr,(double)WAIT_SEM,YS__Simtime);
      YS__QueuePutTail(sptr,YS__ActEvnt);
   }
}

/*****************************************************************************/

int EventGetType(eptr)        /* Returns the event's type                    */
EVENT *eptr;                  /* Pointer to the event, or NULL               */
{
   PSDELAY;

   if (eptr) return eptr->evtype;                     /* eptr points to an event      */
   else if (YS__ActEvnt) return YS__ActEvnt->evtype;  /* Called from the event        */
   else YS__errmsg("Null Activity Referenced");
   return 0;
}

/*****************************************************************************/

void EventSetType(eptr,etype)      /* Sets the event's type                  */
EVENT *eptr;                       /* Pointer to the event                   */
int etype;                         /* Event's new type                       */
{
   PSDELAY;

   TRACE_EVENT_settype;
   if (eptr) eptr->evtype = etype;                    /* eptr points to an event      */
   else if (YS__ActEvnt) YS__ActEvnt->evtype = etype; /* Called from the event        */
   else YS__errmsg("Null Activity Referenced");
}

/*****************************************************************************/

int EventGetDelFlag(eptr)          /* Returns DELETE (1) or NODELETE (0)     */
EVENT *eptr;                       /* Pointer to an event                    */
{
   PSDELAY;

   if (eptr) return eptr->deleteflag;              /* eptr points to an event  */
   else if (YS__ActEvnt) return YS__ActEvnt->deleteflag;  /* Called from event */
   else YS__errmsg("Null Activity Referenced");
   return 0;
}
 
/*****************************************************************************/

void EventSetDelFlag()             /* Makes an event deleting                */
{
   if (YS__ActEvnt == NULL) 
      YS__errmsg("EventSetDelFlag() not called from within an event");
   if (YS__ActEvnt->status != RUNNING)
      YS__errmsg("Changing Delete Flag of a scheduled event");
   TRACE_EVENT_setdelflg;
   YS__ActEvnt->deleteflag = DELETE;
}

/*****************************************************************************/

int EventGetState()              /* Returns the state set by EventSetState() */
{
   PSDELAY;

   if (YS__ActEvnt) return YS__ActEvnt->state;
   else YS__errmsg("EventGetState() must be called from within an active event");
   return 0;
}

/*****************************************************************************/

void EventSetState(eptr,st)      /* Sets state used to designate a return point       */
EVENT *eptr;                     /* Pointer to an event                      */
int st;                          /* New state value                          */
{
   PSDELAY;

   if (eptr == NULL )            /* Called from within the event             */
      if (YS__ActEvnt) YS__ActEvnt->state = st;
      else YS__errmsg("EventSetState() has NULL pointer, but not called from an event");
   else eptr->state = st;
}
