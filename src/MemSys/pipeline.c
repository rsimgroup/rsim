/*
  pipeline.c

  This file contains the procedures relating to the implementation
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


#include "MemSys/cache.h"
#include <malloc.h>

/*****************************************************************************/
/* NewPipeline: creates a cache pipeline with the specified number of ports  */
/*              (width per stage) and number of stages                       */
/*****************************************************************************/

Pipeline *NewPipeline(int ports, int stages)
{
  int i;
  Pipeline *pline = (Pipeline *)malloc(sizeof(Pipeline));
  pline->pipe = (struct YS__Req **)malloc(sizeof(struct YS__Req *)*ports*stages);
  for (i=0; i<ports*stages; i++)
    pline->pipe[i] = NULL;
  pline->depth=stages;
  pline->width=ports;
  pline->NumInPipe=0;
  pline->NumInLastStage=0;
  pline->WhereToAdd = ports * (stages-1); /* add at beginning of first stage */
  return pline;
}

/*****************************************************************************/
/* PipeFull: is the input stage of the cache pipeline full                   */
/*****************************************************************************/
int PipeFull(Pipeline *pline)
{
  return pline->NumInLastStage == pline->width;
}

/*****************************************************************************/
/* CyclePipe: Advances elements in the pipeline to the extent possible (if   */
/* some elements are blocked at the head, other elements will also block     */
/* before getting to the head). This procedure will usually be called after  */
/* GetPipeElt and possibly ClearPipeElt. The return value is the number of   */
/* entries left in the input stage of the pipe afterward                     */
/*****************************************************************************/

int CyclePipe(Pipeline *pline) /* return value is the number of entries in the last stage */
{
  int freespace=0, ctr=0;
  int i,j,prog;
  int dep = pline->depth, wid = pline->width;
  pline->NumInLastStage = 0; /* start from scratch; calculate as pipe progresses */
  for (i=0,ctr=0; i<dep; i++) /* Start at head stage of pipe, and
				 loop to end stage */
    {
      for (j=0; j<wid; j++) /* Loop through width of each pipeline stage */
	{
	  if (pline->pipe[ctr] != NULL) /* is there an entry in this postion? */
	    {
	      prog=0; /* prog says whether or not this entry made progress */
	      /* find the first space in the next stage to where this
		 entry can be moved. Note that not all of the entries
		 in this stage or the next stage have to move at the same
		 time. */
	      for (freespace = ( (i-1)*wid > freespace  ? (i-1)*wid : freespace); /* max progess is 1 stage */
		   freespace < ctr; freespace++)
		{
		  if (pline->pipe[freespace] == NULL)
		    {
		      /* There's a space available in the next stage, so
			 fill in that space and empty the current space */
		      pline->pipe[freespace]=pline->pipe[ctr];
		      pline->pipe[ctr] = NULL;

		      prog=1; /* entry advanced */
		      if (freespace >= wid * (dep -1)) 
			{
			  /* if entry taken is in the input stage, increment
			     NumInLastStage.  */
			  pline->NumInLastStage++;
			}
		      freespace++; /* move the freespace counter forward */
		      break;
		    }
		}
	      if (!prog) /* no progress was made -- entry didn't advance */
		{
		  freespace++;
		  if (ctr >= wid * (dep -1))
		    {
		      pline->NumInLastStage++;
		    }
		}
	    }
	  ctr++; /* Look at next pipeline entry */
	}
    }
  /* Set the WhereToAdd variable for use by AddToPipe routine (which
     puts something into input stage) */
  pline->WhereToAdd = (pline->NumInLastStage == 0) ? (dep-1)*wid : freespace;
  return pline->NumInLastStage;
}


/*****************************************************************************/
/* AddToPipe: add a new entry to the input stage of a cache pipeline         */
/* Returns 0 on success, -1 on failure                                       */
/*****************************************************************************/
int AddToPipe(Pipeline *pline, struct YS__Req *req)
{
  if (pline->NumInLastStage == pline->width)
    return -1;
  
  while (pline->pipe[pline->WhereToAdd] != NULL)
    {
      pline->WhereToAdd++;
      if (pline->WhereToAdd == pline->depth * pline->width)
	return -1;
    }
  
  pline->pipe[pline->WhereToAdd++] = req;
  pline->NumInPipe++;
  pline->NumInLastStage++;
  return 0;
}


/*****************************************************************************/
/* GetPipeElt: peek at an element in the head stage of the pipeline,         */
/* specified through posn. If "posn" isn't in the head stage, return NULL.   */
/*****************************************************************************/
struct YS__Req *GetPipeElt(Pipeline *pline, int posn)
{
  if (posn >= pline->width) /* not in final stage yet, so don't peek! */
    return NULL;
  return pline->pipe[posn];
}

/*****************************************************************************/
/* ClearPipeElt: remove an element from the pipeline. Should normally be     */
/* called after GetPipeElt, and for a "posn" in the head stage               */
/*****************************************************************************/
void ClearPipeElt(Pipeline *pline, int posn)
{
  pline->pipe[posn]=NULL;
  pline->NumInPipe--;
}

/*****************************************************************************/
/* SetPipeElt: replace an element in the pipeline with a new access.         */
/* Should normally be called after GetPipeElt, and for a "posn" in the head  */
/* stage                                                                     */
/*****************************************************************************/
void SetPipeElt(Pipeline *pline, int posn,struct YS__Req *nreq)
{
  pline->pipe[posn]=nreq;
}

