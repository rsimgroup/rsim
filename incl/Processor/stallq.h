/*****************************************************************************

  stallq.h :

  Contains the definition and implementation of the stallqueue and the
  MiniStallQ classes (and also the MiniStallQElt and stallqueueelement
  classes).

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



#ifndef _stallq_h_
#define _stallq_h_ 1

#include <stddef.h>

struct state;
class stallqueueelement;
class instance;

extern int FlushStallQ(int, state *);


/*************************************************************************/
/***************** MiniStallQElt class definition ************************/
/*************************************************************************/

class MiniStallQElt
{
public:
  instance *inst;
  int tag;
  int CLEAR_MASK;
  MiniStallQElt *next;
  
  MiniStallQElt(instance *i,int b);		/* constructor */
  int ClearBusy(state *proc);	 /* does clear busy just for one, used in
				    reg queues; does flushing, returns 1 on
				    success */
  
  MiniStallQElt *GetNext(instance *& i, state *); /* this is used for resource qs */

  /* here's one optimization; once the tags fail to match, you can clear out the
     rest of the q up to the next match! */

};

/*************************************************************************/
/******************** MiniStallQ class definition ************************/
/*************************************************************************/

class MiniStallQ
{
private:
  MiniStallQElt *head;
  MiniStallQElt *tail;
public:
  MiniStallQ(): head(NULL),tail(NULL) {}
  void AddElt(instance *, state *, int b=~0);	/* add element */
  void ClearAll(state *proc); 			/* used only for reg queues */
  instance *GetNext(state *);			/* get next instance */
};

/*************************************************************************/
/********************** stallqueue class definition **********************/
/*************************************************************************/

class stallqueue{
public:
    stallqueueelement *get_next_entry(stallqueueelement *curr_entry);
    stallqueueelement *get_tail() const {return endptr;}
    /* If curr_entry = NULL, returns first entry
       returns NULL if curr_entry is lastelement */
  
    void append(stallqueueelement *newentry);   /* Add entry to end of queue*/
  
    void delete_entry(stallqueueelement *entry,state *proc);
                                                  /* delete currentelement */
  
    stallqueue();		                            /* constructor */
    ~stallqueue();                                          /* destructor  */
    
private:
    stallqueueelement *startptr;
    stallqueueelement *endptr;
};

/*************************************************************************/
/***************** stallqueueelement class definition ********************/
/*************************************************************************/

class stallqueueelement{
public:
  instance *inst;
  stallqueueelement *next;
  stallqueueelement *prev;
  int inuse;
    
  stallqueueelement(instance *instce=NULL):inst(instce),next(NULL),prev(NULL){}
  ~stallqueueelement() {}
};



#endif
