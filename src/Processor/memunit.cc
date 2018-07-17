/*

  memunit.cc

  This file makes up the bulk of the processor memory system, enforcing
  the uniprocessor and multiprocessor ordering constraints.
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
#include "Processor/state.h"
#include "Processor/units.h"
#include "Processor/exec.h"
#include "Processor/memory.h"
#include "Processor/memprocess.h"
#include "Processor/memq.h"
#include "Processor/decode.h"
#include "Processor/mainsim.h"
#include "Processor/active.h"
#include "Processor/branchq.h"
#include "Processor/processor_dbg.h"
#include "Processor/simio.h"
#include <limits.h>
#include <stdlib.h>

int membarprime = 0;

int INSTANT_ADDRESS_GENERATE = 0;

extern "C"
{
#include "MemSys/simsys.h"
#include "MemSys/cache.h"
#include "MemSys/req.h"
#include "MemSys/arch.h"
}

/************************************************************/
/* Overlap: returns 1 if there is overlap between addresses */
/*        : accessed by references a and b                  */
/************************************************************/

#define overlap(a,b) ((a->finish_addr >= b->finish_addr && b->finish_addr >= a->addr) || \
		      (b->finish_addr >= a->finish_addr && a->finish_addr >= b->addr))

#define BAILOUT_TIME 5000
     
ReqType mem_acctype[numINSTRS];
int mem_length[numINSTRS];
int mem_align[numINSTRS];
int CompleteMemOp(instance *inst, state *proc);

/***************************************************************************/
/* IsStore : given a memory operation, identify if it is a store operation */
/***************************************************************************/


int IsStore(instance *inst)
{
  int a = mem_acctype[inst->code->instruction];
  return (a != READ);
}

/*************************************************************************/
/* IsRMW   : given a memory operation, identify if it is of rmw class    */
/*************************************************************************/

int IsRMW(instance *inst)
{
  int acc=mem_acctype[inst->code->instruction];
  return (acc != READ) && (acc != WRITE) && (acc != WRITEFLAG);
}

/*************************************************************************/
/* GetAddr: calculate the simulated address for the instruction          */
/*************************************************************************/

unsigned GetAddr(instance *memop)
{
  unsigned addr;
  if (memop->addrdep != 0)
    return 0;

  if (memop->code->instruction == iCASA || memop->code->instruction == iCASXA)
    addr = memop->rs2vali;
  else if (memop->code->aux1)
    addr = memop->rs2vali+memop->code->imm;
  else
    addr = memop->rs2vali+memop->rsccvali;

  if ((addr & (mem_length[memop->code->instruction] - 1)) != 0) // not aligned by length
    {
      memop->exception_code = BUSERR;
      /* mark it a bus error. We'll check this later before issuing... */
    }
  
  if (memop->code->instruction == iSTFSR || memop->code->instruction == iSTXFSR)
    {
      memop->exception_code = SERIALIZE; /* important to mark this before it takes place */
    }
    
  return addr;
}

extern "C" double drand48();

#ifdef COREFILE

/*************************************************************************/
/* OpCompletionMessage : print debug information at inst completion time */
/*************************************************************************/

static void OpCompletionMessage(instance *inst, state *proc)
{
  if(proc->curr_cycle > DEBUG_TIME)
    {
      fprintf(corefile, "Completed executing tag = %d, %s, ", inst->tag,inames[inst->code->instruction]);

      switch (inst->code->rd_regtype)
	{
	case REG_INT:
	  fprintf(corefile, "rdi = %d, ",inst->rdvali);
	  break;
	case REG_FP:
	  fprintf(corefile, "rdf = %f, ",inst->rdvalf);
	  break;
	case REG_FPHALF:
	  fprintf(corefile, "rdfh = %f, ",double(inst->rdvalfh));
	  break;
	case REG_INTPAIR:
	  fprintf(corefile, "rdp = %d/%d, ",inst->rdvalipair.a,inst->rdvalipair.b);
	  break;
	case REG_INT64:
	  fprintf(corefile, "rdll = %lld, ",inst->rdvalll);
	  break;
	default: 
	  fprintf(corefile, "rdX = XXX, ");
	  break;
	}
      
      
      fprintf(corefile, "rccvali = %d \n", inst->rccvali);

    }
  
}
#endif

/*************************************************************************/
/* GenerateAddress : Simulate the access to the address generation unit  */
/*************************************************************************/

static void GenerateAddress(instance *inst, state *proc)
{
  if (INSTANT_ADDRESS_GENERATE)
    {
      Disambiguate(inst,proc);
    }
  else
    {
      if (proc->UnitsFree[uADDR] == 0)
	{
	  inst->stallqs++;
	  proc->UnitQ[uADDR].AddElt(inst,proc);
	}
      else
	{
	  issue(inst,proc); // issue takes it through address generation
	}
    }
}


/*************************************************************************/
/* CalculateAddress : Calculate memory address and issue the memory oprn */
/*                    to the address generation unit                     */
/*************************************************************************/

void CalculateAddress(instance *inst, state *proc)
{
  inst->addr=GetAddr(inst);
  inst->finish_addr=inst->addr + mem_length[inst->code->instruction] -1;
  GenerateAddress(inst,proc);
}

/*************************************************************************/
/* MemMatch : check for match between load and store and also forward    */
/*          : values where possible. Returns 1 on success, 0 on failure  */
/*************************************************************************/

static int MemMatch(instance *ld, instance *st) // check for match and also copy
{
  INSTRUCTION load = ld->code->instruction;
  INSTRUCTION store = st->code->instruction;
  
  switch (store)
    {
    case iSTW:
      if ((load == iLDUW) || (load == iLDSW) || (load == iLDUWA))
	{
#ifdef COREFILE
	  if(GetSimTime() > DEBUG_TIME) 
	    fprintf(corefile,"Forwarded value %d from %d to %d\n",st->rs1vali,st->tag,ld->tag);
#endif
	  ld->rdvali=st->rs1vali;
	  return 1;
	}
      else if (load == iLDF)
	{
	  float tmp;
	  int *addr = (int *)&tmp;
	  *addr = st->rs1vali;
#ifdef COREFILE
	  if(GetSimTime() > DEBUG_TIME) 
	    fprintf(corefile,"Forwarded value %d as %f from %d to %d\n",st->rs1vali,tmp,st->tag,ld->tag);
#endif
	  ld->rdvalfh = tmp;
	  return 1;
	}
	return 0;
      break;
    case iSTD:
      if (load == iLDD)
	{
#ifdef COREFILE
	  if(GetSimTime() > DEBUG_TIME) 
	    fprintf(corefile,"Forwarded value %d/%d from %d to %d\n",st->rs1valipair.a,st->rs1valipair.b,st->tag,ld->tag);
#endif
	  ld->rdvalipair.a=st->rs1valipair.a;
	  ld->rdvalipair.b=st->rs1valipair.b;
	  return 1;
	}
      else
	return 0;
      break;
    case iSTB:
      if ((load == iLDUB) || (load == iLDSB))
	{
#ifdef COREFILE
	  if(GetSimTime() > DEBUG_TIME) 
	    fprintf(corefile,"Forwarded value %d from %d to %d\n",st->rs1vali,st->tag,ld->tag);
#endif
	  ld->rdvali=st->rs1vali;
	  return 1;
	}
      else
	return 0;
      break;
    case iLDSTUB:
      if (load == iLDUB)
	{
#ifdef COREFILE
	  if(GetSimTime() > DEBUG_TIME) 
	    fprintf(corefile,"Forwarded value %d from %d to %d\n",st->rs1vali,st->tag,ld->tag);
#endif
	  ld->rdvali=255;
	  return 1;
	}
      else
	return 0;
      break;
    case iSTH:
      if ((load == iLDUH) || (load == iLDSH))
	{
#ifdef COREFILE
	  if(GetSimTime() > DEBUG_TIME) 
	    fprintf(corefile,"Forwarded value %d from %d to %d\n",st->rs1vali,st->tag,ld->tag);
#endif
	  ld->rdvali=st->rs1vali;
	  return 1;
	}
      else
	return 0;
      break;
    case iSTDF:
      if (load == iLDDF)
	{
#ifdef COREFILE
	  if(GetSimTime() > DEBUG_TIME) 
	    fprintf(corefile,"Forwarded value %f from %d to %d\n",st->rs1valf,st->tag,ld->tag);
#endif
	  ld->rdvalf=st->rs1valf;
	  return 1;
	}
      else
	return 0;
      break;
    case iSTF:
      if (load == iLDF)
	{
#ifdef COREFILE
	  if(GetSimTime() > DEBUG_TIME) 
	    fprintf(corefile,"Forwarded value %f from %d to %d\n",st->rs1valfh,st->tag,ld->tag);
#endif
	  ld->rdvalfh=st->rs1valfh;
	  return 1;
	}
      else if (load == iLDSW || load == iLDUW)
	{
	  int tmp;
	  float *addr = (float *)&tmp;
	  *addr = st->rs1valfh;
#ifdef COREFILE
	  if(GetSimTime() > DEBUG_TIME) 
	    fprintf(corefile,"Forwarded value %f as %d from %d to %d\n",st->rs1valfh,tmp,st->tag,ld->tag);
#endif
	  ld->rdvali = tmp;
	  return 1;
	}
      else
	return 0;
      break;
    default:
      return 0;
      break;
    }
}

struct MemoryRequestState
{
  instance *inst;
  state *proc;
  
  MemoryRequestState(instance *i,state *p):inst(i),proc(p) {}
};

/*************************************************************************/
/* memory_latency : models the effect of latency spent on memory issue   */
/*                : use StartUpMemReference to interface with MemSys     */
/*************************************************************************/

int memory_latency(instance *inst, state *proc)
{
  GetMap(inst,proc); // just to see if we should execute it at all -- or does it give a seg fault
  if (inst->exception_code == SEGV || inst->exception_code == BUSERR ||
      (!simulate_ilp && IsStore(inst) && inst->exception_code != OK))
    /* the last condition takes care of places that mark_stores_ready
       would have handled for an ILP processor */
    {
      if (inst->addr >= lowsimmed && proc->MEMSYS) /* this one would have taken a unit, so we need to free it */
	{
	  proc->FreeingUnits.insert(proc->curr_cycle, uMEM);
#ifdef COREFILE
	  if(proc->curr_cycle > DEBUG_TIME)
	    {
	      fprintf(corefile,"Freeing memunit for tag %d\n",inst->tag);
	    }
#endif
	}

#ifndef STORE_ORDERING
      if (!simulate_ilp && IsStore(inst) && !IsRMW(inst))
	{
	  proc->active_list->mark_done_in_active_list(inst->tag,inst->exception_code, proc->curr_cycle-1);
	  return 0;
	}
#endif
      
      proc->MemDoneHeap.insert(proc->curr_cycle+1,inst,inst->tag);
      return 0;
    }

#ifndef STORE_ORDERING
  if (!simulate_ilp && IsStore(inst) && !IsRMW(inst))
    {
      inst->in_memunit=0;
      instance *memop = new instance(inst);
      proc->StoreQueue.Replace(inst,memop);
      inst = memop;

      if (inst->addr < lowsimmed || !proc->MEMSYS || L1TYPE == FIRSTLEVEL_WB)
	proc->active_list->mark_done_in_active_list(inst->tag,inst->exception_code, proc->curr_cycle-1);      /* otherwise, wait for it to hit WBUF */
    }
#endif
        
  if (inst->code->instruction == iPREFETCH)
    {
      // send out a prefetch
      
      int excl = inst->code->aux2 == PREF_1WT || inst->code->aux2 == PREF_NWT;
      int level = 1 + (Prefetch==2 || inst->code->aux2 == PREF_1WT || inst->code->aux2 == PREF_1RD);
      
      if (inst->addr >= lowsimmed && proc->MEMSYS && !drop_all_sprefs)
	IssuePrefetch(proc,inst->addr, level,excl,inst->tag);
      
      // the prefetch instruction itself should return instantly
      // a pref isn't like other memops; it can return even without
      // paying cache time
      
      inst->memprogress = 1;
      PerformMemOp(inst,proc);
#ifdef COREFILE
      OpCompletionMessage(inst,proc);
#endif
      proc->DoneHeap.insert(proc->curr_cycle+1,inst,inst->tag);
      return 0;
    }
  
  if (inst->addr < lowsimmed || !proc->MEMSYS)
    {
      int lat;
      lat = 1 *FASTER_PROC_L1; // one cycle added latency: best possible L1 hit
      
      proc->MemDoneHeap.insert(proc->curr_cycle + lat, inst,inst->tag);
      return lat;
    }
  else
    {
      if (StartUpMemRef(proc,inst) == -1) /* not accepted -- should never happen */
	{
	  fprintf(simerr,"StartUpMemRef rejected an operation.\n");
	  exit(1);
	}
      
      return 0;
    }
}

/*************************************************************************/
/* memory_rep : frees up the UMEM units for private/non-MemSys accesses  */
/*************************************************************************/

int memory_rep(instance *inst, state *proc)
{
  if (inst->addr < lowsimmed || !proc->MEMSYS)
    {
      proc->FreeingUnits.insert(proc->curr_cycle, uMEM);
#ifdef COREFILE
      if(proc->curr_cycle > DEBUG_TIME)
	{
	  fprintf(corefile,"Freeing memunit for tag %d\n",inst->tag);
	}
#endif
    }

  /* otherwise, count on the L1 cache to free up the mem units */

  return 0;
}

/*************************************************************************/
/* IssueOp : Issue a memory operation to the cache or the perfect memory */
/*         : system                                                      */
/*************************************************************************/

void IssueOp(instance *memop, state *proc)
{
  /* Note: we've already checked L1Q_FULL before coming here */
  if (memop->vsbfwd < 0)
    {
      proc->vsbfwds++;
    }
      
  memop->memprogress = -1;
  memop->issuetime=proc->curr_cycle;
  memop->time_issued = YS__Simtime;
  if (!IsStore(memop) && (memop->limbo || memop->kill))
    {
      fprintf(simerr,"%s:%d -- there's a limbo or kill in IssueOp!!! P%d,%d @ %d\n",__FILE__,__LINE__,proc->proc_id,memop->tag,proc->curr_cycle);
      exit(-1);
    }

  /* then do issuing code */
  proc->UnitsFree[uMEM]--;
  
#ifdef COREFILE
  if(proc->curr_cycle > DEBUG_TIME)
    {
      fprintf(corefile,"Consuming memunit for tag %d\n",memop->tag);
      fprintf(corefile,"Issue tag = %d, %s, ", memop->tag,inames[memop->code->instruction]);
      
      switch (memop->code->rs1_regtype)
	{
	case REG_INT:
	  fprintf(corefile, "rs1i = %d, ",memop->rs1vali);
	  break;
	case REG_FP:
	  fprintf(corefile, "rs1f = %f, ",memop->rs1valf);
	  break;
	case REG_FPHALF:
	  fprintf(corefile, "rs1fh = %f, ",double(memop->rs1valfh));
	  break;
	case REG_INTPAIR:
	  fprintf(corefile, "rs1p = %d/%d, ",memop->rs1valipair.a,memop->rs1valipair.b);
	  break;
	case REG_INT64:
	  fprintf(corefile, "rs1ll = %lld, ",memop->rs1valll);
	  break;
	default: 
	  fprintf(corefile, "rs1X = XXX, ");
	  break;
	}
      
      switch (memop->code->rs2_regtype)
	{
	case REG_INT:
	  fprintf(corefile, "rs2i = %d, ",memop->rs2vali);
	  break;
	case REG_FP:
	  fprintf(corefile, "rs2f = %f, ",memop->rs2valf);
	  break;
	case REG_FPHALF:
	  fprintf(corefile, "rs2fh = %f, ",double(memop->rs2valfh));
	  break;
	case REG_INTPAIR:
	  fprintf(corefile, "rs2pair unsupported");
	  break;
	case REG_INT64:
	  fprintf(corefile, "rs2ll = %lld, ",memop->rs2valll);
	  break;
	default: 
	  fprintf(corefile, "rs2X = XXX, ");
	  break;
	}
      
      fprintf(corefile,"rscc = %d, imm = %d\n", memop->rsccvali,memop->code->imm);
    }
#endif
  
  memory_rep(memop,proc);
  memory_latency(memop,proc);
}

/*************************************************************************/
/* AddFullToMemorySystem: a space just opened up in the memory queue, so */
/* some other instruction should be filled in (if any is waiting)        */
/*************************************************************************/
inline void AddFullToMemorySystem(state *proc)
{
  instance *i;

  i = proc->UnitQ[uMEM].GetNext(proc);
  if (i != NULL)
    {
      if (STALL_ON_FULL) // we're definitely stalled for this tag
	{
	  // however, we might not just be stalled for memqfull -- we might
	  // also have branch prediction problems
	  if (i->branchdep == 2)
	    proc->type_of_stall_rest = eSHADOW;
	  else if (proc->unpredbranch)
	    proc->type_of_stall_rest = eBADBR;
	  else
	    unstall_the_rest(proc);
	}
      i->stallqs--;
      i->strucdep = 0;
      AddToMemorySystem(i,proc);
    }
}

/*************************************************************************/
/* PerformMemOp : Possibly remove memory operation from queue and free   */
/*              : up slot in memory system. If aggressive use of load    */
/*              : values supported past ambiguous stores, update         */
/*              : register values, but keep load in queue                */
/*************************************************************************/

void PerformMemOp(instance *inst, state *proc) 
{
#ifdef STORE_ORDERING
  instance *i;
#endif
  
#ifndef STORE_ORDERING  
  if (IsRMW(inst))
    {
      proc->rmw_tags.Remove(inst->tag);
      proc->StoreQueue.Remove(inst);
      inst->in_memunit=0;

      AddFullToMemorySystem(proc);
    }
  else if (IsStore(inst))
    {
      proc->st_tags.Remove(inst->tag);
      proc->StoreQueue.Remove(inst);
      /* Don't add anything to memory system here because this wasn't
	 still sitting in the "real" memory queue in RC */
    }
  else
    {
      if (inst->code->instruction == iPREFETCH ||
	  (!inst->limbo && (!Speculative_Loads || 
	   (!(proc->minstore < proc->SLtag && inst->tag > proc->SLtag)&&
	    !(proc->minload < proc->LLtag && inst->tag > proc->LLtag)))))
	// we enter here ordinarily, for loads that are only
	// preceded by disambiguated stores and which are not speculative
	{
	  proc->LoadQueue.Remove(inst);
	  inst->in_memunit=0;
	  AddFullToMemorySystem(proc);
	}
#else // STORE_ORDERING
  if (IsStore(inst))
    {
      // RMW is nothing special here....
      /* In sequential consistency, memory operations are not removed
	 from the memory queue until the operations above them have all
	 finished. */
      int keepgoing = proc->MemQueue.GetMin(i);
      while (keepgoing) // find other ones to remove also
	{
	  if (i->memprogress > 0)
	    {
	      proc->MemQueue.Remove(i);

	      if (simulate_ilp && !Processor_Consistency && IsStore(i))
		proc->ReadyUnissuedStores--;

	      i->in_memunit=0;
	      AddFullToMemorySystem(proc);
	    }
	  else
	    break;

	  keepgoing = proc->MemQueue.GetMin(i);
	}
    }
  else
    {
      if (!inst->limbo  || inst->code->instruction == iPREFETCH)
	// we enter here ordinarily, for accesses past disambiguated stores
	{
	  MemQLink<instance *> *stepper = NULL, *oldstepper;
	  stepper=proc->MemQueue.GetNext(stepper);
	  i = stepper->d;
	  while (stepper) // find other ones to remove also
	    {
	      if (i->memprogress > 0)
		{
		  if (simulate_ilp && !Processor_Consistency && IsStore(i))
		    proc->ReadyUnissuedStores--;
		  
		  oldstepper=stepper;
		  stepper = proc->MemQueue.GetPrev(stepper);
		  proc->MemQueue.Remove(oldstepper);

		  i->in_memunit=0;
		  AddFullToMemorySystem(proc);
		}
	      else if (!(Processor_Consistency && IsStore(i) && !IsRMW(i)))
		break;

	      if ((stepper=proc->MemQueue.GetNext(stepper)) != NULL)
		{
		  i = stepper->d;
		}
	    }
	}
      
#endif // STORE_ORDERING
      else
	/* when we do have a limbo, we need to just pass the value on,
	   by setting the dest reg val */
	{
	  if (inst->limbo && spec_stores != SPEC_EXCEPT)    { /* We come here only for SPEC_EXCEPT */
	    fprintf(simerr,"%s:%d -- should only be entered with SPEC_EXCEPT. P%d,%d\n",
		    __FILE__,__LINE__,proc->proc_id,inst->tag);
	    exit(-1);
	  }
#ifdef COREFILE
	  if(proc->curr_cycle > DEBUG_TIME)
	    {
	      fprintf(corefile, "INST %p - \n", inst);
	      if (inst->limbo)
		{
		  fprintf(corefile,"Sending value down from limbo-load tag %d\n", inst->tag);
		}
	      else // we come here on RC spec loads
		{
		  fprintf(corefile,"Sending value down from spec-load tag %d\n", inst->tag);
		}
	    }
#endif
	  if (inst->code->rd_regtype == REG_INT || inst->code->rd_regtype == REG_INT64)
	    {
	      if (inst->prd != 0)
		proc->physical_int_reg_file[inst->prd] = inst->rdvali;
	      proc->intregbusy[inst->prd] = 0;
	      /* update the distributed stall queues appropriately...*/
	      proc->dist_stallq_int[inst->prd].ClearAll(proc);
	    }
	  else if (inst->code->rd_regtype == REG_FP)
	    {
	      proc->physical_fp_reg_file[inst->prd] = inst->rdvalf;
	      proc->fpregbusy[inst->prd] = 0;
	      /* update the distributed stall queues appropriately...*/
	      proc->dist_stallq_fp[inst->prd].ClearAll(proc);
	    }
	  else if (inst->code->rd_regtype == REG_FPHALF)
	    {
	      proc->physical_fp_reg_file[inst->prd] = inst->rsdvalf;
	      float *address = (float *) (&proc->physical_fp_reg_file[inst->prd]);
	      if (inst->code->rd & 1) /* the odd half */
		{
		  address += 1;
		}
	      *address = inst->rdvalfh;
	      proc->fpregbusy[inst->prd] = 0;
	      /* update the distributed stall queues appropriately...*/
	      proc->dist_stallq_fp[inst->prd].ClearAll(proc);
	    }
	  else if (inst->code->rd_regtype == REG_INTPAIR)
	    {
	      if (inst->prd != 0)
		proc->physical_int_reg_file[inst->prd] = inst->rdvalipair.a;
	      proc->physical_int_reg_file[inst->prdp] = inst->rdvalipair.b;
	      proc->intregbusy[inst->prd] = 0;
	      proc->intregbusy[inst->prdp] = 0;
	      /* update the distributed stall queues appropriately...*/
	      proc->dist_stallq_int[inst->prd].ClearAll(proc);
	      proc->dist_stallq_int[inst->prdp].ClearAll(proc);
	    }
	  
	  
	  /* Do the same for rcc too. */
	  if (inst->prcc != 0)
	    proc->physical_int_reg_file[inst->prcc] = inst->rccvali;
	  proc->intregbusy[inst->prcc] = 0;
	  proc->dist_stallq_int[inst->prcc].ClearAll(proc);
	}
    }
  
}

/**************************************************************************/
/* CompleteMemOp : Perform the operations on completion of a memory instr */
/*               : and add to DoneHeap                                    */
  /************************************************************************/

int CompleteMemOp(instance *inst, state *proc)
{

  if (IsStore(inst))
    {
      /* STORE: Completes perfectly. Mark it done in some way, and if it's the
	 first instruction in the store q, mark all in-order dones possible
	 -- Note that in the SC world, all Stores are like RMWs
	 */
      if (inst->memprogress == 2) // it has been flushed
	{
	  fprintf(simerr,"Flushed operation should never come to CompleteMemOp.\n");
	  exit(-1);
	}
      else
	{
	  inst->memprogress = 1;
	  if ((inst->addr < lowsimmed|| !proc->MEMSYS) && inst->exception_code == OK)
	    (*(instr_func[inst->code->instruction]))(inst,proc);
	  else if (!inst->global_perform && inst->exception_code == OK)
	    {
	      fprintf(simerr,"Tag %d Processor %d was never globally performed!\n",
                      inst->tag,proc->proc_id);
	      exit(1);
	    }
	  // we need to move this to the issue stage, to handle proper simulation of RAW, etc
	  if (IsRMW(inst)
#ifdef STORE_ORDERING
	      || (!SC_NBWrites && !Processor_Consistency)
#endif
	      )
	    {
	      if (inst->memprogress <= 0)
		{
		  inst->issuetime=INT_MAX;
		  return 0; // not yet accepted
		}
	      
	      PerformMemOp(inst,proc); /* use this so that it's clearly done in the memory unit */
#ifdef COREFILE
	      OpCompletionMessage(inst,proc);
#endif
	      proc->DoneHeap.insert(proc->curr_cycle,inst,inst->tag);
	    }
	  else /* an ordinary write in RC or PC or SC w/nb writes */
	    {
#ifndef STORE_ORDERING
	      proc->StoresToMem--;
#endif
	      PerformMemOp(inst,proc); /* use this so that it's clearly done in the memory unit */
#ifdef COREFILE
	      OpCompletionMessage(inst,proc);
#endif
	      delete inst;
	    }
	}
      
      return 0; 
    }
  else
    {
      /* LOAD: On completion, first see if there are any items in
	 store q with lower tag that has same addr or unknown addr. If
	 so, we may need to redo operation. If not, mark it done and
	 if first instr in store q, mark all in-order completes
	 possible */
      if (inst->memprogress == 2)
	{
	  fprintf(simerr,"Flushed operation should not come to CompleteMemOp.\n");
	  exit(-1);
	}
      else if (inst->code->instruction == iPREFETCH)
	{
	  fprintf(simerr,"Why did we enter Complete Mem Op on a prefetch???\n");
	  inst->memprogress = 1; /* no need to check for limbos, spec_stores, etc. */
	  PerformMemOp(inst,proc);
	}
      else
	{
	  if (inst->exception_code == SOFT_SL_COHE ||
	      inst->exception_code == SOFT_SL_REPL ||
	      inst->exception_code == SOFT_LIMBO)
	    // footnote 5 implementation -- we'll need to redo it.
	    {
	      if (inst->exception_code == SOFT_LIMBO)
		proc->redos++;
	      else
		proc->footnote5++;
	      inst->prefetched=0;
	      inst->memprogress = 0;
	      inst->vsbfwd=0;
	      inst->global_perform=0;
	      inst->exception_code = OK;
	      inst->issuetime=INT_MAX;
	      return 0;
	    }
	  
	  int redo = 0;
	  
	  instance *sts;
	  unsigned confaddr;
	  int lowambig;
	  if (inst->kill == 1)
	    {
	      redo=1;
	      inst->kill = 0;
	    }
	  if (spec_stores == SPEC_LIMBO &&
              proc->ambig_st_tags.GetMin(lowambig) &&
              lowambig < inst->tag)       /* ambiguous store before this load... */
	    {
	      proc->limbos++;
	      proc->curr_limbos++;
#ifdef COREFILE
	      if (proc->curr_limbos < 0)
		{
		  fprintf(simerr," Count of limbo loads drops below zero\n");
		  exit(-1);
		}
#endif
	      inst->limbo=1;         /* Set the limbo bit on to remember to check for this */
	    }

	  if (spec_stores != SPEC_STALL) // if so, this is all taken care of before Issue
	    {
	      MemQLink<instance *> *stindex = NULL;
#ifndef STORE_ORDERING
	      while ((stindex = proc->StoreQueue.GetNext(stindex)) != NULL)
#else
		while ((stindex = proc->MemQueue.GetNext(stindex)) != NULL)
#endif
		  {
		    sts=stindex->d;
		    if (sts->tag > inst->tag)
		      break;
		    
#ifdef STORE_ORDERING
		    if (!IsStore(sts))
		      continue;
#endif
		    
		    // if we got a forward, then we can ignore the other cases here
		    if (inst->memprogress <= -sts->tag-3)
                    // we got a forward from an op with greater tag
		      continue;
		    
		    if (!sts->addr_ready && spec_stores == SPEC_EXCEPT)
		      {
			if (inst->limbo == 0)
			  {
			    inst->limbo = 1;
			    proc->limbos++;
			    proc->curr_limbos++;
#ifdef COREFILE
			    if (proc->curr_limbos < 0)
			      {
				fprintf(simerr," Count of limbo loads drops below zero\n");
				exit(-1);
			      }
#endif
			  }
#ifdef COREFILE
			if (proc->curr_cycle > DEBUG_TIME)
			  fprintf(corefile,"P%d,%d Gambling to avoid a limbo on %d\n",proc->proc_id,
				  inst->tag,sts->tag);
#endif
			continue;
		      }
		    confaddr = sts->addr;
		    if (overlap(sts,inst) && (inst->issuetime < sts->issuetime))
		      {
			proc->redos++;
			redo = 1;
			break;
		      }
		  }
	    }
	  
	  if (redo)
	    {
	      if (inst->limbo)
		{
		  proc->curr_limbos--;
#ifdef COREFILE
		  if (proc->curr_limbos < 0)
		    {
		      fprintf(simerr," Count of limbo loads drops below zero\n");
		      exit(-1);
		    }
#endif
		  
		}
	      inst->memprogress = 0; // in other words, start all over
	      inst->vsbfwd = 0;
	      inst->issuetime=INT_MAX;
	      inst->limbo=0;
	      return 0;
	    }
	  else
	    {
	      if (inst->memprogress == -1 ) // non-forwarded
		{
		  if ((inst->addr < lowsimmed|| !proc->MEMSYS) && inst->exception_code == OK)
		    (*(instr_func[inst->code->instruction]))(inst,proc); // perform LOADS at end
		  else if (!inst->global_perform && inst->exception_code == OK)
		    {
		      fprintf(simerr,"Tag %d Processor %d was never globally performed!\n",inst->tag,proc->proc_id);
		      exit(1);
		    }
		}
	      if (inst->memprogress == 0) // should not ever happen
		{
		  fprintf(simerr,"How is memprogress 0 at Complete?\n");
		  exit(-1);
		}
	      
	      if (!inst->limbo || spec_stores == SPEC_EXCEPT) // only_specs)
		{
		  if (!inst->limbo)
		    inst->memprogress = 1;
		  PerformMemOp(inst,proc);
		  if (!inst->limbo)
		    {
#ifdef COREFILE
		      OpCompletionMessage(inst,proc);
#endif
		      proc->DoneHeap.insert(proc->curr_cycle,inst,inst->tag);
		    }
		}
	    }
	}
      
      return 0; 
    }
}

/*************************************************************************/
/* CanExclPrefetch: return true if a write memory operation needs to be  */
/*                : and can be prefetched                                */
/*************************************************************************/

inline int CanExclPrefetch(state *proc,instance *memop)
{
  return ((Prefetch ) &&
	  proc->prefs != proc->max_prefs &&
	  memop->addr_ready &&
	  memop->addr >= lowsimmed && 
	  memop->prefetched==0 &&
	  proc->MEMSYS);
}

/*************************************************************************/
/* CanShPrefetch : return true if a memory operation needs to be and can */
/*               : be prefetched                                         */
/*************************************************************************/

inline int CanShPrefetch(state *proc,instance *memop)
{
  return ((Prefetch) &&
	  proc->prefs != proc->max_prefs &&
	  memop->addr_ready &&
	  memop->addr >= lowsimmed && 
	  memop->prefetched==0 &&
	  proc->MEMSYS);
}

// note: currently CanExclPrefetch and CanShPrefetch are identical, but
// we keep them separate in case they ever become different.

#ifndef STORE_ORDERING

/*************************************************************************/
/* IssueLoads : Get elements from load queue and check for forwarding    */
/*            : resource availability and issue it                       */
/*************************************************************************/

void IssueLoads(state *proc)
{
  instance *memop;
  MemQLink<instance *> *stindex = NULL, *ldindex = NULL;

  while (((ldindex=proc->LoadQueue.GetNext(ldindex)) != NULL) && proc->UnitsFree[uMEM])
    {
      memop=ldindex->d;

      if (memop->memprogress) // instruction is already issued
	continue;
      if (proc->MEMISSUEtag >= 0 && memop->tag > proc->MEMISSUEtag) // can't issue even if we want to
	break; // all later ones are also past MEMISSUE

      if (memop->addr_ready && memop->truedep != 0)
	{
	  /* a load of an fp-half can have a true dependence on the rest of
	     the fp register. In the future, it's possible that such a load
	     should be allowed to issue but not to complete.  */

	  /* The processor should try to prefetch it, if it has that
	     support. Otherwise, this one can't issue yet. In the case
	     of stat_sched, neither can anything after it. */
	  if (CanShPrefetch(proc,memop))
	    {
	      proc->prefrdy[proc->prefs++] = memop;
	      continue; /* can issue or prefetch later loads */
	    }
	  
	  if (stat_sched)
	    break;
	}
      /*********************************************/
      /* Memory operations that are ready to issue */
      /*********************************************/
      if (memop->truedep == 0 && memop->addr_ready)
	{
	  int canissue=1;
	  int speccing=0;
	  unsigned instaddr = memop->addr;
	  instance *conf;
	  unsigned confaddr;
	  stindex=NULL;
	  if (memop->code->instruction == iPREFETCH)
	    {
	      if (L1Q_FULL[proc->proc_id]) /* ports are taken */
		{
		  break; /* no further accesses will be allowed till
			    ports free up */
		}
	      else
		{
		  ldindex = proc->LoadQueue.GetPrev(ldindex); /* do this because we're going to be removing this entry in the loadqueue in the IssueOp function */
		  IssueOp(memop,proc);
		  continue;
		}
	    }
	  while ((stindex=proc->StoreQueue.GetNext(stindex)) != NULL)
	    {
	      conf=stindex->d;
	      if (conf->tag > memop->tag)
		break;
	      if (!conf->addr_ready) 
		{
		  speccing=1;

		  if (spec_stores == SPEC_STALL) // now's the time to give up, if so
		    {
		      memop->memprogress = 0;
		      memop->vsbfwd = 0;
		      return;
		    }
		  continue;
		}
	      confaddr = conf->addr;
	      if (overlap(conf,memop))
		{
		  // ok, either we stall or we have a chance for a quick forwarding.
		  if (confaddr==instaddr && conf->truedep == 0 && 
		      MemMatch(memop,conf) && 
		      !((!membarprime || confaddr >= lowsimmed) &&
			(!Speculative_Loads &&
			 ((memop->tag > proc->SLtag && conf->tag < proc->SLtag) ||
			  (memop->tag > proc->LLtag && proc->minload < proc->LLtag)))))
		    // no forward across membar unless there is specload support
		    // note: specload membar is "speculative" across a LL or SL,
		    // but we _actually_ enforce the MemIssue ones
		    {
		      if (conf->memprogress != 0 && confaddr >= lowsimmed && proc->MEMSYS) // a forward from VSB of something that actually will be GloballyPerformed
			{
			  memop->vsbfwd = -conf->tag-3;
			  memop->memprogress = 0;
			}
		      else
			{
			  memop->vsbfwd = 0; // if we accidentally counted it as a vsbfwd earlier, count it properly now
			  memop->memprogress = -conf->tag-3; // varies from -3 on down!
			}
		      canissue=1; // we can definitely use it
		      continue;
		    }
		  else if (conf->code->instruction == iSTDF &&
			   memop->code->instruction == iLDUW &&
			   conf->truedep == 0 &&
			   ((memop->addr == conf->addr) ||
			    (memop->addr == conf->addr + sizeof(int))) &&
			   !((!membarprime || confaddr >= lowsimmed) &&
			     (!Speculative_Loads &&
			      ((memop->tag > proc->SLtag && conf->tag < proc->SLtag) ||
			       (memop->tag > proc->LLtag && proc->minload < proc->LLtag)))))
		    {
		      /* here's a case where we can forward just like
			 the regular case, even if it's a partial
			 overlap. I mention this case because it's so
			 common, appearing in some libraries all the
			 time */
			
		      if (conf->memprogress != 0 && confaddr >= lowsimmed && proc->MEMSYS) // a forward from VSB of something that actually will be GloballyPerformed -- only way it won't is if it gets flushed
			{
			  memop->vsbfwd = -conf->tag-3;
			  memop->memprogress = 0;
			}
		      else
			{
			  memop->vsbfwd = 0; // if we accidentally counted it as a vsbfwd earlier, count it properly now
			  memop->memprogress = -conf->tag-3; // varies from -3 on down!
			}
		      
		      if (memop->addr == conf->addr)
			{
			  memop->rdvali = *((int *)&conf->rs1valf);
			}
		      else if (memop->addr == conf->addr+sizeof(int))
			{
			  memop->rdvali = *(((int *)(&conf->rs1valf)) +1);
			}
		      else
			{
			  /* a partial overlap of STDF and LDUW that can't be
			     handled. Most probably misalignment or
			     other problems involved. Checked for this
			     case before entering here. */
			  fprintf(simerr,"Partial overlap of STDF and LDUW which couldn't be recognized -- should have been checked.\n");
			  exit(-1);
			}
		      canissue=1; // we can definitely use it
		      continue;
		    }			   
		  else if (conf->memprogress == 0) // the store hasn't been issued yet
		    {
		      memop->memprogress = 0;
		      memop->vsbfwd = 0;
		      canissue=0;
		      continue;
		    }
		  else if (IsRMW(conf)) // you can't go in parallel with any RMW (except of course for simple LDSTUBs which you forward)
		    {
		      memop->memprogress = 0;
		      memop->vsbfwd = 0;
		      canissue=0;
		      continue;
		    }
		  else if (!((!membarprime || memop->addr >= lowsimmed) &&
			     ((memop->tag > proc->SLtag && proc->minstore < proc->SLtag) ||
			      (memop->tag > proc->LLtag && proc->minload < proc->LLtag))))
		    /* in other words, this isn't a case of trying to forward across a membar */
		    {
		      // for now we'll actually block out partial overlaps (except
		      // for the special case we handled above STDF-LDUW),
		      // and count them when they graduate
		      
			{
			  memop->memprogress = 0;
			  memop->vsbfwd = 0;
			  canissue=0;
			  memop->partial_overlap=1;
#ifdef COREFILE
			  if (YS__Simtime > DEBUG_TIME)
			    fprintf(corefile,"P%d @<%d>: Partially overlapping load tag %d[%s] @(%d) conflicts with tag %d[%s] @(%d)\n",proc->proc_id,proc->curr_cycle,memop->tag,inames[memop->code->instruction],instaddr,conf->tag,inames[conf->code->instruction],confaddr);
#endif
			  continue;
			}
		    }
		}
	    }

	  if (canissue && memop->memprogress > -3)
	    {
	      if ((!membarprime || confaddr >= lowsimmed) &&
		    (!Speculative_Loads &&
		     ((memop->tag > proc->SLtag && proc->minstore < proc->SLtag) ||
		      (memop->tag > proc->LLtag && proc->minload < proc->LLtag))))
		{
		  // so we're stuck at an ACQ membar
		  // well, if we have prefetch, let's go ahead and prefetch reads
		  memop->vsbfwd = 0;
		  memop->memprogress = 0;
		  canissue = 0;
		  
		  if (CanShPrefetch(proc,memop)) 
		    {
		      proc->prefrdy[proc->prefs++] = memop;
		      continue;
		    }
		  continue; /* in case we have some prefetches or anything like that */
		  break;
		}
	      // Check if L1 ports are busy before issuing

	      if (L1Q_FULL[proc->proc_id])
		{
		  canissue=0;
		  memop->vsbfwd = 0;
		  memop->memprogress=0;
		  break;
		}
	      
	    }	  

	  if (canissue)
	    {
	      proc->ldissues++;
	      if (speccing)
		proc->ldspecs++;
	      if (memop->memprogress < 0) // we got a quick forward
		{
		  proc->fwds++;
		  memop->issuetime=proc->curr_cycle;
		  // WE GET THE FORWARD VALUE IN THE MemMatch FUNCTION
		  
		  proc->MemDoneHeap.insert(proc->curr_cycle+1, memop,memop->tag);
		}
	      else
		IssueOp(memop,proc);
	    }
	}
    }
}

/**************************************************************************/
/* IssueStores : Get elements from store queue and check for dependences, */
/*             : resource availability and issue it                       */
/**************************************************************************/

void IssueStores(state *proc) /* note, we need to do IssueMem _every cycle_ */
{
  /* first do loads, then stores */
  instance *memop;
  int confstore = 0;
  MemQLink<instance *> *stindex = NULL,*stindex2=NULL;

  /* NOTE: DON'T ISSUE ANY STORES SPECULATIVELY */

  while (((stindex=proc->StoreQueue.GetNext(stindex)) != NULL) && proc->UnitsFree[uMEM]) 
    {
      memop=stindex->d;
      if (memop->memprogress) // the instruction is already issued
	continue;
      if (proc->MEMISSUEtag >= 0 && memop->tag > proc->MEMISSUEtag) // can't issue even if we want to
	break; // all later ones will also be past MEMISSUEtag
      
      if (!memop->st_ready || confstore)
	{
	  if (CanExclPrefetch(proc,memop))
	    {
	      proc->prefrdy[proc->prefs++] = memop;
	    }
	  continue; /* we might be able to pf subsequent ones,
	  regardless of whether we could or couldn't for this
	  one... */
	}
      
      if (!memop->addr_ready) 
	{
#ifdef COREFILE
	  if (proc->curr_cycle > DEBUG_TIME)
	    fprintf(corefile,"store tag %d store ready but not address ready!\n",memop->tag);
#endif
	  confstore = 1;
	  continue; /* we might be able to prefetch some later ones, but
		       don't issue any demand stores */
	}
      
      int canissue=1;

      instance *conf;
      stindex2=NULL;
      while ((stindex2 = proc->StoreQueue.GetNext(stindex2)) != NULL)
	{
	  conf=stindex2->d;
	  if (conf->tag >= memop->tag)
	    break;
	  
	  // block this one if there is a previous unissued one with
	  // overlapping address.... having a previous unissued one
	  // can occur only with membarprime (where MEMBARs do not
	  // block private accesses)
	  
	  if (conf->memprogress == 0 && overlap(conf,memop))
	    {
	      canissue=0;
	      break;
	    }
	}
      
      if (canissue)
	{
	  /* the following checks to implement MEMBAR consistency */
	     
	  if ((!membarprime || memop->addr >= lowsimmed) &&
	      ( (memop->tag > proc->SStag && proc->minstore < proc->SStag) ||
		(memop->tag > proc->LStag && proc->minload < proc->LStag) ||
		(IsRMW(memop) &&
		 (memop->tag > proc->SLtag && proc->minstore < proc->SLtag) ||
		 (memop->tag > proc->LLtag && proc->minload < proc->LLtag))))
	    {
	      canissue=0;
	      if (CanExclPrefetch(proc,memop))
		{
		  proc->prefrdy[proc->prefs++] = memop;
		  continue;
		}
	      continue; // use continue rather than break in case there are any SPINs in the system
	      break;
	    }
	}	  

      /* Now we've passed all the semantic reasons why we can't issue it.
	 So, now we need to check for hardware constraints */

      /* The following checks should not be conditional on
	 inst->addr >= lowsimmed, because we want to model
	 contention for private accesses, even when we don't
	 simulate them. Thus, even these accesses need to
	 check for ports, etc. */

      // check for structural hazards on input ports of cache
      if (L1Q_FULL[proc->proc_id])
	{
	  canissue=0;
	  memop->memprogress=0;
	  break;
	}
	      
      if (canissue)
	{
	  if (!IsRMW(memop))
	    {		  
	      proc->StoresToMem++;
	      AddFullToMemorySystem(proc);
	    }
	  if (simulate_ilp)
	    proc->ReadyUnissuedStores--;
	  
	  IssueOp(memop,proc);
	}
    }
}

#else // STORE_ORDERING
/* NOTE: This is used for both SC and PC. PC is just like SC except
   that the W->R restriction is removed.  Technically, our PC is
   TSO. */

/**************************************************************************/
/* IssueMems : Get elements from memory queue and check for dependences,  */
/*           : resource availability and issue it                         */
/**************************************************************************/

void IssueMems(state *proc)
{
  instance *memop;
  MemQLink <instance *> *index = NULL, *confindex = NULL;
  int ctr= -1, readctr=-1;

  while ((index = proc->MemQueue.GetNext(index)) != NULL && proc->UnitsFree[uMEM])
    {
      ctr++;

      memop = index->d;

      if ((!IsStore(memop) || IsRMW(memop)) && (memop->code->instruction != iPREFETCH))
	readctr++;
      
      if (memop->memprogress) // already issued or dead
	continue;
      if (IsStore(memop)) // treat stores a certain way -- need to be at head of active list
	{
	  if (!memop->st_ready || ctr != 0) /* make sure that not only is st_ready set, but also
					       store should be at top of memory unit. */
	    {
	      if (CanExclPrefetch(proc,memop))
		{
		  proc->prefrdy[proc->prefs++] = memop; // we'll remember to go backward when reading the prefetchqueue
		}
	      continue;
	    }
	  // and since we don't overlap stores anyway in SC or PC, we don't have to check the stq!

	  if (!memop->addr_ready)
	    {
#ifdef COREFILE
	      if (proc->curr_cycle > DEBUG_TIME)
		fprintf(corefile,"store tag %d store ready but not address ready!\n",memop->tag);
#endif
	      continue;
	    }
	  
	  if (!L1Q_FULL[proc->proc_id])
	    {
	      if (simulate_ilp && Processor_Consistency)
		proc->ReadyUnissuedStores--;
	      IssueOp(memop,proc);
	    }
	  else
	    break;
	}
      else // this is where we handle loads 
	{
	  if (memop->code->instruction == iPREFETCH && memop->truedep == 0 && memop->addr_ready /* && memop->branchdep != 2 */)
	    {
	      /* we can issue a s/w prefetch whenever we like, in any of SC versions */
	      if (L1Q_FULL[proc->proc_id])
		{
		  break; /* we're not going to get any further anyway */
		}
	      else
		{
		  index = proc->MemQueue.GetPrev(index);
		  /* do this because we're going to be removing this entry in the loadqueue in the IssueOp function */
		  ctr--;
		  IssueOp(memop,proc);
		  continue;
		}
	    }

	  if (!(Speculative_Loads || (Processor_Consistency&&readctr==0)))
	    {
	      // in this case, we basically have to wait on things to complete
	      // no need to check either load or store queue, since we're at the top
	      if (memop->truedep == 0 && memop->addr_ready && ctr == 0  /* && memop->branchdep != 2 */) 
		{
		  if (!L1Q_FULL[proc->proc_id])
		    {
		      IssueOp(memop,proc);
		    }
		  else
		    break;
		}
	      else
		{
		  // prefetch anything we can!
		  int m = mem_acctype[memop->code->instruction]; // we can't do this on any FAB type
		  if (CanShPrefetch(proc,memop))
		    {
		      proc->prefrdy[proc->prefs++] = memop;
		      continue;
		    }
		}
	    }
	  else
	    /* in this case, we do have speculative load execution or
	       this is processor consistency but there are only writes
	       ahead of us. */
	    {
	      if (memop->addr_ready && memop->truedep != 0)
		{
		  /* a load of an fp-half can have a true dependence
		     on the rest of the fp register. Maybe such a load
		     should be allowed to issue but not to complete?  */
		  if (CanShPrefetch(proc,memop))
		    {
		      proc->prefrdy[proc->prefs++] = memop;
		      continue;
		    }
		}
	      if (memop->truedep == 0 && memop->addr_ready /* && memop->branchdep != 2 */)
		{
		  int canissue = 1;
		  int speccing=0;
		  unsigned instaddr = memop->addr;
		  instance *conf;
		  unsigned confaddr;
		  confindex=NULL;
		  while (confindex=proc->MemQueue.GetNext(confindex))
		    {
		      conf=confindex->d;
		      if (conf->tag >= memop->tag)
			break;
		      if (!IsStore(conf))
			continue;
		      if (conf->addrdep || !conf->addr_ready)
			{
			  speccing=1;
			  continue;
			}
		      confaddr = conf->addr;
		      if (overlap(conf,memop))
			{
			  // ok, either we stall or we have a chance for a quick forwarding.
			  if (confaddr==instaddr && conf->truedep == 0 && MemMatch(memop,conf))
			    {
			      memop->memprogress = -conf->tag-3; // varies from -3 on down!
			      canissue=1; // we can definitely use it
			      continue;
			    }
			  else if (conf->code->instruction == iSTDF &&
				   memop->code->instruction == iLDUW &&
				   conf->truedep == 0 &&
				   ((memop->addr == conf->addr) ||
				    (memop->addr == conf->addr + sizeof(int))))
			    {
			      /* here's a case where we can forward
				 just like the regular case, even if
				 it's a partial overlap. I mention
				 this case because it's so common,
				 appearing in libraries all the time */
			      memop->memprogress = -conf->tag-3; // varies from -3 on down!
			      
			      if (memop->addr == conf->addr)
				{
				  memop->rdvali = *((int *)&conf->rs1valf);
				}
			      else if (memop->addr == conf->addr+sizeof(int))
				{
				  memop->rdvali = *(((int *)(&conf->rs1valf)) +1);
				}
			      else
				{
				  /* Not an acceptable case -- may have
				     a misalignment, etc. checked in if
				     clause above */
				  fprintf(simerr,"Partial overlap of STDF and LDUW which couldn't be recognized -- should have been checked.\n");
				  exit(-1);
				}
			      canissue=1; // we can definitely use it
			      continue;
			    }			   
			  else if (conf->memprogress == 0) // the store hasn't been issued yet, and it only partially overlaps
			    {
			      memop->memprogress =0;
			      canissue=0;
			      continue;
			    }
			  else if (IsRMW(conf)) // you can't go in parallel with any RMW (except of course for simple LDSTUBs which you forward)
			    {
			      memop->memprogress = 0;
			      canissue=0;
			      continue;
			    }
			  else // we would be expecting a partial forward at the cache
			    {
			      memop->memprogress = 0;
			      canissue=0;
			      memop->partial_overlap=1;
			      continue;
#ifdef COREFILE
			      if (YS__Simtime > DEBUG_TIME)
				fprintf(corefile,"P%d @<%d>: Partially overlapping load tag %d @(%d) conflicts with tag %d @(%d)\n",proc->proc_id,proc->curr_cycle,memop->tag,instaddr,conf->tag,confaddr);
#endif
			    }
			}
		    }
		  if (canissue && memop->memprogress > -3)
		    {
		      if (L1Q_FULL[proc->proc_id]) // can't issue it, since all ports taken already
			{
			  canissue=0;
			  memop->memprogress=0;
			  break;
			}
		    }	  
		  
		  if (canissue)
		    {
		      proc->ldissues++;
		      if (speccing)
			proc->ldspecs++;
		      if (memop->memprogress < 0) // we got a quick forward
			{
			  memop->issuetime=proc->curr_cycle;
			  proc->MemDoneHeap.insert(proc->curr_cycle+1, memop,memop->tag);
			}
		      else
			IssueOp(memop,proc);
		    }
		}
	    }
	}
    }
}
#endif // STORE_ORDERING

/**************************************************************************/
/* FlushMems  : Flush the memory queues in the event of a branch mispredn */
/**************************************************************************/

void FlushMems(int tag,state *proc)
{
  /* flush all speculative memory operations in the two queues that come after
     the tag specified */
  instance *junk;
  int tl_tag;
  
  while (proc->ambig_st_tags.GetTail(tl_tag) && tl_tag > tag)
    proc->ambig_st_tags.RemoveTail();

#ifdef STORE_ORDERING
  while (proc->MemQueue.NumItems())
    {
      proc->MemQueue.GetTail(junk);
      if (junk->tag > tag)
	{
	  proc->MemQueue.RemoveTail(); // DeleteFromTail(junk);

	  if (junk->st_ready)
	    {
	      /* This can happen because of non-blocking writes
		 of an ordinary load that was marked ready past a done load
		 that is stuck in the memunit (in which case we rely on
		 the "ctr==0" requirement to stall issue) */

	      if (simulate_ilp)
		proc->ReadyUnissuedStores--;
	    }
	  if (junk->in_memunit == 0)
	    {
	      if (!IsStore(junk) || IsRMW(junk))
		{
		  fprintf(simerr,"How can tag %d be flushed from the memunit, even after it has already been graduated?\n", junk->tag);
		  /* stores can come out of the memunit by being marked ready,
		     and that's ok even with NB writes */
		  exit(-1);
		}
#ifdef COREFILE
	      if (YS__Simtime > DEBUG_TIME)
		fprintf(corefile,"Deleting memop for tag %d which is not in_memunit.\n",junk->tag);
#endif
	      delete junk;
	    }
	  
	  junk->depctr = -2;
	  junk->memprogress = 2;
	}
      else
	break;
    }
#else
  while (proc->StoreQueue.NumItems())
    {
      proc->StoreQueue.GetTail(junk);
      if (junk->tag > tag)
	{
	  proc->StoreQueue.RemoveTail(); // DeleteFromTail(junk);
	  // if (junk->memprogress == 0) // not yet issued
	    junk->depctr = -2;
	  junk->memprogress = 2;
	}
      else
	break;
    }
  while (proc->LoadQueue.NumItems())
    {
      proc->LoadQueue.GetTail(junk);
      if (junk->tag > tag)
	{
	  proc->LoadQueue.RemoveTail(); // DeleteFromTail(junk);
	  if (junk->limbo)
	    {
	      proc->curr_limbos--;
#ifdef COREFILE
	      if (proc->curr_limbos < 0)
		{
		  fprintf(simerr," Current limbo counter drops below zero!!!\n");
		  exit(-1);
		}
#endif
	    }
	  junk->depctr = -2;
	  junk->memprogress = 2; // mark this for done ones
	}
      else
	break;
    }
  
  while (proc->st_tags.GetTail(tl_tag) && tl_tag > tag)
    proc->st_tags.RemoveTail();
  while (proc->rmw_tags.GetTail(tl_tag) && tl_tag > tag)
    proc->rmw_tags.RemoveTail();
  
  int mbchg=0;
  MembarInfo mb;
  while (proc->membar_tags.GetTail(mb) && mb.tag > tag)
    {
      proc->membar_tags.RemoveTail();
      mbchg=1;
#ifdef COREFILE
      if(proc->curr_cycle > DEBUG_TIME)
	fprintf(corefile,"Flushing out membar %d\n",mb.tag);
#endif

    }
  if (mbchg)
    ComputeMembarQueue(proc);
#endif
}

/***************************************************************************/
/* NumInMemorySystem :  return the number of elements in the memory system */
/***************************************************************************/

int NumInMemorySystem(state *proc)
{
#ifndef STORE_ORDERING
  return proc->StoreQueue.NumItems() + proc->LoadQueue.NumItems() - proc->StoresToMem;
#else
  return proc->MemQueue.NumItems();
#endif
}

/*************************************************************************/
/* AddToMemorySystem :  Handle memory operations and insert them in the  */
/*                   :  appropriate queues                               */
/*************************************************************************/

void AddToMemorySystem(instance *inst,state *proc)
{
#ifdef COREFILE
  if (YS__Simtime > DEBUG_TIME)
    fprintf(corefile,"Adding tag %d to memory system\n",inst->tag);
#endif
  
  inst->prefetched=0;
  inst->in_memunit=1;
  inst->miss=mtL1HIT;

#ifdef STORE_ORDERING
  inst->st_ready=0;
  inst->kill=0;
  inst->limbo=0;
  proc->MemQueue.Insert(inst);
#else
  if (IsStore(inst))
    {
      inst->st_ready=0;
      if (IsRMW(inst))
	{
	  proc->rmw_tags.Insert(inst->tag);
	}
      else
	{
	  proc->st_tags.Insert(inst->tag);
	  inst->newst=1;
	}
      proc->StoreQueue.Insert(inst);
    }
  else
    {
      inst->kill=0;
      inst->limbo=0;
      proc->LoadQueue.Insert(inst);
    }
#endif
  inst->addr=0;
  if (IsStore(inst))
    {
      proc->ambig_st_tags.Insert(inst->tag);
    }
  if (inst->addrdep == 0)
    {
      CalculateAddress(inst,proc);
    }
  

}

/*************************************************************************/
/* Disambiguate : Called when the address is newly generated for an      */
/*              : instruction. Checks if newly disambiguated stores      */
/*              : conflict with previously issued loads                  */
/*************************************************************************/

void Disambiguate(instance *inst, state *proc )
{
  inst->addr_ready=1;
  inst->time_addr_ready = YS__Simtime;
  if (IsStore(inst))
    {
      if (!simulate_ilp)
	{
	  inst->newst = 0;
	  inst->st_ready = 1;
	}
      
      int goodpred=1;
      instance *conf;
      MemQLink<instance *> *ldindex = NULL;      
      int lowambig=INT_MAX;

      proc->ambig_st_tags.Remove(inst->tag);
      proc->ambig_st_tags.GetMin(lowambig);
#ifndef STORE_ORDERING
      while ((ldindex=proc->LoadQueue.GetNext(ldindex)) != NULL)
#else
      while ((ldindex=proc->MemQueue.GetNext(ldindex)) != NULL)
#endif
	{
	  conf=ldindex->d;

#ifdef STORE_ORDERING
	  if (IsStore(conf))
	    continue;
#endif
	  
	  if (conf->tag >= inst->tag)
	    {
	      if (conf->limbo)
		{
		  if (overlap(conf,inst))
		    {
		      proc->curr_limbos--;
#ifdef COREFILE
		      if (proc->curr_limbos < 0)
			{
			  fprintf(simerr," Current limbo counter drops below zero!!!\n");
			  exit(-1);
			}
#endif
		      conf->limbo=0;

		      if (spec_stores == SPEC_EXCEPT)
			{
			  if (conf->exception_code == OK || conf->exception_code == SERIALIZE)
			    {
			      conf->exception_code=SOFT_LIMBO; // mark this a soft exception
			      proc->active_list->flag_exception_in_active_list(conf->tag,SOFT_LIMBO);
			    }

			  // the following needs to be done regardless
			  // of the exception code (ie, you may have
			  // already had an exception caused by
			  // SOFT_SL)
			  
			  // mark this as being "done" so that way it
			  // actually graduates

			  MemQLink<instance *> *confindex = ldindex;
			  
#ifndef STORE_ORDERING
			  ldindex=proc->LoadQueue.GetPrev(ldindex);
			  proc->LoadQueue.Remove(confindex);
#else
			  ldindex=proc->MemQueue.GetPrev(ldindex);
			  proc->MemQueue.Remove(confindex);
#endif

			  conf->in_memunit=0;

			  /* We can bring something into the memqueue
			     right now, but we don't have to, since
			     that's definitely going to get flushed */
			  AddFullToMemorySystem(proc);
			  
			  proc->DoneHeap.insert(proc->curr_cycle,conf,conf->tag);
			}
		      else if (spec_stores == SPEC_LIMBO)
			{
			  conf->memprogress = 0;
			  conf->vsbfwd = 0;
			  proc->redos++; // going to have to redo it
			  goodpred=0;
			  conf->issuetime=INT_MAX;
			}
		      else
			{
			  fprintf(simerr,"Disambiguate limbo redos should not be seen with SPEC_STALL?\n");
			  exit(-1);
			}
		    }
		  else if (lowambig > conf->tag)
		    {
		      proc->curr_limbos--;
#ifdef COREFILE
		      if (proc->curr_limbos < 0)
			{
			  fprintf(simerr,"Current limbo counter drops below zero\n");
			  exit(-1);
			}
#endif
		      proc->unlimbos++;
		      conf->limbo=0;
#ifndef STORE_ORDERING
		      ldindex=proc->LoadQueue.GetPrev(ldindex);
#else
		      ldindex=proc->MemQueue.GetPrev(ldindex);
#endif
		      
		      conf->memprogress = 1;
		      PerformMemOp(conf,proc);
		      proc->DoneHeap.insert(proc->curr_cycle,conf,conf->tag);
		    }
		}
	      else if (conf->memprogress < 0 && overlap(conf,inst))
		{
		  goodpred=0;
		  proc->kills++;
		  if (spec_stores == SPEC_EXCEPT)
		    {
		      if (conf->exception_code == OK || conf->exception_code == SERIALIZE)
			{
			  conf->exception_code = SOFT_LIMBO;
			  proc->active_list->flag_exception_in_active_list(conf->tag,SOFT_LIMBO);
			}
		    }
		  else if (spec_stores == SPEC_LIMBO)
		    {
		      conf->kill = 1;
		    }
		  else
		    {
		      fprintf(simerr,"Should never get a disambiguate kill on a SPEC_STALL?\n");
		      exit(-1);
		    }
		}
	    }
	}
    }
}

/*************************************************************************/
/* CompleteMemQueue : Takes entries off the MemDoneHeap and if they are  */
/*                  : valid, inserts them on the running queue           */
/*************************************************************************/


void CompleteMemQueue(state *proc)
{
  int cycle = proc->curr_cycle;
  instance *inst;
  int inst_tag;
  int freed;
  
  while (proc->MemDoneHeap.num() != 0 && proc->MemDoneHeap.PeekMin() <= cycle)
    {
      proc->MemDoneHeap.GetMin(inst,inst_tag);
      if (inst->tag == inst_tag) // hasn't been flushed
	freed = CompleteMemOp(inst,proc); // this will insert completions on running q
#ifdef COREFILE
      else
	{
	  if (proc->curr_cycle > DEBUG_TIME)
	    fprintf(corefile,"Got nonmatching tag %d off memheap\n",inst_tag);
	}
#endif
    }
}

/*************************************************************************/
/* IssueMem    : Called every cycle -- calls IssueStores, IssueLoads,    */
/*               IssueMems and IssuePrefetch. Also responsible for       */
/*               maintaining MEMBAR status                               */
/*************************************************************************/

int IssueMem(state *proc) /* note, we need to do IssueMem _every cycle_ */
{
  if (FASTER_PROC_L1!=1 && unsigned(proc->curr_cycle) % FASTER_PROC_L1 != 0)
    return 0; /* this means that in this cycle we shouldn't try anything */
    
  
#ifndef STORE_ORDERING
  instance *memop;
  proc->minload = INT_MAX, proc->minstore = INT_MAX;
  if (proc->LoadQueue.GetMin(memop))
    {
      proc->minload = (proc->minload < memop->tag) ? proc->minload : memop->tag;
    }
  
  proc->st_tags.GetMin(proc->minstore); /* this will leave minstore alone if nothing there */
  int minrmw = INT_MAX;
  proc->rmw_tags.GetMin(minrmw);
  
  proc->minload = (proc->minload < minrmw) ? proc->minload : minrmw;
  proc->minstore = (proc->minstore < minrmw) ? proc->minstore : minrmw; 
  
  MembarInfo mb;
  int mbchg=0;
  while (proc->membar_tags.GetMin(mb))
    {
      if ((!(mb.LL || mb.LS) || proc->minload > mb.tag) &&
	  (!(mb.SL || mb.SS) || proc->minstore > mb.tag) &&
	  (!mb.MEMISSUE || (proc->minstore > mb.tag && proc->minload > mb.tag)))
	{  
#ifdef COREFILE
	  if(proc->curr_cycle > DEBUG_TIME)
	    fprintf(corefile,"Breaking down membar %d\n",mb.tag);
#endif


	  mbchg =1;
	  proc->membar_tags.Remove(mb);
	}
      else
	break;
    }

  if (mbchg)
    {
      ComputeMembarQueue(proc);
      if (Speculative_Loads) // in this case, we need to go through the Load Queue finding done items
	{
	  MemQLink<instance *> *ldindex = NULL;
	  while ((ldindex = proc->LoadQueue.GetNext(ldindex)) != NULL &&
		 !(proc->minstore < proc->SLtag && ldindex->d->tag > proc->SLtag)&&
		 !(proc->minload < proc->LLtag && ldindex->d->tag > proc->LLtag))
	    {
	      instance *memop2 = ldindex->d;
	      if (memop2->memprogress == 1 && !memop2->limbo)
		{
		  ldindex=proc->LoadQueue.GetPrev(ldindex); /* since this access is going to be removed, be sure to get it out .... */
		  PerformMemOp(memop2,proc); /* this will remove us from the LoadQueue */
		}
	    }
	}
    }
#endif
  
  proc->prefs=0; // zero prefetches so far this cycle

#ifndef STORE_ORDERING
  IssueStores(proc);
  IssueLoads(proc);
#else
  IssueMems(proc);
#endif


  if (Prefetch) 
    {
      for (int pref=0; pref < proc->prefs && proc->UnitsFree[uMEM] && !L1Q_FULL[proc->proc_id] ; pref++)
	{
	  instance *prefop=proc->prefrdy[pref];
	  prefop->prefetched=1;
	  int macc = mem_acctype[prefop->code->instruction];
	  int level = Prefetch; // the variable Prefetch carries the default level for prefetching, as well as being an indication of whether prefetching is on or not.
	  if (PrefetchWritesToL2 && macc != READ)
	    level=2;
#ifdef COREFILE
	  if(proc->curr_cycle > DEBUG_TIME)
	    fprintf(corefile,"%s_PREFETCH tag = %d (%s) addr = %d\n", mem_acctype[prefop->code->instruction]!=READ ? "EXCL" : "SH", prefop->tag,inames[prefop->code->instruction],prefop->addr);
#endif
	  IssuePrefetch(proc,prefop->addr, level,macc != READ);
	  proc->UnitsFree[uMEM]--;

#ifdef COREFILE
	  if(proc->curr_cycle > DEBUG_TIME)
	    {
	      fprintf(corefile,"Consuming memunit for tag %d\n",prefop->tag);
	    }
#endif
	  
	  memory_rep(prefop,proc); // to get the UnitsFree unit back on the system
	}
    }
  
  return 0;
}

#ifndef STORE_ORDERING

/*************************************************************************/
/* ComputeMembarQueue : reset SS/LS/LL/SL tags based on membar queue     */
/*************************************************************************/


void ComputeMembarQueue(state *proc)
{
  proc->SStag = proc->LStag = proc->SLtag = proc->LLtag = proc->MEMISSUEtag = -1; // guaranteed to be out of range
  MemQLink<MembarInfo> *stepper = NULL;
  while ((stepper = proc->membar_tags.GetNext(stepper)) != NULL)
    {
      if (stepper->d.SS && proc->SStag == -1)
	proc->SStag = stepper->d.tag;
      if (stepper->d.LS && proc->LStag == -1)
	proc->LStag = stepper->d.tag;
      if (stepper->d.SL && proc->SLtag == -1)
	{
	  proc->SLtag = stepper->d.tag;
	}
      if (stepper->d.LL && proc->LLtag == -1)
	{
	  proc->LLtag = stepper->d.tag;
	}
      if (stepper->d.MEMISSUE && proc->MEMISSUEtag == -1)
	{
	  proc->MEMISSUEtag = stepper->d.tag;
	}
    }

}

/***************************************************************************/
/* ComputeMembarInfo : On a memory barrier instruction, suitably updates   */
/*                   : the processor's memory-barrier implementation flags */
/***************************************************************************/


void ComputeMembarInfo(state *proc,const MembarInfo& mb)
{
  if (mb.SS && proc->SStag == -1)
    proc->SStag = mb.tag;
  if (mb.LS && proc->LStag == -1)
    proc->LStag = mb.tag;
  if (mb.SL && proc->SLtag == -1)
    {
      proc->SLtag = mb.tag;
    }
  if (mb.LL && proc->LLtag == -1)
    {
      proc->LLtag = mb.tag;
    }
  if (mb.MEMISSUE && proc->MEMISSUEtag == -1)
    {
      proc->MEMISSUEtag = mb.tag;
    }
}

#endif

/*************************************************************************/
/* DoMemFunc : Wrapper around the actual instruction execution function  */
/*************************************************************************/

void DoMemFunc(instance *inst,state *proc)
{
  if (inst->global_perform)
    {
      fprintf(simerr,"Globally performing something that's already been globally performed!!\n");
      exit(-1);
    }

  /* Execute the function associated with this instruction */
  (*(instr_func[inst->code->instruction]))(inst,proc);
  
  inst->global_perform=1;
}


/* What inst->memprogress means

   2       flushed
   1       completed
   0       unissued
   -1      issued

   -3 to -inf forwarded
   */













