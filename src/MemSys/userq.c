/*
  userq.c

  This file provides the resource queues and sempahores used in various
  places in the memory system simulator.

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
#include "MemSys/tr.userq.h"
#include "MemSys/dbsim.h"

#include <string.h>

/*****************************************************************************/
/* QUEUE Operations: Queues implement the basic linked lists used by         */
/* several other objects (e.g., semaphores).  They are simple single-link    */
/* lists with a pointer to the head and tail and links that point in the     */
/* direction from head to tail.  The generic element for these queues is     */
/* a QE, a structure that forms the base of several different types of       */
/* objects that can to put in queues (e.g., events, queues themselves).      */
/* There are several basic operations to add elements to and delete them     */
/* from queues.  Four special insertion operations insert events ordered by  */
/* one of two keys                                                           */
/*****************************************************************************/


QUEUE *YS__NewQueue(qname)    /* Create and return a pointer to a new queue           */
char *qname;                  /* User defined name                                    */
{
   QUEUE *qptr;

   qptr = (QUEUE*)YS__PoolGetObj(&YS__QueuePool);

   qptr->id = YS__idctr++;         /* QE fields */
   qptr->type = QUETYPE;

   strncpy(qptr->name,qname,31);   /* QUEUE fields */
   qptr->name[31] = '\n';
   qptr->head = NULL;
   qptr->tail = NULL;
   qptr->size = 0;

   return qptr;
}

char  *YS__QueueGetHead(qptr)  /* Removes & returns a pointer to the head of a queue  */
SYNCQUE *qptr;                 /* Pointer to the queue                                */
{
   QE *qeptr;

   if (qptr == NULL)
       YS__errmsg("Null queue pointer passed to QueueGetHead");
   if (qptr->head != NULL)  {         /* Queue not empty                              */
      qeptr = qptr->head;                  /* Get the head element                    */
      qptr->head = qeptr->next;            /* Head points to the new head element     */
      qeptr->next = NULL;                  /* Clear next pointer of removed element   */
      if (qptr->head == NULL) 
         qptr->tail = NULL;                /* Queue now empty                         */
      qptr->size--;                   /* One fewer element in the queue               */
      if (qptr->type == SYNCQTYPE) {  /* Resource statistics updated in userq.c       */
         if (qptr->lengthstat)        /* Queue length statistics collectd             */
            StatrecUpdate(qptr->lengthstat,(double)qptr->size,YS__Simtime);
         if (qptr->timestat)          /* Queue time statistics collected              */
            StatrecUpdate(qptr->timestat, 
               YS__Simtime - ((STATQE*)qeptr)->enterque,1.0);
      }
      TRACE_QUEUE_gethead1;           /* Getting from head of queue                   */
   }
   else  {
      TRACE_QUEUE_gethead2;           /* Queue empty                                  */
      qeptr = NULL;                   /* Return NULL                                  */
   }
   TRACE_QUEUE_show(qptr);            /* Prints the contents of the queue             */
   return (char*)qeptr;
}

void YS__QueuePutHead(qptr,qeptr)  /* Puts a queue element at the head of a queue     */
QUEUE  *qptr;                      /* Pointer to the queue                            */
QE *qeptr;                         /* Pointer the element to be added                 */
{
   SYNCQUE *sqptr = (SYNCQUE*)qptr;  /* Only cast qptr once                           */
   STATQE  *sqeptr = (STATQE*)qeptr; /* Only cast qeptr once                          */

   TRACE_QUEUE_puthead;              /* Putting at head of queue                      */
   if (qptr == NULL)
       YS__errmsg("Null queue pointer passed to QueuePutHead");
   if (qeptr == NULL)
       YS__errmsg("Null queue element pointer passed to QueuePutHead");
   if (qptr->head == NULL) {         /* The queue is empty */
      qptr->tail = qeptr;                 /* Head and tail elements are the same      */
      qeptr->next = NULL;                 /*    (just to be sure)                     */
   }
   else {                            /* Adding to the head of a non-empty queue       */
      qeptr->next = qptr->head;           /* Set forward link of new head element     */
   }                                      /*    (just to be sure)                     */
   qptr->head = qeptr;               /* New element in now the head                   */
   qptr->size++;                     /* One more element in the queue                 */

   if (qptr->type == SYNCQTYPE) {    /* Resource statistics updated in userq.c        */
      sqeptr->enterque = YS__Simtime;/* For time in queue statistics                  */
      if (sqptr->lengthstat)         /* Queue length statistics collected             */
         StatrecUpdate(sqptr->lengthstat,(double)qptr->size,YS__Simtime);
   }
   TRACE_QUEUE_show(qptr);           /* Prints the contents of the queue              */
}

void YS__QueuePutTail(qptr,qeptr) /* Puts a queue element at the tail of a queue      */
QUEUE *qptr;                      /* Pointer to the queue                             */
QE    *qeptr;                     /* Pointer the element to be added                  */
{
   SYNCQUE *sqptr = (SYNCQUE*)qptr;  /* Only cast qptr once                           */
   STATQE  *sqeptr = (STATQE*)qeptr; /* Only cast qeptr once                          */

   TRACE_QUEUE_puttail;              /* Putting on the tail of queue                  */
   if (qptr == NULL)
       YS__errmsg("Null queue pointer passed to QueuePutTail");
   if (qeptr == NULL) YS__errmsg("Null queue element pointer passed to QueuePutTail");
   if (qptr->head == NULL) {         /* The queue is empty                            */
      qptr->head = qeptr;              /* Tail and head elements are the same         */  
      qeptr->next = NULL;              /*    (just to be sure)                        */
   }
   else {                            /* Adding to the tail of a non-empty queue       */
      qptr->tail->next = qeptr;        /* Set forward link of old tail element        */
      qeptr->next = NULL;              /* Clear next pointer of new tail element      */
   }                                   /*    (just to be sure)                        */
   qptr->tail = qeptr;               /* New element is now the tail */
   qptr->size++;                     /* One more element in the queue                 */

   if (qptr->type == SYNCQTYPE) {    /* Resource statistics done in userq.c           */
      sqeptr->enterque = YS__Simtime;/* For time in queue statistics                  */
      if (sqptr->lengthstat)         /* Queue length statistics collected             */
         StatrecUpdate(sqptr->lengthstat,(double)qptr->size,YS__Simtime);
   }
   TRACE_QUEUE_show(qptr);           /* Prints the contents of the queue              */
}

char *YS__QueueNext(qptr,qeptr)       /* Get pointer to element following an element  */
QUEUE *qptr;                          /* Pointer to the queue                         */
QE *qeptr;                            /* Pointer to an element in the queue           */
{
   QE *eptr;

   if (qptr == NULL) YS__errmsg("Null queue pointer passed to QueueNext");
   if (qptr->size <= 0) eptr = NULL;  /* Return NULL if the queue is empty            */
   else if (qeptr == NULL) 
      eptr = (QE*)(qptr->head);       /* Return pointer to head if qeptr is NULL      */
   else eptr = (QE*)(qeptr->next);    /* Return next element otherwise                */
   TRACE_QUEUE_next;                  /* The element after ..., The head of ...       */
   return (char*)eptr;
}  

int YS__QueueCheckElement(qptr,qeptr) /* Checks to see if an element is in a queue    */
QUEUE *qptr;                          /* Pointer to the queue                         */
QE    *qeptr;                         /* Pointer to element                           */
{
   QE *ptr;

   if (qptr == NULL) 
      YS__errmsg("Null queue pointer passed to QueueCheckElement");
   if (qeptr == NULL) 
      YS__errmsg("Null queue element pointer passed to QueueCheckElement");

   for (ptr = qptr->head; ptr != NULL; ptr = ptr->next)
      if (ptr == qeptr) {
         TRACE_QUEUE_checkelem1;      /* Element in queue                             */
         return 1;
      }
   TRACE_QUEUE_checkelem2;            /* Element not it queue                         */
   return 0;
}

int YS__QueueDelete(qptr,qeptr)       /* Removes a specified element from the queue   */
QUEUE *qptr;                          /* Pointer to the queue                         */
QE *qeptr;                            /* Pointer to element to be removed             */
/* Returns 1 if the element is in the queue and 0 if not                              */
{  
   QE *eptr, *preveptr;
   SYNCQUE *sqptr = (SYNCQUE*)qptr;   /* Only cast qptr once                          */
   STATQE  *sqeptr = (STATQE*)qeptr;  /* Only cast qeptr once                         */

   if (qptr == NULL) YS__errmsg("Null queue pointer passed to QueueDelete");
   if (qeptr == NULL) YS__errmsg("Null queue element pointer passed to QueueDelete");
   TRACE_QUEUE_takeout;               /* Taking element from queue q                  */

   preveptr = qptr->head;             /* Start at the head                            */
   for (eptr = qptr->head; eptr != NULL; eptr = eptr->next)  { /* and search for it   */
      if (eptr == qeptr)  {                                    /* Found it            */
         if (eptr == (qptr->head))    /* Element at the head of the queue             */
            qptr->head = eptr->next;
         else                         /* Element in the middle of the queue           */
            preveptr->next = eptr->next;
         if (eptr == (qptr->tail)) {  /* Element at the tail of the queue             */
             if (qptr->size == 1)     /* this was the only element in the queue       */
               preveptr = NULL;
             qptr->tail = preveptr;
         }
         eptr->next = NULL;
         qptr->size--;                 /* Queue has one fewer element                 */
         TRACE_QUEUE_show(qptr);       /* Prints the contents of the queue            */

         if (qptr->type == SYNCQTYPE) {/* Resource statistics updated in userq.c      */
            if (sqptr->lengthstat)     /* Queue length statistics collected           */
               StatrecUpdate(sqptr->lengthstat,(double)qptr->size,YS__Simtime);
            if (sqptr->timestat)       /* Queue time statistics collected             */
               StatrecUpdate(sqptr->timestat, YS__Simtime - sqeptr->enterque,1.0);
         }
         return 1;                     /* Element deleted                             */
      }
      preveptr = eptr;
   }
   TRACE_QUEUE_show(qptr);             /* Prints the contents of the queue            */
   return 0;                           /* Element not in queue                        */
}


double YS__QueueHeadval(qptr)    /* Returns the time of the first queue element       */
QUEUE *qptr;                     /* Pointer to the queue                              */
{
   double retval;

   if (qptr->head != NULL)
      retval = ((ACTIVITY*)(qptr->head))->time;     
   else retval = -1.0;
   TRACE_QUEUE_headvalue;        /* Value of head of queue is                         */
   return retval;
}

void YS__QueuePrint(qptr)       /* Prints the contents of a queue                     */
QUEUE *qptr;                    /* Pointer to the queue                               */
{
   QE *qeptr = NULL;
   int i;
   
   if (qptr == NULL) YS__errmsg("Null queue pointer passed to QueuePrint\n");
   else {
      i = 0;
      sprintf(YS__prbpkt,"            Queue %s contents:\n",qptr->name);
      YS__SendPrbPkt(TEXTPROBE,qptr->name,YS__prbpkt);
      qeptr = qptr->head;
      while(qeptr != NULL) {
         sprintf(YS__prbpkt,"              Qelem %d is %s\n",i,qeptr->name);
         YS__SendPrbPkt(TEXTPROBE,qptr->name,YS__prbpkt);
         qeptr = qeptr->next;
         i++;
      }
      if (i == 0) {
         sprintf(YS__prbpkt,"              Queue Empty\n");
         YS__SendPrbPkt(TEXTPROBE,qptr->name,YS__prbpkt);
      }
   }
}

/*****************************************************************************/

int YS__QeId(qeptr)         /* Returns the system defined ID or 0 if TrID is 0        */
QE *qeptr;                  /* Pointer to the queue element                           */
{
   if (TraceIDs)
      return qeptr->id;
   else return 0;
}

/*****************************************************************************/
/* SEMAPHORE Operations: Implement the standard semaphore queue. Since       */
/* this code runs on a uniprocessor, the operations are not implemented in   */
/* an indivisible fashion.                                                   */
/*****************************************************************************/

SEMAPHORE *NewSemaphore(sname,i)  /* Creates & returns a pointer to a new semaphore   */
char *sname;                      /* User assigned name                               */
int i;                            /* Initial semaphore value                          */
{
   SEMAPHORE *semptr;

   PSDELAY;

   semptr = (SEMAPHORE*)YS__PoolGetObj(&YS__SemPool);
   semptr->id = YS__idctr++;
   strncpy(semptr->name,sname,31);
   semptr->name[31] = '\0';
   semptr->next = NULL;
   semptr->head = NULL;
   semptr->tail = NULL;
   semptr->type = SYNCQTYPE;
   semptr->size = 0;
   semptr->lengthstat = NULL;
   semptr->timestat = NULL;
   semptr->val = i;
   semptr->initval = i;
   TRACE_SEMAPHORE_new;        /* Creating semaphore with value                       */
   return semptr;
}

/**************************************************************************************/

void SemaphoreSignal(sptr) /* Signals the semaphore, and continues. If activities are */
                           /* waiting at the semaphore, the one at the head of the    */
                           /* queue is released and the semaphore value is unchanged  */
                           /* If none are waiting, the semaphore value is incremented */
                           /* The queue is FIFO.                                      */
SEMAPHORE *sptr;           /* Pointer to the semaphore                                */
{
   ACTIVITY *aptr;

   PSDELAY;

   if (sptr->size == 0)  {                      /* Queue is empty                     */
      sptr->val++;                              /* increment value and continue       */
      TRACE_SEMAPHORE_v1p;                      /* ... signalling semaphore ...       */
   }
   else  {                                      /* Queue not empty                    */
      TRACE_SEMAPHORE_v2p;                      /* ... signalling semaphore ...       */
      aptr = (ACTIVITY*)YS__QueueGetHead(sptr); /* Get head of queue                  */
      YS__RdyListAppend(aptr);                  /* and add it to the ready list       */
      TRACE_SEMAPHORE_v3;                       /* Actvity released ...               */
   }
}

/**************************************************************************************/

void SemaphoreSet(sptr)    /* Sets the semaphore's value to 1, and coninues.  If      */
                           /* activities are waiting at the semaphore, the one at the */
                           /* head of the queue is released and the semaphore value   */
                           /* is set to 0.  The queue is FIFO.                        */
SEMAPHORE *sptr;           /* Pointer to the semaphore                                */
{
   ACTIVITY *aptr;

   PSDELAY;

   if (sptr->val == 1) YS__warnmsg("Setting a set semaphore");
   if (sptr->size == 0)  {                       /* Queue is empty                    */
      sptr->val = 1;                             
      TRACE_SEMAPHORE_s1p;                       /* ... setting semaphore ...         */
   }
   else  {                                       /* Queue not empty                   */
      TRACE_SEMAPHORE_s2p;                       /* ... setting semaphore             */
      aptr = (ACTIVITY*)YS__QueueGetHead(sptr);  /* Get head of queue                 */
      YS__RdyListAppend(aptr);                   /* and add it to the ready list      */
      sptr->val = 0;                             
      TRACE_SEMAPHORE_v3;                        /* Activity released ...             */
   }
}

/**************************************************************************************/

int SemaphoreDecr(sptr)  /* Decrements the semaphore and returns its new value        */
SEMAPHORE *sptr;         /* Pointer to the semaphore                                  */
{

   PSDELAY;

   if (sptr->val > 0) sptr->val--;   /* Semaphore values can not go negative          */
   return sptr->val;
}

/**************************************************************************************/

void SemaphoreWait(sptr) /* Waits on a semaphore.  If the value is > 0 decrement it   */
                         /* and continues, else suspends                              */
SEMAPHORE *sptr;         /* Pointer to the semaphore                                  */
{
  YS__errmsg("SemaphoreWait not supported in this version!");
}

/**************************************************************************************/

int SemaphoreValue(sptr)  /* Returns the value of the semaphore                       */
SEMAPHORE *sptr;          /* Pointer to the semaphore                                 */
{
   PSDELAY;

   return sptr->val;
}

/**************************************************************************************/

int SemaphoreWaiting(sptr)  /* Returns the # of activities in the queue               */
SEMAPHORE *sptr;            /* Pointer to the semaphore                               */
{
   PSDELAY;

   return sptr->size;
}


/**************************************************************************************/


