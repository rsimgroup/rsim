/*
  bus.h

  contains variable and function definitions relating to the bus module

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



#ifndef _bus_h_
#define _bus_h_ 1

/*########################### BUS  DECLARATION ##############################*/

/*****************************************************************************/
/***************** bus module structure definition ***************************/
/*****************************************************************************/

struct YS__Bus {
  MODULE_FRAMEWORK

  /************************** Statistics collection *************************/
  double  begin_util;          /* for new utilization stats */
  double  time_of_last_clear;  /* for new utilization stats */

  int     bus_type; /* type of bus -- only pipelined/split-transaction
		       supported */
  cond    cohe_rtn; /* unused */
  int     wait;
  func    prnt_rtn; /* statistics */
  int     next_port;  /* counter for round-robin */
  
};

/****************** Bus module function definitions ************************/
/*                              (detailed documentation in the bus.c file) */
BUS *NewBus();
void node_bus();
void bus_stat ();
void BusStatReportAll();
void BusStatReport ();
void BusStatClearAll();
void BusStatClear ();

/******************************* Bus Module Types *************************/
#define SPLIT 1	                 /* split transaction bus */
#define N_SPLIT 2                /* non-split transaction bus */
#define PIPELINED 3              /* pipelined bus */

extern double BUSCYCLE; /* (3.0) */  /* this is time used up for each "flit" */
extern double ARBDELAY; /* (1.0) */ /* time to direct bus control */
extern int BUSWIDTH; /* (32) */   /* bus width in bytes                 */

/*********************** States in the state machine  ************************/
#define BUSSTART    0   /* picks up requests in RR order from bus agents     */
#define SERVICE     1   /* this determines delay for this request            */
#define BUSDELIVER  2   /* one delivers the request to destination bus agent */


#endif
