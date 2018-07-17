/*
  wbuffer.c

  This file contains the initialization and statistics routines used in
  the write buffer.

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
#include "MemSys/cache.h"
#include "MemSys/module.h"
#include "MemSys/net.h"
#include "Processor/simio.h"
#include <malloc.h>

static int wbuffer_index=0;
static WBUFFER *wbuffer_ptr[MAX_MEMSYS_PROCS];
extern ARG *AllWbuffers[];

struct state;

/*****************************************************************************/
/* NewWBuffer: initializes and returns a pointer to a wbuffer module.        */
/*****************************************************************************/

WBUFFER *NewWBuffer(char *name, int node_num, int stat_level, rtfunc routing,
		    struct Delays *Delays, int ports_abv, int ports_blw, 
		    int size, int wbuf_type, func prnt_rtn)
    
#if 0
     char   *name;		/* Name of the module upto 32 characters long          */
    int    node_num;            /* Node number of write buffer */
    int    stat_level;		/* Level of statistics collection for this module      */
    rtfunc routing;		/* Routing function of the module                      */
    char   *Delays;		/* Pointer to user-defined structure for delays        */
    int    ports_abv;		/* Number of ports above this module                   */
    int    ports_blw;		/* Number of ports below this module                   */
    
    int    size;		/* Size of the write buffer (num writes to buffer      */
    int    wbuf_type;		
    func   prnt_rtn;            /* Routine for printing stats at end                   */
#endif
{
    WBUFFER *wbufptr;
    ARG *arg;
    char evnt_name[32];
    
    wbufptr = (WBUFFER *)malloc (sizeof(WBUFFER)); /* allocate structure */
    if (wbuffer_index < MAX_MEMSYS_PROCS) /* used for reporting all statistics */
	{
	    wbuffer_ptr[wbuffer_index] = wbufptr; /* keep this pointer in
						     array used by StatAll
						     functions */
	    wbuffer_index ++;
	}
    wbufptr->id = YS__idctr++;	/* System assigned unique ID   */
    
    strncpy(wbufptr->name, name,31); /* get name of this module */
    wbufptr->name[31] = '\0';
    
    wbufptr->module_type = WBUF_MODULE;
    /* Initialize the data structures common to all shared memory modules */
    ModuleInit ((SMMODULE *)wbufptr, node_num, stat_level, routing, Delays,
		ports_abv + ports_blw, DEFAULT_Q_SZ);
    wbufptr->num_ports_abv = ports_abv; /* assign number of ports above
					   this unit -- used in routing
					   cases */
    
    sprintf(evnt_name, "%s_wbufsim",name);
    
    /* Set ARG data structure so that cycle-by-cycle simulator can use it */
    arg = (ARG *)malloc(sizeof(ARG));
    arg->mptr = (SMMODULE *)wbufptr;
    AllWbuffers[node_num] = arg; 

    /* The RMQ data structure is used for round-robin scheduling */
    wbufptr->rm_q = (char *)malloc(sizeof(RMQ));
    if (wbufptr->rm_q == NULL)
	YS__errmsg("NewWBuffer(): malloc failed");
    ((RMQ *)(wbufptr->rm_q))->u1.abv = 0;
    ((RMQ *)(wbufptr->rm_q))->u2.blw = 0;

    wbufptr->req = NULL; /* message being processed */
    wbufptr->in_port_num = -1; /* in-port of current message */
    
    if (wbuf_type == DIR_RC)
      {
	wbufptr->wakeup = NULL;
	wbufptr->handshake = NULL; /* handshake_WB; */
      }
    else{
      YS__errmsg("Unknown type of write buffer\n");
    }
    
    wbufptr->wbuf_type = wbuf_type;	
    
    wbufptr->wbuf_sz_tot = size;  /* maximum wbuffer entries */
    wbufptr->wbuf_sz = 0;         /* operations waiting to issue to next
				     level */
    wbufptr->counter = 0;         /* outstanding operations (for fence) */
    wbufptr->prnt_rtn = prnt_rtn; /* prints out statistics */

    /* Clear out relevant statistics -- explained in cache.h */
    wbufptr->stall_wb_full=0;
    wbufptr->stall_coal_max=0;
    wbufptr->stall_read_match=0;
    wbufptr->coals=0;
    
    return wbufptr;
}


/*****************************************************************************/
/* WBufferStatReportAll(): Reports statistics of all Wbuffer modules         */
/*****************************************************************************/

void WBufferStatReportAll()
{
  int i;
  if (wbuffer_index)	{
    if (wbuffer_index > MAX_MEMSYS_PROCS) 
      YS__warnmsg("Too many write buffer modules created; only first set of statistics will be reported by WBufferStatReportAll");
    fprintf(simout,"\n##### wbuffer Statistics #####\n");
    for (i=0; i< wbuffer_index; i++) 
      if (wbuffer_ptr[i]->prnt_rtn)
	(wbuffer_ptr[i]->prnt_rtn)(wbuffer_ptr[i]);
  }
}

/*****************************************************************************/
/* WBufferStatClearAll(): Resets statistics collection at all Wbuffers       */
/*****************************************************************************/

void WBufferStatClearAll()
{
  int i;
  if (wbuffer_index){
    for (i=0; i< wbuffer_index; i++) 
      WBufferStatClear(wbuffer_ptr[i]);
  }
}


/*****************************************************************************/
/* WBufferStatReport: Reports statistics of a WBuffer module                 */
/*****************************************************************************/

void WBufferStatReport(WBUFFER *wbufptr)
{
    wbufptr->num_lat = wbufptr->num_ref;
    fprintf(simout,"%s\tCoalescings: %d\tStalls\tFull: %d\tMAX_COAL: %d\tread_match: %d\n",
	    wbufptr->name, wbufptr->coals,wbufptr->stall_wb_full,
	    wbufptr->stall_coal_max,wbufptr->stall_read_match);
}

/*****************************************************************************/
/* WBufferStatClear: Resets statistics collection for a WBuffer module       */
/*****************************************************************************/

void WBufferStatClear (WBUFFER *wbufptr)
{
    SMModuleStatClear(wbufptr);

    wbufptr->stall_wb_full=0;
    wbufptr->stall_coal_max=0;
    wbufptr->stall_read_match=0;
    wbufptr->coals=0;
}


