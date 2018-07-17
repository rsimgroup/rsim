/* mshr.c

   Support routines for cache modules -- These routines handle all
   situations related to MSHRs, from determining whether or not a
   REQUEST needs an MSHR to coalescing at the MSHRs to checking
   incoming COHE transactions with the outstanding MSHRs and acting
   appropriately.

   */
/*****************************************************************************/
/* This file is part of the RSIM Simulator.                                  */
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
#include "MemSys/miss_type.h"
#include "MemSys/stats.h"
#include "MemSys/arch.h"
#include "Processor/simio.h"

int DISCRIMINATE_PREFETCH = 0; /* Will prefetches drop if they don't get
				  an MSHR? */

/*****************************************************************************/
/* MSHR operations: The MSHRs implement the non blocking nature of the       */
/* caches. They are implemented as an array of outstanding REQ's.            */
/*****************************************************************************/

/*****************************************************************************/
/* notpres_mshr: This function is called for all incoming REQUESTs,          */
/* COHEs, and COHE_REPLYs and checks whether the incoming transaction        */
/* matches an outstanding transaction in an MSHR (and in the case of         */
/* REQUESTs, whether it hits in the cache and thus needs an MSHR). The       */
/* return value is an MSHR_Response type. Pseudocode is interspersed         */
/* throughout this (large) function.                                         */
/*****************************************************************************/
  
enum MSHR_Response notpres_mshr(CACHE *captr, REQ *req)
{
  int free_mshr = -1;
  int hit = -1;
  int i, hittype, type, data_type;
  int req_sz, rep_sz, nxt_req_sz;
  long tag, address;
  int set, set_ind, i1, i2;
  int cohe_type, allo_type, allocate,  dest_node;
  ReqType nxt_mod_req, nxt_req;
  CacheLineState cur_state, nxt_st;
  enum MSHR_Response response;
  
  req->tag = req->address>> captr->block_bits;
  req->linesz = captr->linesz;
  
  if(captr->mshr_count == 0)
    hit = -1;
  else
    {
#ifdef DEBUG_MSHR
      for(i=0;i<captr->max_mshrs;i++)
	{
	  if(captr->mshrs[i].valid == 1 && !captr->mshrs[i].mainreq->inuse)
	    {
	      YS__errmsg("MSHR entry in use for a freed request!\n");
	    }
	}
#endif
      
      /* This function first determines whether the incoming
	 transaction matches anything in the MSHRs */
      for(i=0;i<captr->max_mshrs;i++)
	{
	  if((captr->mshrs[i].valid == 1) &&
	     (req->tag == captr->mshrs[i].mainreq->tag)) /* same tag, so match */
	    {
	      hit = i; /* mark down the MSHR of the match */
	      break;
	    }
	}
    }

  if(hit == -1) /* no match */
    {
      /* Find first free MSHR in case we need to allocate a new mshr  */
      for(i=0;i<captr->max_mshrs;i++)
	{
	  if(captr->mshrs[i].valid != 1)
	    {
	      free_mshr = i;
	      break;
	    }
	}
    }
#ifdef DEBUG_MSHR
  if(YS__Simtime > DEBUG_TIME)
    fprintf(simout,"Mshr hit is %d free is %d, reqcount is %d count is %d\n", hit, free_mshr, captr->reqmshr_count,captr->mshr_count);
#endif
  address = req->address;
  type = req->req_type;
  data_type = req->address_type;
  
  /* This function does different things for request, replies and cohe_messages */
  switch(req->s.type)
    {
    case REQUEST:
      /* The "notpres" function is called to determine if the desired line
	 is available in the cache. */
      hittype = notpres(req->address,&tag,&set,&set_ind,captr);
      i1 = set_ind / SUB_SZ;
      i2 = set_ind % SUB_SZ;
      if(hit == -1) /* the line is not present in any MSHR */
	{
	  if(hittype != 0) /* not in cache either */
	    cur_state=INVALID;
	  else /* in cache, so find current state */
	    {
	      cur_state = captr->data[i1][i2].state.st;
	    }

	  /* Determine cohe_type of cache and access */
	  cache_get_cohe_type(captr,hittype,set_ind,address,&cohe_type,&allo_type,&dest_node);

	  req->cohe_type = cohe_type;
	  req->allo_type = allo_type;
	  req->dest_node = dest_node;

	  /* The coherence routine for the cache ("cohe_pr" for L1,
	     "cohe_sl" for L2) is called to determine the appropriate
	     actions for this line, based on whether or not the line
	     is present, the current MESI state of the line in cache,
	     and the type of cache. */
	  
	  captr->cohe_rtn(REQUEST,cur_state,allo_type,type,cohe_type,req->dubref,
			  &nxt_st,&nxt_mod_req,&req_sz,&rep_sz,
			  &nxt_req,&nxt_req_sz,&allocate);

	  /* If the line in question hits in the cache in an
	     acceptable state, this request will not require a request
	     to a lower module. */

	  if(!nxt_mod_req) /* no lower request needed */
	    {
	      if (captr->data[i1][i2].state.cohe_pend) /* L2 cache line with outstanding COHE inflight to L1 */
		{
		  if (MemsimStatOn)
		    captr->stat.pipe_stall_PEND_COHE++;
		  return NOMSHR_STALL_COHE;
		}
	
	      /* This request can be serviced without going to the
                 next level */
	      
	      if (cur_state != nxt_st) 
		{
		  /* Set cache line state accordingly */
		  captr->data[i1][i2].state.st=nxt_st;
		}
	      hit_update(set_ind, captr, set, req); /* update LRU ages */
	      StatSet(captr,req,CACHE_HIT);
	      return NOMSHR; /* no MSHR needed or involved */
	    }
      
	  if(captr->cache_level_type == FIRSTLEVEL_WT ||
	     (captr->cache_level_type == FIRSTLEVEL_WB && (req->prcr_req_type == L2WRITE_PREFETCH || req->prcr_req_type == L2READ_PREFETCH)))
	    {
	      /* check for non-allocating accesses */
	      if(req->prcr_req_type == WRITE ||
		 req->prcr_req_type == L2WRITE_PREFETCH ||
		 req->prcr_req_type == L2READ_PREFETCH)
		{

		  /* If the request goes to the next level of cache
		     without taking an MSHR here (either by being a
		     write in a write-through cache or an L2
		     prefetch), the value "NOMSHR_FWD" is returned to
		     indicate that no MSHR was required, but that the
		     request must be sent forward. */
		  
		  if(hittype == 0)
		    hit_update(set_ind, captr, set, req); /* update LRU age if present */
		  if (req_sz != NOCHANGE)
		    req->size_st = req_sz;
		  if (rep_sz != NOCHANGE)
		    req->size_req = rep_sz;
		  req->req_type = nxt_mod_req; /* this is where req_type changes */

		  if (hittype == 0 && req->prcr_req_type == L2READ_PREFETCH)
		    /* hit in L1 implies hit in L2, and this can't be
		       an upgrade either */
		    {
		      return NOMSHR; 
		    }
		  StatSet(captr,req,CACHE_MISS_WT);
		  return NOMSHR_FWD;
		}
	    }
      
	  /* Else we need to allocate an MSHR for this request */

	  /* Check for structural hazards */
	  if (captr->mshr_count == captr->max_mshrs) /* All REQUEST MSHRs in use? */
	    {
	      /* If the request needs a new MSHR, but none are
		 available, the value "NOMSHR_STALL" is returned. */
	      
	      if (MemsimStatOn)
		captr->stat.pipe_stall_MSHR_FULL++;
	      return NOMSHR_STALL; 
	    }

	  if (captr->cache_level_type == SECONDLEVEL && (captr->wrb_buf_used + captr->mshr_count >= wrb_buf_size))
	    {
	      /* In the case of the L2 cache, a request must also be
		 able to reserve a space in the write-back
		 buffer. Otherwise, the function should return
		 NOMSHR_STALL_WRBBUF_FULL because the REPLY might need
		 a wrb_buf space for a victimization */

	      if (MemsimStatOn)
		captr->stat.pipe_stall_WRBBUF_full++;
	      return NOMSHR_STALL_WRBBUF_FULL;
	    }

	  /* Otherwise, the cache books an MSHR and returns a response
	     based on whether this access was a complete miss
	     ("MSHR_NEW") or an upgrade request ("MSHR_FWD"). */

	  captr->mshr_count++;
	  captr->reqmshr_count++;
	  captr->reqs_at_mshr_count++;
	  
	  /* note: the value fed to the interval statistics is the _current_
	     value, not the value of the previous interval */
	  StatrecUpdate(captr->mshr_occ,(double)captr->reqmshr_count,YS__Simtime);
	  StatrecUpdate(captr->mshr_req_count,(double)captr->reqs_at_mshr_count,YS__Simtime);
      
	  captr->mshrs[free_mshr].valid = 1; /* Set to valid */
	  captr->mshrs[free_mshr].setnum = set; /* just to store state */
	  captr->mshrs[free_mshr].mainreq = req;
	  captr->mshrs[free_mshr].counter = 0;     /* Number of coalesces -- set to 0 */
	  captr->mshrs[free_mshr].pend_cohe_type = BAD_REQ_TYPE; 
	  captr->mshrs[free_mshr].stall_WAR = 0;
	  captr->mshrs[free_mshr].demand = -1.0; /* this'll get set when we get a pref_late */
	  if (req->s.prefetch)
	    {
	      captr->mshrs[free_mshr].only_prefs = 1;
	    }
	  else
	    captr->mshrs[free_mshr].only_prefs = 0;

	  if ((captr->cache_level_type != FIRSTLEVEL_WT) && (req->req_type == WRITE || req->req_type == RMW))
	    captr->mshrs[free_mshr].writes_present = 1; /* MSHR should transition line to dirty  on response */
	  else
	    captr->mshrs[free_mshr].writes_present = 0;

#ifdef DEBUG_MSHR
	  if (YS__Simtime > DEBUG_TIME)
	    fprintf(simout,"MSHR: %s Going to add an MSHR for tag %ld, set %d\n",captr->name,req->tag,set);
#endif
      
	  if(hittype == 0) /* line present in cache -- upgrade-type access */
	    {
	      /* In the case of upgrades, the line is locked into
		 cache by setting "mshr_out"; this guarantees that the
		 line is not victimized on a later "REPLY" before the
		 upgrade reply returns. In all cases where the line is
		 present in cache, the "hit_update" function is called to
		 update the ages of the lines in the set (for LRU
		 replacement). */
	      
	      StatSet(captr,req,CACHE_MISS_UPGR); 
	      hit_update(set_ind, captr, set, req);
	      
	      if (captr->cache_level_type != FIRSTLEVEL_WT)
		captr->data[i1][i2].state.mshr_out = 1; /* mark an upgrade so we don't kick it out */

	      response = MSHR_FWD; /* upgrade */
	    }
	  else if (hittype == 1)
	    {
	      response = MSHR_NEW; /* total miss */
	    }
	  else /* hittype == 2 */
	    {
	      response = MSHR_NEW; /* total miss */
	    }
      
	  if (req_sz != NOCHANGE)
	    req->size_st = req_sz;
	  if (rep_sz != NOCHANGE)
	    req->size_req = rep_sz;
	  req->req_type = nxt_mod_req; /* this is where req_type changes */
	  return response; /* MSHR_NEW if total miss; MSHR_FWD if upgrade */
	}
    
      /* Matches in MSHR -- REQUEST must either be merged, dropped,
	 forwarded around cache, or stalled. */

      /* Try to coalesce REQUEST: can't do this if MSHR is being held
	 for WRB or merged with COHE, first of all */
      if (captr->mshrs[hit].mainreq->req_type == WRB || captr->mshrs[hit].mainreq->req_type == REPL)
	{
	  /* Stalls can occur if the MSHR is being temporarily held for a
	     write-back (in which case the function returns
	     "MSHR_STALL_WRB") */
	  if (MemsimStatOn)
	    captr->stat.pipe_stall_WRB_match++;
	  return MSHR_STALL_WRB;
	}
      if(captr->mshrs[hit].pend_cohe_type)
	{
	  /* Stalls can occur if the MSHR is marked with an
	     unacceptable pending coherence message (return value is
	     "MSHR_STALL_COHE") */
	  
	  /* If it is an external read_sh (COPYBACK), we have to stall on
	     cases where this REQUEST wants it in exclusive mode.
	     If it is not a COPYBACK, it is some kind of invalidate,
	     so this REQUEST must be stalled in all cases */
	  if((captr->mshrs[hit].pend_cohe_type == COPYBACK &&
	      (captr->mshrs[hit].mainreq->prcr_req_type == RMW ||
	       captr->mshrs[hit].mainreq->prcr_req_type == WRITE ||
	       captr->mshrs[hit].mainreq->prcr_req_type == L1WRITE_PREFETCH ||
	       captr->mshrs[hit].mainreq->prcr_req_type == L2WRITE_PREFETCH)) ||
	     captr->mshrs[hit].pend_cohe_type != COPYBACK )
	    {
	      if (MemsimStatOn)
		captr->stat.pipe_stall_MSHR_COHE++;
	      return MSHR_STALL_COHE;
	    }
	}


      /* Now, how does the MSHR handle prefetch matches?
	 At the first level cache, prefetches should either be
	 dropped, forwarded around cache, or stalled. There is never
	 any need to coalesce at L1, since one fetch is sufficient. At
	 the L2, though, prefetches cannot be dropped, as they might
	 already have many requests coalesced into them and waiting
	 for them at the L1 cache. */

      /* So, first handle all the cases where the prefetch should be
	 dropped at or forwarded around L1 cache */
      if (captr->cache_level_type != SECONDLEVEL)
	{
	  /* If a read prefetch wants to coalesce at L1 cache, drop it. */
	  if (req->prcr_req_type == L1READ_PREFETCH || req->prcr_req_type == L2READ_PREFETCH)
	    {
	      return MSHR_USELESS_FETCH_IN_PROGRESS;
	    }
	  if (req->prcr_req_type == L1WRITE_PREFETCH) /* If a write prefetch wants to coalesce at L1 cache */
	    {
	      
	      /* L1WT -- send down as a NOMSHR_FWD, after transforming to an
		 L2WRITE_PREFETCH (may cause upgrade) */
	      if (captr->cache_level_type == FIRSTLEVEL_WT)
		{
		  StatSet(captr,req,CACHE_MISS_WT); /* count the stat as a WT for L1WP; then change it */
		  req->req_type = req->prcr_req_type = L2WRITE_PREFETCH;
		  return NOMSHR_FWD;
		}

	      /* L1WB -- drop it if its to a line with exclusive-mode MSHR.
		         Otherwise, prefetch will need to stall (later). */
	      if (captr->mshrs[hit].mainreq->prcr_req_type == WRITE ||
		  captr->mshrs[hit].mainreq->prcr_req_type == RMW ||
		  captr->mshrs[hit].mainreq->prcr_req_type == L1WRITE_PREFETCH ||
		  captr->mshrs[hit].mainreq->prcr_req_type == L2WRITE_PREFETCH)
		return MSHR_USELESS_FETCH_IN_PROGRESS;
	    }
	  if (req->prcr_req_type == L2WRITE_PREFETCH) /* has no business with this cache */
	    {
	      return NOMSHR_FWD; 
	    }
	}

      /* Now we need to consider the possibility of a "WAR" stall. This is
	 a case where an MSHR has an exclusive-mode request wants to merge
	 with a shared-mode MSHR.  */


      /* If this is a read that hits in the L1WT cache, then there's no need
	 to consider WAR stalls, as those writes could have already put
	 their data into the cache. */

      if (captr->cache_level_type == FIRSTLEVEL_WT && hittype == 0 &&
	  (req->prcr_req_type == READ || req->prcr_req_type == L1READ_PREFETCH || req->prcr_req_type == L2READ_PREFETCH))
	{
	  return NOMSHR; /* hit -- no MSHR needed or involved */
	}

      /* Now, even if this is a read request to an MSHR with an outstanding
	 WAR, this request should be stalled, as otherwise the read would
	 be processed out-of-order with respect to the stalled write */
       
      if (captr->mshrs[hit].stall_WAR)
	{
	  if (MemsimStatOn)
	    captr->stat.pipe_stall_MSHR_WAR++;
      
	  return MSHR_STALL_WAR;
	}
    
      /* RMW/write request matches read MSHR */
      if((req->prcr_req_type == RMW || req->prcr_req_type == L1WRITE_PREFETCH  || req->prcr_req_type == L2WRITE_PREFETCH
	  || req->prcr_req_type == WRITE) &&
	 (captr->cache_level_type == FIRSTLEVEL_WT || /* in a WT cache, a write should never be allowed to coalesce with an MSHR, even if it's for a RMW */
	  captr->mshrs[hit].mainreq->prcr_req_type == READ ||
	  captr->mshrs[hit].mainreq->prcr_req_type == L1READ_PREFETCH || captr->mshrs[hit].mainreq->prcr_req_type == L2READ_PREFETCH))
	{
	  /* Write after read -- stall system */

	  /* note: if the acces is a prefetch that is going to be dropped
	     with DISCRIMINATE prefetching, there is no reason to count this
	     in stats or start considering this an "old" WAR */
	  if (!req->s.prefetch || !(DISCRIMINATE_PREFETCH&&captr->cache_level_type != SECONDLEVEL))
	    {
	      if (MemsimStatOn)
		captr->stat.pipe_stall_MSHR_WAR++;
	      captr->mshrs[hit].stall_WAR = 1;
	    }
	  return MSHR_STALL_WAR;     /* WAR stall */
	}
      if(captr->mshrs[hit].counter == MAX_COALS) /* too many requests coalesced with MSHR */
	{
	  if (MemsimStatOn)
	    captr->stat.pipe_stall_MSHR_COAL++;
	  return MSHR_STALL_COAL;
	} 
    
      /* No problems with coalescing the request, so coalesce it */
      /* Add request to coal req */
      captr->mshrs[hit].coal_req[captr->mshrs[hit].counter++] = req;

      if ((captr->cache_level_type != FIRSTLEVEL_WT) && (req->req_type == WRITE || req->req_type == RMW))
	captr->mshrs[hit].writes_present = 1; /* transition line to dirty when it comes back */
    
      StatrecUpdate(captr->mshr_req_count,(double)captr->reqs_at_mshr_count,YS__Simtime);
      captr->reqs_at_mshr_count++;

      /* Set the "read_with_write" field of the main REQUEST in the
	 MSHR in case this is a coalesce at the L2 cache of an access
	 that did allocate at the L1 cache with an MSHR for an access
	 that did not allocate there -- used in REPLY case of
	 L1ProcessTagReq */

      if( ((L1TYPE==FIRSTLEVEL_WT &&captr->mshrs[hit].mainreq->prcr_req_type == WRITE) ||
	   captr->mshrs[hit].mainreq->prcr_req_type == L2WRITE_PREFETCH ||
	   captr->mshrs[hit].mainreq->prcr_req_type == L2READ_PREFETCH) &&
	  (req->prcr_req_type == L1WRITE_PREFETCH ||
	   req->prcr_req_type == READ ||
	   req->prcr_req_type == L1READ_PREFETCH ||
	   req->prcr_req_type == RMW ||
	   (L1TYPE==FIRSTLEVEL_WB && req->prcr_req_type == WRITE)))
	{
	  captr->mshrs[hit].mainreq->read_with_write = 1;
	}
      StatSet(captr,req,CACHE_MISS_COAL);
      if (captr->mshrs[hit].only_prefs && !req->s.prefetch) /* prefetch just became late because of coalesce */
	{
	  captr->stat.pref_late++;
	  captr->mshrs[hit].demand = YS__Simtime; /* use this to compute lateness factor */
	  captr->mshrs[hit].only_prefs = 0;
	}
      return MSHR_COAL;
    
      YS__errmsg("MSHR: this is supposed to be unreachable code\n");
    
    case REPLY:
      YS__errmsg("Unknown REPLY case -- notpres_mshr \n");
      return NOMSHR;
      break;
    
    case COHE:
    case COHE_REPLY: /* Handle COHE and COHE_REPLY together for now */
      /* Note: L1 cache only calls this case with COHE_REPLY. L2 cache calls
	 this case on upward-bound COHE and again on COHE_REPLY after it
	 has been processed at L1 (it is called at COHE in case it can
	 be NACK'ed immediately -- miss at L2 means miss at L1 also) */
      if(hit == -1)    /* no MSHR match */
	return NOMSHR; /* no MSHR involved -- just rely on cache state */
      if (req->s.type == COHE_REPLY && (req->req_type == WRB || req->req_type == REPL))
	{
	  if (captr->cache_level_type == SECONDLEVEL && req->req_type == WRB && req->s.reply != REPLY)
	    {
	      /* if L2 cache gets a WRB that's a NACK or NACK_PEND but
		 which doesn't match in the wrb_buf, that means that
		 another unsolicited WRB came earlier and acted as an
		 ACK to short-circuit the demand WRB. So, this is the
		 demand WRB coming back as a NACK or NACK_PEND, and
		 can be ignored. */
	      return NOMSHR;
	    }
	  if (captr->cache_level_type != FIRSTLEVEL_WB)
	    {
	      /* We should never get into this case. L1WT should never
		 get a WRB, and L2 cache should never get a solicited WRB
		 to a line with an MSHR outstanding */
	      YS__errmsg("Panic -- a WRB/REPL is trying to coalesce into an MSHR !!!\n");
	      return NOMSHR;
	    }
	}
    
      if(captr->mshrs[hit].mainreq->prcr_req_type == READ ||
	 captr->mshrs[hit].mainreq->prcr_req_type == L1READ_PREFETCH ||
	 captr->mshrs[hit].mainreq->prcr_req_type == L2READ_PREFETCH)
	/* Note: cache would like to be equally aggressive with write
	   prefetches, but it can't because those might be returning
	   in dirty state on a cache-cache transfer, and that would
	   need a copyback that cache can't provide */
	{
	  /* cache has sent a READ_SH type of request. Response might be
	     either SHARED or EXCLUSIVE state, depending on circumstances.

	     These are the basic types of COHEs as far as this is concerned.
	     1) COPYBACK/COPYBACK-INVL/COPYBACK-SHDY (i.e. NACK_NOK)
	        These types of COHEs demand a copyback of the data
		(either to the directory or to the next owner). The
		directory sends these types of messages to a cache
		believed to hold the line in PRIVATE state. So, either
		this cache previously held this line in PRIVATE state
		(and the COHE was sent before the directory got the
		MSHR request), or the MSHR request is going to come
		back as a PRIVATE. In either case, the cache cannot
		handle this type of request. In the first case, it
		would be sufficient to NACK it, but because of the
		second case, cache needs to NACK_PEND it.

	     2) INVL (i.e. NACK_OK)
	        This type of COHE does not demand a copyback of
		data. The directory sends these types of messages on a
		SHARED. So, either this cache previously held this
		line in SHARED state or the MSHR request is going to
		come back as a SHARED. In the first case, we'd like to
		ignore it, but for the second case we need to do the
		coherence action when the line comes back. We can't
		tell between the first case and the second case yet,
		but we do know that if the reply to this line comes
		back as a PRIVATE we had the first case (it might have
		been the first case even if the response comes back
		SHARED, but we can't tell).  So, what we'll do is save
		aside the cohe type and do the action if we get a
		SHARED response (if we get a PRIVATE response, just
		ignore the cohe action at the L2 -- but we'll do it at
		the L1.)

	     3) WRB
	        This can come to the L1 cache from the L2. Treat it
		as an INVL, in the sense that that part should coalesce.
		However, return it as a NACK to indicate to L2 that no
		data is being transferred.
		
		*/
	     
	  if(req->s.type == COHE_REPLY)
	    {
	      if (req->req_type != WRB && req->req_type != REPL)
		{
		  if (req->s.nack_st == NACK_NOK)
		    {
		      /* category 1 above */
		      return MSHR_COAL;
		    }
		  else
		    {
		      /* category 2 above */
		      captr->mshrs[hit].pend_cohe_type = req->req_type;
		      return MSHR_COAL;
		    }
		}
	      else 
		{
		  /* category 3 above: If we get a WRB/REPL, we'll
		     mark it as an INVL locally and go ahead and NACK
		     the WRB also */
#ifdef DEBUG_MSHR
		  if (YS__Simtime > DEBUG_TIME)
		    fprintf(simout,"WRB/REPL to line with READ MSHR -- mark it as an INVL and also nack the WRB.\n");
#endif
		  captr->mshrs[hit].pend_cohe_type = INVL;
		  return NOMSHR;
		}
	    }

	  /* if this is a COHE, return with MSHR_COAL so that cache knows
	     that it needs to send the COHE on to the L1, but that something
	     interesting may happen on COHE_REPLY */
	
	  return MSHR_COAL;
	}
      else /* we're tying to get MSHR back in private (possibly dirty) state */
	{
	  if (req->s.nack_st == NACK_NOK && req->s.type == COHE_REPLY)
	    {
	      /* a NACK_NOK specifically implies a data copyback demand
		 there are 2 ways this can happen:
	       
		 CASE 1 --
		 This case occurs when the MSHR is for a line in
		 private state, the request reaches the directory, and
		 the directory sends out a COHE to this line
		 afterward. However, because of network reordering,
		 the COHE reaches here before the REPLY
	       
		 In this case, the cache would have been able to hold
		 on to the request and handle it only when the reply
		 comes back, since the cache would know here that the
		 reply is on its way back. However, because of the
		 possibility of CASE 2 (blw) the cache can't just do
		 that, and instead has to do something a little more
		 complicated.
	       
		 CASE 2 --
		 
		 This cache wroteback a PR_DY line on a
		 replacement. However, the WRB hasn't reached
		 directory yet. Meanwhile some other cache sends out a
		 READ_OWN, so directory sends this cache a
		 COPYBACK_INVL, NACK_NOK. But, before this cache got
		 that COHE, the cache also sent out a WRITE (READ_OWN)
		 to that same line, booking an MSHR. Now, the COHE has
		 come to the cache and can't just be merged.  The
		 cache needs to know if it was the COHE from before
		 the WRB, or if it's a COHE for the current
		 request. Ideally, the cache would reject it if the
		 former and merge it if the latter, but there isn't
		 enough information to do this. So, the cache will
		 send it back and hope directory will retry it....
		 */
	    
	      if  (captr->cache_level_type != FIRSTLEVEL_WT)
		{
		  return MSHR_COAL;
		}
	      else /* First-level WT cache; no need for later copyback */
		{
		  /* This is to be treated like a read with a pending MSHR */

		  captr->mshrs[hit].pend_cohe_type = req->req_type;
		  return MSHR_COAL;
		}
	    }
	  else /* NACK_OK, including WRBs */
	    {
	      /* this state is a little confusing. It happens when cache
		 has an MSHR outstanding for PRIVATE but get a NACK_OK
		 INVL type of request that doesn't want a data copyback.
		 This must mean that the request originated at the
		 directory before the private request was serviced. */

	      /* so, the cache should do the coherence action _now_ if
		 the line is present. Send this request back with a
		 NACK (which, after all, is OK). Directory should
		 figure out that this cache's upgrade [if it had sent
		 one] is really a full line request, because the
		 upgrade request will see that the sender is not in
		 the list of sharers */
	    
	      return MSHR_FWD;
	    }
	}
      YS__errmsg("MSHR: this is supposed to be unreachable code\n");
      return NOMSHR;
      break;
    
    default:
      YS__errmsg("Unknown req type in notpres_mshr!\n");
      return NOMSHR;
    }
  YS__errmsg("MSHR: this is supposed to be unreachable code\n");
  return NOMSHR;
}


/*****************************************************************************/
/* MSHRIterateUncoalesce: uncoalesce each request in the MSHR specified,     */
/* calling the function provided (to calculate statistics, etc.)             */
/*****************************************************************************/

int MSHRIterateUncoalesce(CACHE *captr, int mshr_num, int (*func_to_apply)(CACHE *, REQ *), enum MISS_TYPE miss_type)
{
  int i,latepf;

  /* First, apply the func to the mainreq */
#ifdef DEBUG_MSHR
  if (captr->mshrs[mshr_num].mainreq->miss_type != miss_type)
    YS__errmsg("Miss type mismatch!\n");
#endif
  /* if this MSHR had a late prefetch, set prefetched_late field to
     allow system to count every access coalesced into the MSHR as
     part of "late PF" time in statistics */
  latepf = (captr->mshrs[mshr_num].demand > 0.0); 
  captr->mshrs[mshr_num].mainreq->prefetched_late = latepf;
  
  func_to_apply(captr, captr->mshrs[mshr_num].mainreq);
  
  for(i=0;i<captr->mshrs[mshr_num].counter;i++)
    {
      /* In addition to latepf, also set miss_type of each coalesced
	 REQUEST to be the same as the type of the main request. This
	 allows all of these times to be counted as the appropriate
	 component of execution time. */

      captr->mshrs[mshr_num].coal_req[i]->miss_type = miss_type;
      captr->mshrs[mshr_num].coal_req[i]->prefetched_late = latepf;
      func_to_apply(captr, captr->mshrs[mshr_num].coal_req[i]);
    }

  return 0;
}


/*****************************************************************************/
/* RemoveMSHR: Remove the requests at the specified MSHR number and update   */
/* statistics. If a "subst" REQ is specified, substitute that one into the   */
/* MSHR without actually freeing the MSHR. Otherwise, free the MSHR.         */
/*****************************************************************************/

int RemoveMSHR(CACHE *captr, int mshr_num, REQ *subst)
{
  int set;
#ifdef DEBUG_MSHR
  if(captr->mshrs[mshr_num].valid != 1)
    YS__errmsg("Freeing invalid MSHR \n");
#endif
  
  /* We have to free mshr entry mshr_num */

  set = captr->mshrs[mshr_num].setnum;
  
#ifdef DEBUG_MSHR
  if (YS__Simtime > DEBUG_TIME)
    fprintf(simout,"MSHR: %s Going to free an MSHR for tag %ld, set %d\n",captr->name, captr->mshrs[mshr_num].mainreq->tag,set);
#endif
  
  if (captr->mshrs[mshr_num].demand > 0.0) /* in other words, there was a late prefetch */
    {
      StatrecUpdate(captr->pref_lateness,(double)(YS__Simtime-captr->mshrs[mshr_num].demand),1.0);
    }

  if (captr->mshrs[mshr_num].mainreq->s.type != COHE_REPLY) /* otherwise, we were using it as a temporary holding thing, like for a subst */
    {
      captr->reqmshr_count--;
      captr->reqs_at_mshr_count -= captr->mshrs[mshr_num].counter +1;
    }
  
  /* subst is used to put a WRB or such request into a smart MSHR
     in place of a former request */
  if (subst == NULL)
    {
      captr->mshr_count--;
      captr->mshrs[mshr_num].valid = 0;
    }
  else
    {
#ifdef DEBUG_MSHR
      if (YS__Simtime > DEBUG_TIME)
	fprintf(simout,"MSHR: %s Substituting in %s for tag %ld, set %d\n",captr->name, Req_Type[subst->req_type],captr->mshrs[mshr_num].mainreq->tag,set);
#endif
  
      captr->mshrs[mshr_num].mainreq = subst;
      /* keep valid, total mshr count constant */
    }

  StatrecUpdate(captr->mshr_req_count,(double)captr->reqs_at_mshr_count,YS__Simtime);
  StatrecUpdate(captr->mshr_occ,(double)captr->reqmshr_count,YS__Simtime);
  return 0;
}

/*****************************************************************************/
/* FindInMshrEntries: Find which MSHR is being used by a current REQUEST     */
/*****************************************************************************/

int FindInMshrEntries(CACHE *captr,REQ *req)
{
  int hit= -1, i;
  
  if(captr->mshr_count == 0)
    hit = -1;
  else
    {
      /* Let us see when the first hit occurs */
      for(i=0;i<captr->max_mshrs;i++)
	{
	  if((captr->mshrs[i].valid == 1) &&
	     (req->tag == captr->mshrs[i].mainreq->tag))
	    {
	      hit = i;
	      break;
	    }
	}
    }
  
  return hit;
}

/*****************************************************************************/
/* GetCoheReq: Get the type of COHE merged with the MSHR, if any.            */
/*****************************************************************************/

enum ReqType GetCoheReq(CACHE *captr, REQ *req, int mshr_num)
{
  return (captr->mshrs[mshr_num].pend_cohe_type);
}
