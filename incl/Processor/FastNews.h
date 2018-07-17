/*****************************************************************************\
*                                                                            *
*  FastNews.h                                                                *
*                                                                            *
*  This file contains the definition and implementation of member functions  *
*  belonging to the allocator class.                                         *
******************************************************************************/
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


#ifndef _FAST_NEWS_H_
#define _FAST_NEWS_H_ 1

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include "instance.h"
#include "state.h"
#include "branchq.h"
#include "stallq.h"
#include "active.h"
#include "tagcvt.h"
#include "simio.h"

extern void *operator new(size_t, void *);

/*****************************************************************************/
/*  NewInstance: Returns a pointer to an instance from the instances pool    */
/*****************************************************************************/

inline instance *NewInstance(instr *i,state *proc)
{

  /* get an instance from the instances pool */
  instance *in = proc->instances->Get();
  if (in == NULL)    {
    fprintf(simerr,"NewInstance returns NULL!\n");
    exit(-1);
  }
  else if (in->inuse)    {
    fprintf(simerr,"Re-allocating an inuse instance element!!!\n");
    exit(-1);
  }
  else
    in->inuse=1;                       /* mark the instance as being in use */
  return new (in) instance(i,proc);
}

/*****************************************************************************/
/*  DeleteInstance: Returns an instance pointer back to the instances pool   */
/*****************************************************************************/

inline void DeleteInstance(instance *inst, state *proc)
{
  if (!inst->inuse)    {
    fprintf(simerr,"De-allocating an already free instance!!!\n");
    exit(-1);
  }
  inst->tag = -1;                  
  inst->inuse=0;                         /* mark the instance as not in use */
  inst->instance::~instance();
  proc->instances->Putback(inst);
  return;
}

/*****************************************************************************/
/*  NewBranchQElement: Returns a pointer to a new branch queue element       */
/*****************************************************************************/

inline BranchQElement *NewBranchQElement(int tg, MapTable *map,
					 /*int shad,*/state *proc)
{
  BranchQElement *in = proc->bqes->Get();
  return new (in) BranchQElement(tg,map);
}

/*****************************************************************************/
/*  DeleteBranchQElement: Returns a branch queue element pointer back to the */
/*                      : branch queue element pool                          */
/*****************************************************************************/

inline void DeleteBranchQElement(BranchQElement *bqe,state *proc)
{
  bqe->BranchQElement::~BranchQElement();
  proc->bqes->Putback(bqe);
}

/*****************************************************************************/
/* NewMapTable  : return a pointer to a new MapTable element                 */
/*****************************************************************************/

inline MapTable *NewMapTable(state *proc)
{
  MapTable *in = proc->mappers->Get();
  return new (in) MapTable;
}

/*****************************************************************************/
/* DeleteMapTable : return a MapTable pointer back to the processor's pool   */
/*                : of MapTable elements                                     */
/*****************************************************************************/

inline void DeleteMapTable(MapTable *map, state *proc)
{
  map->MapTable::~MapTable();
  proc->mappers->Putback(map);
}

/*****************************************************************************/
/* NewMiniStallQElt : return a pointer to a new MiniStallQ element           */
/*****************************************************************************/

inline MiniStallQElt *NewMiniStallQElt(instance *i,int b,state *proc)
{
  MiniStallQElt *in = proc->ministallqs->Get();
  return new (in) MiniStallQElt(i,b);
}

/*****************************************************************************/
/* DeleteMiniStallQElt : return a MiniStallQ element back to the processor's */
/*                     : pool of MiniStallQ elements                         */
/*****************************************************************************/

inline void DeleteMiniStallQElt(MiniStallQElt *msqe, state *proc)
{
  msqe->MiniStallQElt::~MiniStallQElt();
  proc->ministallqs->Putback(msqe);
}

/*****************************************************************************/
/* Newstallqueueelement : return a pointer to a new stall queue element      */
/*****************************************************************************/

inline stallqueueelement *Newstallqueueelement(instance *inst,state *proc)
{
  stallqueueelement *in=proc->stallqs->Get();
  in = new (in) stallqueueelement(inst);
  if (in->inuse)
    {
      fprintf(simerr,"Re-allocating an inuse stallq element!!!\n");
      exit(-1);
    }
  in->inuse=1;
  return in;
}

/*****************************************************************************/
/* Deletestallqueueelement : return a stall queue element back to the        */
/*                         : processor's stall queue elements pool           */
/*****************************************************************************/

inline void Deletestallqueueelement(stallqueueelement *sqe,state *proc)
{
  if (!sqe->inuse)
    {
      fprintf(simerr,"De-allocating an already free stallq element!!!\n");
      exit(-1); 
   }
  sqe->inuse=0;
  sqe->stallqueueelement::~stallqueueelement();
  proc->stallqs->Putback(sqe);
}

/*****************************************************************************/
/* Newactivelistelement : return a pointer to the an new active list element */
/*****************************************************************************/

inline activelistelement *Newactivelistelement(int t, int lr, int pr,REGTYPE type,state *proc)
{
  activelistelement *in=proc->actives->Get();
  return new (in) activelistelement(t,lr,pr,type);
}

/*****************************************************************************/
/* Deleteactivelistelement : return an active list element back to the       */
/*                         : processor's active list elements pool           */
/*****************************************************************************/

inline void Deleteactivelistelement(activelistelement *ale,state *proc)
{
  ale->activelistelement::~activelistelement();
  proc->actives->Putback(ale);
}

/*****************************************************************************/
/* NewTagtoInst : return a pointer to a new TagtoInst element                */
/*****************************************************************************/

inline TagtoInst *NewTagtoInst(int tg, instance *inst, state *proc)
{
  TagtoInst *in = proc->tagcvts->Get();
  return new (in) TagtoInst(tg,inst);
}

/*****************************************************************************/
/* DeleteTagtoInst : return a TagToInst pointer back to the processor's      */
/*                 : TagtoInst pool                                          */
/*****************************************************************************/

inline void DeleteTagtoInst(TagtoInst *tcvt,state *proc)
{
  tcvt->TagtoInst::~TagtoInst();
  proc->tagcvts->Putback(tcvt);
}

#endif
