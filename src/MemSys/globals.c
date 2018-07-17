/*
  globals.c
  
  This file contains some global declarations which are used throught the
  memory system simulator, particularly with regard to network statistics. 
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


#include "MemSys/module.h"
#include "MemSys/simsys.h"
#include <stdio.h>

int MemsimStatOn = ON;

int cur_index;
int net_index[1024];
int rev_index[NUM_SIZES];

BUFFER *BufTable[3][2048];		/* Used to find network utilization */
char WhichBuf[3][2048][100];
int buf_index[3];
OPORT *OportTable[3][200];
int oport_index[3];

double TotBlkTime;
int TotNumSamples;
STATREC *PktNumHopsHist[3];
STATREC *PktSzHist[3];
STATREC *CoheNumInvlHist;

STATREC *PktSzTimeTotalMean[3][NUM_SIZES];
STATREC *PktSzTimeNetMean[3][NUM_SIZES];
STATREC *PktSzTimeBlkMean[3][NUM_SIZES];

STATREC **PktHpsTimeTotalMean[3];
STATREC **PktHpsTimeNetMean[3];
STATREC **PktHpsTimeBlkMean[3];

STATREC *PktTOTimeTotalMean[3];
STATREC *PktTOTimeNetMean[3];
STATREC *PktTOTimeBlkMean[3];

STATREC *Req_stat_total1;
STATREC *Req_stat_cache1;
STATREC *Req_stat_fsend1;
STATREC *Req_stat_fnet1;
STATREC *Req_stat_frcv1;
STATREC *Req_stat_dir1;
STATREC *Req_stat_dir_leave1;
STATREC *Req_stat_ssend1;
STATREC *Req_stat_snet1;
STATREC *Req_stat_srcv1;
STATREC *Req_stat_total2;
STATREC *Req_stat_cache2;
STATREC *Req_stat_fsend2;
STATREC *Req_stat_fnet2;
STATREC *Req_stat_frcv2;
STATREC *Req_stat_dir2;
STATREC *Req_stat_w_pend2;
STATREC *Req_stat_w_cnt2;
STATREC *Req_stat_dir_leave2;
STATREC *Req_stat_memory2;
STATREC *Req_stat_ssend2;
STATREC *Req_stat_snet2;
STATREC *Req_stat_srcv2;
STATREC *Req_stat_rar2;
STATREC *Req_stat_num_rar;
STATREC *Req_stat_wrb2;

int NUM_HOPS;
int blocksize;
int blocksize2;            /* Second level cache */
int block_bits;
