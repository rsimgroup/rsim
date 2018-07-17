/*
  freelist.cc

  Code for member functions for the freelist class

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


#include "Processor/freelist.h"
#include "Processor/processor_dbg.h"

/*************************************************************************/
/* The freelist maintains a stack of free registers                      */
/*************************************************************************/


int freelist::addfreereg(int regnum) /* adds a register back to list */
{
  int res = regs->Push(regnum);
  if (res)
    {
#ifdef COREFILE
      if(GetSimTime() > DEBUG_TIME)
	fprintf(corefile, "added register %d, total now is %d\n", regnum, regs->used());
#endif
      return 0;
    }
  else
    {
#ifdef COREFILE
      if(GetSimTime() > DEBUG_TIME)
	fprintf(corefile, "Free List full\n");
#endif
#ifdef COREFILE
      if(GetSimTime() > DEBUG_TIME)
	fprintf(corefile,"<decstate.cc> Free list Full!\n");
#endif
      return (1);
    }
}

int freelist::getfreereg() /* allocates a free register for renaming */
{
  int ret,fnd;
  fnd=regs->Pop(ret);
  if (fnd)
    {
#ifdef COREFILE
      if(GetSimTime() > DEBUG_TIME)
	fprintf(corefile, "Removed register %d from free list, total now %d\n",ret,regs->used());
#endif
      return ret;
    }
  else
    {
#ifdef COREFILE
      if(GetSimTime() > DEBUG_TIME)
	fprintf(corefile, "Free list empty\n");
#endif
      return -1;
    }
}

