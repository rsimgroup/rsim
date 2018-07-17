/******************************************************************************
  circq.h

  Contains definition and implementation of a "high-power"
  circular queue (FIFO) structure 

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


// C++ templates for general purpose
// queue implementation; provides operations
// to insert at either end or at an
// arbitrary sorted point, as well as to
// dequeue from the front

#ifndef _circq_h_
#define _circq_h_ 1

#include <stddef.h>
#include "normalize.h"

/*************************************************************************/
/*********************** circq class definition **************************/
/*************************************************************************/

template <class Data> class circq /* high power FIFO, limited size */
{
private:
  Data *arr;               /* data * */
  unsigned sz;             /* max size * */
  unsigned mask;           /* mask = size-1 * */
  unsigned head;           /* head of FIFO * */
  unsigned tail;           /* tail of FIFO * */
  int cnt;                 /* Number of elements in FIFO */
  int max; /* max # of elements actually in FIFO;
	      may differ from circq sz if non-power of 2 */
public:
  circq() {} /* This is used only as default constructor for array cases,
		and should be immediately followed by start(int) call */
  circq(int s) {start(s);} // : sz(s),mask(s-1),head(0),tail(0),cnt(0) {arr=new Data[sz];}
                                           /* Constructor * */
  void start(int s) {max=s;sz=normalize(s);mask=sz-1;head=tail=cnt=0; arr=new Data[sz];}
  /* NOTE: sz must get a power of 2; otherwise, this will not work. */
  ~circq() {delete[] arr;}                 /* Destructor * */
  int Insert(Data d)                       /* Insert data at tail * */
    {if (cnt == max) return 0; ++cnt;arr[tail++]=d; tail &= mask; return 1;}
  int InsertAtHead(Data d)                 /* Insert at head * */
    {if (cnt == max) return 0; ++cnt;--head;head &= mask; arr[head]=d; return 1;}
  int Delete(Data& d)                      /* Delete data from head * */
    {if (cnt == 0) return 0; --cnt;d=arr[head];++head;head &= mask;return 1;}
  int DeleteFromTail(Data &d)              /* Delete data from tail * */
    {if (cnt == 0) return 0; --cnt;--tail;tail &= mask;d=arr[tail]; return 1;}
  int PeekHead(Data& d)                    /* Return head element in d * */
    {if (cnt == 0) return 0; d=arr[head]; return 1;}
  int PeekTail(Data& d)                    /* Return tail element in d */
    {if (cnt == 0) return 0; d=arr[(tail-1) & mask]; return 1;}
  int PeekElt(Data &d, int ind)            /* Return element ind */
    {if (cnt <= ind) return 0; d=arr[(head+ind) & mask]; return 1;} 
  int SetElt(Data d, int ind)              /* Modify element d */ 
    {if (cnt <= ind) return 0; arr[(head+ind) & mask]=d; return 1;} 
  int NumInQueue() const                   /* Return number of elements */
    {return cnt;}
  int Search(const Data& key, Data& out, int (*diff)(const Data&, const Data&)) const;
  int Search(int key, Data& out, int& posn, int (*diff)(const Data&, int)) const;
  int Search2(const Data& key, Data& out1, Data& out2, int (*diff)(const Data&, const Data&)) const;
  void reset() {cnt=head=tail=0;}
};


/*************************************************************************/
/*********************** circq class implementation **********************/
/*************************************************************************/

template <class Data> inline int circq<Data>::Search(const Data& key, Data& out, int (*diff)(const Data&, const Data &)) const
     // diff returns + if a>b, 0 if a==b, - if a<b
{
  int lo,lbnd;
  int hi,hbnd;
  lo=lbnd=head;
  hi=hbnd=tail-1;
  if (hi < lo)    {
    hi += sz;
    hbnd += sz;
  }
  int res;
  int mid;
  while (lo <= hi)    {
    mid = (lo+hi)/2;
    res = (*diff)(arr[mid&mask],key);
    if (res == 0)	{
      out=arr[mid&mask];
      return 1;
    }
    if (res < 0)	{
      lo=mid+1;
    }
    else	{
      hi=mid-1;
    }
  }
  return 0;
}

template <class Data> inline int circq<Data>::Search(int key, Data& out,int& posn, int (*diff)(const Data&, int)) const
     // diff returns + if a>b, 0 if a==b, - if a<b
{
  int lo,lbnd;
  int hi,hbnd;
  lo=lbnd=head;
  hi=hbnd=tail-1;
  if (hi < lo)    {
    hi += sz;
    hbnd += sz;
  }
  int res;
  int mid;
  while (lo <= hi)    {
    mid = (lo+hi)/2;
    res = (*diff)(arr[mid&mask],key);
    if (res == 0)	{
      out=arr[mid&mask];
      posn=mid-lbnd;
      return 1;
    }
    if (res < 0)      {
      lo=mid+1;
    }
    else	{
      hi=mid-1;
    }
  }
  return 0;
}

template <class Data> inline int circq<Data>::Search2(const Data& key, Data& out1, Data& out2, int (*diff)(const Data&, const Data &)) const// diff returns + if a>b, 0 if a==b, - if a<b
{
  int lo,lbnd;
  int hi,hbnd;
  lo=lbnd=head;
  hi=hbnd=tail-1;
  if (hi < lo)    {
    hi += sz;
    hbnd += sz;
  }
  int res;
  int mid;
  while (lo <= hi)    {
    mid = (lo+hi)/2;
    res = (*diff)(arr[mid&mask],key);
    if (res == 0)	{
      out1=arr[mid&mask];
      int t=mid-1;
      if (t >= lbnd && t <= hbnd)	    {
	Data o2 = arr[t&mask];
	if ((*diff)(o2,key) == 0)	  {
	  out2=o2;
	  return 1;
	}
      }
      t=mid+1;
      if (t >= lbnd && t <= hbnd)   {
	Data o2 = arr[t&mask];
	if ((*diff)(o2,key) == 0)  {
	  out2=o2;
	  return 1;
	}
      }
      return 0;
    }
    if (res < 0){
      lo=mid+1;
    }
    else{
      hi=mid-1;
    }
  }
  return 0;
}

#endif
