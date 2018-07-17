/*
  cpu.h

  Defines variables and functions relating to the cpu module

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



#ifndef _cpu_h_
#define _cpu_h_ 1

#include "module.h"
#include "simsys.h"

struct state;
struct instance;

/*############################# PROCESSOR  DECLARATION ######################*/

/*::::::::::: Processor Statistics Collection Data Structure  :::::::::::::::*/

struct PrcrStat {
  int read_dbl;           /* read and write doubles */
  int write_dbl;
  int read_sgl;           /* read and write singles */
  int write_sgl;
  int rmw;                /* read-modify-writes */
};

/*:::::::::::::: Processor Module Data Structure  :::::::::::::::::::::::::::*/

struct YS__Prcr {

   MODULE_FRAMEWORK

   int      status;               /* IDLE, BUSY                              */
  IPORT    *iports[MAXFANOUT];   /* Network input ports                      */
  int      noiports;       /* Number of network input ports on the processor */
  int (*router)(int,int,int);  /* Message routing function for the processor */
  OPORT    *oports[MAXFANOUT];   /* Network output ports                     */
  int      nooports;     /* Number of network output ports on the processor  */
  QUEUE    *MsgList;     /* Queue of messages delivered to this processor    */
  QUEUE    *WaitingProcesses;    /* Queue of processes waiting for messages  */
   char     *localptr;   /* Pointer tothis processor's local space           */
   int      localsize;            /* Size of this processor's private space  */

  func      prnt_rtn;            /* Routine used to print statistics at end  */
  PrcrStatStruct stat;
};


extern PROCESSOR *YS__ProcArray[];  /* array of cpu module pointers for use
				       by processor memory unit */

/* addrinsert sends a new REQUEST to memory system simulator */
void addrinsert(struct state *,struct instance *,int,unsigned,int,int,
		int,int,PROCESSOR *, double, double);

/* Functions relating cpu statistics */
void prcr_stat(PROCESSOR *prptr, REQ *req);
void PrcrStatReportAll(void);
void PrcrStatReport(PROCESSOR *prptr);
void PrcrStatClearAll(void);
void PrcrStatClear(PROCESSOR *prptr);

#endif
