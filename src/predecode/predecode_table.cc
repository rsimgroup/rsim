/*
  predecode_table.cc

  Instruction table setup code for the predecode phase

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


#include "Processor/instruction.h"
#include "Processor/table.h"
#include "Processor/decoding.h"

/* First are bits 31 and 30 */

IMF toplevelop[4];

IMF brop2[8];
INSTRUCTION ibrop2[8];

IMF arithop[64];
INSTRUCTION iarithop[64];

IMF fpop1[512];
INSTRUCTION ifpop1[512];

IMF fpop2[512];
INSTRUCTION ifpop2[512];

IMF memop3[64];
INSTRUCTION imemop3[64];

/* Now, within BRANCH, let's look at field op2,
   bits 24-22. We'll keep cond/rcond as a
   field in our instruction structure and
   do a case statement on that. */

/* within ARITH, look at op3, which is bits 24-19 */


/* Within FPop1, look at opf, bits 13-5 */

/* fill in the table with reserveds, then fill in these spaces separately */

/* Within FPop2, look at opf, bits 13-5 */

/* within MEM, look at op3, which is bits 24-19 */

void TableSetup()
{
  int i;

  toplevelop[BRANCH]=branch_instr;
  toplevelop[CALL]=call_instr;
  toplevelop[ARITH]=arith_instr;
  toplevelop[MEM]=mem_instr;
  
  brop2[ILLTRAP]=illtrap;
  ibrop2[ILLTRAP]=iILLTRAP;
  
  brop2[BPcc]=bpcc;
  ibrop2[BPcc]=iBPcc;

  brop2[Bicc]=bicc;
  ibrop2[Bicc]=iBicc;

  brop2[BPr]=bpr;
  ibrop2[BPr]=iBPr;

  brop2[SETHI]=sethi;
  ibrop2[SETHI]=iSETHI;

  brop2[FBPfcc]=fbpfcc;
  ibrop2[FBPfcc]=iFBPfcc;

  brop2[FBfcc]=fbfcc;
  ibrop2[FBfcc]=iFBfcc;

  brop2[brRES]=brres;
  ibrop2[brRES]=iRESERVED;
  
  for (i=0; i <64; i++)
    {
      arithop[i]=arith_res;
      iarithop[i] = iRESERVED;
    }

  arithop[ADD]=arith_3;
  iarithop[ADD]=iADD;

  arithop[AND]=arith_3;
  iarithop[AND]=iAND;

  arithop[OR]=arith_3;
  iarithop[OR]=iOR;

  arithop[XOR]=arith_3;
  iarithop[XOR]=iXOR;

  arithop[SUB]=arith_3;
  iarithop[SUB]=iSUB;

  arithop[ANDN]=arith_3;
  iarithop[ANDN]=iANDN;

  arithop[ORN]=arith_3;
  iarithop[ORN]=iORN;

  arithop[XNOR]=arith_3;
  iarithop[XNOR]=iXNOR;

  arithop[ADDC]=arith_3sc;
  iarithop[ADDC]=iADDC;

  arithop[MULX]=arith_3;
  iarithop[MULX]=iMULX;

  arithop[UMUL]=arith_3y;
  iarithop[UMUL]=iUMUL;

  arithop[SMUL]=arith_3y;
  iarithop[SMUL]=iSMUL;

  arithop[SUBC]=arith_3sc;
  iarithop[SUBC]=iSUBC;

  arithop[UDIVX]=arith_3;
  iarithop[UDIVX]=iUDIVX;

  arithop[UDIV]=arith_3y;
  iarithop[UDIV]=iUDIV;

  arithop[SDIV]=arith_3y;
  iarithop[SDIV]=iSDIV;

  
  arithop[ADDcc]=arith_3cc;
  iarithop[ADDcc]=iADDcc;

  arithop[ANDcc]=arith_3cc;
  iarithop[ANDcc]=iANDcc;

  arithop[ORcc]=arith_3cc;
  iarithop[ORcc]=iORcc;

  arithop[XORcc]=arith_3cc;
  iarithop[XORcc]=iXORcc;

  arithop[SUBcc]=arith_3cc;
  iarithop[SUBcc]=iSUBcc;

  arithop[ANDNcc]=arith_3cc;
  iarithop[ANDNcc]=iANDNcc;

  arithop[ORNcc]=arith_3cc;
  iarithop[ORNcc]=iORNcc;

  arithop[XNORcc]=arith_3cc;
  iarithop[XNORcc]=iXNORcc;

  arithop[ADDCcc]=arith_3sccc;
  iarithop[ADDCcc]=iADDCcc;

  
  arithop[arithRES1]=arith_res;
  
  arithop[UMULcc]=arith_3y;
  iarithop[UMULcc]=iUMULcc;

  arithop[SMULcc]=arith_3y;
  iarithop[SMULcc]=iSMULcc;

  arithop[SUBCcc]=arith_3sccc;
  iarithop[SUBCcc]=iSUBCcc;

  
  arithop[arithRES2]=arith_res;

  arithop[UDIVcc]=arith_3y;
  iarithop[UDIVcc]=iUDIVcc;

  arithop[SDIVcc]=arith_3y;
  iarithop[SDIVcc]=iSDIVcc;

  
  arithop[TADDcc]=arith_3cc;
  iarithop[TADDcc]=iTADDcc;

  arithop[TSUBcc]=arith_3cc;
  iarithop[TSUBcc]=iTSUBcc;

  arithop[TADDccTV]=arith_3cc;
  iarithop[TADDccTV]=iTADDccTV;

  arithop[TSUBccTV]=arith_3cc;
  iarithop[TSUBccTV]=iTSUBccTV;

  arithop[MULScc]=arith_3y;
  iarithop[MULScc]=iMULScc;

  
  arithop[SLL]=arith_shift;
  iarithop[SLL]=iSLL;

  arithop[SRL]=arith_shift;
  iarithop[SRL]=iSRL;

  arithop[SRA]=arith_shift;
  iarithop[SRA]=iSRA;

  
  arithop[arithSPECIAL1]= arith_spec1;/* includes RDY, RDCCR, etc */
  iarithop[arithSPECIAL1]=iarithSPECIAL1;

  arithop[arithRES3]=arith_res;
  arithop[RDPR]=rdpr;
  iarithop[RDPR]=iRDPR;

  arithop[FLUSHW]=flushw;
  iarithop[FLUSHW]=iFLUSHW;

  arithop[MOVcc]=movcc;
  iarithop[MOVcc]=iMOVcc;

  arithop[SDIVX]=arith_3;
  iarithop[SDIVX]=iSDIVX;

  arithop[POPC]=popc;
  iarithop[POPC]=iPOPC;

  arithop[MOVR]=movr;
  iarithop[MOVR]=iMOVR;

  
  arithop[arithSPECIAL2]= arith_spec2;/* includes WRY, etc */
  iarithop[arithSPECIAL2]=iarithSPECIAL2;

  arithop[SAVRESTD]=savrestd;
  iarithop[SAVRESTD]=iSAVRESTD;

  arithop[WRPR]=wrpr;
  iarithop[WRPR]=iWRPR;

  arithop[arithRES4]=arith_res;
  arithop[FPop1]=fp_op1;

  arithop[FPop2]=fp_op2;

  arithop[IMPDEP1]=impdep1;
  iarithop[IMPDEP1]=iIMPDEP1;

  arithop[IMPDEP2]=impdep2;
  iarithop[IMPDEP2]=iIMPDEP2;

  
  arithop[JMPL]=jmpl;
  iarithop[JMPL]=iJMPL;

  arithop[RETURN]=ret;
  iarithop[RETURN]=iRETURN;

  arithop[Tcc]=tcc;
  iarithop[Tcc]=iTcc;

  arithop[FLUSH]=flush;
  iarithop[FLUSH]=iFLUSH;

  arithop[SAVE]=sarith_3;
  iarithop[SAVE]=iSAVE;

  arithop[RESTORE]=rarith_3;
  iarithop[RESTORE]=iRESTORE;

  arithop[DONERETRY]=savrestd;
  iarithop[DONERETRY]=iDONERETRY;

  arithop[arithRES5]=arith_res;

  for (i=0; i <512; i++)
    {
      fpop1[i]=arith_res;
      ifpop1[i]=iRESERVED;
    }

  fpop1[FMOVs]=fp_2s;
  ifpop1[FMOVs]=iFMOVs;

  fpop1[FMOVd]=fp_2;
  ifpop1[FMOVd]=iFMOVd;

  fpop1[FMOVq]=fp_2;
  ifpop1[FMOVq]=iFMOVq;

  
  fpop1[FNEGs]=fp_2s;
  ifpop1[FNEGs]=iFNEGs;

  fpop1[FNEGd]=fp_2;
  ifpop1[FNEGd]=iFNEGd;

  fpop1[FNEGq]=fp_2;
  ifpop1[FNEGq]=iFNEGq;

  
  fpop1[FABSs]=fp_2s;
  ifpop1[FABSs]=iFABSs;

  fpop1[FABSd]=fp_2;
  ifpop1[FABSd]=iFABSd;

  fpop1[FABSq]=fp_2;
  ifpop1[FABSq]=iFABSq;

  
  fpop1[FSQRTs]=fp_2s;
  ifpop1[FSQRTs]=iFSQRTs;

  fpop1[FSQRTd]=fp_2;
  ifpop1[FSQRTd]=iFSQRTd;

  fpop1[FSQRTq]=fp_2;
  ifpop1[FSQRTq]=iFSQRTq;

  
  fpop1[FADDs]=fp_3s;
  ifpop1[FADDs]=iFADDs;

  fpop1[FADDd]=fp_3;
  ifpop1[FADDd]=iFADDd;

  fpop1[FADDq]=fp_3;
  ifpop1[FADDq]=iFADDq;

  
  fpop1[FSUBs]=fp_3s;
  ifpop1[FSUBs]=iFSUBs;

  fpop1[FSUBd]=fp_3;
  ifpop1[FSUBd]=iFSUBd;

  fpop1[FSUBq]=fp_3;
  ifpop1[FSUBq]=iFSUBq;

  
  fpop1[FMULs]=fp_3s;
  ifpop1[FMULs]=iFMULs;

  fpop1[FMULd]=fp_3;
  ifpop1[FMULd]=iFMULd;

  fpop1[FMULq]=fp_3;
  ifpop1[FMULq]=iFMULq;

  
  fpop1[FDIVs]=fp_3s;
  ifpop1[FDIVs]=iFDIVs;

  fpop1[FDIVd]=fp_3;
  ifpop1[FDIVd]=iFDIVd;

  fpop1[FDIVq]=fp_3;
  ifpop1[FDIVq]=iFDIVq;

  
  fpop1[FsMULd]=fp_3sd;
  ifpop1[FsMULd]=iFsMULd;

  fpop1[FdMULq]=fp_3;
  ifpop1[FdMULq]=iFdMULq;

  
  fpop1[FsTOx]=fp_2sd;
  ifpop1[FsTOx]=iFsTOx;

  fpop1[FdTOx]=fp_2;
  ifpop1[FdTOx]=iFdTOx;

  fpop1[FqTOx]=fp_2;
  ifpop1[FqTOx]=iFqTOx;

  fpop1[FxTOs]=fp_2ds;
  ifpop1[FxTOs]=iFxTOs;
  
  fpop1[FxTOd]=fp_2;
  ifpop1[FxTOd]=iFxTOd;

  fpop1[FxToq]=fp_2;
  ifpop1[FxToq]=iFxToq;

  
  fpop1[FiTOs]=fp_2s;
  ifpop1[FiTOs]=iFiTOs;

  
  fpop1[FdTOs]=fp_2ds;
  ifpop1[FdTOs]=iFdTOs;

  fpop1[FqTOs]=fp_2ds;
  ifpop1[FqTOs]=iFqTOs;

  fpop1[FiTOd]=fp_2sd; 
  ifpop1[FiTOd]=iFiTOd;

  fpop1[FsTOd]=fp_2sd;
  ifpop1[FsTOd]=iFsTOd;
  
  fpop1[FqTOd]=fp_2;
  ifpop1[FqTOd]=iFqTOd;

  fpop1[FiTOq]=fp_2sd;
  ifpop1[FiTOq]=iFiTOq;

  fpop1[FsTOq]=fp_2sd;
  ifpop1[FsTOq]=iFsTOq;

  fpop1[FdTOq]=fp_2;
  ifpop1[FdTOq]=iFdTOq;

  
  fpop1[FsTOi]=fp_2s;
  ifpop1[FsTOi]=iFsTOi;

  fpop1[FdTOi]=fp_2ds;
  ifpop1[FdTOi]=iFdTOi;

  fpop1[FqTOi]=fp_2ds;
  ifpop1[FqTOi]=iFqTOi;

  

  for (i=0; i <512; i++)
    {
      fpop2[i]=arith_res;
      ifpop2[i]=iRESERVED;
    }

  fpop2[FMOVs0]=fmovccs;
  ifpop2[FMOVs0]=iFMOVs0;

  fpop2[FMOVd0]=fmovcc;
  ifpop2[FMOVd0]=iFMOVd0;

  fpop2[FMOVq0]=fmovcc;
  ifpop2[FMOVq0]=iFMOVq0;

  
  fpop2[FMOVs1]=fmovccs;
  ifpop2[FMOVs1]=iFMOVs1;

  fpop2[FMOVd1]=fmovcc;
  ifpop2[FMOVd1]=iFMOVd1;

  fpop2[FMOVq1]=fmovcc;
  ifpop2[FMOVq1]=iFMOVq1;

  
  fpop2[FMOVs2]=fmovccs;
  ifpop2[FMOVs2]=iFMOVs2;

  fpop2[FMOVd2]=fmovcc;
  ifpop2[FMOVd2]=iFMOVd2;

  fpop2[FMOVq2]=fmovcc;
  ifpop2[FMOVq2]=iFMOVq2;

  
  fpop2[FMOVs3]=fmovccs;
  ifpop2[FMOVs3]=iFMOVs3;

  fpop2[FMOVd3]=fmovcc;
  ifpop2[FMOVd3]=iFMOVd3;

  fpop2[FMOVq3]=fmovcc;
  ifpop2[FMOVq3]=iFMOVq3;

  
  fpop2[FMOVsi]=fmovccs;
  ifpop2[FMOVsi]=iFMOVsi;

  fpop2[FMOVdi]=fmovcc;
  ifpop2[FMOVdi]=iFMOVdi;

  fpop2[FMOVqi]=fmovcc;
  ifpop2[FMOVqi]=iFMOVqi;

  
  fpop2[FMOVsx]=fmovccs;
  ifpop2[FMOVsx]=iFMOVsx;

  fpop2[FMOVdx]=fmovcc;
  ifpop2[FMOVdx]=iFMOVdx;

  fpop2[FMOVqx]=fmovcc;
  ifpop2[FMOVqx]=iFMOVqx;

  
  fpop2[FCMPs]=fcmps;
  ifpop2[FCMPs]=iFCMPs;

  fpop2[FCMPd]=fcmp;
  ifpop2[FCMPd]=iFCMPd;

  fpop2[FCMPq]=fcmp;
  ifpop2[FCMPq]=iFCMPq;

  
  fpop2[FCMPEs]=fcmps;
  ifpop2[FCMPEs]=iFCMPEs;

  fpop2[FCMPEd]=fcmp;
  ifpop2[FCMPEd]=iFCMPEd;

  fpop2[FCMPEq]=fcmp;
  ifpop2[FCMPEq]=iFCMPEq;

  
  fpop2[FMOVRsZ]=fmovrccs;
  ifpop2[FMOVRsZ]=iFMOVRsZ;

  fpop2[FMOVRdZ]=fmovrcc;
  ifpop2[FMOVRdZ]=iFMOVRdZ;

  fpop2[FMOVRqZ]=fmovrcc;
  ifpop2[FMOVRqZ]=iFMOVRqZ;

  
  fpop2[FMOVRsLEZ]=fmovrccs;
  ifpop2[FMOVRsLEZ]=iFMOVRsLEZ;

  fpop2[FMOVRdLEZ]=fmovrcc;
  ifpop2[FMOVRdLEZ]=iFMOVRdLEZ;

  fpop2[FMOVRqLEZ]=fmovrcc;
  ifpop2[FMOVRqLEZ]=iFMOVRqLEZ;

  
  fpop2[FMOVRsLZ]=fmovrccs;
  ifpop2[FMOVRsLZ]=iFMOVRsLZ;

  fpop2[FMOVRdLZ]=fmovrcc;
  ifpop2[FMOVRdLZ]=iFMOVRdLZ;

  fpop2[FMOVRqLZ]=fmovrcc;
  ifpop2[FMOVRqLZ]=iFMOVRqLZ;

  
  fpop2[FMOVRsNZ]=fmovrccs;
  ifpop2[FMOVRsNZ]=iFMOVRsNZ;

  fpop2[FMOVRdNZ]=fmovrcc;
  ifpop2[FMOVRdNZ]=iFMOVRdNZ;

  fpop2[FMOVRqNZ]=fmovrcc;
  ifpop2[FMOVRqNZ]=iFMOVRqNZ;

  
  fpop2[FMOVRsGZ]=fmovrccs;
  ifpop2[FMOVRsGZ]=iFMOVRsGZ;

  fpop2[FMOVRdGZ]=fmovrcc;
  ifpop2[FMOVRdGZ]=iFMOVRdGZ;

  fpop2[FMOVRqGZ]=fmovrcc;
  ifpop2[FMOVRqGZ]=iFMOVRqGZ;

  
  fpop2[FMOVRsGEZ]=fmovrccs;
  ifpop2[FMOVRsGEZ]=iFMOVRsGEZ;

  fpop2[FMOVRdGEZ]=fmovrcc;
  ifpop2[FMOVRdGEZ]=iFMOVRdGEZ;

  fpop2[FMOVRqGEZ]=fmovrcc;
  ifpop2[FMOVRqGEZ]=iFMOVRqGEZ;


  for (i=0; i<64; i++)
    {
      imemop3[i]=iRESERVED;
    }
  
  memop3[LDUW]=mem_op2;
  imemop3[LDUW]=iLDUW;
  memop3[LDUB]=mem_op2;
  imemop3[LDUB]=iLDUB;

  memop3[LDUH]=mem_op2;
  imemop3[LDUH]=iLDUH;

  memop3[LDD]=dmem_op2;
  imemop3[LDD]=iLDD;

  memop3[STW]=smem_op2;
  imemop3[STW]=iSTW;

  memop3[STB]=smem_op2;
  imemop3[STB]=iSTB;

  memop3[STH]=smem_op2;
  imemop3[STH]=iSTH;

  memop3[STD]=sdmem_op2;
  imemop3[STD]=iSTD;
  
  memop3[LDSW]=mem_op2;
  imemop3[LDSW]=iLDSW;

  memop3[LDSB]=mem_op2;
  imemop3[LDSB]=iLDSB;

  memop3[LDSH]=mem_op2;
  imemop3[LDSH]=iLDSH;

  memop3[LDX]=mem_op2;
  imemop3[LDX]=iLDX;

  memop3[memRES1]=mem_res;
  memop3[LDSTUB]=mem_op2;
  imemop3[LDSTUB]=iLDSTUB;

  memop3[STX]=smem_op2;
  imemop3[STX]=iSTX;

  memop3[SWAP]=swap;
  imemop3[SWAP]=iSWAP;

  
  memop3[LDUWA]=amem_op2;
  imemop3[LDUWA]=iLDUWA;

  memop3[LDUBA]=amem_op2;
  imemop3[LDUBA]=iLDUBA;

  memop3[LDUHA]=amem_op2;
  imemop3[LDUHA]=iLDUHA;

  memop3[LDDA]=damem_op2;
  imemop3[LDDA]=iLDDA;

  memop3[STWA]=samem_op2;
  imemop3[STWA]=iSTWA;

  memop3[STBA]=samem_op2;
  imemop3[STBA]=iSTBA;

  memop3[STHA]=samem_op2;
  imemop3[STHA]=iSTHA;

  memop3[STDA]=sdamem_op2;
  imemop3[STDA]=iSTDA;

  
  memop3[LDSWA]=amem_op2;
  imemop3[LDSWA]=iLDSWA;

  memop3[LDSBA]=amem_op2;
  imemop3[LDSBA]=iLDSBA;

  memop3[LDSHA]=amem_op2;
  imemop3[LDSHA]=iLDSHA;

  memop3[LDXA]=amem_op2;
  imemop3[LDXA]=iLDXA;

  memop3[memRES2]=mem_res;
  memop3[LDSTUBA]=amem_op2;
  imemop3[LDSTUBA]=iLDSTUBA;

  memop3[STXA]=samem_op2;
  imemop3[STXA]=iSTXA;

  memop3[SWAPA]=aswap;
  imemop3[SWAPA]=iSWAPA;

  
  memop3[LDF]=mem_op2fs;
  imemop3[LDF]=iLDF;

  memop3[LDFSR]=mem_op2fsr;
  imemop3[LDFSR]=iLDFSR;

  memop3[LDQF]=mem_op2f;
  imemop3[LDQF]=iLDQF;

  memop3[LDDF]=mem_op2f;
  imemop3[LDDF]=iLDDF;

  memop3[STF]=smem_op2fs;
  imemop3[STF]=iSTF;

  memop3[STFSR]=smem_op2fsr;
  imemop3[STFSR]=iSTFSR;

  memop3[STQF]=smem_op2f;
  imemop3[STQF]=iSTQF;

  memop3[STDF]=smem_op2f;
  imemop3[STDF]=iSTDF;

  
  memop3[memRES3]=mem_res;

  memop3[memRES4]=mem_res;
  memop3[memRES5]=mem_res;
  memop3[memRES6]=mem_res;

  memop3[memRES7]=mem_res;
  
  memop3[PREFETCH]=pref;
  imemop3[PREFETCH]=iPREFETCH;

  memop3[memRES8]=mem_res;
  memop3[memRES9]=mem_res;
  
  memop3[LDFA]=amem_op2fs;
  imemop3[LDFA]=iLDFA;

  memop3[memRES10]=mem_res;
  memop3[LDQFA]=amem_op2f;
  imemop3[LDQFA]=iLDQFA;

  memop3[LDDFA]=amem_op2f;
  imemop3[LDDFA]=iLDDFA;

  memop3[STFA]=samem_op2fs;
  imemop3[STFA]=iSTFA;

  memop3[memRES11]=mem_res;
  memop3[STQFA]=samem_op2f;
  imemop3[STQFA]=iSTQFA;

  memop3[STDFA]=samem_op2f;
  imemop3[STDFA]=iSTDFA;

  memop3[memRES12]=mem_res;

  memop3[memRES13]=mem_res;
  memop3[memRES14]=mem_res;
  memop3[memRES15]=mem_res;
  
  memop3[CASA]=cas;
  imemop3[CASA]=iCASA;

  memop3[PREFETCHA]=apref;
  imemop3[PREFETCHA]=iPREFETCHA;

  memop3[CASXA]=cas;
  imemop3[CASXA]=iCASXA;

  memop3[memRES16]=mem_res;
}
