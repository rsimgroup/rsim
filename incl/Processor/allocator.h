/*****************************************************************************

  allocator.h
   
   Definitions of the Allocator and Stack classes. The Allocator is just
   a high powered stack. Used for instances, stallq, active list elements,
   shadow mappers, checkpoints, etc. The Stack is used for free list, etc.
   
   ***************************************************************************/
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


#ifndef _allocator_h_
#define _allocator_h_ 1

#include <memory.h>
#include <malloc.h>
#include <string.h>

/*************************************************************************/
/******************  Allocator  class routines ***************************/
/*************************************************************************/

template <class Data> class Allocator /* high power stack, limited size */
{
private:
  Data **arr;
  Data **original_newed_objects; // this gets copied into arr at reset time
  int sz;
  int tail;
  void (*reset_func)(Data *);
public:
  Allocator(int s, void (*rf)(Data *) = NULL);
  /* new the arrays, then new the original objects, then call reset */
  ~Allocator(); /* delete the newed objects,delete the arrays */
  /* NOTE: NEW THE ABOVE BY MALLOC ONLY */

  /* put back into stack */
  int Putback(Data *d){if (tail == sz) return 0; arr[tail++] = d; return 1;}

  /* get from stack */
  Data *Get() {if (tail == 0) return 0; return arr[--tail];}

  /* number of elements in stack */
  int Elts() const {return tail;}

  /* reset stack */
  void reset();// {tail=sz;memcpy(arr,original_newed_objects,sizeof(Data *)*sz);}
};

/**************************************************************************/
/******** Implementation of member functions of allocator class ***********/
/**************************************************************************/

/******************** Allocator class constructor *************************/
#ifndef __GNUC__
template <class Data> inline Allocator<Data>::Allocator(int s,void (*rf)(Data *))
#else
template <class Data> inline Allocator<Data>::Allocator(int s,void (*rf)(Data *) = NULL)
#endif
{
  typedef Data *dp;
  arr = new dp[s];
  original_newed_objects = new dp[s];
  sz=s;
  for (int i=0; i<s; i++)
    {
      original_newed_objects[i] = (Data *)malloc(sizeof(Data));
      memset((char *)original_newed_objects[i],0,sizeof(Data));
    }
  reset_func=rf;
  reset();
}

/********************* Allocator class reset *****************************/
template <class Data> inline void Allocator<Data>::reset()
{
  tail=sz;
  memcpy(arr,original_newed_objects,sizeof(Data *)*sz);
  if (reset_func)
    {
      for (int i=0; i<sz;i++)
	(*reset_func)(arr[i]);
    }
}

/********************* Allocator class destructor ************************/
template <class Data> inline Allocator<Data>::~Allocator()
{
  for (int i=0; i<sz; i++)
    free(original_newed_objects[i]);
  delete arr;
  delete original_newed_objects;
}


/*************************************************************************/
/* ****************   Stack class routines *******************************/
/*************************************************************************/

template <class Data> class Stack // high powered Stack, use it to implement freelist
{
private:
  Data *arr;
  Data *origmap;
  unsigned origused;
  unsigned sz;
  unsigned tail;
public:
  Stack(Data *omap, int used, int s); /* new the arrays, then new the original
					 objects, then call reset */
  ~Stack() {delete[] arr;delete[] origmap;} /* delete the newed objects,delete the arrays */
  
  /* NOTE: NEW THE ABOVE BY MALLOC ONLY */

  /* push data into stack */
  int Push(Data d){if (tail == sz) return 0; arr[tail++] = d; return 1;}

  /* pop data from stack */
  int Pop(Data& d) {if (tail == 0) return 0; d = arr[--tail];return 1;}

  /* number of elements in stack */
  int used() const {return tail;}

  /* Is stack full?? */
  int full() const {return tail==sz;}

  /* reset stack */
  void reset() {tail=origused;memcpy(arr,origmap,origused*sizeof(Data));}
};

/*************************************************************************/
/*** Implementation of member functions of stack class *******************/
/*************************************************************************/

template <class Data> inline Stack<Data>::Stack(Data *omap,int used,int s):origmap(omap),origused(used),sz(s)
{
  arr = new Data[sz];
  reset();
}

#endif
