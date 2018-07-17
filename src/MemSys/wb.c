/*
  wb.c

  This file contains the actual simulation routines for the coalescing
  write buffer.

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
#include "MemSys/req.h"
#include "Processor/memprocess.h"
#include "MemSys/net.h"
#include "MemSys/arch.h"
#include "Processor/mainsim.h"
#include "Processor/simio.h"
#include <malloc.h>

static int notpres_wb(WBUFFER *, REQ *); /* function to check for a read-match
					    or a coalescing write */
/*****************************************************************************/
/* WBSim: This module simulates a write-buffer for a no-write-allocate       */
/* write-through primary cache.  It is connected to a non-blocking           */
/* write-allocate write-back secondary cache.                                */
/* This is a coalescing line write buffer, used in systems with a            */
/* write-through L1 cache. Although the write buffer is conceptually         */
/* in parallel with the L1 cache, the MemSys (RPPT) module sits		     */
/* between the two caches. In order to provide the semblance of		     */
/* parallel access, the write buffer has zero delay.			     */
/*									     */
/* Reads are normally allowed to bypass the write-buffer, but stall in       */
/* case of a match. The read cannot simply be forwarded with a		     */
/* write-value if available, as the read has already booked an MSHR at	     */
/* the L1 cache and other requests may have coalesced with it		     */
/* already. So, we choose to stall the read until the corresponding	     */
/* write is issued from the write buffer.                                    */
/*									     */
/* Writes to the same L2 cache line coalesce in the write-buffer and	     */
/* are sent down as a single request. A write is sent from the		     */
/* write-buffer as soon as there is port space for it without		     */
/* interfering with a read in the same cycle.                                */
/*****************************************************************************/

struct state;

void WBSim(struct state *proc, int call_type)
{    
  WBUFFER *wbufptr;      /* Wbuffer being simulated */
  ARG *argptr;           /* processor structure that stores wbufptr */
  REQ *req;              /* request being processed */
  int case_num;          /* state of wbuffer processing */
  int oport_num, reqsrc; /* destination/source ports of request */
  int i;                 /* general counter variable */
  int hittype;           /* checks for write coalesce or read match */
  int done = FALSE;
  REQ *tempreq;          /* issues writes from buffer */
  REQ **req_array;       /* used for coalescing */
  WBUFITEM *tempreq1;    /* used for buffering writes */

  /* Find out which write-buffer is being simulated */
  argptr = (ARG *)GetWBArgPtr(proc);
  wbufptr = (WBUFFER *)argptr->mptr;
  req = wbufptr->req; /* partially-processed request, if any */
  case_num = 0; 
#ifdef COREFILE1
  if(YS__Simtime > DEBUG_TIME)
    fprintf(simout,"%s calling WBsim %g\n",wbufptr->name, YS__Simtime);
#endif
  while(!done)    {
    switch (case_num)	{
    case 0:
      /* Get an input request from the specified queue */
      /* We use call_type to indicate which queue to look at */
      req = peekQ(wbufptr->in_port_ptr[call_type]);
      if(call_type == L1WB && req == NULL){
	wbufptr->inq_empty = 1;
      }
      if(req)
	{
	  req->in_port_num = call_type; /* port from which it was obtained */
	  reqsrc = req->in_port_num+1;
	}

      wbufptr->in_port_num = -1;   /* To initialize the RR for next time */
      if(req == NULL){
	/* No inputs to read */
	if(wbufptr->write_buffer && call_type == L1WB){
	  /* Check for L1WB -- we want to trickle out entries from the
	     write buffer only if we are coming down from the L1. It
	     is possible otherwise that we trickled out writebuffer
	     entries at the rate of 2 per cycle! */
	  /* At least trickle out an write entry */
	  case_num = 9;
	  break;
	}
	else{
	  /* Nothing to do -- no inputs, no write buffer entries -- go
             to sleep */
	  done = TRUE;
	  break;
	}
      }
      
      wbufptr->state = PROCESSING;

      if (req->s.type == REQUEST) 
	case_num = 1;
      else if (req->s.type == REPLY)
	case_num = 3;
      else if (req->s.type == COHE)
	case_num = 2;
      else if (req->s.type == COHE_REPLY)
	case_num = 2;
      else{
	YS__errmsg("WBSim(): Unknown request type");
      }
      break;
      
    case 1: /* process incoming REQUEST */
#ifdef DEBUG_WBUFFER
	  if (YS__Simtime>DEBUG_TIME)
	    fprintf(simout,"1WB: REQ\t%s\t\t&:%ld\tinst_tag:%d\tTAG:%ld\tType:%s\tSize:%d\tCNT:%d prreq %s @%1.0f Sz:%d src=%d dest=%d wbsz %d\n",
		   wbufptr->name, req->address,req->s.inst_tag,req->tag, Req_Type[req->req_type],
		   wbufptr->wbuf_sz,wbufptr->counter,Req_Type[req->prcr_req_type],
		   YS__Simtime, req->size_st,req->src_node,req->dest_node, wbufptr->wbuf_sz);
#endif
      if((req->prcr_req_type == READ) ||
	 req->prcr_req_type == RMW ||
	 req->prcr_req_type == L1READ_PREFETCH ||
	 req->prcr_req_type == L1WRITE_PREFETCH ||
	 req->prcr_req_type == L2READ_PREFETCH ||
	 req->prcr_req_type == L2WRITE_PREFETCH){
	/* Read stalls get done here */

	hittype = notpres_wb(wbufptr,req);
	if (hittype != 2) /* a read matched a write tag!!! */
	  {
#ifdef DEBUG_WBUFFER
	    if (!req->s.prefetch && hittype == 0) /* exact match */
	      fprintf(simerr," : ");
#endif
	    if (req->s.prefetch)
	      {
		/* Code can be added here to either drop the prefetch
		   (which will not be useful), or to let it pass
		   (since it won't conflict, as it's a
		   prefetch). However, for now just go ahead and
		   stall it -- this situation is relatively unlikely
		   anyway */
	      }
	    
	    /* This is a read-match request stall */
	    /* Dont commit request */
	    req = NULL;
	    wbufptr->stall_read_match++;
	    case_num = 9;
	    break;
	  }
	
	/* Send it on to the next module */
	case_num = 7;
	break;
      }
      /* These are writes */

#ifndef STORE_ORDERING
      if (!simulate_ilp)  /* these processors do not commit writes until
			     they reach the write-buffer */
	AckWriteToWBUF(req->s.inst,req->s.proc);
#endif
      
      hittype = notpres_wb(wbufptr, req); /* checks and adds */
       if(hittype != -1){
	/* This request has been added or coalesced into the write buffer */
	commit_req((SMMODULE *)wbufptr, req->in_port_num ); /* can pull it out of the input now */
	req = NULL;
	case_num = 9;
	break;
      }
      /* This is a structural hazard stall */
      /* Don't commit request -- we'll process it in a later cycle */
      req = NULL;
      case_num = 9;
      break;
      
    case 2: /* process COHE */
      YS__errmsg("No coherence messages handled at the write buffer\n");
      
    case 3: /* process incoming REPLY  */
#ifdef DEBUG_WBUFFER
      if(!req->inuse){
	YS__errmsg("Freed request at the write buffer\n");
      }
      if (YS__Simtime > DEBUG_TIME)
	{
	  fprintf(simout,"3WB: REPLY\t%s\t\t&:%ld\tinsttag:%d\tTAG:%ld\t%s\tSz:%d\tCNT:%d prreq = %s @%1.0f src=%d dest=%d wbufsz %d\n",
		 wbufptr->name, req->address, req->s.inst_tag,req->tag, Req_Type[req->req_type],
		 wbufptr->wbuf_sz,wbufptr->counter, Req_Type[req->prcr_req_type],
		 YS__Simtime,req->src_node,req->dest_node, wbufptr->wbuf_sz);
	}
#endif
      /* No special processing is done for REPLYs at the write buffer.
	 These are just passed on to the next module */
      case_num = 7; /* Forward it to the next module */
      break;
      
    case 7:
      /* Send a request to the next module */
      oport_num = wbufptr->routing((SMMODULE *)wbufptr, req);
      if(wbufptr->out_port_ptr[oport_num]->ov_req){
	/* Structural hazard -- go to sleep */
	wbufptr->req = NULL;
	done = TRUE;
	break;
      }
      /* No structural hazard -- commit request */
      commit_req((SMMODULE *)wbufptr, req->in_port_num);
      /* add request to output port */
      new_add_req(wbufptr->out_port_ptr[oport_num], req);
      req = NULL;
      case_num = 10;
      break;
      
    case 9:
      /* Handle clear up from the write buffer code */
      if(req){
	/* This is not the case to come to for req -- go to case 7 */
	YS__errmsg("Error in case allocation\n");
      }
      if(!req){
	if(wbufptr->write_buffer){ /* are there any entries in write buffer? */
	  /* If so, try to send one out now */
	  tempreq = wbufptr->write_buffer->req; /* Get first request */
	  req = tempreq;
	  /* See which port it has to go to */
	  oport_num = wbufptr->routing((SMMODULE *)wbufptr, tempreq);

	  /* Does the port have space? */
	  if(wbufptr->out_port_ptr[oport_num]->ov_req){
	    /* Structural hazard -- go to sleep */
	    wbufptr->req = NULL;
	    done = TRUE;
	    break;	      
	  }
	  else{
	    /* There is space in the port, so remove this element and send
	       it out */
	    tempreq1 = wbufptr->write_buffer; /* note the entry */
	    wbufptr->wbuf_sz--;            /* size of write buffer decreases */
	    if(wbufptr->wbuf_sz == 0)      /* Is write-buffer now empty? */
	      wbufptr->pipe_empty = 1;     /* Let cycle-by-cycle know, so that
					      it doesn't have to simulate an
					      empty write buffer */
#ifdef DEBUG_WBUFFER
	    if(YS__Simtime > DEBUG_TIME)
	      fprintf(simout, " Write buffer %s has %d entries now\n",wbufptr->name, wbufptr->wbuf_sz);
#endif
	    /* We have to send a message to the L2 with all the
	       coalescing information too */
	    req->coal_count = tempreq1->counter;
	    if (req->coal_count)
	      {
		/* Bring all the coalesced requests from the WBUFITEM
		   into the REQ data structure */
		req_array = (REQ **)malloc(sizeof(REQ *)*tempreq1->counter);
		for(i=0;i<tempreq1->counter;i++){
		  req_array[i] = tempreq1->coal_req[i];
		}
	      }
	    else
	      req_array = NULL;
	    req->coal_req_array = req_array;
	    
	    wbufptr->write_buffer = tempreq1->next; /* Now the next entry is
						       new head of write-buf */
	    free(tempreq1); /* free up the write buffer item */

	    /* Send out the request -- we have already checked that this
	       has the space it needs */
	    new_add_req(wbufptr->out_port_ptr[oport_num], tempreq);
	  }
	}
      }
      /* NO break here -- go on to case 10 */
    case 10:
      /* Done with WBSim for this cycle */
      done = TRUE;
      break;
    default:
      YS__errmsg("WBSim(): unknown case number");
    }
  }
}

/*****************************************************************************/
/* notpres_wb: checks if an incoming REQUEST matches the tag of an entry in  */
/* the write buffer. For reads, stall until corresponding entry is freed.    */
/* For writes, attempt to coalesce.                                          */
/*****************************************************************************/

static int notpres_wb(WBUFFER *wbufptr, REQ *req)
{
  WBUFITEM *newitem = NULL, *prev_req=NULL, *tempreq=NULL;
  /* This procedure checks for the presence of a request at the write-buffer
     This has different functionality for READS and WRITES */
  int i;
  int hittype = 2;   /* Miss */
  tempreq = wbufptr->write_buffer;
  
  while (tempreq != NULL)   { /* are there entries left in the write buffer? */
    if ((req->tag == tempreq->tag)){ /* does the tag match? */
      hittype = 1;
      /* Same line-- let use see if same word too */
      if(tempreq->req->address == req->address){
	/* Same address too */
	hittype = 0;
	break;
      }
      for(i=0;i<tempreq->counter;i++){ /* check all coalesced accesses too */
	if(tempreq->coal_req[i]->address == req->address){
	  /* Same address too */
	  hittype = 0;
	  break;
	}
      }
      break;
    }
    prev_req = tempreq;
    tempreq = tempreq->next;
  }

  if(hittype != 2){ /* If it's some sort of match or coalesce */
    if(req->prcr_req_type == WRITE) { 
      /* We need to coalesce writes if hittype is 0 or 1 */
      if(tempreq->counter == MAX_COALS){
	/* More than maximum allowed coalesced writes */
	wbufptr->stall_coal_max++;
	return -1; /* Stall */
      }
      
      /* This write successfully coalesces */
      
      tempreq->coal_req[tempreq->counter++] = req;
      wbufptr->coals++;
      req->handled=reqWBCOAL;
      return hittype;
    }
  }
  
  if(!(req->prcr_req_type == WRITE)) {
    /* This is a read of some sort. Return whether or not
       we had a read match (so WBSim can decide whether to stall
       or pass it down */
    return hittype;
  }

  if(hittype != 2)
    YS__errmsg("Error in wb coalescing/matching.");

  /* This access is a write and was not a coalesced access.
     Allocate a new entry in the write-buffer (if available),
     and buffer the write */
  
  if(wbufptr->wbuf_sz == wbufptr->wbuf_sz_tot){
    /* All entries in write buffer are full -- structural hazard */
    wbufptr->stall_wb_full++;
    return -1; /* Stall */
  }
  /* allocate new entry */
  wbufptr->wbuf_sz++;

  wbufptr->pipe_empty = 0; /* Let cycle-by-cycle simulator know that
			      write-buffer has entries and may need
			      processing */
#ifdef DEBUG_WBUFFER
  if(YS__Simtime > DEBUG_TIME)
    fprintf(simout, " Write buffer %s has %d entries now\n",wbufptr->name, wbufptr->wbuf_sz);
#endif

  /* Allocate the new write-buffer entry and initialize all needed fields */
  newitem = (WBUFITEM *)malloc(sizeof(WBUFITEM));
  newitem->req = req; 
  newitem->counter = 0;
  newitem->next = NULL;
  newitem->tag = req->tag; /* set the tag of the write buffer entry */
  if(prev_req == NULL){
    /* First entry to the write buffer */
    wbufptr->write_buffer = newitem;
  }
  else
    prev_req->next = newitem;  /* Added to write buffer */
  return hittype;
}
