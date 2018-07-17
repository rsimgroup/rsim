/*
  directory.c

  Support for simulating the functionality of a directory. The function
  DirSim is the body of the module's simulation event. The directory is
  thought of as being merged with the memory (or, accessed in parallel).

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
#include "MemSys/req.h"
#include "MemSys/directory.h"
#include "MemSys/net.h"
#include "MemSys/module.h"
#include "MemSys/misc.h"
#include "MemSys/associate.h"
#include "MemSys/miss_type.h"
#include "Processor/memprocess.h"
#include "MemSys/cache.h"
#include "MemSys/bus.h"
#include "Processor/simio.h"

#include <malloc.h>
#include <string.h>

int dir_index=0;
DIR *dir_ptr[128];
extern PROCESSOR **cpu;
extern int YS__NumNodes; /* from mainsim.cc */

STATREC CoheNumInvlMeans;
STATREC BufTotSzMeans;

static int DirFind_FM(DIR *, Dirst *, int *, int, int *); /* builds
node_ary structure which lists the nodes that need to be sent COHE
messages */
static int DirAdd_FM(DIR *, Dirst *, int, REQ *);
static void DirRmAllAdd_FM(DIR *, Dirst *, int, REQ *);
static void DirRmAll_FM(DIR *, Dirst *, int);
struct DirQueue;
static REQ *DirQueueRm(struct DirQueue *);
static REQ *DirQueuePeek(struct DirQueue *);
static void DirQueueAdd(struct DirQueue *, REQ *);
static struct DirQueue *NewDirQueue();
static void DirAddBuf(DIR *, REQ *);
static void DirCheckBuf(DIR *, long);
static void FixForwardRARs(REQ *);
static void ReqSetHandled(REQ *, int, int); /* req, remote?, nocohe/
					                     localcohe/
							     remote-cohes */
static REQ *dir_commit_req(DIR *,int, int);



/*****************************************************************************/
/* DirSim: The body of the simulation event for the directory.  The          */
/* directory is responsible for maintaining the current state of a cache     */
/* line, serializing accesses to each line, generating and collecting        */
/* coherence messages, sending replies, and handling race-conditions and     */
/* network congestion.  In addition, the directory coherence protocol        */
/* used in RSIM relies on cache-to-cache transfers and uses replacement      */
/* hints. Coherence replies are collected at the directory, and in the       */
/* case of transfers that require coherence actions (other than              */
/* cache-to-cache transfers), the data is sent to the requestor only         */
/* after all coherence replies have been collected.                          */
/*****************************************************************************/

void DirSim()
{
  DIR    *dirptr;
  REQ    *req, *req1, *reqtmp;
  ARG    *argptr;
  int    case_num, return_st, oport_num, i;
  int    done = FALSE;
  double delay = 0.0;
  Dirst  *dir_item, copy;
  DirEP  *extra;
  int flag = 0;
  int loop_count = 0;
  int initial_state;
  int temp;
  int checkedOutbound;
  int rcohe;

  int owner_node;
  enum DirStates {DIRSTART, DIRSERV, DIRREQ, DIRCOHEREP, DIRWRB, DIROUTBOUND, DIRSENDREQ, DIRSENDCOHE, DIRSTARTOVER};
  
  argptr = (ARG *)ActivityGetArg(ME);
  dirptr = (DIR *)argptr->mptr; /* Pointer to dir module */
  req = dirptr->req;		/* Pointer to request being processed */
  
  case_num = initial_state = EventGetState();	/* Get next case to go to */

  if (initial_state == DIRSTART) /* if we start out in DIRSTART, then we'll
				    check outbound */
    checkedOutbound = 0;
  else
    checkedOutbound = 1;

#if defined(DEBUG_DIRECTORY)
  if (dirptr->bufsz_rdy < 0 || dirptr->buftotsz < 0 || dirptr->outboundsz < 0 || dirptr->wait_cntsz < 0)
    YS__errmsg("directory buffer size drops below zero");
#endif

  while(!done)
    {
      switch(case_num)
	{
	case DIRSTART: 
	  flag = 0;
	  req = NULL;
	  dirptr->req = NULL;
	  dirptr->wasPending = 0;

	  /* at this point do the following:
	     
	     If there is anything on OutboundReqs and it can be sent
	     out, pick it up and send it out. If not, recycle the
	     thing there.
	       
	     Otherwise, try to get something off any of the ports; go
	     through them in sequential order, starting with next_port
	     and wrapping around. 
	       
	     If the thing taken is a REQUEST or if there was nothing,
	     then check req_partial and ReadyReqs and take something
	     from there instead, if there is anything.  */
	     
	  if (dirptr->outboundsz && !checkedOutbound)
	    {
	      /* check the first entry in the outbound buffer and
		 see if we can send it */
	      req = DirQueueRm(dirptr->OutboundReqs); 
	      oport_num = dirptr->routing((SMMODULE *)dirptr,req);
	      checkedOutbound = 1;
	      if (checkQ(dirptr->out_port_ptr[oport_num]) == -1)
		{
		  /* can't send it out, so recycle it */
		  DirQueueAdd(dirptr->OutboundReqs,req);
		  req = NULL;
		}
	      else /* able to send it out */
		{
		  dirptr->outboundsz--;
		  dirptr->req = req;
		  case_num=DIROUTBOUND; /* Jump to state DIROUTBOUND */
		  break;
		}
	    }

	  if (dirptr->next_port != dirptr->reqport) 
	    {
	      /* For other ports, look at the port first -- COHE_REPLY
		 is the other input port (includes WRB/REPL) */
	      req = (REQ *)peekQ(dirptr->in_port_ptr[dirptr->next_port]);
	    }
	  else
	    {
	      /* When it's the REQUEST ports turn for being processed,
		 the directory should prefer to take the
		 partially-completed request over any new ones, and
		 then take any request that was pending but has now
		 finished (ReadyReqs). Only if these two are empty
		 should new REQUESTs be brought in. */
	      
	      if (dirptr->req_partial) /* partially completed request in flight? */
		{
		  req=dirptr->req_partial; /* take that instead */
		  dir_item=req->dir_item;
#ifdef DEBUG_DIRECTORY
		  if (DEBUG_TIME < YS__Simtime)
		    fprintf(simout,"0 Dir: %s Wokeup partially completed request\t&:%ld\tinst_tag:%d\tTAG:%ld\tTYP:%s\tStatus:%s\tNode_Req: %d @:%1.0f src=%d dest=%d DirState %d\n", 
			   dirptr->name, req->address, req->s.inst_tag,req->tag, Req_Type[req->req_type],
			   DirRtnStatus[return_st], req->src_node, YS__Simtime,req->src_node,
			   req->dest_node, dir_item->state);
#endif
		  /* in this case, request is already committed, and partially done */
		  case_num=DIRSENDCOHE; /* try to send out one more COHE for this access */
		  break; /* don't go through the delay step */
		}
	      else if (dirptr->bufsz_rdy) /* formerly pending request ready to
					     be reprocessed */
		{
		  req = DirQueuePeek(dirptr->ReadyReqs);
		  dirptr->wasPending = 1;
		}
	      else /* no req_partial, and no ReadyReqs to be reprocessed */
		{
		  /* try to bring in something from REQUEST input port */
		  req = (REQ *)peekQ(dirptr->in_port_ptr[dirptr->next_port]);
		  dirptr->wasPending = 0;
		}
	    }
	  if (!req)
	    {
	      /* No new request, so loop through the ports until all
		 have been checked */
	      case_num = DIRSTART;
	      if (++dirptr->next_port == dirptr->num_ports)
		dirptr->next_port = 0;
	      loop_count++;
	      if (loop_count==dirptr->num_ports) /* all have been checked */
		{
		  /* at this point, we'll go to sleep.

		     If we don't have anything on partial, ready reqs,
		     or outbound reqs, we'll sleep on INQ_EMPTY.

		     If we have a ready req, we'll go as soon as we
		     can.

		     If we do have something on partial or outbound,
		     that means that we need more out queue space, so
		     we'll sleep on QUEUE_EVENT, to handle both INQ
		     and OUTQ activity */
		  dirptr->Sim = (ACTIVITY *)YS__ActEvnt;
		  EventSetState(ME, DIRSTART);
		  if (dirptr->bufsz_rdy) /* we removed some pendings this time around */
		    {
		      /* set the directory to wake up as soon as possible
			 to process the newly removed pendings.
			 Users might not want to actually wait here, since
			 the request itself will have a normal delay when
			 it is reprocessed. */
		      ActivitySchedTime(ME,DIRCYCLE,INDEPENDENT);
		    }
		  else if (dirptr->req_partial || dirptr->outboundsz)
		    dirptr->state = WAIT_QUEUE_EVENT; /* note: don't wait on OUTQ_FULL, because the cache should always be woken up for new COHE_REPLYs, etc */
		  else
		    dirptr->state = WAIT_INQ_EMPTY;
		  done = TRUE;
		}
	      break;
	    }
	  else
	    {
#ifdef DEBUG_DIRECTORY
	      if (DEBUG_TIME < YS__Simtime)
		fprintf(simout,"0 Dir: %s\t%s\t&:%ld\tinst_tag:%d\tTAG:%ld\tTYP:%s\tNode_Req: %d @:%1.0f src=%d dest=%d\n", 
			Request_st[req->s.type],dirptr->name, req->address, req->s.inst_tag,req->tag, Req_Type[req->req_type],
			req->src_node, YS__Simtime,req->src_node,
			req->dest_node);
#endif
	      req->in_port_num = dirptr->next_port; /* store aside the input port number for the actual commit */
	      dirptr->req = req;
	      dirptr->Sim = NULL;
	      dirptr->state = PROCESSING;

	      delay = (double)(((struct DirDelays *)(dirptr->Delays))->init_tfr_time +
			       ((req->size_st/(dirptr->out_port_ptr[req->in_port_num]->width)) *
				(((struct DirDelays *)(dirptr->Delays))->flit_tfr_time)));
	      dirptr->start_time = YS__Simtime;

	      /* Now the directory should delay for the access latency.
		 This latency is calculated based on the width of the
		 directory port and the flit transfer time.
		 
		 Additionally, for all accesses except COHE_REPLYs
		 that do not access memory (simple acknowledgments
		 without copy-back), the directory must delay by the
		 DRAM memory latency. For COHE_REPLYs that do not
		 access memory, the directory only stalls for an
		 additional directory cycle.

		 */
	      
#ifndef COHEREPLYFULLWAIT
	      if (req->s.type == COHE_REPLY &&
		  (req->req_type == INVL || req->req_type == COPYBACK_INVL || req->req_type == COPYBACK_SHDY || (req->req_type == COPYBACK && req->size_st <= REQ_SZ)))
		{
		  /* in this case, we don't need to actually wait for
		     memory access, so we may have a shorter access time */
		  delay += DIRCYCLE;
		}
	      else
#endif
		/* add access time of module to delay */
		delay += (double)((struct DirDelays *)(dirptr->Delays))->access_time;

	      if (delay)
		{
		  /* if delay, reschedule event */
		  EventSetState(ME, DIRSERV);
		  ActivitySchedTime(ME, delay, INDEPENDENT);
		  done = TRUE;
		  break;
		}
	      case_num = DIRSERV;
	      break;
	    }

	case DIRSTARTOVER:
	  {
	    /* This state simply increments the round-robin port and
	       jumps back to DIRSTART. This will not loop forever no
	       matter what, because of "loop_count" in DIRSTART, which
	       stops after passing through all ports. */
	    if (++dirptr->next_port == dirptr->num_ports)
	      dirptr->next_port = 0;
	    case_num = DIRSTART;
	    if (MemsimStatOn)
	      {
		dirptr->utilization += YS__Simtime - dirptr->start_time;
	      }
	    req = dirptr->req = NULL;
	    break;
	  }
	case DIRSERV: /* the message has waited for its delay. Now it is
			 ready for action */
#ifdef DEBUG_DIRECTORY
	  if (YS__NumNodes != 1)
	    {
	      LookupAddrNode(req->address, &owner_node);
	      if (owner_node == NLISTED)
		{
		  YS__errmsg("Unlisted address at time of directory access.");
		}
	      if (owner_node != dirptr->node_num)
		{
		  YS__errmsg("Directory gets message for line not owned by it!\n");
		}
	    }
#endif

	  if (req->s.type == REQUEST)
	    {
	      dir_stat(dirptr, req, req->req_type);
	      case_num = DIRREQ; /* dispatch it to REQUEST case */
	    }
	  else if (req->s.type == COHE_REPLY && (req->req_type == WRB || (req->req_type == REPL))) 
	    case_num = DIRWRB; /* send these to WRB case */
	  else if (req->s.type == COHE_REPLY) /* Reply from network */
	    case_num = DIRCOHEREP; /* send to COHE-REPLY case */
	  else
	    YS__errmsg("Unknown request type");
	  break;
	case DIRREQ:
	  /* First, handle replies that are sent back by the caches (because
	     of too many outstanding Upgrades to the same set -- see l1cache.c
	     for discussion).
	     These have already been handled by the directory once; the
	     directory now just acts as a mirror to bounce the REQUEST off. */
	  if (req->s.preprocessed == 1) /* already been handled */
	    {
	      if (dirptr->wasPending)
		YS__errmsg("preprocessed request should not have been pending.");
#ifdef DEBUG_DIRECTORY
	      if (YS__Simtime > DEBUG_TIME)
		fprintf(simout,"%s Handling pre-processed request @ %g!\n",
			dirptr->name, YS__Simtime);
#endif
	      req->s.reply = REPLY; /* bounce it back as a REPLY */
	      req->s.type = REPLY;
	      oport_num = dirptr->routing((SMMODULE *)dirptr,req);
	      if (checkQ(dirptr->out_port_ptr[oport_num]) == -1)
		{
		  /* reply queue is full */
		  /* in this case, don't commit the request. Let the
		     dir handle some other access or just keep
		     retrying this one until a space comes up */
		  req->s.dir = REQ_FWD;
		  req->s.type = REQUEST;
		  case_num = DIRSTARTOVER;
		  break;
		}
	      else /* space is available to send out the REPLY */
		{
		  reqtmp = dir_commit_req(dirptr,req->in_port_num,dirptr->wasPending); /* guaranteed to commit. */
		  if (reqtmp != req)
		    YS__errmsg("Error when committing req in dir.");
		  req->s.dir = REQ_BWD;
		  case_num=DIRSENDREQ; /* jump to case for sending out */
		  break;
		}
	    }
	  else /* the REQUEST is not preprocessed */
	    {
	      if (!req->s.dirdone) /* Dir_Cohe has not yet been called */
		{
		  /* Call coherence routine (Dir_Cohe). Since the
		     coherence routine may require buffering of
		     things, make sure we have space in the buffers
		     before attempting this */
		  if (dirptr->buftotsz < dirptr->bufmaxsz || (dirptr->buftotsz == dirptr->bufmaxsz && dirptr->wasPending))
		    {
		      return_st = dirptr->cohe_rtn(dirptr, req, &dir_item, &copy, dirptr->extra);
		    }
		  else /* no buffers available */
		    {
		      /* we might not be able to complete the call of
			 the cohe_rtn, so we shouldn't try to do
			 so. At this point, try to RAR. If we cannot
			 RAR then don't commit the request. */
#ifdef DEBUG_DIRECTORY
		      if (YS__Simtime > DEBUG_TIME)
			{
			  fprintf(simout,"2 Dir:REQUEST\t%s\t\t&:%ld\tTAG:%ld\tTYP:%s\t%s\tNode_Req: %d @:%1.0f\n", 
				 dirptr->name, req->address, req->tag, Req_Type[req->req_type],
				 "BUF_FULL_RAR", req->src_node, YS__Simtime);
			}
#endif
		      req->s.reply = RAR;
		      req->s.type = REPLY;
		      
		      oport_num = dirptr->routing((SMMODULE *)dirptr, req);
		      if (checkQ(dirptr->out_port_ptr[oport_num]) == -1)
			{
			  /* convert back to request. don't commit. Just
			     retry the access from the input port later */
#ifdef DEBUG_DIRECTORY
			  if (YS__Simtime > DEBUG_TIME)
			    {
			      fprintf(simout,"BUF_FULL_RAR failed. Try again later.\n");
			    }
#endif
			  req->s.dir = REQ_FWD;
			  req->s.type = REQUEST; /* convert back to request */
			  case_num = DIRSTARTOVER;
			  break;
			}
		      else
			{
			  /* commit and send out the RAR */
			  reqtmp = dir_commit_req(dirptr,req->in_port_num,dirptr->wasPending);
			  if (reqtmp != req)
			    YS__errmsg("Error when committing req in dir.");
			  req->s.dir = REQ_BWD;
			  case_num = DIRSENDREQ; /* send it out */
			  break;
			}
		    }
		}
	      else /* the cohe function has already been done */
		{
		  /* this case is only used upon revival of some WAITFORWRBs */
		  return_st = DIR_REPLY;
		}
	    }

#ifdef DEBUG_DIRECTORY
	  if (DEBUG_TIME < YS__Simtime)
	    fprintf(simout,"2 Dir: REQUEST\t%s\t\t&:%ld\tinst_tag:%d\tTAG:%ld\tTYP:%s\tStatus:%s\tNode_Req: %d @:%1.0f src=%d dest=%d DirState %d\n", 
		   dirptr->name, req->address, req->s.inst_tag,req->tag, Req_Type[req->req_type],
		   DirRtnStatus[return_st], req->src_node, YS__Simtime,req->src_node,
		   req->dest_node, dir_item->state);
#endif

	  if (return_st == DIR_REPLY || return_st == VISIT_MEM)
	    {
	      /* This case occurs when we get a reference to a line
		 that doesn't require any COHEs to be sent out. */
	      ReqSetHandled(req,req->src_node != req->dest_node, 0);
	      /* note: this sets it only if it's never been set before
                 already. */
	      if (dir_item->extra) /* free up any possible extra state */
		{
		  YS__PoolReturnObj(&YS__DirEPPool, dir_item->extra);
		  dir_item->extra = NULL;
		}

	      /* note: reply type, etc has been set already in Dir_Cohe */
	      
	      req->s.reply = REPLY;
	      req->s.type = REPLY;
	      GlobalPerform(req);
	      req->size_st = req->size_req; /* set the size appropriately */
	      /* now, try to send out the REPLY */
	      oport_num = dirptr->routing((SMMODULE *)dirptr, req);
	      if (checkQ(dirptr->out_port_ptr[oport_num]) == -1) /* reply queue is full */
		{
		  /* in this case, don't commit the request. Let the
		     dir handle some other access or just keep
		     retrying this one until a space comes up */
		  
		  req->s.dir = REQ_FWD;
		  req->s.type = REQUEST;
		  case_num = DIRSTARTOVER;
		  break;
		}
	      else /* space available to send out the REPLY */
		{
		  reqtmp = dir_commit_req(dirptr,req->in_port_num,dirptr->wasPending);
		  if (reqtmp != req)
		    YS__errmsg("Error when committing req in dir.");
		  req->s.dir = REQ_BWD; 
		  
		  if (dirptr->wasPending) /* directory had a buf for this */
		    {
		      dirptr->buftotsz--;
#ifdef DEBUG_DIRECTORY
		      if (YS__Simtime > DEBUG_TIME)
			fprintf(simout,"%s: buftotsz now %d\n",dirptr->name,dirptr->buftotsz);
#endif
		    }
		      
		  case_num=DIRSENDREQ; /* send it out */
		  break;
		}
	    }
	  else if (return_st == WAITFORWRB)
	    {
	      /* REQUEST must wait for a WRB/REPL from its source node
		 to arrive here before it can be processed. In the meanwhile,
		 allow other REQUESTs/COHE-REPLYs to be processed. */
	      
	      /* This request has already been put in its waiting buffer,
		 so it can be committed. */
	      reqtmp = dir_commit_req(dirptr,req->in_port_num,dirptr->wasPending);
	      if (reqtmp != req)
		YS__errmsg("Error when committing req in dir.");
	      case_num = DIRSTARTOVER; /* start over */
	      break;
	    }
	  else if (return_st == WAIT_PEND)
	    {
	      /* REQUEST must wait because its line is in a transient
		 directory state, caused by outstanding COHE messages
		 or an outstanding WAITFORWRB */
	      
	      /* The request can be added to the module buffer --
                 there is definitely space (already checked for
                 it). So, this request can be committed */
	      reqtmp = dir_commit_req(dirptr,req->in_port_num,dirptr->wasPending);
	      if (reqtmp != req)
		YS__errmsg("Error when committing req in dir.");
	      DirAddBuf(dirptr, req); /* add it to pending buffer */
	      case_num = DIRSTARTOVER; /* start over */
	      break;
	    }
	  else if (return_st == WAIT_CNT)
	    {
	      /* Request requires new COHE messages to be sent out, the
		 Dir_Cohe function returns WAIT_CNT */
	      
	      /* The directory has already reserved a buffer entry for
		 this access, so the request can definitely be committed. */
	      reqtmp = dir_commit_req(dirptr,req->in_port_num,dirptr->wasPending);
	      if (reqtmp != req)
		YS__errmsg("Error when committing req in dir.");
	      rcohe=0; /* Is a remote COHE needed for this access? */
#ifdef DEBUG_DIRECTORY
	      if (DEBUG_TIME < YS__Simtime)
		fprintf(simout,"2 Dir: WAITCNT\tnum_invl: %d\tinst_tag:%d\ttag: %ld\tNodes_invl: ",
		       dirptr->extra->counter, req->s.inst_tag,req->tag);
	      if (DEBUG_TIME < YS__Simtime)
		for (i=0; i<dirptr->extra->counter; i++)
		  fprintf(simout,"Node:%d, ",dirptr->extra->node_ary[i]);
	      if (DEBUG_TIME < YS__Simtime)
		fprintf(simout," @:%1.0f\n", YS__Simtime);
#endif
	      if (MemsimStatOn)
		{
		  for (i=0; i<dirptr->extra->counter; i++)
		    {
		      if (dirptr->extra->node_ary[i] != dirptr->node_num) /* One of the COHEs must be remote */
			{
			  rcohe=1; /* set rcohe flag */
			  break;
			}
		    }
		}
	      ReqSetHandled(req,req->src_node!=dirptr->node_num,1+rcohe);
	      if (!dirptr->wasPending) /* if it was pending, directory already had a buffer for it */
		{
		  dirptr->buftotsz ++;	/* Size of directory buffer is updated;
					   request is not added to buffer
					   queue (that is only used for pend) */
#ifdef DEBUG_DIRECTORY
		  if (YS__Simtime > DEBUG_TIME)
		    fprintf(simout,"%s: buftotsz now %d\n",dirptr->name,dirptr->buftotsz);
#endif
		}
	      dirptr->wait_cntsz++;	/* wait cnt size */
	      if (MemsimStatOn)
		{
		  StatrecUpdate(dirptr->BufTotSzMeans, (double)dirptr->buftotsz, YS__Simtime);
		}
	      dir_item->pend = 1; /* Mark the line in pending state */
	      dir_item->extra = dirptr->extra; /* Get an extra structure */
	      dir_item->extra->pend_req = req; /* Save this REQ to send out
						  later */
	      req->dir_item = dir_item;
	      /* Set the number of COHEs remaining for this transaction */
	      dir_item->extra->num_left = dir_item->extra->counter; 
	      
	      dirptr->extra = (DirEP *)YS__PoolGetObj(&YS__DirEPPool); 
	      dirptr->extra->waiting = 0;     /* Initialize a new extra */
	      dirptr->extra->pend_req = NULL; /* structure for the directory */
	      dirptr->extra->counter = 0;
	      dirptr->extra->num_left = 0;
	      dirptr->extra->WaitingFor = -1;
	      dirptr->extra->WasHere = -1;
	      
	      if(MemsimStatOn)
		dirptr->num_miss++;	/* STATS */
	      req->start_time = GetSimTime();
	      if (dirptr->stat_level > 2 && MemsimStatOn)
		{
		  StatrecUpdate(dirptr->CoheNumInvlMeans, (double)dir_item->extra->counter, 1.0);
		  StatrecUpdate(CoheNumInvlHist, (double)dir_item->extra->counter, 1.0);
		}			

	      /* Directory must now stall for packet creation and can
		 then start sending out the cohe requests */

	      delay = ((struct DirDelays *)(dirptr->Delays))->pkt_create_time;
	      if (delay)
		{
		  EventSetState(ME, DIRSENDCOHE); /* jump to SENDCOHE, where
						     the system starts sending
						     out COHE messages */
		  /* Note: a pessimistic system would jump back to DIRSTART
		     with the hope of accepting other access-types. However,
		     that is pessimistic about being able to send out COHEs,
		     and it may be better in the usual case to be optimistic
		     here, since it's safe to do so. */

		  ActivitySchedTime(ME, delay, INDEPENDENT);
		  dirptr->req_partial = req;
		  done = TRUE;
		  break;
		}
	      else
		{
		  case_num=DIRSENDCOHE; /* jump to SENDCOHE immediately */
		  break;
		}
	    }
	  else if (return_st == FORWARD_REQUEST) /* cache-to-cache xfer request */
	    {
	      /* The buffer for this access has already been reserved,
		 so it can be committed now. */
	      reqtmp = dir_commit_req(dirptr,req->in_port_num,dirptr->wasPending);
	      if (reqtmp != req)
		YS__errmsg("Error when committing req in dir.");

#ifdef DEBUG_DIRECTORY
	      if (DEBUG_TIME < YS__Simtime)
		{
		  fprintf(simout,"%s: FORWARD_REQ addr=%ld src=%d forward_to=%d at %1.0f\n",
			 dirptr->name, req->address, req->src_node, dirptr->extra->node_ary[0],
			 YS__Simtime);
		}
#endif
	  
	      dir_item->pend = 1; /* mark the line pending */
	      req->dir_item = dir_item;

	      req->forward_to = req->src_node; /* set the forwarding destination */
	      req->src_node = dirptr->node_num; /* the forward starts from here */
	      req->dest_node = dirptr->extra->node_ary[0]; /* this is the owner */
	      req->req_type = dirptr->extra->next_req_type; /* as set in DirCohe; either COPYBACK or COPYBACK_INVL */

	      dir_item->extra = dirptr->extra; 
	      dir_item->extra->pend_req = req; /* Remember this in case it needs to be retried */
	      dir_item->extra->num_left = dir_item->extra->counter; /* Clear up buffer when this hits 0. */

	      if (!dirptr->wasPending) /* if it was pending, system has already counted a buffer for it */
		{
		  /* if it hadn't been pending, count the buffer for it now */
		  dirptr->buftotsz ++;	/* Size is updated; request is not
					   added to buffer queue */
#ifdef DEBUG_DIRECTORY
		  if (YS__Simtime > DEBUG_TIME)
		    fprintf(simout,"%s: buftotsz now %d\n",dirptr->name,dirptr->buftotsz);
#endif
		}
	      dirptr->wait_cntsz++;	/*** wait cnt size */
	      if (MemsimStatOn)
		{
		  StatrecUpdate(dirptr->BufTotSzMeans, (double)dirptr->buftotsz, YS__Simtime);
		}

	      dirptr->extra = (DirEP *)YS__PoolGetObj(&YS__DirEPPool);
	      dirptr->extra->waiting = 0;     /* allocate and initialize */
	      dirptr->extra->pend_req = NULL; /* a new extra structure   */
	      dirptr->extra->counter = 0;     /* for the directory       */
	      dirptr->extra->num_left = 0;
	      dirptr->extra->WaitingFor = -1;
	      dirptr->extra->WasHere = -1;

	      req->s.reply = 0;
	      req->s.type = COHE; /* send the $-$ demand as a COHE */
	      req->s.nack_st = dir_item->extra->nack_st;
	      /* nack_st identifies it as either a COPYBACK or an INVL --
		 will always be NACK_NOK (COPYBACK) for forwards */
	      req->s.route = ABV; /* prepare fields to send out request */
	      req->s.ret = RET;
	      req->s.dir = REQ_FWD; 
	      req->s.nc = 0;
	      req->s.dirdone = 0;
	      req->size_st = REQ_SZ; /* only forward request */

	      /* delay for packet creation time */
	      delay = ((struct DirDelays *)(dirptr->Delays))->pkt_create_time;
	      if (delay)
		{
		  EventSetState(ME, DIRSENDCOHE); /* jump to SENDCOHE,
						     where the system
						     starts sending
						     out COHE messages */
		  /* Note: a pessimistic system would jump back to DIRSTART
		     with the hope of accepting other access-types. However,
		     that is pessimistic about being able to send out COHEs,
		     and it may be better in the usual case to be optimistic
		     here, since it's safe to do so. */

		  ActivitySchedTime(ME, delay, INDEPENDENT);
		  dirptr->req_partial = req;
		  done = TRUE;
		  break;
		}
	      else
		{
		  case_num=DIRSENDCOHE; /* jump straight to case SENDCOHE */ 
		  break;
		}
	      /* end of additions */
	    }
	  else 
	    YS__errmsg("DirSim(): Unknown return state");
	  YS__errmsg("DirSim(): shouldn't fall through case statement");
	case DIROUTBOUND:
	  /* this is coming from OutboundReqs. It's already been checked
	     for space, so just add the message to its output port,
	     possibly free its buffer, and then continue processing */
	  oport_num = dirptr->routing((SMMODULE *)dirptr,req);
	  if (oport_num == -1)
	    {
	      YS__errmsg("DirSim(): Routing function is unable to route this request");
	    }
	  if (!add_req(dirptr->out_port_ptr[oport_num],req))
	    {
	      YS__errmsg("DirSim(): Failed to add request in DIROUTBOUND. Should have already been checked.\n");
	    }
	  checkedOutbound = 1; /* Don't check outbound twice on a single
				  pass */
	  case_num = DIRSTART; /* don't go to DIRSTARTOVER, as STARTOVER
				  changes the value of next_port. That's
				  not acceptable here. */
	  if (MemsimStatOn)
	    {
	      dirptr->utilization += YS__Simtime - dirptr->start_time;
	    }
	  if (req->s.type == REPLY &&
	      (req->s.reply == REPLY || req->s.reply == RAR))
	    {
	      /* in this case, decrement buftotsz. since this access
		 will no longer use anything */
	      dirptr->buftotsz--;	/* buffer no longer needed */
#ifdef DEBUG_DIRECTORY
	      if (YS__Simtime > DEBUG_TIME)
		fprintf(simout,"%s: buftotsz now %d\n",dirptr->name,dirptr->buftotsz);
#endif
	      if (MemsimStatOn)
		{
		  StatrecUpdate(dirptr->BufTotSzMeans, (double)dirptr->buftotsz, YS__Simtime);
		}
	    }
	  req = dirptr->req = NULL;
	  break;
	case DIRSENDREQ:
	  /* in this case, directory should have already made sure
	     that there is space to actually send the REQ out on its
	     output port. */
	  oport_num = dirptr->routing((SMMODULE *)dirptr,req);
	  if (oport_num == -1)
	    {
	      YS__errmsg("DirSim(): Routing function is unable to route this request");
	    }
	  if (!add_req(dirptr->out_port_ptr[oport_num],req))
	    {
	      YS__errmsg("DirSim(): Failed to add request in DIRSENDREQ. Should have already been checked.\n");
	    }
	  case_num = DIRSTARTOVER; /* send to DIRSTARTOVER so that
				      round-robin pointer (next_port)
				      advances */
	  break;
	case DIRSENDCOHE:
	  /* Directory can enter this case from two paths.
	     1: WAIT_CNT. Here req->s.type will be REQUEST, and this
	     state should send out COHEs based on the "extra" fields of "req".
	     2: FORWARD_REQUEST. Here req->s.type will be COHE, and this state
	     will send out this "req" itself */
	  if (req->s.type == COHE) /* came here from FORWARD_REQUEST */
	    {
	      /* Find out the output port number */
	      oport_num = dirptr->routing((SMMODULE *)dirptr, req);

	      /* Any space on the output port? */
	      if (checkQ(dirptr->out_port_ptr[oport_num]) == -1) /* COHE queue is full */
		{
		  /* Let the dir handle some other request or just keep
		     retrying this one until a space comes up */
		  dirptr->req_partial = req;
		  case_num = DIRSTARTOVER; /* let next_port be changed */
		  break;
		}
	      else /* space available, so send out "req" */
		{
		  dirptr->req_partial = NULL;
		  case_num = DIRSENDREQ;
		  break;
		}
	    }
	  else /* a REQUEST from WAIT_CNT */
	    {
	      /* Since this is a REQUEST, this state needs to figure
		 out how to send out the COHEs for this access . */

	      dir_item = req->dir_item;
      
	      dir_item->extra->num_left --;

	      /* Create and initialize a request for each cache's
		 coherence message */
	      req1 = (REQ *)YS__PoolGetObj(&YS__ReqPool);

	      /* Set up the fields for this COHE message */
	      req1->id = YS__idctr ++;
	      req1->priority = 0;
	      req1->address = req->address;
	      req1->src_node = dirptr->node_num;
	      req1->dest_node = dir_item->extra->node_ary[dir_item->extra->num_left];

	      /* The req_type, size_st, and size_req must be set up
		 properly now.

		 Currently, remote-writes are not supported in RSIM.
		 However, part of their handling code is stubbed out here.
		 All ordinary COHEs enter the "if" part of this condition,
		 but certain remote-writes enter the "else" */
		 
	      if (!((req->req_type == RWRITE) && ((req->s.rw_flags == 7)||(req->s.rw_flags == 6)||(req->s.rw_flags==15))))
		{
		  /* Ordinary COHEs enter here */
		  req1->req_type = dir_item->extra->next_req_type;
		  req1->size_st = dir_item->extra->size_st;
		  req1->size_req = dir_item->extra->size_req;
		}
	      else /* certain remote-write types: not currently supported */
		{
		  YS__warnmsg("Directory sending a synchronizing Push\n");
		  req1->s.rw_flags = req->s.rw_flags;
	
		  if (req->push_dest == -1) 
		    if (req1->dest_node == dirptr->node_num)
		      {
			req1->req_type = PUSH;
			req1->size_st = LINESZ + REQ_SZ;
			req1->size_req = REQ_SZ;
	    
		      }
		    else
		      {
			req1->req_type = dir_item->extra->next_req_type;
			req1->size_st = dir_item->extra->size_st;
			req1->size_req = dir_item->extra->size_req;
		      }
		  else
		    if (req1->dest_node == req->push_dest)
		      {		    
			req1->req_type = PUSH;
			req1->size_st = LINESZ + REQ_SZ;
			req1->size_req = REQ_SZ;
		      }
		    else
		      {
			req1->req_type = dir_item->extra->next_req_type;
			req1->size_st = dir_item->extra->size_st;
			req1->size_req = dir_item->extra->size_req;
		      }
		} /* end of synchronizing PUSH case */

	      /* Other fields for outgoing COHE transaction */
	      req1->address_type = req->address_type;
	      req1->dubref = req->dubref;
	      req1->s.reply = 0;
	      req1->s.dir = REQ_FWD;
	      req1->s.route = ABV;
	      req1->s.ret = RET;
	      req1->s.type = COHE;
	      req1->s.nack_st = dir_item->extra->nack_st; /* this identifies it as either a COPYBACK-type or an INVL */
	      req1->s.nc = 0;
	      req1->s.dirdone = 0;
	      req1->start_time = GetSimTime();
      
	      req1->tag = req->tag;
	      req1->cohe_type = req->cohe_type;
	      req1->allo_type = req->allo_type;
	      req1->linesz = req->linesz;
	      req1->dir_item = req->dir_item;
      
	      /* Get oport number from routing function to determine
	       if this COHE can be sent out now. */
	      oport_num = dirptr->routing((SMMODULE *)dirptr, req1);
	      if (oport_num == -1)
		{
		  YS__errmsg("DirSim(): Routing function is unable to route this coherence message");
		}
	      /* Is there any space on the output port? */
	      
	      if (checkQ(dirptr->out_port_ptr[oport_num]) == -1) /* no space on output port */
		{
#ifdef DEBUG_DIRECTORY
		  if (DEBUG_TIME < YS__Simtime)
		    {
		      fprintf(simout,"%s: can't send out COHE on tag %ld to processor %d at %1.0f\n",
			     dirptr->name, req1->tag, req1->dest_node, 
			     YS__Simtime);
		    }
#endif
		  YS__PoolReturnObj(&YS__ReqPool,req1); /* free the COHE since
							   it wasn't sent out
							   */
		  dirptr->req_partial = req; /* Keep holding onto this, as
						req still partially completed */
		  dir_item->extra->num_left++; /* didn't get to send this out */
		  case_num = DIRSTARTOVER; /* try to get some other
					      access-type from the input
					      ports */
		  break;
		}
	  
	      if (!add_req(dirptr->out_port_ptr[oport_num], req1))
		{
		  YS__errmsg("DirSim(): Must have reserved request queue; should not be full");
		}				

	      if (dir_item->extra->num_left) /* still more COHEs need to be
						sent out */
		{
		  /* stall for the additional packet creation time, rather than
		     the first one */
		  delay = ((struct DirDelays *)(dirptr->Delays))->addtl_pkt_crtime;
		  if (delay)
		    {
		      EventSetState(ME, DIRSENDCOHE);
		      /* As before, a pessimistic system would send this first
			 to DIRSTART. */
		  
		      ActivitySchedTime(ME, delay, INDEPENDENT);
		      dirptr->req_partial = req;
		      done = TRUE;
		      break;
		    }
		  else
		    {
		      case_num=DIRSENDCOHE; /* jump straight to DIRSENDCOHE */
		      break;
		    }
		}
	      else /* all COHEs sent out */
		{
		  dirptr->req_partial = NULL;
		  case_num = DIRSTARTOVER;
		  break;
		}
	      YS__errmsg("DIRECTORY: this is supposed to be unreachable code\n");
	    }
	case DIRCOHEREP:			/* COHERENCE REPLY */
	  /* note: COHE_REPLYs should be sunk _NO MATTER WHAT_. Directory
	     can always do the action associated with them; if some
	     condition is temporarily blocking some consequent
	     outbound message from going out, then just add that to
	     the outbound queue -- think of the outbound queue as a
	     "smart MSHR" list for the directory, since each possible
	     thing that could be sent out because of a COHE_REPLY has
	     already reserved a buffer before it could be processed. */
#ifdef DEBUG_DIRECTORY
	  if (DEBUG_TIME < YS__Simtime)
	    fprintf(simout,"5 Dir: COHE_REPLY\t%s\t\t&:%ld\tinst_tag:%d\tTAG:%ld\tTYP:%s\tinvl_left: %d\t%s @:%1.0f src=%d dest=%d\n",
		   dirptr->name, req->address, req->s.inst_tag,req->tag, Req_Type[req->req_type],
		   req->dir_item->extra->counter, Reply[req->s.reply], YS__Simtime,req->src_node,req->dest_node);
#endif
	  dir_item = req->dir_item;
	  if (!dir_item)
	    YS__errmsg("Coherence request must bring back the pointer to directory item");

	  /* Immediately commit the COHE_REPLY -- it is either a WRB/REPL
	     (which can not send out anything further), or a reply to
	     something that already has a buffer in the directory. */
	  reqtmp = commit_req((SMMODULE *)dirptr,req->in_port_num);
	  if (reqtmp != req)
	    YS__errmsg("Error when committing req in dir.");
	  
	  if ((!dir_item->extra || !dir_item->extra->counter)&&(req->s.reply!=NACK))
	    {
	      fprintf(simerr,"Dir_Cohe():Not Expecting a coherence reply for this line ; dir node: %d\n",
		      dirptr->node_num);
	      fprintf(simerr,"Requested tag: %ld\t Request type: %s\t Request src: %d\n",
		      req->tag, Req_Type[req->req_type], req->src_node);
	      YS__errmsg("Not Expecting a coherence reply for this line");
	    }
	  if (req->s.reply == NACK) /* negative ack (miss in remote cache) */
	    {
	      if (dir_item->extra->nack_st == NACK_NOK) /* If NACK is not OK */
		{
		  /* These are COPYBACK-type COHE messages. Such a message
		     would have been sent if the cache in question was though
		     to be the owner. */
		  if (dir_item->extra->WasHere != req->src_node) /* Has a WRB come in from this cache in the meanwhile? */
		    {
		      /* No WRB has come yet, so the request can't just
			 be reprocessed right away. */
#ifdef DEBUG_DIRECTORY
		      if (YS__Simtime > DEBUG_TIME)
			{
			  fprintf(simout,"NACK_NOK: tag=%ld from %d no WRB yet! at %1.0f\n",
				 req->tag,req->src_node,YS__Simtime);
			}
#endif
		      if (req != dir_item->extra->pend_req)
			{
			  /* if it was a NACK_NOK, the req sent should always
			     have been the same as the one kept away in the
			     pend_req field, because that was just for
			     restarting it in case it needed to be. */
			  YS__errmsg("NACK_NOK received with req != pend_req and no WRB yet");
			}
		      else
			{
			  /* This is what we call a CLASS 2 RAR. In
			     this case, a COHE_REPLY has been received
			     with a NACK, but this is to a NACK_NOK
			     line. In this case, we force a retry
			     because a WRB has not been received for
			     this line from the expected owner. We cannot
			     stall, as this might prevent us from ever
			     seeing the desired WRB. */

			  /* Set everything back so that caches will
			     recognize this as a retry request and will
			     resend the original request */
			  req->s.reply = RAR;
			  req->s.type = REPLY;
			  req->s.dir = REQ_BWD;

			  /* Set the state of the line back to the pre-race
			     condition: thought to be held in private state
			     by the cache in question */
			  DirRmAllAdd_FM(dirptr,dir_item,req->src_node,req);
			  dir_item->state = DIR_PRIVATE;
			  if (req->forward_to != -1)
			    {
			      /* Unset forward to and set the source node
				 back to the original sender */
			      req->src_node = req->forward_to;
			      req->forward_to = -1;
			      /* If it was a forward request, its req_type
				 has been changed from what the cache will
				 recognize. Fix this back to the original
				 using FixForwardRARs */
			      FixForwardRARs(req);
			    }
			  dir_item->pend = 0; /* line is no longer pending, since this access is being RAR'ed */
			  DirCheckBuf(dirptr,req->tag); /* free up accesses pending on this line */
			  YS__PoolReturnObj(&YS__DirEPPool,dir_item->extra); /* free up the extra info */
			  dir_item->extra = NULL;

			  /* Try to send out the RAR */
			  oport_num = dirptr->routing((SMMODULE *)dirptr, req); 
			  if (checkQ(dirptr->out_port_ptr[oport_num])==-1) /* REPLY port full */
			    {
			      /* in this case, add it to queue of outbound
				 requests -- this is ok since this
				 RAR is still holding on to its buffer */
#ifdef DEBUG_DIRECTORY
			      if (YS__Simtime > DEBUG_TIME)
				fprintf(simout, "%s DIRECTORY could not send CLASS2 RAR tag %ld - %g\n", dirptr->name,
					req->tag, YS__Simtime);
#endif
			      dirptr->outboundsz++;
			      DirQueueAdd(dirptr->OutboundReqs,req);
			      case_num = DIRSTARTOVER;
			      break;
			    }
			  else /* space available for sending */
			    {
#ifdef DEBUG_DIRECTORY
			      if (YS__Simtime > DEBUG_TIME)
				fprintf(simout, "%s DIRECTORY SENDING CLASS2 RAR tag %ld - %g\n", dirptr->name,
					req->tag, YS__Simtime);
#endif
			      /* Free the buffer for this RAR, send it will be sent out */
			      dirptr->buftotsz--;
#ifdef DEBUG_DIRECTORY
			      if (YS__Simtime > DEBUG_TIME)
				fprintf(simout,"%s: buftotsz now %d\n",dirptr->name,dirptr->buftotsz);
#endif
			      case_num = DIRSENDREQ; /* send the request out */
			      break;
			    }
			}
		      /* this code should not be reached */
		      YS__errmsg("unknown case in directory NACK_NOK");
		      break;
		    }
		  else /* a WRB was received in the meanwhile */
		    {
#ifdef DEBUG_DIRECTORY
		      if (YS__Simtime > DEBUG_TIME)
			{
			  fprintf(simout,"NACK_NOK: tag=%ld from %d WRB received! at %1.0f\n",
				 req->tag,req->src_node,YS__Simtime);
			}
#endif
		      /* Since a WRB was received, the original REQUEST
			 should be reprocessed. Recover the old REQUEST
			 state and add it to the queue of ReadyReqs (as
			 though it had been pending) */

		      /* Clear out the WRB information */
		      dir_item->extra->WasHere = -1;
		      if (req->forward_to != -1)
			{
			  req->dir_item->pend = 0; /* clear out the pending bit */
			  /* Turn this back into a proper REQUEST */
			  req->s.reply = 0;
			  req->s.dirdone = 0;
			  req->s.dir = REQ_FWD;
			  req->s.type = REQUEST;

			  /* Remove the forwarding information */
			  req->src_node = req->forward_to;
			  req->forward_to = -1;
			  /* Recover the original req_type */
			  FixForwardRARs(req);

			  /* Free up the extra info */
			  YS__PoolReturnObj(&YS__DirEPPool, dir_item->extra);
			  dir_item->extra = NULL;

			  /* Put this access in the ready queue to be
			     reprocessed */
			  dirptr->bufsz_rdy++;
			  DirQueueAdd(dirptr->ReadyReqs,req);
			  
			  /* Revive any other requests that might have been
			     pending for the same line */
			  DirCheckBuf(dirptr, req->tag);
			  
			  case_num = DIRSTARTOVER;
			  break; 
			}
		      else
			{
			  YS__errmsg("How does a non-forward have a NACK_NOK?\n");
			}
		    }
		}
	    }
	  else if (req->s.reply == NACK_PEND)
	    {
#ifdef DEBUG_DIRECTORY
	      if(YS__Simtime > DEBUG_TIME)
		fprintf(simout," %g %s handling NACK_PEND Tag:%ld inst tag: %d\n",
			YS__Simtime, dirptr->name, req->tag, req->s.inst_tag);
#endif
	      if (dir_item->extra->nack_st != NACK_NOK)
		{
		  YS__errmsg("Why did we get a NACK_OK NACK_PENDING\n");
		}
	      /* This is basically sent in the case when the cache is
		 stuck on something that it cannot handle. The directory
		 should see if it can handle this, otherwise it should
		 just send it back to the cache */
	      if(dir_item->extra->WasHere == req->src_node) /* has there been a WRB from the cache in question? */
		{
		  /* This is the case when the cache responded with a
		     NACK_PEND saying that it did not have the line
		     plus it had a pending mshr for that line -- This
		     can occur if our cohe message had gone there
		     before the replacement came here (in which case
		     we have to resend this request back, just as in
		     NACK_NOK above) */
#ifdef DEBUG_DIRECTORY
		  if (YS__Simtime > DEBUG_TIME)
		    {
		      fprintf(simout,"%s NACK_PEND: tag=%ld from %d WRB received! at %1.0f\n",dirptr->name,
			     req->tag,req->src_node,YS__Simtime);
		    }
#endif
		  dir_item->extra->WasHere = -1;
		  if (req->forward_to != -1)
		    {
		      /* in this case, reprocess the request, just
		       as in NACK_NOK above. */
		      ((Dirst*)req->dir_item)->pend = 0;  /* clear out the pending bit */
		      /* Turn this back into a proper REQUEST */
		      req->s.reply = 0;
		      req->s.dirdone = 0;
		      req->s.dir = REQ_FWD;
		      req->s.type = REQUEST;

		      /* Remove the forwarding information */
		      req->src_node = req->forward_to;
		      req->forward_to = -1;
		      
		      /* Recover the original req_type */
		      FixForwardRARs(req);
		      
		      /* Free up the extra info */
		      YS__PoolReturnObj(&YS__DirEPPool, ((Dirst *) dir_item)->extra);	
		      ((Dirst *) dir_item)->extra = NULL;

		      /* Put this access in the ready queue to be
			 reprocessed */
		      dirptr->bufsz_rdy++;
		      DirQueueAdd(dirptr->ReadyReqs,req);

		      /* Revive any other requests that might have been
			 pending for the same line */
		      DirCheckBuf(dirptr, req->tag);
			  
		      case_num = DIRSTARTOVER;
		      break; 
		    }
		  else
		    {
		      YS__errmsg("How does a non-forward have a NACK_PEND?\n");
		    }
		}
	      else /* there has not been a WRB from that cache. So, the COHE
		      sent there must have just passed the REPLY. Retry
		      the COHE under that assumption */
		{
		  /* Resend the COHE to its target (owner line) */
		  /* Restore the COHE information */
		  req->s.dir = REQ_FWD; 
		  req->s.type = COHE;

		  /* Swap source and destination (for proper routing with
		     REQ_FWD */
		  temp = req->src_node;
		  req->src_node = req->dest_node;
		  req->dest_node = temp;

		  /* Try to send the COHE out */
		  oport_num = dirptr->routing((SMMODULE *)dirptr, req); 
		  if (checkQ(dirptr->out_port_ptr[oport_num])==-1) /* COHE port full */
		    {
		      /* in this case, add it to queue of outbound requests */
#ifdef DEBUG_DIRECTORY
		      if (YS__Simtime > DEBUG_TIME)
			fprintf(simout, "%s retrying NACK-PEND on tag %ld - %g\n", dirptr->name,req->tag, YS__Simtime);
#endif
		      dirptr->outboundsz++;
		      DirQueueAdd(dirptr->OutboundReqs,req);
		      case_num = DIRSTARTOVER;
		      break;
		    }
		  else /* there is space available to send out COHE */
		    {
#ifdef DEBUG_DIRECTORY
		      if (YS__Simtime > DEBUG_TIME)
			fprintf(simout, "%s redoing NACK-PEND on tag %ld - %g\n", dirptr->name,req->tag, YS__Simtime);
#endif
		      case_num = DIRSENDREQ; /* send out req */
		      break;
		    }
		}
	    }
	  else if (req->s.reply != REPLY)
	    YS__errmsg("Unknown reply type");

	  if ((req->forward_to != -1)&&(dir_item->extra->ret_st != ACK_REPLY))
	    {
	      YS__errmsg("How did we have a forward to that wasn't set to ACK_REPLY");
	    }

	  /* From this point on, we're dealing with normal (acceptable)
             replies.  We have to be able to sink these in all cases */
	  
	  dir_item->extra->counter --; /* one more COHE_REPLY received */

	  if (dir_item->extra->ret_st != ACK_REPLY) /* the COHE_REPLY sent in
						       is only used further in
						       $-$ case (ACK_REPLY) */
	    {
	      YS__PoolReturnObj(&YS__ReqPool, req);
	    }
	  
	  if (dir_item->extra->counter == 0) /* all COHE_REPLYs have returned */
	    {
	      /* Note: don't do buf_totsz-- until REPLY is about to be sent out */
	      dirptr->wait_cntsz --;	/*** decrement wait cnt size */
	      if (dir_item->extra->ret_st != ACK_REPLY) /* DIR_REPLY, VISIT_MEM, or SPECIAL_REPLY */
		req = dir_item->extra->pend_req; /* revive the pending req */
      
	      if (MemsimStatOn)
		{
		  dirptr->latency += GetSimTime() - req->start_time; /* stats */
		}
	      
	      if (dir_item->extra->ret_st == DIR_REPLY || dir_item->extra->ret_st == VISIT_MEM)
		{
		  /* First, revive any reqs that may be pending to the
		     same line, as this item is no longer pending */
		  DirCheckBuf(dirptr, req->tag); 
		  dir_item->pend = 0; /* clear pending bit */

		  /* Free up the "extra" information */
		  YS__PoolReturnObj(&YS__DirEPPool, ((Dirst *) req->dir_item)->extra);	
		  ((Dirst *) req->dir_item)->extra = NULL;

		  /* Set up all the needed REPLY fields */
		  req->s.reply = REPLY;
		  req->s.type = REPLY;
		  req->s.dir = REQ_BWD;
		  GlobalPerform(req);

		  /* Set the size of the REPLY according to the size
		     originally sought by the REQUEST */
		  req->size_st = req->size_req;

		  /* Try to send out the reply */
		  oport_num = dirptr->routing((SMMODULE *)dirptr, req); 
		  if (checkQ(dirptr->out_port_ptr[oport_num])==-1) /* REPLY port full */
		    {
		      /* in this case, add it to queue of outbound
			 requests -- don't free buffer until REPLY
			 finally sent out */
		      
#ifdef DEBUG_DIRECTORY
		      if (YS__Simtime > DEBUG_TIME)
			fprintf(simout, "%s DIR_REPLY reply queue full on tag %ld - %g\n", dirptr->name,req->tag, YS__Simtime);
#endif
		      dirptr->outboundsz++;
		      DirQueueAdd(dirptr->OutboundReqs,req);
		      case_num = DIRSTARTOVER; /* start over with next port */
		      break;
		    }
		  /* otherwise, the REPLY can be sent out immediately */
		  dirptr->buftotsz--;	/* buffer no longer needed, so free it */
#ifdef DEBUG_DIRECTORY
		  if (YS__Simtime > DEBUG_TIME)
		    fprintf(simout,"%s: buftotsz now %d\n",dirptr->name,dirptr->buftotsz);
#endif
		  if (MemsimStatOn)
		    {
		      StatrecUpdate(dirptr->BufTotSzMeans, (double)dirptr->buftotsz, YS__Simtime);
		    }
	      
		  case_num = DIRSENDREQ; /* send the req out */
		  break;	 
		}
	      else if (dir_item->extra->ret_st  == ACK_REPLY) /* $-$ case */
		{
		  /* this is the acknowledgment (in the case of COPYBACK_INVL)
		     or COPYBACK from the cache-to-cache xfer */
		  if (req != dir_item->extra->pend_req) /* these should not be equal, as the original req is sent as the xfer itself, not the ack */
		    {
		      /* this is the normal case */
		      
		      /* Revive any pending requests to the same line */
		      DirCheckBuf(dirptr, req->tag);
		      dir_item->pend = 0; /* line is no longer pending */

		      /* free up the "extra" info */
		      YS__PoolReturnObj(&YS__DirEPPool, ((Dirst *) req->dir_item)->extra);
		      dir_item->extra = NULL;
		      dirptr->buftotsz--;	/* buffer no longer needed, as no more messages sent from this case */
#ifdef DEBUG_DIRECTORY
		      if (YS__Simtime > DEBUG_TIME)
			fprintf(simout,"%s: buftotsz now %d\n",dirptr->name,dirptr->buftotsz);
#endif
		      if (MemsimStatOn)
			{
			  StatrecUpdate(dirptr->BufTotSzMeans, (double)dirptr->buftotsz, YS__Simtime);
			}
	      
		      YS__PoolReturnObj(&YS__ReqPool, req);
		      case_num = DIRSTARTOVER; /* start over with next port */
		      break;
		    }
		  else
		    {
		      YS__errmsg("Forward acknowledged, but was neither satisfied nor NACKed.");
		    }
		}
	      else if (dir_item->extra->ret_st == SPECIAL_REPLY)
		{
		  /* First, revive any reqs that may be pending to the
		     same line, as this item is no longer pending */
		  DirCheckBuf(dirptr, req->tag);
		  dir_item->pend = 0; /* clear pending bit */
		  
		  /* Free up the "extra" information */
		  YS__PoolReturnObj(&YS__DirEPPool, ((Dirst *) req->dir_item)->extra);	
		  ((Dirst *) req->dir_item)->extra = NULL;
		  
		  /* Set up all the needed REPLY fields */
		  req->s.reply = REPLY;
		  req->s.type = REPLY;
		  req->s.dir = REQ_BWD;
		  GlobalPerform(req);
		  /* Set the size of the REPLY according to the size
		     originally sought by the REQUEST */
		  req->size_st = req->size_req;

		  /* Try to send out the reply */
		  oport_num = dirptr->routing((SMMODULE *)dirptr, req); 
		  if (checkQ(dirptr->out_port_ptr[oport_num])==-1)  /* REPLY port full */
		    {
		      /* in this case, add it to queue of outbound
			 requests -- don't free buffer until REPLY
			 finally sent out */
#ifdef DEBUG_DIRECTORY
		      if (YS__Simtime > DEBUG_TIME)
			fprintf(simout, "%s SPECIAL_REPLY reply queue full on tag %ld - %g\n", dirptr->name,req->tag, YS__Simtime);
#endif
		      dirptr->outboundsz++;
		      DirQueueAdd(dirptr->OutboundReqs,req);
		      case_num = DIRSTARTOVER;
		      break;
		    }
		  /* otherwise, the REPLY can be sent out immediately */
		  dirptr->buftotsz--;	/* buffer no longer needed, so free it */
#ifdef DEBUG_DIRECTORY
		  if (YS__Simtime > DEBUG_TIME)
		    fprintf(simout,"%s: buftotsz now %d\n",dirptr->name,dirptr->buftotsz);
#endif
		  if (MemsimStatOn)
		    {
		      StatrecUpdate(dirptr->BufTotSzMeans, (double)dirptr->buftotsz, YS__Simtime);
		    }
	      
		  case_num = DIRSENDREQ;  /* send the req out */
		  break;	 
		}     
	      else
		YS__errmsg("Unknown return state");
	    }
	  else /* there are still more COHEs that need to finish */
	    {
	      case_num = DIRSTARTOVER;
	      break;
	    }
	case DIRWRB: /* a WRB or REPL message */
	  /* Dir_Cohe should be called in order to determine what changes
	     this WRB has on directory line state, pending requests, etc. */

	  /* Definitely commit the request, since this will not take up
	     any new resources or send out any new messages */
	  reqtmp = commit_req((SMMODULE *)dirptr,req->in_port_num);
	  if (reqtmp != req)
	    YS__errmsg("Error when committing req in dir.");

	  /* call Dir_Cohe to set line state and see if line has an extra */
	  return_st = dirptr->cohe_rtn(dirptr, req, &dir_item, &copy, dirptr->extra);
	  if (dir_item->extra) /* extra information present -- accesses in flight */
	    {
	      extra = ((Dirst *) dir_item)->extra;
	      if (extra->pend_req == req)
		{
		  YS__errmsg("how can pend req be req for a WRB?");
		}
	      else
		{
		  if (extra->waiting) /* is the pending request waiting for WRB? */
		    {
#ifdef DEBUG_DIRECTORY
		      if (YS__Simtime > DEBUG_TIME)
			fprintf(simout, " Writeback to tag %ld now allows %d to proceed %g\n",
				req->tag, extra->pend_req->s.inst_tag, YS__Simtime);
#endif
		      /* mark this line non-pending and make it clear that
			 it is no longer waiting for WRB */
		      ((Dirst *) dir_item)->pend = 0;
		      extra->waiting = 0;
		      extra->WaitingFor = -1;

		      /* Free up the extra info */
		      YS__PoolReturnObj(&YS__DirEPPool, extra);	
		      ((Dirst *) dir_item)->extra = NULL;

		      /* Revive pending requests */
		      DirCheckBuf(dirptr, req->tag);
		    }
		  else /* nothing actually waiting for WRB */
		    {
		      /* in this case, just a cache to cache (or
			 similar) must be in flight, so this WRB is a
			 surprise to the directory.  No need to do
			 anything special about it, since that will
			 still be pending. However, we need to
			 remember to clear out the pendings when the
			 NACK or NACK_PEND arrives. DirCohe has already
			 set the "WasHere" field of the extra information
			 so that the NACK or NACK_PEND will realize that
			 the WRB has come. */
#ifdef DEBUG_DIRECTORY
		      if (YS__Simtime > DEBUG_TIME)
			fprintf(simerr, " %s WRB/REPL to tag %ld while line pending but nobody waiting %g\n",
				dirptr->name, req->tag, YS__Simtime);
#endif
		      
		    }
		}
	    }
	  else /* there is nothing inflight for an extra, but something may have been pending? */
	    {
	      
#ifdef DEBUG_DIRECTORY
	      if (YS__Simtime > DEBUG_TIME)
		fprintf(simout, " Tag %ld relieved from pending without extras %g\n",
			req->tag, YS__Simtime);
#endif
	      ((Dirst *) dir_item)->pend = 0; /* clear pending bit */
	      DirCheckBuf(dirptr, req->tag); /* Check to revive pending
						requests to this line */
	    }
	  YS__PoolReturnObj(&YS__ReqPool,req); /* free up the WRB */
	  case_num=DIRSTARTOVER; /* start over with next_port */
	  break;
	default:
	  YS__errmsg("Unknown case in directory.");
	  break;
	}
    }
}

/*****************************************************************************/
/* Dir_Cohe maintains the coherence protocol of the system. The directory    */
/* considers lines as being in one of 3 states: PRIVATE, SHARED, UNCHACHED.  */
/* The caches themselves consider a MESI or MSI protocol based on the        */
/* directory responses (MESI and MSI responses are supported).               */
/*****************************************************************************/

int Dir_Cohe(DIR *dirptr, REQ *req, Dirst **dir_item, Dirst *copy, DirEP *extra)
{
  /* The states a directory line can be in are
     UNCACHED, PRIVATE, SHARED -- this is because the directory
     never knows when a PR_CL becomes a PR_DY.

     Adding SH_DY might require another type "OWNED" or
     something like that.

     The states that shape decisions are
     UNCACHED, PRIVATE_SELF, PRIVATE_OTHER,
     SHARED_SELF (only self), SHARED_OTHER (only others),
     SHARED_BOTH (both self and others) */

  /* This Dir_Cohe function assumes ReplacementHintsLevel == EXCL */

  int find;
  int node_copy;
  long    tag = req->tag;
  int     req_type;
  int     node_index = req->src_node;
  REQ  *tempreq;
  unsigned oursalone;

    
  if (dirptr->dir_type != CNTRL_FULL_MAP )
    YS__errmsg("Dir_Cohe(): This routine supports only CNTL_FULL_MAP directory");
    
  *dir_item = DirHashLookup_FM(dirptr, tag);
#ifdef DEBUG_DIRECTORY
  if (DEBUG_TIME < YS__Simtime) 
    if (req->req_type == WRB || req->req_type == REPL)
      fprintf(simout,"dir_cohe(): %s from %d tag=%ld received at %1.0f\n",
	     Req_Type[req->req_type],req->src_node,req->tag,YS__Simtime);
#endif

  if ((*dir_item)->pend) /* is line pending */
    {
      if ((*dir_item)->extra && (req->req_type == WRB || req->req_type == REPL))
	/* are there accesses inflight when incoming access is a WRB or REPL? */
	{
	  /* If so, check to see if anything is in WAITFORWRB mode */
	  if ((*dir_item)->extra->waiting) /* WAITFORWRB? */
	    {
	      if ((*dir_item)->extra->WaitingFor == req->src_node) /* If so, check to make sure that this WRB is from the expected node */
		{
#ifdef DEBUG_DIRECTORY
		  if (YS__Simtime > DEBUG_TIME)
		    fprintf(simout,"dir_cohe(): %s from %d tag=%ld received at %1.0f waiting for me!!\n",
			   Req_Type[req->req_type],req->src_node,req->tag,YS__Simtime);
#endif
		  (*dir_item)->extra->WaitingFor = -1; /* No longer waiting */
		  (*dir_item)->extra->counter --;
		  if ((*dir_item)->extra->counter != 0) 
		    YS__errmsg("Dir_Cohe(): counter should have been 1, not higher");
		  /* Hold onto the buffer in order to reprocess the revived
		     REQUEST */

#ifdef DEBUG_DIRECTORY
		  if (YS__Simtime > DEBUG_TIME)
		    fprintf(simout,"%s: buftotsz now %d\n",dirptr->name,dirptr->buftotsz);
#endif
		  dirptr->wait_cntsz --; /*** wait cnt size */
		  tempreq = (*dir_item)->extra->pend_req; /* Look at original REQUEST */

		  /* If the original request was from the same source
		     node as the WRB/REPL, then let it be reprocessed
		     normally.  Otherwise, consider it a "dirdone" and
		     don't make it call Dir_Cohe -- the latter should
		     not occur currently, but is reserved for future
		     expansion with remote-writes */
		     
		  if(req->src_node == tempreq->src_node)
		    {
		      tempreq->s.dirdone = 0;
		    }
		  else
		    {
		      tempreq->s.dirdone = 1;
		    }
		    
		  if (tempreq->forward_to != -1)
		    {
		      /* Set the src_node of the tempreq back
			 correctly, and make it a proper REQUEST
			 again. This whole case does not occur with
			 the current RSIM, but is reserved for future
			 expansion. */
		      tempreq->src_node = tempreq->forward_to;
		      tempreq->s.reply = 0;
		      tempreq->s.type = REQUEST;
		      tempreq->s.dirdone = 0; /* since we need to set
						 the line state
						 correctly.  It is
						 UNCACHED now! */
		      tempreq->s.dir = REQ_FWD;
		    }
		  /* Send this through to ReadyReqs to be reprocessed: keep
		     holding the buffer, though. */
		  dirptr->bufsz_rdy ++;
		  DirQueueAdd(dirptr->ReadyReqs,tempreq);
		}
	      else /* Waiting for some other message? Not allowed. */
		{
		  fprintf(simerr,"address: %ld, tag %ld, req_node: %d, dir node: %d Waiting for: %d\n",
			  req->address, req->tag, req->src_node, dirptr->node_num, 
			  (*dir_item)->extra->WaitingFor);
		  YS__errmsg("This request is WRB/REPL; Pending request should not be waiting for some other message");
		}
	    }
	  else /* not a WAITFORWRB, but this WRB/REPL may influence how
		  the directory goes on to handle a NACK or NACK_PEND
		  for this line */
	    {
#ifdef DEBUG_DIRECTORY
	      if (DEBUG_TIME < YS__Simtime) 
		fprintf(simout,"dir_cohe(): %s from %d tag=%ld received at %1.0f not waiting yet!\n",
		       Req_Type[req->req_type],
		       req->src_node,req->tag,YS__Simtime);
#endif
	      /* Mark down that a WRB came for this line */
	      (*dir_item)->extra->WasHere = req->src_node;
	    }
	}
      else /* other types of pending REQUESTS */
	return WAIT_PEND; /* this access also needs to keep waiting for 
			     previous op */
    }

  *copy = **dir_item;
  req_type = req->req_type;
  switch(req_type)
    {
    case READ_SH:
      if ((*dir_item)->state == UNCACHED) /* line is currently uncached */
	{
	  DirAdd_FM(dirptr, *dir_item, node_index,req); /* add a sharer */
#ifdef DEBUG_DIRECTORY
	  if (YS__Simtime > DEBUG_TIME)
	    fprintf(simout,">>>>>>>>>>%s: READ_SH from %d tag %ld state UNCACHED new:%x %x \n",dirptr->name,
		   req->src_node,req->tag,(*dir_item)->vector[1],(*dir_item)->vector[0]);
#endif
	  extra->counter = 0; /* no COHEs needed */
	  /* return as PRIVATE if MESI; SHARED if MSI */
	  if (CCProtocol != MSI)
	    {
	      (*dir_item)->state = DIR_PRIVATE;
	      req->req_type = REPLY_EXCL;
	    }
	  else /* MSI */
	    {
	      (*dir_item)->state = DIR_SHARED;
	      req->req_type = REPLY_SH;
	    }
	  return VISIT_MEM; 
	}
      else if ((*dir_item)->state == DIR_SHARED) /* add a sharer */
	{
	  oursalone = DirAdd_FM(dirptr, *dir_item, node_index,req);
#ifdef DEBUG_DIRECTORY
	  if (YS__Simtime > DEBUG_TIME)
	    fprintf(simout,">>>>>>>>>>%s: READ_SH from %d tag %ld state DIR_SHARED new:%x %x\n",dirptr->name,
		   req->src_node,req->tag,(*dir_item)->vector[1],(*dir_item)->vector[0]);
#endif
	  extra->counter = 0; /* no COHEs needed */
	  
	  /* return as SHARED _unless_ the only sharer currently listed
	     is this processor itself (which must have subsequently evicted
	     the line without telling the directory) and system uses MESI.
	     In the latter case, return as PRIVATE */
	  
	  if (oursalone && CCProtocol != MSI)
	    {
	      (*dir_item)->state = DIR_PRIVATE;
	      req->req_type = REPLY_EXCL;
	    }
	  else /* some other cache has it, or the protocol is MSI */
	    {
	      /* keep state DIR_SHARED */
	      req->req_type = REPLY_SH;
	    }
	  
	  return VISIT_MEM;
	}
      else if ((*dir_item)->state == DIR_PRIVATE) /* some cache owns it */
	{
	  /* If the owner is some other cache, send a $-$ downgrade (COPYBACK)
	     to that cache so that both processors end up with the line
	     in shared state, and the directory will be given a copy of
	     the line (so the directory has it clean). That will be
	     a FORWARD_REQUEST

	     If the owner is the sender cache itself, then the cache
	     must have replaced it and immediately sent a REQUEST. In
	     that case, wait for that replacement to come in to the
	     directory (WAITFORWRB).
	     */
	  
#ifdef DEBUG_DIRECTORY
	  if (YS__Simtime > DEBUG_TIME)
	    fprintf(simout,">>>>>>>>>>%s: READ_SH from %d tag %ld state DIR_PRIVATE was:%x %x\n",dirptr->name,
		   req->src_node,req->tag,(*dir_item)->vector[1],(*dir_item)->vector[0]);
#endif
	  find = DirFind_FM(dirptr, *dir_item, extra->node_ary, node_index, &node_copy);
	  if (find != 1)
	    {
	      fprintf(simerr,"Dir_Cohe(): State is DIR_PRIVATE; find must be 1; dir node: %d\n",
		      dirptr->node_num);
	      fprintf(simerr,"Requested tag: %ld\t Request type: %s\t Request src: %d\n",
		      req->tag, Req_Type[req->req_type], req->src_node);
	      YS__errmsg("Dir_Cohe(): State is DIR_PRIVATE; find must be 1");
	    }
	  if (node_copy)
	    {
#ifdef DEBUG_DIRECTORY
	      if (YS__Simtime > DEBUG_TIME)
		fprintf(simout,"Dir_Cohe(): write-back/replacement overtaken by read tag %ld at %1.0f\n",
			req->tag, YS__Simtime);
#endif
	      /* The WRB/REPL was overtaken by the READ_SH. Must wait for the
		 WriteBack before processing this request, so set the
		 variables for this condition */
	      extra->counter = 1;
	      extra->WaitingFor = req->src_node; /* Must wait for a WRB from this same node */
	      extra->waiting = 1;
	      extra->pend_req = req; /* this REQUEST is the one to be revived */
	      (*dir_item)->pend = 1; /* mark line in pending state */
	      dirptr->wait_cntsz++;
	      if (!dirptr->wasPending) /* if it already was pending, we already have a buffer for it */
		{
		  /* otherwise, add a new buffer -- was already reserved
		     before calling DirCohe */
		  dirptr->buftotsz++;
#ifdef DEBUG_DIRECTORY
		  if (YS__Simtime > DEBUG_TIME)
		    fprintf(simout,"%s: buftotsz now %d\n",dirptr->name,dirptr->buftotsz);
#endif
		}
	      (*dir_item)->extra = extra;
	      dirptr->extra = (DirEP *)YS__PoolGetObj(&YS__DirEPPool);
	      dirptr->extra->waiting = 0;     /* allocate and initialize */
	      dirptr->extra->pend_req = NULL; /* a new "extra" structure */
	      dirptr->extra->WaitingFor = -1; /* for the directory       */
	      dirptr->extra->WasHere = -1;
	      return WAITFORWRB;
	    }
	  /* otherwise, some other cache holds the line. Send a
	     cache-to-cache xfer request/downgrade as discussed above */
	  DirAdd_FM(dirptr, *dir_item, node_index,req);
	  extra->counter = 1;
	  (*dir_item)->state = DIR_SHARED; /* end state will be shared */

	  /* Specify the COHE message type to send out */
	  extra->next_req_type = COPYBACK;
	  extra->size_st = REQ_SZ;
	  extra->size_req =  REQ_SZ+LINESZ;
	  extra->nack_st = NACK_NOK;
	  extra->ret_st = ACK_REPLY; /* return state when COHE returns */
	  return FORWARD_REQUEST;
	}
      else 
	YS__errmsg("Dir_Cohe(): Directory entry in unknown state");
      break;
    case UPGRADE:
      /* Cache had the line in SHARED state and now wants it in PRIVATE
	 state. In some cases (certain races, discussed in l1cache.c), the
	 directory must transparently convert this UPGRADE to a READ_OWN.
	 This race is detected if the line is currently not considered to
	 be shared by the requestor node. */
      if ((*dir_item)->state == DIR_SHARED)
	{
#ifdef DEBUG_DIRECTORY
	  if (YS__Simtime > DEBUG_TIME)
	    fprintf(simout,">>>>>>>>>>%s: UPGRADE from %d tag %ld state DIR_SHARED was:%x %x",dirptr->name,
		   req->src_node,req->tag,(*dir_item)->vector[1],(*dir_item)->vector[0]);
#endif
	  find = DirFind_FM(dirptr, *dir_item, extra->node_ary, node_index, &node_copy); /* get list of sharers */
	  if (find == 0) /* no sharers? Not allowed. */
	    {
	      fprintf(simerr,"Dir_Cohe(): State is DIR_SHARED; find must be > 1; dir node: %d\n",
		      dirptr->node_num);
	      fprintf(simerr,"Requested tag: %ld\t Request type: %s\t Request src: %d\n",
		      req->tag, Req_Type[req->req_type], req->src_node);
	      YS__errmsg("Dir_Cohe(): State is DIR_SHARED; find must be at least 1");
	    }
	  if (!node_copy) /* Node does not have a copy of this line */
	    {
	      /* This case can come from a race. The UPGRADE must be
		 transparently converted into a READ_OWN */
#ifdef DEBUG_DIRECTORY
	      if (YS__Simtime > DEBUG_TIME)
		{
		  fprintf(simout,"Got UPGRADE request from node not registered in directory; dir node: %d\n",
			  dirptr->node_num);
		  fprintf(simout,"Requested tag: %ld\t Requested Node: %d @%1.0f\n",
			  req->tag, req->src_node, YS__Simtime);
		}
#endif
	      req->req_type = READ_OWN;
	      req->size_req = LINESZ+REQ_SZ;
	    }
	  else /* node does have a copy. This is a regular upgrade */
	    {
	      /* Request will have to invalidate all other sharers */
	      extra->counter = find - 1; /* number of sharers to invalidate */
	      DirRmAllAdd_FM(dirptr, *dir_item, node_index,req); /* Build the node_ary and other fields related to INVLs */ 
#ifdef DEBUG_DIRECTORY
	      if (YS__Simtime > DEBUG_TIME)
		fprintf(simout,"  new: %x %x\n",(*dir_item)->vector[1],(*dir_item)->vector[0]);
#endif
	      (*dir_item)->state = DIR_PRIVATE; /* will be held in PRIVATE state by requestor */
		req->req_type = REPLY_UPGRADE; /* requestor will get upgrade reply */
		if (extra->counter) /* Must Invalidate other nodes */
		  {
		    /* Specify the type of COHE message to send to each
		       of the sharing nodes */
		    extra->next_req_type = INVL;
		    extra->size_st = REQ_SZ;
		    extra->size_req = REQ_SZ;
		    extra->nack_st = NACK_OK;
		    extra->ret_st = DIR_REPLY; /* return state when all COHE_REPLYs are processed */
		    return WAIT_CNT;
		  }
		else /* no nodes to invalidate */
		  {
		    return DIR_REPLY;
		  }
	    }
	}
      else if ((*dir_item)->state == UNCACHED)
	{
	  /* This is another manifestation of same race discussed above.
	     Must convert upgrade to READ_OWN in order to handle this */
#ifdef DEBUG_DIRECTORY
	  if (YS__Simtime > DEBUG_TIME)
	    {
	      fprintf(simout,">>>>>>>>>>%s: UPGRADE from %d tag %ld state UNCACHED \n",dirptr->name,req->src_node,req->tag);
	      fprintf(simout,"doing transparent conversion UPGRADE->READ_OWN tag=%ld @%1.0f\n",req->tag, YS__Simtime);
	    }
#endif
	  req->req_type = READ_OWN;
	  req->size_req = LINESZ+REQ_SZ;
	}
      else if ((*dir_item)->state == DIR_PRIVATE)
	{
	  /* This is another manifestation of same race discussed above.
	     Must convert upgrade to READ_OWN in order to handle this */
#ifdef DEBUG_DIRECTORY
	  if (YS__Simtime > DEBUG_TIME)
	    {
	      fprintf(simout,">>>>>>>>>>%s: UPGRADE from %d tag %ld state DIR_PRIVATE\n",dirptr->name,req->src_node,req->tag);
	      fprintf(simout,"doing transparent conversion UPGRADE->READ_OWN tag=%ld @%1.0f\n",req->tag, YS__Simtime);
	    }
#endif
	  req->req_type = READ_OWN;
	  req->size_req = LINESZ+REQ_SZ;
	}
      else
	YS__errmsg("Dir_Cohe(): Directory entry in unknown state");
      /* DON'T BREAK HERE -- WE WANT REMAINING CASES TO FALL THROUGH
         TO READ_OWN */
      if (req->req_type != READ_OWN)
	{
	  YS__errmsg("unhandled UPGRADE that's not a READ_OWN!!!");
	}
    case READ_OWN:
      if ((*dir_item)->state == UNCACHED)
	{
	  /* Return the line in exclusive state. No COHEs involved */
	  DirAdd_FM(dirptr, *dir_item, node_index,req);
#ifdef DEBUG_DIRECTORY
	  if (YS__Simtime > DEBUG_TIME)
	    fprintf(simout,">>>>>>>>>>%s: READ_OWN from %d tag %ld state UNCACHED new:%x %x\n",dirptr->name,
		   req->src_node,req->tag,(*dir_item)->vector[1],(*dir_item)->vector[0]);
#endif
	  extra->counter = 0; /* no COHEs needed */
	  (*dir_item)->state = DIR_PRIVATE; /* requestor is owner */
	  req->req_type = REPLY_EXCL;  /* exclusive response */
	  return VISIT_MEM;
	}
      else if ((*dir_item)->state == DIR_SHARED)
	{
	  /* Requires invalidations to be sent out to all other sharers */
#ifdef DEBUG_DIRECTORY
	  if (YS__Simtime > DEBUG_TIME)
	    fprintf(simout,">>>>>>>>>>%s: READ_OWN from %d tag %ld state DIR_SHARED was:%x %x",
		   dirptr->name,req->src_node,req->tag,
		   (*dir_item)->vector[1],(*dir_item)->vector[0]);
#endif

	  /* identify all other sharers */
	  find = DirFind_FM(dirptr, *dir_item, extra->node_ary, node_index, &node_copy);
	  if (find == 0)
	    {
	      fprintf(simerr,"Dir_Cohe(): State is DIR_SHARED; find must be at least 1; dir node: %d\n",
		      dirptr->node_num);
	      fprintf(simerr,"Requested tag: %ld\t Request type: %s\t Request src: %d\n",
		      req->tag, Req_Type[req->req_type], req->src_node);
	      YS__errmsg("Dir_Cohe(): State is DIR_SHARED; find must be at least 1");
	    }

	  /* Is the requestor node one of the sharers? If so, it must
	     have subsequently victimized the line without informing the
	     directory. In any case, don't send a COHE message to the
	     requestor node itself */
	  if (node_copy) 
	    extra->counter = find - 1;
	  else
	    extra->counter = find;
	  
	  DirRmAllAdd_FM(dirptr, *dir_item, node_index,req);
#ifdef DEBUG_DIRECTORY
	  if (YS__Simtime > DEBUG_TIME)
	    fprintf(simout,"  new:%x %x\n",(*dir_item)->vector[1],(*dir_item)->vector[0]);
#endif
	  (*dir_item)->state = DIR_PRIVATE; /* line will be owned by the requestor */
	  
	    req->req_type = REPLY_EXCL;  /* requestor gets an exclusive reply from directory */
	    if (extra->counter)
	      {
		/* Must Invalidate other nodes */
		/* Specify type of COHE message to send out */
		extra->next_req_type = INVL; 
		extra->size_st = REQ_SZ;
		extra->size_req = REQ_SZ;
		extra->nack_st = NACK_OK;
		extra->ret_st = SPECIAL_REPLY; /* return state after all COHE_REPLYs collected */
		return WAIT_CNT;
	      }
	    else /* no other nodes to invalidate -- reply right away */
	      {
		return VISIT_MEM;
	      }
	}
      else if ((*dir_item)->state == DIR_PRIVATE) /* some cache owns line */
	{
	  /* If the owner is some other cache, send a $-$+invalidate
	     (COPYBACK_INVL) to that cache so that the other processor
	     ships the data and ownership to the requestor. The
	     directory will be receive an acknowledgment only. The
	     return_value is FORWARD_REQUEST

	     If the owner is the requestor cache itself, then the cache
	     must have replaced it and immediately sent a REQUEST. In
	     that case, wait for that replacement to come in to the
	     directory (WAITFORWRB).  */
#ifdef DEBUG_DIRECTORY
	  if (YS__Simtime > DEBUG_TIME)
	    fprintf(simout,">>>>>>>>>>%s: READ_OWN from %d tag %ld state DIR_PRIVATE was:%x %x",dirptr->name,
		   req->src_node,req->tag,(*dir_item)->vector[1],(*dir_item)->vector[0]);
#endif
	  find = DirFind_FM(dirptr, *dir_item, extra->node_ary, node_index, &node_copy);
	  if (find != 1)
	    {
	      fprintf(simerr,"Dir_Cohe(): State is DIR_PRIVATE; find must be 1; dir node: %d\n",
		      dirptr->node_num);
	      fprintf(simerr,"Requested tag: %ld\t Request type: %s\t Request src: %d\n",
		      req->tag, Req_Type[req->req_type], req->src_node);
	      YS__errmsg("Dir_Cohe(): State is DIR_PRIVATE; find must be 1");
	    }
	  
	  if (node_copy)
	    {
	      /* this is the case when a line was written back to
		 memory but a subsequent miss was issued and beat the
		 write-back to the directory.  Now the request waits
		 for the write-back to occur and then things happen as
		 usual.  */
	      
#ifdef DEBUG_DIRECTORY
	      if(YS__Simtime > DEBUG_TIME)
		fprintf(simout,"Dir_Cohe(): write-back/replacement overtaken by write tag %ld at %1.0f\n",
			req->tag, YS__Simtime);
#endif

	      /* Set the variables needed to indicate waiting for WRB/REPL */
	      extra->counter = 1;
	      extra->WaitingFor = req->src_node; /* Must wait for a WRB from the same node */
	      extra->waiting = 1;
	      extra->pend_req = req; /* this REQUEST is the one to be revived */
	      (*dir_item)->pend = 1; /* mark line in pending state */
	      dirptr->wait_cntsz++;
	      if (!dirptr->wasPending) /* if it already was pending, we already have a buffer for it */
		{
		  /* otherwise, add a new buffer -- was already reserved
		     before calling DirCohe */
		  dirptr->buftotsz++;
#ifdef DEBUG_DIRECTORY
		  if (YS__Simtime > DEBUG_TIME)
		    fprintf(simout,"%s: buftotsz now %d\n",dirptr->name,dirptr->buftotsz);
#endif
		}
	      (*dir_item)->extra = extra;
	      dirptr->extra = (DirEP *)YS__PoolGetObj(&YS__DirEPPool);
	      dirptr->extra->waiting = 0;     /* allocate and initialize */
	      dirptr->extra->pend_req = NULL; /* a new "extra" structure */
	      dirptr->extra->WaitingFor = -1; /* for the directory       */
	      dirptr->extra->WasHere = -1;
	      return WAITFORWRB;
	    }

	  /* otherwise, some other cache holds the line. Send a
	     cache-to-cache xfer request/invalidate as discussed above */
	  
	  DirRmAllAdd_FM(dirptr, *dir_item, node_index,req);
#ifdef DEBUG_DIRECTORY
	  if (YS__Simtime > DEBUG_TIME)
	    fprintf(simout,"  new:%x %x\n",(*dir_item)->vector[1],(*dir_item)->vector[0]);
#endif
	  (*dir_item)->state = DIR_PRIVATE; /* requesting processor will end
					       up as owner */

	    /* Specify type of COHE to send */
	    extra->counter = 1; 
	    extra->next_req_type = COPYBACK_INVL;
	    extra->size_st = REQ_SZ;
	    extra->size_req =  REQ_SZ;
	    extra->nack_st = NACK_NOK;
	    extra->ret_st = ACK_REPLY;  /* return state when all COHE_REPLYs collected */
	  
	    return FORWARD_REQUEST;
	}	
      else 
	YS__errmsg("Dir_Cohe(): Directory entry in unknown state");
      break;
    case WRB:
    case REPL: /* these two are handled in the same way -- the difference is
		  whether it has data or not */
      if ((*dir_item)->state == DIR_PRIVATE)
	{
	  /* this is the expected case */
#ifdef DEBUG_DIRECTORY
	  if (YS__Simtime > DEBUG_TIME)
	    fprintf(simout,">>>>>>>>>>%s: WRB from %d tag %ld state DIR_PRIVATE was:%x %x",dirptr->name,
		   req->src_node,req->tag,(*dir_item)->vector[1],(*dir_item)->vector[0]);
#endif
	  find = DirFind_FM(dirptr, *dir_item, NULL, node_index, &node_copy);
	  if (find != 1)
	    {
	      fprintf(simerr,"Dir_Cohe(): State is DIR_PRIVATE; find must be 1; dir node: %d\n",
		      dirptr->node_num);
	      fprintf(simerr,"Requested tag: %ld\t Request type: %s\t Request src: %d\n",
		      req->tag, Req_Type[req->req_type], req->src_node);
	      YS__errmsg("Dir_Cohe(): State is DIR_PRIVATE; find must be 1");
	    }
	  if (!node_copy)
	    {
	      /* This case can happen in cases where a line was pending:
		 the other processor tried to demand a $-$, but before that
		 could happen, the owner did a WRB. In this case, the
		 WasHere flag has been set above, and the $-$ will realize
		 that the REQUEST has to be retried when it returns with
		 a NACK or NACK_PEND */
	      DirRmAll_FM(dirptr, *dir_item, node_index); /* line is now uncached */
	      (*dir_item)->state = UNCACHED;
	      return VISIT_MEM;
	    }
	  DirRmAll_FM(dirptr, *dir_item, node_index); /* line is now uncached */
#ifdef DEBUG_DIRECTORY
	  if (YS__Simtime > DEBUG_TIME)
	    fprintf(simout,"  new:%x %x\n",(*dir_item)->vector[1],(*dir_item)->vector[0]);
#endif
	  (*dir_item)->state = UNCACHED;
	    return VISIT_MEM;
	}
      else if ((*dir_item)->state == UNCACHED) /* line is uncached -- this should never happen. */
	{
#ifdef DEBUG_DIRECTORY
	  if (YS__Simtime > DEBUG_TIME)
	    fprintf(simout,">>>>>>>>>>%s: %s from %d tag %ld state UNCACHED\n",dirptr->name,Req_Type[req->req_type],req->src_node,req->tag);
#endif
	  fprintf(simerr,"Requested tag: %ld\t Requested Node: %d\t dir node %d\n",
		  req->tag, req->src_node, dirptr->node_num);
	  fprintf(simout,"Requested tag: %ld\t Requested Node: %d\t dir node %d\n",
		 req->tag, req->src_node, dirptr->node_num);
	  YS__errmsg("Dir_Cohe(): State is UNCACHED; Must not get WRB/REPL request"); 
	  DirRmAll_FM(dirptr, *dir_item, node_index);
	  (*dir_item)->state = UNCACHED;
	  return VISIT_MEM;
	}
      else if ((*dir_item)->state == DIR_SHARED)
	{
	  /* This case can happen in cases where a line was pending:
	     the other processor tried to demand a $-$, but before that
	     could happen, the owner did a WRB. In this case, the
	     WasHere flag has been set above, and the $-$ will realize
	     that the REQUEST has to be retried when it returns with
	     a NACK or NACK_PEND */
#ifdef DEBUG_DIRECTORY
	  if (YS__Simtime > DEBUG_TIME)
	    {
	      fprintf(simout,">>>>>>>>>>%s: %s from %d tag %ld state DIR_SHARED\n",dirptr->name,Req_Type[req->req_type],req->src_node,req->tag);
	    }
	  fprintf(simout,"Requested tag: %ld\t Requested Node: %d\t dir node %d\n",
		 req->tag, req->src_node, dirptr->node_num);
	  fprintf(simout,"The address is : %ld at time %f\n", req->address, YS__Simtime);
#endif
	  /* the line is now uncached, held by no node */
	  DirRmAll_FM(dirptr, *dir_item, node_index);
	  (*dir_item)->state = UNCACHED;
	  return VISIT_MEM;
	}
      else 
	YS__errmsg("Dir_Cohe(): Directory entry in unknown state");
      break;
    case RWRITE:
      YS__errmsg("Remote writes not supported in this version of RSIM\n");
      break;
      
    default:
      YS__errmsg("Dir_Cohe(): Unknown request type");
      break;
    }
  YS__errmsg("Request was not handled by Dir_Cohe!\n");
  return VISIT_MEM;
}


/*****************************************************************************/
/* Pending buffer functions: manage the pending buffer of the directory.     */
/* These are the requests that are waiting on either a WRB or a transient    */
/* directory state to clear up before being allowed to be processed.         */
/*****************************************************************************/

static void DirAddBuf(dirptr, req) /* put a new entry into pending buffer */
     DIR *dirptr;
     REQ *req;
{
  if ((dirptr->buftotsz == dirptr->bufmaxsz && !dirptr->wasPending) || dirptr->buftotsz > dirptr->bufmaxsz)
    YS__errmsg("DirAddBuf(): Should check buffer size before calling this routine");
  if (dirptr->BufTail)    {
    dirptr->BufTail->next = req;
    dirptr->BufTail = req;
  }
  else    {
    dirptr->BufHead = req;
    dirptr->BufTail = req;
  }
  req->next = NULL;
  if (!dirptr->wasPending) /* if it had already been pending, we had a buffer for it */
    {
      /* Otherwise, increment the total directory buffer as well. We
	 should have already reserved one of these for possible expansion
	 before calling DirAddBuf (probably before calling Dir_Cohe */
      dirptr->buftotsz ++;
#ifdef DEBUG_DIRECTORY
      if (YS__Simtime > DEBUG_TIME)
	fprintf(simout,"%s: buftotsz now %d\n",dirptr->name,dirptr->buftotsz);
#endif
    }
  if (MemsimStatOn)    {
    StatrecUpdate(dirptr->BufTotSzMeans, (double)dirptr->buftotsz, YS__Simtime);
  }
  
}

static void DirCheckBuf(DIR *dirptr, long tag) /* step through pending buffer clearing out entries that match the specified tag */
{
  /* a line has just become non-pending, so revive its pending REQUESTs */
  REQ *temp, *temp_prev, *temp_next;
  
  if (dirptr->BufHead)
    {
      temp = dirptr->BufHead;
      temp_prev = NULL;
      while (temp) /* step through the buffer */
	{
	  if (temp->tag == tag) /* a match */
	    {
#ifdef DEBUG_DIRECTORY
	      if (YS__Simtime > DEBUG_TIME)
		{
		  fprintf(simout,"DirCheckBuf: %s removing tag %ld src_node %d from queue.\n",dirptr->name,tag,temp->src_node);
		}
#endif
	      dirptr->bufsz_rdy ++; /* This is now a "ReadyReq" */

	      /* Remove this element from the pending buffer queue */
	      if (temp_prev) 
		{
		  temp_prev->next = temp->next;
		}
	      else
		{
		  dirptr->BufHead = temp->next;
		}
	      temp_next=temp->next;
	      if (temp->next == NULL) /* last item in this buffer */
		{
		  dirptr->BufTail = temp_prev;
		}
	      else
		{
		  temp->next = NULL;
		}

	      /* Now add the REQUEST into the ReadyReqs queue, which are
		 requests that are now ready for processing. These are
		 processed after req_partial clears out, but before any
		 new incoming REQUESTs are allowed. */
	      DirQueueAdd(dirptr->ReadyReqs,temp);

	      temp=temp_next;
	      /* we don't change temp_prev, since that's still the prev one */
	    }
	  else /* no match, keep walking through queue */
	    {
	      temp_prev = temp;
	      temp = temp->next;
	    }
	}
    }
}

/*****************************************************************************/
/* DirQueue structure and operations: used for OutboundReqs and ReadyReqs.   */
/*****************************************************************************/

struct DirQueue
{
  REQ *head;
  REQ *tail;
};

static struct DirQueue *NewDirQueue()
{
  struct DirQueue *res = malloc(sizeof(struct DirQueue));
  res->head=res->tail=NULL;
  return res;
}

static void DirQueueAdd(struct DirQueue *q,REQ *req)
{
  if (q->head == NULL)
    {
      q->head=q->tail=req;
      req->next=NULL;
    }
  else
    {
      q->tail->next=req;
      q->tail=req;
    }
}

static REQ *DirQueuePeek(struct DirQueue *q)
{
  return q->head;
}

static REQ *DirQueueRm(struct DirQueue *q)
{
  REQ *old = q->head;
  if (old)
    {
      q->head=q->head->next;
      if (old->next == NULL) /* it was a one element queue, now zero */
	q->tail=NULL;
      else
	old->next=NULL;
    }
  return old;
}

/*****************************************************************************/
/* Table Maintenance Routines: These keep track of the state of each cache   */
/* line and the sharers                                                      */
/*****************************************************************************/

/* adds a new sharer */
static int DirAdd_FM(DIR *dirptr, Dirst *dir_item, int node_index, REQ *req) /* return nonzero if we alone used to have it */
#if 0
     DIR *dirptr;		/* Pointer to directory module */
     Dirst *dir_item;		/* Pointer to data structure for this line */
     int  node_index;		/* node_index should go from 0 to num_nodes-1 */
#endif
{
  int i, j, num_nodes;
  unsigned oursalone, hadit;
  
  num_nodes = dirptr->num_nodes;
  if (node_index >= num_nodes)
    YS__errmsg("Invalid node index");
  
  i = node_index / 32;
  j = node_index % 32;

  hadit = (dir_item->vector[i] & (1 << j));
  oursalone = hadit && (dir_item->numsharers == 1);
  if (!hadit)
    dir_item->numsharers++;
  dir_item->vector[i] = dir_item->vector[i] | (1<<j);

  /* Stats for cold misses */
  if (!(dir_item->cold_vec[i] & (1 << j))) {
    /* in other words, we've never come here before for this processor */
    req->line_cold = 1; /* so we'll see it for sure when we get line cold */
    dir_item->cold_vec[i] = dir_item->cold_vec[i] | (1<<j); 
  }

  return oursalone;
}

/* removes all sharers but the specified node */
static void DirRmAllAdd_FM(DIR *dirptr, Dirst *dir_item, int node_index, REQ *req)
#if 0
     DIR *dirptr;		/* Pointer to directory module */
     Dirst *dir_item;		/* Pointer to data structure for this line */
     int  node_index;		/* node_index should go from 0 to num_nodes-1 */
#endif
{
  int i, j, k, num_nodes;
  
  num_nodes = dirptr->num_nodes;
  if (node_index >= num_nodes)
    YS__errmsg("Invalid node index");
  
  i = node_index / 32;
  j = node_index % 32;

  dir_item->numsharers = 1;
  dir_item->vector[i] =  (1<<j);

  for (k=0; k<i; k++)
    dir_item->vector[k] = 0;
  for (k=i+1; k < dirptr->vec_index; k++)
    dir_item->vector[k] = 0;

  /* Stats for cold misses */
  if (!(dir_item->cold_vec[i] & (1 << j))) {
    /* in other words, we've never come here before for this processor */
    req->line_cold = 1; /* so we'll see it for sure when we get line cold */
    dir_item->cold_vec[i] = dir_item->cold_vec[i] | (1<<j); 
  }


}

/* remove all sharers, including requestor. Used in WRBs and invalidation
   case of remote writes. */
static void DirRmAll_FM(DIR *dirptr, Dirst *dir_item, int node_index)
#if 0
     DIR *dirptr;		/* Pointer to directory module */
     Dirst *dir_item;		/* Pointer to data structure for this line */
     int  node_index;		/* node_index should go from 0 to num_nodes-1 */
#endif
{
  int k, num_nodes;
  
  num_nodes = dirptr->num_nodes;
  if (node_index >= num_nodes)
    YS__errmsg("Invalid node index");
  
  dir_item->numsharers = 0;
  for (k=0; k<dirptr->vec_index; k++)
    dir_item->vector[k] = 0;
}

/* remove specified node from sharers */
void DirRm_FM(DIR *dirptr, Dirst *dir_item, int node_index)
#if 0
     DIR *dirptr;		/* Pointer to directory module */
     Dirst *dir_item;		/* Pointer to data structure for this line */
     int  node_index;		/* node_index should go from 0 to num_nodes-1 */
#endif
{
  int i, j, num_nodes;
  
  num_nodes = dirptr->num_nodes;
  if (node_index >= num_nodes)
    YS__errmsg("Invalid node index");
  
  i = node_index / 32;
  j = node_index % 32;

  if (dir_item->vector[i] & (1<<j))
    dir_item->numsharers--;
  
  dir_item->vector[i] = dir_item->vector[i] & (~(1<<j));
  
}

/* find the list of sharers, built up in node_ary */
static int DirFind_FM(dirptr, dir_item, node_ary, node_num, node_copy)
     DIR *dirptr;		/* Pointer to directory module */
     Dirst *dir_item;		/* Pointer to data structure for this line */
     int *node_copy, *node_ary;	 
     int node_num;		/* Node number of current request */
{
  int i, k, node;
  unsigned j;
  int node_ary_idx = 0;
  int find = 0;
  
  *node_copy = 0;

  for (i=0; i < dirptr->vec_index; i++)
    {
      if (dir_item->vector[i])
	{
	  j=1;
	  for (k=0; k<32; k++)
	    {
	      if (dir_item->vector[i] & j)
		{
		  if ((node_ary != NULL) && ((node = i*32+k) != node_num))
		    {
		      node_ary[node_ary_idx] = node ;
		      node_ary_idx++;
		    }
		  else *node_copy = 1; /* requesting node (node_num) has a copy of data */
		  find ++;
		}
	      j = j<<1;
	    }
	}
    }
  return find;
}

/*****************************************************************************/
/* DirHashLookup_FM: Lookup the specified line in the directory chain        */
/* hash table. If line is not present, add a new empty entry for it.         */
/*****************************************************************************/
Dirst *DirHashLookup_FM(dirptr, tag)
     DIR *dirptr;
     long tag;
{
  int i,j;

  Dirst *temp, *temp_prev;

  i = tag & HashIdxMask;		/* hash index for this tag  */

  if (dirptr->data_hash[i] == NULL) { /* this hash head is null */
    dirptr->data_hash[i] = (Dirst *)YS__PoolGetObj(&YS__DirstPool);
    temp = dirptr->data_hash[i];
    temp->tag = tag;
    for (j=0; j < dirptr->vec_index; j++)
      temp->vector[j] = temp->cold_vec[j] = 0;
    temp->state = UNCACHED;
    temp->pend = 0;
    temp->extra = NULL;
    temp->next = NULL;
    return temp;
  }
    
  else {
    temp = dirptr->data_hash[i];
    temp_prev = NULL;
    
    while(temp) {
      if (temp->tag == tag)
	{
	  /* We'll do a self-optimizing hash list that moves this to the
	     head automatically */
	  if (temp_prev) /* this means we weren't already at the head */
	    {
	      temp_prev->next = temp->next; /* take us out of the list */
	      temp->next = dirptr->data_hash[i]; 
	      dirptr->data_hash[i] = temp; /* move this to head, since it's likely to be ref'd again soon */
	    }
	  return temp;
	}
      else {
	temp_prev = temp;
	temp = temp->next;
      }
    }
    if (!temp) {
      temp = (Dirst *)YS__PoolGetObj(&YS__DirstPool);
      if (temp_prev)
	{
	  /* Put the new entry at the head -- done below */
	}
      else
	YS__errmsg("DirHashLookup_FM(): This should be a hash head entry");
    }
    temp->tag = tag;
    for (j=0; j < dirptr->vec_index; j++)
      temp->vector[j] = temp->cold_vec[j] = 0;
    temp->state = UNCACHED;
    temp->pend = 0;
    temp->extra = NULL;
    temp->next = dirptr->data_hash[i];
    dirptr->data_hash[i] = temp; /* move this to head, since it's likely to be ref'd */
    return temp;
  }
}

/*****************************************************************************/
/* FixForwardRARs: auxiliary function used to reset req_type when a forward  */
/* is being sent back to cache as an RAR                                     */
/*****************************************************************************/

static void FixForwardRARs(REQ *req)
{
  /* Resets req->req_type appropriately.
     COPYBACK --> READ_SH
     COPYBACK_INVL --> READ_OWN
     COPYBACK_SHDY --> READ_SH
     other --> unknown */
  
  switch (req->req_type)
    {
    case COPYBACK:
    case COPYBACK_SHDY:
      req->req_type=READ_SH;
      break;
    case COPYBACK_INVL:
      req->req_type=READ_OWN;
      break;
    default:
      YS__errmsg("Unknown forward type being nacked!");
    }
}

/*****************************************************************************/
/* ReqSetHandled: function sets the type of access that this REQ             */
/* incurred.  The type is based on whether or not the directory is local     */
/* to the requestor and whether or not the transaction required sending      */
/* out any COHE messages (and, if so, whether or not those were local to     */
/* the directory node)                                                       */
/*****************************************************************************/


static void ReqSetHandled(REQ *req, int remotehome, int coheness)
{
  /* NOTE: This function sets handled only if it hasn't already been set */
  if (req->handled != reqUNKNOWN)
    return;
  if (remotehome)
    {
      req->miss_type = mtREM;
      if (coheness == 0)
	req->handled = reqDIR_RH_NOCOHE;
      else if (coheness == 1)
	req->handled = reqDIR_RH_LCOHE;
      else if (coheness == 2)
	req->handled = reqDIR_RH_RCOHE;
      else
	YS__errmsg("bad cohe state in ReqSetHandled");
    }
  else
    {
      if (coheness)
	{
	  req->miss_type = mtREM;
	  req->handled = reqDIR_LH_RCOHE;
	}
      else
	{
	  req->miss_type = mtLOCAL;
	  req->handled = reqDIR_LH_NOCOHE;
	}
    }
}

/*****************************************************************************/
/* dir_commit_req: commit a request either from an actual input port or from */
/* the ReadyReqs queue, as appropriate                                       */
/*****************************************************************************/

static REQ *dir_commit_req(DIR *dirptr,int in_port_num, int wasPending)
{
  if (wasPending)
    {
      dirptr->bufsz_rdy--;
      return DirQueueRm(dirptr->ReadyReqs);      
    }
  else
    return commit_req((SMMODULE *)dirptr,in_port_num);
}

/*****************************************************************************/
/* NewDir: This routine initializes and returns a pointer to a DIR module.   */
/*****************************************************************************/

DIR *NewDir(char *name, int node_num, int stat_level, rtfunc routing,
	    struct DirDelays *Delays, int ports_abv, int ports_blw, 
	    int line_size, int dir_type, int num_nodes, int bufsz,
	    cond cohe_rtn, int netsend_port, int netrcv_port, LINKPARA *link_para)
#if 0
     char   *name;		/* Name of the module upto 32 characters long          */
     int    node_num;		/* Node to which this directory is attached            */
     int    stat_level;		/* Level of statistics collection for this module      */
     rtfunc routing;		/* Routing function of the module                      */
     struct Delays   *Delays;	/* Pointer to user-defined structure for delays        */
     int    ports_abv;		/* Number of ports above this module                   */
     int    ports_blw;		/* Number of ports below this module                   */
                                /* DIR Attributes                                      */
     int    line_size;
     int    dir_type;		/* Only CNTRL_FULL_MAP supported; (full bit map vector) */
     int    num_nodes;		/* number of nodes present */
     int    bufsz;		/* size of buffering in module */
     cond   cohe_rtn;
     int    netsend_port, netrcv_port;		
     LINKPARA *link_para;
#endif
{
  DIR *dirptr;
  ARG *arg;
  int i;
  char evnt_name[32];
  
  dirptr = (DIR *)malloc(sizeof(DIR)); 
  if (dir_index < MAX_MEMSYS_PROCS)
    {
      dir_ptr[dir_index] = dirptr;
      dir_index ++;
    }
  if (dir_index == 1) 		/* First directory module created */
    CoheNumInvlHist =
      NewStatrec("NumInvl", POINT, MEANS, HISTSPECIAL, num_nodes, 0.0, (double)num_nodes);
  
  dirptr->id = YS__idctr++;	/* System assigned unique ID   */
  strncpy(dirptr->name, name,31); /* Copy module name */
  dirptr->name[31] = '\0';
  dirptr->module_type = DIR_MODULE; /* Initialize module type as a dir */

  /* This sets the output q size of the ports */
  ModuleInit ((SMMODULE *)dirptr, node_num, stat_level, routing, Delays,
	      ports_abv + ports_blw, portszdir-1);
  /* Initialize data structures common to all modules    */
  
 dirptr->num_ports_abv = ports_abv; /* Number of ports above */

 
  sprintf(evnt_name, "%s_dirsim",name);
  /* "_dirsim" is appended to  module name */

  dirptr->Sim  = (ACTIVITY *)NewEvent(evnt_name, DirSim, NODELETE, 0);
  
  /* Create new event */
  arg = (ARG *)malloc(sizeof(ARG)); /* Setup arguments for dir event */ 
  arg->mptr = (SMMODULE *)dirptr; 
  ActivitySetArg(dirptr->Sim, (char *)arg, UNKNOWN);  
  
  dirptr->req = NULL;
  dirptr->in_port_num = -1;
  if (link_para)
    YS__errmsg("link_para not supported for directory");
  else
    dirptr->wakeup = wakeup;	/* Wakeup routine for dir */
  dirptr->handshake = NULL; /* no handshake function used */
  
  dirptr->dir_type = dir_type; /* Dir parameters */
  
  dirptr->reqport = 0; /* constant for our directory connections */
  
  if (node_num >= num_nodes || node_num < 0)
    YS__errmsg("NewDir(): Node number of directory not in the range 0 to num_nodes-1");
  dirptr->line_size = line_size;
  dirptr->num_nodes = num_nodes;
  if (dir_type == CNTRL_FULL_MAP)     
    dirptr->vec_index = (num_nodes / (sizeof(int)*8)) + ((num_nodes%(sizeof(int)*8)) ? 1:0);

  /* Initialize directory buffer, pending buffer, and queues */
  dirptr->buftotsz = 0;
  dirptr->bufmaxsz = bufsz;
  dirptr->OutboundReqs = NewDirQueue();
  dirptr->ReadyReqs = NewDirQueue();
  dirptr->wasPending = 0;
  
  dirptr->BufHead = NULL;
  dirptr->BufTail = NULL;
  dirptr->buftotsz = 0;
  dirptr->bufsz_rdy = 0;
  dirptr->outboundsz = 0;
  dirptr->wait_cntsz = 0;

  dirptr->req_partial = NULL;

  dirptr->next_port = 0; /* initialize round-robin pointer */

  /* Allocate and initialize an "extra" info structure */
  dirptr->extra = (DirEP *)YS__PoolGetObj(&YS__DirEPPool);
  dirptr->extra->waiting = 0;
  dirptr->extra->pend_req = NULL;
  dirptr->extra->WaitingFor = -1;
  dirptr->extra->WasHere = -1;


  /* Start out the chain Hash table */
  for (i=0; i<HashIdxMask+1; i++)  {
    dirptr->data_hash[i] = NULL;
  }
  
  dirptr->cohe_rtn = cohe_rtn;      /* Dir_Cohe by default */
  if (link_para)   {
   YS__errmsg("link para not supported\n");
  }
  
  dirptr->num_read = 0;
  dirptr->num_cohe = 0;
  dirptr->num_clean = 0;
  dirptr->num_write = 0;
  dirptr->num_writeback = 0;
  dirptr->num_local = 0;
  dirptr->num_remote = 0;
  dirptr->num_buf_RAR = 0;
  dirptr->num_race_RAR = 0;
  dirptr->num_c2c = 0;
  sprintf(evnt_name, "%s_NumInvlStat",name);
  dirptr->CoheNumInvlMeans = NewStatrec(evnt_name, POINT, MEANS, NOHIST, 0, 0, 0);
  sprintf(evnt_name, "%s_DirBufTotSize",name);
  dirptr->BufTotSzMeans = NewStatrec(evnt_name, INTERVAL, MEANS, NOHIST, 0, 0, 0);
  dirptr->utilization = 0.0;
  dirptr->time_of_last_clear = 0.0;
  return dirptr;
}

/*****************************************************************************/
/* Directory statistics routines: collect some important statistics about    */
/* directory behavior.                                                       */
/*****************************************************************************/


void dir_stat(dirptr, req, type)
     DIR *dirptr;
     REQ *req;
     int type;
{
  if (MemsimStatOn)  {
    dirptr->num_ref ++;		/* Number of refernces to dir module */
    
    if (dirptr->stat_level > 1) {
      if (type == READ || type == READ_SH)
	dirptr->num_read ++;
      else if (type == WRITE || type == READ_OWN)
	dirptr->num_write ++;
      
      if (req->src_node == req->dest_node) 
	dirptr->num_local ++;
    }
  }
}

/*****************************************************************************/

/* Reports statistics of all dir modules
 */
void DirStatReportAll()
{
  int i;
  if (dir_index) {
    if (dir_index == MAX_MEMSYS_PROCS) 
      YS__warnmsg("Greater than MAX_MEMSYS_PROCS dirs created; statistics for the first MAX_MEMSYS_PROCS will be reported by DirStatReportAll");
    
    fprintf(simout,"\n##### Dir Statistics #####\n");
    
    for (i=0; i< dir_index; i++) 
      DirStatReport (dir_ptr[i]); /* Call print routine supplied by user for each module */

  }
}

void DirStatClearAll()
{
  int i;
  if (dir_index) {
    for (i=0; i< dir_index; i++) 
      DirStatClear (dir_ptr[i]);	/* Call print routine supplied by user for each module */
  }
}

void DirStatReport (dirptr)
     DIR *dirptr;
{
  int remote;
  double rfr, wfr, lfr, refr, buffr, buf_pendfr;
  double  hitpr, util, lat;
  FILE *out;

  out=simout;
  

  SMModuleStatReport(dirptr , &hitpr, &lat, &util);

  if (dirptr->stat_level > 1) {
    
    if (dirptr->num_ref) {
      rfr = (double)dirptr->num_read/(double)dirptr->num_ref;
      wfr = (double)dirptr->num_write/(double)dirptr->num_ref;
      lfr = (double)dirptr->num_local/(double)dirptr->num_ref;
      remote = dirptr->num_ref - dirptr->num_local;
      refr = (double)remote/(double)dirptr->num_ref;
      buffr  =(double)dirptr->num_buf_RAR/(double)dirptr->num_ref;
      buf_pendfr  =(double)dirptr->num_race_RAR/(double)dirptr->num_ref;
    }
    else {rfr = 0.0; wfr = 0.0; lfr = 0.0; refr = 0.0; }
    
    fprintf(simout,"              Read             Write              Local            Remote\n");
    fprintf(simout,"         %10d(%6.4g) %8d(%6.4g) %8d(%.4g) %8d(%6.4g)\n", 
	    dirptr->num_read, rfr, dirptr->num_write, wfr, dirptr->num_local, lfr, remote, refr);
    fprintf(simout,"              NumBufRAR \n");
    fprintf(simout,"         %10d(%6.4g) \n", 
	    dirptr->num_buf_RAR, buffr);
  }
  
}

void DirStatClear (dirptr)
     DIR *dirptr;
{

  SMModuleStatClear(dirptr);
  
  if (dirptr->stat_level > 1) {
    dirptr->num_read = 0;
    dirptr->num_cohe = 0;
    dirptr->num_clean = 0;
    dirptr->num_write = 0;
    dirptr->num_writeback = 0;
    dirptr->num_local = 0;
    dirptr->num_remote = 0;
    dirptr->num_buf_RAR = 0;
    dirptr->num_race_RAR = 0;
    dirptr->num_c2c = 0;
    dirptr->utilization = 0.0;
    dirptr->time_of_last_clear = YS__Simtime;
  }
  if (dirptr->stat_level > 2) {
    StatrecReset (dirptr->CoheNumInvlMeans);
    StatrecReset(dirptr->BufTotSzMeans );
  }
}

