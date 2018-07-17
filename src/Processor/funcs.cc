/*
  funcs.cc

  This file emulates instruction processing within a functional unit.

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
#include "Processor/instance.h"
#include "Processor/state.h"
#include "Processor/table.h"
#include "Processor/exec.h"
#include "Processor/mainsim.h"
#include "Processor/processor_dbg.h"
#include "Processor/types.h"
#include "Processor/memory.h"
#include "Processor/simio.h"
#include <limits.h>
#include <math.h>
#include <malloc.h>
#include <signal.h>
#include <string.h>
extern "C"
{
#include "MemSys/cache.h"
}


/* here are the functions that actually emulate the instructions */

inline int xor(int a,int b) {return (a||b)&&(!(a&&b));}

#define icCARRY 1
#define icOVERFLOW 2
#define icZERO 4
#define icNEG 8
#define iccCARRY (v1 & icCARRY)
#define iccOVERFLOW (v1 & icOVERFLOW)
#define iccZERO (v1 & icZERO)
#define iccNEG (v1 & icNEG)

#define xcSHIFT 16
#define xccTOicc(x) ((x) >> xcSHIFT)

/* icc/xcc definitions get anded with the icc/xcc value for testing
   and ored for setting */

#define fccEQ 0
#define fccLT 1
#define fccGT 2
#define fccUO 3 /* unordered */

/* in all cases its frs1 [relop] frs2 */

inline int SignExtend(int num, int width)
{
  /* So, we want to sign extend an int field of size num */
  int bit = num & (1 << (width-1));
  if (bit ==0)
    return num;
  unsigned left = ((unsigned)-1) ^ ((1<<width)-1);
  return (unsigned) num | left;
}

#define SE SignExtend



void fnRESERVED(instance *inst, state *)
{
  /* fprintf(simerr,"empty function\n"); */
   inst->exception_code = ILLEGAL;
}


void fnILLTRAP(instance *inst, state *)
{
  if (inst->code->aux2 >= 4096)
    {
      /* these are special cases used for statistics hooks, etc. */
      /* make this just an ordinary move... */
      inst->rdvali = inst->rs1vali;
    }
  else
    inst->exception_code=SYSTRAP;
}


/************* Control flow instructions *************/

void fnCALL(instance *inst, state *)
{
  /* a call is always properly predicted, so leave it alone */
  inst->rdvali = InstrNumToAddr(inst->pc);
}

void fnJMPL(instance *inst, state *)
{
  int tmp, badpc;
  inst->rdvali = InstrNumToAddr(inst->pc);
  if (inst->code->aux1)    {
    tmp=InstrAddrToNum(inst->rs1vali + inst->code->imm,badpc);
  }
  else    {
    tmp=InstrAddrToNum(inst->rs1vali + inst->rs2vali,badpc);
  }
  if (badpc)
    inst->exception_code = BADPC;
  inst->newpc = tmp;
  inst->mispredicted = (tmp != inst->branch_pred);
}

void fnRETURN(instance *inst, state *)
{
  int tmp;
  int badpc;
  if (inst->code->aux1)
    {
      tmp=InstrAddrToNum(inst->rs1vali + inst->code->imm,badpc);
    }
  else
    {
      tmp =InstrAddrToNum(inst->rs1vali + inst->rs2vali,badpc);
    }

  if (badpc)
    inst->exception_code = BADPC;
  inst->newpc = tmp;
  /* proc->BPBComplete(inst->pc,inst->newpc); */
  inst->mispredicted = (inst->newpc != inst->branch_pred);
}


void fnTcc(instance *inst, state *)
{
  /* fprintf(simerr,"empty function\n"); */
  inst->exception_code = ILLEGAL; /* not currently supported */
}


void fnDONERETRY(instance *inst, state *proc)
{
  if (proc->privstate == 0)
    inst->exception_code = PRIVILEGED;
  else
    inst->exception_code = SERIALIZE;
}


void fnBPcc(instance *inst, state *proc)
{
  int v1;
  int taken=0;

  v1 = inst->rs1vali;
  if (inst->code->rs1 == COND_XCC)
    v1 = xccTOicc(v1);
  switch (inst->code->aux1) /* these will be cases of bp_conds */
    {
    case Ab:
      taken=1;
      break;
    case Nb:
      break;
    case Eb:
      if (iccZERO)
	taken=1;
      break;
    case NEb:
      if (!iccZERO)
	taken=1;
      break;
    case Gb:
      if (!(iccZERO || xor(iccNEG,iccOVERFLOW)))
	taken = 1;
      break;
    case LEb:
      if (iccZERO || xor(iccNEG,iccOVERFLOW))
	taken = 1;
      break;
    case GEb:
      if (!xor(iccNEG, iccOVERFLOW))
	taken = 1;
      break;
    case Lb:
      if (xor(iccNEG,iccOVERFLOW))
	taken = 1;
      break;
    case GUb:
      if (!(iccCARRY || iccZERO))
	taken = 1;
      break;
    case LEUb:
      if (iccCARRY || iccZERO)
	taken = 1;
      break;
    case CCb:
      if (!iccCARRY)
	taken=1;
      break;
    case CSb:
      if (iccCARRY)
	taken=1;
      break;
    case POSb:
      if (!iccNEG)
	taken=1;
      break;
    case NEGb:
      if (iccNEG)
	taken=1;
      break;
    case VCb:
      if (!iccOVERFLOW)
	taken=1;
      break;
    case VSb:
    default:
      if (iccOVERFLOW)
	taken=1;
      break;
    }  

  if (taken)
    inst->newpc = inst->pc + inst->code->imm;
  else
    inst->newpc = inst->pc + 2;

  if (inst->code->aux1 != Nb) /* don't mark it for bn */
    proc->BPBComplete(inst->pc,taken,inst->code->taken);

  inst->mispredicted = (inst->taken != taken);

}

#define fnBicc fnBPcc

void fnBPr(instance *inst, state *proc)
{
  int v1;
  int taken=0;

  v1 = inst->rs1vali;
  switch (inst->code->aux1) /* these will be cases of bp_conds */
    {
    case 1:
      if (v1 == 0)
	taken = 1;
      break;
    case 2:
      if (v1 <= 0)
	taken = 1;
      break;
    case 3:
      if (v1 < 0)
	taken = 1;
      break;
    case 5:
      if (v1 != 0)
	taken = 1;
      break;
    case 6:
      if (v1 > 0)
	taken = 1;
      break;
    case 7:
      if (v1 >= 0)
	taken = 1;
      break;
    case 0:
    case 4:
    default:
      break;
    }  
  
  if (taken)
    inst->newpc = inst->pc + inst->code->imm;
  else
    inst->newpc = inst->pc + 2;

  proc->BPBComplete(inst->pc,taken,inst->code->taken);
  
  inst->mispredicted = (inst->taken != taken);

}


void fnFBPfcc(instance *inst, state *proc)
{
  double v1;
  int taken=0;

  v1 = inst->rs1vali;
  
#define E (v1 == fccEQ)
#define L (v1 == fccLT)
#define G (v1 == fccGT)
#define U (v1 == fccUO)
  
  switch (inst->code->aux1)
    {
    case Af:
      taken=1;
      break;
    case Uf:
      if (U)
	taken=1;
      break;
    case Gf:
      if (G)
	taken=1;
      break;
    case UGf:
      if (U || G)
	taken=1;
      break;
    case Lf:
      if (L)
	taken=1;
      break;
    case ULf:
      if (U || L)
	taken=1;
      break;
    case LGf:
      if (L || G)
	taken=1;
      break;
    case NEf:
      if (L || G || U)
	taken=1;
      break;
    case Nf:
      break;
    case Ef:
      if (E)
	taken=1;
      break;
    case UEf:
      if (E || U)
	taken=1;
      break;
    case GEf:
      if (G || E)
	taken=1;
      break;
    case UGEf:
      if (U || G || E)
	taken=1;
      break;
    case LEf:
      if (L || E)
	taken=1;
      break;
    case ULEf:
      if (U||L||E)
	taken=1;
      break;
    case Of:
      if (E||L||G)
	taken=1;
      break;
    }
  
  
  if (taken)
    inst->newpc = inst->pc + inst->code->imm;
  else
    inst->newpc = inst->pc + 2;

  if (inst->code->aux1 != Nf) /* don't mark it for bn */
    proc->BPBComplete(inst->pc,taken,inst->code->taken);
  
  inst->mispredicted = (inst->taken != taken);

}

#define fnFBfcc fnFBPfcc

/************* Arithmetic instructions *************/

void fnSETHI(instance *inst, state *)
{
  unsigned tmp = inst->code->imm;
  inst->rdvali=tmp << 10;
}


void fnADD(instance *inst, state *)
{
  if (inst->code->aux1)
    {
      inst->rdvali=inst->rs1vali + inst->code->imm;
    }
  else
    {
      inst->rdvali=inst->rs1vali + inst->rs2vali;
    }
}


void fnAND(instance *inst, state *)
{
  if (inst->code->aux1)
    {
      inst->rdvali=inst->rs1vali & inst->code->imm;
    }
  else
    {
      inst->rdvali=inst->rs1vali & inst->rs2vali;
    }
}


void fnOR(instance *inst, state *)
{
  if (inst->code->aux1)
    {
      inst->rdvali=inst->rs1vali | inst->code->imm;
    }
  else
    {
      inst->rdvali=inst->rs1vali | inst->rs2vali;
    }
}


void fnXOR(instance *inst, state *)
{
  if (inst->code->aux1)
    {
      inst->rdvali=inst->rs1vali ^ inst->code->imm;
    }
  else
    {
      inst->rdvali=inst->rs1vali ^ inst->rs2vali;
    }
}


void fnSUB(instance *inst, state *)
{
  if (inst->code->aux1)
    {
      inst->rdvali=inst->rs1vali - inst->code->imm;
    }
  else
    {
      inst->rdvali=inst->rs1vali - inst->rs2vali;
    }
}


void fnANDN(instance *inst, state *)
{
  if (inst->code->aux1)
    {
      inst->rdvali=inst->rs1vali & (~inst->code->imm);
    }
  else
    {
      inst->rdvali=inst->rs1vali & (~inst->rs2vali);
    }
}


void fnORN(instance *inst, state *)
{
  if (inst->code->aux1)
    {
      inst->rdvali=inst->rs1vali | (~ inst->code->imm);
    }
  else
    {
      inst->rdvali=inst->rs1vali | (~ inst->rs2vali);
    }
}


void fnXNOR(instance *inst, state *)
{
  if (inst->code->aux1)
    {
      inst->rdvali=~(inst->rs1vali ^ inst->code->imm);
    }
  else
    {
      inst->rdvali=~(inst->rs1vali ^ inst->rs2vali);
    }
}


void fnADDC(instance *inst, state *)
{
  int vcc = inst->rsccvali;
  if (inst->code->rscc == COND_XCC)
    vcc = xccTOicc(vcc);
  if (inst->code->aux1)
    {
      inst->rdvali=inst->rs1vali + inst->code->imm + (vcc&icCARRY);
    }
  else
    {
      inst->rdvali=inst->rs1vali + inst->rs2vali + (vcc&icCARRY);
    }
}


void fnMULX(instance *inst, state *)
{
  if (inst->code->aux1)
    {
      inst->rdvali=inst->rs1vali * inst->code->imm;
    }
  else
    {
      inst->rdvali=inst->rs1vali * inst->rs2vali;
    }
}



void fnUMUL(instance *inst, state *)
{
  UINT64 destval;
  
  if (inst->code->aux1)
    {
      destval=UINT32(inst->rs1vali) * UINT32(inst->code->imm);
    }
  else
    {
      destval=UINT32(inst->rs1vali) * UINT32(inst->rs2vali);
    }
  inst->rdvali = destval; /* WRITE ALL 64 bits when supported... */
  inst->rccvali = (destval >> 32); /* Y reg gets 32 MSBs */
}

void fnSMUL(instance *inst, state *)
{
  INT64 destval;
  
  if (inst->code->aux1)
    {
      destval=INT32(inst->rs1vali) * INT32(inst->code->imm);
    }
  else
    {
      destval=INT32(inst->rs1vali) * INT32(inst->rs2vali);
    }
  inst->rdvali = destval; /* WRITE ALL 64 bits when supported... */
  inst->rccvali = (UINT64(destval) >> 32); /* Y reg gets 32 MSBs */
}



void fnUDIVX(instance *inst, state *)
{
  unsigned v1 = inst->rs1vali;
  unsigned v2;
  v2 = (inst->code->aux1) ? inst->code->imm : inst->rs2vali;

  if (v2 == 0) /* divide by 0 */
    inst->exception_code=DIV0;
  else
    inst->rdvali=v1/v2;
}


void fnUDIV(instance *inst, state *)
{
  UINT64 v1 = (UINT64(UINT32(inst->rsccvali)) << 32) | UINT32(inst->rs1vali);
  /* Y ## lower 32 of rs1 */
  UINT64 quot;
  UINT32 v2;
  v2 = (inst->code->aux1) ? UINT32(inst->code->imm) : UINT32(inst->rs2vali);

  if (inst->code->instruction == iUDIVcc)
    {
      inst->rccvali = 0;
    }
  if (v2 == 0) /* divide by 0 */
    inst->exception_code=DIV0;
  else
    {
      quot = v1/UINT64(v2);
      if (quot > UINT64(UINT32_MAX))
	{
	  inst->rdvali = UINT32_MAX;
	  if (inst->code->instruction == iUDIVcc)
	    inst->rccvali |= iccOVERFLOW;
	}
      else
	{
	  inst->rdvali=quot;
	}
      if (inst->code->instruction == iUDIVcc)
	{
	  if (INT32(inst->rdvali) == 0)
	    inst->rccvali |= iccZERO;
	  if (INT32(inst->rdvali) < 0) /* if bit 31 is 1 */
	    inst->rccvali |= iccNEG;
	}
    }
}

void fnSDIV(instance *inst, state *)
{
  INT64 v1 = INT64((UINT64(UINT32(inst->rsccvali)) << 32) | UINT32(inst->rs1vali));
  /* Y ## lower 32 of rs1 */
  INT64 quot;
  INT32 v2;
  v2 = (inst->code->aux1) ? INT32(inst->code->imm) : INT32(inst->rs2vali);

  if (inst->code->instruction == iSDIVcc)
    {
      inst->rccvali = 0;
    }
  
  if (v2 == 0) /* divide by 0 */
    inst->exception_code=DIV0;
  else
    {
      quot = v1 / INT64(v2);
      if (quot < INT64(INT32_MIN))
	{
	  inst->rdvali = INT32_MIN;
	  if (inst->code->instruction == iSDIVcc)
	    inst->rccvali |= iccOVERFLOW;
	}
      else if (quot > INT64(INT32_MAX))
	{
	  inst->rdvali = INT32_MAX;
	  if (inst->code->instruction == iSDIVcc)
	    inst->rccvali |= iccOVERFLOW;
	}
      else
	{
	  inst->rdvali=quot;
	}
      if (inst->code->instruction == iSDIVcc)
	{
	  if (INT32(inst->rdvali) == 0)
	    inst->rccvali |= iccZERO;
	  if (INT32(inst->rdvali) < 0) /* if bit 31 is 1 */
	    inst->rccvali |= iccNEG;
	}
    }
}


void fnSUBC(instance *inst, state *)
{
  int vcc = inst->rsccvali;
  if (inst->code->rscc == COND_XCC)
    vcc = xccTOicc(vcc);
  if (inst->code->aux1)
    {
      inst->rdvali=inst->rs1vali - inst->code->imm - (vcc&icCARRY);
    }
  else
    {
      inst->rdvali=inst->rs1vali - inst->rs2vali - (vcc&icCARRY);
    }
}

void fnADDcc(instance *inst, state *)
{
  int v1,v2,tmp;
  v1=inst->rs1vali;

  if (inst->code->aux1)
    {
      v2 = inst->code->imm;
    }
  else
    {
      v2 = inst->rs2vali;
    }

  tmp=v1+v2;
  inst->rdvali=tmp;

  inst->rccvali = 0;
  if (tmp < 0)
    inst->rccvali |= icNEG; 
  if (tmp == 0)
    inst->rccvali |= icZERO;

  if ((v1 >= 0 && v2 >= 0 && tmp < 0) || (v1<0 && v2<0 && tmp>=0))
    inst->rccvali |= icOVERFLOW;
  if ((v1 < 0 && v2 < 0) || (tmp > 0 && (v1 < 0 || v2 < 0)))
    inst->rccvali |= icCARRY;
}

void fnADDCcc(instance *inst, state *)
{
  int v1,v2,v3,tmp;
  v1=inst->rs1vali;
  
  int vcc=inst->rsccvali;
  if (inst->code->rscc == COND_XCC)
    vcc = xccTOicc(vcc);

  if (inst->code->aux1)
    {
      v3=v2 = inst->code->imm;
    }
  else
    {
      v3=v2 = inst->rs2vali;
    }

  v2 += vcc & icCARRY;
  tmp=v1+v2;
  inst->rdvali=tmp;

  inst->rccvali = 0;
  if (tmp < 0)
    inst->rccvali |= icNEG; 
  if (tmp == 0)
    inst->rccvali |= icZERO;

  if ((v1 >= 0 && v2 >= 0 && tmp < 0) || (v1<0 && v2<0 && tmp>=0))
    inst->rccvali |= icOVERFLOW;
  if ((v1 < 0 && v2 < 0) || (tmp > 0 && (v1 < 0 || v2 < 0)))
    inst->rccvali |= icCARRY;
}


void fnANDcc(instance *inst, state *)
{
  int v1,v2,tmp;
  v1=inst->rs1vali;

  if (inst->code->aux1)
    {
      v2 = inst->code->imm;
    }
  else
    {
      v2 = inst->rs2vali;
    }

  tmp=v1 & v2;
  inst->rdvali=tmp;

  inst->rccvali = 0;
  if (tmp < 0)
    inst->rccvali |= icNEG; 
  if (tmp == 0)
    inst->rccvali |= icZERO;
}


void fnORcc(instance *inst, state *)
{
  int v1,v2,tmp;
  v1=inst->rs1vali;

  if (inst->code->aux1)
    {
      v2 = inst->code->imm;
    }
  else
    {
      v2 = inst->rs2vali;
    }

  tmp=v1 | v2;
  inst->rdvali=tmp;

  inst->rccvali = 0;
  if (tmp < 0)
    inst->rccvali |= icNEG; 
  if (tmp == 0)
    inst->rccvali |= icZERO;
}


void fnXORcc(instance *inst, state *)
{
  int v1,v2,tmp;
  v1=inst->rs1vali;

  if (inst->code->aux1)
    {
      v2 = inst->code->imm;
    }
  else
    {
      v2 = inst->rs2vali;
    }

  tmp=v1 ^ v2;
  inst->rdvali=tmp;

  inst->rccvali = 0;
  if (tmp < 0)
    inst->rccvali |= icNEG; 
  if (tmp == 0)
    inst->rccvali |= icZERO;
}


void fnSUBcc(instance *inst, state *)
{
  int v1,v2,tmp;
  v1=inst->rs1vali;

  if (inst->code->aux1)
    {
      v2 = inst->code->imm;
    }
  else
    {
      v2 = inst->rs2vali;
    }

  tmp=v1 - v2;
  inst->rdvali=tmp;

  inst->rccvali = 0;
  if (tmp < 0)
    inst->rccvali |= icNEG; 
  if (tmp == 0)
    inst->rccvali |= icZERO;

  if ((v1 >= 0 && v2 < 0 && tmp < 0) || (v1<0 && v2>=0 && tmp>=0))
    inst->rccvali |= icOVERFLOW;
  if ((unsigned)v1 < (unsigned) v2)
    inst->rccvali |= icCARRY;
}

void fnSUBCcc(instance *inst, state *)
{
  int v1,v2,tmp;
  v1=inst->rs1vali;

  int vcc=inst->rsccvali;
  if (inst->code->rscc == COND_XCC)
    vcc = xccTOicc(vcc);

  if (inst->code->aux1)
    {
      v2 = inst->code->imm;
    }
  else
    {
      v2 = inst->rs2vali;
    }

  v2 += vcc & icCARRY;
  tmp=v1 - v2;
  inst->rdvali=tmp;

  inst->rccvali = 0;
  if (tmp < 0)
    inst->rccvali |= icNEG; 
  if (tmp == 0)
    inst->rccvali |= icZERO;

  if ((v1 >= 0 && v2 < 0 && tmp < 0) || (v1<0 && v2>=0 && tmp>=0))
    inst->rccvali |= icOVERFLOW;
  if ((unsigned)v1 < (unsigned) v2)
    inst->rccvali |= icCARRY;
//  if ((v1 >= 0 && v2 < 0) || (v1 >= 0 && v2 > 0 && tmp < 0) || (tmp <0 && v1 < 0 && v2 < 0))
//    inst->rsccvali |= icCARRY;
 
}


void fnANDNcc(instance *inst, state *)
{
  int v1,v2,tmp;
  v1=inst->rs1vali;

  if (inst->code->aux1)
    {
      v2 = inst->code->imm;
    }
  else
    {
      v2 = inst->rs2vali;
    }

  tmp=v1 & ~ v2;
  inst->rdvali=tmp;

  inst->rccvali = 0;
  if (tmp < 0)
    inst->rccvali |= icNEG; 
  if (tmp == 0)
    inst->rccvali |= icZERO;
}


void fnORNcc(instance *inst, state *)
{
  int v1,v2,tmp;
  v1=inst->rs1vali;

  if (inst->code->aux1)
    {
      v2 = inst->code->imm;
    }
  else
    {
      v2 = inst->rs2vali;
    }

  tmp=v1 |~ v2;
  inst->rdvali=tmp;

  inst->rccvali = 0;
  if (tmp < 0)
    inst->rccvali |= icNEG; 
  if (tmp == 0)
    inst->rccvali |= icZERO;
}


void fnXNORcc(instance *inst, state *)
{
  int v1,v2,tmp;
  v1=inst->rs1vali;

  if (inst->code->aux1)
    {
      v2 = inst->code->imm;
    }
  else
    {
      v2 = inst->rs2vali;
    }

  tmp=~(v1 ^ v2);
  inst->rdvali=tmp;

  inst->rccvali = 0;
  if (tmp < 0)
    inst->rccvali |= icNEG; 
  if (tmp == 0)
    inst->rccvali |= icZERO;
}

void fnUMULcc(instance *inst, state *)
{
  inst->exception_code = SERIALIZE;
}

void fnSMULcc(instance *inst, state *)
{
  inst->exception_code = SERIALIZE;
}

void fnMULScc(instance *inst, state *)
{
  inst->exception_code = SERIALIZE;
}

void fnUMULcc_serialized(instance *inst, state *proc)
{
  fnUMUL(inst,proc);  /* this will set dest val in rdvali, Y val in rccvali */
  proc->physical_int_reg_file[inst->lrd] =
    proc->logical_int_reg_file[inst->lrd] = inst->rdvali;
  proc->physical_int_reg_file[convert_to_logical(proc->cwp,STATE_Y)] =
    proc->logical_int_reg_file[convert_to_logical(proc->cwp,STATE_Y)] = inst->rccvali;
  int vcc = 0;
  if (INT32(inst->rdvali) < 0)
    vcc |= icNEG;
  if (INT32(inst->rdvali) == 0)
    vcc |= icZERO;
  proc->physical_int_reg_file[convert_to_logical(proc->cwp,COND_ICC)] =
    proc->logical_int_reg_file[convert_to_logical(proc->cwp,COND_ICC)] = vcc;
}

void fnSMULcc_serialized(instance *inst, state *proc)
{
  fnSMUL(inst,proc); /* this will set dest val in rdvali, Y val in rccvali */
  proc->physical_int_reg_file[inst->lrd] =
    proc->logical_int_reg_file[inst->lrd] = inst->rdvali;
  proc->physical_int_reg_file[convert_to_logical(proc->cwp,STATE_Y)] =
    proc->logical_int_reg_file[convert_to_logical(proc->cwp,STATE_Y)] = inst->rccvali;
  int vcc = 0;
  if (INT32(inst->rdvali) < 0)
    vcc |= icNEG;
  if (INT32(inst->rdvali) == 0)
    vcc |= icZERO;
  proc->physical_int_reg_file[convert_to_logical(proc->cwp,COND_ICC)] =
    proc->logical_int_reg_file[convert_to_logical(proc->cwp,COND_ICC)] = vcc;
}

void fnMULScc_serialized(instance *inst, state *proc)
{
  /* this is a very messed up instruction. In this
     instruction, Y and ICC are both inputs and outputs */
  
  /* First, form a 32-bit value by shifting rs1 right by
     one bit and shifting in N^V as the high bit */
  INT32 vcc = proc->logical_int_reg_file[convert_to_logical(proc->cwp,COND_ICC)], vy = proc->logical_int_reg_file[convert_to_logical(proc->cwp,STATE_Y)];
  int neg = (vcc & icNEG), ovf = (vcc & icOVERFLOW);
  INT32 nxorv = (neg && !ovf) || (!neg && ovf);
  INT32 tmpval = (UINT32(inst->rs1vali) >> 1) | (nxorv<< 31);
  INT32 multiplicand, oldtmpval;
  if (inst->code->aux1)
    {
      multiplicand = INT32(inst->code->imm);
    }
  else
    {
      multiplicand = INT32(inst->rs2vali);
    }
  /* Now, iff LSB of Y is 1, multiplicand is added to tmpval;
     otherwise 0 is added; set ICC based on addition */
  
  vcc = 0;
  oldtmpval = tmpval;
  if (vy & 1)
    {
      tmpval += multiplicand;
      if ((oldtmpval < 0 && multiplicand < 0 && tmpval >= 0) ||
	  (oldtmpval >= 0 && multiplicand >= 0 && tmpval < 0))
	vcc |= icOVERFLOW;
    }
  if (tmpval < 0)
    vcc |= icNEG;
  if (tmpval == 0)
    vcc |= icZERO;
  
  /* put those sums/conditions in the dest and ICC */
  proc->physical_int_reg_file[inst->lrd] =
    proc->logical_int_reg_file[inst->lrd] = tmpval;
  proc->physical_int_reg_file[convert_to_logical(proc->cwp,COND_ICC)] =
    proc->logical_int_reg_file[convert_to_logical(proc->cwp,COND_ICC)] = vcc;
  /* the Y reg is shifted right one bit, with the old LSB of rs1 moving
     into the MSB */
  vy = (UINT32(vy) >> 1) | (UINT32(inst->rs1vali & 1) << 31);
  proc->physical_int_reg_file[convert_to_logical(proc->cwp,STATE_Y)] =
    proc->logical_int_reg_file[convert_to_logical(proc->cwp,STATE_Y)] = vy;
}

#define fnUDIVcc fnUDIV
#define fnSDIVcc fnSDIV

void fnSLL(instance *inst, state *)
{
  int v1=inst->rs1vali;
  int v2;
  if (inst->code->aux1 & 2)
    v2=inst->code->imm;
  else
    v2=inst->rs2vali;

  inst->rdvali=v1<<v2;
}


void fnSRL(instance *inst, state *)
{
  unsigned v1=inst->rs1vali;
  unsigned v2;
  if (inst->code->aux1 & 2)
    v2=inst->code->imm;
  else
    v2=inst->rs2vali;

  inst->rdvali=v1 >> v2;
}


void fnSRA(instance *inst, state *)
{
  int v1=inst->rs1vali;
  int v2;
  if (inst->code->aux1 & 2)
    v2=inst->code->imm;
  else
    v2=inst->rs2vali;

  inst->rdvali=v1 >> v2;
}


void fnarithSPECIAL1(instance *inst, state *)
{
  if (inst->code->rs1 == STATE_MEMBAR) /* stbar/membar */
    {
      /* nothing to do, but make sure not illegal...*/
      if (inst->code->rd != 0) /* membar shouldn't have this... */
	{
	  inst->exception_code=ILLEGAL;
	}
    }
  else
    {
      inst->rdvali=inst->rs1vali;
    }
}

void fnRDPR(instance *inst, state *)
{
  /* fprintf(simerr," *********** RDPR currently not implemented\n"); */
  inst->exception_code = PRIVILEGED;
}


void fnMOVcc(instance *inst, state *)
{
  int v1, v2;
  int& out=inst->rdvali;
  out=inst->rs1vali;
  if (inst->code->aux1)
    v2 = inst->code->imm;
  else
    v2 = inst->rs2vali;

  v1 = inst->rsccvali;
  if (inst->code->rscc >= COND_ICC) /* the integer condition code register */
    {
      if (inst->code->rscc == COND_XCC)
	v1 = xccTOicc(v1);
      switch (inst->code->aux2) /* these will be cases of bp_conds */
	{
	case Ab:
	  out=v2;
	  break;
	case Nb:
	  break;
	case NEb:
	  if (!iccZERO)
	    out=v2;
	  break;
	case Eb:
	  if (iccZERO)
	    out=v2;
	  break;
	case Gb:
	  if (!(iccZERO || xor(iccNEG,iccOVERFLOW)))
	    out = v2;
	  break;
	case LEb:
	  if (iccZERO || xor(iccNEG,iccOVERFLOW))
	    out = v2;
	  break;
	case GEb:
	  if (!xor(iccNEG ,iccOVERFLOW))
	    out = v2;
	  break;
	case Lb:
	  if (xor(iccNEG,iccOVERFLOW))
	    out = v2;
	  break;
	case GUb:
	  if (!(iccCARRY || iccZERO))
	    out = v2;
	  break;
	case LEUb:
	  if (iccCARRY || iccZERO)
	    out = v2;
	  break;
	case CCb:
	  if (!iccCARRY)
	    out=v2;
	  break;
	case CSb:
	  if (iccCARRY)
	    out=v2;
	  break;
	case POSb:
	  if (!iccNEG)
	    out=v2;
	  break;
	case NEGb:
	  if (iccNEG)
	    out=v2;
	  break;
	case VCb:
	  if (!iccOVERFLOW)
	    out=v2;
	  break;
	case VSb:
	default:
	  if (iccOVERFLOW)
	    out=v2;
	  break;
	}
    }
  else /* use floating point condition codes */
    {
      #define E (v1 == fccEQ)
      #define L (v1 == fccLT)
      #define G (v1 == fccGT)
      #define U (v1 == fccUO)
      
      switch(inst->code->aux2)
	{
	case Af:
	  out=v2;
	  break;
	case Nf:
	  break;
	case Uf:
	  if (U)
	    out=v2;	
	  break;
	case Gf:
	  if (G)
	    out=v2;
	  break;
	case UGf:
	  if (U || G)
	    out=v2;
	  break;
	case Lf:
	  if (L)
	    out=v2;
	  break;
	case ULf:
	  if (U || L)
	    out=v2;
	  break;
	case LGf:
	  if (L || G)
	    out=v2;
	  break;
	case NEf:
	  if (L || G || U)
	    out=v2;
	  break;
	case Ef:
	  if (E)
	    out=v2;
	  break;
	case UEf:
	  if (E || U)
	    out=v2;
	  break;
	case GEf:
	  if (G || E)
	    out=v2;
	  break;
	case UGEf:
	  if (U || G || E)
	    out=v2;
	  break;
	case LEf:
	  if (L||E)
	    out=v2;
	  break;
	case ULEf:
	  if (U||L||E)
	    out=v2;
	  break;
	case Of:
	  if (E||L||G)
	    out=v2;
	  break;
	}
    }
}


void fnSDIVX(instance *inst, state *)
{
  int v1 = inst->rs1vali;
  int v2;
  v2 = (inst->code->aux1) ? inst->code->imm : inst->rs2vali;

  if (v2 == 0) /* divide by 0 */
    inst->exception_code=DIV0;
  else
    inst->rdvali=v1/v2;
}


void fnPOPC(instance *inst, state *)
{
  static int POPC_arr[16]={0,1,1,2,
		       1,2,2,3,
		       1,2,2,3,
		       2,3,3,4};
  unsigned v1;
  int &out=inst->rdvali;
  if (inst->code->aux1)
    v1=inst->code->imm;
  else
    v1=inst->rs1vali;

  out =0;

  while (v1 != 0)
    {
      out += POPC_arr[v1 & 15];
      v1 >>= 4;
    }
}


void fnMOVR(instance *inst, state *)
{
  int &out=inst->rdvali;
  out=inst->rs1vali;
  int v1 = inst->rsccvali;
  int v2 = (inst->code->aux1) ? inst->code->imm : inst->rs2vali;

  switch (inst->code->aux2)
    {
    case 1:
      if (v1 == 0)
	out = v2;
      break;
    case 2:
      if (v1 <= 0)
	out = v2;
      break;
    case 3:
      if (v1 < 0)
	out = v2;
      break;
    case 5:
      if (v1 != 0)
	out = v2;
      break;
    case 6:
      if (v1 > 0)
	out = v2;
      break;
    case 7:
      if (v1 >= 0)
	out = v2;
      break;
    case 0:
    case 4:
    default:
      break;
    }
}


void fnarithSPECIAL2(instance *inst, state *)
{
  /* right now, only implement basic ones -- save others for later... */
  if (inst->code->rd == STATE_Y || inst->code->rd == STATE_CCR)
    {
      /* these get renamed, so they can be rewritten whenever */
      inst->rdvali=inst->rs1vali;
    }
  else if (inst->code->rd == STATE_ASI || inst->code->rd == STATE_FPRS)
    {
      /* These may be things of overall system-wide consequence, so handle
	 them appropriately by serializing before writing to these
	 registers */
      inst->exception_code = SERIALIZE;
    }
  else
    {
      fprintf(simerr,"Other cases of WRSR not yet implemented...\n");
    }
}


void fnSAVRESTD(instance *inst, state *proc)
{
  if (proc->privstate == 0)
    inst->exception_code = PRIVILEGED;
  else
    inst->exception_code = SERIALIZE;
}


void fnWRPR(instance *inst, state *)
{
  /* fprintf(simerr,"empty function\n"); */
  inst->exception_code = PRIVILEGED; /* not currently supported */
}


void fnIMPDEP1(instance *inst, state *)
{
  /* fprintf(simerr,"empty function\n"); */
  inst->exception_code = ILLEGAL; /* not currently supported */
}


void fnIMPDEP2(instance *inst, state *)
{
  /* fprintf(simerr,"empty function\n"); */
  inst->exception_code = ILLEGAL; /* not currently supported */
}

#define fnSAVE fnADD
#define fnRESTORE fnADD

/************* Floating-point instructions *************/

void fnFMOVs(instance *inst, state *)
{
  inst->rdvalfh=inst->rs2valfh;
}


void fnFMOVd(instance *inst, state *)
{
  inst->rdvalf=inst->rs2valf;
}


void fnFMOVq(instance *inst, state *)
{
  inst->rdvalf=inst->rs2valf;
}


void fnFNEGs(instance *inst, state *)
{
  inst->rdvalfh=-inst->rs2valfh;
}


void fnFNEGd(instance *inst, state *)
{
  inst->rdvalf=-inst->rs2valf;
}


void fnFNEGq(instance *inst, state *)
{
  inst->rdvalf=-inst->rs2valf;
}


void fnFABSs(instance *inst, state *)
{
  if (inst->rs2valfh >= 0)
    inst->rdvalfh=inst->rs2valfh;
  else
    inst->rdvalfh=-inst->rs2valfh;
}


void fnFABSd(instance *inst, state *)
{
  if (inst->rs2valf >= 0)
    inst->rdvalf=inst->rs2valf;
  else
    inst->rdvalf=-inst->rs2valf;
}


void fnFABSq(instance *inst, state *)
{
  if (inst->rs2valf >= 0)
    inst->rdvalf=inst->rs2valf;
  else
    inst->rdvalf=-inst->rs2valf;
}


void fnFSQRTs(instance *inst, state *)
{
  if (inst->rs2valfh >= 0)
    inst->rdvalfh=sqrt(inst->rs2valfh);
  else
    inst->exception_code=FPERR;
}


void fnFSQRTd(instance *inst, state *)
{
  if (inst->rs2valf >= 0)
    inst->rdvalf=sqrt(inst->rs2valf);
  else
    inst->exception_code=FPERR;
}


void fnFSQRTq(instance *inst, state *)
{
  if (inst->rs2valf >= 0)
    inst->rdvalf=sqrt(inst->rs2valf);
  else
    inst->exception_code=FPERR;
}

void fnFADDs(instance *inst, state *)
{
  fpfailed=0;
  fptest=1;
  inst->rdvalfh = inst->rs1valfh+ inst->rs2valfh;
  if (fpfailed)
    inst->exception_code=FPERR;
  fptest=0;
}


void fnFADDd(instance *inst, state *)
{
  fpfailed=0;
  fptest=1;
  inst->rdvalf = inst->rs1valf+ inst->rs2valf;
  if (fpfailed)
    inst->exception_code=FPERR;
  fptest=0;
}


void fnFADDq(instance *inst, state *)
{
  fpfailed=0;
  fptest=1;
  inst->rdvalf = inst->rs1valf+ inst->rs2valf;
  if (fpfailed)
    inst->exception_code=FPERR;
  fptest=0;
}

void fnFSUBs(instance *inst, state *)
{
  fpfailed=0;
  fptest=1;
  inst->rdvalfh = inst->rs1valfh - inst->rs2valfh;
  if (fpfailed)
    inst->exception_code=FPERR;
  fptest=0;
}


void fnFSUBd(instance *inst, state *)
{
  fpfailed=0;
  fptest=1;
  inst->rdvalf = inst->rs1valf - inst->rs2valf;
  if (fpfailed)
    inst->exception_code=FPERR;
  fptest=0;
}


void fnFSUBq(instance *inst, state *)
{
  fpfailed=0;
  fptest=1;
  inst->rdvalf = inst->rs1valf - inst->rs2valf;
  if (fpfailed)
    inst->exception_code=FPERR;
  fptest=0;
}


void fnFMULs(instance *inst, state *)
{
  fpfailed=0;
  fptest=1;
  inst->rdvalfh = inst->rs1valfh * inst->rs2valfh;
  if (fpfailed)
    inst->exception_code=FPERR;
  fptest=0;
}


void fnFMULd(instance *inst, state *)
{
  fpfailed=0;
  fptest=1;
  inst->rdvalf = inst->rs1valf * inst->rs2valf;
  if (fpfailed)
    inst->exception_code=FPERR;
  fptest=0;
}


void fnFMULq(instance *inst, state *)
{
  fpfailed=0;
  fptest=1;
  inst->rdvalf = inst->rs1valf * inst->rs2valf;
  if (fpfailed)
    inst->exception_code=FPERR;
  fptest=0;
}


void fnFDIVs(instance *inst, state *)
{
  fpfailed=0;
  fptest=1;
  inst->rdvalfh = inst->rs1valfh / inst->rs2valfh;
  if (fpfailed)
    inst->exception_code=FPERR;
  fptest=0;
}


void fnFDIVd(instance *inst, state *)
{
  fpfailed=0;
  fptest=1;
  inst->rdvalf = inst->rs1valf / inst->rs2valf;
  if (fpfailed)
    inst->exception_code=FPERR;
  fptest=0;
}


void fnFDIVq(instance *inst, state *)
{
  fpfailed=0;
  fptest=1;
  inst->rdvalf = inst->rs1valf / inst->rs2valf;
  if (fpfailed)
    inst->exception_code=FPERR;
  fptest=0;
}


void fnFsMULd(instance *inst, state *)
{
  fpfailed=0;
  fptest=1;
  inst->rdvalf = double(inst->rs1valfh) * double(inst->rs2valfh);
  if (fpfailed)
    inst->exception_code=FPERR;
  fptest=0;
}


void fnFdMULq(instance *inst, state *)
{
  fpfailed=0;
  fptest=1;
  inst->rdvalf = inst->rs1valf * inst->rs2valf;
  if (fpfailed)
    inst->exception_code=FPERR;
  fptest=0;
}


void fnFdTOx(instance *inst, state *)
{
  long long *ip = (long long *)&(inst->rdvalf);
  *ip=(long long) inst->rs2valf;
}


void fnFsTOx(instance *inst, state *)
{
  long long *ip = (long long *)&(inst->rdvalf);
  *ip=(long long) inst->rs2valfh;
}


void fnFqTOx(instance *inst, state *)
{
  long long *ip = (long long *)&(inst->rdvalf);
  *ip=(long long) inst->rs2valf;
}


void fnFxTOs(instance *inst, state *)
{
  long long *ip = (long long *)&(inst->rs2valf);
  long long res = *ip;
  inst->rdvalfh=float(res);
}


void fnFxTOd(instance *inst, state *)
{
  long long *ip = (long long *)&(inst->rs2valf);
  long long res = *ip;
  inst->rdvalf=double(res);
}


void fnFxToq(instance *inst, state *)
{
  long long *ip = (long long *)&(inst->rs2valf);
  long long res = *ip;
  inst->rdvalf=double(res);
}


void fnFiTOs(instance *inst, state *)
{
  int *ip = (int *)&(inst->rs2valfh);
  int res = *ip;
  inst->rdvalfh=float(res);
}


void fnFdTOs(instance *inst, state *)
{
  inst->rdvalfh=float(inst->rs2valf);
}


void fnFqTOs(instance *inst, state *)
{
  inst->rdvalfh=float(inst->rs2valf);
}



void fnFsTOd(instance *inst, state *)
{
  inst->rdvalf=double(inst->rs2valfh);
}


void fnFqTOd(instance *inst, state *)
{
  inst->rdvalf=inst->rs2valf;
}


void fnFiTOq(instance *inst, state *)
{
  int *ip = (int *)&(inst->rs2valfh);
  int res = *ip;
  inst->rdvalf=double(res);
}


void fnFsTOq(instance *inst, state *)
{
  inst->rdvalf=double(inst->rs2valfh);
}


void fnFdTOq(instance *inst, state *)
{
  inst->rdvalf=inst->rs2valf;
}


void fnFdTOi(instance *inst, state *)
{
  int *ip = (int *)&(inst->rdvalfh);
  *ip=(int) inst->rs2valf;
}

void fnFiTOd(instance *inst,state *)
{
  int *ip = (int *)&(inst->rs2valfh);
  int res = *ip;
  inst->rdvalf=double(res);
}

void fnFsTOi(instance *inst, state *)
{
  int *ip = (int *)&(inst->rdvalf);
  *ip=(int) inst->rs2valfh;
}

void fnFqTOi(instance *inst, state *)
{
  int *ip = (int *)&(inst->rdvalf);
  *ip=(int) inst->rs2valf;
}

#define fnFMOVq0 fnFMOVd0

void fnFMOVd0(instance *inst, state *)
{
  int v1;
  double v2;
  double& out=inst->rdvalf;
  out=inst->rs1valf;

  v2 = inst->rs2valf;

  v1 = inst->rsccvali;
  if (inst->code->rscc >= COND_ICC) /* the integer condition code register */
    {
      if (inst->code->rscc == COND_XCC) /* the xcc */
	v1 = xccTOicc(v1);
      switch (inst->code->aux2) /* these will be cases of bp_conds */
	{
	case Ab:
	  out=v2;
	  break;
	case Nb:
	  break;
	case NEb:
	  if (!iccZERO)
	    out=v2;
	  break;
	case Eb:
	  if (iccZERO)
	    out=v2;
	  break;
	case Gb:
	  if (!(iccZERO || xor(iccNEG,iccOVERFLOW)))
	    out = v2;
	  break;
	case LEb:
	  if (iccZERO || xor(iccNEG,iccOVERFLOW))
	    out = v2;
	  break;
	case GEb:
	  if (!xor(iccNEG,iccOVERFLOW))
	    out = v2;
	  break;
	case Lb:
	  if (xor(iccNEG,iccOVERFLOW))
	    out = v2;
	  break;
	case GUb:
	  if (!(iccCARRY || iccZERO))
	    out = v2;
	  break;
	case LEUb:
	  if (iccCARRY || iccZERO)
	    out = v2;
	  break;
	case CCb:
	  if (!iccCARRY)
	    out=v2;
	  break;
	case CSb:
	  if (iccCARRY)
	    out=v2;
	  break;
	case POSb:
	  if (!iccNEG)
	    out=v2;
	  break;
	case NEGb:
	  if (iccNEG)
	    out=v2;
	  break;
	case VCb:
	  if (!iccOVERFLOW)
	    out=v2;
	  break;
	case VSb:
	default:
	  if (iccOVERFLOW)
	    out=v2;
	  break;
	}
    }
  else /* use floating point condition codes */
    {
      #define E (v1 == fccEQ)
      #define L (v1 == fccLT)
      #define G (v1 == fccGT)
      #define U (v1 == fccUO)
      
      switch(inst->code->aux2)
	{
	case Af:
	  out=v2;
	  break;
	case Nf:
	  break;
	case Uf:
	  if (U)
	    out=v2;	
	  break;
	case Gf:
	  if (G)
	    out=v2;
	  break;
	case UGf:
	  if (U || G)
	    out=v2;
	  break;
	case Lf:
	  if (L)
	    out=v2;
	  break;
	case ULf:
	  if (U || L)
	    out=v2;
	  break;
	case LGf:
	  if (L || G)
	    out=v2;
	  break;
	case NEf:
	  if (L || G || U)
	    out=v2;
	  break;
	case Ef:
	  if (E)
	    out=v2;
	  break;
	case UEf:
	  if (E || U)
	    out=v2;
	  break;
	case GEf:
	  if (G || E)
	    out=v2;
	  break;
	case UGEf:
	  if (U || G || E)
	    out=v2;
	  break;
	case LEf:
	  if (L||E)
	    out=v2;
	  break;
	case ULEf:
	  if (U||L||E)
	    out=v2;
	  break;
	case Of:
	  if (E||L||G)
	    out=v2;
	  break;
	}
    }
}

void fnFMOVs0(instance *inst, state *)
{
  int v1;
  float v2;
  float& out=inst->rdvalfh;
  out=inst->rs1valfh;

  v2 = inst->rs2valfh;

  v1 = inst->rsccvali;
  if (inst->code->rscc >= COND_ICC) /* the integer condition code register */
    {
      if (inst->code->rscc == COND_XCC) /* the xcc */
	v1 = xccTOicc(v1);
      switch (inst->code->aux2) /* these will be cases of bp_conds */
	{
	case Ab:
	  out=v2;
	  break;
	case Nb:
	  break;
	case NEb:
	  if (!iccZERO)
	    out=v2;
	  break;
	case Eb:
	  if (iccZERO)
	    out=v2;
	  break;
	case Gb:
	  if (!(iccZERO || xor(iccNEG,iccOVERFLOW)))
	    out = v2;
	  break;
	case LEb:
	  if (iccZERO || xor(iccNEG,iccOVERFLOW))
	    out = v2;
	  break;
	case GEb:
	  if (!xor(iccNEG,iccOVERFLOW))
	    out = v2;
	  break;
	case Lb:
	  if (xor(iccNEG,iccOVERFLOW))
	    out = v2;
	  break;
	case GUb:
	  if (!(iccCARRY || iccZERO))
	    out = v2;
	  break;
	case LEUb:
	  if (iccCARRY || iccZERO)
	    out = v2;
	  break;
	case CCb:
	  if (!iccCARRY)
	    out=v2;
	  break;
	case CSb:
	  if (iccCARRY)
	    out=v2;
	  break;
	case POSb:
	  if (!iccNEG)
	    out=v2;
	  break;
	case NEGb:
	  if (iccNEG)
	    out=v2;
	  break;
	case VCb:
	  if (!iccOVERFLOW)
	    out=v2;
	  break;
	case VSb:
	default:
	  if (iccOVERFLOW)
	    out=v2;
	  break;
	}
    }
  else /* use floating point condition codes */
    {
      #define E (v1 == fccEQ)
      #define L (v1 == fccLT)
      #define G (v1 == fccGT)
      #define U (v1 == fccUO)
      
      switch(inst->code->aux2)
	{
	case Af:
	  out=v2;
	  break;
	case Nf:
	  break;
	case Uf:
	  if (U)
	    out=v2;	
	  break;
	case Gf:
	  if (G)
	    out=v2;
	  break;
	case UGf:
	  if (U || G)
	    out=v2;
	  break;
	case Lf:
	  if (L)
	    out=v2;
	  break;
	case ULf:
	  if (U || L)
	    out=v2;
	  break;
	case LGf:
	  if (L || G)
	    out=v2;
	  break;
	case NEf:
	  if (L || G || U)
	    out=v2;
	  break;
	case Ef:
	  if (E)
	    out=v2;
	  break;
	case UEf:
	  if (E || U)
	    out=v2;
	  break;
	case GEf:
	  if (G || E)
	    out=v2;
	  break;
	case UGEf:
	  if (U || G || E)
	    out=v2;
	  break;
	case LEf:
	  if (L||E)
	    out=v2;
	  break;
	case ULEf:
	  if (U||L||E)
	    out=v2;
	  break;
	case Of:
	  if (E||L||G)
	    out=v2;
	  break;
	}
    }
}

#define fnFCMPq fnFCMPd
#define fnFCMPEq fnFCMPEd

void fnFCMPd(instance *inst, state *)
{
  double v1 = inst->rs1valf, v2=inst->rs2valf;
  int& out = inst->rdvali;
  if (v1==v2)
    out=fccEQ;
  else if (v1 < v2)
    out=fccLT;
  else if (v1 > v2)
    out=fccGT;
  else
    out=fccUO;
}

void fnFCMPs(instance *inst, state *)
{
  float v1 = inst->rs1valfh, v2=inst->rs2valfh;
  int& out = inst->rdvali;
  if (v1==v2)
    out=fccEQ;
  else if (v1 < v2)
    out=fccLT;
  else if (v1 > v2)
    out=fccGT;
  else
    out=fccUO;
}

void fnFCMPEd(instance *inst, state *)
{
  double v1 = inst->rs1valf, v2=inst->rs2valf;
  int& out = inst->rdvali;
  if (v1==v2)
    out=fccEQ;
  else if (v1 < v2)
    out=fccLT;
  else if (v1 > v2)
    out=fccGT;
  else
    inst->exception_code=FPERR;
}

void fnFCMPEs(instance *inst, state *)
{
  float v1 = inst->rs1valfh, v2=inst->rs2valfh;
  int& out = inst->rdvali;
  if (v1==v2)
    out=fccEQ;
  else if (v1 < v2)
    out=fccLT;
  else if (v1 > v2)
    out=fccGT;
  else
    inst->exception_code=FPERR;
}


#define fnFMOVRq fnFMOVRd

void fnFMOVRd(instance *inst, state *)
{
  double &out=inst->rdvalf;
  out=inst->rs1valf;
  int v1 = inst->rsccvali;
  double v2 = inst->rs2valf;

  switch (inst->code->aux2)
    {
    case 1:
      if (v1 == 0)
	out = v2;
      break;
    case 2:
      if (v1 <= 0)
	out = v2;
      break;
    case 3:
      if (v1 < 0)
	out = v2;
      break;
    case 5:
      if (v1 != 0)
	out = v2;
      break;
    case 6:
      if (v1 > 0)
	out = v2;
      break;
    case 7:
      if (v1 >= 0)
	out = v2;
      break;
    case 0:
    case 4:
    default:
      break;
    }
}

void fnFMOVRs(instance *inst, state *)
{
  float &out=inst->rdvalfh;
  out=inst->rs1valfh;
  int v1 = inst->rsccvali;
  float v2 = inst->rs2valfh;

  switch (inst->code->aux2)
    {
    case 1:
      if (v1 == 0)
	out = v2;
      break;
    case 2:
      if (v1 <= 0)
	out = v2;
      break;
    case 3:
      if (v1 < 0)
	out = v2;
      break;
    case 5:
      if (v1 != 0)
	out = v2;
      break;
    case 6:
      if (v1 > 0)
	out = v2;
      break;
    case 7:
      if (v1 >= 0)
	out = v2;
      break;
    case 0:
    case 4:
    default:
      break;
    }
}

/************* Memory instructions *************/


#include "Processor/hash.h"

/*************************************************************************/
/* GetMap : gets the UNIX address of the address accessed by memory      */
/*          instruction. If no corresponding address, this access hasn't */
/*          been accessed and is some sort of seg fault                  */
/*************************************************************************/

unsigned GetMap(instance *inst, state *proc)
{
  unsigned pa;
  unsigned addr = inst->addr;
  
  if (addr < lowshared)
    {
      /* look it up in regular page table. If failure, grow the stack */
      if (proc->PageTable.lookup(addr / ALLOC_SIZE,pa))
	{ /* success */
	  return pa+(addr&(ALLOC_SIZE-1));
	}
      else
	{
	  /* failure, mark a violation and try to grow the stack (if
	     applicable) later */
	  if (inst->code->instruction != iPREFETCH)
	    inst->exception_code=SEGV;
	  return 0;
	  
	}
    }
  else
    {
      /* look it up in shared page table. If failure, it's an exception */
      if (SharedPageTable->lookup(addr / ALLOC_SIZE,pa))
	{ /* success */
	  return pa+(addr&(ALLOC_SIZE-1));
	}
      else
	{
	  if (inst->code->instruction != iPREFETCH)
	    inst->exception_code=SEGV;
	  return 0;
	}
    }
}

/****** Template for a wide variety of integer loads  *********/
template <class T> void ldi(instance *inst,state *proc, T t)
{
  unsigned pa=GetMap(inst,proc);
  if (pa)
    {
      T *ptr = (T *)pa; 
      T result = *ptr;
      if (t)
	result = SE(result,sizeof(T)*8);
      inst->rdvali=result;
    }
}

/* Templates for long-long's do not seem to compile with the SC-4.0 for
   SPARC v8, so the long-long version of ldi is specified separately. */

void ldll(instance *inst,state *proc, unsigned long long t)
{
  unsigned pa=GetMap(inst,proc);
  if (pa)
    {
      unsigned long long *ptr = (unsigned long long *)pa; 
      unsigned long long result = *ptr;
      if (t)
	result = SE(result,sizeof(unsigned long long)*8);
      inst->rdvalll=result;
    }
}

/****** Template for a wide variety of FP loads  *********/
template <class T> void ldf(instance *inst,state *proc, T)
{
  unsigned pa=GetMap(inst,proc);
  if (pa)
    {
      T *ptr = (T *)pa;
      T result = *ptr;
      if (sizeof(T) == sizeof(double))
	inst->rdvalf=result; /* double-precision */
      else
	inst->rdvalfh=result; /* single-precision */
    }
}

/****** Template for a wide variety of integer stores  *******/
template <class T> void sti(instance *inst,state *proc, T)
{
  unsigned pa=GetMap(inst,proc);
  if (pa)
    {
      T *ptr = (T *)pa;
      *ptr = inst->rs1vali;
    }
}

/****** Template for a wide variety of FP stores  *******/
template <class T> void stf(instance *inst,state *proc, T)
{
  unsigned pa=GetMap(inst,proc);
  if (pa)
    {
      T *ptr = (T *)pa;
      if (sizeof(T) == sizeof(double))
	*ptr = inst->rs1valf;
      else
	*ptr = inst->rs1valfh;
    }
}

void fnLDUW(instance *inst,state *proc)
{
  ldi(inst,proc,(unsigned)0);
}


void fnLDUB(instance *inst,state *proc)
{
  ldi(inst,proc,(unsigned char)0);
}

void fnLDUH(instance *inst,state *proc)
{
  ldi(inst,proc,(unsigned short)0);
}

void fnLDSW(instance *inst,state *proc)
{
  ldi(inst,proc,(int)0);
}

void fnLDSB(instance *inst,state *proc)
{
  ldi(inst,proc,(char)1);
}

void fnLDSH(instance *inst,state *proc)
{
  ldi(inst,proc,(short)1);
}

void fnLDD(instance *inst,state *proc)
{
  unsigned pa=GetMap(inst,proc);
  if (pa)
    {
      unsigned *ptr = (unsigned *)pa;
      unsigned result = *ptr;
      inst->rdvalipair.a=result;
      ptr++;
      inst->rdvalipair.b=*ptr;
    }
}

void fnLDX(instance *inst,state *proc)
{
  ldi(inst,proc,(unsigned)0);
}

void fnLDF(instance *inst,state *proc)
{
  ldf(inst,proc,(float)0);
}

void fnLDDF(instance *inst,state *proc)
{
  ldf(inst,proc,(double)0);
}

void fnLDQF(instance *inst,state *proc)
{
  ldf(inst,proc,(double)0);
}

void fnSTW(instance *inst,state *proc)
{
  sti(inst,proc,(int)0);
}


void fnSTB(instance *inst,state *proc)
{
  sti(inst,proc,(char)0);
}

void fnSTH(instance *inst,state *proc)
{
  sti(inst,proc,(short)0);
}

void fnSTX(instance *inst,state *proc)
{
  sti(inst,proc,(unsigned)0);
}

void fnSTF(instance *inst,state *proc)
{
  stf(inst,proc,(float)0);
}

void fnSTDF(instance *inst,state *proc)
{
  stf(inst,proc,(double)0);
}

void fnSTQF(instance *inst,state *proc)
{
  stf(inst,proc,(double)0);
}

void fnSTD(instance *inst,state *proc)
{
  unsigned pa=GetMap(inst,proc);
  if (pa)
    {
      unsigned *ptr = (unsigned *)pa;
      *ptr = inst->rs1valipair.a;
      ptr++;
      *ptr = inst->rs1valipair.b;
    }
}

#define fnSTDA fnSTD

void fnLDUWA(instance *inst,state *proc)
{
  ldi(inst,proc,(unsigned)0);
}

void fnLDUBA(instance *inst,state *proc)
{
  ldi(inst,proc,(unsigned char)0);
}

void fnLDUHA(instance *inst,state *proc)
{
  ldi(inst,proc,(unsigned short)0);
}

void fnLDSWA(instance *inst,state *proc)
{
  ldi(inst,proc,(int)0);
}

void fnLDSBA(instance *inst,state *proc)
{
  ldi(inst,proc,(char)1);
}

void fnLDSHA(instance *inst,state *proc)
{
  ldi(inst,proc,(short)1);
}

#define fnLDDA fnLDD

void fnLDXA(instance *inst,state *proc)
{
  ldi(inst,proc,(unsigned)0);
}

void fnLDFA(instance *inst,state *proc)
{
  ldf(inst,proc,(float)0);
}

void fnLDDFA(instance *inst,state *proc)
{
  ldf(inst,proc,(double)0);
}

void fnLDQFA(instance *inst,state *proc)
{
  ldf(inst,proc,(double)0);
}

void fnSTWA(instance *inst,state *proc)
{
  sti(inst,proc,(int)0);
}

/* functions to store destination processor for WriteSend */
void fnRWSD(instance *inst,state *proc)
{
    send_dest[proc->proc_id] = inst->rs1vali; /* store destination processor id */
}

void fnSTBA(instance *inst,state *proc)
{
  sti(inst,proc,(char)0);
}

void fnSTHA(instance *inst,state *proc)
{
  sti(inst,proc,(short)0);
}

void fnSTXA(instance *inst,state *proc)
{
  sti(inst,proc,(unsigned)0);
}

void fnSTFA(instance *inst,state *proc)
{
  stf(inst,proc,(float)0);
}

void fnSTDFA(instance *inst,state *proc)
{
  stf(inst,proc,(double)0);
}

void fnSTQFA(instance *inst,state *proc)
{
  stf(inst,proc,(double)0);
}

void fnLDSTUB(instance *inst, state *proc)
{
  unsigned pa=GetMap(inst,proc);
  if (pa)
    {
      unsigned char *ptr = (unsigned char *)pa;
      unsigned char result = *ptr;
      inst->rdvali=result;
      *ptr=0xff;
    }
}


void fnSWAP(instance *inst, state *proc)
{
  unsigned pa=GetMap(inst,proc);
  if (pa)
    {
      int *ptr = (int *)pa;
      int result = *ptr;
      inst->rdvali=result;
      *ptr = inst->rs1vali;
    }
}

#define fnLDSTUBA fnLDSTUB

#define fnSWAPA fnSWAP

void fnLDFSR(instance *inst, state *proc)
{
  ldi(inst,proc,(unsigned)0);
  inst->exception_code = SERIALIZE;
  /* load value from memory, but also serialize it. If it turns out to be
     a limbo, then let the limbo override the serialize */
}
void fnLDXFSR(instance *inst, state *proc)
{
  ldll(inst,proc,(unsigned long long)0);
  inst->exception_code = SERIALIZE;
  /* load value from memory, but also serialize it. If it turns out to be
     a limbo, then let the limbo override the serialize */
}


void fnSTFSR(instance *inst, state *)
{
  inst->exception_code = SERIALIZE;
}

void fnSTXFSR(instance *inst, state *)
{
  inst->exception_code = SERIALIZE;
}


void fnPREFETCH(instance *, state *)
{
}


void fnCASA(instance *inst, state *proc)
{
  unsigned pa=GetMap(inst,proc);
  if (pa)
    {
      int *ptr = (int *)pa;
      int result = *ptr;
      inst->rdvali=result;
      if (result == inst->rs1vali)
	{
	  /* put the old dest value into the memory location */
	  *ptr=inst->rsccvali;
	}
    }
}

#define fnPREFETCHA fnPREFETCH

/*void fnPREFETCHA(instance *inst, state *proc)
{
}*/

#define fnCASXA fnCASA

/*void fnCASXA(instance *inst, state *proc)
{
}*/


void fnFLUSH(instance *, state *)
{
  /* fprintf(simerr,"empty function\n"); */
  /* just make this silent -- no point doing anything specific. */
}


void fnFLUSHW(instance *inst, state *)
{
  /* fprintf(simerr,"empty function\n"); */
  inst->exception_code = PRIVILEGED; /* not currently supported */
}

void fnTADDcc(instance *inst, state *)
{
  /* fprintf(simerr,"empty function\n"); */
  inst->exception_code = ILLEGAL; /* not currently supported */
}


void fnTSUBcc(instance *inst, state *)
{
  /* fprintf(simerr,"empty function\n"); */
  inst->exception_code = ILLEGAL; /* not currently supported */
}


void fnTADDccTV(instance *inst, state *)
{
  /* fprintf(simerr,"empty function\n"); */
  inst->exception_code = ILLEGAL; /* not currently supported */
}


void fnTSUBccTV(instance *inst, state *)
{
  /* fprintf(simerr,"empty function\n"); */
  inst->exception_code = ILLEGAL; /* not currently supported */
}



void FuncTableSetup()
{
  instr_func[iRESERVED]=fnRESERVED;
  instr_func[iILLTRAP]=fnILLTRAP;

  instr_func[iCALL]=fnCALL;
  instr_func[iJMPL]=fnJMPL;
  instr_func[iRETURN]=fnRETURN;
  instr_func[iTcc]=fnTcc;
  instr_func[iDONERETRY]=fnDONERETRY;

  instr_func[iBPcc]=fnBPcc;
  instr_func[iBicc]=fnBicc;
  instr_func[iBPr]=fnBPr;
  instr_func[iFBPfcc]=fnFBPfcc;
  instr_func[iFBfcc]=fnFBfcc;
  
  instr_func[iSETHI]=fnSETHI;
  instr_func[iADD]=fnADD;
  instr_func[iAND]=fnAND;
  instr_func[iOR]=fnOR;
  instr_func[iXOR]=fnXOR;
  instr_func[iSUB]=fnSUB;
  instr_func[iANDN]=fnANDN;
  instr_func[iORN]=fnORN;
  instr_func[iXNOR]=fnXNOR;
  instr_func[iADDC]=fnADDC;
  
  instr_func[iMULX]=fnMULX;
  instr_func[iUMUL]=fnUMUL;
  instr_func[iSMUL]=fnSMUL;
  instr_func[iUDIVX]=fnUDIVX;
  instr_func[iUDIV]=fnUDIV;
  instr_func[iSDIV]=fnSDIV;
  
  instr_func[iSUBC]=fnSUBC;
  instr_func[iADDcc]=fnADDcc;
  instr_func[iANDcc]=fnANDcc;
  instr_func[iORcc]=fnORcc;
  instr_func[iXORcc]=fnXORcc;
  
  instr_func[iSUBcc]=fnSUBcc;
  instr_func[iANDNcc]=fnANDNcc;
  instr_func[iORNcc]=fnORNcc;
  instr_func[iXNORcc]=fnXNORcc;
  instr_func[iADDCcc]=fnADDCcc;
  
  instr_func[iUMULcc]=fnUMULcc;
  instr_func[iSMULcc]=fnSMULcc;
  instr_func[iUDIVcc]=fnUDIVcc;
  instr_func[iSDIVcc]=fnSDIVcc;
  
  instr_func[iSUBCcc]=fnSUBCcc;
  instr_func[iTADDcc]=fnTADDcc;
  instr_func[iTSUBcc]=fnTSUBcc;
  instr_func[iTADDccTV]=fnTADDccTV;
  instr_func[iTSUBccTV]=fnTSUBccTV;
  instr_func[iMULScc]=fnMULScc;
  
  instr_func[iSLL]=fnSLL;
  instr_func[iSRL]=fnSRL;
  instr_func[iSRA]=fnSRA;
  instr_func[iarithSPECIAL1]=fnarithSPECIAL1;
  
  instr_func[iRDPR]=fnRDPR;
  instr_func[iMOVcc]=fnMOVcc;
  instr_func[iSDIVX]=fnSDIVX;
  instr_func[iPOPC]=fnPOPC;
  instr_func[iMOVR]=fnMOVR;
  instr_func[iarithSPECIAL2]=fnarithSPECIAL2;
  
  instr_func[iSAVRESTD]=fnSAVRESTD;
  instr_func[iWRPR]=fnWRPR;
  instr_func[iIMPDEP1]=fnIMPDEP1;
  instr_func[iIMPDEP2]=fnIMPDEP2;
  instr_func[iSAVE]=fnSAVE;
  instr_func[iRESTORE]=fnRESTORE;
  
  instr_func[iFMOVs]=fnFMOVs;
  instr_func[iFMOVd]=fnFMOVd;
  instr_func[iFMOVq]=fnFMOVq;

  instr_func[iFNEGs]=fnFNEGs;
  instr_func[iFNEGd]=fnFNEGd;
  instr_func[iFNEGq]=fnFNEGq;
  instr_func[iFABSs]=fnFABSs;
  instr_func[iFABSd]=fnFABSd;
  instr_func[iFABSq]=fnFABSq;
  instr_func[iFSQRTs]=fnFSQRTs;
  
  instr_func[iFSQRTd]=fnFSQRTd;
  instr_func[iFSQRTq]=fnFSQRTq;
  instr_func[iFADDs]=fnFADDs;
  instr_func[iFADDd]=fnFADDd;
  instr_func[iFADDq]=fnFADDq;
  instr_func[iFSUBs]=fnFSUBs;
  instr_func[iFSUBd]=fnFSUBd;
  instr_func[iFSUBq]=fnFSUBq;
  
  instr_func[iFMULs]=fnFMULs;
  instr_func[iFMULd]=fnFMULd;
  instr_func[iFMULq]=fnFMULq;
  instr_func[iFDIVs]=fnFDIVs;
  instr_func[iFDIVd]=fnFDIVd;
  instr_func[iFDIVq]=fnFDIVq;
  instr_func[iFsMULd]=fnFsMULd;
  instr_func[iFdMULq]=fnFdMULq;
  
  instr_func[iFsTOx]=fnFsTOx;
  instr_func[iFdTOx]=fnFdTOx;
  instr_func[iFqTOx]=fnFqTOx;
  instr_func[iFxTOs]=fnFxTOs;
  instr_func[iFxTOd]=fnFxTOd;
  instr_func[iFxToq]=fnFxToq;
  instr_func[iFiTOs]=fnFiTOs;
  instr_func[iFdTOs]=fnFdTOs;
  
  instr_func[iFqTOs]=fnFqTOs;
  instr_func[iFiTOd]=fnFiTOd;
  instr_func[iFsTOd]=fnFsTOd;
  instr_func[iFqTOd]=fnFqTOd;
  instr_func[iFiTOq]=fnFiTOq;
  instr_func[iFsTOq]=fnFsTOq;
  instr_func[iFdTOq]=fnFdTOq;
  instr_func[iFsTOi]=fnFsTOi;
  
  instr_func[iFdTOi]=fnFdTOi;
  instr_func[iFqTOi]=fnFqTOi;
  instr_func[iFMOVs0]=fnFMOVs0;
  instr_func[iFMOVd0]=fnFMOVd0;
  instr_func[iFMOVq0]=fnFMOVq0;
  instr_func[iFMOVs1]=fnFMOVs0;
  instr_func[iFMOVd1]=fnFMOVd0;
  instr_func[iFMOVq1]=fnFMOVq0;
  
  instr_func[iFMOVs2]=fnFMOVs0;
  instr_func[iFMOVd2]=fnFMOVd0;
  instr_func[iFMOVq2]=fnFMOVq0;
  instr_func[iFMOVs3]=fnFMOVs0;
  instr_func[iFMOVd3]=fnFMOVd0;
  instr_func[iFMOVq3]=fnFMOVq0;
  instr_func[iFMOVsi]=fnFMOVs0;
  
  instr_func[iFMOVdi]=fnFMOVd0;
  instr_func[iFMOVqi]=fnFMOVq0;
  instr_func[iFMOVsx]=fnFMOVs0;
  instr_func[iFMOVdx]=fnFMOVd0;
  instr_func[iFMOVqx]=fnFMOVq0;
  instr_func[iFCMPs]=fnFCMPs;
  instr_func[iFCMPd]=fnFCMPd;
  instr_func[iFCMPq]=fnFCMPq;
  
  instr_func[iFCMPEs]=fnFCMPEs;
  instr_func[iFCMPEd]=fnFCMPEd;
  instr_func[iFCMPEq]=fnFCMPEq;
  instr_func[iFMOVRsZ]=fnFMOVRs;
  instr_func[iFMOVRdZ]=fnFMOVRd;
  instr_func[iFMOVRqZ]=fnFMOVRq;
  instr_func[iFMOVRsLEZ]=fnFMOVRs;
  
  instr_func[iFMOVRdLEZ]=fnFMOVRd;
  instr_func[iFMOVRqLEZ]=fnFMOVRq;
  instr_func[iFMOVRsLZ]=fnFMOVRs;
  instr_func[iFMOVRdLZ]=fnFMOVRd;
  instr_func[iFMOVRqLZ]=fnFMOVRq;
  instr_func[iFMOVRsNZ]=fnFMOVRs;
  
  instr_func[iFMOVRdNZ]=fnFMOVRd;
  instr_func[iFMOVRqNZ]=fnFMOVRq;
  instr_func[iFMOVRsGZ]=fnFMOVRs;
  instr_func[iFMOVRdGZ]=fnFMOVRd;
  instr_func[iFMOVRqGZ]=fnFMOVRq;
  instr_func[iFMOVRsGEZ]=fnFMOVRs;
  
  instr_func[iFMOVRdGEZ]=fnFMOVRd;
  instr_func[iFMOVRqGEZ]=fnFMOVRq;


  instr_func[iLDUW]=fnLDUW;
  instr_func[iLDUB]=fnLDUB;
  instr_func[iLDUH]=fnLDUH;
  instr_func[iLDD]=fnLDD;
  instr_func[iSTW]=fnSTW;
  instr_func[iSTB]=fnSTB;
  instr_func[iSTH]=fnSTH;
  
  instr_func[iSTD]=fnSTD;
  instr_func[iLDSW]=fnLDSW;
  instr_func[iLDSB]=fnLDSB;
  instr_func[iLDSH]=fnLDSH;
  instr_func[iLDX]=fnLDX;
  instr_func[iLDSTUB]=fnLDSTUB;
  instr_func[iSTX]=fnSTX;
  instr_func[iSWAP]=fnSWAP;
  instr_func[iLDUWA]=fnLDUWA;
  instr_func[iLDUBA]=fnLDUBA;
  
  instr_func[iLDUHA]=fnLDUHA;
  instr_func[iLDDA]=fnLDDA;
  instr_func[iSTWA]=fnSTWA;
  instr_func[iSTBA]=fnSTBA;
  instr_func[iSTHA]=fnSTHA;
  instr_func[iSTDA]=fnSTDA;
  instr_func[iLDSWA]=fnLDSWA;
  instr_func[iLDSBA]=fnLDSBA;
  instr_func[iLDSHA]=fnLDSHA;
  
  instr_func[iLDXA]=fnLDXA;
  instr_func[iLDSTUBA]=fnLDSTUBA;
  instr_func[iSTXA]=fnSTXA;
  instr_func[iSWAPA]=fnSWAPA;
  instr_func[iLDF]=fnLDF;
  instr_func[iLDFSR]=fnLDFSR;
  instr_func[iLDXFSR]=fnLDXFSR;
  instr_func[iLDQF]=fnLDQF;
  instr_func[iLDDF]=fnLDDF;
  instr_func[iSTF]=fnSTF;
  
  instr_func[iSTFSR]=fnSTFSR;
  instr_func[iSTXFSR]=fnSTXFSR;
  instr_func[iSTQF]=fnSTQF;
  instr_func[iSTDF]=fnSTDF;
  instr_func[iPREFETCH]=fnPREFETCH;
  instr_func[iLDFA]=fnLDFA;
  instr_func[iLDQFA]=fnLDQFA;
  instr_func[iLDDFA]=fnLDDFA;
  instr_func[iSTFA]=fnSTFA;
  instr_func[iSTQFA]=fnSTQFA;
  
  instr_func[iSTDFA]=fnSTDFA;
  instr_func[iCASA]=fnCASA;
  instr_func[iPREFETCHA]=fnPREFETCHA;
  instr_func[iCASXA]=fnCASXA;
  instr_func[iFLUSH]=fnFLUSH;
  instr_func[iFLUSHW]=fnFLUSHW;

/* REMOTE WRITE RELATED FUNCTIONS */
  instr_func[iRWWT_I] =fnSTW;	
  instr_func[iRWWTI_I]=fnSTW;	
  instr_func[iRWWS_I] =fnSTW;	
  instr_func[iRWWSI_I]=fnSTW;

  instr_func[iRWWT_F] =fnSTDF;
  instr_func[iRWWTI_F]=fnSTDF;
  instr_func[iRWWS_F] =fnSTDF;
  instr_func[iRWWSI_F]=fnSTDF;
  instr_func[iRWSD] = fnRWSD;

}

