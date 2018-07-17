/*
  evlst.c

  This file contains functions associated with maintaining
  the simulator event list
  
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
#include "MemSys/tr.evlst.h"
#include "MemSys/dbsim.h"

/*****************************************************************************/
/* EVENT LIST Operations: There are two implementations of the eventlist:    */
/* a calendar queue and a simple linear list.  The choice of which list      */
/* to use is determined by a command line argument or by the operation       */
/* EventListSelect().  The implementaton of the calendar queue algorithm     */
/* follows the description in "Calendar Queues: A Fast O(1) Priority         */
/* Queue Implementation for the Simulation Event Set Problem," by Randy      */
/* Brown, Comm. ACM, Oct. 1988, pp. 1220-1227.                               */
/*****************************************************************************/

static int EventListType = CALQUE; /* Calendar queue = 0; Linear queue = 1 */
static STATREC *lengthstat = NULL; /* For collecting queue length statistics */

/*  Static variables used when the event List is implemented as a simple linear list  */

static ACTIVITY *head;                /* Pointer to the first element of the queue */
static ACTIVITY *tail;                /* Pointer to the last element of the queue */
static int      size;                 /* Number of elements in the queue     */

/*  Static variables used when the event List is implemented as a Calendar Queue */

static ACTIVITY *headspace[3*CALQSZ]; /* Head array: elements point to bin list heads */
static ACTIVITY *tailspace[3*CALQSZ]; /* Tail array: elements point to bin list tails */
static ACTIVITY **binhead;            /* Pointer to head array in headspace  */
static ACTIVITY **bintail;            /* Pointer to tail array in tailspace  */
static double   binwidth = 1.0;       /* Range of values in a bin            */
static double   rbinwidth = 0.5;      /* 1/binwidth                          */
static int      nbins = 2;            /* Number of bins used in a calendar queue      */
static int      cqsize;               /* Total number of elements in the queue */
static int      thtop;                /* High resize threshold               */
static int      thbot;                /* Low resize threshold                */
static int      lastbin;              /* Index of last bin dequeued from     */
static double   bintop;               /* Priority at top of lastbin          */
static double   lastprio;             /* Priority of last element dequeued   */
static int      toparray;             /* TRUE if queue now at the top of queue space  */
static int      resizeon;             /* TRUE if resizing is enabled         */
static int      autosize = 1;         /* TRUE if autosizing is enabled       */
static int      emptybins;            /* Number of empty bins                */
static STATREC  *binsstat = NULL;     /* For collecting number of bins statistics     */
static STATREC  *widthstat = NULL;    /* For collecting bin width statistics */
static STATREC  *emptystat = NULL;    /* For collecting numbr of empty bins stats     */

/* Function declarations for calendar queue operations */

void     YS__CalQueInit();
double   YS__CalQueNewWidth();
void     YS__CalQueResize();
void     YS__CalQueFindHead();

/*****************************************************************************/

void YS__CalQueInit(qbase, bins, bwidth, startprio) 
/* Initialize a bin array within headspace                                   */
int    qbase;                /* Start of bin array in headspace              */
int    bins;                 /* Number of bins                               */
double bwidth;               /* Width of bins                                */
double startprio;            /* Priority (time) at which dequeueing begins   */
{
   int i;
   long int n;
   
   toparray = qbase;              /* TRUE if at top of array, FALSE if at bottom      */
   binhead = &(headspace[qbase]); /* binhead points to first bin head pointer*/
   bintail = &(tailspace[qbase]); /* bintail points to first bin tail pointer*/
   nbins = bins;                  /* Set the initial number of bins          */
   emptybins = nbins;             /* All bins empty at start                 */
   binwidth = bwidth;             /* Set the initial bin width               */
   rbinwidth = 1/bwidth;          /* Compute the inverse of the bin width    */
   cqsize = 0;                    /* Queue empty initially                   */
   for (i=0; i<bins; i++) {       /* Clear the bin head and tail arrays      */
      binhead[i] = NULL;
      bintail[i] = NULL;
   }
   lastprio = startprio;          
   n = (int)(startprio/bwidth); 
   lastbin = n%bins;
   bintop = (n+1)*bwidth + 0.5*bwidth;
   thbot = bins/2 - 1;
   thtop = 2*bins;
}

/*****************************************************************************/

double YS__CalQueNewWidth()   /* Calculates a new bin width                  */
{
   int i,n,nsamples;
   double sum,ave,x;
   int savelastbin;
   double savebintop, savelastprio;
   ACTIVITY *samples[25];

   /* Compute the number of samples to use in computing the new bin width */

   if (cqsize < 2) return 1.0;            /* New bin width is 1              */
   if (cqsize <= 5) nsamples = cqsize;    /* Use all the bin elements as samples */
   else nsamples = 5 + (int)(cqsize/10);  /* Use 1/10th of the total + 5 samples */
   if (nsamples > 25) nsamples = 25;      /* But don't use more than 25 samples*/

   /* Copy samples into an array */

   savelastbin = lastbin;
   savelastprio = lastprio;
   savebintop = bintop;
   resizeon = 0;                           /* Turn off resizing during the copy */
   for (i = 0; i < nsamples; i++) {        /* Take the samples out of the list */
      samples[i] = YS__EventListGetHead(); /* And save them in an array      */
   }
   for (i = nsamples-1; i >= 0; i--) {     /* Now put them back into the list*/
      YS__EventListInsert(samples[i]);
   }
   lastbin = savelastbin;
   lastprio = savelastprio;
   bintop = savebintop;
   resizeon = 1;

   /* Compute new bin width */

   sum = 0.0;
   for (i = 0; i < nsamples-1; i++) {
      x = samples[i+1]->time - samples[i]->time; /* X is diff between adj samples */ 
      if (x < 0.0) x = -x;                       /* Use the magnitude        */
      sum = sum + x;                             /* Accumulate the differences */
   }
   ave = sum/(double)(nsamples - 1);
   sum = 0.0;
   n = 0;
   for (i = 0; i < nsamples-1; i++) {            /* Recalculate the average  */
      x = samples[i+1]->time - samples[i]->time;
      if (x < 0.0) x = -x;
      if (x <= 2*ave) {                          /* Only use difference <= 2 x ave    */
         sum = sum + x;
         n++;                                    /* N is the number of samples used   */
      }
   }
   ave = sum/(double)n;
   return 3.0*ave;                               /* Use 3 x the average found above   */
}

/*****************************************************************************/

void YS__CalQueResize(newsize)   /* Resizes the queue by creating a new one at the    */
                                 /* opposite end of the allocated queue space and     */
                                 /* then copying the current queue to the newly       */
                                 /* created one                              */
int newsize;                     /* Number of bins in the new queue          */
{
   int      i;
   int      oldnbins;
   ACTIVITY **oldbinhead;
   ACTIVITY *aptr;
   ACTIVITY *bptr;
   double   x;

   if (resizeon == 0) return;          /* Resizing inhibited                 */

   if (newsize > 2*CALQSZ) return;     /* All space allocated for the queue is used   */

   x = YS__CalQueNewWidth();           /* Calculate the new bin width to use */
   if (x <= 0.0) x = 1.0;              /* Smallest bin width is 1.0          */
   binwidth = x; 
   oldbinhead = binhead;               /* Current queue location used to copy from    */
   oldnbins = nbins;                   /* Current number of bins in the queue*/

   if (toparray) {                     /* Currently at the top of the queue space     */
      YS__CalQueInit(0,newsize,binwidth,lastprio);
      toparray = 0;                    /* Now at the bottom of queue space   */
   }
   else {                              /* Currently at the bottom of the queue space  */
      YS__CalQueInit(3*CALQSZ-newsize,newsize,
         binwidth,lastprio);
      toparray = 1;                    /* Now at the top of queue space      */
   }

   for (i = oldnbins - 1; i >= 0; i--) { /* Copy the queue one bin at a time */
      aptr = oldbinhead[i];              /* Start with the old head ptr for bin i     */
      while (aptr != NULL) {             /* Copy the bin contents            */
         bptr = aptr;                    /* Get the next element in the bin  */
         aptr = (ACTIVITY*)(aptr->next); /* Remember the next element in the bin      */
         YS__EventListInsert(bptr);      /* Put the element in the new queue */
      }
   }
}

/*****************************************************************************/

void YS__CalQueFindHead()   /* Locates the new queue head after a dequeue or delete   */
{
   int      i;
   double   minprio;
   int      minindex; 
   ACTIVITY *aptr;
   int      ex = 1;

   /* Try the current bin */

   i = lastbin;
   aptr = binhead[i];
   while (ex) {
      if (aptr != NULL && aptr->time < bintop) { /* Head is in bin i         */
         lastprio = aptr->time;                  /* Remember time of head element     */
         lastbin = i;                            /* Remember bin of head element      */
         return;
      }
      else {                                     /* Try the next bin          */
         i++;
         if (i == nbins) i = 0;                  /* Wrap around               */
         bintop = bintop + binwidth;
         aptr = binhead[i];
         if (i == lastbin) ex = 0;               /* Back at starting bin, break        */
      }
   }

   /* Next find the bin containing the element with the smallest time */

   minprio = -1.0;
   minindex = 0;
   for (i = 0; i < nbins; i++) {
      aptr = binhead[i];
      if (aptr != NULL)
         if (minprio < 0.0) {                /* First non-empty bin          */
            minprio = aptr->time;
            minindex = i;
         }
         else if ( aptr->time < minprio) {   /* Found new minimum            */
            minprio = aptr->time;
            minindex = i;
         }
   }

   /* Now set up the queue parameters for the new head and return */

   aptr = *(binhead+minindex);
   lastbin = minindex;
   lastprio = minprio;
   i = (int)(minprio*rbinwidth)+1;
   bintop = (double)(i*binwidth + 0.5*binwidth);
   return;
}

/*****************************************************************************/

void YS__EventListSetBins(i)       /* Sets the number of bins to a fixed size*/
int i;                             /* The new bin size                       */
{
   if (i <= 1) EventListType = LINQUE;
   else {
      nbins = i;
      autosize = 0;
   }
}

/*****************************************************************************/

void YS__EventListSetWidth(x)      /* Sets the bin width to a fixed size     */
double x ;                         /* The new bin width                      */
{
   binwidth = x;
   autosize = 0;
}

/*****************************************************************************/

void YS__EventListInit()           /* Initializes the event list             */
{
   if (EventListType == CALQUE)

   /* Using calendar queue implementation for the event list */
   {
      TRACE_EVLST_init1;
      YS__CalQueInit(0,nbins,binwidth,0.0);
      resizeon = 1;
   }

   else  /* Using simple linear search implementation for the event list */
   {
      TRACE_EVLST_init2;
      head = NULL;
      tail = NULL;
      size = 0;
   }
}

/*****************************************************************************/

void YS__EventListInsert(aptr)   /* Inserts an element into the event list   */
ACTIVITY *aptr;                  /* Pointer to element to be enqueued        */
{
   int i;
   ACTIVITY *tp1;
   ACTIVITY *tp2;

   TRACE_EVLST_insert;

   if (EventListType == CALQUE) /* Using calendar queue for the event list            */  
   {
      i = (int)(aptr->time*rbinwidth);             /* Find the activity's bin*/
      i = i % nbins;

      if (cqsize == 0 || aptr->time < lastprio) {  /* Activity becomes new head       */
         lastprio = aptr->time;
         lastbin = i;
         bintop = (i+1)*binwidth+0.5*binwidth;
      }
      cqsize++;                                 /* Increment the queue size  */

      tp1 = binhead[i];
      if (tp1 == NULL) emptybins--;             /* The bin was empty         */

      if (emptystat)                            /* Empty bin statistics collected     */
         StatrecUpdate(emptystat,(double)emptybins,1.0);
      if (lengthstat)                           /* Queue length statistics collected  */
         StatrecUpdate(lengthstat,(double)cqsize,1.0);
      if (binsstat)                             /* Bin count statistics collected     */
         StatrecUpdate(binsstat,(double)nbins,1.0);
      if (widthstat)                            /* Bin width statistics collected     */
         StatrecUpdate(widthstat,binwidth,1.0);

      if (tp1 == NULL || aptr->time < tp1->time 
         || (resizeon == 0 && aptr->time == tp1->time)) { /* Put activity at bin head */
            aptr->next = (QE*)tp1;
            binhead[i] = aptr;
            if (bintail[i] == NULL) bintail[i] = aptr;
      }
      else {
         if (aptr->time >= bintail[i]->time) {     /* put activity at tail of bin     */
            bintail[i]->next = (QE*)aptr;
            aptr->next = NULL;
            bintail[i] = aptr;
         }
         else {                                    /* put element in middle of bin    */
            while (tp1 != NULL) {
               tp2 = (ACTIVITY*)(tp1->next);
               if (tp2 == NULL || aptr->time < tp2->time 
                   || (resizeon == 0 && aptr->time == tp2->time)) {
                  aptr->next = (QE*)tp2;
                  tp1->next = (QE*)aptr;
                  tp1 = NULL;
               }
               else {
                  tp1 = tp2;
                  tp2 = (ACTIVITY*)(tp2->next);
               }
            }
         }
      }
      if (autosize && cqsize > thtop) {            /* Increase the number of bins     */
         YS__CalQueResize(2*nbins);
      }
   }

   else  /* Using simple linear search implementation for the event list */
   {
      if (head == NULL)  {          /* The queue is empty                    */
         head = aptr;
         tail = aptr;
         aptr->next = NULL;
         size++;
      }
      else  {
         if (aptr->time < ((head))->time) {  /* Put the new element at head of queue  */
            aptr->next = (QE*)head;
            head = aptr;
            size++;
         }
         else {
            if ( aptr->time >= ((tail))->time)  { /* Put new element at tail of queue */
               tail->next = (QE*)aptr;
               aptr->next = NULL;
               tail = aptr;
               size++;
            }
            else  {                               /* put element in middle of queue   */
               tp1 = head;
               while (aptr->time >= ((ACTIVITY*)(tp1->next))->time) /* Find position  */
                  tp1 = (ACTIVITY*)(tp1->next);
               aptr->next = tp1->next;
               tp1->next = (QE*)aptr;
               size++;
            }
         }
      }
      if (lengthstat)                 /* Queue length statistics collected   */
         StatrecUpdate(lengthstat,(double)size,1.0);
   }
   TRACE_EVLST_show;
}

/*****************************************************************************/

ACTIVITY *YS__EventListGetHead()    /* Returns the head of the event list    */
{
   ACTIVITY *retptr;

   if (EventListType == CALQUE)  /* Using calendar queue for the event list  */
   {
      if (cqsize == 0) {                              /* Queue is empty      */
         TRACE_EVLST_gethead1;
         return NULL;
      }
      retptr = binhead[lastbin];                      /* Return its head element      */
      binhead[lastbin] = (ACTIVITY*)(retptr->next);
      retptr->next = NULL;
      if (binhead[lastbin] == NULL) {           /* The bin is now empty      */
         bintail[lastbin] = NULL;
         emptybins++;
      }
      cqsize--;

      if (emptystat)                            /* Empty bin statistics collected     */
         StatrecUpdate(emptystat,(double)emptybins,1.0);
      if (lengthstat)                           /* Queue length statistics collected  */
         StatrecUpdate(lengthstat,(double)cqsize,1.0);
      if (binsstat)                             /* Bin count statistics collected     */
         StatrecUpdate(binsstat,(double)nbins,1.0);
      if (widthstat)                            /* Bin width statistics collected     */
         StatrecUpdate(widthstat,binwidth,1.0);

      TRACE_EVLST_gethead2;
      if (cqsize <= 0) return retptr;        

      YS__CalQueFindHead();              /* Position a pointer to the new queue head  */

      if (autosize && cqsize < thbot) {  
         YS__CalQueResize(nbins/2);                   /* Halve the number of bins     */
      }
      TRACE_EVLST_show;
      return retptr;
   }

   else  /* Using simple linear search implementation for the event list */
   {
      if (head != NULL)  {         /* Queue not empty              */
         retptr = head;            /* Get the head element         */
         head = 
            (ACTIVITY*)(retptr->next);       /* Head points to the new head element   */
         retptr->next = NULL;                /* Clear next pointer of removed element */
         if (head == NULL) 
            tail = NULL;           /* Queue now empty              */
         size--;                   /* One fewer element in the queue        */
         if (lengthstat)           /* Queue length statistics collectd      */
            StatrecUpdate(lengthstat,(double)size,1.0);
         TRACE_EVLST_gethead2;
      }
      else  {                                /* Queue empty                  */
         retptr = NULL;                      /* Return NULL                  */
         TRACE_EVLST_gethead1;
      }
      TRACE_EVLST_show;
      return retptr;
   }
}

/*****************************************************************************/

double YS__EventListHeadval()       /* Returns the time value of the event list head  */
{
   double retval; 

   if (EventListType == CALQUE)   /* Using calendar queue for the event list */
   {
      if (cqsize != 0) {
         retval = lastprio;
      }
      else {
         retval = -1.0;
      }
      TRACE_EVLST_headval;
      return retval;
   }

   else  /* Using simple linear search implementation for the event list */
   {
      if (head != NULL) {
         retval = (head)->time;     
      }
      else {
         retval =  -1.0;
      }
      TRACE_EVLST_headval;
      return retval;
   }

}

/*****************************************************************************/

int YS__EventListDelete(aptr)     /* Removes an element from the event list  */
ACTIVITY *aptr;                   /* Pointer to the element to remove        */
{ /* Returns 1 if the element is in the queue and 0 if not                   */

   int i;
   ACTIVITY *actptr;
   ACTIVITY *eptr;

   if (aptr == NULL) YS__errmsg("Null queue element pointer passed to QueueDelete");

   if (EventListType == CALQUE)  /* Using calendar queue for the event list  */
   {
      TRACE_EVLST_delete;                 /* Deleting element from the Event List     */
      i = (int)(aptr->time*rbinwidth);    /* Find the element's bin          */
      i = i % nbins;
   
      if (binhead[i] == aptr) {           /* Element at head of bin; take it out      */
         binhead[i] = (ACTIVITY*)(aptr->next);
         aptr->next = NULL;
         if (bintail[i] == aptr) bintail[i] = binhead[i];
      }
      else {                              /* Loacate the element in the bin */
         for (actptr = binhead[i]; 
                actptr != NULL && actptr->next != (QE*)aptr;
                actptr = (ACTIVITY*)(actptr->next));
         if (actptr == NULL) {            /* The elelment was not in the bin*/
            return 0;
         }          
         else {                           /* Found the element; take it out */
            actptr->next = aptr->next;
            aptr->next = NULL;
            if (bintail[i] == aptr) bintail[i] = actptr;
         }
      }
      cqsize--;
      if (binhead[i] == NULL) emptybins++;      /* The bin is now empty     */

      if (emptystat)                            /* Empty bin statistics collected     */
         StatrecUpdate(emptystat,(double)emptybins,1.0);
      if (lengthstat)                           /* Queue length statistics collected  */
         StatrecUpdate(lengthstat,(double)cqsize,1.0);
      if (binsstat)                             /* Bin count statistics collected     */
         StatrecUpdate(binsstat,(double)nbins,1.0);
      if (widthstat)                            /* Bin width statistics collected     */
         StatrecUpdate(widthstat,binwidth,1.0);

      YS__CalQueFindHead();              /* Positions a pointer to the new queue head */

      if (autosize && cqsize < thbot) {  
         YS__CalQueResize(nbins/2);      /* Halve the number of bins         */
      }
      TRACE_EVLST_show;
      return 1;
   }

   else  /* Using simple linear search implementation for the event list */
   {

      TRACE_EVLST_delete;                 /*     Deleting element from the Event List */
      actptr = head;
      for (eptr = head; eptr != NULL; eptr = (ACTIVITY*)(eptr->next))  {
         if (eptr == aptr)  {             /* Found the element in the queue  */
            if (eptr == (head)) 
               head = (ACTIVITY*)(eptr->next);
            else actptr->next = eptr->next;
            if (eptr == (tail)) tail = actptr;
            eptr->next = NULL;
            size--;
            if (lengthstat)            /* Queue length statistics collected  */
               StatrecUpdate(lengthstat,(double)size,1.0);
            TRACE_EVLST_show;
            return 1;
         }
         actptr = eptr;                    /* Element not found yet          */
      }                                    /* Fall out here if element not found      */
      TRACE_EVLST_show;
      return 0;
   }
}

/*****************************************************************************/

void YS__EventListPrint()    /* Lists the contents of the event list         */
{
   int i;
   ACTIVITY *tp;

   if (EventListType == CALQUE)   /* Using calendar queue for the event list          */ 
   {
      sprintf(YS__prbpkt,"\n            EVENT LIST: Size = %d, Bucket Size = %g\n",
              cqsize,binwidth);
      YS__SendPrbPkt(TEXTPROBE,"EventList",YS__prbpkt);
      for (i = 0; i<nbins; i++) {
         tp = binhead[i];
         sprintf(YS__prbpkt,"                Bucket %d:\n",i);
         YS__SendPrbPkt(TEXTPROBE,"EventList",YS__prbpkt);
         while (tp != NULL) {
            sprintf(YS__prbpkt,"                    Time of activity %s[%d] is %g\n",
                    tp->name, YS__QeId(tp),tp->time);
            YS__SendPrbPkt(TEXTPROBE,"EventList",YS__prbpkt);
            tp = (ACTIVITY*)(tp->next);
         }
      }
      sprintf(YS__prbpkt,"\n");
      YS__SendPrbPkt(TEXTPROBE,"EventList",YS__prbpkt);
   }
   
   else  /* Using simple linear search implementation for the event list */
   {
      i = 0;
      sprintf(YS__prbpkt,"\n            Event List contents:\n");
      YS__SendPrbPkt(TEXTPROBE,"EventList",YS__prbpkt);
      tp = head;
      while(tp != NULL) {
         sprintf(YS__prbpkt,"              Qelem %d is %s, time is %g\n",
            i,tp->name,tp->time);
         YS__SendPrbPkt(TEXTPROBE,"EventList",YS__prbpkt);
         tp = (ACTIVITY*)(tp->next);
         i++;
      }
      if (i == 0) {
         sprintf(YS__prbpkt,"              Queue Empty\n");
         YS__SendPrbPkt(TEXTPROBE,"EventList",YS__prbpkt);
      }
      sprintf(YS__prbpkt,"\n");
      YS__SendPrbPkt(TEXTPROBE,"EventList",YS__prbpkt);
   }
}
 
/*****************************************************************************/
/* Operations available to the YACSIM user                                   */
/*****************************************************************************/


int EventListSize()           /* Returns the number of elements in the eventlist      */
{
   if (EventListType == CALQUE)    /* Calendar queue                         */
      return cqsize;
   else                       /* Linear list                                 */
      return size;
}

void EventListSelect(type,bins,bwidth)  /* Selects the type of event list to use      */
int type;                               /* CALQUE or LINQUE                  */
int bins;                               /* Fix the number of bins            */
double bwidth;                          /* Fix the width of the bins         */
{
   /* Note that this routine must be called before anything else uses
    the event list. Moreover, there is no check of this of warning to
    the user if it happens */

   EventListType = type;
   if (type == CALQUE) {
      if (bins >= 2) YS__EventListSetBins(bins);        /* Otherwise bins and bin     */
      if (bwidth > 0.0) YS__EventListSetWidth(bwidth);  /* width automatic   */
      YS__EventListInit();
   }
}
   
void EventListCollectStats(type,meanflg,histflg,nbin,low,high)
                     /* Activates automatic statistics collection for the event list  */
int type;            /* LENGTH, BINS, EMPTYBINS, or BINWIDTH                 */
int meanflg;         /* MEANS or NOMEANS                                     */
int histflg;         /* HIST, NOHIST                                         */
int nbin;            /* Number of bins                                       */
double low;          /* Max value for low bin                                */
double high;         /* Min value for high bin                               */
{
   if (type == LENGTH) {
      if (lengthstat == NULL)  {              /* Stat collection not yet initiated    */
         lengthstat =                         /* Get a new statrec & and update it    */
            NewStatrec("EventList.length",POINT,meanflg,histflg,nbin,low,high);
      }
      else YS__warnmsg("Queue length statistics collection already set");
   }

   else if (type == BINS) {
      if (binsstat == NULL)  {                /* Stat collection not yet initiated    */
         binsstat =                           /* Get a new statrec & and update it    */
            NewStatrec("EventList.bins",POINT,meanflg,histflg,nbin,low,high);
      }
   }

   else if (type == BINWIDTH) {
      if (widthstat == NULL)  {                /* Stat collection not yet initiated    */
         widthstat =                           /* Get a new statrec & and update it    */
            NewStatrec("EventList.binwidth",POINT,meanflg,histflg,nbin,low,high);
      }
   }

   else if (type == EMPTYBINS) {
      if (emptystat == NULL)  {                /* Stat collection not yet initiated    */
         emptystat =                           /* Get a new statrec & and update it    */
            NewStatrec("EventList.emptybins",POINT,meanflg,histflg,nbin,low,high);
      }
   }

   else YS__warnmsg("Invalid event list statistics type; statistics not collected");
}

void EventListResetStats()       /* Resest a statistics record of a queue    */
{
   if (lengthstat != NULL) StatrecReset(lengthstat);
   if (binsstat != NULL)   StatrecReset(binsstat);
   if (widthstat != NULL)  StatrecReset(widthstat);
   if (emptystat != NULL)  StatrecReset(emptystat);
}

STATREC *EventListStatPtr(type)      /* Returns a pointer to a event list's statrec   */
int type;                            /* LENGTH, TIME, UTIL                   */
{
   if (type == LENGTH)    return lengthstat;
   if (type == BINS)      return binsstat;
   if (type == BINWIDTH)  return widthstat;
   if (type == EMPTYBINS) return emptystat;
   YS__errmsg("Invalid statistics type passed to EventListStatPtr()");
   return NULL;
}
