/*
  tr.pool.h

  Pool tracing macros
  
  */
/*****************************************************************************/
/* This file is part of the RSIM Simulator, and is based on earlier code     */
/* from RPPT: The Rice Parallel Processing Testbed.                          */
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

#ifndef BQUEUESTRH
#define BQUEUESTRH

#ifdef debug
#include "dbsim.h"
/************************************************************************\
*                                 POOL tracing statements                              *
\**************************************************************************************/

#define TRACE_POOL_getobj1  \
   if (TraceLevel >= MAXDBLEVEL) { \
      sprintf(YS__prbpkt,"        Getting object from the %s\n",pptr->name);  \
      YS__SendPrbPkt(TEXTPROBE,pptr->name,YS__prbpkt); \
   }

#define TRACE_POOL_getobj2  \
   if (TraceLevel >= MAXDBLEVEL) { \
      sprintf(YS__prbpkt,"        - %s gets new block from system\n",pptr->name);  \
      YS__SendPrbPkt(TEXTPROBE,pptr->name,YS__prbpkt); \
   }

#define TRACE_POOL_retobj  \
   if (TraceLevel >= MAXDBLEVEL) { \
      sprintf(YS__prbpkt,"        Returning object to %s\n",pptr->name);  \
      YS__SendPrbPkt(TEXTPROBE,pptr->name,YS__prbpkt); \
   }


#else  /*******************************************************************************/

#define TRACE_POOL_getobj1
#define TRACE_POOL_getobj2
#define TRACE_POOL_retobj

#endif  /******************************************************************************/

#endif
