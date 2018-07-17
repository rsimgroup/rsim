/*****************************************************************************/
/*                                                                           */
/*     instruction.h :   Defines the instruction data structure              */
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


#ifndef _instruction_h_
#define _instruction_h_ 1

#include "regtype.h"
#include <stdio.h>
extern FILE *corefile;

/****************************************************************************/
/**** INSTRUCTION enumerated type -- SPARC architecture-specific ************/
/****************************************************************************/

enum INSTRUCTION 
{
  iRESERVED, iCALL, iILLTRAP, iBPcc, iBicc, iBPr, iSETHI, iFBPfcc, iFBfcc,
  iADD, iAND, iOR, iXOR, iSUB, iANDN, iORN, iXNOR, iADDC, iMULX, iUMUL,
  iSMUL, iSUBC, iUDIVX, iUDIV, iSDIV, iADDcc, iANDcc, iORcc, iXORcc,
  iSUBcc, iANDNcc, iORNcc, iXNORcc, iADDCcc, iUMULcc, iSMULcc, iSUBCcc,
  iUDIVcc, iSDIVcc, iTADDcc, iTSUBcc, iTADDccTV, iTSUBccTV, iMULScc,
  iSLL, iSRL, iSRA, iarithSPECIAL1 /* includes RDY, RDCCR, MEMBAR, etc */,
  iRDPR, iFLUSHW, iMOVcc, iSDIVX, iPOPC, iMOVR, iarithSPECIAL2 /* WRY, etc */,
  iSAVRESTD, iWRPR, iIMPDEP1, iIMPDEP2, iJMPL,
  iRETURN, iTcc, iFLUSH, iSAVE, iRESTORE, iDONERETRY, iFMOVs, iFMOVd,
  iFMOVq, iFNEGs, iFNEGd, iFNEGq, iFABSs, iFABSd, iFABSq, iFSQRTs,
  iFSQRTd, iFSQRTq, iFADDs, iFADDd, iFADDq, iFSUBs, iFSUBd, iFSUBq,
  iFMULs, iFMULd, iFMULq, iFDIVs, iFDIVd, iFDIVq, iFsMULd, iFdMULq,
  iFsTOx, iFdTOx, iFqTOx, iFxTOs, iFxTOd, iFxToq, iFiTOs, iFdTOs,
  iFqTOs, iFiTOd, iFsTOd, iFqTOd, iFiTOq, iFsTOq, iFdTOq, iFsTOi,
  iFdTOi, iFqTOi, iFMOVs0, iFMOVd0, iFMOVq0, iFMOVs1, iFMOVd1, iFMOVq1,
  iFMOVs2, iFMOVd2, iFMOVq2, iFMOVs3, iFMOVd3, iFMOVq3, iFMOVsi,
  iFMOVdi, iFMOVqi, iFMOVsx, iFMOVdx, iFMOVqx, iFCMPs, iFCMPd, iFCMPq,
  iFCMPEs, iFCMPEd, iFCMPEq, iFMOVRsZ, iFMOVRdZ, iFMOVRqZ, iFMOVRsLEZ,
  iFMOVRdLEZ, iFMOVRqLEZ, iFMOVRsLZ, iFMOVRdLZ, iFMOVRqLZ, iFMOVRsNZ,
  iFMOVRdNZ, iFMOVRqNZ, iFMOVRsGZ, iFMOVRdGZ, iFMOVRqGZ, iFMOVRsGEZ,
  iFMOVRdGEZ, iFMOVRqGEZ, iLDUW, iLDUB, iLDUH, iLDD, iSTW, iSTB, iSTH,
  iSTD, iLDSW, iLDSB, iLDSH, iLDX, iLDSTUB, iSTX, iSWAP, iLDUWA, iLDUBA,
  iLDUHA, iLDDA, iSTWA, iSTBA, iSTHA, iSTDA, iLDSWA, iLDSBA, iLDSHA,
  iLDXA, iLDSTUBA, iSTXA, iSWAPA, iLDF, iLDFSR, iLDXFSR, iLDQF, iLDDF, iSTF,
  iSTFSR, iSTXFSR, iSTQF, iSTDF, iPREFETCH, iLDFA, iLDQFA, iLDDFA, iSTFA,
  iSTQFA, iSTDFA, iCASA, iPREFETCHA, iCASXA,
  iRWSD, iRWWT_I, iRWWTI_I, iRWWS_I, iRWWSI_I,  /* various WriteThrough RW */
  iRWWT_F, iRWWTI_F, iRWWS_F, iRWWSI_F,          /* various WriteSend    RW */
  numINSTRS /* NOTE: THIS MUST BE THE LAST ENTRY HERE --
	       PUT ALL NEW INSTRUCTIONS BEFORE THIS ONE!!! */
};

/************ Window pointer change enumerated type defintion *************/
enum WPC {
  WPC_RESTORE = 1,          /* increment the current window pointer (cwp) */
  WPC_NONE = 0,                 /* retain the same current window pointer */
  WPC_SAVE = -1                   /* decrement the current window pointer */
};

/****************** Memory barrier related definitions ********************/
#define iMEMBAR iarithSPECIAL1
#define MB_StoreStore 8
#define MB_LoadStore 4
#define MB_StoreLoad 2
#define MB_LoadLoad 1

#define MB_MEMISSUE 2

#define PREF_NRD 0
#define PREF_1RD 1
#define PREF_NWT 2
#define PREF_1WT 3

extern char *inames[numINSTRS];                   /* instruction names array */

/****************************************************************************/
/********* Static instruction structure definition **************************/
/****************************************************************************/

struct instr                  /************** this is a _static_ instruction */
{
  INSTRUCTION instruction;

/* WRITTEN REGISTERS */
  int rd;				/* destination register */
  int rcc; 				/* destination condition code register */

/* READ REGISTERS */
  int rs1;				/* source register 1 */
  int rs2;				/* source register 2 */
  int rscc;				/* source condition code register */

/* AUXILIARY DATA */  
  int aux1;				
  int aux2;
  int imm;				/* immediate field */

/* BIT FIELDS */
  
  REGTYPE rd_regtype; 			/* is rd a floating point?  */
  REGTYPE rs1_regtype;			/* is rs1 a floating point? */
  REGTYPE rs2_regtype;			/* is rs2 a floating point? */
  int taken;				/* taken hint for branches  */
  int annul;				/* annul bit for branches   */
  int cond_branch; 			/* indicate conditional branch */
  int uncond_branch; 			/* indicate non-conditional branch
					0 for other,
					1 for address calc needed,
					2 for immediate address,
					3 for "call",
					4 for probable return (address calc needed) */

  WPC wpchange; 			/* indicate instructions that change window
					   pointer +1 for save, -1 for restore */
  
  instr(): instruction(iRESERVED),aux1(0),aux2(0),imm(0) {rscc=rcc=rd=rs1=rs2=0;rd_regtype=rs1_regtype=rs2_regtype=REG_INT;wpchange=WPC_NONE;taken=annul=cond_branch=uncond_branch=0;}
  instr(INSTRUCTION i): instruction(i),aux1(0),aux2(0),imm(0) {rscc=rcc=rd=rs1=rs2=0;rd_regtype=rs1_regtype=rs2_regtype=REG_INT;wpchange=WPC_NONE;taken=annul=cond_branch=uncond_branch=0;}
  instr (INSTRUCTION in_instruction, int in_rd, int in_rcc, int in_rs1, int in_rs2, int in_rscc, int in_aux1, int in_aux2, int in_imm, REGTYPE in_rd_regtype, REGTYPE in_rs1_regtype, REGTYPE in_rs2_regtype, int in_taken, int in_annul, int in_cond_branch, int in_uncond_branch, WPC in_wpchange) {instruction=in_instruction;aux1=in_aux1;aux2=in_aux2;imm=in_imm;rscc=in_rscc;rcc=in_rcc;rd=in_rd;rs1=in_rs1;rs2=in_rs2;rd_regtype=in_rd_regtype;rs1_regtype=in_rs1_regtype;rs2_regtype=in_rs2_regtype;wpchange=in_wpchange;taken=in_taken;annul=in_annul;cond_branch=in_cond_branch;uncond_branch=in_uncond_branch;}


  /* NOTE: in the SPARC architecture, and thus all architectures we'll
     support, int register #0 is constantly mapped to the number 0, so
     the value is always ready and can be ignored as a dependence */

  /* writing instruction to a file */
  void output(FILE *out) {fwrite((const char *)this,sizeof(instr),1,out);}

  /* reading instruction from a file */
  void input(FILE *in) {fread((char *)this,sizeof(instr),1,in);}

  /* printing a file */
  void print(); // define it as appropriate in other files
};

/* instruction manipulation function */
typedef int (*IMF)(instr *, unsigned); /* instr. manip. f'n */

/* instruction execution function */
typedef void (*IEF)(instr *); /* instr. exec. f'n */

#define Extract(v,hi,lo) ((v >> lo) & (((unsigned)-1) >> (31-hi+lo)))

#endif
