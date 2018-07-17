/*
  shmalloc.c

  A simple shared-memory allocator for the simulated applications.
  
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



#include <stdlib.h>
#include "Processor/state.h"
#include "Processor/simio.h"
extern "C"
{
#include "MemSys/misc.h"
}

/****************************************************************************/
/* our_sh_malloc: this is a simple allocator -- all it does is bump up      */
/* highsharedused and make sure that all new pages are in the               */
/* SharedPageTable. This is so simple because we don't currently supply a   */
/* "free" function                                                          */
/*                                                                          */
/* For the user's convenience, this returns blocks aligned to cache lines.  */
/****************************************************************************/
 
void *our_sh_malloc(int size)
{
  if (size <= 0)
    return NULL;
  unsigned oldmax = highsharedused;
  void *res = (void *)highsharedused;
  highsharedused += size;
  if (highsharedused % blocksize) /* not aligned */
    highsharedused += blocksize - (highsharedused % blocksize);

  /* now make sure all pages are in page table */
  unsigned startpg = oldmax / ALLOC_SIZE + (oldmax % ALLOC_SIZE != 0);
  unsigned endpg = highsharedused/ALLOC_SIZE + (highsharedused % ALLOC_SIZE != 0);
  if (startpg < endpg)
    {
      /* add pages as needed */
      unsigned chunkptr = (unsigned)malloc((endpg - startpg) * ALLOC_SIZE);
      for (unsigned pg=startpg; pg < endpg; pg++, chunkptr += ALLOC_SIZE)
	{
	  SharedPageTable->insert(pg,chunkptr);
	}
    }
  else if (startpg > endpg)
    {
      fprintf(simerr,"error in shmalloc!");
      exit(-1);
    }

  return res;
}
