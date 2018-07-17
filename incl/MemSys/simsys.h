/*
  simsys.h

  Declarations for a variety of memory-system simulation features, such
  as event-driven simulation library and some parts of network simulator.
  
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


#ifndef SIMH
#define SIMH

#include <stdio.h>

#include "typedefs.h"


/*****************************************************************************/
/* Declaration of CONSTANTS                                                  */
/*****************************************************************************/

/*****************************************************************************/
/* Internal constants (not used by user)                                     */
/*****************************************************************************/

#ifdef DEBUG_TRACE
#define debug           /* Causes trace statments to be included in the source code   */
#endif

#define MAXDBLEVEL 5    /* The number of trace levels                        */

/*****************************************************************************/
/* Object types                                                              */
/*****************************************************************************/

#define UNTYPED                  0

/* YACSIM Objects */

#define QETYPE                1000      /* Queue elements                    */
#define   QUETYPE             1100      /* Queues                            */
#define     SYNCQTYPE         1110      /* Synchronization Queueus           */
#define       SEMTYPE         1111      /* Semaphores                        */
#define       FLGTYPE         1112      /* Flages                            */
#define       BARTYPE         1113      /* Barriers                          */
#define     RESTYPE           1120      /* Resources                         */
#define   STATQETYPE          1200      /* Statistical Queue Elements        */
#define     ACTTYPE           1210      /* Activities                        */
#define       PROCTYPE        1211      /* Processes                         */
#define       EVTYPE          1212      /* Events                            */
#define     MSGTYPE           1220      /* Messages                          */
#define   STVARTYPE           1300      /* State Variables                   */
#define     IVARTYPE          1310      /* Integer Valued State Variable     */
#define     FVARTYPE          1320      /* Real Valued State Variable        */
#define   STRECTYPE           1400      /* Statistics Records                */
#define     PNTSTATTYPE       1410      /* Point Statistics Records          */
#define     INTSTATTYPE       1420      /* Interval STatistics Records       */

/* NETSIM Objects */

#define BUFFERTYPE            2001      /* Netsim Buffers                    */
#define MUXTYPE               2002      /* Netsim Multiplexers               */
#define DEMUXTYPE             2003      /* Netsim Demultiplexers             */
#define IPORTTYPE             2004      /* Netsim Input Ports                */
#define OPORTTYPE             2005      /* Netsim Output Ports               */
#define DUPLEXMODTYPE         2006      /* Netsim Duplex Module              */
#define HEADTYPE              2007      /* Head Flits                        */
#define TAILTYPE              2008      /* Tail Flits                        */

/* PARCSIM Objects */

#define PROCRTYPE             3001      /* Processors                        */
#define USRPRTYPE             3002      /* User Processes                    */
#define OSPRTYPE              3003      /* OS Processes                      */
#define OSEVTYPE              3004      /* OS Events                         */

/*****************************************************************************/
/* Activity States                                                           */
/*****************************************************************************/


#define LIMBO                    0      /* Just created                      */
#define READY                    1      /* In the ready list                 */
#define DELAYED                  2      /* In the event list                 */
#define WAIT_SEM                 3      /* In a semaphore's queue            */
#define WAIT_FLG                 4      /* In a flag's queue                 */
#define WAIT_CON                 5      /* In a conditions                   */
#define WAIT_RES                 6      /* In a resorce's queue              */
#define USING_RES                7      /* In the event getting resource
					   service      */

/*****************************************************************************/
/* Event, resource, process, and processor States                            */
/*****************************************************************************/

#define RUNNING                  8      /* Executing the body function       */

#define BLOCKED                  9      /* Due to a blocking schedule        */
#define WAIT_JOIN               10      /* Due to a ProcessJoin              */
#define WAIT_MSG                11      /* Due to a blocking ReceiveMsg      */
#define WAIT_BAR                12      /* Due to a BarrierSync()            */

#define IDLE                     0      /* Processor Ready List empty        */
#define BUSY                     1      /* Processor Ready List not empty    */

/*****************************************************************************/
/* Delay constants                                                           */
/*****************************************************************************/

#define NUMDELAYS                6     
#define MSG_ROUTING_DELAY        0
#define MSG_BUFOUT_DELAY         1
#define MSG_PACKETIZE_DELAY      2
#define MSG_PASSTHRU_DELAY       3
#define MSG_BUFIN_DELAY          4
#define MSG_DELIVER_DELAY        5

#define NET_FLITMOVE_DELAY       0
#define NET_MUXARB_DELAY         1
#define NET_MUXMOVE_DELAY        2
#define NET_DEMUX_DELAY          3

/*****************************************************************************/
/* NETSIM statistics constants                                               */
/*****************************************************************************/

#define NETTIME                  1
#define BLKTIME                  2
#define OPORTTIME                3
#define MOVETIME                 4
#define LIFETIME                 5

/*****************************************************************************/
/* Miscellaneous constants                                                   */
/*****************************************************************************/

#define POOLBLKSZ               10
#define DEFAULTHIST             64
#define CALQSZ                1024      /* Should be a power of 2            */

#define MAXFANIN                11
#define MAXFANOUT               11
#define MSGACK                  -2

#define PROCESSORDEST            2
#define RDYLIST                  0      /* RRPRWP resource used as a cpu's ready list */

#ifndef FALSE
#define FALSE                    0
#endif
#ifndef TRUE
#define TRUE                     1
#endif

/*****************************************************************************/
/* User-visible (external) constants                                         */
/*****************************************************************************/


/* Statistics Record Characteristics */

#define NOMEANS                  0      /* Means not computed                */
#define MEANS                    1      /* Means are computed                */
#define HIST                     2      /* Histogram is collected            */
#define NOHIST                   3      /* Histogram not collected           */
#define POINT                    4      /* Point statistics record type      */
#define INTERVAL                 5      /* Interval statistics record type   */
#define HISTSPECIAL              6      /* Collect histograms, but only show
					   non-zero entries when reporting   */

/* Blocking Actions */

#define INDEPENDENT              0      /* Unrelated child scheduled         */
#define NOBLOCK                  0      /* Nonblocking MessageReceive        */
#define BLOCK                    1      /* Blocking Schedule or Message Receive*/
#define FORK                     2      /* Forking Schedule                  */

/* Event Characteristics */

#define DELETE                   1      /* Event deleted at termination      */
#define NODELETE                 0      /* Event not deleted at termination  */

/* Message Parameters */

#define ANYTYPE                 -1      /* Receiving process ignores message type     */
#define ANYSENDER                0      /* Recieving process ignores message sender   */

#define ANYSENDERPROCESSOR      -1      /* Receiving processor ignores mesage sender  */
#define PROCESSDEST              1      /* Message sent to a process         */
#define PROCESSORDEST            2      /* Message sent to a processor       */
#define BLOCK_UNTIL_RECEIVED     1
#define BLOCK_UNTIL_SENT         2      
#define BLOCK_UNTIL_PACKETIZED   3
#define BLOCK_UNTIL_BUFFERED     4
#define DEFAULT_PKT_SZ          64

/* Argument and Buffer Size */

#define UNKNOWN                 -1      /* Size not specified                */
#define DEFAULTSTK               0      /* Default stack size                */

/* Types of Queue Disciplines */

#define FCFS                     1      /* First Come First Served           */
#define FCFSPRWP                 2      /* FCFS Preemptive Resume with Priorities     */
#define LCFSPR                   3      /* Last Come First Served Preemptive Resume   */
#define PROCSHAR                 4      /* Processor Sharing                 */
#define RR                       5      /* Round Robin                       */
#define RAND                     6      /* Random                            */
#define LCFSPRWP                 7      /* LCFS Preemptive Resume with Priorities     */
#define SJN                      8      /* Shortest Job Next                 */
#define RRPRWP                   9      /* RR Preemptive Resume with Priorities       */
#define LCFS                    10      /* Last Come First Served            */
#define OURTYPE                 11      /* Our ready list with active messages        */

/* Queue Statistics **/

#define TIME                     1
#define UTIL                     2
#define LENGTH                   3
#define BINS                     4
#define BINWIDTH                 5
#define EMPTYBINS                6

/* Event List Types */

#define CALQUE                   0
#define LINQUE                   1

/* Netsim Routing modes */

#define NOWAIT                   0
#define WAIT                     1

/* Misc. */

#define ME                       0
#define GLOBAL                   0
#define COMBINED                 0
#define INDIVIDUAL               1
#define ALL                      0
#define BOTH                     2


/*****************************************************************************/
/* MACROS                                                                    */
/*****************************************************************************/

#define PSDELAY

/*****************************************************************************/
/* Global variables                                                          */
/*****************************************************************************/

extern QUEUE    *YS__PendRes;         /* Queue of Resources to be evaluated  */
extern EVENT    *YS__ActEvnt;         /* Pointer to the currently occurring event     */
extern double   YS__Simtime;          /* The current simulation time         */
extern int      YS__idctr;            /* Used to generate unique ids for objects      */
extern char     YS__prbpkt[];         /* Buffer for probe packets            */
extern int      YS__msgcounter;       /* System defined unique message ID    */
extern int      YS__interactive;      /* Flag; set if running under viewsim or dbsim  */

extern POOL     YS__StkPool;          /* Pool of stacks of the default stack size     */
extern POOL     YS__MsgPool;          /* Pool of MESSAGE descriptors         */
extern POOL     YS__EventPool;        /* Pool of EVENT descriptors           */
extern POOL     YS__QueuePool;        /* Pool of QUEUE descriptor            */
extern POOL     YS__SemPool;          /* Pool of SEMAPHORE descriptors       */
extern POOL     YS__QelemPool;        /* Pool of QELEM descriptors           */
extern POOL     YS__StatPool;         /* Pool of STATREC descriptors         */
extern POOL     YS__HistPool;         /* Pool of histograms of default size  */

extern QUEUE    *YS__ActPrcr;         /* List of active processors           */
extern POOL     YS__PrcrPool;         /* Pool of PROCESSOR descriptors       */
extern POOL     YS__PktPool;          /* Pool of short PACKET descriptors    */
extern POOL     YS__ModulePool;       /* Pool of SMMODULE descriptors        */
extern POOL     YS__CachePool;        /* Pool of CACHE descriptors           */
extern POOL     YS__BusPool;          /* Pool of BUS descriptors             */
extern POOL     YS__ReqPool;          /* Pool of REQ descriptors             */
extern POOL     YS__SMPortPool;       /* Pool of SMPORT descriptors          */
extern POOL     YS__ReqStPool;        /* Pool of REQST descriptors */
extern POOL     YS__DirstPool;       
extern POOL     YS__DirEPPool;       
extern int      YS__Cycles;           /* Count of profiling cycles accumulated        */
extern double   YS__CycleTime;        /* Cycle time                          */

extern STATREC  *YS__BusyPrcrStat;    /* Statrec for processor utilization   */
extern int      YS__TotalPrcrs;       /* Number of total processors          */

extern int      TraceIDs;             /* If != 0, show object ids in trace output     */
extern int      TraceLevel;           /* Controls the amount of trace information     */

/*****************************************************************************/
/* Object declarations                                                       */
/*****************************************************************************/

/*****************************************************************************/
/* Queue element declaration                                                 */
/*****************************************************************************/

 struct YS__Qelem {       /* Used only with state variables and pools  */
   char   *pnxt;                /* Next pointer for Pools                    */
   char   *pfnxt;                /* Next pointer for Pools                    */
   QELEM  *next;                /* Pointer to the next Qelem in the list     */
   char   *optr;                /* Pointer to an object                      */
}; 

/*****************************************************************************/
/* Pool declarations -- for faster memory allocation                         */
/*****************************************************************************/

 struct YS__Pool {        /* Used to minimize the use of malloc        */
   char name[32];               /* User defined name                         */
   char *p_head;                  /* Pointer to the first element of the queue */
   char *p_tail;                  /* Pointer to the last element of the queue  */
   char *pf_head;                  /* Pointer to the first element of the queue */
   char *pf_tail;                  /* Pointer to the last element of the queue  */
   int  objects;                /* Number of objects to malloc               */
   int  objsize;                /* Size of objects in bytes                  */
   int newed;
   int killed;
};

void YS__PoolInit(POOL *pptr, char *name, int objs, int objsz);  /* Initialize a pool  */
void YS__PoolStats(POOL *);
char *YS__PoolGetObj(POOL *pptr);     /* Get an object from a pool */
void YS__PoolReturnObj(POOL *pptr, void *optr);       /* Return an object to its pool              */
void YS__PoolReset(POOL *pptr);        /* Deallocate all objects in a pool */


/*****************************************************************************/
/* Queue declarations                                                        */
/*****************************************************************************/

 struct YS__Qe {          /* Queue element; base of most other objects */
   char   *pnxt;                /* Next pointer for Pools                    */
   char   *pfnxt;                /* Next pointer for Pools                    */
   QE   *next;                  /* Pointer to the element after this one     */
   int  type;                   /* PROCTYPE or EVTYPE                        */
   int  id;                     /* System defined unique ID                  */
   char name[32];               /* User assignable name for dubugging        */
};

 struct YS__Queue {  /* Basic queue of QEs; derived from QE            */
                           /* QE fields */
   char   *pnxt;                /* Next pointer for Pools                    */
   char    *pfnxt;               /* Next pointer for Pools                    */
   QE      *next;               /* Pointer to the element after this one     */
   int     type;                /* Identifies the type of queue element      */
   int     id;                  /* System defined unique ID                  */
   char    name[32];            /* User assignable name for dubugging        */
                           /* QUEUE fields */
   QE      *head;               /* Pointer to the first element of the queue */
   QE      *tail;               /* Pointer to the last element of the queue  */
   int     size;                /* Number of elements in the queue           */
};

QUEUE  *YS__NewQueue();         /* Create and return a pointer to a new queue*/
char   *YS__QueueGetHead();     /* Removes & returns a pointer to the head of a queue */
void   YS__QueuePutHead();      /* Puts a queue element at the head of a queue        */
void   YS__QueuePutTail();      /* Puts a queue element at the tail of a queue        */
char   *YS__QueueNext();        /* Get pointer to element following an element        */
int    YS__QueueDelete();       /* Removes a specified element from the queue*/
void   YS__QueueReset();        /* Removes and frees all of the queue's elements      */
double YS__QueueHeadval();      /* Returns the time of the first queue element        */
void   YS__QueueInsert();       /* Inserts an element in order of its time, FIFO      */
void   YS__QueueEnter();        /* Enters an element in order of its time, LIFO       */
void   YS__QueuePrinsert();     /* Inserts an elem in order of its priority, FIFO     */
void   YS__QueueMyinsert();     /* Inserts an elem in order of priority, and enterque */
void   YS__QueuePrenter();      /* Enters an element in order of its priority, LIFO   */
void   YS__QueuePrint();        /* Prints the contents of a queue            */
int    YS__QeId();              /* Returns the system defined ID or 0 if TrID is 0    */
int    YS__QueueCheckElement(); /* Checks to see if an element is in a queue */
void   QueueCollectStats(SYNCQUE *,int,int,int,int,double,double);     /* Initiates statistics collection for a queue        */
void   QueueResetStats(SYNCQUE *);       /* Resets statistics collectin for a queue   */
STATREC* QueueStatPtr(SYNCQUE *,int);        /* Returns a pointer to a queue's statistics */

/*****************************************************************************/
/* Synchronization queue -- the base of semaphore and other YACSIM objects   */
/*****************************************************************************/

 struct YS__SyncQue {/* Synchronization Queue; base of sema, flg, cond, barr    */
                           /* QE fields */
   char   *pnxt;                /* Next pointer for Pools                    */
   char    *pfnxt;               /* Next pointer for Pools                    */
   QE      *next;               /* Pointer to the element after this one     */
   int     type;                /* Identifies the type of queue element      */
   int     id;                  /* System defined unique ID                  */
   char    name[32];            /* User assignable name for dubugging        */
                           /* QUEUE fields */
   QE      *head;               /* Pointer to the first element of the queue */
   QE      *tail;               /* Pointer to the last element of the queue  */
   int     size;                /* Number of elements in the queue           */
                           /* SYNCQUE fields */
   STATREC *lengthstat;         /* Queue length                              */
   STATREC *timestat;           /* Time in the queue                         */
};

/*****************************************************************************/
/* Semaphores declarations                                                   */
/*****************************************************************************/

 struct YS__Sema  {
                           /* QE fields */
   char   *pnxt;                /* Next pointer for Pools                    */
   char     *pfnxt;              /* Next pointer for Pools                    */
   QE       *next;              /* Pointer to the element after this one     */
   int      type;               /* SEMTYPE                                   */
   int      id;                 /* System defined unique ID                  */
   char     name[32];           /* User assignable name for dubugging        */
                           /* QUEUE fields */
   ACTIVITY *head;              /* Pointer to first element of the queue     */
   ACTIVITY *tail;              /* Pointer to last element of the queue      */
   int      size;               /* Number of elements in the queue           */
                           /* SYNCQUE fields */
   STATREC  *lengthstat;        /* Queue length                              */
   STATREC  *timestat;          /* Time in the queue                         */
                           /* SEMAPHORE fields */
   int      val;                /* Semaphore value                           */
   int      initval;            /* Initial value used for resetting the semaphore     */
};

SEMAPHORE *NewSemaphore();      /* Creates & returns a pointer to a new semaphore     */
int        SemaphoreInit();     /* If queue is empty its value is set to i   */
void       SemaphoreSignal();   /* Signals the semaphore                     */
void       SemaphoreSet();      /* Set sem. to 1 if empty, or release an waiting act. */
int        SemaphoreDecr();     /* Decrement the sem. value and return the new value  */
void       SemaphoreWait();     /* Wait on a semaphore                       */
int        SemaphoreValue();    /* Returns the value of the semaphore        */
int        SemaphoreWaiting();  /* Returns the # of activities in the queue  */


/*****************************************************************************/
/* Statistical queue -- the base of activities and messages                  */
/*****************************************************************************/

 struct YS__StatQe { /* Statistical queue element; base of activities & msgs    */
                           /* QE fields */
   char   *pnxt;                /* Next pointer for Pools                    */
   char       *pfnxt;            /* Next pointer for Pools                    */
   QE         *next;            /* Pointer to the element after this one     */
   int        type;             /* PROCTYPE or EVTYPE                        */
   int        id;               /* System defined unique ID                  */
   char       name[32];         /* User assignable name for dubugging        */
                           /* STATQE fields */
   double     enterque;         /* Time the activity enters a queue          */
};


/*****************************************************************************/
/* Activity: base of events and (YACSIM) processes                           */
/*****************************************************************************/


#define ACTIVITY_FRAMEWORK    char   *pnxt;                /* Next pointer for Pools                             */ \
   char       *pfnxt;            /* Next pointer for Pools                             */			    \
   QE         *next;            /* Pointer to the element after this one              */			    \
   int        type;             /* PROCTYPE or EVTYPE                                 */			    \
   int        id;               /* System defined unique ID                           */			    \
   char       name[32];         /* User assignable name for dubugging                 */			    \
                           /* STATQE fields */									    \
   double     enterque;         /* Time the activity enters a queue                   */			    \
                           /* ACTIVITY fields */								    \
   char       *argptr;          /* Pointer to arguments or data for this activity     */			    \
   int        argsize;          /* Size of argument structure                         */			    \
   double     time;             /* time used for ordering priority queues             */			    \
   int        status;           /* Status of activity (e.g., ready, waiting,...)      */			    \
   int        blkflg;           /* INDEPENDENT, BLOCK, or FORK                        */			    \
   STATREC    *statptr;         /* Statistics record for status statistics            */			    \
   double     priority;         /* Priority used for resource scheduling              */			    \
   double     timeleft;         /* Used for RR resources and time slicing             */			    \
   ACTIVITY   *rscnext;         /* Used for PROCSHAR resources only                   */			   


 struct YS__Act {
   ACTIVITY_FRAMEWORK
};

int      YS__ActId();           /* Returns the system define Id or 0 if TrID is 0     */
void     ActivitySetArg(ACTIVITY *,void *,int);      /* Sets the argument pointer of an activity  */
char     *ActivityGetArg(ACTIVITY *);     /* Returns the argument pointer of an activity        */
int      ActivityArgSize();     /* Returns the size of an argument           */
void     ActivitySchedTime(ACTIVITY *,double,int);   /* Schedules an activity to start in the future       */
void     ActivitySchedSema();   /* Schedules an activity to wait for a semaphore      */
void     ActivitySchedFlag();   /* Schedules an activity to wait for a flag  */
void     ActivitySchedCond();   /* Schedules an activity to wait for a condition      */
void     ActivitySchedRes();    /* Schedules an activity to wait use a resource       */
void     ActivityCollectStats();/* Activates statistics collection for an activity    */
void     ActivityStatRept();    /* Prints a report of an activity's statistics        */
STATREC  *ActivityStatPtr();    /* Returns a pointer to an activity's stat record     */
ACTIVITY *ActivityGetMyPtr();   /* Returns a pointer to the active activity  */
ACTIVITY *ActivityGetParPtr();  /* Returns a pointer to the active activity's parent  */

/*****************************************************************************/
/* Event: the basic unit of scheduling in the YACSIM driver as used by RSIM  */
/*****************************************************************************/

 struct YS__Event {
   ACTIVITY_FRAMEWORK
   func     body;               /* Defining function for the event           */
   int      state;              /* Used to save return point after reschedule*/
   int      deleteflag;         /* DELETE or NODELETE                        */
   int      evtype;             /* User defined event type                   */
};

EVENT *NewEvent(char *,void (*)(),int,int);              /* Creates and returns a pointer to a new event       */
int    EventGetType();      /* Returns the events type                      */
void   EventSetType();      /* Sets the event's type                        */
int    EventGetDelFlag();   /* Returns DELETE (1) or NODELETE (0)           */
void   EventSetDelFlag();   /* Makes an event deleting                      */
int    EventGetState();     /* Returns the state of an event                */
void   EventSetState();     /* Sets state used to designate a return point  */
void   EventReschedTime();  /* Reschedules an event to occur in the future  */
void   EventReschedSema();  /* Reschedules an event to wait for a semaphore */

/*****************************************************************************/
/* MESSAGE: used as the basic information exchange unit in NETSIM            */
/*****************************************************************************/

 struct YS__Mess {
                           /* QE fields */
   char   *pnxt;                /* Next pointer for Pools                    */
   char      *pfnxt;             /* Next pointer for Pools                    */
   QE        *next;             /* Pointer to the element after this one     */
   int       type;              /* Indentifies the type of QE                */
   int       id;                /* System defined unique ID                  */
   char      name[32];          /* User assignable name for dubugging        */
                           /* STATQE fields */
   double    enterque;
                           /* MESSAGE fields */
   char      *bufptr;           /* Pointer to the message contents           */
   int       msgtype;           /* User defined message type                 */
   int       msgtype2;          /* Holds function index for active messages:
				   unused */
   int       msgsize;           /* Size of the message in bytes              */
   int       pktsz;             /* LONG or SHORT packets                     */
   double    sendtime;          /* Time the message was sent                 */
   ACTIVITY  *source;           /* Pointer to sending activity               */
   int       srccpu;            /* ID of sending process' processor          */
   double    priority;          /* Priority used for resolving routing conflicts      */
   int       blockflag;         /* BLOCK or NOBLOCK                          */
   int       destflag;		/* msg sent to a PROCESSDEST or PROCESSORDEST*/
   PACKET    *packets;          /* Pointer to message's packets              */
   int       pktorecv;          /* Count of packets yet to be recieved       */
   int       pktosend;          /* Count of packets yet to be sent           */
};


/*****************************************************************************/
/* STATREC: used for calculating a variety of statistics                     */
/*****************************************************************************/

 struct YS__Stat {
                           /* QE fields */
   char   *pnxt;                /* Next pointer for Pools                    */
   char    *pfnxt;               /* Next pointer for Pools                    */
   QE      *next;               /* Pointer to the element after this one     */
   int     type;                /* PNTSTATTYPE or INTSTATTYPE                */
   int     id;                  /* System defined unique ID                  */
   char    name[32];            /* User assignable name for dubugging        */
                           /* STATRECORD Fields */
   double  maxval,minval;       /* Max and min update values encountered     */
   int     samples;             /* Number of updates                         */
   int     meanflag;            /* MEANS or NOMEANS                          */
   double  sum;                 /* Accumulated sum of update values          */
   double  sumsq;               /* Accumulated sum of squares of update values        */
   double  sumwt;               /* Accumulated sum of weights                */
   double  *hist;               /* Pointer to a histogram array              */
   int     histspecial;         /* Report only non-zero histogram bins?      */
   int     bins;                /* Number of bins of histograms              */
   double  time0;               /* Starting time of the current sampling interval     */
   double  time1;               /* Time of the last sample                   */
   double  lastt;               /* Last interval point entered with Update() */
   double  lastv;               /* Last interval value entered with Update() */
   double  interval;            /* The sampling interval                     */
   int     intervalerr;         /* Nonzero => a negative interval encountered*/
};

int      YS__StatrecId();       /* Returns the system defined ID or 0        */
STATREC  *NewStatrec(char *,int,int,int,int,double,double);         /* Creates and returns a pointer to a new statrec     */
void     StatrecSetHistSz();    /* Sets the default histogram size           */
void     StatrecReset(STATREC *);        /* Resets the statrec                        */
void     StatrecUpdate(STATREC *,double,double);       /* Updates the statrec                       */
void     StatrecReport(STATREC *);       /* Generates and displays a statrec report   */

void     StatrecReport_usr(STATREC *);   /* Generates and displays a statrec report   */
void     StatrecReport_usr1_avg(STATREC *,STATREC *);   /* Generates and displays a statrec report and averages into another */
int      StatrecSamples(STATREC *);      /* Returns the number of samples    */
double   StatrecMaxVal();       /* Returns the maximum sample value          */
double   StatrecMinVal();       /* Returns the minimum sample value          */
int      StatrecBins();         /* Returns the number of bins                */
double   StatrecLowBin();       /* Returns the low bin upper bound           */
double   StatrecHighBin();      /* Returns the high bin lower bound          */
double   StatrecBinSize();      /* Returns the bin size                      */
double   StatrecHist();         /* Returns the value of the ith histogram element     */
double   StatrecMean(STATREC *);         /* Returns the mean                 */
double   StatrecSum(STATREC *);         /* Returns the sum                   */
double   StatrecSdv(STATREC *);          /* Returns the standard deviation   */
double   StatrecInterval();     /* Returns the sampling interval             */
double   StatrecRate();         /* Returns the sampling rate                 */
void     StatrecEndInterval();  /* Terminates a sampling interval            */


/*****************************************************************************/
/* Event list operations                                                     */
/*****************************************************************************/

void     YS__EventListSetBins();  /* Sets the number of bins to a fixed size */
void     YS__EventListSetWidth(); /* Sets the bin width to a fixed size      */
void     YS__EventListInit();     /* Initializes the event list              */
void     YS__EventListInsert();   /* Inserts an element into the event list  */
ACTIVITY *YS__EventListGetHead(); /* Returns the head of the event list      */
double   YS__EventListHeadval();  /* Returns the time value of the event list head    */
int      YS__EventListDelete();   /* Removes an element from the event list  */
void     YS__EventListPrint();    /* Lists the contents of the event list    */
int      EventListSize();         /* Returns the size of the event list      */
void     EventListSelect();       /* Selects the type of event list to use   */
void     EventListCollectStats(); /* Activates auto stats collection for event list   */
void     EventListResetStats();   /* Resest a statistics record of a queue   */
STATREC  *EventListStatPtr();     /* Returns a pointer to a event list's statrec      */

/*****************************************************************************/
/* Driver operations                                                         */
/*****************************************************************************/

void YS__RdyListAppend();       /* Appends an activity onto the systme ready list     */
void DriverReset();             /* Resets the driver (Sets YS__Simtime to 0) */
void DriverInterrupt();         /* Interrupts the driver and returns to the user      */
int  DriverRun(double);               /* Activates the simulation driver; returns a value   */


/*****************************************************************************/
/* Utility operations -- provide some useful functions                       */
/*****************************************************************************/


void   YS__errmsg(char *);           /* Prints error message & terminates simulation        */
void   YS__warnmsg();          /* Prints warning message (if TraceLevel > 0) */
void   YS__SendPrbPkt();       /* Sends a trace message to a probe           */

double GetSimTime();           /* Returns the current simulation time        */
void   TracePrint();
void   TracePrintTag();


/*****************************************************************************/
/* PROCESSOR operations                                                      */
/*****************************************************************************/

PROCESSOR *NewProcessor();        /* Creates a new PROCESSOR                 */
void      ProcessorUtilRept();    /* Prints out the processor util statistics*/
void      YS__PSDelay();


/*****************************************************************************/
/* NETSIM Module -- not to be confused with "SMMODULE" type or common        */
/* MODULE_FRAMEWORK. This module type here is a base for buffers, muxes,     */
/* demuxes, network IPORTs, and network OPORTS                               */
/*****************************************************************************/

struct YS__Mod
{
   int id;                   /* User Supplied ID for debugging               */
   int type;                 /* Module type - BUFFERTYPE, MUXTYPE, DEMUXTYPE PORTTYPE */
};

/*****************************************************************************/
/* NETSIM multiplexer                                                        */
/*****************************************************************************/

struct YS__Mux
{
   int id;                      /* User Supplied ID for debugging            */
   int type;                    /* Module type = MUXTYPE                     */

   MODULE *nextmodule;          /* Pointer to the next module connected to this one   */
   SEMAPHORE *arbsema;          /* Pointer to the semaphore for this MUX     */
   int index;                   /* Input index if next module is a MUX       */
   MUX *muxptr;                 /* Pointer to a preceeding MUX               */
   int fan_in;                  /* No of inputs to the MUX                   */
};

MUX       *NewMux();              /* Creates and returns a pointer to a new mux */

/*****************************************************************************/
/* NETSIM demultiplexer                                                      */
/*****************************************************************************/

struct YS__Demux
{
   int id;                         /* User Supplied ID for debugging         */
   int type;                       /* Module type = DEMUXTYPE                */

   MODULE **nextmodule;            /* Pointers to the next modules           */
   int *index;                     /* Input index if next module is a MUX    */
   int fan_out;                    /* Fan-out of the demux                   */
   rtfunc2 router;                  /* Pointer to a routing function          */
};

DEMUX     *NewDemux();            /* Creates and returns a pointer to a new demux     */


/*****************************************************************************/
/* NETSIM input ports: note that NETSIM ports are not to be confused with    */
/* the "SMPORT"s used to connect between various "SMMODULES", as discussed   */
/* in module.h                                                               */
/*****************************************************************************/

struct YS__IPort
{
   int id;                      /* User Supplied ID for debugging            */
   int type;                    /* Module type - IPORTTYPE                   */

   MODULE *nextmodule;          /* Pointer to the next module connected to this one   */
   MODULE *destination;         /* Pointer to next module for tail to enter  */
   int index;                   /* Input index if next module is a MUX       */
   SEMAPHORE *netrdy;           /* Semaphore that controls packet movement into net   */
   SEMAPHORE *portrdy;          /* Semaphore for users to wait at            */
   int qfree;                   /* Number of packets that can be queued at port       */
};

IPORT     *NewIPort();            /* Creates and returns a pointer to a new iport     */
SEMAPHORE *IPortSemaphore();      /* Returns a pointer to the ready sema of an iport  */
int       IPortSpace();           /* Returns the # of free packet slots in an iport   */

/*****************************************************************************/
/* NETSIM output ports                                                       */
/*****************************************************************************/

struct YS__OPort
{
   int id;                      /* User Supplied ID for debugging            */
   int type;                    /* Module type - OPORTTYPE                   */

   SEMAPHORE *freespace;        /* Free packet space avaialble in port       */
   SEMAPHORE *pktavail;         /* Packet available to processor             */
   PACKET *qhead;               /* Head pointer for the queue                */
   PACKET *qtail;               /* Tail pointer for the queue                */
   MUX *muxptr;                 /* Pointer to a preceeding MUX               */
   int count;                   /* Number of packets in the port's queue     */
   PROCESSOR *pr;               /* Processor to which the oport is connected */
   EVENT *event;                /* PacketReceiverEvent running on procr if attached   */
   double channel_busy;		/* for network utilization stats             */
   double begin_util;           /* for new utilization stats                 */
   double time_of_last_clear;   /* " " " " */
};

OPORT     *NewOPort();          /* Creates and returns a pointer to a new oport       */
SEMAPHORE *OPortSemaphore();    /* Returns a pointer to the req sema of an oport      */
int       OPortPackets();       /* Returns the number of pkts avail at an oport       */


/*****************************************************************************/
/* NETSIM switch buffers                                                     */
/*****************************************************************************/

struct YS__Buf
{
   int id;                      /* User Supplied ID for debugging            */
   int type;                    /* Module type = BUFFERTYPE                  */
 
   PACKET *head;                /* Pointer to element at the front of the buffer      */
   PACKET *tail;                /* Pointer to element at the end of the buffer        */ 
   int    size;                 /* Size of the buffer in flits               */
   int    free;                 /* Number of free flit positions             */
   int    tailtype;             /* Type of last event in buffer: HEADTYPE or TAILTYPE */
   MODULE *nextmodule;          /* Pointer to the next module connected to this one   */
   MODULE *destination;         /* Pointer to next module for tail to enter  */
   EVENT  *WaitingHead;         /* Pointer to pkt head waiting to enter the buffer    */
   MUX    *muxptr;              /* Pointer to semaphore acquired by HeadEvent*/
   int    index;                /* Input index if next module is a MUX       */
   double channel_busy;		/* Time for which channel is busy            */
   double begin_util;           /* for new utilization stats                 */
   double time_of_last_clear;   /* stats */
};

BUFFER    *NewBuffer();           /* Creates and returns a pointer to a new buffer    */


/*****************************************************************************/
/* NETSIM packet data -- carries the information being sent on the network   */
/*****************************************************************************/

struct YS__PktData
{
   int     seqno;               /* User supplied id for sequencing packts of a msg    */
   MESSAGE *mesgptr;            /* Pointer the message for this packet       */
   int     pktsize;             /* Number of flits in the packet             */
   int     srccpu;              /* ID of CPU sending message                 */
   int     destcpu;             /* ID of CPU receiving message               */
   int     num_hops;		/* Number of hops traveled, for stats */
   double  recvtime;            /* Time to move the packet out of an output port      */

   double  createtime;          /* Time the packet was created               */
   double  nettime;             /* Time the packet spent in the network      */
   double  blktime;             /* Time the packet was blocked in the network*/
   double  oporttime;           /* Time th packet spent waiting in an output port     */
};


/*****************************************************************************/
/* NETSIM packets -- these consist of a number of flits, and are moved       */
/* through the network switches according to the specified rules             */
/*****************************************************************************/

struct YS__Pkt
{
   char   *pnxt;                /* Next pointer for Pools                    */
   char    *pfnxt;          /* Next pointer for Pools                        */
   PACKET  *next;          /* Used in message operations                     */

   struct YS__PktData data;/* Packet's user accessible data (See prev declaration)    */

   MUX     *muxptr;        /* Pointer to the last MUX passed through by the head      */
   int     index;          /* Index used for routing out of a demux          */
   EVENT   *headev;        /* Pointer to this packet's head event            */
   MODULE  *module;        /* Pointer to the module that the head is at      */
   EVENT   *SleepingTail;  /* Pointer to the tail event of this packet, if sleeping   */
   int     waitingfortail; /* Used in SAF when head must wait for tail       */
   IPORT   *lastiport;     /* Iport through which the packet entered the network      */

   /* The flits of a packet can be in different buffers at the same
      time.  Headbuf and Tailbuf point to the buffers that contain the
      head and tail.  Since the head and tail flits can be in the
      event list at the same time they are in buffers, we can not use
      the event's next pointer for buffers.  Therefore headnext and
      tailnext are fields in the packet's descriptor for this purpose.
      Tailoffset indicates the number of flit positions that the tail
      is from the front of a buffer if it is the first element in the
      buffer queue.  That is, the tail flit may be at the front of the
      queue, but there may still be some flits internal to the packet
      ahead of the tail flit in the buffer.  */

   MODULE  *headbuf;       /* Pointer to the module that holds the head      */
   MODULE  *tailbuf;       /* Pointer to the module that holds the tail      */
   PACKET  *headnext;      /* Next pointer for the head event                */
   PACKET  *tailnext;      /* Next pointer for the tail event                */
   int     tailoffset;     /* Offset of the tail from the front of its buffer*/
};

PACKET    *NewPacket();           /* Creates and returns a pointer to a new packet    */
double    PacketSend();           /* Sends a packet to a network iport       */
PACKET    *PacketReceive();       /* Receives a packet from a network oport  */
PKTDATA   *PacketGetData();       /* Returns a pointer to the data in a packet        */
void      PacketFree();           /* Returns a packet to the pool of free packets     */

/*****************************************************************************/
/* NETSIM network operations. Set parameters, collect statistics, etc.       */
/*****************************************************************************/

void    NetworkConnect();       /* Connects two network modules              */
void    NetworkSetCycleTime();  /* Sets the cycle time; all other times are multiples */
void    NetworkSetFlitDelay();  /* Sets the flit delay                       */
void    NetworkSetMuxDelay();   /* Sets the time to move a flit through a mux*/
void    NetworkSetArbDelay();   /* Sets the time for a flit to arbitrate at a mux     */
void    NetworkSetDemuxDelay(); /* Sets the time to move a flit through a demux       */
void    NetworkSetPktDelay();   /* Sets the packet delay                     */
void    NetworkSetThresh();     /* Sets the buffer threshold                 */
void    NetworkSetWFT();        /* Sets the WFT mode                         */
void    NetworkCollectStats();  /* Activates stat colleciton for the network */
void    NetworkResetStats();    /* Resets all network statistics records     */
STATREC *NetworkStatPtr();      /* Returns a pointer to a network statistics record   */
void    NetworkStatRept();      /* Prints a report of network statistics     */

#endif

