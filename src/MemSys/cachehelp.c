/*
  cachehelp.c

  Auxiliary functions used in caches --
  used to hide routing from cache modules, to do all processing for REPLYs,
  to implement smart MSHRs, to bounce back REPLYs when there are too many
  UPGRADEs to a given set outstanding, and to implement cache-to-cache
  transfers

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


#include "MemSys/net.h"
#include "MemSys/req.h"
#include "MemSys/cache.h"
#include "MemSys/mshr.h"
#include "MemSys/simsys.h"
#include "MemSys/miss_type.h"
#include "Processor/memprocess.h"
#include "Processor/simio.h"
#include <malloc.h>

/***************************************************************************/
/* AddReqToOutQ: This REQ should be placed in its output port, if there is */
/* space available. If there is space, the function returns 1 to indicate  */
/* success. If there is no space, the function returns 0 and does not add  */
/* anything into the output port.                                          */
/***************************************************************************/

int AddReqToOutQ(CACHE *captr,REQ *req)
{
  /* When we come here the req has all it direction and abv/blw set suitably */
  /* return 0 on failure, 1 on success */
  int oport_num;
  oport_num =  captr->routing((SMMODULE *)captr, req);
  if(oport_num == -1)
    YS__errmsg("Cache routing routine unable to route this request \n");
  
  if (captr->out_port_ptr[oport_num]->ov_req) {
    /* the port is busy */
    return 0;
  }
  else
    {
      new_add_req(captr->out_port_ptr[oport_num],req);
      return 1;
    }
}

/***************************************************************************/
/* Reply-handling functions: These functions are called on REPLYs to call  */
/* GlobalPerform on each object that may have coalesced with the REPLY at  */
/* various stages of the memory hierarchy, and possibly to send these      */
/* REPLYs back to the processor. When possible, these also free the REQ    */
/* data structures associated with the original and coalesced accesses     */
/***************************************************************************/

/* GlobalPerform all coalesced operations; add all to processor MemDoneHeap,
   and frees the REQ data structures. Called by L1 */
int GlobalPerformAndHeapInsertAllCoalesced(CACHE *captr, REQ *req)
{
  REQ *tmp;
  int i;
  enum MISS_TYPE miss_type = req->miss_type; /* the miss_type of the REPLY
						will be used for all of the
						coalesced REQUESTs, to provide
						meaningful statistics */
  GlobalPerform(req);
  MemDoneHeapInsert(req,miss_type); /* add to memory unit heap */
  for (i=0; i< req->coal_count; i++) /* step through the coalesced array */
    {
      tmp = req->coal_req_array[i];
      GlobalPerform(tmp);
      MemDoneHeapInsert(tmp,miss_type);
      YS__PoolReturnObj(&YS__ReqPool,tmp); /* free each coalesced req */
    }
  if (req->coal_req_array != NULL)
    {
      free(req->coal_req_array); /* free up the coalesced array */
      req->coal_req_array = NULL;
    }
  YS__PoolReturnObj(&YS__ReqPool, req); /* free the REQ data structure */
  return 0;           
}

/* GlobalPerform only the coalesced writes and L2 prefetches; also
   adds these to processor MemDoneHeap and frees the REQ structures.
   Called by L1WT */
int GlobalPerformAndHeapInsertAllCoalescedWritesOnly(CACHE *captr, REQ *req)
{
  REQ *tmp;
  int i;

  /* count L2 prefetchs as write-like, since they are not allocated in L1WT */
  enum MISS_TYPE miss_type = req->miss_type;
  
  if(req->prcr_req_type == WRITE || req->prcr_req_type == L2WRITE_PREFETCH || req->prcr_req_type == L2READ_PREFETCH){
    GlobalPerform(req);
    MemDoneHeapInsert(req,miss_type);
  } 
  for (i=0; i< req->coal_count; i++)
    {
      tmp = req->coal_req_array[i];
      if(tmp->prcr_req_type == WRITE || tmp->prcr_req_type == L2WRITE_PREFETCH || tmp->prcr_req_type == L2READ_PREFETCH){
	GlobalPerform(tmp);
	MemDoneHeapInsert(tmp,miss_type);
	YS__PoolReturnObj(&YS__ReqPool,tmp);
      }
    }
  if (req->coal_req_array != NULL)
    {
      free(req->coal_req_array);
      req->coal_req_array = NULL;
    }
  if(req->prcr_req_type == WRITE || req->prcr_req_type == L2WRITE_PREFETCH || req->prcr_req_type == L2READ_PREFETCH ){
    YS__PoolReturnObj(&YS__ReqPool, req);
  }
  return 0;
}

/* GlobalPerform only the coalesced L2 prefetches; also adds these to
   processor MemDoneHeap and frees the REQ structures.
   Called by L1WB */
int GlobalPerformAndHeapInsertAllCoalescedL2PrefsOnly(CACHE *captr, REQ *req)
{
  REQ *tmp;
  int i;

  /* count L2 prefetchs as write-like, since they are not allocated in L1WT */
  
  enum MISS_TYPE miss_type = req->miss_type;
  if(req->prcr_req_type == L2WRITE_PREFETCH || req->prcr_req_type == L2READ_PREFETCH){
    GlobalPerform(req);
    MemDoneHeapInsert(req,miss_type);
  } 
  for (i=0; i< req->coal_count; i++)
    {
      tmp = req->coal_req_array[i];
      if(tmp->prcr_req_type == L2WRITE_PREFETCH || tmp->prcr_req_type == L2READ_PREFETCH)
	{
	  GlobalPerform(tmp);
	  MemDoneHeapInsert(tmp,miss_type);
	  YS__PoolReturnObj(&YS__ReqPool,tmp);
	}
    }
  if (req->coal_req_array != NULL)
    {
      free(req->coal_req_array);
      req->coal_req_array = NULL;
    }
  if(req->prcr_req_type == L2WRITE_PREFETCH || req->prcr_req_type == L2READ_PREFETCH ){
    YS__PoolReturnObj(&YS__ReqPool, req);
  }
  return 0;
}

/* Calls GlobalPerform, but does not add to heap or free REQ data structures.
   Called at L2.*/
int GlobalPerformAllCoalesced(CACHE *captr, REQ *req)
{
  int i;
  
  GlobalPerform(req);
  for (i=0; i< req->coal_count; i++)
    {
      GlobalPerform(req->coal_req_array[i]);
    }
  return 0;
}

/***************************************************************************/
/* Smart MSHR functions: These functions maintain a smart MSHR list for    */
/* each cache. Smart MSHRs are used when the cache holds a resource for    */
/* some particular access that must be sent out and uses that resource to  */
/* buffer the access so that the access does not stall any othe part of    */
/* the cache processing. In some cases, the resource will be freed once    */
/* the smart MSHR access is sent out. Note that these don't just include   */
/* MSHRs -- at the L2 cache, WRB-buffer entries are also counted.          */
/* Smart MSHRs are important in any case where stalling could lead to      */
/* deadlocks.                                                              */
/***************************************************************************/


struct YS__SmartMSHRlist
{
  REQ *req;     /* REQ that should be sent out from the list */
  int mshr_num; /* resource identifier number */
  int (*freemshr)(CACHE *,int,REQ*); /* optional function to call on
					completion to free the resource.
					Third argument is always NULL. */
  struct YS__SmartMSHRlist *next;
};


/* add an entry to the smart MSHR list */
void AddToSmartMSHRList(CACHE *captr, REQ *req, int mshr_num, int (*freemshr)(CACHE *,int,REQ*))
{
  struct YS__SmartMSHRlist *listmem = malloc(sizeof(struct YS__SmartMSHRlist));
  listmem->next=NULL;
  listmem->req=req;
  listmem->mshr_num=mshr_num;
  listmem->freemshr=freemshr;
  if (captr->SmartMSHRTail)
    {
      captr->SmartMSHRTail->next=listmem;
    }
  else
    {
      captr->SmartMSHRHead = listmem;
    }
  captr->SmartMSHRTail = listmem;
  captr->num_in_pipes++; /* cache should never go to sleep while there
			    are smart MSHRs outstanding, so act as though
			    this were in a cache pipe */
  captr->pipe_empty = 0;
}

/* remove head element from smart MSHR list */
void RemoveSmartMSHRHead(CACHE *captr)
{
  struct YS__SmartMSHRlist *old;
  old = captr->SmartMSHRHead;
  if ((captr->SmartMSHRHead = captr->SmartMSHRHead->next) == NULL)
    captr->SmartMSHRTail = NULL;

  free(old);
}

/* recycle head element of smart MSHR list -- push it to end so all */
/* accesses can be retried */
void RecycleSmartMSHRHead(CACHE *captr)
{
  struct YS__SmartMSHRlist *old;
#ifdef DEBUG_MSHR
  if (YS__Simtime > DEBUG_TIME)
    fprintf(simout,"%s: recycling smart mshr head @%.1f\n",captr->name,YS__Simtime);
#endif
  old = captr->SmartMSHRHead;
  if ((captr->SmartMSHRHead = captr->SmartMSHRHead->next) == NULL)
    {
      /* in this case, there was only one item */
      captr->SmartMSHRHead=captr->SmartMSHRTail=old;
    }
  else
    {
      /* head has been set; now fix up tail. */
      captr->SmartMSHRTail->next=old;
      captr->SmartMSHRTail=old;
      old->next=NULL;
    }
}

/* Caches call ProcessFromSmartMSHRList every cycle to try to send out
   an access from smart MSHR and possibly free up the resource. */
int ProcessFromSmartMSHRList(CACHE *captr, int max_to_remove)
{
  int removed = 0;
  while (captr->SmartMSHRHead && removed < max_to_remove) /* anything available? */
    {
      if (AddReqToOutQ(captr,captr->SmartMSHRHead->req)) /* it can be sent out. */
	{
	  if (captr->SmartMSHRHead->freemshr) /* is there a resource-freeing function set? */
	    {
#ifdef DEBUG_MSHR
	      if (YS__Simtime > DEBUG_TIME)
		{
		  fprintf(simout,"Process SmartMSHR %s: freeing smart mshr %d related to tag %ld\n",captr->name,captr->SmartMSHRHead->mshr_num,captr->SmartMSHRHead->req->tag);
		}
#endif
	      /* call the resource-freeing function with the cache pointer,
		 the resource identifier, and the NULL pointer. The
		 NULL pointer was chosen as the last argument so that
		 RemoveMSHR could be used directly when it needed to be */
	      
	      (*(captr->SmartMSHRHead->freemshr))(captr,captr->SmartMSHRHead->mshr_num,NULL);
	    }
	  RemoveSmartMSHRHead(captr); /* free up the head entry */
	  removed++;
	  captr->num_in_pipes--; /* don't need to count this access any more */
	  if (captr->num_in_pipes == 0) /* was this the last thing in pipes? */
	    captr->pipe_empty = 1; /* then, cache pipes are now empty */
	}
      else
	break;
    }
  return removed;
}

/***************************************************************************/
/* NackUpgradeConflictReply: This function is called when cache gets a     */
/* reply which can't victimize a line because all the lines in the set     */
/* have upgrades outstanding.  This function puts the request's "Smart     */
/* MSHR" in contention in the smart MSHR queue.  The REPLY is going to be  */
/* "NACKed" down to its next level (the original REQUEST will be sent      */
/* down) If it's from the L2, send it back "preprocessed" to the           */
/* directory. Directory will simply mirror it.  If it's from the L1, send  */
/* it down to L2 as a regular request.                                     */
/***************************************************************************/

void NackUpgradeConflictReply(CACHE *captr, REQ *req)
{
  captr->stat.replies_nacked++; /* collect stats on this occurrence */

  /* Turn the access back into a REQUEST */
  req->s.type = REQUEST;
  req->s.dir = REQ_FWD;
  req->s.route = BLW;
  AddToSmartMSHRList(captr,req,req->mshr_num,NULL);
}

/***************************************************************************/
/* Cache-to-cache transfer support functions.                              */
/*                                                                         */
/* MakeForwardAck makes the acknowledgment (possibly with data) that is    */
/* sent back to the directory as the result of a cache-to-cache transfer   */
/* request.  The acknowledgment is actually a separately allocated REQ     */
/* data structure.                                                         */
/*                                                                         */
/* MakeForward turns the REQ passed in into a cache-to-cache transfer      */
/* reply that will be sent to the specified node (according to the         */
/* "forward_to" field)                                                     */
/***************************************************************************/

REQ *MakeForwardAck(REQ *req, CacheLineState cur_state) {

  REQ *tempreq;
  char *temp1, *temp2;
  
  /* First, create the REQ data structure for the ack */
  tempreq = (REQ *) YS__PoolGetObj(&YS__ReqPool);
  temp1 = tempreq->pnxt;
  temp2 = tempreq->pfnxt;
  *tempreq = *req;
  tempreq->pnxt = temp1;
  tempreq->pfnxt = temp2;
  
  /* these next few lines insure that if the request is a COPYBACK
     (shared) from dirty that a copyback to the directory also
     occurs. Note that a copyback isn't needed in the case of a
     COPYBACK_INVL (or COPYBACK_SHDY from PR_DY) request, since the
     desired line will still be private and dirty, or in the case of a
     COPYBACK from PR_CL since the directory already had the data. */
  if (req->req_type == COPYBACK && cur_state != PR_CL) {
    tempreq->size_st = LINESZ + REQ_SZ;
    tempreq->size_req = LINESZ + REQ_SZ;
  }
  else {
    tempreq->size_st = REQ_SZ;
    tempreq->size_req = REQ_SZ;
  }

  /* Set up the routing fields for a COHE_REPLY. */
  tempreq->s.dir = REQ_FWD;
  tempreq->s.route = BLW;
  tempreq->s.reply = REPLY;
  tempreq->s.type = COHE_REPLY;

  return tempreq;
}

void MakeForward(CACHE *captr,REQ *req, CacheLineState cur_state)
{
  /* Turn the specified req into a cache-to-cache xfer REPLY */
  
  if ((req->req_type == COPYBACK_INVL) || (req->req_type == COPYBACK) || (req->req_type == COPYBACK_SHDY)) { /* an actual $-$ request */
#ifdef DEBUG_SECONDARY
    if (DEBUG_TIME < YS__Simtime) {
      fprintf(simout,"%s forward %d %d %1.0f\n", captr->name, req->forward_to, captr->node_num,
	     YS__Simtime);
      fprintf(simout,"Forward address : %d\n", (int)req);
    }
#endif
    /* FORWARD */
    req->s.reply = REPLY; /* but don't change the s.type until cache
			     can move it out, for req->progress reasons */
    
    if (req->req_type == COPYBACK_INVL) /* $-$ + ack */
      {
	if (cur_state == PR_DY || cur_state == SH_DY)
	  req->req_type = REPLY_EXCLDY; /* receiver should immediately take
					   line in PR_DY state, since
					   directory was never sent a copy of
					   the data */
	else /* line being held in clean state, so it's same as directory */
	  req->req_type = REPLY_EXCL;
      }
    else if (cur_state == PR_DY && req->req_type == COPYBACK_SHDY) /* for expansion */
      req->req_type = REPLY_SHDY;
    else /* a copyback of some sort */
      req->req_type = REPLY_SH;

    /* Set up routing fields for cache-to-cache xfer */
    req->s.dir = REQ_BWD;
    req->size_st = LINESZ + REQ_SZ;
    req->size_req = LINESZ + REQ_SZ;
    req->src_node = req->forward_to; /* Send it to the node that requested it */
    req->dest_node = captr->node_num;

    /* statistics fields for $-$ xfer */
    req->handled = reqCACHE_TO_CACHE; /* cache-to-cache xfer */
    req->miss_type = mtREM; /* remote miss */
  }

}
