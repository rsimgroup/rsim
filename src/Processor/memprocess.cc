/*
   Processor/memprocess.cc

   This file provides most of the interface between the memory
   hierarchy and the processor.
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


#include "Processor/instance.h"
#include "Processor/instruction.h"
#include "Processor/state.h"
#include "Processor/memory.h"
#include "Processor/exec.h"
#include "Processor/memprocess.h"
#include "Processor/mainsim.h"
#include "Processor/processor_dbg.h"
#include "Processor/simio.h"
#include <stdlib.h>
#include <limits.h>

extern "C"
{
#include "MemSys/cpu.h"
#include "MemSys/simsys.h"
#include "MemSys/req.h"
#include "MemSys/cache.h"
#include "MemSys/arch.h"
#include "MemSys/misc.h"
}

unsigned lowsimmed; /* this needs to get initialized in mainsim */
state *MemPProcs[MAX_MEMSYS_PROCS];


int captr_block_bits;

/***************************************************************************/
/* StartUpMemRef : Set up arguments and pass a memory reference to MemSys  */
/***************************************************************************/


int StartUpMemRef(state *proc,instance *inst)
{
  int acc_type,flagvar,phys_addr,flagval;
  
  flagvar=0;
  flagval=0;
  inst->global_perform=0; // clear this bit!!
  acc_type=mem_acctype[inst->code->instruction];
  
  phys_addr= inst->addr-PROC_TO_MEMSYS;
#ifdef COREFILE
  if(proc->curr_cycle > DEBUG_TIME)
    fprintf(corefile,"rsim address %d changed to MemSys address %d\n",inst->addr,
	    phys_addr);
#endif
  addrinsert(proc,inst,inst->tag,phys_addr,acc_type,
	     0, /* dubref is for expansion -- to later support unaligned
		   accesses that spans cache lines */
	     flagvar,flagval,
	     YS__ProcArray[proc->proc_id], inst->time_addr_ready, inst->time_active_list);

  return 0;
}

/*************************************************************************/
/* IssuePrefetch : Send prefetches down to the memory heirarchy          */
/*************************************************************************/

int IssuePrefetch(state *proc,unsigned addr, int level, int excl, int inst_tag)
{
  
#ifdef COREFILE
  if(proc->curr_cycle > DEBUG_TIME)
    fprintf(corefile,"Processor %d sending out %s prefetch on address %d\n",proc->proc_id,
	    excl ? "exclusive" : "shared" ,addr);
#endif

  int preftype; 
  if (excl && level == 1)
    preftype = L1WRITE_PREFETCH;
  else if (!excl && level == 1)
    preftype = L1READ_PREFETCH;
  else if (excl && level == 2)
    preftype = L2WRITE_PREFETCH;
  else if (!excl && level == 2)
    preftype = L2READ_PREFETCH;
  else
    {
      fprintf(simerr,"Unkown prefetch type in IssuePrefetch!!\n");
      exit(-1);
    }
  
  addrinsert(proc,NULL,inst_tag,addr-PROC_TO_MEMSYS,preftype,
	     0,0,0,
	     YS__ProcArray[proc->proc_id], YS__Simtime,0.0);

  return 0;
}


/**************************************************************************/
/* MemDoneHeapInsert :  Wrapper around MemDoneHeap.Insert + updates stats */
/**************************************************************************/

extern "C" void MemDoneHeapInsert(REQ *req, MISS_TYPE miss_type) // state *proc, instance *inst, int inst_tag)
{
  state *proc=req->s.proc;
  instance *inst=req->s.inst;
  int inst_tag=req->s.inst_tag;

  if (miss_type == mtUNK)
    {
      fprintf(simerr,"An UNKNOWN miss type arrived at time %.1f\n",YS__Simtime);
      exit(-1);
    }
  
  switch (req->prcr_req_type)
    {
    case READ:
      StatrecUpdate(proc->readacc,double(YS__Simtime-req->mem_start_time),1.0);
      StatrecUpdate(proc->readact,double(YS__Simtime-req->active_start_time),1.0);
      StatrecUpdate(proc->readiss,double(YS__Simtime-req->issue_time),1.0);
      StatrecUpdate(proc->demand_read[req->handled],double(YS__Simtime-req->mem_start_time),1.0);
      StatrecUpdate(proc->demand_read_act[req->handled],double(YS__Simtime-req->active_start_time),1.0);
      StatrecUpdate(proc->demand_read_iss[req->handled],double(YS__Simtime-req->issue_time),1.0);
      break;
    case WRITE:
      StatrecUpdate(proc->writeacc,double(YS__Simtime-req->mem_start_time),1.0);
      StatrecUpdate(proc->writeact,double(YS__Simtime-req->active_start_time),1.0);
      StatrecUpdate(proc->writeiss,double(YS__Simtime-req->issue_time),1.0);
      StatrecUpdate(proc->demand_write[req->handled],double(YS__Simtime-req->mem_start_time),1.0);
      StatrecUpdate(proc->demand_write_act[req->handled],double(YS__Simtime-req->active_start_time),1.0);
      StatrecUpdate(proc->demand_write_iss[req->handled],double(YS__Simtime-req->issue_time),1.0);
      break;
    case RMW:
      StatrecUpdate(proc->rmwacc,double(YS__Simtime-req->mem_start_time),1.0);
      StatrecUpdate(proc->rmwact,double(YS__Simtime-req->active_start_time),1.0);
      StatrecUpdate(proc->rmwiss,double(YS__Simtime-req->issue_time),1.0);
      StatrecUpdate(proc->demand_rmw[req->handled],double(YS__Simtime-req->mem_start_time),1.0);
      StatrecUpdate(proc->demand_rmw_act[req->handled],double(YS__Simtime-req->active_start_time),1.0);
      StatrecUpdate(proc->demand_rmw_iss[req->handled],double(YS__Simtime-req->issue_time),1.0);
      break;
    case L1READ_PREFETCH:
    case L2READ_PREFETCH:
      StatrecUpdate(proc->pref_sh[req->handled],double(YS__Simtime-req->mem_start_time),1.0);
      break;
    case L1WRITE_PREFETCH:
    case L2WRITE_PREFETCH:
      StatrecUpdate(proc->pref_excl[req->handled],double(YS__Simtime-req->mem_start_time),1.0);
      break;
    default:
      YS__errmsg("Unknown processor request type!\n");
      break;
    }
  
  if (!req->s.prefetch)
    {
#ifdef COREFILE
      if(YS__Simtime > DEBUG_TIME)
	fprintf(simout,"RSIM processor completes address %ld tag %ld inst_tag %d @ %g\n",
	       req->address,req->tag,req->s.inst_tag, YS__Simtime);
#endif
      
      if (inst->tag == inst_tag) /* otherwise, it's been reassigned as a result of exception */
	{
	  proc->MemDoneHeap.insert(proc->curr_cycle,inst,inst_tag);

	  // if (req->handled != reqL1HIT)
	  inst->miss=miss_type;
	  inst->latepf=req->prefetched_late;
	}
      else
	{
#ifdef COREFILE
	  if(proc->curr_cycle > DEBUG_TIME)
	    fprintf(corefile,"MemDoneHeapInsert: reassigned instance!\n");
#endif
	}
    }
}

/*************************************************************************/
/* GlobalPerform : Make the effect of reads and writes visible to the    */
/*               : simulator (read/write the unix address space)         */
/*************************************************************************/


extern "C" void GlobalPerform(REQ *req)
{
  if (req->s.inst == NULL || req->s.inst->global_perform || req->s.prefetch) /* then nothing to do */
    return;
  if (req->s.inst_tag==req->s.inst->tag) // to handle times when reused by exception, prediction, etc
    {
      if (req->s.inst->vsbfwd)
	{
	  if (req->s.inst->memprogress == -1) // standard for when issued
	    {
	      req->s.inst->memprogress = req->s.inst->vsbfwd;
	      req->s.inst->vsbfwd = 0;
	      req->s.inst->global_perform = 1;
	    }
	  else
	    {
	      // if instruction got killed, don't reset memprogress
	      req->s.inst->vsbfwd = 0;
	      req->s.inst->global_perform = 1;
	    }
	  
	}
      else /* Call the instruction function in funcs.cc */
	{
	  DoMemFunc(req->s.inst,req->s.proc);
	}
    }
}


/*************************************************************************/
/* SpecLoadBufCohe: Called in systems with speculative loads whenever    */
/* a coherence comes to the L1 cache (whether an invalidation or a       */
/* replacement from L2). This function checks the coherence message      */
/* against outstanding speculative loads to determine if any needs to    */
/* be rolled back.                                                       */
/*************************************************************************/

extern "C" int SpecLoadBufCohe(int proc_id,int rtag, SLBFailType fail)
{
  state *proc=MemPProcs[proc_id];
  except e_code;
  MemQLink<instance *> *slbindex;

  if (fail == SLB_Cohe)
    e_code = SOFT_SL_COHE;
  else if (fail == SLB_Repl)
    e_code = SOFT_SL_REPL;
  else
    YS__errmsg("Unknown speculative load type\n");

#ifdef STORE_ORDERING
  slbindex = proc->MemQueue.GetNext(NULL);
  // skip the first element, since this is the active one by SC/PC standards
#else // RC
  slbindex = NULL;
  // in RC you might need to even look at the first load, since you
  // might just be waiting on previous stores
#endif
  
  
  
#ifdef STORE_ORDERING
  int specstart; /* this indicates whether the operations being
		    investigated are speculative or not. In the case
		    of SC, all reads other than the head of the queue
		    are speculative. In PC, all reads after some other
		    read are speculative */
  if (Processor_Consistency)
    {
      specstart=0;
      if (slbindex && (!IsStore(slbindex->d) || IsRMW(slbindex->d)))
	specstart=1;
      
    }
  else // SC
    {
      specstart=1;
    }
#else // RC
  int minspec = -1;
  if (proc->LLtag >= 0)
    minspec = proc->LLtag;
  else if (proc->SLtag >= 0 && proc->SLtag < minspec)
    minspec = proc->SLtag;
  if (minspec == -1) /* no way we're speculating */
    return 0;
#endif

  while ((slbindex =
#ifdef STORE_ORDERING
	  proc->MemQueue.GetNext(slbindex)
#else
	  proc->LoadQueue.GetNext(slbindex)
#endif
	  ) != NULL)
    
    {
      instance *d=slbindex->d;
      if (d->memprogress && ((unsigned)rtag == ((d->addr-PROC_TO_MEMSYS) >> captr_block_bits))
#ifdef STORE_ORDERING
	  && specstart
#else // RC
	  && (d->tag > minspec)
#endif
	  )
	{
#ifdef COREFILE
	  if (YS__Simtime > DEBUG_TIME)
	    fprintf(corefile,"Marking consistency violation on tag %d in SLBCohe\n",d->tag);
#endif
	  
	  if (d->exception_code == OK || d->exception_code == SERIALIZE)
	    d->exception_code=e_code;
	  
	  proc->active_list->flag_exception_in_active_list(d->tag,e_code);
	  d->exception_code = e_code;
	}
#ifdef STORE_ORDERING
      if (Processor_Consistency && !specstart && (!IsStore(d) || IsRMW(d)))
	specstart=1;
#endif
    }
  
  return 0;
}

/*************************************************************************/
/* Wrapper functions to interface to the various MemSys routines         */
/*************************************************************************/


extern "C" ARG *GetL2ArgPtr(state *proc)
{
  return(proc->l2_argptr);
}

extern "C" ARG *GetL1ArgPtr(state *proc)
{
  return (proc->l1_argptr);
}
extern "C" ARG *GetWBArgPtr(state *proc)
{
  return(proc->wb_argptr);
}

extern "C" void FreeAMemUnit(state *proc, int tag)
{
  // do this when you commit a request from the L1 ports
#ifdef COREFILE
  if (YS__Simtime > DEBUG_TIME)
    fprintf(proc->corefile,"Freeing memunit for tag %d\n",tag);
#endif
  proc->FreeingUnits.insert(proc->curr_cycle,uMEM);
}

extern "C" void AckWriteToWBUF(instance *inst, state *proc)
{
  proc->active_list->mark_done_in_active_list(inst->tag,inst->exception_code, proc->curr_cycle-1);
}
