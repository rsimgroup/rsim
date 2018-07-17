/*
   instheap.cc

   A specialized heap implementation for instances.

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



#include "Processor/instheap.h"
#include <memory.h>

int InstHeap::minheapsz = 8; /* must be a power of 2 */

inline void InstHeap::resize(int sz) /* follows corresponding from heap.h */
{
  instance **obj2 = new instptr[sz];
  memcpy(obj2,obj,sz*sizeof(instptr));
  
  int *tags2 = new int[sz];
  memcpy(tags2,tags,sz*sizeof(int));

  int *ord2=new int[sz];
  memcpy(ord2,ord,sz*sizeof(int));

  delete[] obj;
  delete[] tags;
  delete[] ord;
  
  size=sz;
  obj=obj2;
  tags=tags2;
  ord=ord2;
}

/*****************************************************************************/
/* The InstHeap is similar to the ordinary heap of heap.h, with a            */
/* slightly different definition of "less".  Just like any other heap, an    */
/* inst heap also requires parent to be less than both children.             */
/*                                                                           */
/* However, "less" here is defined as having a lower timestamp or            */
/* having an equal timestamp and a lesser tag (only the former is            */
/* considered in ordinary heaps)                                             */
/*****************************************************************************/


/* follows corresponding from heap.h */
int InstHeap::insert(int ts, instance *o, int tag) 
{
  if (used >= size)
    {
      resize(2*size);
    }
  
  instance *tmpT;
  int tmp;
      
  obj[used]=o;
  tags[used]=tag;
  ord[used]=ts;

  int step = used;
  int par=parent(step);
  used++;
  while (step >0 && (ts < ord[par] || (ts == ord[par] && tag<tags[par])))
    {
      tmp=ord[par];
      ord[par]=ts;
      ord[step]=tmp;

      tmp=tags[par];
      tags[par]=tag;
      tags[step]=tmp;

      tmpT=obj[par];
      obj[par]=o;
      obj[step]=tmpT;

      step=par;
      par=parent(par);
    }

  return 1;

}

/* follows corresponding from heap.h */
int InstHeap::GetMin(instance *& min, int &tag)
{
  if (used == 0)
    return -1;
  
  min=obj[0];
  tag=tags[0];
  int ans=ord[0];
  --used;
  if (used > 0)
    {
      instance *prop;
      int ptag;
      prop=obj[0]=obj[used];
      ptag=tags[0]=tags[used];
      int ts=ord[0]=ord[used];
      
      int posn=0;
      int l,r;
      
      while (1)
	{
	  l=lchild(posn);
	  r=rchild(posn);
	  if (l >= used) // means that we are at a leaf
	    break;
	  if (r >= used) // no right child
	    {
	      ord[r] = ord[posn] ; // so we can handle it all together
	      obj[r] = obj[posn] ; // so we can handle it all together
	      tags[r] = tags[posn];
	    }
	  
	  int lts = ord[l], rts = ord[r];
	  int ltag = tags[l], rtag = tags[r];

	  int lcomp = (lts < ts || (lts==ts && ltag<ptag)),
	    rcomp = (rts < ts || (rts==ts && rtag<ptag)),
	    lrcomp = (lts < rts || (lts==rts&&ltag<rtag));
	  
	  int done = 0;
	  switch (lcomp * 4 + rcomp * 2 + lrcomp)
	    {
	    case 0: // parent is least
	    case 1:
	      done = 1;
	      break;
	    case 2: // right is least, left greatest
	    case 3:
	    case 6: // right least, parent greatest
	      ord[posn] = rts;
	      obj[posn] = obj[r];
	      tags[posn] = rtag;
	      ord[r] = ts;
	      obj[r] = prop;
	      tags[r] = ptag;
	      posn = r;
	      break;
	    case 4: // left is least, right greatest
	    case 5:
	    case 7: // left the lowest, parent greatest
	      ord[posn] = lts;
	      obj[posn] = obj[l];
	      tags[posn] = ltag;
	      ord[l] = ts;
	      obj[l] = prop;
	      tags[l] = ptag;
	      posn = l;
	      break;
	    default: // can't happen
	      break;
	    }
	  
	  if (done)
	    break;
	}
      
    }
  return ans;
}

