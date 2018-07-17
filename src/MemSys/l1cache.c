/*
  l1cache.c

  File contains the first level cache procedures
  
  The first-level of cache can be either write-through with
  no-write-allocate or write-back with write-allocate.  The cache
  supports multiple outstanding misses, and may be pipelined and
  multiported.  If the configuration uses a write-through L1 cache, a
  write-buffer is also included between the two levels of cache. The
  L1 cache is modeled as a unified SRAM array.

  It is highly recommended that the user be familiar (to some extent)
  with mshr.c and pipeline.c before seeking to modify this file.

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
#include "MemSys/mshr.h"
#include "MemSys/req.h"
#include "MemSys/net.h"
#include "MemSys/stats.h"
#include "MemSys/misc.h"

#include "Processor/memprocess.h"
#include "Processor/capconf.h"
#include "Processor/mainsim.h"
#include "Processor/simio.h"

int L1TAG_DELAY=1; /* time spent in TAG-RAM pipe */
int L1TAG_PIPES=3; /* number of ports to L1 Tag -- includes REQUEST, REPLY, COHE */
int L1TAG_PORTS[10]={L1_DEFAULT_NUM_PORTS,1,1}; /* width of individual pipes */
int L1_MAX_MSHRS=8; /* default number of MSHRs */

int L1_NUM_PORTS = L1_DEFAULT_NUM_PORTS;

/* macros which specify which pipeline each type of access belongs to */
#define L1CoheReplyTAGPIPE(req) 2
#define L1CoheTAGPIPE(req) 2 /* in the L1 cache, COHE is the same as
				COHE_REPLY Note: we have added a
				separate COHE tag pipe in order to
				avoid deadlock.  Such a port may be
				considered excessive in a real system;
				thus, it may be advisable to reserve a
				certain number of MSHR-like buffers
				for incoming {\tt COHE} messages and
				simply withhold processing of
				additional {\tt COHE}s until space
				opens up in one of these buffers. */
#define L1ReplyTAGPIPE(req) 1 /* reply tag pipe */
#define L1ReqTAGPIPE(req) 0 /* request tag pipe */

struct state;
static int L1ProcessTagReq(CACHE *, REQ *); /* function that actually processes
					       transactions */

/***************************************************************************/
/* L1CacheInSim: function that brings new transactions from the ports into */
/* the pipelines associated with the various cache parts. An operation can */
/* be removed from its port as soon as pipeline space opens up for it.     */
/* Called whenever there may be something on an input port                 */
/***************************************************************************/

void L1CacheInSim(struct state *proc)
{
  REQ *req;
  
  int i;
  int nothing_on_ports = 1;
  ARG    *argptr;
  CACHE *captr;
  nothing_on_ports=1;
  
  /* Get pointer to cache module */
  argptr = (ARG *) GetL1ArgPtr(proc);
  captr = (CACHE *)argptr->mptr;
#ifdef COREFILE
  if(YS__Simtime > DEBUG_TIME)
    fprintf(simout,"%s calling \t\tL1CacheInSim %.2f \n",captr->name, YS__Simtime);
#endif
  for (i=captr->num_ports-1; i >= 0 ; i--)  /* loop through the input ports */
    {
      while( (req = peekQ(captr->in_port_ptr[i]))) /* anything available */
	{
	  nothing_on_ports=0;           /* there's something on the ports   */
	  if (req->s.type == REQUEST)   /* a request from processor memunit */
	    {
	      if (AddToPipe(captr->pipe[L1ReqTAGPIPE(req)],req) == 0)
		{
		  /* successfully added to request pipeline */
		  req->progress=0;
		  commit_req((SMMODULE *)captr,i); /* remove from queue */

		  /* now that we've committed it, we should also free the mem
		     unit corresponding to it */
		  FreeAMemUnit(req->s.proc, req->s.inst_tag);
		  
		  L1Q_FULL[captr->node_num] = 0; /* no longer full! */
		  captr->num_in_pipes++;
		  captr->pipe_empty = 0; /* No longer empty */
		}
	      else /* couldn't add it to request pipeline */
		break;
	    }
	  else if (req->s.type == REPLY) /* reply from lower levels */
	    {
	      if (AddToPipe(captr->pipe[L1ReplyTAGPIPE(req)],req) == 0)
		{
		  /* successfully added to reply pipeline */
		  req->progress=0; 
		  commit_req((SMMODULE *)captr,i); /* remove from port */

		  captr->num_in_pipes++;
		  captr->pipe_empty = 0; /* No longer empty */
		}
	      else /* couldn't add it to pipe */
		break;
	    }
	  else if (req->s.type == COHE) /* system coherence transaction */
	    {
	      if (AddToPipe(captr->pipe[L1CoheTAGPIPE(req)],req) == 0)
		{
		  /* successfully added to cohe pipeline */
		  req->progress=0;
		  commit_req((SMMODULE *)captr,i);
		  /* rmQ(captr->in_port_ptr[i]); */
		  captr->num_in_pipes++;
		  captr->pipe_empty = 0; /* No longer empty */
		}
	      else  /* couldn't add it to pipe */
		break;
	    }
	  else if (req->s.type == COHE_REPLY)
	    {
	      YS__errmsg("L1 cache should not get a COHE_REPLY in InSim?");
	    }
	  else
	    {
	      YS__errmsg("Unknown req type");
	    }
	}
    }
  if(nothing_on_ports) /* nothing available for processing */
    captr->inq_empty = 1; /* All inqs are apparently empty */
  
  return;
}

/***************************************************************************/
/* L1CacheOutSim: initiates actual processing of various REQ types         */
/* Called each cycle that there is something in pipes to be processed      */
/* (even if not at the head of the pipes -- the pipes also need to be      */
/* cycled)                                                                 */
/***************************************************************************/

void L1CacheOutSim(struct state *proc)
{
  REQ *req;
  int pipenum;
  int ctr;
  
  Pipeline *pipe;
  ARG    *argptr;
  CACHE *captr;

  /* Get the cache pointer */
  argptr = (ARG *) GetL1ArgPtr(proc);
  captr = (CACHE *)argptr->mptr; /* Pointer to cache module */
#ifdef COREFILE
  if(YS__Simtime > DEBUG_TIME)
    fprintf(simout,"%s calling \t\tL1CacheOutSim %.2f\n",captr->name, YS__Simtime);
#endif

  /* start out by checking what the system calls "smart MSHR list".
     These are activities which are being held in some cache resource
     (such as an MSHR) waiting to be sent to another module. If there
     are any such actions, the cache attempts to process one of these
     by trying to send it on its appropriate port. In some cases,
     successful sending from a smart MSHR list may free up the resource
     being held.  */
  
  if (captr->SmartMSHRHead) /* Anything on smart MSHRs */
    ProcessFromSmartMSHRList(captr,1); /* right now, we can only do 1 */

  for (pipenum=0; pipenum< L1TAG_PIPES; pipenum++)
    /* cycle through the pipes checking if there is anything */
    {
      pipe = captr->pipe[pipenum];
      ctr=0;
      /* See if there is any request that has reached head of this pipe */
      while ((req=GetPipeElt(pipe,ctr)) != NULL) /* once NULL, we're done */
	{
	  if (L1ProcessTagReq(captr, req)) /* process the request */
	    {
	      /* if it succeeds */
	      ClearPipeElt(pipe,ctr); 
	      captr->num_in_pipes--;
	      if(captr->num_in_pipes == 0)
		captr->pipe_empty = 1;
	    }
	  ctr++;
	}
      CyclePipe(pipe); /* advance the pipe by a stage */
    }
  
  captr->utilization += 1.0;
  if (captr->cache_level_type == FIRSTLEVEL_WT)
    WBSim(proc, L1WB); /* Let write-buffer handle any new requests put in */
  return;
}

/***************************************************************************/
/* L1ProcessTagReq: simulates accesses to the L1 cache.                    */
/* Its behavior depends on the type of message (i.e. s.type) received      */
/* Detailed comments are interspersed throughout this (large) function     */
/* Returns 0 on failure; 1 on success.                                     */
/***************************************************************************/

   
static int L1ProcessTagReq(CACHE *captr, REQ *req)
{
  int  hittype, type, data_type, reply;
  int req_sz, rep_sz, nxt_req_sz;
  long tag, address;
  int set, set_ind, i1, i2;
  int cohe_type, allo_type, allocate,  dest_node;
  int hittype_pres_mshr;
  CacheLineState cur_state, nxt_st, dummy3;
  ReqType nxt_mod_req, nxt_req, dummy2;
  int    req_sz_abv, rep_sz_abv, req_sz_blw, rep_sz_blw;
  ReqType blw_req_type, abv_req_type;
  int    allo_type_repl, cohe_type_repl, dest_node_repl, dummy, tag_repl;
  int return_int,pend_cohe;
  enum CacheMissType ccdres;
  int mshr_num, writes_present;
  REQ *wrb_req;
  enum MISS_TYPE miss_type;

  /* Unused variables */
  /* int i, temp; */
  /* this has most of the functionality of the old cache */
  /* remember -- if you stall the pipeline, return 0; if you leave the pipeline, return 1 */
  address = req->address;
  type = req->req_type;
  data_type = req->address_type;
  
  req->tag = address >> captr->block_bits; 
  req->linesz = captr->linesz;
  
  switch (req->s.type) /* decide what to do based on what type of REQ */
    {
    case REQUEST:
      if (req->progress == 0) /* Request has yet to do anything productive! */
	{
	  /* check for hit in cache or present in MSHRs */
	  hittype_pres_mshr = notpres_mshr(captr,req);
	  
#ifdef DEBUG_PRIMARY
	  hittype = notpres(address,&tag,&set,&set_ind,captr);
	  i1 = set_ind / SUB_SZ;
	  i2 = set_ind % SUB_SZ;
	  
	  if (DEBUG_TIME < YS__Simtime)
	    {
	      fprintf(simout,"2L1: REQ\t%s\t\t&:%ld\tinst_tag:%d\tTag:%ld\tType:%s\thit: %d @%1.0f Size:%d src=%d dest=%d %s\n",
		     captr->name, req->address,
		     req->s.inst_tag,req->tag, Req_Type[req->req_type],
		     hittype, YS__Simtime, req->size_st,req->src_node,req->dest_node,
		     MSHRret[hittype_pres_mshr]);
	    }
#endif

	  switch (hittype_pres_mshr)
	    {
	    case MSHR_COAL: /* coalesced into an MSHR */
#ifdef DEBUG_PRIMARY
	      if (YS__Simtime > DEBUG_TIME)
		fprintf(simout,"2L1: Coalesced request inst_tag=%d \n",req->s.inst_tag);
#endif
	      if (req->s.prefetch)
		{
#ifdef DEBUG_PRIMARY
		  if (DEBUG_TIME < YS__Simtime)
		    {
		      fprintf(simout,"2L1: REQ\t%s\t\tPrefetch:%ld\tUNNEC-COAL @%.1f\n",captr->name,req->address,YS__Simtime);
		    }
#endif
		  captr->stat.pref_total++;
		  captr->stat.pref_unnecessary++;
		}
	      req->handled = reqL1COAL;
	      return 1; /* if MSHR coalesced it, success */
	      break;
	    case MSHR_NEW:
	    case MSHR_FWD: /* need to send down some sort of miss or upgrade */
	      if (req->s.prefetch)
		{
		  /* Calculate some prefetching stats in these cases */
#ifdef DEBUG_PRIMARY
		  if (DEBUG_TIME < YS__Simtime)
		    {
		      fprintf(simout,"2L1: REQ\t%s\t\tPrefetch:%ld\tMSHR_NEW or FWD @%.1f\n",captr->name,req->address,YS__Simtime);
		    }
#endif
		  if (hittype_pres_mshr == MSHR_NEW)
		    {
		      captr->stat.pref_total++;
		    }
		  else if (captr->cache_level_type == FIRSTLEVEL_WT) /* an upgrade type to L1 WT*/
		    {
		      /* We don't want to count a prefetch of any type
			 in this case, since it's not really getting
			 us anything. */
		    }
		  else /* an upgrade to a L1WB cache -- certain to be useful in the sense that cache didn't have it in the exclusive mode yet. */
		    {
		      captr->stat.pref_total++;
		      captr->stat.pref_useful_upgrade++;
		    }
		}
	      req->progress=1; /* this indicates that cache need to send
				  something out, and that we should try to
				  do it */
	      break;
	    case NOMSHR_FWD: /* means REQUEST is a non-write-allocate
				write miss (or perhaps an L2 prefetch) */
	      /* no prefetch stats here, since this access is non-allocating */
#ifdef DEBUG_PRIMARY
	      if (req->s.prefetch)
		{
		  if (YS__Simtime > DEBUG_TIME)
		    fprintf(simout,"2L1: REQ\t%s\t\tPrefetch:%ld\tNOMSHR_FWD @%.1f\n",captr->name,req->address,YS__Simtime);
		}
#endif
	      req->progress=1; /* try to send it out */
	      break;
	    case MSHR_STALL_WAR:
	      /* a write/exclusive request wants to merge with a read MSHR */
#ifdef DEBUG_PRIMARY
	      if (YS__Simtime > DEBUG_TIME)
		fprintf(simout,"2L1: MSHR_Stall WAR for inst_tag=%d \n",req->s.inst_tag);
#endif
	      if (req->s.prefetch)
		{
		  /* prefetch stats and possible prefetch dropping */
		  if (DISCRIMINATE_PREFETCH)
		    {
		      /* just drop the prefetch here. */
#ifdef DEBUG_PRIMARY
		      if (DEBUG_TIME < YS__Simtime)
			{
			  fprintf(simout,"2L1: REQ\t%s\t\tPrefetch:%ld\tDROPPED-WAR @%.1f\n",captr->name,req->address,YS__Simtime);
			}
#endif
		      captr->stat.pref_total++;
		      captr->stat.dropped_pref++;
		      YS__PoolReturnObj(&YS__ReqPool, req);
		      return 1;
		    }
		}
	      return 0; /* failure */
	      break;
	    case MSHR_STALL_COHE:
	      /* a request wants to merge with an MSHR that has a pending
		 COHE message */
#ifdef DEBUG_PRIMARY
	      if (YS__Simtime > DEBUG_TIME)
		fprintf(simout,"2L1: MSHR_Stall COHE for inst_tag=%d \n",req->s.inst_tag);
#endif

	      if (req->s.prefetch)
		{
		  /* prefetch stats and possible prefetch dropping */
		  if (DISCRIMINATE_PREFETCH)
		    {
		      /* just drop the prefetch here. */
#ifdef DEBUG_PRIMARY
		      if (DEBUG_TIME < YS__Simtime)
			{
			  fprintf(simout,"2L1: REQ\t%s\t\tPrefetch:%ld\tDROPPED-COHE @%.1f\n",captr->name,req->address,YS__Simtime);
			}
#endif
		      captr->stat.pref_total++;
		      captr->stat.dropped_pref++;
		      YS__PoolReturnObj(&YS__ReqPool, req);
		      return 1;
		    }
		}
	      return 0; /* failure */
	      break;
	    case MSHR_STALL_WRB:
	      /* a request wants to merge with an MSHR that has a WRB that
		 has not yet issued*/
#ifdef DEBUG_PRIMARY
	      if (YS__Simtime > DEBUG_TIME)
		fprintf(simout,"2L1: MSHR_Stall WRB for inst_tag=%d \n",req->s.inst_tag);
#endif

	      if (req->s.prefetch)
		{
		  /* prefetch stats and possible prefetch dropping */
		  if (DISCRIMINATE_PREFETCH)
		    {
		      /* just drop the prefetch here. */
#ifdef DEBUG_PRIMARY
		      if (DEBUG_TIME < YS__Simtime)
			{
			  fprintf(simout,"2L1: REQ\t%s\t\tPrefetch:%ld\tDROPPED-WRB @%.1f\n",captr->name,req->address,YS__Simtime);
			}
#endif
		      captr->stat.pref_total++;
		      captr->stat.dropped_pref++;
		      YS__PoolReturnObj(&YS__ReqPool, req);
		      return 1;
		    }
		}
	      return 0;
	      break;
	    case MSHR_STALL_COAL:
	      /* too many requests already coalesced to this MSHR */
#ifdef DEBUG_PRIMARY
	      if (YS__Simtime > DEBUG_TIME)
		fprintf(simout,"2L1: MSHR_Stall COAL for inst_tag=%d \n",req->s.inst_tag);
#endif

	      if (req->s.prefetch && DISCRIMINATE_PREFETCH)
		{
		  /* just drop the prefetch here. */
#ifdef DEBUG_PRIMARY
		  if (DEBUG_TIME < YS__Simtime)
		    {
		      fprintf(simout,"2L1: REQ\t%s\t\tPrefetch:%ld\tDROPPED-COAL @%.1f\n",captr->name,req->address,YS__Simtime);
		    }
#endif
		  captr->stat.pref_total++;
		  captr->stat.dropped_pref++;
		  YS__PoolReturnObj(&YS__ReqPool, req);
		  return 1;
		}
	      return 0;
	      break;
	    case MSHR_USELESS_FETCH_IN_PROGRESS:
	      /* a prefetch to a line that already has an outstanding fetch */
	      /* drop this prefetch even without discriminate prefetch on */
	      if (!req->s.prefetch)
		{
		  YS__errmsg("USELESS FETCH should only be seen on prefetch requests.");
		}
#ifdef DEBUG_PRIMARY
	      if (DEBUG_TIME < YS__Simtime)
		{
		  fprintf(simout,"2L1: REQ\t%s\t\tPrefetch:%ld\tFETCHINPROG @%.1f\n",captr->name,req->address,YS__Simtime);
		}
#endif
	      captr->stat.pref_total++;
	      captr->stat.pref_unnecessary++; /* this prefetch had no value */
	      YS__PoolReturnObj(&YS__ReqPool, req);
	      return 1; /* successfully processed */
	      break;
	    case NOMSHR_STALL:
	      /* No MSHRs available for this request */
#ifdef DEBUG_PRIMARY
	      if (YS__Simtime > DEBUG_TIME)
		fprintf(simout,"2L1: No more MSHRs for inst_tag=%d \n",req->s.inst_tag);
#endif

	      if (req->s.prefetch && DISCRIMINATE_PREFETCH)
		{
		  /* just drop the prefetch here. */
#ifdef DEBUG_PRIMARY
		  if (DEBUG_TIME < YS__Simtime)
		    {
		      fprintf(simout,"2L1: REQ\t%s\t\tPrefetch:%ld\tDROPPED-MSHR @%.1f\n",captr->name,req->address,YS__Simtime);
		    }
#endif
		  captr->stat.pref_total++;
		  captr->stat.dropped_pref++;
		  YS__PoolReturnObj(&YS__ReqPool, req);
		  return 1;
		}
	      return 0; /* failure */
	      break;
	    case NOMSHR_STALL_COHE:
	      YS__errmsg("L1 cache shouldn't get NOMSHR_STALL_COHE!!!\n");
	      break;
	    case NOMSHR:
	      /* a hit with no associated forward request */
	      if (req->s.prefetch)
		{
		  /* mark this prefetch as unnecessary */
#ifdef DEBUG_PRIMARY
		  if (DEBUG_TIME < YS__Simtime)
		    {
		      fprintf(simout,"2L1: REQ\t%s\t\tPrefetch:%ld\tUNNEC @%.1f\n",captr->name,req->address,YS__Simtime);
		    }
#endif
		  captr->stat.pref_total++;
		  captr->stat.pref_unnecessary++;
		}
	      req->handled = reqL1HIT;
	      req->miss_type = mtL1HIT;
	      
	      /* send the request back to the processor, as it's done. */
	      GlobalPerformAndHeapInsertAllCoalesced(captr, req);  /* REQs also get freed up here */
	      return 1;
	      break;
	    default:
	      YS__errmsg("Default case in mshr_hittype");
	      break;
	    }
	}
      if (req->progress == 1) /*  we'll send down an upgrade, miss, or
				  non-allocating access */
	{
	  return AddReqToOutQ(captr,req); /* successful if there's
                                             space in the output port;
					     otherwise, return 0 and
					     allow retry */
	}
      break;
    case REPLY:
      /* incoming reply from lower levels */

#ifdef DEBUG_PRIMARY
      if(!req->inuse){
	YS__errmsg("Reply at the L1 cache has already been freed\n");
      }
#endif
      if (req->progress == 0) /* no progress made so far */
	{
#ifdef DEBUG_PRIMARY
	  if (req->req_type != REPLY_SH && req->req_type != REPLY_EXCL && req->req_type != REPLY_EXCLDY && req->req_type != REPLY_UPGRADE)
	    YS__errmsg("L1: Unknown reply type!\n");
#endif
	  reply = req->s.reply; /* what type of reply is it? */
	  hittype = notpres(address,&tag,&set,&set_ind,captr); /* is it in cache already? */
	  i1 = set_ind / SUB_SZ;
	  i2 = set_ind % SUB_SZ;
	  
#ifdef DEBUG_PRIMARY
	  if (DEBUG_TIME < YS__Simtime)
	    {
	      fprintf(simout,"5L1: REPLY\t%s\t\t&:%ld\tinst_tag:%d\tTag:%ld\tType:%s\tReply_State:%s @%1.0f Size:%d src=%d dest=%d \n",
		     captr->name, req->address, req->s.inst_tag,req->tag,
		     Req_Type[req->req_type],
		     Reply_st[req->s.reply], YS__Simtime, req->size_st,req->src_node,
		     req->dest_node);
	    }
#endif
	  
	  if (reply == REPLY) /* only acceptable reply-type at L1 cache */
	    {
	      /* as needed set req->wrb_req and req->invl_req */
	      
	      mshr_num = FindInMshrEntries(captr,req); /* look it up in MSHRs */
	      if(((captr->cache_level_type == FIRSTLEVEL_WT && req->prcr_req_type == WRITE) || req->prcr_req_type == L2WRITE_PREFETCH || req->prcr_req_type == L2READ_PREFETCH) && req->read_with_write != 1)
		{
		  mshr_num = -1;
		}
	      if (mshr_num == -1)
		{
		  /* No MSHR for this access. This must be a non-allocating
		     access,  such as a write with L1WT or an L2 prefetch */
		  if ((captr->cache_level_type == FIRSTLEVEL_WT && req->prcr_req_type == WRITE)
		      || req->prcr_req_type == L2WRITE_PREFETCH
		      || req->prcr_req_type == L2READ_PREFETCH)
		    {
		      /* this reply will be successfully processed no
			 matter what. It is just a coalesced write response
			 or a prefetch that doesn't affect this cache. Send
			 it up to the processor for its perusal (and possibly
			 for checking against MEMBAR fences) . */
		      GlobalPerformAndHeapInsertAllCoalesced(captr, req);
		      return 1;
		    }
		  
		  YS__errmsg("received a reply for a nonexistent MSHR\n");
		  return 1;
		}
	      
	      /* Does the MSHR have a coherence message merged into it?
		 This happens when MSHR receives a certain type of COHE
		 while outstanding. In this case, the line must
		 transition to a different state than the REPLY itself
		 would indicate. Note: such merging is only allowed if
		 the merge will not require a copyback of any sort */
	      pend_cohe = GetCoheReq(captr,req,mshr_num);

	      /* Are there any writes with this MSHR? */
	      writes_present = captr->mshrs[mshr_num].writes_present;
	      
	      if (hittype == 0) /* line present in cache -- upgrade reply */
		{
		  /* Look up the cohe_type for the access */
		  cache_get_cohe_type (captr, hittype, set_ind, address, &cohe_type,
				       &allo_type, &dest_node);

		  cur_state = captr->data[i1][i2].state.st; /* current state of line */
		  captr->cohe_rtn(REPLY, cur_state, allo_type, req->req_type,
				  cohe_type, req->dubref,  &nxt_st, &nxt_mod_req,
				  &req_sz, &rep_sz, &nxt_req, &nxt_req_sz, &allocate); /* call coherence routing (cohe_pr) */

		  if (writes_present && nxt_st != PR_DY)
		    {
		      /* this case arises when the REPLY itself brings back
			 a private clean line (REPLY_EXCL). The write will
			 immediately make it dirty. */
		      if (nxt_st == PR_CL)
			nxt_st = PR_DY;
		      else
			YS__errmsg("Writes present and not PR_CL");
		    }

		  req->invl_req=req->wrb_req=NULL; /* nothing was victimized
						      by this REPLY */

		  captr->data[i1][i2].state.st = nxt_st; /* next state */
		  cur_state = nxt_st; /* Now set that as cur_state for
					 possible use later on */
		  captr->data[i1][i2].state.mshr_out = 0; /* mshr_out is there for upgrades */
		  if (pend_cohe) 
		    {
		      /* call the cohe function (cohe_pr) in order to find
			 out the new state. Since this can't cause a new
			 request to be sent out, use dummy variables there */
		      captr->cohe_rtn(COHE, cur_state, allo_type, pend_cohe,
				      cohe_type, 0,  &nxt_st, &dummy2,
				      &dummy, &dummy, &dummy2, &dummy, &dummy);
		      captr->data[i1][i2].state.st = nxt_st; /* set state */
		      if (captr->data[i1][i2].state.mshr_out)
			{
			  YS__errmsg("should not be evicting a line with mshr_out.\n");
			}
#ifdef DEBUG_PRIMARY
		      if (YS__Simtime > DEBUG_TIME)
			fprintf(simout,"5L1: TAG:%ld Pend cohe: change from %s to %s on reply @%g\n",
			       req->tag, State[cur_state], State[nxt_st], YS__Simtime);
#endif
		 
		    }
		}
	      else /* line not present in cache */
		{
		  if (hittype == 1) /* "present" miss -- COHE miss */
		    {
		      req->invl_req=req->wrb_req=NULL; /* since we didn't kick anything out */
		      premiss_ageupdate(captr,set_ind,req,captr->mshrs[mshr_num].only_prefs);
		      CCD_InsertNewCacheLine(captr->ccd,req->tag); /* add to capacity/conflict detector */
		      StatSet(captr,req,CACHE_MISS_COHE);
		    }
		  else
		    {
		      /* find a victim and updates ages */
		      return_int = miss_ageupdate(req,captr,&set_ind,tag,captr->mshrs[mshr_num].only_prefs);

		      if (return_int == NO_REPLACE) /* no line can be replaced */
			{
			  /* In this case, all lines in set are locked
			     due to upgrades */
			  req->s.l1nack=1; /* we have to retry this line */
			  
			  if (captr->cache_level_type == FIRSTLEVEL_WT)
			    YS__errmsg("L1WT should never see a NO_REPLACE");

			  /* Find out where this request has to go */
			  cache_get_cohe_type (captr, hittype, set_ind,
					       address, &cohe_type,
					       &allo_type, &dest_node);
			  req->src_node = captr->node_num;
			  req->dest_node = dest_node;

			  /* Set the req_type back to the original value */
			  req->req_type = req->prcr_req_type;
#ifdef DEBUG_PRIMARY
			  if (YS__Simtime > DEBUG_TIME)
			    fprintf(simout,"L1WB Nacking reply to Request tag %ld inst_tag %d\n",
				    req->tag,req->s.inst_tag);
#endif
			  req->wrb_req=req->invl_req=NULL; /* nothing was victimized */
			  
			  NackUpgradeConflictReply(captr,req); /* Nack the reply, using request MSHR as a "smart MSHR" */
			  return 1; /* don't stall regardless -- sink the REPLY */			  
			}
			
		      i1 = set_ind/SUB_SZ;
		      i2 = set_ind%SUB_SZ;

		      /* if we're here, we're bringing a line into the
			 cache, so we should determine COLD/CAP/CONF */
		      ccdres = CCD_InsertNewCacheLine(captr->ccd,req->tag);
		  
		      if (req->line_cold)
			{
			  StatSet(captr,req,CACHE_MISS_COLD);
			}
		      else
			{
			  StatSet(captr,req,ccdres); /* either CAP or CONF */
			}
		    }

		  /* Find the coherence type of the line currently held */
		  cache_get_cohe_type (captr, hittype, set_ind, address, &cohe_type,
				       &allo_type, &dest_node);

		  /* the current state of the line being brought in is
		     INVALID, as it is not present in cache */
		  cur_state = INVALID;

		  /* Find next state, etc. */
		  captr->cohe_rtn(REPLY, cur_state, allo_type, req->req_type,
				  cohe_type, req->dubref,  &nxt_st, &nxt_mod_req,
				  &req_sz, &rep_sz, &nxt_req, &nxt_req_sz, &allocate);
		  
		  if (writes_present && nxt_st != PR_DY)
		    {
		      /* this case arises when the REPLY itself brings back
			 a private clean line (REPLY_EXCL). The write will
			 immediately make it dirty. */
		      if (nxt_st == PR_CL)
			nxt_st = PR_DY;
		      else
			YS__errmsg("Writes present and return state is not PR_CL");
		    }

		  req->invl_req=req->wrb_req=NULL; /* by default */
		  if (captr->data[i1][i2].state.st == INVALID)
		    {
		      /* if victim tag is invalid, use the line without
			 doing any replacement */
		      if (captr->data[i1][i2].state.mshr_out)
			{
			  YS__errmsg("should not be replacing line with mshr_out!!!\n");
			}

		      /* Now fill in the information for the new line being
			 brought in */
		      captr->data[i1][i2].tag = tag; /* fill in new tag */
		      captr->data[i1][i2].state.st = nxt_st; /* Change state */
		      captr->data[i1][i2].state.cohe_type = cohe_type;
		      captr->data[i1][i2].state.allo_type = allo_type;
		      captr->data[i1][i2].dest_node = dest_node;
		      cur_state = nxt_st; /* set this for later use */
		      if (pend_cohe)
			{
			  /* call the cohe function (cohe_pr) in order
			     to find out the new state. Since this
			     can't cause a new request to be sent out,
			     use dummy variables there */
			  captr->cohe_rtn(COHE, cur_state, allo_type, pend_cohe,
					  cohe_type, 0,  &nxt_st, &dummy2,
					  &dummy, &dummy, &dummy2, &dummy, &dummy);
			  captr->data[i1][i2].state.st = nxt_st; /* set new state */
#ifdef DEBUG_PRIMARY
			  
			  if (DEBUG_TIME < YS__Simtime)
			    fprintf(simout,"5L2: Pending cohe changes state to %s on reply.\n",
				   State[nxt_st]);
#endif			  
			}
		    }
		  else
		    {
		      /* the line requires replacing some other line from
			 the cache. Find out the cohe_type, allo_type,
			 dest_node, cur_state, and tag of the line being
			 replaced */
		   
		      cohe_type_repl = captr->data[i1][i2].state.cohe_type;
		      allo_type_repl = captr->data[i1][i2].state.allo_type;  
		      dest_node_repl = captr->data[i1][i2].dest_node;
		      cur_state = captr->data[i1][i2].state.st;
		      if (cur_state == SH_CL)
			captr->stat.shcl_victims++;
		      else if (cur_state == PR_CL)
			captr->stat.prcl_victims++;
		      else if (cur_state == PR_DY)
			captr->stat.prdy_victims++;

		      /* Determine if replacement needs to go to next
			 module, etc. by calling cohe_rtn (cohe_pr) */
		      captr->cohe_rtn(REQUEST, captr->data[i1][i2].state.st, allo_type_repl,
				      REPL, cohe_type_repl, 0,  &dummy3, &blw_req_type,
				      &req_sz_blw, &rep_sz_blw, &abv_req_type, &req_sz_abv,
				      &rep_sz_abv);
		      
		      /* tag of replaced line */
		      tag_repl = (captr->data[i1][i2].tag << captr->set_bits) | set ;

		      /* the fact that we've found something to replace
			 indicates that it doesn't have an mshr_out */

		      /* Set tags and states to reflect new line */
		      captr->data[i1][i2].tag = tag; 
		      captr->data[i1][i2].state.cohe_type = cohe_type;
		      captr->data[i1][i2].state.allo_type = allo_type;
		      captr->data[i1][i2].dest_node = dest_node;
		      captr->data[i1][i2].state.st = nxt_st;
		      cur_state = nxt_st; /* set this field for later use */
		      if (captr->data[i1][i2].state.mshr_out)
			{
			  YS__errmsg("line shouldn't have mshr_out in replacement case\n");
			}
		 
		      if(blw_req_type)
			{
			  /* the replacement requires another message
			     (e.g. WRB) to be sent to lower level of cache */
			  
			  if (captr->cache_level_type == FIRSTLEVEL_WT)
			    YS__errmsg("L1WT should never enter blw_req_type on l1cache.c REPLY");

			  /* Need to access next module to replace line */
			  
#ifdef DEBUG_PRIMARY
			  if (DEBUG_TIME < YS__Simtime)
			    fprintf(simout,"5L1: Need to WRB line %d for incoming REPLY.\n",tag_repl);
#endif

			  /* Build a new REQ to send down to the next
			     level of the cache hierarchy with the WRB
			     information */
			  req->wrb_req = GetReplReq(captr, req, tag_repl, blw_req_type,
						    req_sz_blw, rep_sz_blw, cohe_type_repl,
						    allo_type_repl, captr->node_num,
						    dest_node_repl, BLW);
			}
		      if(abv_req_type)
			{
			  /* Does replacement require sending a message to
			     a module above? */
			  YS__errmsg("Should never enter abv_req_type on l1cache.c REPLY");
			}
		      if (pend_cohe)
			{
			  /* call the cohe function (cohe_pr) in order
			     to find out the new state. Since this
			     can't cause a new request to be sent out,
			     use dummy variables there. */

			  captr->cohe_rtn(COHE, cur_state, allo_type, pend_cohe,
					  cohe_type, 0,  &nxt_st, &dummy2,
					  &dummy, &dummy, &dummy2, &dummy, &dummy);
			  captr->data[i1][i2].state.st = nxt_st; /* set new state */
			  if (captr->data[i1][i2].state.mshr_out)
			    {
			      YS__errmsg("shouldn't be replacing  line with mshr_out\n");
			    }
#ifdef DEBUG_PRIMARY
			  if (DEBUG_TIME < YS__Simtime)
			    fprintf(simout,"5L2: Pending cohe changes state to %s on reply.\n",State[nxt_st]);
#endif
			  /* Of course, a very aggressive solution
			     would not have thrown the previous line
			     out if this line is about to be
			     invalidated, since the MSHR knew that
			     this line which came back was about to be
			     invalidated. However, it's not clear that
			     real hardware will be able to service
			     REPLYs from just the MSHR, rather than
			     from the cache */
			}
		    }
		}

	      req->progress=1;
	      req->mshr_num = mshr_num;
	    }
	  else
	    {
	      /* RARs and other REPLY types are not acceptable at L1 cache */
	      YS__errmsg("RARs and other REPLY types not supported by L1");
	      return 1;
	    }
	}
      if (req->progress == 1) /* finish processing the REPLY */
	{
	  mshr_num = req->mshr_num; /* use the MSHR number */
	  miss_type = req->miss_type; 
	  wrb_req = req->wrb_req; /* does the request require a WRB? */
	  if(req->read_with_write == 1)
	    {
	      /* read_with_write is set in the case of accesses
		 that coalesce together in later levels of the memory
		 hierarchy although one type (i.e. writes with a non-write
		 allocate L1 or L2 prefs) doesn't allocate in the L1 cache.
		 It's important to handle read_with_writes separately,
		 as the non-allocating requests must first be processed,
		 after which the MSHR-coalesced requests can be processed */
	      
	      if((req->prcr_req_type != WRITE && req->prcr_req_type != L2WRITE_PREFETCH && req->prcr_req_type != L2READ_PREFETCH) ||
		 (captr->cache_level_type == FIRSTLEVEL_WB && req->prcr_req_type == WRITE))
		YS__errmsg("Read has read_with_write set.\n");

	      /* now send these request types back to the processor (for
		 possible further processing there, such as statistics or
		 MEMBAR fence handling */
	      
	      if (captr->cache_level_type == FIRSTLEVEL_WT)
		GlobalPerformAndHeapInsertAllCoalescedWritesOnly(captr, req); /* this function includes both writes and L2 prefs , REQs also get freed up here */
	      else
		GlobalPerformAndHeapInsertAllCoalescedL2PrefsOnly(captr, req);  /* REQs also get freed up here */
	    }

	  /* Now, resolve all requests coalesced into the MSHR in question.
	     Send them back to the processor for possible further handling. */
	  MSHRIterateUncoalesce(captr,mshr_num,GlobalPerformAndHeapInsertAllCoalesced,miss_type); /* REQs also get freed up here */

#ifdef DEBUG_PRIMARY
	  if (wrb_req)
	    {
	      if (DEBUG_TIME < YS__Simtime)
		fprintf(simout,"5L1: Replacing mshr %d with WRB of line %ld.\n",mshr_num,wrb_req->tag);
	    }
#endif
	  RemoveMSHR(captr,mshr_num,wrb_req); /* free up the MSHR, or replace
						 it with the wrb_req for the
						 line. */
	  if (wrb_req)
	    {
	      AddToSmartMSHRList(captr,wrb_req,mshr_num,RemoveMSHR);
	      /* when wrb_req issues, remove the MSHR. We use the smart
		 MSHR guarantee that we sink this REPLY, without having
		 to wait for anything else. */
	    }
	  return 1; /* REPLY is done here -- always gets sunk */
	}
      break;
    case COHE:
      /* coherence transaction to L1 cache from lower level */
     if (req->progress == 0) /* Progress cases fall through to COHE_REPLY */
       {
	 req->s.type = COHE_REPLY; /* turn around immediately, as there are
				      no higher levels */
	 req->s.dir = REQ_BWD;

	 captr->stat.cohe++;
	 captr->stat.cohe_reply++;
	 
	 req->size_st =  REQ_SZ;
	 req->size_req = REQ_SZ;
	 
	 hittype = notpres(address,&tag,&set,&set_ind,captr); /* is line present in cache? */
	 i1= set_ind / SUB_SZ;
	 i2 = set_ind % SUB_SZ;
	 hittype_pres_mshr = notpres_mshr(captr,req); /* does line match an outstanding MSHR? */
	 
#ifdef DEBUG_PRIMARY
	if (DEBUG_TIME < YS__Simtime)
	  fprintf(simout,"8L1: COHE\t%s\t\t%ld\tTag:%ld\t%s\thittype:%d @%1.0f src=%d dest=%d %s\n",
		 captr->name, req->address, 
		 req->tag, Req_Type[req->req_type], hittype, YS__Simtime,
		 req->src_node,req->dest_node, MSHRret[hittype_pres_mshr]);
#endif
	
	if (hittype_pres_mshr == MSHR_COAL)
	  {
	    /* MSHR_COAL indicates that COHE matched an MSHR and either

	       1) MSHR is for a read and the coherence demands a copyback.
	       In this case, directory considers this cache to be the
	       owner of the line at the time of the message. Thus, the
	       cache either had previously been the owner or is
	       going to be the owner because of an exclusive REPLY.
	       Because of the latter possibility, this response must be a
	       NACK_PEND.
	       
	       2) MSHR is for a read and message does not require a
	       copyback (INVL). In this case, the directory either has
	       already received the read request and considers the
	       cache a sharer (and is therefore invalidating), or is
	       invalidating based on old information (the line may
	       have been a sharer before the REQUEST was sent out). In
	       either case, a conservative thing to do is to send out
	       a positive acknowledgment for the COHE, which we then merge
	       into the MSHR. As soon as the REPLY returns, the line should
	       be invalidated. Ideally, if the cache knew that the INVL was
	       based on old information, it wouldn't have to drop that line,
	       but there's no way of knowing right now.

	       Note that if the COHE is a WRB (L2 replacement of
	       dirty), the response is a NOMSHR return value and a
	       NACK, even though those are also merged into the MSHR
	       
	       3) If the MSHR is for a write miss or upgrade and the
	       COHE demands a data copyback from a write-back cache,
	       the request must be sent back with a NACK_PEND. If the
	       COHE is a type that seeks a data copyback, but the
	       cache is write-through, the request is handled by
	       acknowledging the message and handling it later.
	       */
	       	    
	    if (Speculative_Loads && req->req_type != COPYBACK) /* line is being invalidated */
	      SpecLoadBufCohe(captr->node_num,req->tag, SLB_Cohe); /* check for speculative load violations */
	    
	    if (req->req_type == WRB || req->req_type == REPL)
	      {
		/* should never happen -- these return NOMSHR on merge */
#ifdef DEBUG_PRIMARY
		if (DEBUG_TIME < YS__Simtime)
		  fprintf(simout,"8L2: %s in MSHR_COAL case\n",Req_Type[req->req_type]);
#endif
	      }
	    
	    if (captr->cache_level_type == FIRSTLEVEL_WT || req->s.nack_st == NACK_OK) /* no copyback demanded/possible -- merge the COHE with the MSHR */
	      {
		req->s.reply = REPLY; /* because we're going to send out a positive ACK */
		
		captr->stat.cohe_reply_merge++;
#ifdef DEBUG_PRIMARY
		if (DEBUG_TIME < YS__Simtime)
		  fprintf(simout,"8L1: COHE merged with a MSHR; positive ACK\n");
#endif
	      }
	    else
	      {
		/* a FIRSTLEVEL_WB with NACK_NOK -- we'll have to do a
		   nack_pend, since this wants data. */
		captr->stat.cohe_reply_nack_pend++;
		req->s.reply = NACK_PEND;
		req->size_st = REQ_SZ;
		req->size_req = REQ_SZ;
		
#ifdef DEBUG_PRIMARY
		if (DEBUG_TIME < YS__Simtime)
		  fprintf(simout,"8L1: COHE wants to merge NACK_NOK with a PR_DY MSHR, but we have to NACK_PEND\n");
#endif
	      }
	  }
	else if (hittype_pres_mshr == MSHR_FWD)
	  {
	    /* In this case, the MSHR is for a write miss or upgrade
	       and the COHE did not demand a data copyback. The COHE
	       must have originated before the exclusive request was
	       serviced. Thus, the coherence action can be done
	       immediately and returned with a NACK. If the line was
	       being upgraded, it can still be unlocked and
	       invalidated here: the directory must convert the
	       processor's UPGRADE request to a READ_OWN when it
	       realizes that the processor doesn't actually have the
	       line it wants. */

	    
	    if (req->req_type == WRB)
	      {
		/* In this case, this is an MSHR for a line that had
		   hit in the L2, but is just now (in a race) being
		   replaced from the L2. So, we'll send it down to L2
		   as a NACK_PEND and tell the L2 to resend it */
		req->s.reply = NACK_PEND; 
		req->size_st = REQ_SZ;
		req->size_req = REQ_SZ;
		
#ifdef DEBUG_PRIMARY
		if (DEBUG_TIME < YS__Simtime)
		  fprintf(simout,"8L1: WRB tried to merge NACK_OK with a PR_DY MSHR ; send down a NACK_PEND\n");
#endif
	      }
	    else
	      {
		/* other COHE types */
		req->s.reply = NACK; /* these are going to L2 and will also be handled there */
		req->size_st = REQ_SZ;
		req->size_req = REQ_SZ;
	      }
	    
	    if (req->req_type != COPYBACK) /* some sort of invalidate op */
	      {
		if (Speculative_Loads) /* check for speculative loads */
		  SpecLoadBufCohe(captr->node_num,req->tag,SLB_Cohe);
		
		if (hittype == 0) /* a hit */
		  {		     
		    captr->data[i1][i2].state.st = INVALID;
		    captr->data[i1][i2].state.mshr_out = 0;
		    captr->stat.cohe_reply_nack_docohe++;
#ifdef DEBUG_PRIMARY
		    if (DEBUG_TIME < YS__Simtime)
		      fprintf(simout,"L1: Forcing MSHR_out to be 0 for tag %ld at time %f\n",req->tag,YS__Simtime);
#endif
		  }
		else
		  {
#ifdef DEBUG_PRIMARY
		    if (DEBUG_TIME < YS__Simtime && req->req_type != WRB)
		      fprintf(simout,"8L1: COHE tried to merge NACK_OK with a PR_DY (miss) MSHR ; send back a NACK\n");
#endif
		    captr->stat.cohe_reply_nack_mergefail++;
		  }
	      }
	    else
	      {
		YS__errmsg("L1 MSHR_FWD should never see COPYBACK");
	      }
	  }
	else /* function returns NOMSHR if NOMSHR present, involved,
		or needed. No MSHR mach, among other things. */
	  {
	    /* ok, now check if it's a regular hit */
	    if (hittype != 0) /* some sort of miss */
	      {
#ifdef DEBUG_PRIMARY
		if (DEBUG_TIME < YS__Simtime)
		  fprintf(simout,"8L1: COHE on a miss; NACK\n");
#endif

		/* Note -- even if it's a NACK, it might have
		   formerly been in the L1 and then replaced, so we
		   need to call SLBCohe about it */

		if (Speculative_Loads && req->req_type != COPYBACK)
		  SpecLoadBufCohe(captr->node_num,req->tag,
				  (req->req_type == WRB) ? SLB_Repl : SLB_Cohe);

		req->s.reply = NACK; /* nack it even if it's a WRB -- we don't have data for it. */
		captr->stat.cohe_reply_nack++;
	      }
	    else if (req->req_type == WRB) /* it was a WRB hit */
	      {
		/* a replacement from L2 that also fit in the L1 */
		if (captr->cache_level_type == FIRSTLEVEL_WT)
		  YS__errmsg("L1WT should not get a WRB\n");
		 
		if (Speculative_Loads) /* this is an invalidation */
		  SpecLoadBufCohe(captr->node_num,req->tag, SLB_Repl);

		cur_state = captr->data[i1][i2].state.st;
		if (cur_state != PR_DY)
		  {
		    /* NACK this, this means that we have the line, but we
		       didn't write to it */
#ifdef DEBUG_PRIMARY
		    if (YS__Simtime > DEBUG_TIME)
		      fprintf(simout,"8L1: %s nacking a WRB to a clean for tag %ld\n",captr->name,req->tag);
#endif
		    req->s.reply = NACK;
		    captr->data[i1][i2].state.st = INVALID;
		    if (captr->data[i1][i2].pref)
		      {
			captr->stat.pref_useless_cohe++;
			captr->data[i1][i2].pref = 0;
			captr->data[i1][i2].pref_tag_repl = -1;
		      }
		    captr->stat.cohe_reply_nack++;
		    if (captr->data[i1][i2].state.mshr_out)
		      {
			YS__errmsg("coherence interfering with  line that has  mshr_out!!!\n");
		      }
		  }
		else
		  {
		    /* a real WRB that hits in L1 in dirty state */
#ifdef DEBUG_PRIMARY
		    if (YS__Simtime > DEBUG_TIME)
		      fprintf(simout,"8L1: %s got a real WRB to tag %ld\n",captr->name,req->tag);
#endif
		    req->s.reply = REPLY;
		    req->size_st = LINESZ + REQ_SZ; /* send down a full line */
		    req->size_req = LINESZ + REQ_SZ;		     
		    captr->data[i1][i2].state.st = INVALID; /* set new state */
		    if (captr->data[i1][i2].pref)
		      {
			captr->stat.pref_useless++; /* count this as a replacement, rather than a cohe */
			captr->data[i1][i2].pref = 0;
			captr->data[i1][i2].pref_tag_repl = -1;
		      }
		    if (captr->data[i1][i2].state.mshr_out)
		      {
			YS__errmsg("coherence interfering with line with mshr_out!!!\n");
		      }
		  }
	      }
	    else /* it was a regular non-WRB hit! */
	      {
		req->s.reply = REPLY; /* positive acknowledgment */
		
		/* here's where we need to change the state also */
		cohe_type = captr->data[i1][i2].state.cohe_type;
		cur_state = captr->data[i1][i2].state.st;

		/* Call coherence routine (cohe_pr) to set up nxt_state,
		   determine other needed actions, etc. */
		captr->cohe_rtn(COHE, cur_state, 0, req->req_type, cohe_type,
				req->dubref, 
				&nxt_st, &nxt_mod_req, &req_sz, &rep_sz, &nxt_req,
				&nxt_req_sz,
				&allocate);
		 
		captr->data[i1][i2].state.st = nxt_st; /* set new state */

		if (captr->data[i1][i2].state.mshr_out)
		  {
		    YS__errmsg("Coherence transaction to line with mshr_out!!!\n");
		  }
		 
		if (captr->data[i1][i2].pref && nxt_st == INVALID)
		  {
		    captr->stat.pref_useless_cohe++;
		    captr->data[i1][i2].pref = 0;
		    captr->data[i1][i2].pref_tag_repl = -1;
		  }
		if (captr->data[i1][i2].pref && (cur_state == PR_DY  || cur_state == PR_CL) && (nxt_st == SH_CL))
		  {
		    captr->stat.pref_downgraded++;
		  }
		 
		if (Speculative_Loads && nxt_st == INVALID) /* invalidated; check outstanding speculative loads */
		  SpecLoadBufCohe(captr->node_num,req->tag,SLB_Cohe);
		/* call SLB on a COHE that is positive ACKed */
		if (req_sz >= LINESZ + REQ_SZ) /* this is some sort of copyback case */
		  {
		    if (captr->cache_level_type == FIRSTLEVEL_WT)
		      YS__errmsg("Shouldn't enter copyback case in COHE of l1cache.c");
		    req->s.reply = REPLY;
		    req->size_st = LINESZ + REQ_SZ;
		    req->size_req = LINESZ + REQ_SZ;		     
		     
		    /* in this case we'll need to go to the data pipe at the L2 */
		    req->progress = 1;
		  }
	      }
	  }
       }
     /* NOTE: break left out _intentionally_ so we fall through to the
	actual part where we send it out & stall if necessary*/
    case COHE_REPLY:
#ifdef DEBUG_PRIMARY
      if (DEBUG_TIME < YS__Simtime)
	fprintf(simout,"8L1: COHE_REPLY\t%s\t\t%ld\tTag:%ld\t@%1.0f\n",captr->name, req->address,req->tag,YS__Simtime);
#endif
      return AddReqToOutQ(captr,req); /* successful if there's
					 space in the output port;
					 otherwise, return 0 and
					     allow retry */
      break;
    }

  YS__errmsg("unreachable code in l1cache.c");
  return 0;
}

