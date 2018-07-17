/*
  branchresolve.cc

  This function makes it possible to recover from a misspeculated branch or
  continue normally after a properly speculated branch. 
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


#include "Processor/decode.h"
#include "Processor/branchq.h"
#include "Processor/state.h"
#include "Processor/FastNews.h"
#include "Processor/memory.h"
#include "Processor/exec.h"
#include "Processor/simio.h"
#include <stdlib.h>

extern "C"
{
#include "MemSys/simsys.h"
}

/***************************************************************************/
/* GoodPrediction : Free up shadow mapper and update processor speculation */
/*                : level one we know that the prediction was correct      */
/***************************************************************************/

void GoodPrediction(instance *inst, state *proc)
{
  if (inst->code->uncond_branch == 4) /* if it's a RAS access */
    proc->ras_good_predicts++;
  else
    proc->bpb_good_predicts++;
  int tag_to_use = inst->tag;
  instance *inst2;

  if(inst->code->annul == 0)
    {
      tag_to_use = inst->tag + 1;
      inst2 = convert_tag_to_inst(tag_to_use, proc);
    }
  else
    inst2 = inst;

  if (tag_to_use == proc->instruction_count)
    {
      /* We havent issued the delay slot yet! */
      proc->copymappernext = 0; /* We dont need to copy the
				   mappers now */
      return;
    }
       
  /* Remove the entry from the Branch Q */
  if( RemoveFromBranchQ(tag_to_use, proc) != 0)
    {
      if(inst2->branchdep == 2)
	{
#ifdef COREFILE
	  if(proc->curr_cycle > DEBUG_TIME)
	    fprintf(corefile, "shadow mapper allocated/released\n");
#endif
	  if (proc->stall_the_rest != inst2->tag)
	    {
	      fprintf(simerr,"BRANCH STALLING ERROR P%d(%d/%d) %s:%d!!\n", proc->proc_id,inst->tag,inst->pc,__FILE__,__LINE__);
	      exit(-1);
	    }
	  if (!STALL_ON_FULL || proc->type_of_stall_rest != eMEMQFULL)
	    unstall_the_rest(proc); // we might have also had later memq full problems if stall_on_full

	  inst2->branchdep = 0;
	  instance *inst3;
	  inst3 = proc->BranchDepQ.GetNext(proc); // this should be inst2 itself
	  if (inst3 && inst3->tag != inst2->tag) /* it wouldn't have been in the branchdepq if it stalled in rename */
	    {
	      fprintf(simerr,"BRANCH STALLING ERROR P%d(%d/%d) %s:%d!!\n", proc->proc_id,inst->tag,inst->pc,__FILE__,__LINE__);
	      exit(-1);
	    }
	  if (inst3) /* if it was in the branch depq */
	    inst3->stallqs--;
	  return;
	}
      else
	{
#ifdef COREFILE
	  if(proc->curr_cycle > DEBUG_TIME)
	    fprintf(corefile,"<decode.cc> Tag not found in branch queue\n");
#endif
	  // continue;
	  return;
	}
    }
  else
    {
#ifdef COREFILE
      if(proc->curr_cycle > DEBUG_TIME)
	fprintf(corefile, "Mapper for tag %d removed \n", tag_to_use);
#endif
      return;
    }
}

/*************************************************************************/
/* BadPrediction : Steps to be taken on event of a branch misprediction  */
/*************************************************************************/

void BadPrediction(instance *inst, state *proc)
{
  /* we assume that proc->pc and proc->npc have been
     set properly before calling BadPrediction */
  
  if (inst->code->uncond_branch == 4) /* if it's a RAS access */
    proc->ras_bad_predicts++;
  else
    proc->bpb_bad_predicts++;
  
  int tag_to_use = inst->tag;
  instance *inst2;
  if(inst->code->annul == 0)
    {
      tag_to_use = inst->tag + 1;
      inst2 = convert_tag_to_inst(tag_to_use, proc);
    }
  else
    inst2 = inst;
    
  if(tag_to_use == proc->instruction_count)
    {
      /* The delay slot (and things after it) have not been issued yet! */
      proc->copymappernext = 0; /* We are saved the touble
				   of saving mappers */
      return;
    }

  inst2->npc = proc->pc; /* to indicate that the next instruction after this
			    one is the point from which the processor will
			    be fetching after this misprediction */
  
  if(inst2->branchdep == 2)
    {
#ifdef ERRCORE
      fprintf(simerr, "shadow mapper released freed\n");
#endif
#ifdef COREFILE
      if(proc->curr_cycle > DEBUG_TIME)
	fprintf(corefile, "shadow mapper released freed\n");
#endif
      if (proc->stall_the_rest != inst2->tag)
	{
	  fprintf(simerr,"BRANCH STALLING ERROR P%d(%d/%d) %s:%d!!\n", proc->proc_id,inst->tag,inst->pc,__FILE__,__LINE__);
	  exit(-1);
	}      
      if (!STALL_ON_FULL || proc->type_of_stall_rest != eMEMQFULL)
	unstall_the_rest(proc);
      inst2->branchdep = 0;
      instance *inst3;
      inst3 = proc->BranchDepQ.GetNext(proc); // result should be inst2 itself, if anything
      if (inst3 && inst3->tag != inst2->tag) /* wouldn't have been in branchq on renaming stall */
	{
	  fprintf(simerr,"BRANCH STALLING ERROR P%d(%d/%d) %s:%d!!\n", proc->proc_id,inst->tag,inst->pc,__FILE__,__LINE__);
	  exit(-1);
	}
      if (inst3)
	inst3->stallqs--;
      
    }
  else
    {  
      /* Copy the corresponding mapper entry to the original mappers */
      /* Also copy the busy state of the integers at that instant */
      
      CopyBranchQ(tag_to_use, proc);
    }
  /* Flush the mapper tables to remove this and later branches if any */
  FlushBranchQ(tag_to_use, proc);
  
  /* We should flush the memory queue too so that
     the loads and stores odnt keep waiting to verify
     that the branch is done or not... */
  FlushMems(tag_to_use,proc);
  
  /* Flush all entries in the stallQ which were sent in after the
     branch. Must be flushed before active list since some entries in
     stallQ are not in active list. */
  FlushStallQ(tag_to_use, proc);

  /* Flush the active list to remove any entries after the branch */
  /* While flushing, put back the registers in the free list */
  
  int pre = proc->active_list->NumElements();
  FlushActiveList(tag_to_use, proc);
  int post = proc->active_list->NumElements();
  StatrecUpdate(proc->bad_pred_flushes,double(pre-post),1.0);
  
  return;
}

void HandleUnPredicted(instance *inst, state *proc)
{
  /* we assume that proc->pc and proc->npc have been
     set properly before calling HandleUnPredicted */
  int tag_to_use = inst->tag;
  instance *inst2;
  if(inst->code->annul == 0)
    {
      tag_to_use = inst->tag + 1;
      inst2 = convert_tag_to_inst(tag_to_use, proc);
    }
  else
    inst2 = inst;
  
  if(tag_to_use == proc->instruction_count)
    {
      /* The delay slot has not been issued yet! */
      return;
    }

  inst2->npc = proc->pc; /* to indicate that the next instruction after this
			    one is the point from which the processor will
			    be fetching after this misprediction */
  inst2->branchdep = 0;
  
}

/*************************************************************************/
/* FlushBranchQ : Flush all elements (shadow mappers) in the branchq     */
/*************************************************************************/

void FlushBranchQ(int tag, state *proc)
{
  int i=0;
  BranchQElement *junk;
  while (proc->branchq.NumItems())
    {
      proc->branchq.GetTail(junk);
      if (junk->tag >= tag)
	{
	  proc->branchq.RemoveTail();
	  DeleteMapTable(junk->specmap,proc);
	  DeleteBranchQElement(junk,proc);
	  i++;
	}
      else
	break;
    }
}

/*************************************************************************/
/* GetElement : Get branch queue element that matches given tag          */
/*************************************************************************/


BranchQElement *GetElement(int tag,state *proc)
{
  MemQLink<BranchQElement *> *stepper = NULL;
  BranchQElement *junk = NULL;
  while ((stepper = proc->branchq.GetNext(stepper)) != NULL)
    {
      junk=stepper->d;
      if (junk->tag == tag)
	return junk;
    }
  return NULL;
}

/*************************************************************************/
/* RemoveFromBranchQ : Remove entry specified by tag from branch queue   */
/*************************************************************************/

int RemoveFromBranchQ(int tag, state *proc) // for successful predictions
{
  // this part is like the way we handle memory system
  BranchQElement *junk = GetElement(tag,proc);

  if (junk == NULL)
    return -1;

  junk->done = 1;
  proc->branchq.Remove(junk);
  DeleteMapTable(junk->specmap,proc);
  DeleteBranchQElement(junk,proc);
  instance *i;
  i = proc->BranchDepQ.GetNext(proc);
  if (i != NULL)
    {
      i->stallqs--;		
      if (AddBranchQ(i->tag,proc) != 0)
	{
	  fprintf(simerr,"SERIOUS ERROR IN REVIVING A SLEPT BRANCH!!! P%d(%d/%d) %s:%d!!\n", proc->proc_id,i->tag,i->pc,__FILE__,__LINE__);
	  exit(-1);
	}
      i->branchdep = 0;
#ifdef COREFILE
      if (YS__Simtime > DEBUG_TIME)
	fprintf(corefile,"shadow mapper allocated for tag %d\n",i->tag);
#endif
      if (proc->stall_the_rest != i->tag)
	{
	  fprintf(simerr,"SERIOUS ERROR IN REVIVING A SLEPT BRANCH!!! P%d(%d/%d) %s:%d!!\n", proc->proc_id,i->tag,i->pc,__FILE__,__LINE__);
	  exit(-1);
	}
      if (!STALL_ON_FULL || proc->type_of_stall_rest != eMEMQFULL)
	unstall_the_rest(proc);
    }
  
  return 0;
}

/*************************************************************************/
/* AddBranchQ : Initialize shadow mappers and add to list of outstanding */
/*            : branches. return -1 if out out shadow mappers            */
/*************************************************************************/

int AddBranchQ(int tag, state *proc)
{
  if(proc->branchq.NumItems() >= MAX_SPEC){
    /* out of speculations */
#ifdef COREFILE
    if(proc->curr_cycle > DEBUG_TIME)
      fprintf(corefile, "Out of shadow mappers !\n");
#endif
    return (-1);
  }

  MapTable *specmap = NewMapTable(proc);
  if (specmap == NULL)
    {
      fprintf(simerr,"Got a NULL map table entry!!\n");
      exit(-1);
    }

  memcpy(specmap->imap,proc->intmapper,
	 sizeof(int)*NO_OF_LOGICAL_INT_REGISTERS);
  memcpy(specmap->fmap,proc->fpmapper,
	 sizeof(int)*NO_OF_LOGICAL_FP_REGISTERS);
  
  /* Add it into our Branch List */
  BranchQElement *tmpptr = NewBranchQElement(tag,specmap,proc);

  proc->branchq.Insert(tmpptr);
  return 0;
}

/*************************************************************************/
/* CopyBranchQ : Set up the state after a branch misprediction           */
/*************************************************************************/

int CopyBranchQ(int tag, state *proc)
{
  /* Let us get the entry corresponding to this tag */
  BranchQElement *entry = GetElement(tag,proc);
  
  if(entry == NULL){
#ifdef COREFILE
    if(proc->curr_cycle > DEBUG_TIME)
      fprintf(corefile, "<branchspec.cc> Error : Shadow mapper not found for tag %d\n",tag);
#endif
    return (-1);
  }
  
  /* Change the mappers to point to the right ones */
  /* First the integer and the fp mappers */
  MapTable *tmpmap = proc->activemaptable;
  proc->activemaptable = entry->specmap;
  entry->specmap = tmpmap;

  proc->intmapper = proc->activemaptable->imap;
  proc->fpmapper = proc->activemaptable->fmap;
  
  return (0);
}

/*************************************************************************/
/* FlushActiveList : Flush the active list on a branch misprediction     */
/*************************************************************************/

int FlushActiveList(int tag, state *proc)
{
  proc->copymappernext=0;
  proc->unpredbranch=0;
  unstall_the_rest(proc);
    /* Flush out the active lsit suitable freeing
       the physical registers used too. */
    return(proc->active_list->flush_active_list(tag, proc));
}

/*************************************************************************/
/* FlushStallQ : Flush stall queue elements on branch misprediction      */
/*************************************************************************/

int FlushStallQ(int tag, state *proc)
{
  stallqueueelement *stallqitem = proc->stallq->get_tail();
  instance *tmpinst;
  while(stallqitem != NULL && stallqitem->inst->tag > tag)
    {
      /* Check for tag > tagof our valid instructions */
      tmpinst = stallqitem->inst;
      if (tmpinst->code->wpchange && !(tmpinst->strucdep>0 && tmpinst->strucdep <5) && tmpinst->exception_code != WINTRAP) /* unupdate CWP that has been changed */
	{
	  proc->cwp = unsigned(proc->cwp - tmpinst->code->wpchange) & (NUM_WINS-1);
	    if (!proc->privstate) /* these are not modified in privstate, so nothing to undo */
	      {
		proc->CANSAVE -= tmpinst->code->wpchange;
		proc->CANRESTORE += tmpinst->code->wpchange;
	      }
	}
      tmpinst->depctr = -2;
      if (tmpinst->strucdep > 0 && tmpinst->strucdep < 7) // renaming only partially done
	{
	  FlushTagConverter(tmpinst->tag,proc);
	  DeleteInstance(tmpinst,proc);
	}
      else
	tmpinst->tag = -1;

      proc->stallq->delete_entry(stallqitem,proc);
      stallqitem = proc->stallq->get_tail();
    }
  
  return(0);
}




