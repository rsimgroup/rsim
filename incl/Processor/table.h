/*****************************************************************************/
/*   table.h :   Table of instructions (SPARC-specific)                      */
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


#ifndef _table_h_
#define _table_h_ 1

#include "instruction.h"

/* IMF => instruction manipulation functions */

extern IMF toplevelop[4];        /* top level classification of instructions */

extern IMF brop2[8];             /* branch operations */
extern INSTRUCTION ibrop2[8];

extern IMF arithop[64];          /* arithmetic operations */
extern INSTRUCTION iarithop[64];

extern IMF fpop1[512];           /* floating point operations */
extern INSTRUCTION ifpop1[512];

extern IMF fpop2[512];           /* floating point operations -- (move, etc) */
extern INSTRUCTION ifpop2[512];

extern IMF memop3[64];           /* memory operations */
extern INSTRUCTION imemop3[64];

/* First are bits 31 and 30 */

enum op {BRANCH=0,CALL=1,ARITH=2,MEM=3};

/* Now, within BRANCH, let's look at field op2,
	bits 24-22. We'll keep cond/rcond as a
	field in our instruction structure and
	do a case statement on that. */

enum br_op2 {ILLTRAP=0,BPcc=1,Bicc=2,BPr=3,SETHI=4,FBPfcc=5,FBfcc=6,brRES=7};

/* within ARITH, look at op3, which is bits 24-19 */

enum arith_ops {
	ADD,AND,OR,XOR,SUB,ANDN,ORN,XNOR,ADDC,
	MULX,UMUL,SMUL,SUBC,UDIVX,UDIV,SDIV,
	ADDcc,ANDcc,ORcc,XORcc,SUBcc,ANDNcc,ORNcc,XNORcc,ADDCcc,
	arithRES1,UMULcc,SMULcc,SUBCcc,arithRES2,UDIVcc,SDIVcc,
	TADDcc,TSUBcc,TADDccTV,TSUBccTV,MULScc,SLL,SRL,SRA,
	arithSPECIAL1 /* includes RDY, RDCCR, etc */,
	arithRES3, RDPR, FLUSHW, MOVcc, SDIVX, POPC, MOVR,
	arithSPECIAL2 /* includes WRY, etc */,
	SAVRESTD, WRPR, arithRES4, FPop1, FPop2, IMPDEP1, IMPDEP2,
	JMPL, RETURN, Tcc, FLUSH, SAVE, RESTORE, DONERETRY,
	arithRES5 };

/* Within FPop1, look at opf, bits 13-5 */

enum FPop1_ops {
	FMOVs=0x01, FMOVd, FMOVq,
	FNEGs=0x05, FNEGd, FNEGq,
	FABSs=0x09, FABSd, FABSq,

	FSQRTs=0x29, FSQRTd, FSQRTq,

	FADDs=0x41, FADDd, FADDq,
	FSUBs=0x45, FSUBd, FSUBq,
	FMULs=0x49, FMULd, FMULq,
	FDIVs=0x4d, FDIVd, FDIVq,

	FsMULd = 0x69, FdMULq = 0x6e,

	FsTOx=0x81,FdTOx,FqTOx,FxTOs,
	FxTOd=0x88, FxToq =0x8c,

	FiTOs=0xc4,
	FdTOs=0xc6,FqTOs,FiTOd,FsTOd,
	FqTOd=0xcb,FiTOq,FsTOq,FdTOq,

	FsTOi=0xd1,FdTOi,FqTOi
		
}; /* fill in the table with reserveds, then fill in these spaces separately */

/* Within FPop2, look at opf, bits 13-5 */

enum FPop2_ops {
	FMOVs0=0x01,FMOVd0,FMOVq0,
	FMOVs1=0x41,FMOVd1,FMOVq1,
	FMOVs2=0x81,FMOVd2,FMOVq2,
	FMOVs3=0xc1,FMOVd3,FMOVq3,

	FMOVsi=0x101,FMOVdi,FMOVqi,
	FMOVsx=0x181,FMOVdx,FMOVqx,

	FCMPs=0x51,FCMPd,FCMPq,
	FCMPEs=0x55,FCMPEd,FCMPEq,

	FMOVRsZ=0x25,FMOVRdZ,FMOVRqZ,
	FMOVRsLEZ=0x45,FMOVRdLEZ,FMOVRqLEZ,
	FMOVRsLZ=0x65,FMOVRdLZ,FMOVRqLZ,

	FMOVRsNZ=0xa5,FMOVRdNZ,FMOVRqNZ,
	FMOVRsGZ=0xc5,FMOVRdGZ,FMOVRqGZ,
	FMOVRsGEZ=0xe5,FMOVRdGEZ,FMOVRqGEZ
};

/* within MEM, look at op3, which is bits 24-19 */

enum mem_ops {
	LDUW, LDUB, LDUH, LDD, STW, STB, STH, STD,
	LDSW, LDSB, LDSH, LDX, memRES1, LDSTUB, STX, SWAP,
	LDUWA, LDUBA, LDUHA, LDDA, STWA, STBA, STHA, STDA,
	LDSWA, LDSBA, LDSHA, LDXA, memRES2, LDSTUBA, STXA, SWAPA,
	LDF, LDFSR, LDQF, LDDF, STF, STFSR, STQF, STDF,
	memRES3, memRES4, memRES5, memRES6, memRES7,
	PREFETCH, memRES8,memRES9,
	LDFA, memRES10, LDQFA, LDDFA, STFA, memRES11, STQFA, STDFA,
	memRES12, memRES13, memRES14, memRES15,
	CASA, PREFETCHA, CASXA, memRES16 };

/* here's cond field used in branches, bits 28-25 */

enum bp_conds {Nb,Eb,LEb,Lb,LEUb,CSb,NEGb,VSb,Ab,NEb,Gb,GEb,GUb,CCb,POSb,VCb};

enum fp_conds {Nf,NEf,LGf,ULf,Lf,UGf,Gf,Uf,Af,Ef,UEf,GEf,UGEf,LEf,ULEf,Of};

/* here's the rcond field used in BPr, MOVr, and FMOVr */

enum rcond {rRES1,Zr,LEr,Lr,rRES2,Nr,Gr,GEr};


#endif
