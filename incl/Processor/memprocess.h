/****************************************************************************/
/*   memprocess.h :  The memory system interface as seen at the processor   */
/****************************************************************************/
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


#ifndef _memprocess_h_
#define _memprocess_h_ 1

#include "MemSys/miss_type.h"

#define PROC_TO_MEMSYS (int)(lowsimmed)
extern unsigned lowsimmed;
/* the minimum address simmed by MemSim: 0x80000000 if only shared; 0 if all */

#ifdef _LANGUAGE_C_PLUS_PLUS
#ifndef __cplusplus
#define __cplusplus
#endif
#endif

#ifdef __cplusplus
/* Detailed documentation on these functions can be found in the .c files */
struct instance;
struct state;
extern int StartUpMemRef(state *,instance *);
extern void MyAssociate(instance *,state *);
extern void MyAssociateCohe(instance *,state *);
extern int IssueProbe(state *,unsigned);
extern int IssuePrefetch(state *,unsigned, int level, int excl, int inst_tag=0);

extern "C"
{
#endif
  struct YS__Req;
  void MemRefProcess();
  void MemDoneHeapInsert(struct YS__Req *req, enum MISS_TYPE);
  void MemOpReject(struct YS__Req *req);
  void GlobalPerform(struct YS__Req *req);
  void UnsetGlobalPerform(struct YS__Req *req);
  int GetFlagReadVal(struct YS__Req *req);
  int GetFlagWriteVal(struct YS__Req *req);
  enum SLBFailType {SLB_Repl, SLB_Cohe};
  int SpecLoadBufCohe(int,int, enum SLBFailType);
  void SetL2ArgPtr(struct state *, struct YS__arg *);
  void SetL1ArgPtr(struct state *, struct YS__arg *);
  void SetWBArgPtr(struct state *, struct YS__arg *);
  struct YS__arg *GetL2ArgPtr(struct state *);
  struct YS__arg *GetL1ArgPtr(struct state *);
  struct YS__arg *GetWBArgPtr(struct state *);
  void FreeAMemUnit(struct state *, int);
  void AckWriteToWBUF(struct instance *, struct state *);
#ifdef __cplusplus
}
#endif

extern struct state *MemPProcs[];
extern int Speculative_Loads;         /* enable speculative loads */
extern int captr_block_bits;          /* No. of block address bits at cache */

#define GetFlagVal GetFlagReadVal

#endif

