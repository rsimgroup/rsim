/*
  active.h
  
  Contains definitions for active list class and its elements
   
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


#ifndef _active_h_
#define _active_h_ 1

#include "circq.h"
#include "regtype.h"
#include <stddef.h>
struct state;
struct instance;

extern int FlushActiveList(int, state *);

/*************************************************************************/
/* ***************** activelistelement class definition ******************/
/*************************************************************************/

class activelistelement{
public:
    int tag;		                              /* instruction tag */
    int logicalreg;     /* stores the old mapping from the logical register
			   to the physical register; this is because the
			   logical register is now renamed  */
    int phyreg;		             /* physical register after renaming */
    REGTYPE regtype;                            	/* register type */
    int done;		                         /* is instruction done? */
    int cycledone;           /* The cycle when the instruction completed */
    int exception;	               /* exception status of intruction */
    activelistelement(int t, int lr, int pr,REGTYPE type, int dn =
		      0, int exc = 0);	                  /* constructor */
    ~activelistelement() {}			          /* destructor  */
};

/*************************************************************************/
/* ********************* activelist class definition ******************* */
/*************************************************************************/

class activelist{
  /* The active list is implemented as a circular queue of active list
     entries. Note that there are two entries associated with each
     instruction -- one for the dest. register and one for the dest.
     conditon code register */
private:
  circq<activelistelement *> *q;
  int mx;				/* max elements in active list */

  /* mark an exception for an active list entry */
  int flag_exception_in_active_list(activelistelement *, activelistelement *, int exception);

  /* mark an active list entry as "done" */
  int mark_done_in_active_list(activelistelement *, activelistelement *, int exception, int cycle);
public:
  activelist(int maxelements);		/* constructor */
  ~activelist(){};			/* destructor  */
  
  int full() const			/* is active list full */
	{return (q->NumInQueue() == mx);}
  
  /* Add to the active list
	- return 0 on success, 1 if active list is full */
  int add_to_active_list(int tagnum, int oldlogical, int
			 oldphysical, REGTYPE regtype,state *proc);
  
  /* Mark tagged elements as done and update their exception values,
     return 0 on success, 1 if element not found */
  int mark_done_in_active_list(int tagnum, int exception, int cycle);

  /* Mark stores in the active list ready */
  void mark_stores_ready(int cycle,state *proc);

  /* set an exception (SOFT exception type) even after the instance has completed */
  int flag_exception_in_active_list(int tagnum, int exception);
  
  /* Remove tagged elements from the active list if exception status
     is OK. return 0 on success, else return tag causing problems */
  /* -1 if the active list is empty, -2 if there are no removable
     entries this cycle */
  
  instance *remove_from_active_list(int cycle, state *proc);

  /* Remove active list element */
  int remove_element(activelistelement *ptr);

  /* Flush all active list entries */
  int flush_active_list(int tag, state *proc);

  /* Return the number of elements in active list */
  int NumElements() const {return q->NumInQueue()/2;}

 /* return entries in active list */
  int NumEntries() const {return q->NumInQueue();}

  /* Return number of available instruction slots in window */
  int NumAvail() const {return (mx-q->NumInQueue())/2;}
};



#endif
