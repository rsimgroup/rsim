/*
  graduate.cc

  This file contains the functions that implement in-order retirement
  (graduation) of completed instructions, and also makes sure that store
  data is not sent to cache until the store is absolutely ready to graduate.

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


#include "Processor/state.h"
#include "Processor/active.h"
#include "Processor/instance.h"
#include "Processor/tagcvt.h"
#include "Processor/memory.h"
#include "Processor/exec.h"
#include "Processor/mainsim.h"
#include "Processor/freelist.h"
#include "Processor/traps.h"
#include "Processor/FastNews.h"
#include "Processor/simio.h"
#include <stdlib.h>

extern "C"
{
#include "MemSys/simsys.h"
#include "MemSys/arch.h"
}

/**************************************************************************/
/* graduate_cycle : remove elements in-order from the active list, handle */
/*                : any exceptions that are flagged                       */
/**************************************************************************/

void graduate_cycle(state *proc)
{
  /* If we are at the head of the active list, and no excepns,
     graduate, release into free list too. */
  /* Takes care of freeing and writing back and exceptions. */
  instance *rettagval = proc->active_list->remove_from_active_list(proc->curr_cycle, proc);
  if(rettagval != NULL)
    {
      /* Exception at the tag returned ! */
      
      /* Exception Handler returns -1 if non returnable exception,
	 otherwise returns 0,
	 
	 it also flushes ALL relevant queues and resets all relevant
	 lists and restarts execution from the instruction which
	 caused the exception */
      
      proc->time_pre_exception = proc->curr_cycle;
      PreExceptionHandler(rettagval, proc);
    }

  if (simulate_ilp)
    proc->active_list->mark_stores_ready(proc->curr_cycle,proc);
  
  if (proc->time_to_dump)
    {
      proc->time_to_dump = 0; // this will get set by alarm
      proc->report_partial();
    }
}

/*************************************************************************/
/* mark_stores_ready : This function makes sure that stores are marked   */
/* ready to issue only when they are ready to graduate in the next       */
/* cycle -- namely, the store must be one of the first 4 (or whatever    */
/* the graduation rate is) instructions in the active list and each      */
/* instructions before it must have completed without excepting. In      */
/* cases with non-blocking stores (RC, PC, SC with the "-N" option),     */
/* stores can leave the active list after being marked ready.            */
/*************************************************************************/


void activelist::mark_stores_ready(int cycle,state *proc)
{
  activelistelement *ptr, *ptr2;
  int numelts, cango;
  int ind,actind;

  numelts=q->NumInQueue();
  cango = 1;
  /* cango is set to false when it finds one of the following
     1) an instruction that isn't done in the active list
        (or, with non-blocking writes, a store that doesn't have it's
	address ready)
     2) an instruction that has its exception code set
     3) a mispredicted branch whose following hasn't yet been flushed
        (in our case, no such thing exists, so we don't do this check)
  */
  
  for (ind=0,actind=0; ind < proc->graduate_rate && actind<numelts && cango; ind++, actind+=2) /* 2 accounts for two active list "elements" for each entry */
    {
      if (!q->PeekElt(ptr,actind))
	{
	  fprintf(simerr,"PeekElt gave invalid value in mark_stores_ready.\n");
	  exit(-1);
	}
      
      instance *tmpinst=GetTagCvtByPosn(ptr->tag,ind,proc);

      if (tmpinst->unit_type == uMEM && IsStore(tmpinst) && !tmpinst->st_ready && !tmpinst->stallqs &&tmpinst->strucdep == 0 && tmpinst->addr_ready) // no true dependence is possible; only struct dep can be a problem
	{
	  tmpinst->newst=0;
	  tmpinst->st_ready=1;
	  proc->ReadyUnissuedStores++;

#ifdef COREFILE
	  if (YS__Simtime > DEBUG_TIME)
	    fprintf(corefile,"Marking store %d as ready to issue\n",tmpinst->tag);
#endif
	  
	  if (!IsRMW(tmpinst)
#ifdef STORE_ORDERING
	      && (Processor_Consistency || SC_NBWrites)
#endif
	      )
	    {
	      GetMap(tmpinst,proc); // to see if it gives seg fault
	      if (tmpinst->exception_code == OK)
		{
		  tmpinst->in_memunit=0;
		  instance *memop = new instance(tmpinst);
		  memop->newst = 0;
		  memop->st_ready=1;
#ifndef STORE_ORDERING
		  proc->StoreQueue.Replace(tmpinst,memop);
#else
		  proc->MemQueue.Replace(tmpinst,memop);
#endif
		  
		}
	      else
		{
		  tmpinst->in_memunit=0;
		  proc->ReadyUnissuedStores--;
#ifndef STORE_ORDERING
		  proc->StoreQueue.Remove(tmpinst);
		  proc->st_tags.Remove(tmpinst->tag);
#else
		  proc->MemQueue.Remove(tmpinst);
#endif
		}
	      q->PeekElt(ptr2,actind+1);
	      mark_done_in_active_list(ptr, ptr2, tmpinst->exception_code, cycle);
	      /* a STORE in RC, PC, or SC w/non-blocking writes is
		 actually "done" when its address is ready, but we
		 should be careful in PC or SC w/non-blocking writes
		 not to let such a store issue unless it is at the top
		 of the memory unit, even if st_ready is set.  */
	    }
	}

      if (ptr->done == 0 || ptr->exception != OK)
	{
	  cango = 0;
	}
    }
}

/*************************************************************************/
/* remove_from_active_list : handles removal of instructions from active */
/*                         : list. Also computes components of execution */
/*                         : time.                                       */
/*************************************************************************/


instance *activelist::remove_from_active_list(int cycle, state *proc) /* return value is instance that causes exception */
{
  activelistelement *ptr, *ptrremove;
  instance *tmpinst;
  int gradded = 0, busy=0;
  if (q->NumInQueue() == 0)
    return NULL; /* no exception */
  
  
  while (q->NumInQueue() && gradded != proc->graduate_rate)
    {
      q->PeekHead(ptr);
      tmpinst = TagCvtHead(ptr->tag, proc);

      if (ptr->cycledone+simulate_ilp <= cycle && ptr->done != 0)
	{
	  if ((!tmpinst->in_memunit || tmpinst->exception_code != OK))
	    {
	      // finish it
	      q->Delete(ptr);

	      if (ptr->exception)
		{
		  /* Exception is set, we are in trouble */
		  UpdateTaghead(ptr->tag, proc); // THIS WILL BE CAUGHT THE FIRST TIME
		  Deleteactivelistelement(ptr,proc);
		  /* cwp, etc. will get set through flushactivelist, etc. */
		  return tmpinst;
		}

	      /* No exception, we can free the active list entry */
	      /* Also free the old physical register for later use */
	      if(ptr->regtype == REG_FP){
		proc->free_fp_list->addfreereg(ptr->phyreg);
	      }
	      else {
		if(ptr->phyreg != 0)
		  proc->free_int_list->addfreereg(ptr->phyreg);
	      }

	      /* We should also check to make sure that destinations
		 are indeed not busy (this is an easy mistake to make
		 when modifying code). At the same time, we'll now
		 update the logical register file (simulator
		 abstraction) */

	      if(tmpinst == NULL){
#ifdef COREFILE
		if(GetSimTime() > DEBUG_TIME)
		  fprintf(corefile, "Something is wrong: no translation\
for this TAG %d!\n", ptr->tag);
#endif
		fprintf(simerr,"Something is wrong: no translation for this TAG %d!\n", ptr->tag);
		exit(-1);
		return NULL;
	      }
	      if(tmpinst->code->rd_regtype == REG_FP || tmpinst->code->rd_regtype == REG_FPHALF)
		{
		  int logreg;
		  if (tmpinst->code->rd_regtype == REG_FP)
		    logreg = tmpinst->lrd;
		  else if (tmpinst->code->rd_regtype == REG_FPHALF)
		    logreg = unsigned(tmpinst->lrd)&~1U; // the logical register is really the double, not the half...
		      
		  proc->logical_fp_reg_file[logreg] =
		    proc->physical_fp_reg_file[tmpinst->prd];
		  if (proc->fpregbusy[tmpinst->prd])
		    {
		      YS__errmsg("BUSY FP DESTINATION REGISTER AT GRAD!!\n");
		      exit(-1);
		    }
		  proc->fpregbusy[tmpinst->prd] = 0;
		}
	      else
		{
		  proc->logical_int_reg_file[tmpinst->lrd] =
		    proc->physical_int_reg_file[tmpinst->prd];
		  if (proc->intregbusy[tmpinst->prd])
		    {
		      YS__errmsg("BUSY INT DESTINATION REGISTER AT GRAD!!\n");
		      exit(-1);
		    }
		  proc->intregbusy[tmpinst->prd] = 0;
		}

	      proc->logical_int_reg_file[tmpinst->lrcc] =
		proc->physical_int_reg_file[tmpinst->prcc];

	      if (proc->intregbusy[tmpinst->prcc])
		{
		  YS__errmsg("BUSY CC DESTINATION REGISTER AT GRAD!!\n");
		  exit(-1);
		}
	      proc->intregbusy[tmpinst->prcc] = 0;

	      /* Remove entry from active list */
	      ptrremove = ptr;

	      if(UpdateTaghead(ptrremove->tag, proc) == 2){
#ifdef COREFILE
		if(GetSimTime() > DEBUG_TIME)
		  {
		    fprintf(corefile, "Graduating pc %d tag %d: %s", tmpinst->pc, tmpinst->tag,inames[tmpinst->code->instruction]);
		    switch (tmpinst->code->rd_regtype)
		      {
		      case REG_INT:
			if (tmpinst->lrd != ZEROREG)
			  fprintf(corefile, " i%d->%d",tmpinst->lrd,tmpinst->rdvali);
			break;
		      case REG_FP:
			fprintf(corefile, " f%d->%f",tmpinst->lrd,tmpinst->rdvalf);
			break;
		      case REG_FPHALF:
			fprintf(corefile, " fh%d->%f",tmpinst->lrd,tmpinst->rdvalfh);
			break;
		      case REG_INTPAIR:
			if (tmpinst->lrd != ZEROREG)
			  fprintf(corefile, " i%d->%d i%d->%d",tmpinst->lrd,tmpinst->rdvalipair.a,tmpinst->lrd+1,tmpinst->rdvalipair.b);
			else
			  fprintf(corefile, " i%d->%d",tmpinst->lrd+1,tmpinst->rdvalipair.b);
			break;
		      case REG_INT64:
			fprintf(corefile, " ll%d->%lld",tmpinst->lrd,tmpinst->rdvalll);
			break;
		      default: 
			fprintf(corefile, " rdX = XXX");
			break;
		      }
		    if (tmpinst->lrcc != ZEROREG)
		      fprintf(corefile, " i%d->%d",tmpinst->lrcc,tmpinst->rccvali);
		    if (IsStore(tmpinst) && !IsRMW(tmpinst))
		      {
			switch (tmpinst->code->rs1_regtype)
			  {
			  case REG_INT:
			    fprintf(corefile, " i%d->[%d]",tmpinst->rs1vali,tmpinst->addr);
			    break;
			  case REG_FP:
			    fprintf(corefile, " f%f->[%d]",tmpinst->rs1valf,tmpinst->addr);
			    break;
			  case REG_FPHALF:
			    fprintf(corefile, " fh%f->[%d]",tmpinst->rs1valfh,tmpinst->addr);
			    break;
			  case REG_INTPAIR:
			    fprintf(corefile, " i%d->[%d] i%d->[%d]",tmpinst->rs1valipair.a,tmpinst->addr,tmpinst->rs1valipair.b,tmpinst->addr+4);
			    break;
			  case REG_INT64:
			    fprintf(corefile, " ll%lld->[%d]",tmpinst->rs1valll,tmpinst->addr);
			    break;
			  default: 
			    break;
			  }
			
		      }
		    fprintf(corefile,"\n");
		  }
#endif
		gradded++;

		GraduateTagConverter(ptrremove->tag, proc);
		tmpinst->depctr = -2;
		/* To indicate that we should not look at this */
	    
		if (tmpinst->code->instruction==iILLTRAP
		    && tmpinst->code->aux2 >= 4096)
		  {
		    if (tmpinst->code->aux2 > 4096)
		      {
			if (proc->agg_lat_type != -1)
			  {
			    StatrecUpdate(proc->lat_contrs[proc->agg_lat_type],
					  proc->curr_cycle-proc->last_counted,1.0);
			    proc->last_graduated=proc->last_counted=proc->curr_cycle;
			  }
			
			proc->agg_lat_type = tmpinst->code->aux2-4096; 
		      }
		    else /* == 4096, so end aggregate */
		      {
			if (proc->agg_lat_type != -1)
			  StatrecUpdate(proc->lat_contrs[proc->agg_lat_type],
					proc->curr_cycle-proc->last_counted,1.0);
			proc->last_graduated=proc->last_counted=proc->curr_cycle;
			proc->agg_lat_type = -1;
		      }
		  }
		else if (proc->agg_lat_type == -1)
		  {
		    int lastcount = proc->curr_cycle-proc->last_counted;
		    if (lastcount > 0 || proc->graduate_rate == 0)
		      // how much do we need to account for this tag
		      {
			/* we account in this fashion if we have
			   infinite grad rate, single grad rate, or if
			   we have finite multiple grads but we didn't
			   have a completely busy cycle last time */

			if (tmpinst->miss != mtL1HIT)
			  {
			    // if it's a miss, count it in its miss
			    // latency in addition to its regular
			    // latency

			    switch (lattype[tmpinst->code->instruction])
			      {
			      case lRD:
				StatrecUpdate(proc->lat_contrs[lRDmiss],(double)lastcount,1.0);
				break;
			      case lWT:
				StatrecUpdate(proc->lat_contrs[lWTmiss],(double)lastcount,1.0);
				break;
			      case lRMW:
				StatrecUpdate(proc->lat_contrs[lRMWmiss],(double)lastcount,1.0);
				break;
			      default:
				// don't know why we're here
				break;
			      }
			  }
			if (tmpinst->partial_overlap)
			  {
			    StatrecUpdate(proc->partial_otime,(double)lastcount,1.0);
			  }
		    
			StatrecUpdate(proc->lat_contrs[lattype[tmpinst->code->instruction]],
				      (double)lastcount,1.0);
		    
			switch(lattype[tmpinst->code->instruction])
			  {
			  case lRD:
			    StatrecUpdate(proc->lat_contrs[lRD_L1+(int)tmpinst->miss],(double)lastcount,1.0);
			    if (tmpinst->latepf)
			      {
#ifdef COREFILE
				if (YS__Simtime > DEBUG_TIME)
				  fprintf(corefile,"Tag %d was a late pf by %d cycles at grad.\n",tmpinst->tag,lastcount);
#endif
				StatrecUpdate(proc->lat_contrs[lattype[tmpinst->code->instruction]+lRMW_PFlate-lRMW],(double)lastcount,1.0);
			      }
#ifdef COREFILE
			    if (YS__Simtime > DEBUG_TIME && tmpinst->miss == mtL1HIT)
			      {
				fprintf(corefile,"Tag %d at time %.1f took %d cycles for a hit.\n", tmpinst->tag,YS__Simtime,lastcount);
			      }
#endif
#ifdef DEBUG_HIT
			    if (tmpinst->miss == mtL1HIT && lastcount>20)
			      {
				fprintf(simout,"Tag %d at time %.1f took %d cycles for a hit.\n", tmpinst->tag,YS__Simtime,lastcount);
			      }
#endif
			    break;
			  case lWT:
#ifndef STORE_ORDERING
#ifdef COREFILE
			    if (YS__Simtime > DEBUG_TIME)
			      {
				fprintf(corefile,"Tag %d at time %.1f took %d cycles for a write.\n", tmpinst->tag,YS__Simtime,lastcount);
			      }
#endif
#ifdef DEBUG_HIT
			    if (lastcount>20)
			      {
				fprintf(simout,"Tag %d at time %.1f took %d cycles for a write.\n", tmpinst->tag,YS__Simtime,lastcount);
			      }
#endif
#endif

			    StatrecUpdate(proc->lat_contrs[lWT_L1+(int)tmpinst->miss],(double)lastcount,1.0);
			    if (tmpinst->latepf)
			      {
#ifdef COREFILE
				if (YS__Simtime > DEBUG_TIME)
				  fprintf(corefile,"Tag %d was a late pf by %d cycles at grad.\n",tmpinst->tag,lastcount);
#endif
				StatrecUpdate(proc->lat_contrs[lattype[tmpinst->code->instruction]+lRMW_PFlate-lRMW],(double)lastcount,1.0);
			      }
			    break;
			  case lRMW:
			    StatrecUpdate(proc->lat_contrs[lRMW_L1+(int)tmpinst->miss],(double)lastcount,1.0);
			    if (tmpinst->latepf)
			      {
#ifdef COREFILE
				if (YS__Simtime > DEBUG_TIME)
				  fprintf(corefile,"Tag %d was a late pf by %d cycles at grad.\n",tmpinst->tag,lastcount);
#endif
				StatrecUpdate(proc->lat_contrs[lattype[tmpinst->code->instruction]+lRMW_PFlate-lRMW],(double)lastcount,1.0);
			      }
			    break;
			  default:
			    break;
			  }
		    
			proc->last_counted = proc->curr_cycle;
		      }
		    proc->last_graduated=proc->curr_cycle;
		    busy++;
		  }
	    
		proc->graduation_count++;
		proc->graduates++;
		if (tmpinst->partial_overlap)
		  proc->partial_overlaps++;
	    
		DeleteInstance(tmpinst,proc);
	      }
	  
	      Deleteactivelistelement(ptrremove,proc);
	    }
	  else
	    break;
	}
      else
	break;
    }

  if (busy == proc->graduate_rate && proc->graduate_rate > 0) // meaningless for infinite (0) graduation rates
    {
      StatrecUpdate(proc->lat_contrs[lBUSY],1.0,1.0);
      proc->last_counted = proc->curr_cycle+FASTER_NET;
    }

  return NULL; /* in other words, no exception */
}
