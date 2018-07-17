/*
   Processor/capconf.cc

   Used for distinguishing between capacity and conflict misses.

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



#include "Processor/capconf.h"
#include "Processor/simio.h"
extern "C"
{
#include "MemSys/simsys.h"
}
#include <values.h>
#include <stdio.h>
#include <stdlib.h>

static unsigned line_map1(unsigned,unsigned);
static unsigned line_map2(unsigned,unsigned);


/*************************************************************************/
/* CapConfDetector: the division between conflict and capacity misses in */
/* an ILP processor is difficult to state, as speculation can alter the  */
/* pattern of memory references. As a result, it is no longer sufficient */
/* to compare miss count to that seen with a fully-associative cache. We */
/* distinguish between these two types of misses (approximately) using a */
/* structure called the "CapConfDetector". The detector consists of a    */
/* hash table combined with a fixed-size circular queue, both of which   */
/* start empty                                                           */
/*************************************************************************/

CapConfDetector::CapConfDetector(int linect) :
  lines_hash(line_map1,line_map2),
  lines_circq(linect),
  lines(linect)
{
#ifdef DEBUG_CCD
  fprintf(simout,"New Capacity Conflict detector started -- lines %d\n",lines);
#endif
}

extern "C" struct CapConfDetector *NewCapConfDetector(int lines)
{
  return new CapConfDetector(lines);
}

/*****************************************************************************/
/* CCD_InsertNewCacheLine: Conceptually, new lines brought into the cache    */
/* are put into the circular queue, which has a size equal to the number     */
/* of cache lines. When the circular queue has filled up, new insertions     */
/* replace the oldest element in the queue. However, before inserting a      */
/* new line into the detector, the detector must first be checked to make    */
/* sure that the line is not already in the queue. If it is, then a line     */
/* has been brought back into the cache after being replaced in less time    */
/* than its taken to refill the entire cache; consequently, it is a          */
/* conflict miss. We consider a miss a capacity miss if it isn't already     */
/* in the queue when it is brought in, as this indicates that at least as    */
/* many lines as are in the cache have been brought into the cache since     */
/* the last time this line was present. The hash table is used to provide    */
/* a fast check of the entries available; the entries in the hash table      */
/* are always the same as those in the circular queue.                       */
/*****************************************************************************/


extern "C" CacheMissType CCD_InsertNewCacheLine(CapConfDetector *ccd, int tag)
{
  unsigned tmp;
  if (ccd->lines_hash.lookup(tag,tmp))
    {
      /* it's in there! */
      return CACHE_MISS_CONF;
    }
  else
    {
      /* the cache has been swamped since the last time this was in there */
      if (ccd->lines_circq.NumInQueue() == ccd->lines)
	{
	  ccd->lines_circq.Delete(tmp); /* this tells the line we're kicking out by cap */
	  if (ccd->lines_hash.remove(tmp) != 1)
	    {
	      fprintf(simerr,"Removing line %d from CCD that isn't there!!!\n",tmp);
	      exit(1);
	    }
	}
      if (ccd->lines_hash.insert(tag,tag) != 1 || ccd->lines_circq.Insert(tag) != 1)
	{
	  fprintf(simerr,"Adding line %d from CCD fails at time %.1f!!!\n",tag,YS__Simtime);
	  exit(1);
	}
      return CACHE_MISS_CAP;
    }
}

/************************************************************************/
/* Hash functions used for the hash table                               */
/************************************************************************/


static unsigned line_map1(unsigned k, unsigned sz)
{
  return k&(sz-1);
}

static unsigned line_map2(unsigned k,unsigned sz)
{
  return (((k<< 11) | (k >> (BITS(unsigned)-11))) & (sz-1))*2+1;
}
