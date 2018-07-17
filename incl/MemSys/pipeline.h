/*
  pipeline.h

  This file contains the declarations relating to the implementation
  of a cache pipeline -- used in the L1 and L2 caches.

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



#ifndef _pipeline_h_
#define _pipeline_h_ 1

typedef struct YS__Pipeline { /* the pipeline data structure */
  int depth;             /* number of stages in pipeline */
  int width;             /* how many pipeline entries can be in each stage */
  struct YS__Req **pipe; /* pipeline implemented as an array of requests */
  int NumInPipe;         /* number of entries in pipe */
  int NumInLastStage;    /* number of entries in input stage */
  int WhereToAdd;        /* where in input stage to add next entry */
} Pipeline;

/* Allocate and initialize a pipeline */
Pipeline *NewPipeline(int ports, int stages);

/* is the input stage of the pipeline full? */
int PipeFull(Pipeline *);


/* Advance elements in pipeline to the extent possible (if nothing blocking
   in an earlier stage). Return number of entries left in input stage of
   pipe afterward. Normally used after GetPipeElt/ClearPipeElt */
int CyclePipe(Pipeline *);


/* add a new entry to input stage of pipeline --
   normally used after CyclePipe*/
int AddToPipe(Pipeline *, struct YS__Req *);

/* Find an element in the head stage of the pipeline */
struct YS__Req *GetPipeElt(Pipeline *, int);


/* Clear out  an element in the pipeline -- normally used after GetPipeElt */
void ClearPipeElt(Pipeline *, int);


/* Replace a pipeline element with a new entry --
   can be used after GetPipeElt */
void SetPipeElt(Pipeline *, int,struct YS__Req *);

   
#endif
