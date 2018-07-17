/*
  typedefs.h

  Collection of typedefs used in various parts of the simulator.  Also
  holds the declaration for DEBUG_TIME, which is used to control when
  tracing informations are printed by the various modules when
  compiled with debugging/tracing options.

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



#ifndef _typedefs_h_
#define _typedefs_h_ 1

typedef struct YS__Module SMMODULE; 

typedef struct YS__Cache  CACHE;
typedef struct YS__Bus    BUS;
typedef struct YS__Link   LINK;
typedef struct YS__WBuffer WBUFFER;
typedef struct YS__Directory    DIR;
typedef struct YS__NetInterface SMNET;

typedef struct YS__Req     REQ;
typedef struct YS__arg     ARG;
typedef struct YS__SMPort  SMPORT;
typedef struct YS__Rmq     RMQ;
typedef struct YS__RmqItem RMQI;
typedef struct YS__WbufItem WBUFITEM;
typedef struct YS__MSHR    MSHRITEM;
typedef struct cache_data_struct Cachest;
typedef struct YS__DirLine Dirst;
typedef struct YS__DirEP   DirEP;

typedef struct CacheStat   CacheStatStruct;
typedef struct PrcrStat    PrcrStatStruct;

typedef struct link_para LINKPARA;

typedef struct YS__Qelem      QELEM;
typedef struct YS__Pool       POOL;
typedef struct YS__Qe         QE;
typedef struct YS__Queue      QUEUE;
typedef struct YS__SyncQue    SYNCQUE;
typedef struct YS__Sema       SEMAPHORE;
typedef struct YS__StatQe     STATQE;
typedef struct YS__Act        ACTIVITY;
typedef struct YS__Event      EVENT;
typedef struct YS__Mess       MESSAGE;
typedef struct YS__Stat       STATREC;

typedef struct YS__Prcr       PROCESSOR;
typedef struct YS__Mod        MODULE;
typedef struct YS__Buf        BUFFER;
typedef struct YS__Mux        MUX;
typedef struct YS__Demux      DEMUX;
typedef struct YS__Duplexmod  DUPLEXMOD;
typedef struct YS__OPort      OPORT;
typedef struct YS__IPort      IPORT;
typedef struct YS__Pkt        PACKET;
typedef struct YS__PktData    PKTDATA;

typedef int    (*cond)();     /* Pointer to a function that returns int               */
typedef void   (*func)();     /* Pointer to a function with no return value           */
typedef int    (*rtfunc)(SMMODULE *,REQ *);   /* Pointer to a routing function                        */
typedef int    (*rtfunc2)(int *,int *,int);   /* Pointer to a routing function for demux */

extern int DEBUG_TIME;

#endif
