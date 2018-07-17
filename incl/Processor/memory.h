/*****************************************************************************
  
  memory.h

  definitions relating to the interface to the processor memory unit

  ****************************************************************************/
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


#ifndef _memory_h_
#define _memory_h_ 1

struct state;
class instance;

/* These functions are documented in more detail in the respective .c files */
extern void FlushMems(int, state *);
extern int  IssueMem(state *);
extern void CompleteMemQueue(state *);
extern int  memory_latency(instance *,state *);
extern int  memory_rep(instance *,state *);
extern void AddToMemorySystem(instance *,state *);
extern void CalculateAddress(instance *,state *);
extern void Disambiguate(instance *,state *);
extern int  NumInMemorySystem(state *);
extern void PerformMemOp(instance *,state *);

/* Memory barrier handling functions */
struct MembarInfo;                         
extern void ComputeMembarQueue(state *);
extern void ComputeMembarInfo(state *,const MembarInfo&);

extern void DoMemFunc(instance *,state *);
extern int  IsStore(instance *inst);
extern int  IsRMW(instance *inst);

extern enum ReqType mem_acctype[];       /* keep track of memory access type */
extern int  mem_length[];              /* keep track of memory access length */
extern int  mem_align[];      /* keep track of memory alignment requirements */

extern int INSTANT_ADDRESS_GENERATE;    /* skip the address generation stage */
extern int membarprime;    /* apply memory barrier only for shared variables */

#endif
