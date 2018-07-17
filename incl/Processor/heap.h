/******************************************************************************

  heap.h

  This file contains the definition and implementation of the heap class.

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


#ifndef _heap_h_
#define _heap_h_ 1

/*****************************************************************************/
/************************** Heap class definition ****************************/
/*****************************************************************************/

template<class T> class Heap
{
private:
  T *obj;
  int *ord;
  int size;
  int used;
  static int lchild(int node) {return 2*node+1;}
  static int rchild(int node) {return 2*node+2;}
  static int parent(int node) {return (node-1)/2;}
public:
  Heap(int sz):size(sz),used(0) {obj = new T[sz];ord=new int[sz];}
  int insert(int ts, T o);             /* Insert element o with timestamp ts */
  int num() const {return used;}        /* return number of elements in heap */
  int PeekMin() const {return ord[0];}               /* return min timestamp */
  int GetMin(T& min);                       /* return min object & timestamp */
};

/*****************************************************************************/
/******************** Heap class member function definitions *****************/
/*****************************************************************************/

template<class T> inline int Heap<T>::insert(int ts, T o)
{
  if (used >= size) {
    return 0;
  } else  {
    T tmpT;
    int tmp;
    
    obj[used]=o;
    ord[used]=ts;
    
    int step = used;
    int par=parent(step);
    used++;
    while (step >0 && (ts < ord[par])){
      tmp=ord[par];
      ord[par]=ts;
      ord[step]=tmp;
      
      tmpT=obj[par];
      obj[par]=o;
      obj[step]=tmpT;
      
      step=par;
      par=parent(par);
    }
    return 1;
  }
}

template<class T> inline int Heap<T>::GetMin(T& min)
{
  if (used == 0)
    return -1;
  
  min=obj[0];
  int ans=ord[0];
  --used;
  if (used > 0)
    {
      T prop;
      prop=obj[0]=obj[used];
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
	    }
	  
	  int lts = ord[l], rts = ord[r];

	  int lcomp = (lts < ts), rcomp = (rts < ts), lrcomp = (lts < rts);
	  
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
	      ord[r] = ts;
	      obj[r] = prop;
	      posn = r;
	      break;
	    case 4: // left is least, right greatest
	    case 5:
	    case 7: // left the lowest, parent greatest
	      ord[posn] = lts;
	      obj[posn] = obj[l];
	      ord[l] = ts;
	      obj[l] = prop;
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

#endif
