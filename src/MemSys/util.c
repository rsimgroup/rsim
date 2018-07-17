/*
  util.c

  This function defines some useful global variables (such as the pools
  used for memory allocation.)

  Additionally, this file provides functions used for error messages and
  warning messages, as well as functions for tracing.

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
#include "MemSys/dbsim.h"
#include "Processor/simio.h"
#include <varargs.h>
#include <string.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <math.h>

/*****************************************************************************\
********************************* Global Variables ****************************
*******************************************************************************
**                                                                           **
**  All global variabes, except those that are are static and only accessed  **
**  in a single file, are defined here.  There is a corresponding extern     **
**  declaration of these variables in the file simsys.h. Declarations that   **
**  start with YS__ are not visible to the user.                             **
**                                                                           **
*******************************************************************************
\*****************************************************************************/

POOL    YS__MsgPool;                     /* Pool of MESSAGE descriptors      */
POOL    YS__EventPool;                   /* Pool of EVENT descriptors        */
POOL    YS__QueuePool;                   /* Pool of QUEUE descriptors        */
POOL    YS__SemPool;                     /* Pool of SEMAPHORE descriptors    */
POOL    YS__QelemPool;                   /* Pool of QELEM descriptors        */
POOL    YS__StatPool;                    /* Pool of STATREC descriptors      */
POOL    YS__HistPool;                    /* Pool of default size histograms  */
POOL    YS__ModulePool;                  /* POOL of SMMODULE descriptors     */
POOL    YS__CachePool;                   /* POOL of cache descriptors        */
POOL    YS__BusPool;                     /* POOL of bus descriptors          */
POOL    YS__ReqPool;                     /* POOL of REQ descriptors          */
POOL    YS__SMPortPool;                  /* POOL of SMPORT descriptors       */
POOL    YS__DirstPool;		         /* Pool of Dir line structures      */
POOL    YS__DirEPPool;		         /* Pool of Dir extra structures     */

EVENT   *YS__ActEvnt;                    /* Pointer to the currently active
					    event event  */
double  YS__Simtime;                     /* The current simulation time      */

int     YS__idctr;                       /* Used to generate unique ids for
					    objects   */
char    YS__prbpkt[1024];                /* Buffer for probe packets         */
int     YS__msgcounter = 0;              /* System defined unique message ID */
int     YS__interactive = 0;             /* Flag; set if for viewsim or shsim*/

QUEUE   *YS__ActPrcr;                    /* List of active processors        */
POOL    YS__PrcrPool;                    /*POOL of PROCESSOR descriptors    */
POOL    YS__PktPool;                     /*POOL of packet descriptors       */

int     YS__Cycles = 0;                  /* Count of accumulated profiling
					    cycles     */
double  YS__CycleTime = 1.0;             /* Cycle Time                       */
int     YS__NetCycleTime = 1.0;          /* Network cycle time (in clock
					    cycles) */

STATREC *YS__BusyPrcrStat;		 /* Statrec for processor utilization*/
int	YS__TotalPrcrs;			 /* Number of total processors 	     */

/****************** Global variables visible to the user *********************/

int     TraceLevel = 0;                  /* Controls the amount of trace
					    information  */
int     TraceIDs = 1;                    /* If != 0, show object ids in trace
					    output  */

/*****************************************************************************/
/*  YS__errmsg and YS__warnmsg: miscellaneous routines for reporting errors  */
/*                              and warnings                                 */
/*****************************************************************************/

void YS__errmsg(s)        /* Prints error message & terminates simulation             */
char *s;
{
   fprintf(simerr, "\nERROR AT TIME <%g>: %s \n\n",YS__Simtime,s); 
   exit(-1);
}

/****************************************************************************/

void YS__warnmsg(s)       /* Prints warning message (if TraceLevel > 0)               */
char *s;
{
   if (TraceLevel >= MAXDBLEVEL-4)
      TracePrintTag("warnings","\nWARNING: %s\n\n",s);
}

/*****************************************************************************/
/*  GetSimTime: returns simulation time                                      */
/*****************************************************************************/

double GetSimTime()
{
   return (YS__Simtime);
}

/*****************************************************************************/
/* Trace Output routines:                                                    */
/* These routines print tracing information on the simulator output.         */
/*****************************************************************************/

void YS__SendPrbPkt(type, name, data)  /* Sends a packet to a probe         */
char type;                             /* The type of the probe             */
char *name;                            /* Name of the object generating
					  the packet    */
char *data;                            /* Packet data                       */
{
  /* These functions have a different role with dbsim than with RSIM.
     In RSIM, trace messages go only to simulator output currently. */
  
  if (type==TEXTPROBE)     /* The only types of probes supported in RSIM  */
    fprintf(simout,data);  /* are TEXT PROBES                             */
}

void TracePrint(va_alist)     /* Sends user generated messages to text probe */
va_dcl                        /* Arguments exactly like printf              */
{
    va_list var;
    char *fmt;

    va_start(var);
    fmt = va_arg(var, char *);  /* The first argument is the format string            */

    vfprintf(simout,fmt, var);       
    
    va_end(var);
}

void TracePrintTag(va_alist)  /* Sends a tagged message to a text probe     */
va_dcl                        /* Arguments are a tag and then printf style  */
{                             /* Probes can filter on this tag              */
    va_list var;
    char *tag, *fmt;
    
    va_start(var);
    tag = va_arg(var, char *); /* The first argument is a tag                         */
    fmt = va_arg(var, char *); /* The next arguemnt is the format string              */

    vfprintf(simout,fmt, var);

    va_end(var);
}
