/*
  stallq.cc

  This file contains the implementation of the member functions
  for the MiniStallQ and stallqueue classes

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


#include "Processor/stallq.h"
#include "Processor/units.h"
#include "Processor/instance.h"
#include "Processor/state.h"
#include "Processor/FastNews.h"
#include "Processor/memory.h"
#include "Processor/decode.h"
#include "Processor/simio.h"

#include <stddef.h>

/* ----------------------------------------------------- */
/* Member function defintions for the stallqueue class */

stallqueue::stallqueue()
{
  startptr = NULL;
  endptr = NULL;
}
stallqueue::~stallqueue()
{
  /* Nothing I can think of right now! */
}

void stallqueue::append(stallqueueelement *newentry)
{
  if(startptr == NULL){
    startptr = newentry;
    endptr = newentry;
    newentry->prev = NULL;
    newentry->next = NULL;
  }
  else{
    endptr->next = newentry;
    newentry->prev = endptr;
    newentry->next = NULL;
    endptr=newentry;
  }
}

void stallqueue::delete_entry(stallqueueelement *entry,state *proc)
{
  if(entry == startptr){
    startptr = entry->next;
    if(entry->next != NULL) entry->next->prev = NULL;
  }
  else
    entry->prev->next = entry->next;

  if(entry == endptr){
    endptr = entry->prev;
    if(entry->prev != NULL) entry->prev->next = NULL;
  }
  else
    entry->next->prev = entry->prev;
  Deletestallqueueelement(entry,proc);

}

stallqueueelement *stallqueue::get_next_entry(stallqueueelement *curr_entry)
{
  if( curr_entry == NULL )
    return(startptr);
  else
    return(curr_entry->next);
}     


void MiniStallQ::AddElt(instance *inst, state *proc, int b)
{
  MiniStallQElt *elt=NewMiniStallQElt(inst,b,proc);
  if (tail == NULL)
    {
      head=tail=elt;      
    }
  else
    {
      tail->next=elt;
      tail=elt;
    }
}

void MiniStallQ::ClearAll(state *proc)
{
  MiniStallQElt *old;
  while (head != NULL)
    {
      head->ClearBusy(proc);
      old=head;
      head=head->next;
      DeleteMiniStallQElt(old,proc);
    }
  head=tail=NULL;
}

instance *MiniStallQ::GetNext(state *proc)
{
  if (head == NULL)
    {
      return NULL;
    }
  else
    {
      MiniStallQElt *stepper = head;
      MiniStallQElt *old;
      while (stepper != NULL && (stepper->inst->tag != stepper->tag)) // ||
	//				 (stepper->inst->unit_type == uMEM &&
	// stepper->inst->depctr == -2)))
	{
	  old=stepper;
	  stepper=stepper->next;
	  DeleteMiniStallQElt(old,proc);
	}
      if (stepper == NULL)
	{
	  head=tail=NULL;
	  return NULL;
	}
      instance *res;
      res=stepper->inst;
      old = stepper;
      head=stepper->next;
      if (head == NULL)
	{
	  tail = NULL;
	}
      DeleteMiniStallQElt(old,proc);
      return res;
    }
}

int MiniStallQElt::ClearBusy(state *proc)
{
  if (inst->tag != tag)
    {
      return 0;
    }
  inst->busybits &= CLEAR_MASK;
  if (inst->unit_type == uMEM && 
      !(inst->busybits & (BUSY_SETRS2 | BUSY_SETRSCC)) &&
      ((~CLEAR_MASK) & (BUSY_SETRS2 | BUSY_SETRSCC)))
    {
      if(inst->code->rs2_regtype == REG_INT)
	inst->rs2vali = proc->physical_int_reg_file[inst->prs2];
      else
	{
	  fprintf(simerr,"Mem with rs2 not an integer??\n");
	  exit(-1);
	}

      inst->rsccvali =  proc->physical_int_reg_file[inst->prscc];
      
      inst->addrdep = 0;
      if (inst->strucdep == 0) // has a slot in memory system 
	CalculateAddress(inst,proc);
    }
  
  if (inst->busybits == BUSY_ALLCLEAR)
    {
      inst->truedep=0;
      SendToFU(inst,proc);
    }
  return 1;
}

MiniStallQElt *MiniStallQElt::GetNext(instance *& i,state *proc)
{
  if (inst->tag != tag)
    {
      i=NULL;
      MiniStallQElt *nx;
      while (next != NULL) /* DO A FLUSH!! */
	{
	  nx=next;
	  next=next->next;
	  DeleteMiniStallQElt(nx,proc);
	}
      return NULL;
    }
  i=inst;
  return next;
}

MiniStallQElt::MiniStallQElt(instance *i,int b):inst(i),tag(i->tag),CLEAR_MASK(b),next(NULL) {}
