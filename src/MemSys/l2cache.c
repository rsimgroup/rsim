/*
  l2cache.c

  Contains the second level cache procedures
  
  The second-level cache is write-back with write-allocate.  The cache
  supports multiple outstanding misses, and is pipelined. Unlike the L1
  cache, the L2 cache is split into a tag RAM array and a data RAM array. 
  
  It is highly recommended that the user be familiar (to some extent)
  with mshr.c, l1cache.c, and pipeline.c before seeking to modify this
  file.  In particular, familiarity with l1cache.c (L1 cache) is assumed,
  as documentation for the principal L2 cache functions only points out
  differences from the L1 cache.

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
#include "MemSys/stats.h"
#include "MemSys/misc.h"
#include "MemSys/net.h"
#include "MemSys/arch.h" /* use this to determine L1TYPE */

#include "Processor/memprocess.h"
#include "Processor/capconf.h"
#include "Processor/simio.h"
#include <malloc.h>

struct state;

int L2TAG_DELAY=3; /* time spent in TAG-RAM pipe */
int L2TAG_PIPES=3; /* number of ports to L2 Tag -- default of 3 (REQUEST, REPLY, COHE)*/
int L2TAG_PORTS[10]={1,1,L2_DEFAULT_NUM_PORTS}; /* width of each individual pipe */

int L2DATA_DELAY=5; /* time spent in DATA-RAM pipe */
int L2DATA_PIPES=1; /* number of ports to L2 Data */
int L2DATA_PORTS[10]={1}; /* width of each individual pipe */

int L2_MAX_MSHRS=8; /* default MSHR count */

int L2_NUM_PORTS = L2_DEFAULT_NUM_PORTS;

int send_dest[MAX_MEMSYS_PROCS]; /* used for remote writes -- not currently supported */

#define ABSORB 1

/* macros which specify which pipeline each type of access belongs to */
#define L2ReqDATAPIPE(req) 0
#define L2ReplyTAGPIPE(req) 1 
#define L2CoheReplyTAGPIPE(req) 1 /* COHE_REPLY and REPLY shared a tag pipe.
				     No possibility of deadlock from this, as
				     these requests are sunk. */
#define L2CoheTAGPIPE(req) 2 /* Note: we have added a separate COHE
				tag pipe in order to avoid
				deadlock. Such a port may be
				considered excessive in a real system;
				thus, it may be advisable to reserve a
				certain number of MSHR-like buffers
				for incoming {\tt COHE} messages and
				simply withhold processing of
				additional {\tt COHE}s until space
				opens up in one of these buffers. */
#define L2ReqTAGPIPE(req) 3 /* Request tag pipe */

static int L2ProcessDataReq(CACHE *, REQ *);
static int L2ProcessTagReq(CACHE *, REQ *);

/* Write-back buffer functions */
/* Note: the wrb_buf is used for sending write-backs and/or
   invalidates to L1 and for sending write-backs down to the bus. This
   is used in conjunction with smart MSHRs */
static int hit_wrb_buf(CACHE *, REQ *);
static void insert_in_wrb_buf(CACHE *, REQ *, REQ *);
static int remove_from_wrb_buf(CACHE *, int,REQ *);
static int replace_in_wrb_buf(CACHE *, int,REQ *);
static int posn_in_wrb_buf(CACHE *, REQ *);
static void MoveWRBToL1(CACHE *, REQ *, int);
static void MoveINVLToL1(CACHE *, REQ *, REQ *);
static int mark_done_data_in_wrb_buf(CACHE *, REQ *, int);
static int check_done_l1_in_wrb_buf(CACHE *, REQ *);
static int mark_done_l1_in_wrb_buf(CACHE *, REQ *);
static void mark_undone_l1_in_wrb_buf(CACHE *, REQ *);
static int hit_markstall_wrb_buf(CACHE *, REQ *);
static int check_stalling_in_wrb_buf(CACHE *, REQ *);

/***************************************************************************/
/* L2CacheInSim: function that brings new transactions from the ports into */
/* the pipelines associated with the various cache parts. An operation can */
/* be removed from its port as soon as pipeline space opens up for it.     */
/* Called whenever there may be something on an input port                 */
/* Behaves like L1CacheInSim except where noted (with NOTE)                */
/***************************************************************************/
void L2CacheInSim(struct state *proc)
{
  REQ *req;
  int i;
  int nothing_on_ports = 0; 
  CACHE *captr;
  ARG    *argptr;

  argptr = (ARG *)GetL2ArgPtr(proc);
  captr = (CACHE *)argptr->mptr; /* Pointer to cache module */
#ifdef COREFILE1
  if(YS__Simtime > DEBUG_TIME)
    fprintf(simout,"%s calling L2CacheInSim %.2f\n",captr->name, YS__Simtime);
#endif

  nothing_on_ports=1;
  for (i=captr->num_ports-1; i >= 0 ; i--)
    {
      while ((req = peekQ(captr->in_port_ptr[i])) != NULL)
	{
	  nothing_on_ports=0;
	  if (req->s.type == REQUEST)
	    {
	      if (AddToPipe(captr->pipe[L2ReqTAGPIPE(req)],req) == 0)
		{
		  req->progress=0;
		  commit_req((SMMODULE *)captr,i); /* does rmQ  */
		  captr->num_in_pipes++;
		  captr->pipe_empty = 0; /* No longer empty */
		}
	      else
		break;
	    }
	  else if (req->s.type == REPLY)
	    {
	      /* NOTE: Unlike the L1 cache, the L2 cache holds a
		 wrb-buf entry for possible use at the time of a REPLY.
		 Actually, this entry was reserved at REQUEST time, as
		 the MSHRs wouldn't send anything out unless there was
		 corresponding space here. This is for a possible
		 victimization */
	      if (captr->wrb_buf_used == wrb_buf_size)
		{
		  YS__errmsg("L2CACHE.C: Should have checked for space in wrb_buf before sending request.");
		}
	      if (AddToPipe(captr->pipe[L2ReplyTAGPIPE(req)],req) == 0)
		{
#ifdef DEBUG_SECONDARY
		  if (YS__Simtime > DEBUG_TIME)
		    fprintf(simout,"%s reserving a WRB space for a reply\n",captr->name);
#endif
		  captr->wrb_buf_used++; /* hold a WRB buffer */
#ifdef DEBUG_SECONDARY
		  if (YS__Simtime > DEBUG_TIME)
		    fprintf(simout,"%s wrb_buf size now %d -- line %d\n",captr->name,captr->wrb_buf_used, __LINE__);
#endif

		  /* CALCULATE "net miss time" right here */
		  /* That's the statistic for time in the network */
		  switch (req->prcr_req_type)
		    {
		    case READ:
		      StatrecUpdate(captr->net_demand_miss[0],YS__Simtime-req->net_start_time,1.0);
		      break;
		    case WRITE:
		      StatrecUpdate(captr->net_demand_miss[1],YS__Simtime-req->net_start_time,1.0);
		      break;
		    case RMW:
		      StatrecUpdate(captr->net_demand_miss[2],YS__Simtime-req->net_start_time,1.0);
		      break;
		    case L1WRITE_PREFETCH:
		      StatrecUpdate(captr->net_pref_miss[0],YS__Simtime-req->net_start_time,1.0);
		      break;
		    case L1READ_PREFETCH:
		      StatrecUpdate(captr->net_pref_miss[1],YS__Simtime-req->net_start_time,1.0);
		      break;
		    case L2WRITE_PREFETCH:
		      StatrecUpdate(captr->net_pref_miss[2],YS__Simtime-req->net_start_time,1.0);
		      break;
		    case L2READ_PREFETCH:
		      StatrecUpdate(captr->net_pref_miss[3],YS__Simtime-req->net_start_time,1.0);
		      break;
		    case WRB:
		      /* do nothing */
		      break;
		    default:
		      break;
		    }
	      
		  req->progress=0;
		  commit_req((SMMODULE *)captr,i);
		  captr->num_in_pipes++;
		  captr->pipe_empty = 0; /* No longer empty */
		}
	      else
		break;
	    }
	  else if (req->s.type == COHE)
	    {
	      if (AddToPipe(captr->pipe[L2CoheTAGPIPE(req)],req) == 0)
		{
		  req->progress=0;
		  commit_req((SMMODULE *)captr,i);
		  captr->num_in_pipes++;
		  captr->pipe_empty = 0;
		}
	      else
		break;
	    }
	  else if (req->s.type == COHE_REPLY)
	    {
	      /* NOTE: Some COHE_REPLYs from the L1 are absorbed at the L2
		 cache. These have no further action at the L2. Such
		 COHE_REPLYs include invalidations for subset-enforcement */
	      if(req->absorb_at_l2 == ABSORB){
		commit_req((SMMODULE *)captr, i);
		YS__PoolReturnObj(&YS__ReqPool, req);
		break;
	      }

	      if (AddToPipe(captr->pipe[L2CoheReplyTAGPIPE(req)],req) == 0)
		{
		  req->progress=0;
		  commit_req((SMMODULE *)captr,i);
		  captr->num_in_pipes++;
		  captr->pipe_empty = 0; /* No longer empty */
		}
	      else
		break;
	    }
	  else
	    {
	      YS__errmsg("Unknown req type");
	    }
	}
    }
  if(nothing_on_ports){
    captr->inq_empty = 1;
  }
  return;
}

/***************************************************************************/
/* L2CacheOutSim: initiates actual processing of various REQ types         */
/* Called each cycle that there is something in pipes to be processed      */
/* (even if not at the head of the pipes -- the pipes also need to be      */
/* cycled). Exactly like L1CacheOutSim except where noted.                 */
/***************************************************************************/

void L2CacheOutSim(struct state *proc)
{
  REQ *req;
  int pipenum;
  int ctr;
  Pipeline *pipe;
  CACHE *captr;
  ARG    *argptr;
  int result;
  
  argptr = (ARG *)GetL2ArgPtr(proc);
  captr = (CACHE *)argptr->mptr; /* Pointer to cache module */
#ifdef COREFILE
  if(YS__Simtime > DEBUG_TIME)
    fprintf(simout,"%s calling L2CacheOutSim %.2f \n",captr->name, YS__Simtime);
#endif

  /* NOTE: L2 is not a monolithic piece of SRAM like the L1. The L2 is
     split into a "tag SRAM" array and a "data SRAM" array. So, this function
     needs to look at both tag pipes and data pipes. It looks at data pipes
     first, as tag pipes might push something into data pipes, but never
     the other way around. */
     
  for (pipenum=0; pipenum< L2DATA_PIPES; pipenum++)
    {
      pipe = captr->pipe[pipenum];
      ctr=0;
      while ((req=GetPipeElt(pipe,ctr)) != NULL) /* once NULL, we're done */
	{
	  if (L2ProcessDataReq(captr, req))
	    {
	      ClearPipeElt(pipe,ctr); /* if it succeeds */
	      captr->num_in_pipes--;
	      if(captr->num_in_pipes == 0)
		captr->pipe_empty = 1;
	    }
	  ctr++;
	}
      CyclePipe(pipe);
    }

  if (captr->SmartMSHRHead) /* Smart MSHRs include MSHRs and write-back buffers */
    ProcessFromSmartMSHRList(captr,1); /* right now, we can only do 1 */
  
  for (pipenum=0; pipenum< L2TAG_PIPES; pipenum++) /* now loop through tag pipes */
    {
      pipe = captr->pipe[pipenum+L2DATA_PIPES];
      ctr=0;
      while ((req=GetPipeElt(pipe,ctr)) != NULL) /* once NULL, we're done */
	{
	  result = L2ProcessTagReq(captr,req);
	  if (result == 1) /* if it succeeds */
	    {
	      ClearPipeElt(pipe,ctr); 
	      captr->num_in_pipes--;
	      if(captr->num_in_pipes == 0)
		captr->pipe_empty = 1;
	    }
	  if (result == -1) /* NOTE: special return value: instead of
			       removing, this return value indicates
			       that the entry should be _replaced_ */
	    {
	      /* this is used in $-$ transfer to send out the relevant
		 ack/copyback _after_ sending out the $-$ transfer
		 response */
	      SetPipeElt(pipe,ctr,req->invl_req);
	      break;
	    }
	  ctr++;
	}
      CyclePipe(pipe);
    }

  captr->utilization += 1.0;
  if (L1TYPE == FIRSTLEVEL_WT)
    WBSim(proc, L2WB); /* This is called to push REPLYs on up immediately. */
  return;
}

/***************************************************************************/
/* L2ProcessDataReq: simulates accesses to the L2 data SRAM array,         */
/* Behavior depends on some extent to the type of message (i.e. s.type)    */
/* received, but is significantly less variable than L2ProcessTagReq.      */
/* Returns 0 on failure; 1 on success. No equivalent function in L1.       */
/***************************************************************************/
   
int L2ProcessDataReq(CACHE *captr, REQ *req)
{
  switch (req->s.type)
    {
    case REQUEST:
      /* this is a cache hit that gets sent back to the L1 */
      {
	  if (req->progress == 0)
	    {
	      /* do global performs of all coalesced operations */
	      GlobalPerformAllCoalesced(captr, req); 
	      req->s.dir = REQ_BWD; /* change direction */
	      req->s.type = REPLY; /* change to REPLY */
	      req->s.reply = REPLY; /* positive REPLY */
	      req->progress++; /* set progress so as not to repeat */
	    }
	  /* Now, see if this access can be sent back to L1 */
	  return AddReqToOutQ(captr,req); /* this returns 0 on failure, 1 on success, same as this function */
	  /* if AddReqToOutQ fails, request will be awoken in REPLY
	     case, which just seeks to send up access also. */
	}
	break;
    case REPLY:
      {
	/* GlobalPerforms were already done when the MSHR was removed
	   (or, if this was a hit, when this was in REQUEST state in
	   data SRAM pipeline). */

	/* Just try to send this request up to L1 (or, in the case of $-$
	   xfers, down to bus) */
	return AddReqToOutQ(captr,req); /* this returns 0 on failure, 1 on success */
      }
      break;
    case COHE: /* this is an upward-bound WRB  --
		  they are the only COHEs that come to the Data array */
      {
	/* call MoveWRBToL1 to send the request up to the L1 cache,
	   possibly taking advantage of the wrb-buf as a smart
	   MSHR. Thus, this access will not block the pipe, and cannot
	   cause a deadlock here. */
	
	if (L1TYPE == FIRSTLEVEL_WB && req->req_type == WRB)
	  MoveWRBToL1(captr,req,1); 
	else
	  YS__errmsg("Unknown COHE type in L2 data pipe.\n");
	  
	return 1; /* always be able to sink this! */
      }
      break;
    case COHE_REPLY:
      {
	/* This can be an unsolicited L1 writeback to L2 (which is
	   absorbed), a WRB-INVL clubbing (if the L1 cache is WT,
	   the INVL for subset enforcement to the L1 is sent from here,
	   as well as the WRB to memory), or an external COHE with
	   copyback (sent down to memory). */

	if (req->absorb_at_l2 == ABSORB) /* an L1 writeback to L2, for example */
	  {
#ifdef DEBUG_SECONDARY
	    if (YS__Simtime > DEBUG_TIME)
	      fprintf(simout,"%s: Digesting a WRB message %ld from L1 in Data pipe\n",captr->name,req->tag);
#endif
	    YS__PoolReturnObj(&YS__ReqPool, req); /* we need to digest it */
	    return 1;
	  }

	/* This access may be a INVL-WRB clubbing. In that case,
	   use MoveINVLToL1 to send the INVL request up to the L1 cache and
	   the WRB request down to memory. This is parallel to the
	   use of MoveWRBToL1 in the COHE case above. */
	if (L1TYPE==FIRSTLEVEL_WT && req->req_type == WRB)
	  {
	    MoveINVLToL1(captr,req->invl_req,req);
	    return 1;
	  }
	else if (req->req_type == WRB)
	  {
	    YS__errmsg("Invalid COHE_REPLY type for L2ProcessDataReq.");
	    return 1;
	  }
	else /* ordinary coherence action */
	  return AddReqToOutQ(captr,req); /* this returns 0 on failure, 1 on success, same as us */
      }
    default:
      YS__errmsg("Invalid case in L2ProcessDataReq");
      return 1;
      break;
    }
}

/***************************************************************************/
/* L2ProcessTagReq: simulates accesses to the L2 cache tag array.          */
/* Its behavior depends on the type of message (i.e. s.type) received      */
/* This is very similar to L1ProcessTagReq, except where noted.            */
/* Returns 0 on failure; 1 on success; -1 on replace                       */
/***************************************************************************/


int L2ProcessTagReq(CACHE *captr, REQ *req)
{
  int i, hittype, type, data_type, reply;
  int req_sz, rep_sz, nxt_req_sz;
  long tag, address;
  int set, set_ind, i1, i2;
  int cohe_type, allo_type, allocate,  dest_node;
  int hittype_pres_mshr, temp;
  ReqType nxt_mod_req,nxt_req ;
  int    req_sz_abv, rep_sz_abv, req_sz_blw, rep_sz_blw;
  CacheLineState cur_state, nxt_st, dummy3;
  ReqType abv_req_type, blw_req_type, dummy2;
  int    allo_type_repl, cohe_type_repl, dest_node_repl, dummy;
  long tag_repl;
  int return_int,pend_cohe;
  int wasnack;
  int writes_present;
  REQ **req_array;
  enum CacheMissType ccdres;

  address = req->address;
  type = req->req_type;
  data_type = req->address_type;
    
  req->tag = address >> captr->block_bits;
  req->linesz = captr->linesz; 
  
  switch (req->s.type)
    {
    case REQUEST:
      if (req->progress == 0) /* Request has yet to do anything productive! */
	{
	  wasnack =0;
	  /* NOTE: The L2 cache must handle REQUESTs from L1 that were
	           already sent as REPLYs, but NACKed because of too many
		   upgrades outstanding. However, there is no special
		   handling needed for these (except for statistics) */
	  if (req->s.l1nack)
	    {
	      wasnack = 1;
	      req->s.l1nack = 0;
	    }
	  /* NOTE: If an incoming REQUEST matchs a WRB-buf entry, it should
	     be stalled until the WRB leaves the L2 cache. This allows
	     the cache to avoid some very difficult races (if a REPLY comes
	     back before the WRB has had a chance to be sent out, etc.) */
	  if (hit_markstall_wrb_buf(captr,req))
	    {
#ifdef DEBUG_SECONDARY
	      if (DEBUG_TIME < YS__Simtime)
		{
		  fprintf(simout,"2L2: REQ\t%s\t\t&:%ld\tinst_tag:%d\tTag:%ld\tStalled on WRB\n",
			  captr->name, req->address,
			  req->s.inst_tag,req->tag);
		}
#endif
	      captr->stat.pipe_stall_WRB_match++;
	      return 0; /* stall any request that matches in this,
			   so that cache never ends up with a reply that
			   matches in this... */
	    }

	  /* NOTE: L2 cache has to explicitly call notpres in order to
	     obtain set index and state for hits and thereby send
	     appropriate REPLY types (REPLY_EXCL or REPLY_SH) to L1
	     cache */
	  hittype = notpres(address,&tag,&set,&set_ind,captr);
	  i1 = set_ind / SUB_SZ;
	  i2 = set_ind % SUB_SZ;
	  hittype_pres_mshr = notpres_mshr(captr,req);
	  
#ifdef DEBUG_SECONDARY
	  if (DEBUG_TIME < YS__Simtime)
	    {
	      fprintf(simout,"2L2: REQ\t%s\t\t&:%ld\tinst_tag:%d\tTag:%ld\tType:%s\thit: %d @%1.0f Size:%d src=%d dest=%d %s\n",
		     captr->name, req->address,
		     req->s.inst_tag,req->tag, Req_Type[req->req_type], hittype,
		     YS__Simtime, req->size_st,req->src_node,req->dest_node,
		     MSHRret[hittype_pres_mshr]);
	    }
#endif
	  /* NOTE: L2 cache does not have option of dropping prefetches
	     as L1 can */
	  switch (hittype_pres_mshr)
	    {
	    case MSHR_COAL:
#ifdef DEBUG_SECONDARY
	      if (YS__Simtime > DEBUG_TIME)
		fprintf(simout,"2L2: Coalesced request inst_tag=%d \n",req->s.inst_tag);
#endif
	      req->handled=reqL2COAL;
	      if (req->s.prefetch && !wasnack)
		{
#ifdef DEBUG_SECONDARY
		  if (DEBUG_TIME < YS__Simtime)
		    {
		      fprintf(simout,"2L2: REQ\t%s\t\tPrefetch:%ld\tUNNEC-COAL @%.1f\n",captr->name,req->address,YS__Simtime);
		    }
#endif
		  captr->stat.pref_total++;
		  captr->stat.pref_unnecessary++;
		}
	      return 1; /* success if coalesced */
	      break;
	    case NOMSHR_FWD:
	    case NOMSHR_PFBUF:
	      YS__errmsg("Not in secondary cache\n");
	    case MSHR_NEW:
	    case MSHR_FWD: /* send down some sort of miss or upgrade */
	      if (req->s.prefetch && !wasnack)
		{
#ifdef DEBUG_SECONDARY
		  if (DEBUG_TIME < YS__Simtime)
		    {
		      fprintf(simout,"2L2: REQ\t%s\t\tPrefetch:%ld\tMSHR_NEW or FWD @%.1f\n",captr->name,req->address,YS__Simtime);
		    }
#endif
		  captr->stat.pref_total++;
		  if (hittype_pres_mshr == MSHR_FWD) /* an upgrade -- certain to be useful in the sense that cache didn't have it in the exclusive mode yet. */
		    captr->stat.pref_useful_upgrade++;
		}
	      req->progress=1; /* this indicates that cache need to send
				  something out, and that we should try to
				  do it */
	      break;
	    case MSHR_STALL_WAR:
#ifdef DEBUG_SECONDARY
	      if (YS__Simtime > DEBUG_TIME)
		fprintf(simout,"2L2: MSHR_Stall WAR for inst_tag=%d \n",req->s.inst_tag);
#endif
	      return 0;
	      break;
	    case MSHR_STALL_COHE:
#ifdef DEBUG_SECONDARY
	      if (YS__Simtime > DEBUG_TIME)
		fprintf(simout,"2L2: MSHR_Stall COHE for inst_tag=%d \n",req->s.inst_tag);
#endif

	      return 0;
	      break;
	    case MSHR_STALL_COAL:
#ifdef DEBUG_SECONDARY
	      if (YS__Simtime > DEBUG_TIME)
		fprintf(simout,"2L2: MSHR_Stall COAL for inst_tag=%d \n",req->s.inst_tag);
#endif

	      return 0;
	      break;
	    case MSHR_STALL_WRB:
#ifdef DEBUG_SECONDARY
	      if (YS__Simtime > DEBUG_TIME)
		fprintf(simout,"2L2: MSHR_Stall WRB for inst_tag=%d \n",req->s.inst_tag);
#endif

	      return 0;
	      break;
	    case NOMSHR_STALL:
#ifdef DEBUG_SECONDARY
	      if (YS__Simtime > DEBUG_TIME)
		fprintf(simout,"2L2: No more MSHRs for inst_tag=%d \n",req->s.inst_tag);
#endif

	      return 0;
	      break;
	    case NOMSHR_STALL_COHE:
	      /* NOTE: There is currently a COHE transaction being
		 processed for this line, so this REQ cannot be
		 serviced yet */
#ifdef DEBUG_SECONDARY
	      if (YS__Simtime > DEBUG_TIME)
		fprintf(simout,"2L2: Hit, but can't service for a pending COHE inst_tag=%d \n",req->s.inst_tag);
#endif
	      return 0;
	      break;
	    case NOMSHR_STALL_WRBBUF_FULL:
#ifdef DEBUG_SECONDARY
	      if (YS__Simtime > DEBUG_TIME)
		fprintf(simout,"2L2: Can't provide MSHR for inst_tag=%d because no space available in WRB BUF \n",req->s.inst_tag);
#endif
	      return 0;
	      break;
	    case MSHR_USELESS_FETCH_IN_PROGRESS:
	      YS__errmsg("USELESS FETCH is not acceptable at the L2.");
	      return 1;
	      break;
	    case NOMSHR:
	      /* going to Data RAM in this case */
	      /* note that this isn't considered progress because no
		 resource has been booked, nor has any visible state been
		 changed (although LRU bits may have been). */
	      /* NOTE: hits must be sent to the data array before being
		 sent to L1 cache */
	      if (AddToPipe(captr->pipe[L2ReqDATAPIPE(req)],req) == 0)
		{
		  if (captr->data[i1][i2].state.st == PR_CL || captr->data[i1][i2].state.st == PR_DY)
		    req->req_type = REPLY_EXCL; /* L1 caches need to handle
						   REPLY_EXCL as REPLY_UPGRADE
						   also, since REQUESTs aren't
						   distinguished here. */
		  else
		    req->req_type = REPLY_SH;
		    
		  req->progress=0;
		  if (req->s.prefetch && !wasnack)
		    {
#ifdef DEBUG_SECONDARY
		      if (DEBUG_TIME < YS__Simtime)
			{
			  fprintf(simout,"2L2: REQ\t%s\t\tPrefetch:%ld\tUNNEC @%.1f\n",captr->name,req->address,YS__Simtime);
			}
#endif
		      captr->stat.pref_total++;
		      captr->stat.pref_unnecessary++;
		    }
		  captr->num_in_pipes++;
		  req->handled=reqL2HIT;
		  req->miss_type = mtL2HIT;
		  captr->pipe_empty = 0; /* No longer empty */
		  return 1; /* succeeded, it's in Data pipe */
		}
	      else
		return 0;
	      break;
	    default:
	      YS__errmsg("Default case in mshr_hittype");
	      break;
	    }
	}
      if (req->progress == 1) /*  cache sends down an upgrade or miss */
	{
	  req->net_start_time = YS__Simtime;
	  return AddReqToOutQ(captr,req); /* returns 0 on failure, 1 on success */
	}
      break;
    case REPLY:
#ifdef DEBUG_SECONDARY
      if(!req->inuse){
	YS__errmsg("Freed reply has been received by L2 cache");
      }
      if (hit_wrb_buf(captr,req))
	YS__errmsg("Cache should never get a reply at the L2 to tag that matches the WRB buffer");
#endif

      if (req->progress == 0)
	{
#ifdef DEBUG_SECONDARY
	  if (req->req_type != REPLY_SH && req->req_type != REPLY_EXCL && req->req_type != REPLY_EXCLDY && req->req_type != REPLY_UPGRADE && req->s.reply != RAR)
	    YS__errmsg("L2: Unknown reply type!\n");
#endif
      
	  reply = req->s.reply;
	  hittype = notpres(address,&tag,&set,&set_ind,captr);
	  i1 = set_ind / SUB_SZ;
	  i2 = set_ind % SUB_SZ;
	  
#ifdef DEBUG_SECONDARY
	  if (DEBUG_TIME < YS__Simtime)
	    {
	      fprintf(simout,"5L2: REPLY\t%s\t\t&:%ld\tinst_tag:%d\tTag:%ld\tType:%s\tReply_State:%s @%1.0f Size:%d src=%d dest=%d\n",
		     captr->name, req->address, req->s.inst_tag,req->tag, Req_Type[req->req_type],
		     Reply_st[req->s.reply], YS__Simtime, req->size_st,req->src_node,req->dest_node);
	    }
#endif
	  
	  if (reply == RAR)
	    {
	      /* NOTE: L2 cache can receive RARs from the directory. This
		 means that this access needs to be retried. The access
		 can be retried using the smart-MSHR already reserved
		 for the REQUEST. Thus, the actual sending of the RETRY
		 can be decoupled from the reply handling, and will not
		 cause an unwanted dependence */
#ifdef DEBUG_SECONDARY
	      if (YS__Simtime > DEBUG_TIME)
		fprintf(simout,"%s Sending RAR at %g for inst_tag %d tag %ld\n",captr->name, YS__Simtime,req->s.inst_tag,req->tag);
#endif
	      
	      /* Prepare to send the REQUEST back down to the directory */
	      req->s.dir = REQ_FWD;
	      req->s.route = BLW;
	      req->s.type = REQUEST;
	      req->mshr_num = FindInMshrEntries(captr,req);
	      if (req->mshr_num == -1)
		{
		  YS__errmsg("Received RAR for a nonexistent MSHR\n");
		  return 1;
		}
	      if (req->src_node != captr->node_num) /* flipped src and dest */
		{
		  temp = req->src_node;
		  req->src_node = req->dest_node;
		  req->dest_node = temp;
		}

	      captr->stat.rars_handled++;
	      
	      AddToSmartMSHRList(captr,req,req->mshr_num,NULL); /* Use smart MSHR to handle this */

#ifdef DEBUG_SECONDARY
	      if (YS__Simtime > DEBUG_TIME)
		fprintf(simout,"%s freeing a WRB space for a RAR %ld\n",captr->name,req->tag);
#endif
	      captr->wrb_buf_used--; /* since this was an RAR, no need to keep
					the wrb_buf that was held (note that
					wrb_buf entry is still booked for it;
					it just isn't active until the regular
					REPLY comes in. */
#ifdef DEBUG_SECONDARY
	      if (YS__Simtime > DEBUG_TIME)
		fprintf(simout,"%s wrb_buf size now %d -- line %d\n",captr->name,captr->wrb_buf_used, __LINE__);
#endif
	      return 1;
	    }
	  else if (reply == REPLY)
	    {
	      if(req->req_type == WRB){
		YS__errmsg("WRBs should not send replys in this system");
		return 1;
	      }
	      /* as needed set req->wrb_req, req->invl_req for victimization */
	      
	      req->mshr_num = FindInMshrEntries(captr,req);
	      if (req->mshr_num == -1)
		{
		  YS__errmsg("received a reply for a nonexistent MSHR\n");
		  return 1;
		}

	      /* Does the MSHR have a coherence message merged into it?
		 This happens when MSHR receives a certain type of COHE
		 while outstanding. In this case, the line must
		 transition to a different state than the REPLY itself
		 would indicate. Note: such merging is only allowed if
		 the merge will not require a copyback of any sort */
	      pend_cohe = GetCoheReq(captr,req,req->mshr_num);

	      /* Are there any writes with this MSHR? If so, make
	       sure to transition to dirty.... */
	      writes_present = captr->mshrs[req->mshr_num].writes_present;
	      
	      if (hittype == 0) /* this was for an upgrade */
		{
		  cache_get_cohe_type (captr, hittype, set_ind, address, &cohe_type,
				       &allo_type, &dest_node);
		  cur_state = captr->data[i1][i2].state.st;
		  captr->cohe_rtn(REPLY, cur_state, allo_type, req->req_type,
				  cohe_type, req->dubref, &nxt_st, &nxt_mod_req,
				  &req_sz, &rep_sz, &nxt_req, &nxt_req_sz, &allocate);
		  
		  if (writes_present && nxt_st != PR_DY)
		    {
		      if (nxt_st == PR_CL)
			nxt_st = PR_DY;
		      else
			YS__errmsg("Writes present and reply state is PR_CL");
		    }

		  req->invl_req = req->wrb_req = NULL; /* by default */

		  /* NOTE: non-replacing replies do not need wrb_buf entry
		     being held */
		    
#ifdef DEBUG_SECONDARY
		  if (YS__Simtime > DEBUG_TIME)
		    fprintf(simout,"%s freeing a WRB space for non-replacing reply %ld\n",captr->name,req->tag);
#endif
		  captr->wrb_buf_used--; /* no longer need this */
#ifdef DEBUG_SECONDARY
		  if (YS__Simtime > DEBUG_TIME)
		    fprintf(simout,"%s wrb_buf size now %d -- line %d\n",captr->name,captr->wrb_buf_used, __LINE__);
#endif

		  captr->data[i1][i2].state.st = nxt_st;
		  cur_state = nxt_st;
		  captr->data[i1][i2].state.mshr_out = 0;

		  if (pend_cohe)
		    {
		      if (cur_state == PR_CL || cur_state == PR_DY)
			{
			  if (cur_state == PR_DY)
			    YS__errmsg("Pend cohe on PR_DY response!");
			  
			  /* NOTE: Recall category 2 of the first MSHR-COHE
			     merger handled in mshr.c . If this reply
			     comes back in PRIVATE state, that means the COHE
			     can be ignored, as the COHE was sent by the
			     directory based on old information. Therefore,
			     the cache now ignores the COHE */
			     
#ifdef DEBUG_SECONDARY
			  if (YS__Simtime > DEBUG_TIME)
			    fprintf(simout,"5L2: TAG:%ld Ignoring merged pend cohe %s because of private reply %s @%g\n",
				   req->tag, Req_Type[pend_cohe],Req_Type[req->req_type], YS__Simtime);
#endif
			  captr->stat.cohe_reply_merge_ignore++;
			}
		      else
			{
			  captr->cohe_rtn(COHE, cur_state, allo_type,
					  pend_cohe,cohe_type, 0,  &nxt_st,
					  &dummy2,&dummy, &dummy,
					  &dummy2, &dummy, &dummy);
			  captr->data[i1][i2].state.st = nxt_st;
			  if (captr->data[i1][i2].state.mshr_out)
			    {
			      YS__errmsg("replacing line with mshr_out.\n");
			    }
#ifdef DEBUG_SECONDARY
			  if (YS__Simtime > DEBUG_TIME)
			    fprintf(simout,"5L2: TAG:%ld Pend cohe: change from %s to %s on reply @%g\n",
				   req->tag, State[cur_state], State[nxt_st], YS__Simtime);
#endif
			}
		    }
		}
	      else
		{
		  if (hittype == 1) /* present miss */
		    {
		      premiss_ageupdate(captr,set_ind,req,captr->mshrs[req->mshr_num].only_prefs);
		      CCD_InsertNewCacheLine(captr->ccd,req->tag); /* add to capacity/conflict detector */
		      StatSet(captr,req,CACHE_MISS_COHE);
		      /* NOTE: these will always be non-replacing
			 REPLIES, so the wrb_buf_used will get
			 decremented below */
		    }
		  else /* hittype == 2 : total miss */
		    {
		      return_int = miss_ageupdate(req,captr,&set_ind,tag,captr->mshrs[req->mshr_num].only_prefs);
		      /* finds a victim and updates ages */

		      if (return_int == NO_REPLACE)
			{
			  /* NOTE: If NACK because of too many upgrades,
			     L2 cache sets s.preprocessed before bouncing
			     whole line back to directory. */
			  req->s.preprocessed = 1;
			  cache_get_cohe_type (captr, hittype, set_ind,
					       address, &cohe_type,
					       &allo_type, &dest_node);
			  req->src_node = captr->node_num;
			  req->dest_node = dest_node;

#ifdef DEBUG_SECONDARY
			  if (YS__Simtime > DEBUG_TIME)
			    fprintf(simout,"L2 Nacking reply to Request tag %ld inst_tag %d\n",
				    req->tag,req->s.inst_tag);
#endif
						  
			  NackUpgradeConflictReply(captr,req);

#ifdef DEBUG_SECONDARY
			  if (YS__Simtime > DEBUG_TIME)
			    fprintf(simout,"%s freeing a WRB space for nacked reply %ld\n",captr->name,req->tag);
#endif
			  captr->wrb_buf_used--; /* we don't need it, so we're done */
#ifdef DEBUG_SECONDARY
			  if (YS__Simtime > DEBUG_TIME)
			    fprintf(simout,"%s wrb_buf size now %d -- line %d\n",captr->name,captr->wrb_buf_used, __LINE__);
#endif

			  return 1;
			}
			
	 	      i1 = set_ind/SUB_SZ;
		      i2 = set_ind%SUB_SZ;
		  
		      ccdres = CCD_InsertNewCacheLine(captr->ccd,req->tag);

		      if (req->line_cold)
			{
			  StatSet(captr,req,CACHE_MISS_COLD);
			}
		      else
			{
			  StatSet(captr,req,ccdres);
			}
		    }
		  
		  cache_get_cohe_type (captr, hittype, set_ind, address, &cohe_type,
				       &allo_type, &dest_node);
		  cur_state = INVALID;
		  captr->cohe_rtn(REPLY, cur_state, allo_type, req->req_type,
				  cohe_type, req->dubref, &nxt_st, &nxt_mod_req,
				  &req_sz, &rep_sz, &nxt_req, &nxt_req_sz, &allocate);
	      
		  if (writes_present && nxt_st != PR_DY)
		    {
		      if (nxt_st == PR_CL)
			nxt_st = PR_DY;
		      else
			YS__errmsg("Writes present and not PR_CL");
		    }

		  req->invl_req=req->wrb_req=NULL; /* by default */
		  captr->data[i1][i2].state.cohe_pend=0; /* clear this bit for the new line */
		  if (captr->data[i1][i2].state.st == INVALID)
		    {
		      if (captr->data[i1][i2].state.mshr_out)
			{
			  YS__errmsg("replacing line with mshr_out.\n");
			}
		      /* if victim tag is invalid, use the line */
		      if (captr->data[i1][i2].state.mshr_out)
			{
			  YS__errmsg("replacing line with mshr_out.\n");
			}
		      captr->data[i1][i2].tag = tag; /* put in the new tag */
		      captr->data[i1][i2].state.st = nxt_st; /* Change state */
		      captr->data[i1][i2].state.cohe_type = cohe_type;
		      captr->data[i1][i2].state.allo_type = allo_type;
		      captr->data[i1][i2].dest_node = dest_node;
		      cur_state = nxt_st;
		      if (pend_cohe)
			{
			  if (cur_state == PR_CL || cur_state == PR_DY)
			    {
			      if (cur_state == PR_DY)
				YS__errmsg("Pend cohe on PR_DY response!");
			      /* NOTE: Recall category 2 of the first
				 MSHR-COHE merger handled in mshr.c
				 If this reply comes back in PRIVATE
				 state, that means the COHE can be
				 ignored, as the COHE was sent by the
				 directory based on old
				 information. Therefore, the cache now
				 ignores the COHE */
			     
#ifdef DEBUG_SECONDARY
			      if (YS__Simtime > DEBUG_TIME)
				fprintf(simout,"5L2: TAG:%ld Ignoring merged pend cohe %s because of private reply %s @%g\n",
				       req->tag, Req_Type[pend_cohe],Req_Type[req->req_type], YS__Simtime);
#endif	
			      captr->stat.cohe_reply_merge_ignore++;
			    }
			  else
			    {
			      captr->cohe_rtn(COHE, cur_state, allo_type, pend_cohe,
					      cohe_type, 0, &nxt_st, &dummy2,
					      &dummy, &dummy, &dummy2, &dummy, &dummy);
			      captr->data[i1][i2].state.st = nxt_st;
#ifdef DEBUG_SECONDARY
			      
			      if (DEBUG_TIME < YS__Simtime)
				fprintf(simout,"5L2: Pending cohe changes state to %s on reply.\n",
				       State[nxt_st]);
#endif			  
			    }
			}

		      /* NOTE: non-replacing replies always free up the
			 WRB-buf entry held for them */
		      
#ifdef DEBUG_SECONDARY
		      if (YS__Simtime > DEBUG_TIME)
			fprintf(simout,"%s freeing a WRB space for non-replacing reply %ld\n",captr->name,req->tag);
#endif
		      captr->wrb_buf_used--; /* we don't need it, so we're done */
#ifdef DEBUG_SECONDARY
		      if (YS__Simtime > DEBUG_TIME)
			fprintf(simout,"%s wrb_buf size now %d -- line %d\n",captr->name,captr->wrb_buf_used, __LINE__);
#endif
		    }
		  else
		    {
		      /* In this case, some sort of replacement is needed */
		      /* Gather all information about line being replaced */
		      cohe_type_repl = captr->data[i1][i2].state.cohe_type;
		      allo_type_repl = captr->data[i1][i2].state.allo_type;  
		      dest_node_repl = captr->data[i1][i2].dest_node;
		      tag_repl = (captr->data[i1][i2].tag << captr->set_bits) | set ;
		      cur_state = captr->data[i1][i2].state.st;
		      if (cur_state == SH_CL)
			captr->stat.shcl_victims++;
		      else if (cur_state == PR_CL)
			captr->stat.prcl_victims++;
		      else if (cur_state == PR_DY)
			captr->stat.prdy_victims++;
#ifdef DEBUG_SECONDARY
		      if(YS__Simtime > DEBUG_TIME)
			fprintf(simout, "5L2: REPL %s Tag:%ld (%ld) in state %s at %g\n",
				captr->name, tag_repl, captr->data[i1][i2].tag,
				State[captr->data[i1][i2].state.st], YS__Simtime);
#endif
		      /* Determine if replaced needs to go to next module etc
			 by calling cohe_rtn (cohe_sl) */
		      captr->cohe_rtn(REQUEST, captr->data[i1][i2].state.st, allo_type_repl,
				      REPL, cohe_type_repl, 0, &dummy3, &blw_req_type,
				      &req_sz_blw, &rep_sz_blw, &abv_req_type, &req_sz_abv,
				      &rep_sz_abv);
		      
		      /* Set tags and states to reflect new line */
		      captr->data[i1][i2].tag = tag; 
		      captr->data[i1][i2].state.cohe_type = cohe_type;
		      captr->data[i1][i2].state.allo_type = allo_type;
		      captr->data[i1][i2].dest_node = dest_node;
		      captr->data[i1][i2].state.st = nxt_st;
		      if (captr->data[i1][i2].state.mshr_out)
			{
			  YS__errmsg("replacing line with mshr_out!!!\n");
			}

		      /* NOTE: The actions taken by the L2 cache on
			 a replacement depend to some extent on the L1
			 cache above it */
		      
		      if (L1TYPE == FIRSTLEVEL_WB)
			{
			  if (cur_state == SH_CL) /* cur_state is the state of the replaced line right now */
			    {
			      /* NOTE: If the {\tt REPLY} replaces a
				 shared line, the cache sends a subset
				 enforcement invalidation to the L1
				 cache, possibly using the write-back
				 buffer as a smart MSHR until the
				 invalidation can be sent. */
#ifdef DEBUG_SECONDARY
			      if (YS__Simtime > DEBUG_TIME)
				fprintf(simout,"%s using a WRB space for INVL on reply to %ld that replaces clean line %ld\n",captr->name,req->tag,tag_repl);
#endif

			      /* NOTE: need to replace line in included
				 caches if any */
			      
			      /* Generate a coherence message to replace
				 line */
			      req->invl_req = GetReplReq(captr, req, tag_repl, abv_req_type,
							 req_sz_abv, rep_sz_abv, cohe_type_repl,
							 allo_type_repl, captr->node_num,
							 dest_node_repl, ABV);
			      /* This is just an invalidation that has
				 to be sent to the L1 alone and
				 absorbed when it returns to L2 */
			      req->invl_req->absorb_at_l2 = ABSORB;

#ifdef DEBUG_SECONDARY
			      if (DEBUG_TIME < YS__Simtime)
				fprintf(simout,"5L2: Need to invalidate line %ld for incoming REPLY.\n",tag_repl);
#endif
			      insert_in_wrb_buf(captr,req->invl_req,NULL);
			    }
			  else /* line is private, possibly dirty */
			    {
			      /* NOTE: If the REPLY causes a
				 replacement of an exclusive line, the
				 write-back buffer space may be used
				 for data as well. If the L1 cache is
				 write-back, the write-back first
				 passes through the L2 data array if
				 the line in L2 is held in dirty
				 state, then to the L1 cache as a {\tt
				 COHE} message. The next {\tt WRB}
				 coherence reply from the L1 is used
				 to either replace the data currently
				 held for the line in the write-back
				 buffer (on a positive acknowledgment)
				 or to inform the cache that the L1
				 cache did not have the desired data
				 (on a {\tt NACK}). */
			      
			      req->wrb_req = GetReplReq(captr, req, tag_repl, WRB,
							req_sz_abv, rep_sz_abv, cohe_type_repl,
							allo_type_repl, captr->node_num,
							dest_node_repl, ABV);
			      /* send the above request _explicitly_ as a WRB
				 even if the L2 has it in PR_CL,
				 since the L1 may have it in PR_DY */
			      if (cur_state == PR_CL)
				{
				  req->wrb_req->s.prclwrb = 1;
				  /* this is used to indicate that this
				     WRB can skip over the L2 Data pipe
				     access; however it may need to access
				     the L1 anyway */
				}
#ifdef DEBUG_SECONDARY
			      if (DEBUG_TIME < YS__Simtime)
				fprintf(simout,"5L2: Need to invalidate line %ld for incoming REPLY -- may lead to L1WB.\n",tag_repl);
#endif
			      insert_in_wrb_buf(captr,req->wrb_req,NULL);
			    }
			  cur_state = nxt_st;
			}
		      else /* FIRSTLEVEL_WT above this cache */
			{
			  if (L1TYPE == FIRSTLEVEL_WB)
			    YS__errmsg("Should not enter this code for L1WB\n");
			  if(blw_req_type)
			    {
			      /* NOTE: Need to access next module to
				 replace line (ie. Write back/REPL of
				 private line)
				 If the L1 cache is write-through, an
				 invalidation message is sent up to
				 the L1 cache (possibly using the
				 write-back buffer as a smart MSHR),
				 after which the write-back or
				 replacement message tries to issue
				 from the write-back buffer to the
				 port below it, also using the smart
				 MSHR list if necessary. */
				 
			  
			      req->wrb_req = GetReplReq(captr, req, tag_repl, blw_req_type,
							req_sz_blw, rep_sz_blw, cohe_type_repl,
							allo_type_repl, captr->node_num,
							dest_node_repl, BLW);
#ifdef DEBUG_SECONDARY
			      if (DEBUG_TIME < YS__Simtime)
				fprintf(simout,"5L2: Need to writeback/replace line %ld for incoming REPLY@ %g\n",
				       tag_repl, YS__Simtime);
#endif
			    }

			  
			  if(abv_req_type)
			    {
			      /* NOTE: need to invalidate in L1 cache. Can
				 be either for shared or private access.
				 In either case, wrb_buf is used as smart MSHR
				 to send up access. It's only a question of
				 whether both abv & blw are clubbed into smart
				 MSHR (private), or if only abv is present
				 (shared) */
			      
			      /* Produce a coherence message */
			      req->invl_req = GetReplReq(captr, req, tag_repl, abv_req_type,
							 req_sz_abv, rep_sz_abv, cohe_type_repl,
							 allo_type_repl, captr->node_num,
							 dest_node_repl, ABV);
			      if(cur_state == SH_CL){
				/* This is just an invalidation that
				   has to be sent to the L1 alone and
				   abosrbed when it gets back */
				req->invl_req->absorb_at_l2 = ABSORB;
			      }
			      else if(cur_state == PR_CL || cur_state == PR_DY){
				/* This will have to be sent on to the
				   directory as a WRB request */
				req->invl_req->absorb_at_l2 = ABSORB;
			      }
			      else {
				YS__errmsg("Unknown state in L2 for line being invalidated above for incoming REPLY.");
			      }
#ifdef DEBUG_SECONDARY
			      if (DEBUG_TIME < YS__Simtime)
				fprintf(simout,"5L2: Need to invalidate line %ld for incoming REPLY.\n",tag_repl);
#endif
			    }

			  if (abv_req_type)
			    {
			      /* need to put the INVL, and possibly also the
				 WRB into wrb-buf */
			      insert_in_wrb_buf(captr,req->invl_req,req->wrb_req);
			    }
			  else if (blw_req_type)
			    {
			      /* need to put the WRB into wrb-buf */
			      insert_in_wrb_buf(captr,req->wrb_req, NULL);
			    }
			  else /* neither blw or abv req */
			    {
			      /* no write-back or invl needed */
			      YS__errmsg("L2 cache does not allow replacement that has neither abv nor blw req.");
			    }
			  
			  cur_state = nxt_st;
			}
		      if (pend_cohe)
			{
			  if (cur_state == PR_CL || cur_state == PR_DY)
			    {
			      if (cur_state == PR_DY)
				YS__errmsg("Pend cohe on PR_DY response!");
			      /* NOTE: Recall category 2 of the first
				 MSHR-COHE merger handled in mshr.c
				 If this reply comes back in PRIVATE
				 state, that means the COHE can be
				 ignored, as the COHE was sent by the
				 directory based on old
				 information. Therefore, the cache now
				 ignores the COHE */
			     
#ifdef DEBUG_SECONDARY
			      if (YS__Simtime > DEBUG_TIME)
				fprintf(simout,"5L2: TAG:%ld Ignoring merged pend cohe %s because of private reply %s @%g\n",
				       req->tag, Req_Type[pend_cohe],Req_Type[req->req_type], YS__Simtime);
#endif
			      captr->stat.cohe_reply_merge_ignore++;
			    }
			  else
			    {
			      captr->cohe_rtn(COHE, cur_state, allo_type, pend_cohe,
					      cohe_type, 0, &nxt_st, &dummy2,
					      &dummy, &dummy, &dummy2, &dummy, &dummy);
			      captr->data[i1][i2].state.st = nxt_st;
			      if (captr->data[i1][i2].state.mshr_out)
				{
				  YS__errmsg("replacing line with mshr_out.\n");
				}
#ifdef DEBUG_SECONDARY
			      if (DEBUG_TIME < YS__Simtime)
				fprintf(simout,"5L2: Pending cohe changes state to %s on reply.\n",State[nxt_st]);
#endif
			    }
			}
		    }
		}
	      req->progress=1;
	    }
	  else
	    {
	      YS__errmsg("L2: unexpected reply type");
	      return 1;
	    }
	}
      if (req->progress == 1) /* NOTE: ready to send out an INVL upward, if any */
	{
	  if (req->invl_req && !req->wrb_req) /* NOTE: cases with both invl
						 and wrb_req are handled after
						 data pipe access for wrb_req
						 if WRB, or separately below
						 for REPL */
	    {
	      /* NOTE: this message does not need a data pipe access
		 here, so try to send it directly, using smart MSHR to
		 avoid stalling. */
	      MoveINVLToL1(captr,req->invl_req,NULL);
	    }
	  req->progress = 2;
	}
      if (req->progress == 2) /* NOTE: ready to send out a WRB to Data pipe or a REPL down, if any */
	{
	  if (req->wrb_req) /* is there a downward message? */
	    {
	      if (req->wrb_req->req_type == WRB) /* is it a WRB? */
		{
		  if (!req->wrb_req->s.prclwrb)
		    {
		      /* In this case, we're sending a WRB downward (in
			 case of L1WT) or we're doing the data
			 victimization access for a dirty line before
			 sending a message upward to L1WB */

		      if (req->invl_req) /* in case of L1 WT */
			req->wrb_req->invl_req = req->invl_req; /* set this so that it will be picked up in COHE case of data pipe */
		      
		      if (AddToPipe(captr->pipe[L2ReqDATAPIPE(req->wrb_req)],req->wrb_req) != 0)
			/* couldn't go to Data pipe */
			return 0;
		      else
			{
			  req->wrb_req->progress=0;
			  captr->num_in_pipes++;captr->pipe_empty = 0; /* No longer empty */
			}
		    }
		  else /* This is a victimization that doesn't need an L2
			  data pipe access, but may need something at the
			  L1 */
		    {
		      MoveWRBToL1(captr,req->wrb_req,0);
		      captr->stat.wb_prcl_inclusions_sent++;
		    }
		}
	      else /* it is a simple REPL without WRB */
		{
		  /* In this case, it shouldn't have to go to Data
		     pipe -- send it out immediately. This access has
		     a space in the wrb_buf and we must sink it to
		     avoid deadlock*/
		  if (req->invl_req) /* this can happen with L1WT */
		    MoveINVLToL1(captr,req->invl_req,req->wrb_req); /* will try to move both INVL and WRB */
		  else if (AddReqToOutQ(captr,req->wrb_req)) /* try to send it out */
		    remove_from_wrb_buf(captr,posn_in_wrb_buf(captr,req->wrb_req),NULL);
		  else /* use smart mshrs to send it out and remove wrb_buf entry */
		    AddToSmartMSHRList(captr,req->wrb_req,posn_in_wrb_buf(captr,req->wrb_req),remove_from_wrb_buf);
		}
	    }


	  /* Now, cache is ready to remove the MSHR. */

	  /* NOTE: before removing the MSHR, cache must account for
	     multiple coalesced accesses to the same line and send
	     them as a unit to the L1 */
	  
	  if (captr->mshrs[req->mshr_num].counter){
	    /* more than one request coalesced to the same line */
	    /* note that system must not only account for the
	       requests coalesced at the MSHR, but also the ones
	       which were coalesced at the WBuf */
	    int coalcount,j,posn;
	    
	    coalcount = req->coal_count;
	    /* two passes; one to count, and one to actually coalesce */
	    
	    for (i=0; i<captr->mshrs[req->mshr_num].counter; i++)
	      {
		coalcount += 1 + captr->mshrs[req->mshr_num].coal_req[i]->coal_count; 
	      }

	    /* Now that number coalesced has been counted, allocate
	       an array of that size and fill in coalesced REQs */
	    
	    req_array = (REQ **)malloc(sizeof(REQ *)*coalcount);
	    posn=0;
	    for (i=0; i<req->coal_count; i++)
	      {
		req_array[posn++]=req->coal_req_array[i];
	      }
	    
	    for (i=0; i<captr->mshrs[req->mshr_num].counter; i++)
	      {
		REQ *coalreq=captr->mshrs[req->mshr_num].coal_req[i];
		req_array[posn++]=coalreq;
		for (j=0; j < coalreq->coal_count; j++)
		  req_array[posn++]=coalreq->coal_req_array[j];

		/* If any of the coalesced requests had requests coalesce
		   at the WBuf, free the array that indicates those */
		if (coalreq->coal_req_array) 
		  {
		    free(coalreq->coal_req_array);
		    coalreq->coal_count=0;
		    coalreq->coal_req_array=NULL;
		  }
	      }
	    
	    if (posn != coalcount)
	      YS__errmsg("L2 cache has problem accounting for coalesced requests");
	    
	    if (req->coal_req_array)  /* If req had any requests coalesced
					 with it at the WRB, free the array
					 now */
	      {
		free(req->coal_req_array);
	      }
	    
	    req->coal_count=coalcount;
	    req->coal_req_array = req_array;
	  }
	      	  
	  /* now Cache is ready to do a full GlobalPerform of this
	     REQ, everything coalesced onto this REQ, each req on
	     MSHR, and everything coalesced onto each such request
	     Also, free up the MSHR. */
	  MSHRIterateUncoalesce(captr,req->mshr_num,GlobalPerformAllCoalesced,req->miss_type);
	  RemoveMSHR(captr,req->mshr_num,NULL);
	  req->progress = 4; /* ready to send REPLY to data pipe */
	}
      if (req->progress == 4) 
	{
	  if (AddToPipe(captr->pipe[L2ReqDATAPIPE(req)],req) != 0) /* couldn't go to Data pipe */
	    return 0;
	  else
	    {
	      req->progress=0;
	      captr->num_in_pipes++;captr->pipe_empty = 0; /* No longer empty */
	      /* after Data pipe, REPLY will go up to L1 */
	    }
	    	  
	  return 1;
	}
      YS__errmsg("Unknown state in REPLY at L2\n");
      break;
    case COHE:
      /* NOTE: When COHEs come to L2 cache, L2 just calls notpres and
	 notpres_mshr to find out if these lines are present. If not,
	 COHE is immediately NACKed. Otherwise, the access is first
	 sent on to L1 and is only processed at the L2 on the
	 COHE_REPLY path. */
     if (req->progress == 0)
       {
	 hittype = notpres(address,&tag,&set,&set_ind,captr);
	 i1= set_ind / SUB_SZ;
	 i2 = set_ind % SUB_SZ;
	 hittype_pres_mshr = notpres_mshr(captr,req);

	 captr->stat.cohe++;
	 
#ifdef DEBUG_SECONDARY
	 if (DEBUG_TIME < YS__Simtime)
	   fprintf(simout,"8L2: COHE\t%s\t\t%ld\tTag:%ld\t%s\thittype:%d @%1.0f src=%d dest=%d %s\n",
		  captr->name, req->address, 
		  req->tag, Req_Type[req->req_type], hittype, YS__Simtime,
		  req->src_node,req->dest_node,
		  MSHRret[hittype_pres_mshr]);
#endif

	 if (hittype == 0)
	   {
	     /* NOTE: Cache must be careful not to service any REQs
		from this line between now and the actual action, as
		that could easily lead to problems such as inclusion
		failures. So, cache sets a cohe_pend bit on the line */
	     if (captr->data[i1][i2].state.cohe_pend) /* If cohe_pend already set. */
	       {
#ifdef DEBUG_SECONDARY
		 if (YS__Simtime > DEBUG_TIME)
		   fprintf(simout,"Cache %s stalling COHE to tag %ld on cohe_pend @%.1f",captr->name,req->tag,YS__Simtime);
#endif
		 captr->stat.cohe_stall_PEND_COHE++;
		 return 0; /* Don't allow 2 cohe_pend's to the same line */
	       }
	     else
	       {
#ifdef DEBUG_SECONDARY
		 if (YS__Simtime > DEBUG_TIME)
		   fprintf(simout,"Cache %s marking tag %ld with cohe_pend @%.1f",captr->name,req->tag,YS__Simtime);
#endif
		 captr->data[i1][i2].state.cohe_pend=1;
	       }
	   }

	 if (hittype_pres_mshr == MSHR_COAL || hittype_pres_mshr == MSHR_FWD)
	   /* a possible MSHR merge. Can be handled on COHE_REPLY */
	   {
	     req->progress = 7; /* Send up to L1 */
	   }
	 else /* function returns NOMSHR if NOMSHR involved */
	   {
	     if (hittype != 0) /* some sort of miss -- can be NACKed */
	       {
#ifdef DEBUG_SECONDARY
		 if (DEBUG_TIME < YS__Simtime)
		   {
		     fprintf(simout,"8L2: COHE on a miss; NACK\n");
		     if (req->forward_to != -1)
		       {
			 fprintf(simout,"8L2: COHE\t%s\tFORWARD NACKED!, tag=%ld\n",
				captr->name,req->tag);
		       }
		   }
#endif
		   
		 captr->stat.cohe_nack++;
		 if (req->forward_to != -1)
		   captr->stat.cohe_cache_to_cache_fail++;
		 req->s.reply = NACK; /* inform directory that this was NACKed */
		 req->s.type = COHE_REPLY; 
		 req->s.dir = REQ_BWD;
		 req->progress = 8;
	       }
	     else /* access was a hit! */
	       {
		 /* send the access to L1 cache; change state of line, etc.
		    only after L1 COHE_REPLY */
		 req->progress = 7;
	       }
	   }
       }
     if (req->progress == 7) /* send a COHE upward */
       {
	 return AddReqToOutQ(captr,req); /* can stall until space available in
					    port to L1 */
       }
     if (req->progress == 8) /* send a NACK downward */
       {
	 req->s.type = COHE_REPLY;    /* set this to COHE_REPLY to route to 
				         correct port */
	 if (AddReqToOutQ(captr,req)) /* COHE_REPLY sent out successfully */
	   return 1;                  /* return success */
	 req->s.type = COHE;          /* set back to COHE in order to awaken */
	 return 0;                    /* transaction in correct place */
       }
     break;
    case COHE_REPLY:
      /* this case is where actions for invalidates, write-backs, etc.
	 actually take place*/
      /* NOTE: cache must also unset the cohe_pend bit on lines here */
      if (req->s.reply == NACK_PEND && req->progress == 0 && req->req_type != WRB)
	{
	  /* NOTE: propagate NACK_PEND responses from L1 */

	  /* but first be sure to unset the cohe_pend bit! */
	  hittype = notpres(address,&tag,&set,&set_ind,captr);
	  i1= set_ind / SUB_SZ;
	  i2 = set_ind % SUB_SZ;

	  if (hittype == 0 && captr->data[i1][i2].state.cohe_pend)
	    {
	      captr->data[i1][i2].state.cohe_pend=0;
#ifdef DEBUG_SECONDARY
	      if (YS__Simtime > DEBUG_TIME)
		fprintf(simout,"Cache %s unmarking cohe_pend on tag %ld on NACK_PEND propogated @%.1f\n",captr->name,req->tag,YS__Simtime);
#endif
	    }
	  
	  captr->stat.cohe_reply_prop_nack_pend++;
	  req->s.reply = NACK_PEND;
	  req->size_st = REQ_SZ;
	  req->size_req = REQ_SZ;

#ifdef DEBUG_SECONDARY
	  if (DEBUG_TIME < YS__Simtime)
	    fprintf(simout,"8L2: COHE propagating NACK_PEND from L1 tag %ld @ %.1f\n",req->tag,YS__Simtime);
#endif
	  /* directory should either retry this or reevaluate its choice */
	  req->progress = 8; /* go to case where we send down NACK_PEND */
	}

     if (req->progress == 0)
       {
	 captr->stat.cohe_reply++;

	 hittype = notpres(address,&tag,&set,&set_ind,captr);
	 i1= set_ind / SUB_SZ;
	 i2 = set_ind % SUB_SZ;
	 hittype_pres_mshr = notpres_mshr(captr,req);


	 
#ifdef DEBUG_SECONDARY
	 if (DEBUG_TIME < YS__Simtime)
	   fprintf(simout,"8L2: COHE_REPLY\t%s\t\t%ld\tTag:%ld\t%s\thittype:%d @%1.0f src=%d dest=%d %s\n",
		  captr->name, req->address, 
		  req->tag, Req_Type[req->req_type], hittype, YS__Simtime,
		  req->src_node,req->dest_node,
		  MSHRret[hittype_pres_mshr]);
#endif

	 /* NOTE: unmark cohe_pend for COHE_REPLY */
	  if (hittype == 0 && captr->data[i1][i2].state.cohe_pend)
	    {
	      captr->data[i1][i2].state.cohe_pend=0;
#ifdef DEBUG_SECONDARY
	      if (YS__Simtime > DEBUG_TIME)
		fprintf(simout,"Cache %s unmarking cohe_pend on tag %ld on COHE_REPLY @%.1f\n",captr->name,req->tag,YS__Simtime);
#endif
	    }
	  
	 if (hittype_pres_mshr == MSHR_COAL)
	   {
	     if (req->s.nack_st == NACK_OK)
	       {
		 /* this case occurs when the MSHR is for a line in SH_CL
		    state and the directory sends up this invalidate message.
		    Since no copyback is implied or needed, the access can be
		    positively ACKed here. It might be ignored at the
		    REPLY as appropriate (category 2 of the first MSHR_COAL
		    case in mshr.c) */
		    
		 req->s.reply = REPLY;
		 
		 req->size_st  = REQ_SZ;
		 req->size_req = REQ_SZ;
		 
#ifdef DEBUG_SECONDARY
		 if (DEBUG_TIME < YS__Simtime)
		   fprintf(simout,"8L2: COHE merged with/ignored by (read/write) MSHR; positive ACK\n");
#endif
		 
		 captr->stat.cohe_reply_merge++;
		 req->progress = 8;
	       }
	     else if (req->s.nack_st == NACK_NOK)
	       {
		 /* a NACK_NOK specifically implies a copyback demand 
		    Only possible response from here is to NACK_PEND
		    (discussed in l1cache.c and mshr.c) 
		    */

		 captr->stat.cohe_reply_nack_pend++;
		 req->s.reply = NACK_PEND;/*IF MERGE   REPLY */ /* Change to PEND */
		 req->size_st = REQ_SZ; /*  IF MERGE   REQ_SZ+LINESZ; */
		 req->size_req = REQ_SZ; /* IF MERGE   REQ_SZ+LINESZ;*/

#ifdef DEBUG_SECONDARY
		 if (DEBUG_TIME < YS__Simtime)
		   fprintf(simout,"8L2: COHE wants to merge NACK_NOK with a PRivate MSHR, but we have to NACK_PEND\n");
#endif
		 
		 req->progress = 8; /* go to case where we send down NACK */
	       }
	     else
	       {
		 YS__errmsg("unknown nack_st");
		 return 1;
	       }

	   }
	 else if (hittype_pres_mshr == MSHR_FWD)
	   {
	     /* handled in same way as discussed in l1cache.c and mshr.c */
	     
	     req->s.reply = NACK;
	     req->size_st  = REQ_SZ;
	     req->size_req = REQ_SZ;

	     if (hittype != 0) /* a miss */
	       {
#ifdef DEBUG_SECONDARY
		 if (DEBUG_TIME < YS__Simtime)
		   fprintf(simout,"8L2: COHE tried to merge NACK_OK with a PR_DY (miss) MSHR ; send back a NACK\n");
#endif
		 captr->stat.cohe_reply_nack_mergefail++;
	       }
	     else
	       {
		 /* if a hit, do the coherence action right now also */
		 /* the coherence action must be an invalidate, since the
		    directory thinks cache has the line in SH_CL */
		 if(req->req_type == COPYBACK){
		   YS__errmsg("COPYBACK message not valid in this case\n");
		 }
		 captr->stat.cohe_reply_nack_docohe++;
		 captr->data[i1][i2].state.st = INVALID;
		 captr->data[i1][i2].state.mshr_out = 0;
#ifdef DEBUG_SECONDARY
		 if (DEBUG_TIME < YS__Simtime)
		   fprintf(simout,"L2: Forcing MSHR_out to be 0 for tag %ld at time %f\n",req->tag,YS__Simtime);
#endif
		 if (captr->data[i1][i2].pref)
		   {
#ifdef DEBUG_SECONDARY
		     if (DEBUG_TIME < YS__Simtime)
		       {
			 fprintf(simout,"8L2: COHE_REPLY\t%s\t\tPrefetch:%ld\tINVALIDATED! @%.1f\n",captr->name,req->address,YS__Simtime);
		       }
#endif
		     captr->stat.pref_useless_cohe++;
		     captr->data[i1][i2].pref = 0;
		     captr->data[i1][i2].pref_tag_repl = -1;
		   }
	       }
	     req->progress = 8;
	   }
	 else if (hittype_pres_mshr == NOMSHR)
	   /* function returns NOMSHR if no MSHR involved */
	   {
	     /* ok, now check if it's a regular hit */
	     if (hittype != 0)
	       /* some sort of miss  -- this can occur in cases of L2
		  replacements (either this COHE itself was a replacement,
		  or a replacement came in between the COHE and the COHE_REPLY) */
	       {
		 if (req->req_type == WRB) /* a WRB replacement message */
		   {
		     /* NOTE: this entire section is different from the L1 */
		     if (hit_wrb_buf(captr,req)) /* is there a wrb-buf entry */
		       {
			 if (check_done_l1_in_wrb_buf(captr,req)) /* if so, then L1 path was done already */
			   {
			     /* another, perhaps unsolicited write-back
				request from the L1 cache has already come
				along and satisfied this. Thus, this
				reply should be a NACK of some sort. */
			     if (req->s.reply == REPLY)
			       {
				 YS__errmsg("ACK in the race condition where both caches replace at same time and the line has already been done at the L1 (should have been NACK)\n");
			       }
#ifdef DEBUG_SECONDARY
			     if (YS__Simtime > DEBUG_TIME)
			       fprintf(simout,"8L2: two cache-replace race with L1 done early on WRB %ld in wrb_buf\n",req->tag);
#endif
			     /* free up the response, as it contributes
				nothing */
			     YS__PoolReturnObj(&YS__ReqPool,req);
			     return 1; /* successfully handled */
			   }
			 /* L1 wasn't already done before this */
			 if (req->s.reply == NACK)
			   {
			     /* the L1 didn't have it, so don't do anything with the reply */
#ifdef DEBUG_SECONDARY
			     if (YS__Simtime > DEBUG_TIME)
			       fprintf(simout,"8L2: WRB COHE-REPLY from L1 (NACK)\n");
#endif
			   }
			 else if (req->s.reply == NACK_PEND)
			   {
			     /* There are two cases here.
				
				In the first case, there is a request
				stalling on this WRB here.  This
				implies that the L1 cache couldn't
				possibly have provided data for this
				WRB. As a result, we should send this
				WRB on down.  The request will get
				sent down as soon as it sees there's
				no more entry in the wrb_buf

				In the second case, there is no
				request stalling on this WRB
				here. Thus, all we need to do is
				resend this WRB back to L1.  If we
				directly try to send it through this
				same request, we have a
				COHE_REPLY->COHE dependence, and
				that's clearly unacceptable. So, we'll
				send it through the WRB_BUF, where
				we've already booked resources.
				So, this is effectively another
				SmartMSHR case */

			     if (check_stalling_in_wrb_buf(captr,req))
			       {
				 /* case 1 above -- treated just like a NACK */
#ifdef DEBUG_SECONDARY
				 if (YS__Simtime > DEBUG_TIME)
				   fprintf(simout,"8L2: WRB COHE-REPLY from L1 (NACK_PEND that's like a NACK)\n");
#endif
			       }
			     else
			       {
				 /* case 2 above -- repeat the WRB message */
#ifdef DEBUG_SECONDARY
				 if (DEBUG_TIME < YS__Simtime)
				   fprintf(simout,"8L2: WRB needs to repeat on NACK_PEND\n");
#endif
				 
				 req->s.type=COHE; /* set up the transaction */
				 req->s.dir=REQ_FWD; /* as demand WRB again */
				 
				 req->size_st=REQ_SZ;
				 req->size_req=REQ_SZ+LINESZ;
				 
				 if (!AddReqToOutQ(captr,req))
				   AddToSmartMSHRList(captr,req,-1,NULL); /* consider the reserved WRB buffer to be a smart MSHR */
				 
				 captr->stat.wb_repeats++;
				 
				 return 1; /* sink this! */
			       }
			   }
			 else /* the response was a WRB REPLY -- L1 provides data */
			   {
			     /* fill in the line from the reply */
#ifdef DEBUG_SECONDARY
			     if (YS__Simtime > DEBUG_TIME)
			       fprintf(simout,"8L2: WRB COHE-REPLY from L1 (ACK)\n");
#endif
			     captr->stat.wb_inclusions_real++;
			   }
			 if (mark_done_l1_in_wrb_buf(captr,req))
			   {
			     /* now, both the L2 and L1 portions
				(including data accesses) of the WRB
				have been completed. So, this WRB is
				ready to be sent out.  Note that mark_done_l1
				may have changed this WRB to a REPL, if
				the line was actually strictly clean. */
			     
			     req->s.dir = REQ_FWD; /* set this up */
			     req->s.route = BLW;   /* to send to directory */
			     req->progress = 8; 
			     /* now send down WRB/REPL message */
			   }
			 else /* race: L2 has not completed yet */
			   {
			     /* If this is the case, the cache has
				received an unsolicited writeback from
				the L1 before the L2 demand writeback
				even went to the L1. However, the L2
				replacement is still sent up
				separately (and will be nacked), so
				this one can be dissolved. */

#ifdef DEBUG_SECONDARY
			     if (YS__Simtime > DEBUG_TIME)
			       fprintf(simout,"8L2: race on WRB %ld in wrb_buf\n",req->tag);
#endif
			     /* free up the unsolicited WRB */
			     YS__PoolReturnObj(&YS__ReqPool,req);
			     return 1;
			   }
		       }
		     else /* no wrb-buf entry */
		       {
			 /* it hit in neither cache nor WRB buffer, so
			    this is a bizarre race condition: just
			    before a line got replaced from L2, it got
			    replaced from L1. So, that writeback got
			    merged into the WRB buffer and sent down
			    as the regular ACK. Now this one should be
			    the corresponding NACK from the L1 */

			 if (req->s.reply == REPLY)
			   {
			     YS__errmsg("ACK in the race condition where both caches replace at same time (should have been NACK)\n");
			   }

#ifdef DEBUG_SECONDARY
			 if (YS__Simtime > DEBUG_TIME)
			   fprintf(simout,"8L2: two cache-replace race on WRB %ld in wrb_buf\n",req->tag);
#endif
			 /* free up the NACK */
			 YS__PoolReturnObj(&YS__ReqPool,req);
			 return 1;
		       }
		   }
		 else /* not a WRB, but some other COHE_REPLY type */
		   {
		     if (hit_wrb_buf(captr,req)) /* in the wrb-buf? */
		       {
			 /* This case is a race where cache sent
			    a cohe up to L1 and then had a replacement
			    at L2 before the COHE_REPLY could come
			    back. So, either got the line from the
			    L1 or will send it as part of a WRB */
			 
			 if (req->s.reply == NACK)
			   {
			     /* in this case the L1 didn't have it, so
                                don't fill the line */
#ifdef DEBUG_SECONDARY
			     if (YS__Simtime > DEBUG_TIME)
			       fprintf(simout,"8L2: Cohe-reply nack to a line being WRBed %ld in wrb_buf\n",req->tag);
#endif
			   }
			 else
			   {
			     /* fill the line in the WRB_buf from the
				line we're getting back with this ack */
#ifdef DEBUG_SECONDARY
			     if (YS__Simtime > DEBUG_TIME)
			       fprintf(simout,"8L2: Cohe-reply race ack to a line being WRBed %ld in wrb_buf\n",req->tag);
#endif
			     captr->stat.wb_inclusions_race++;
			   }

			 /* Other than that, the COHE_REPLY is processed
			    as an ordinary NACK would be. */
		       }
		     /* Line is not present in cache, so COHE_REPLY sent
			to directory will be a NACK */
#ifdef DEBUG_SECONDARY
		     if (DEBUG_TIME < YS__Simtime)
		       fprintf(simout,"8L2: COHE on a miss; NACK\n");
#endif
#ifdef DEBUG_SECONDARY
		     if (req->forward_to != -1)
		       {
			 fprintf(simout,"8L2: COHE\t%s\tFORWARD NACKED!, tag=%ld\n",
				captr->name,req->tag);
		       }
#endif
		     captr->stat.cohe_reply_nack++;
		     if (req->forward_to != -1) /* is this a $-$ transfer? */
		       captr->stat.cohe_cache_to_cache_fail++;
		     req->s.reply = NACK;
		     req->progress = 8;
		   }
	       }
	     else /* it was a hit in the cache */
	       {
		 if (req->forward_to != -1)
		   captr->stat.cohe_cache_to_cache_good++;
		 cohe_type = captr->data[i1][i2].state.cohe_type;
		 cur_state = captr->data[i1][i2].state.st;

		 if (req->req_type != WRB) /* external COHE transaction */
		   {
		     /* NOTE: Check to see what happened at the L1
			cache: it could have been in PR_DY there but
			PR_CL here, in which case reply must be
			processed using that L1 copyback data. */

		     if (cur_state == PR_CL && req->size_st > REQ_SZ) /* was a copyback sent? */
		       {
			 cur_state = PR_DY; 
			 nxt_st = PR_DY;
			 captr->data[i1][i2].state.st = nxt_st;
		       }

		     /* Call cohe_rtn (cohe_sl) to determine nxt_st,
			response type, etc. */
		     captr->cohe_rtn(COHE, cur_state, 0, req->req_type, cohe_type,
				     req->dubref, 
				     &nxt_st, &nxt_mod_req, &req_sz, &rep_sz, &nxt_req,
				     &nxt_req_sz,
				     &allocate);
		     
		     captr->data[i1][i2].state.st = nxt_st;
		     
		     if (nxt_st == INVALID && captr->data[i1][i2].pref)
		       {
#ifdef DEBUG_SECONDARY
			 if (DEBUG_TIME < YS__Simtime)
			   {
			     fprintf(simout,"8L2: COHE_REPLY\t%s\t\tPrefetch:%ld\tINVALIDATED! @%.1f\n",captr->name,req->address,YS__Simtime);
			   }
#endif
			 captr->stat.pref_useless_cohe++;
			 captr->data[i1][i2].pref = 0;
			 captr->data[i1][i2].pref_tag_repl = -1;
		       }
		     if (captr->data[i1][i2].pref && (cur_state == PR_DY || cur_state == PR_CL) && (nxt_st == SH_CL))
		       {
#ifdef DEBUG_SECONDARY
			 if (DEBUG_TIME < YS__Simtime)
			   {
			     fprintf(simout,"8L2: COHE_REPLY\t%s\t\tPrefetch:%ld\tDOWNGRADED! @%.1f\n",captr->name,req->address,YS__Simtime);
			   }
#endif
			 captr->stat.pref_downgraded++;
		       }
		     if (captr->data[i1][i2].state.mshr_out)
		       {
			 YS__errmsg("coherence transaction to a line with mshr_out!!!\n");
		       }
		   }
		 else /* an unsolicited WRB was received */
		   {
		     if (cur_state == PR_CL) /* in this case, L2 cache state should be changed to PR_DY and the WRB processed */
		       {
			 cur_state = PR_DY;
			 nxt_st = PR_DY;
			 captr->data[i1][i2].state.st = nxt_st;
		       }
		     else if (cur_state != PR_DY)
		       YS__errmsg("L1 WRB received to bad line\n");
		     if (req->s.reply == NACK)		       
		       YS__errmsg("L1 sends a WRB NACK that hits in L2 cache (unsolicited WRB should not be a NACK)\n");
		   }

		 /* NOTE: code to handle cache-to-cache transfers */
		 if (req->forward_to != -1) /* a cache-to-cache transfer (forward) */
		   {
		     if (cur_state == SH_CL)
		       {
			 YS__errmsg("Cache-to-cache transfer request came to a non-pending SH_CL line\n");
		       }
		     else /* $-$ request to a private line */
		       {
			 /* The $-$ transfer consists of 2 parts: the response
			    to the directory and the response to the cache
			    itself. The response to the directory can either
			    be an ACK or a copyback, while the response to
			    the cache will be the full line with our
			    implementation */
			 req->invl_req = MakeForwardAck(req, cur_state);
			 MakeForward(captr, req, cur_state);
			 req->progress = 9; /* this is where forwards are sent out */
		       }
		   }
		 else
		   {
		     if (req->req_type == WRB)
		       {
			 /* just a replacement from the L1 that
			    happens to fit into the L2. */

			 req->absorb_at_l2 = ABSORB;
			 captr->stat.cohe_reply_unsolicited_WRB++;			 
			 req->progress = 1; /* let unsolicited WRB be sent to
					       the data pipe */
		       }
		     else if (req_sz >= LINESZ + REQ_SZ) /* this is a copyback or copyback+invl */
		       {
			 req->s.reply = REPLY;
			 req->size_st = LINESZ + REQ_SZ;
			 req->size_req = LINESZ + REQ_SZ;
			 /* in this case, response must go to the Data pipe */
			 req->progress = 1;
		       }
		     else /* just plain INVL */
		       {
			 req->s.reply = REPLY;
			 req->size_st =  REQ_SZ;
			 req->size_req = REQ_SZ;
			 req->progress = 8;
		       }
		   }

	       }
	   }
	 else
	   {
	     YS__errmsg("Unknown request type in cohe\n");
	   }
       }
     if (req->progress == 1) /* send this message to Data pipe */
       {
	 if (AddToPipe(captr->pipe[L2ReqDATAPIPE(req)],req) != 0) /* couldn't go to Data pipe */
	   return 0;

	 req->progress=0;
	 captr->num_in_pipes++;	 captr->pipe_empty = 0; /* No longer empty */
	 return 1;
       }
     if (req->progress == 8) /* send a message downward */
       {
	 if (req->req_type == WRB || req->req_type == REPL) /* might have changed to REPL because of mark_done_l1 */
	   {
	     /* has a space in the wrb_buf must be sunk to avoid deadlock*/
	     if (AddReqToOutQ(captr,req))
	       remove_from_wrb_buf(captr,posn_in_wrb_buf(captr,req),NULL);
	     else
	       AddToSmartMSHRList(captr,req,posn_in_wrb_buf(captr,req),remove_from_wrb_buf);
	     return 1;
	   }
	 else /* ordinary external coherence action */
	   return AddReqToOutQ(captr,req); /* this returns 0 on failure, 1 on success, same as us */
       }
     if (req->progress == 9) /* progress #9 and #10 handle the $-$ xfer */
       {
	 /* in this step, the $-$ xfer is actually sent out */
	 req->s.type = REPLY; /*req->req_type already set in MakeForward */
	 if (AddToPipe(captr->pipe[L2ReqDATAPIPE(req)],req) != 0) /* couldn't go to Data RAM */
	   {
	     req->s.type = COHE_REPLY; /* to make sure it comes back to proper progress case when revived */
	     return 0;
	   }

	 req->progress=0;
	 captr->num_in_pipes++;	 captr->pipe_empty = 0; /* No longer empty */
	 
	 req->invl_req->progress = 10;
	 /* no need to set invl_req's s.type since it should already be a COHE_REPLY */
	 return -1; /* to indicate that the REQ being processed (req) should be
		       substituted with req->invl_req */
       }
     if (req->progress == 10)
       {
	 /* in this step, try to send the ack/copyback to the directory */

	 if (req->size_st == LINESZ + REQ_SZ)
	   {
	     /* this case is a copyback, so it must first be sent
		through Data array -- it'll get sent down by Data array
		unit*/
	     
	     if (AddToPipe(captr->pipe[L2ReqDATAPIPE(req)],req) != 0)
	       return 0;

	     req->progress=0;
	     captr->num_in_pipes++;	 captr->pipe_empty = 0; /* No longer empty */
	     return 1;
	   }
	 else
	   {
	     /* cache just needs to send an ack, so just send this one down */
	     return AddReqToOutQ(captr,req);
	   }
       }
     fprintf(simerr, "L2cache: should be unreached code\n");
     break;
    }
  YS__errmsg("L2TagProcessReq: req not handled\n");
  return 0;
}

/*****************************************************************************/
/* wrb_buf functions: handle the wrb_buf resource used in the L2 cache for   */
/* sending out subset enforcement and write-backs                            */
/*****************************************************************************/

struct YS__WrbBufEntry
{
  long tag;         /* tag of the line being victimized */
  REQ *req2;        /* Possible secondary request clubbed with wrb-buf.
		       Should have same tag as main request */
  int done_data:1;  /* done with the L2 data path of WRB? */
  int done_l1:1;    /* done with the L1 path of WRB? */
  int dirty:1;      /* has it been set dirty either through L2 or L1? */
  int stalling:1;   /* is there a REQUEST stalling for an outstanding wrb-buf
		       entry */
};

/*****************************************************************************/
/* hit_markstall_wrb_buf: checks wrb_buf for an entry with tag matching an   */
/* incoming REQUEST. If a hit, the wrb_buf entry should be marked as having  */
/* a stalled REQUEST.                                                        */
/*****************************************************************************/
   
static int hit_markstall_wrb_buf(CACHE *captr, REQ *req)
{
  int i;
  for (i=0; i<wrb_buf_size; i++)
    if (captr->wrb_buf[i] && captr->wrb_buf[i]->tag == req->tag)
      {
	captr->wrb_buf[i]->stalling=1;
	return 1;
      }

  return 0;
}

/*****************************************************************************/
/* hit_wrb_buf: checks wrb_buf for an entry with tag matching REQ (called    */
/* for a COHE_REPLY)                                                         */
/*****************************************************************************/

static int hit_wrb_buf(CACHE *captr, REQ *req)
{
  int i;
  for (i=0; i<wrb_buf_size; i++)
    if (captr->wrb_buf[i] && captr->wrb_buf[i]->tag == req->tag)
      return 1;
  
  return 0;
}

/*****************************************************************************/
/* insert_in_wrb_buf: Adds a new entry to wrb_buf at first available space   */
/*****************************************************************************/

static void insert_in_wrb_buf(CACHE *captr, REQ *req, REQ *req2)
{
  int i;
  for (i=0; i<wrb_buf_size; i++)
    if (captr->wrb_buf[i] == NULL)
      {
	captr->wrb_buf[i] = malloc(sizeof(struct YS__WrbBufEntry));
	captr->wrb_buf[i]->tag = req->tag;
	captr->wrb_buf[i]->req2 = req2;
	captr->wrb_buf[i]->done_data = 0;
	captr->wrb_buf[i]->done_l1 = 0;
	captr->wrb_buf[i]->dirty = 0;
	captr->wrb_buf[i]->stalling = 0;
	return;
      }

  YS__errmsg("No space left in WRB_BUF -- should have booked space before inserting\n");
}

/*****************************************************************************/
/* posn_in_wrb_buf: Find entry in wrb_buf corresponding to desired REQ       */
/*****************************************************************************/

static int posn_in_wrb_buf(CACHE *captr, REQ *req)
{
  int i;
  for (i=0; i<wrb_buf_size; i++)
    if (captr->wrb_buf[i] && captr->wrb_buf[i]->tag == req->tag)
      return i;
  return -1;
}

/*****************************************************************************/
/* remove_from_wrb_buf: Remove an entry from the wrb-buf.                    */
/*****************************************************************************/

static int remove_from_wrb_buf(CACHE *captr, int i, REQ *requnused)
{
  if (i<0 || i > wrb_buf_size || !captr->wrb_buf[i])
    {
      YS__errmsg("Cannot remove empty or invalid entry from WRBBUF\n");
    }
#ifdef DEBUG_SECONDARY
  if (YS__Simtime > DEBUG_TIME)
    fprintf(simout,"L2: removing WRB %ld from wrb_buf\n",captr->wrb_buf[i]->tag);
#endif
  captr->wrb_buf_used--; /* free the wrb_buf entry, as it is no longer
			    used or needed. */
#ifdef DEBUG_SECONDARY
  if (YS__Simtime > DEBUG_TIME)
    fprintf(simout,"%s wrb_buf size now %d -- line %d\n",captr->name,captr->wrb_buf_used, __LINE__);
#endif
  free(captr->wrb_buf[i]);
  captr->wrb_buf[i] = NULL;
  return 0;
}

/*****************************************************************************/
/* replace_in_wrb_buf: Replace an entry from the wrb-buf with the req2       */
/* passed in at insert-time. Occurs for INVL clubbed with WRB if L1 is WT    */
/*****************************************************************************/

static int replace_in_wrb_buf(CACHE *captr, int i, REQ *requnused)
{
  if (i<0 || i > wrb_buf_size || !captr->wrb_buf[i])
    {
      YS__errmsg("Cannot replace empty or invalid entry in WRBBUF!\n");
    }
#ifdef DEBUG_SECONDARY
  if (YS__Simtime > DEBUG_TIME)
    fprintf(simout,"L2: replacing INVL %ld in wrb_buf with WRB %ld\n",captr->wrb_buf[i]->tag,captr->wrb_buf[i]->req2->tag);
#endif
  captr->wrb_buf[i]->tag = captr->wrb_buf[i]->req2->tag;
  /* continue holding onto the wrbbuf entry */
  
  /* Immediately see if former req2 can be sent out. If so, free
     up this wrb-buf. Otherwise, use this wrb-buf as a SmartMSHR again. */
  if (AddReqToOutQ(captr,captr->wrb_buf[i]->req2))
    {
      captr->wrb_buf[i]->req2 = NULL;
      return remove_from_wrb_buf(captr,i,NULL);
    }
  else
    {
      AddToSmartMSHRList(captr,captr->wrb_buf[i]->req2,i,remove_from_wrb_buf);
      captr->wrb_buf[i]->req2 = NULL;
      return 0;
    }
}

/*****************************************************************************/
/* mark_done_data_in_wrb_buf: L2 data access for WRB has completed. Return   */
/* whether or not L1 access has also completed.                              */
/*****************************************************************************/

static int mark_done_data_in_wrb_buf(CACHE *captr, REQ *req, int withdata)
{
  int i;
  for (i=0; i<wrb_buf_size; i++)
    if (captr->wrb_buf[i] && captr->wrb_buf[i]->tag == req->tag)
      {
	if (withdata)
	  {
	    captr->wrb_buf[i]->dirty = 1;
	  }
	captr->wrb_buf[i]->done_data = 1;
	return captr->wrb_buf[i]->done_l1;
      }

  YS__errmsg("No match in wrb_buf!!!\n");
  return -1;
}

/*****************************************************************************/
/* mark_done_l1_in_wrb_buf: L1  access for WRB has completed. Return whether */
/* or not L2 data access has also completed. WRBs are sent out only after    */
/* mark_done_l1_in_wrb_buf returns successfully.                             */
/*****************************************************************************/

static int mark_done_l1_in_wrb_buf(CACHE *captr, REQ *req)
{
  int i;
  for (i=0; i<wrb_buf_size; i++)
    if (captr->wrb_buf[i] && captr->wrb_buf[i]->tag == req->tag)
      {
	if (req->size_st > REQ_SZ) /* this one is bringing in data */
	  {
	    captr->wrb_buf[i]->dirty = 1;
	  }
	captr->wrb_buf[i]->done_l1 = 1;
	if (captr->wrb_buf[i]->done_data)
	  {
	    if (captr->wrb_buf[i]->dirty)
	      {
		/* set size_st in case an unsolicited WRB had set dirty
		   earlier */
		if (req->size_st != REQ_SZ + LINESZ)
		  req->size_st = REQ_SZ + LINESZ;
	      }
	    else /* sure to be correct size */
	      req->req_type = REPL;
	  }
	  
	return captr->wrb_buf[i]->done_data;
      }

  YS__errmsg("No match in wrb_buf!!!\n");
  return -1;
}

/*****************************************************************************/
/* mark_undone_l1_in_wrb_buf: Used to clear out L1 done flag in some races   */
/*                            (see MoveWRBToL1 for more information.)        */
/*****************************************************************************/

static void mark_undone_l1_in_wrb_buf(CACHE *captr, REQ *req)
{
  int i;
  for (i=0; i<wrb_buf_size; i++)
    if (captr->wrb_buf[i] && captr->wrb_buf[i]->tag == req->tag)
      {
	captr->wrb_buf[i]->done_l1 = 0;
	return;
      }

  YS__errmsg("No match in wrb_buf!!!\n");
}

/*****************************************************************************/
/* check_stalling_in_wrb_buf: Any REQUEST stalled because of a match with    */
/* this entry?                                                               */
/*****************************************************************************/

static int check_stalling_in_wrb_buf(CACHE *captr, REQ *req)
{
  int i;
  for (i=0; i<wrb_buf_size; i++)
    if (captr->wrb_buf[i] && captr->wrb_buf[i]->tag == req->tag)
      {
	return captr->wrb_buf[i]->stalling;
      }

  YS__errmsg("No match in wrb_buf!!!\n");
  return -1;
}

/*****************************************************************************/
/* check_done_l1_in_wrb_buf: Has L1 access for this WRB already completed?   */
/*****************************************************************************/

static int check_done_l1_in_wrb_buf(CACHE *captr, REQ *req)
{
  int i;
  for (i=0; i<wrb_buf_size; i++)
    if (captr->wrb_buf[i] && captr->wrb_buf[i]->tag == req->tag)
      {
	return captr->wrb_buf[i]->done_l1;
      }
  
  YS__errmsg("No match in wrb_buf!!!\n");
  return -1;
}

/*****************************************************************************/
/* MoveWRBToL1: Called at completion of L2 data access (if any). Marks L2    */
/* access part of WRB complete. Tries to send message up to L1, using entry  */
/* in wrb-buf as a "smart MSHR" if it can't be immediately sent.             */
/*****************************************************************************/
   
static void MoveWRBToL1(CACHE *captr, REQ *req, int withdata)
{
  if (mark_done_data_in_wrb_buf(captr,req,withdata)) /* now both data and l1 paths are done */
    {
#ifdef DEBUG_SECONDARY
      if (YS__Simtime > DEBUG_TIME)
	fprintf(simout,"%s\tWRB_COHE tag %ld-- race condition coming out of Data pipe\n",captr->name,req->tag);
#endif
      /* this is a race condition:

	 L2 was in the process of victimizing this line, but in the
	 meanwhile, L1 has sent an unsolicited WRB to the same line.
	 The data from the unsolicited response from the L1 is the
	 correct data, so we must use that for the write-back to
	 send out. 
       
	 A more complex L2 logic controller would go ahead and send
	 down the ACK from this point itself, but we'll assume a less
	 complicated controller for now. The L2 should go ahead and send
	 a message to the L1 and wait for it to get NACKed. So,
	 clear out the "L1 done" bit in order to allow this.
       */
      mark_undone_l1_in_wrb_buf(captr,req);
    }

  /* always be sure to decouple the WRB inclusion request from the previous */
  if (!AddReqToOutQ(captr,req))
    AddToSmartMSHRList(captr,req,-1,NULL); /* consider the reserved WRB buffer to be a smart MSHR */
  captr->stat.wb_inclusions_sent++;
}

/*****************************************************************************/
/* MoveINVLToL1: Called when sending an invl_req upward (possibly also with  */
/* a wrb_req if the L1 cache is WT). Tries to send message up to L1, using   */
/* entry in wrb-buf as a "smart MSHR" if it can't be immediately sent.       */
/*****************************************************************************/

static void MoveINVLToL1(CACHE *captr, REQ *req, REQ *wrb_req)
{
  /* always be sure to decouple the inclusion request from the previous */
  /* if this replacement has both an invl and a separate WRB (used if L1WT),
     then this req's WRB_REQ should be set to indicate this. */

  if (wrb_req) /* hold onto the smart MSHR even after sending INVL */
    {
      req->wrb_req = wrb_req;
      if (AddReqToOutQ(captr,req))
	{
	  /* the INVL has been sent out, now try to send out the WRB --
	   that is taken care of by the replace_in_wrb_buf function*/
	  replace_in_wrb_buf(captr,posn_in_wrb_buf(captr,req),NULL); 
	}
      else
	{
	  AddToSmartMSHRList(captr,req,posn_in_wrb_buf(captr,req),replace_in_wrb_buf);
	}
    }
  else /* the smart MSHR can be let go as soon as the INVL is sent */
    {
      if (AddReqToOutQ(captr,req))
	remove_from_wrb_buf(captr,posn_in_wrb_buf(captr,req),NULL);
      else
	AddToSmartMSHRList(captr,req,posn_in_wrb_buf(captr,req),remove_from_wrb_buf);
    }
}

