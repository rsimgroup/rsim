/*****************************************************************************/
/*                                                                           */
/*  mainsim.h :   Miscellaneous declarations                                 */
/*                                                                           */
/*****************************************************************************/
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


#ifndef _mainsim_h_
#define _mainsim_h_ 1

extern int simulate_ilp;
extern int stat_sched;

#ifdef __cplusplus
extern instr *instr_array;
extern int num_instructions;

enum SpecStores {SPEC_STALL, SPEC_LIMBO, SPEC_EXCEPT};
extern SpecStores spec_stores;
/* 
   this explains what to do if you have a load past an ambiguous store
   if SPEC_STALL, just don't issue the load
   if SPEC_LIMBO, issue the load, then keep it in "limbo" state -- when
   all previous stores are disambiguated, you then use the value of the
   load. If a previous store conflicts, you'll mark the load as though
   it had never been issued
   if SPEC_EXCEPT, issue the load. When it comes back, keep it in LoadQueue,
   but let its value go on to subsequent instructions. If all previous stores
   are disambiguated, remove the thing from the load queue. If a previous
   store conflicts, you'll mark the load as a soft exception
   
   */

/* global variables used to identify errors (overflow, underflow, etc) while
   simulating the floating point instruction set */
extern int fpfailed;                     
extern int fptest;

extern int FAST_UNITS;               /* flags single-cycle functional units */
extern int Prefetch;                                /* turns on prefetching */
extern int PrefetchWritesToL2;      /* flag to indicate that writes need to be
				       prefetched only to L1 with an L1
				       write-through cache */

/* indicate the number of instructions that can be flushed per cycle from
   the active list when handling an exception */
extern int NO_OF_EXCEPT_FLUSHES_PER_CYCLE;
extern int soft_rate;

extern int drop_all_sprefs;         /* flag to drop software prefetches */
extern int SC_NBWrites;             /* flag to allow non-blocking writes with
				      sequential consistency */
extern int Processor_Consistency;  /* flag to turn on processor consistency */
extern double parelapsedtime;      /* measure elapsed time in parallel code */
#endif

#ifdef COREFILE
extern FILE *corefile;            /* corefile for dumping debug information */
extern int DEBUG_TIME;          /* time after which debug dump is activated */ 
#endif

extern void ParseConfigFile();       /* parses the configuration file */

#endif
