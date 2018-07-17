/****************************************************************************/
/*                                                                          */
/*    units.h :  Definition of units and latency types                      */
/*                                                                          */
/****************************************************************************/
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


#ifndef _units_h_
#define _units_h_ 1

/* enumnerated type defining the type of functional units */
enum UTYPE {uALU,           /* Aritimetic and logical units */
	    uFP,            /* Floating point units */
	    uMEM,           /* Memory units */
	    uADDR,          /* Address-generation units */
	    numUTYPES       /* Total number of function unit types */
};

extern UTYPE unit[]; /* one for each instruction */
enum LATTYPE
{
  lALU,			/* ALU operations */
  lUSR1=1,		/* for user types */
  lUSR2=2,		/* for user types */
  lUSR3=3,		/* for user types */
  lUSR4=4,		/* for user types */
  lUSR5=5,		/* for user types */
  lUSR6=6,		/* for user types */
  lUSR7=7,		/* for user types */
  lUSR8=8,		/* for user types */
  lUSR9=9,		/* for user types */
  lBAR=10,		/* barrier synch  */
  lSPIN=11,		/* spin synch     */
  lACQ=12, 		/* acquire synch  */
  lREL=13,		/* release synch  */
  lRMW, lWT, lRD,	/* memory operations */
  lBRANCH,		/* branch operations */
  lFPU,			/* floating-point operations */
  lEXCEPT,		/* exceptions */
  lMEMBAR,		/* memory barriers */
  lBUSY,		/* busy time       */
  lRDmiss, lWTmiss, lRMWmiss, /* memory miss classifications */
  lRD_L1, lRD_L2, lRD_LOC, lRD_REM, /* read classification:
					L1, L2, local and remote */
  lWT_L1, lWT_L2, lWT_LOC, lWT_REM, /* write classification:
					L1, L2, local and remote */
  lRMW_L1, lRMW_L2, lRMW_LOC, lRMW_REM, /* RMW   classification:
					L1, L2, local and remote */
  lRMW_PFlate, lWT_PFlate, lRD_PFlate, /* prefetch classifications */
  lNUM_LAT_TYPES	/* number of latency types supported */
};

 
extern LATTYPE lattype[];
extern void UnitArraySetup();
extern void FuncTableSetup();

extern int LAT_ALU_OTHER, LAT_ALU_MUL, LAT_ALU_DIV, LAT_ALU_SHIFT;
extern int REP_ALU_OTHER, REP_ALU_MUL, REP_ALU_DIV, REP_ALU_SHIFT;
extern int LAT_FPU_COMMON, LAT_FPU_MOV, LAT_FPU_CONV,LAT_FPU_DIV, LAT_FPU_SQRT;
extern int REP_FPU_COMMON, REP_FPU_MOV, REP_FPU_CONV,REP_FPU_DIV, REP_FPU_SQRT;

#endif
