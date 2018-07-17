/*

  active.cc

  Member Function definitions for the active list class

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


#include <stdio.h>
#include <memory.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "Processor/circq.h"
#include "Processor/active.h"
#include "Processor/instance.h"
#include "Processor/tagcvt.h"
#include "Processor/state.h"
#include "Processor/freelist.h"
#include "Processor/FastNews.h"
#include "Processor/processor_dbg.h"
#include "Processor/mainsim.h"
#include "Processor/simio.h"

/*************************************************************************/
/* normalize_to_power_of_2: helper function to round up to power of 2    */
/*************************************************************************/

inline int normalize_to_power_of_2(int n)
{
  int t=0;
  n += n-1;
  while (n >>= 1) t++;
  return (1 << t);
}


/* Member functions for the active list structure */
/*************************************************************************/
/*  Constructor :  since active list is a fast circular queue, it is     */
/*              :  implemented with a power-of-2 length structure        */
/*************************************************************************/

activelist::activelist(int max_elemts)
{
  mx = max_elemts;
  q = new circq<activelistelement *>(normalize_to_power_of_2(mx));
}

/*************************************************************************/
/* add_to_active_list: Add the old logical to physical mapping           */
/*************************************************************************/
int activelist::add_to_active_list(int tag, int oldlogical, int
				   oldphysical, REGTYPE regtype,state *proc)
{
  if(q->NumInQueue() == mx) return (1);
  activelistelement *newentry = Newactivelistelement(tag, oldlogical,
						     oldphysical,regtype,proc);
  q->Insert(newentry); // inserts it at end of list
#ifdef COREFILE
  if(GetSimTime() > DEBUG_TIME)
    fprintf(corefile, "Add to active list : tag %d :Total now %d\n", tag, q->NumInQueue());
#endif
  return (0);
}

/*************************************************************************/
/* cmp: Used in binary search of active list                             */
/*************************************************************************/

static int cmp(activelistelement *const& a, activelistelement *const& b)
{
  return (a->tag - b->tag);
}

/*************************************************************************/
/* mark_done_in_active_list : called on instruction completion to inform */
/*                          : graduation stage that this instruction has */
/*                          : completed                                  */
/*************************************************************************/

int activelist::mark_done_in_active_list(int tagnum, int except, int curr_cycle)
{
  activelistelement tmp(tagnum,0,0,REG_INT); // junk
  activelistelement *ptr,*ptr2;

  int found = q->Search2(&tmp,ptr,ptr2,cmp);
  if (found)
    {
      ptr->done = 1; 
      ptr->cycledone = curr_cycle;
      ptr->exception = except;
      ptr2->done = 1; 
      ptr2->cycledone = curr_cycle;
      ptr2->exception = except;
      return 0;
    }
  else
    {
#ifdef COREFILE
      if(GetSimTime() > DEBUG_TIME)
	fprintf(corefile,"MARK DONE FAILED FOR TAG %d\n", tagnum);
#endif
      return 1;
    }
}

/*************************************************************************/
/* mark_done_in_active_list: an overloaded version of above              */
/*************************************************************************/

int activelist::mark_done_in_active_list(activelistelement *ptr, activelistelement *ptr2, int except, int curr_cycle)
{
  ptr->done = 1; 
  ptr->cycledone = curr_cycle;
  ptr->exception = except;
  ptr2->done = 1; 
  ptr2->cycledone = curr_cycle;
  ptr2->exception = except;
  return 0;
}

/*************************************************************************/
/* flag_exception_in_active_list: after "completion", this instruction   */
/* was discovered to have an exception. Used in the case of speculative  */
/* load execution.                                                       */
/*************************************************************************/

int activelist::flag_exception_in_active_list(int tagnum, int except)
{
  activelistelement tmp(tagnum,0,0,REG_INT); // junk
  activelistelement *ptr,*ptr2;

  int found = q->Search2(&tmp,ptr,ptr2,cmp);
  if (found)
    {
      ptr->exception = except;
      ptr2->exception = except;
      return 0;
    }
  else
    {
      return 1;
    }
}

/*************************************************************************/
/* flush_active_list: empty elements from the list on a misprediction or */
/*                    exception. Free registers in the process.          */
/*************************************************************************/


/* returns -1 for empty active list , 0 on success
   -2 if element not found in list,
   */
int activelist::flush_active_list(int tag, state *proc)
{
#ifdef COREFILE
  if(proc->curr_cycle > DEBUG_TIME)
    fprintf(corefile," FLUSH EVERYTHING AFTER %d\n", tag);
#endif
#ifdef DEBUG_PREFETCH
  int pfs=0;
#endif
  activelistelement *ptr, *ptrremove;
  if (q->NumInQueue() == 0)
    return -1;
  while (q->NumInQueue())
    {
      q->PeekTail(ptr);
      if (ptr->tag <= tag)
	{
#ifdef DEBUG_PREFETCH
	  if (pfs !=0)
	    {
	      instance *tmpinst = TagCvtTail(ptr->tag, proc);
	      fprintf(simout,"Proc %d: culprit was tag %d -- instruction at pc %d\n",proc->proc_id,ptr->tag,tmpinst->pc);
	    }
#endif
	  break;
	}
      
      q->DeleteFromTail(ptr);
      
      /* *** THIS IS ALWAYS LAST IN TAG CONVERTER */
      instance *tmpinst = TagCvtTail(ptr->tag, proc);

      if(tmpinst == NULL){
#ifdef COREFILE
	if(proc->curr_cycle > DEBUG_TIME)
	  fprintf(corefile,"Something is wrong, I dont have translation for tag %d", ptr->tag);
#endif
	return(-1);
      }
      
      /* Remove entry from active list */
      ptrremove = ptr;
#ifdef COREFILE
      if(proc->curr_cycle > DEBUG_TIME)
	fprintf(corefile,"Tag %d is being flushed from active list \n", ptr->tag);
#endif
      
      if(UpdateTagtail(ptrremove->tag, proc) == 2){
	FlushTagConverter(ptrremove->tag, proc);

	/* We need to free the registers but only ONCE */
#ifdef DEBUG_PREFETCH
	if (tmpinst->code->instruction == iPREFETCH)
	  {
	    fprintf(simout,"Proc %d flushing prefetch tag %d: %s\n",proc->proc_id,tmpinst->tag,(tmpinst->memprogress)?"issued":"unissued");
	    pfs++;
	  }
#endif
	
	if(tmpinst->code->rd_regtype == REG_FP || tmpinst->code->rd_regtype == REG_FPHALF)
	  {
	    proc->fpregbusy[tmpinst->prd] = 0;
	    proc->free_fp_list->addfreereg(tmpinst->prd);
	    proc->dist_stallq_fp[tmpinst->prd].ClearAll(proc);
	  }
	else // INT, INT64, INTPAIR?
	  {
	    proc->intregbusy[tmpinst->prd] = 0;
	    if(tmpinst->prd != 0)
	      proc->free_int_list->addfreereg(tmpinst->prd);
	    proc->dist_stallq_int[tmpinst->prd].ClearAll(proc);
	    proc->intregbusy[tmpinst->prdp] = 0;
	    if(tmpinst->prdp != 0)
	      proc->free_int_list->addfreereg(tmpinst->prdp);
	    proc->dist_stallq_int[tmpinst->prdp].ClearAll(proc);
	  }
	
	proc->intregbusy[tmpinst->prcc] = 0;
	if(tmpinst->prcc != 0)
	  proc->free_int_list->addfreereg(tmpinst->prcc);
	proc->dist_stallq_int[tmpinst->prcc].ClearAll(proc);

	if (STALL_ON_FULL &&
	    ((tmpinst->unit_type != uMEM && tmpinst->issuetime == INT_MAX) ||
	     (stat_sched && tmpinst->unit_type == uMEM && tmpinst->addrissuetime == INT_MAX))) // it wasn't issued
	  {
	    proc->unissued--;
#ifdef COREFILE
	    if (YS__Simtime > DEBUG_TIME)
	      fprintf(corefile,"unissued now %d\n",proc->unissued);
#endif
	  }
	
	if (tmpinst->code->wpchange && !(tmpinst->strucdep>0 && tmpinst->strucdep <5) && tmpinst->exception_code != WINTRAP) /* unupdate CWP that has been changed */
	  {
	    proc->cwp = unsigned(proc->cwp - tmpinst->code->wpchange) & (NUM_WINS-1);
	    if (!proc->privstate) /* these are not modified in privstate, so nothing to undo */
	      {
		proc->CANSAVE -= tmpinst->code->wpchange;
		proc->CANRESTORE += tmpinst->code->wpchange;
	      }
#ifdef COREFILE
	    if (YS__Simtime > DEBUG_TIME)
	      fprintf(corefile,"Flushing winchange instr. Now CANSAVE %d, CANRESTORE %d\n",proc->CANSAVE,proc->CANRESTORE);
#endif
	  }
	DeleteInstance(tmpinst,proc);
      }
      Deleteactivelistelement(ptr,proc);
      
    }
  return 0;
}

/**************************************************************************/
/* An element of the active list saves an old logical-to-physical mapping */
/**************************************************************************/

activelistelement::activelistelement(int tagnum, int logreg, int
				     physreg,REGTYPE type, int dne, int except)
{
  tag = tagnum;
  logicalreg = logreg;
  phyreg = physreg;
  done = dne; 
  cycledone = -1;
  exception = except;
  regtype = type;
}








