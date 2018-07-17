/*****************************************************************************
  
  Processor/memq.h
  
  Definition and implementation of the MemQ class. The MemQ class defines
  a specialized linked list used to implement the memory queues in the
  processor
  
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


#ifndef _memq_h_
#define _memq_h_ 1

#include <stddef.h>

/*************************************************************************/
/***************** Individual elements of MemQ ***************************/
/*************************************************************************/

template <class Data> struct MemQLink
{
    Data d;
    MemQLink<Data> *next, *prev;
    MemQLink(Data x):d(x) {next=prev=NULL;}
};


/*************************************************************************/
/***************** MemQ class definition *********************************/
/*************************************************************************/

template <class Data> class MemQ
{
private:
    MemQLink<Data> *head;
    MemQLink<Data> *tail;
    int items;
public:
    MemQ(): head(NULL),tail(NULL),items(0) {}	/* constructor */
  void restart() {				/* destructor  */
	MemQLink<Data> *old;
	while (head != NULL) {old=head; head=head->next; delete old;}
	head=tail=NULL; items=0;}
    void Insert(Data d);			/* insert element */
    int Lookup(Data d);				/* search for element */
    int Remove(Data d);				/* delete element */
    void Remove(MemQLink<Data> *stepper);	/* delete element: type 2 */
    int RemoveGetPrev(Data d, Data &d2);	/* delete element: type 3 */
    int RemoveGetNext(Data d, Data &d2);	/* delete element: type 4 */
    int Replace(Data d,Data d2);		/* replace element */
    void Replace(MemQLink<Data> *stepper,Data d2); /* repl element: type 2 */
    int GetMin(Data &d)				/* get head of memQ */
	{if (!items) return 0; d=head->d;return 1;}
    int GetTail(Data &d)			/* get tail of memQ */
	{if (!items) return 0; d=tail->d;return 1;}
    void RemoveTail();				/* delete tail */
    MemQLink<Data> *GetNext(MemQLink<Data> *a)	/* get next element */
	{if (a==NULL)return head;else return a->next;}
    MemQLink<Data> *GetPrev(MemQLink<Data> *a)	/* get previous element */
	{if (a==NULL)return tail;else return a->prev;}
    int NumItems() const {return items;}	/* get number of items */
};

/*************************************************************************/
/***************** MemQ class implementation *****************************/
/*************************************************************************/

/********************* Insert operation *********************************/

template <class Data> inline void MemQ<Data>::Insert(Data d)
{
  items++;
  MemQLink<Data> *item = new MemQLink<Data>(d);
  if (tail == NULL)	{
    head=tail=item;
  }
  else{
    tail->next=item;
    item->prev=tail;
    tail=item;
  }
}

/********************* Lookup operation *********************************/

template <class Data> inline int MemQ<Data>::Lookup(Data d)
{
  MemQLink<Data> *stepper = head;
  while (stepper != NULL)	{
    if (stepper->d == d)
      return 1;
    stepper=stepper->next;
  }
  return 0;
}

/************************* Replace operations ****************************/

template <class Data> inline void MemQ<Data>::Replace(MemQLink<Data> *stepper,Data d2)
{
  stepper->d=d2;
}


template <class Data> inline int MemQ<Data>::Replace(Data d,Data d2)
{
  MemQLink<Data> *stepper = head;
  while (stepper != NULL)   {
    if (stepper->d == d)     {
      stepper->d = d2;
      return 1;
    }
    stepper=stepper->next;
  }
  return 0;
}

/**************************** Delete operations ***************************/

template <class Data> inline void MemQ<Data>::Remove(MemQLink<Data> *stepper)
{
  items--;
  if (stepper == tail)   {
    if (stepper == head)
      head=tail=NULL;
    else   {
      tail = stepper->prev;
      tail->next = NULL;
    }
  }
  else if (stepper == head)   {
    head = stepper->next;
    head->prev = NULL;
  }  else  {
    stepper->next->prev = stepper->prev;
    stepper->prev->next = stepper->next;
  }
  delete stepper;
}

template <class Data> inline int MemQ<Data>::Remove(Data d)
{
  MemQLink<Data> *stepper = head;
  
  while (stepper != NULL){
    if (stepper->d == d){
      Remove(stepper);
      return 1;
    }
    stepper=stepper->next;
  }
  return 0;
}

template <class Data> inline int MemQ<Data>::RemoveGetPrev(Data d, Data& d2)
{
  MemQLink<Data> *prev,*stepper = head;
  
  while (stepper != NULL){
    if (stepper->d == d){
      prev=stepper->prev;
      Remove(stepper);
      if (prev != NULL)
	d2=prev->d;
      return 1;
    }
    stepper=stepper->next;
  }
  return 0;
}

template <class Data> inline int MemQ<Data>::RemoveGetNext(Data d, Data& d2)
{
  MemQLink<Data> *next,*stepper = head;
  
  while (stepper != NULL){
    if (stepper->d == d){
      next=stepper->next;
      Remove(stepper);
      if (next != NULL)
	d2=next->d;
      return 1;
    }
    stepper=stepper->next;
  }
  return 0;
}

template <class Data> inline void MemQ<Data>::RemoveTail()
{
  if (tail){
    items--;
    
    if (tail == head){
      delete tail;
      head=tail=NULL;
    } else{
      register MemQLink<Data> *stepper = tail;
      tail = tail->prev;
      tail->next = NULL;
      delete stepper;
    }
  }
}

#endif
