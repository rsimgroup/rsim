/* predecode_instr.cc

   Code for the predecode part of RSIM -- splits up each tightly-encoded
   SPARC instruction into a loosely-encoded RSIM instruction.

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


#include "Processor/sync_asis.h"

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

/* NOTE: The X  condition code, but will be part of the Icondition code

   0 -- fcc0
   1 -- fcc1
   2 -- fcc2
   3 -- fcc3
 4-7 -- icc (default)

   */

#include "Processor/instruction.h"
#include "Processor/table.h"
#include "Processor/archregnums.h"
#include <stdio.h>

int call_instr(instr *in, unsigned undec)
{
  in->rd = 15; /* gets pc value of this instruction */
  in->imm = Extract(undec,29,0);
  in->imm = SE(in->imm,30); /* sign extend it */
  in->instruction = iCALL;
  in->uncond_branch = 3; /* take unconditionally plus add in RAS */
  return 1;
}

int arith_instr(instr *in, unsigned undec)
{
  IMF fp;
  int tmp = Extract(undec,24,19);
  in->instruction = iarithop[tmp];
  fp=arithop[tmp];
  return (*fp)(in,undec);
}

int branch_instr(instr *in, unsigned undec)
{
  IMF fp;
  int tmp = Extract(undec,24,22);
  in->instruction=ibrop2[tmp];
  fp=brop2[tmp];
  return (*fp)(in,undec);
}

int illtrap(instr *in, unsigned undec)
{
  in->rs1=8; /* o0 */
  in->rd=8; /* o0 */
  in->rs2=0; /* f0 */
  in->rs2_regtype=REG_FP;
  in->aux1=Extract(undec,29,25);
  in->aux2=Extract(undec,21,0);
  /* We'll use all these different illegal instructions for all the functions we need */

  return 1;
}

int bpcc(instr *in, unsigned undec)
{
  int cond;
  in->rs1=COND_REGISTERS+ 4 /* + Extract(undec,21,20) */; 
  in->imm=SE(Extract(undec,18,0),19);
  cond=in->aux1=Extract(undec,28,25);
  in->annul=Extract(undec,29,29);
  in->taken=Extract(undec,19,19);
  if (cond == 8) /* branch always */
    {
      in->uncond_branch=2;
      in->rs1=0;
    }
  else // note: the "bn" is used as an instruction prefetch, so we should treat it as a conditional that we intentionally mispredict
    {
      in->cond_branch=1;
    }

  return 1;
}

int fbpfcc(instr *in, unsigned undec)
{
  int cond;
  in->rs1=COND_REGISTERS+ Extract(undec,21,20);
  in->imm=SE(Extract(undec,18,0),19);
  cond=in->aux1=Extract(undec,28,25);
  if (cond == 8) /* branch always */
    {
      in->uncond_branch=2;
      in->rs1=0;
    }
  else
    {
      in->cond_branch=1;
    }
  
  in->annul=Extract(undec,29,29);
  in->taken=Extract(undec,19,19);

  return 1;
}

int fbfcc(instr *in, unsigned undec)
{
  in->rs1=COND_REGISTERS;
  in->imm=SE(Extract(undec,21,0),22);
  in->taken = (in->imm < 0); /* a backward branch */
  int cond;
  cond=in->aux1=Extract(undec,28,25);
  in->annul=Extract(undec,29,29);
  if (cond == 8) /* branch always */
    {
      in->uncond_branch=2;
      in->rs1=0;
    }
  else
    {
      in->cond_branch=1;
    }
  
  return 1;
}

int sethi(instr *in,unsigned undec)
{
  in->imm=Extract(undec,21,0);
  in->rd=Extract(undec,29,25);

  return 1;
}

int bpr(instr *in, unsigned undec)
{
  in->rs1=Extract(undec,18,14);
  in->imm=SE((Extract(undec,21,20) << 14)+Extract(undec,13,0),16);
  in->aux1=Extract(undec,27,25);
  in->annul=Extract(undec,29,29);
  in->taken=Extract(undec,19,19);
  in->cond_branch=1;

  return 1;
}

int bicc(instr *in, unsigned undec)
{
  in->rs1=COND_REGISTERS+ 4; /* use icc */
  in->imm=SE(Extract(undec,21,0),22);
  in->taken = (in->imm < 0); /* a backward branch */
  int cond;
  cond=in->aux1=Extract(undec,28,25);
  in->annul=Extract(undec,29,29);
  if (cond == 8) /* branch always */
    {
      in->uncond_branch=2;
      in->rs1=0;
    }
  else
    {
      in->cond_branch=1;
    }

  return 1;
}

int brres(instr *in, unsigned undec)
{
  /* fprintf(stderr,"Reserved branch instruction encountered\n"); */

  return 1;
}

int arith_res(instr *in, unsigned undec)
{
  /* fprintf(stderr,"Reserved arithmetic instruction encountered\n"); */
  return 1;
}

int arith_3(instr *in, unsigned undec)
{
  in->rd=Extract(undec,29,25);
  in->rs1=Extract(undec,18,14);
  if ((in->aux1 = Extract(undec,13,13)))
    {
      /* it's an immediate */
      in->imm=SE(Extract(undec,12,0),13);
    }
  else
    {
      in->rs2=Extract(undec,4,0);
    }

  return 1;
}

int arith_3y(instr *in, unsigned undec)
{
  /* NOTE: several instructions must be handled here, and handled
     differently in each case.

     All of them take rs1 and rs2/imm inputs and have an rd output.
     The differences are with regard to ICC and Y
     
     SMUL/UMUL -- Y is an output.
     SMULcc/UMULcc -- Y and ICC are outputs.
     SDIV/UDIV -- Y is an input
     SDIVcc/UDIVcc -- Y is an input, ICC is an output
     MULScc -- Y is an input. ICC is an input.
               Y is an output. ICC is an output.

     Everything can be popped into the normal framework except
     for SMULcc, UMULcc, and MULScc. Either have a super-robust renaming
     structure or serialize those instructions -- anyway, they're
     all deprecated in the SPARC V9 (actually all of these are, but some
     will be needed for 32 bit code).

     In our implementation, we'll serialize the ones we don't like. */
     
  in->rd=Extract(undec,29,25);
  in->rs1=Extract(undec,18,14);
  if ((in->aux1 = Extract(undec,13,13)))
    {
      /* it's an immediate */
      in->imm=SE(Extract(undec,12,0),13);
    }
  else
    {
      in->rs2=Extract(undec,4,0);
    }

  if (in->instruction == iSMUL || in->instruction == iUMUL)
    {
      in->rcc = STATE_Y;
    }
  if (in->instruction == iSDIV || in->instruction == iUDIV || in->instruction == SDIVcc || in->instruction == UDIVcc)
    {
      in->rscc = STATE_Y;
    }
  if (in->instruction == SDIVcc || in->instruction == UDIVcc)
    {
      in->rcc = COND_ICC;
    }
  
  return 1;
}

int arith_3sc(instr *in, unsigned undec)
{
  in->rd=Extract(undec,29,25);
  in->rs1=Extract(undec,18,14);
  in->rscc=COND_REGISTERS + 4;
  if ((in->aux1 = Extract(undec,13,13)))
    {
      /* it's an immediate */
      in->imm=SE(Extract(undec,12,0),13);
    }
  else
    {
      in->rs2=Extract(undec,4,0);
    }

  return 1;
}

int sarith_3(instr *in, unsigned undec)
{
  in->wpchange=WPC_SAVE;
  in->rd=Extract(undec,29,25);
  in->rs1=Extract(undec,18,14);
  if ((in->aux1 = Extract(undec,13,13)))
    {
      /* it's an immediate */
      in->imm=SE(Extract(undec,12,0),13);
    }
  else
    {
      in->rs2=Extract(undec,4,0);
    }

  return 1;
}

int rarith_3(instr *in, unsigned undec)
{
  in->wpchange=WPC_RESTORE;
  in->rd=Extract(undec,29,25);
  in->rs1=Extract(undec,18,14);
  if ((in->aux1 = Extract(undec,13,13)))
    {
      /* it's an immediate */
      in->imm=SE(Extract(undec,12,0),13);
    }
  else
    {
      in->rs2=Extract(undec,4,0);
    }

  return 1;
}

int arith_3cc(instr *in, unsigned undec)
{
  in->rd=Extract(undec,29,25);
  in->rs1=Extract(undec,18,14);
  if ((in->aux1 = Extract(undec,13,13)))
    {
      /* it's an immediate */
      in->imm=SE(Extract(undec,12,0),13);
    }
  else
    {
      in->rs2=Extract(undec,4,0);
    }

  /* NOTE : ANY op that writes integer rcc writes _BOTH_ icc and xcc */
  in->rcc=COND_REGISTERS+4;

  return 1;
}

int arith_3sccc(instr *in, unsigned undec)
{
  in->rd=Extract(undec,29,25);
  in->rs1=Extract(undec,18,14);
  in->rscc=COND_REGISTERS + 4;
  if ((in->aux1 = Extract(undec,13,13)))
    {
      /* it's an immediate */
      in->imm=SE(Extract(undec,12,0),13);
    }
  else
    {
      in->rs2=Extract(undec,4,0);
    }

  /* NOTE : ANY op that writes integer rcc writes _BOTH_ icc and xcc */
  in->rcc=COND_REGISTERS+4;

  return 1;
}

int jmpl(instr *in, unsigned undec)
{
  in->rd=Extract(undec,29,25);
  in->rs1=Extract(undec,18,14);
  if ((in->aux1 = Extract(undec,13,13)))
    {
      /* it's an immediate */
      in->imm=SE(Extract(undec,12,0),13);
    }
  else
    {
      in->rs2=Extract(undec,4,0);
    }
  if (in->imm == 8 && (in->rs1 == 15 || in->rs1 == 31))
    in->uncond_branch = 4; /* return, so check RAS */
  else
    in->uncond_branch=1; /* random jump */

  return 1;
}

int ret(instr *in, unsigned undec) /* for RETURN instruction */
{
  in->rs1=Extract(undec,18,14);
  if ((in->aux1 = Extract(undec,13,13)))
    {
      /* it's an immediate */
      in->imm=SE(Extract(undec,12,0),13);
    }
  else
    {
      in->rs2=Extract(undec,4,0);
    }
  in->uncond_branch=4; /* returns should always be predicted using RAS */
  in->wpchange=WPC_RESTORE;

  return 1;
}

int flush(instr *in, unsigned undec)
{
  /* fprintf(stderr,"flush encountered\n"); */
  in->rs1=Extract(undec,18,14);
  if ((in->aux1 = Extract(undec,13,13)))
    {
      /* it's an immediate */
      in->imm=SE(Extract(undec,12,0),13);
    }
  else
    {
      in->rs2=Extract(undec,4,0);
    }

  return 1;
}

/********** JMPL, RETURN WILL BE MESSY FOR US BECAUSE IT USES NON-RELATIVE
  CONTROL TRANSFER!!!! THIS IS A BIG PROBLEM SINCE ALL "RET"s are
  actually "JMPL", as well as many "CALL"s ********/

int arith_shift(instr *in, unsigned undec)
{
  in->rd=Extract(undec,29,25);
  in->rs1=Extract(undec,18,14);

  switch (in->aux1 = Extract(undec,13,12))
    {
    case 3:
      /* extended */
      in->imm=Extract(undec,5,0);
      break;
    case 2:
      in->imm=Extract(undec,4,0);
      break;
    case 1:
    default:
      in->rs2=Extract(undec,4,0);
      break;
    }

  return 1;
}

int arith_spec1(instr *in, unsigned undec)
{
  /* NOTE: arith_spec1 can either be read of some state register or
     a MEMBAR. It's a MEMBAR if the register specified is #15.

     Here is the register mapping:
     rs1               register type
     0                 Y register (for old mul/div ops)
     1		       reserved
     2		       Condition Codes Reg (xcc/icc)
     3		       ASI reg
     4		       Tick register
     5		       PC
     6		       FP registers status register
     7-14	       ancillary state regs (reserved)
     15		       STBAR/MEMBAR
     16-31	       imp-dep
     */
     

  in->rd = Extract(undec,29,25);
  in->rs1 = STATE_REGISTERS + Extract(undec,18,14); /* this is all for read state register */
  if (in->rs1 == STATE_MEMBAR)
    {
      /* this is special... treat it as such */
      if (Extract(undec,13,13)) /* MEMBAR */
	{
	  in->aux2=Extract(undec,3,0);
	  in->aux1=Extract(undec,6,4);
	  /* Bits 6-4 includes "instruction issue barrier" (6),
	     "memory issue barrier" (5),
	     and "lookaside barrier" (4, prior stores must complete before
	     subsequent loads to same address can be initiated: so, no fwds
	     from memq, etc.) --
	     we currently only implement #5 (and only in RC) -- all these
	     cases are basically invisible to the user */
	}
      else /* STBAR */
	{
	  in->aux2=8; /* STORE-STORE only */
	  in->aux1=0;
	}
    }
  else if (in->rs1 == STATE_CCR)
    {
      in->rs1 = COND_ICC; /* they're the same thing... */
    }
  
  return 1;
}

int arith_spec2(instr *in, unsigned undec)
{
  /* NOTE: arith_spec1 can either be write of some state register or
     a software-initiated reset.

     Here is the register mapping:
     rd                register type
     0                 Y register (for old mul/div ops)
     1		       reserved
     2		       Condition Codes Reg (xcc/icc)
     3		       ASI reg
     4,5	       Ancillary state registers (reserved)
     6		       FP registers status register
     7-14	       Ancillary state regs (reserved)
     15		       Software initiated reset
     16-31	       imp-dep
     */
     
  in->rd = STATE_REGISTERS + Extract(undec,29,25);  /* this is all for write state register */
  in->rs1 = Extract(undec,18,14);
  if (in->rd == STATE_CCR)
    {
      in->rd = COND_ICC; /* they're the same thing... */
    }
  /* we will implement the other specialties later.... */
  return 1;
}

int rdpr(instr *in, unsigned undec)
{
  /* fprintf(stderr,"rdpr encountered\n"); */
  in->rd = Extract(undec,29,25);
  in->rs1 = PRIV_REGISTERS + Extract(undec,18,14); /* this is all for read priv register */

  return 1;
}

int wrpr(instr *in, unsigned undec)
{
  /* fprintf(stderr,"wrpr encountered\n"); */
  in->rd = Extract(undec,29,25);
  in->rs1 = PRIV_REGISTERS + Extract(undec,18,14); /* this is all for write priv register */
  if ((in->aux1 = Extract(undec,13,13)))
    {
      in->imm = SE(Extract(undec,12,0),13);
    }
  else
    {
      in->rs2 = Extract(undec,4,0);
    }

  return 1;
}

int flushw(instr *in, unsigned undec)
{
  /* fprintf(stderr,"Flush w encountered\n"); */
  return 1;
}

int fp_op1(instr *in, unsigned undec)
{
  IMF fp;
  int tmp=Extract(undec,13,5);
  fp=fpop1[tmp];
  in->instruction=ifpop1[tmp];
  return (*fp)(in,undec);
}

int fp_op2(instr *in, unsigned undec)
{
  IMF fp;
  int tmp = Extract(undec,13,5);  
  fp=fpop2[tmp];
  in->instruction=ifpop2[tmp];
  return (*fp)(in,undec);
}

int fp_3(instr *in, unsigned undec)
{
  in->rd =Extract(undec,29,25);
  in->rs1 = Extract(undec,18,14);
  in->rs2 = Extract(undec,4,0);
  in->rd_regtype =REG_FP;
  in->rs1_regtype =REG_FP;
  in->rs2_regtype =REG_FP;

  return 1;
}

int fp_3s(instr *in, unsigned undec)
{
  in->rd =Extract(undec,29,25);
  in->rs1 = Extract(undec,18,14);
  in->rs2 = Extract(undec,4,0);
  in->rd_regtype =REG_FPHALF;
  in->rs1_regtype =REG_FPHALF;
  in->rs2_regtype =REG_FPHALF;

  return 1;
}

int fp_3sd(instr *in, unsigned undec)
{
  in->rd =Extract(undec,29,25);
  in->rs1 = Extract(undec,18,14);
  in->rs2 = Extract(undec,4,0);
  in->rd_regtype =REG_FP;
  in->rs1_regtype =REG_FPHALF;
  in->rs2_regtype =REG_FPHALF;

  return 1;
}

int fmovrcc(instr *in, unsigned undec)
{
  in->rs1=in->rd =Extract(undec,29,25);
  in->rscc = Extract(undec,18,14);
  in->rs2 = Extract(undec,4,0);
  in->aux1 = Extract(undec,12,10);
  in->rd_regtype =REG_FP;
  in->rs2_regtype =REG_FP;
  in->rs1_regtype =REG_FP;

  return 1;
}

int fmovrccs(instr *in, unsigned undec)
{
  in->rs1=in->rd =Extract(undec,29,25);
  in->rscc = Extract(undec,18,14);
  in->rs2 = Extract(undec,4,0);
  in->aux1 = Extract(undec,12,10);
  in->rd_regtype =REG_FPHALF;
  in->rs2_regtype =REG_FPHALF;
  in->rs1_regtype =REG_FPHALF;

  return 1;
}

int fcmp(instr *in, unsigned undec)
{
  in->rd = COND_REGISTERS+Extract(undec,26,25);
  in->rs1 = Extract(undec,18,14);
  in->rs2 = Extract(undec,4,0);
  in->rs1_regtype =REG_FP;
  in->rs2_regtype =REG_FP;

  return 1;
}

int fcmps(instr *in, unsigned undec)
{
  in->rd = COND_REGISTERS+Extract(undec,26,25);
  in->rs1 = Extract(undec,18,14);
  in->rs2 = Extract(undec,4,0);
  in->rs1_regtype =REG_FPHALF;
  in->rs2_regtype =REG_FPHALF;

  return 1;
}

int fp_2(instr *in, unsigned undec)
{
  in->rd =Extract(undec,29,25);
  in->rs2 = Extract(undec,4,0);
  in->rd_regtype =REG_FP;
  in->rs2_regtype =REG_FP;

  return 1;
}

int fp_2s(instr *in, unsigned undec)
{
  in->rd =Extract(undec,29,25);
  in->rs2 = Extract(undec,4,0);
  in->rd_regtype =REG_FPHALF;
  in->rs2_regtype =REG_FPHALF;

  return 1;
}

int fp_2sd(instr *in, unsigned undec)
{
  in->rd =Extract(undec,29,25);
  in->rs2 = Extract(undec,4,0);
  in->rd_regtype =REG_FP;
  in->rs2_regtype =REG_FPHALF;

  return 1;
}

int fp_2ds(instr *in, unsigned undec)
{
  in->rd =Extract(undec,29,25);
  in->rs2 = Extract(undec,4,0);
  in->rd_regtype =REG_FPHALF;
  in->rs2_regtype =REG_FP;

  return 1;
}


int mem_instr(instr *in, unsigned undec)
{
  IMF fp;
  int tmp=Extract(undec,24,19);
  fp=memop3[tmp];
  in->instruction=imemop3[tmp];
  return (*fp)(in,undec);
}

int mem_op2(instr *in, unsigned undec)
{
  in->rd=Extract(undec,29,25);
  in->rs2=Extract(undec,18,14);
  if (Extract(undec,13,13))
    {
      /* immediate */
      in->aux1 = 1;
      in->imm = SE(Extract(undec,12,0),13);
    }
  else
    {
      in->aux1=0;
      in->rscc =Extract(undec,4,0);
    }

  return 1;
}

int dmem_op2(instr *in, unsigned undec)
{
  in->rd=Extract(undec,29,25);
  // in->rcc=in->rd+1;
  in->rd_regtype = REG_INTPAIR;
  in->rs2=Extract(undec,18,14);
  if (Extract(undec,13,13))
    {
      /* immediate */
      in->aux1 = 1;
      in->imm = SE(Extract(undec,12,0),13);
    }
  else
    {
      in->aux1=0;
      in->rscc =Extract(undec,4,0);
    }

  return 1;
}

int mem_op2f(instr *in, unsigned undec)
{
  in->rd=Extract(undec,29,25);
  in->rd_regtype=REG_FP;
  in->rs2=Extract(undec,18,14);
  if (Extract(undec,13,13))
    {
      /* immediate */
      in->aux1 = 1;
      in->imm = SE(Extract(undec,12,0),13);
    }
  else
    {
      in->aux1=0;
      in->rscc =Extract(undec,4,0);
    }

  return 1;
}

int mem_op2fsr(instr *in, unsigned undec)
{
  int fsr;
  in->rd = 0; /* no "basic" register used; only FSR */
  fsr=Extract(undec,29,25); /* if 0: FSR; if 1: XFSR */
  if (fsr == 0) // FSR
    {
      in->rd_regtype=REG_INT;
    }
  else if (fsr == 1)
    {
      in->rd_regtype = REG_INT64;
      in->instruction = iLDXFSR; /* override default */
    }
  else
    {
      /* neither an FSR or XFSR ... just let it be */
    }
  in->rs2=Extract(undec,18,14);
  if (Extract(undec,13,13))
    {
      /* immediate */
      in->aux1 = 1;
      in->imm = SE(Extract(undec,12,0),13);
    }
  else
    {
      in->aux1=0;
      in->rscc =Extract(undec,4,0);
    }

  return 1;
}

int mem_op2fs(instr *in, unsigned undec)
{
  mem_op2f(in,undec);
  in->rd_regtype=REG_FPHALF;
  return 1;
}

int pref(instr *in, unsigned undec)
{
  in->aux2=Extract(undec,29,25);
  in->rs2=Extract(undec,18,14);
  if (Extract(undec,13,13))
    {
      /* immediate */
      in->aux1 = 1;
      in->imm = SE(Extract(undec,12,0),13);
    }
  else
    {
      in->aux1=0;
      in->rscc =Extract(undec,4,0);
    }

  return 1;
}

int apref(instr *in, unsigned undec)
{
  in->aux2=Extract(undec,29,25);
  in->rs2=Extract(undec,18,14);
  if (Extract(undec,13,13))
    {
      /* immediate */
      in->aux1 = 1;
      in->imm = SE(Extract(undec,12,0),13);
    }
  else
    {
      in->aux1=0;
      in->rscc =Extract(undec,4,0);
      in->imm=Extract(undec,12,5);
    }

  return 1;
}

int amem_op2(instr *in, unsigned undec)
{
  in->rd=Extract(undec,29,25);
  in->rs2=Extract(undec,18,14);
  if (Extract(undec,13,13))
    {
      /* immediate */
      in->aux1 = 1;
      in->imm = SE(Extract(undec,12,0),13);
    }
  else
    {
      in->aux1=0;
      in->rscc =Extract(undec,4,0);
    }

  return 1;
}

int damem_op2(instr *in, unsigned undec)
{
  in->rd=Extract(undec,29,25);
  // in->rcc=in->rd+1;
  in->rd_regtype = REG_INTPAIR;
  in->rs2=Extract(undec,18,14);
  if (Extract(undec,13,13))
    {
      /* immediate */
      in->aux1 = 1;
      in->imm = SE(Extract(undec,12,0),13);
    }
  else
    {
      in->aux1=0;
      in->rscc =Extract(undec,4,0);
    }

  return 1;
}

int amem_op2f(instr *in, unsigned undec)
{
  in->rd=Extract(undec,29,25);
  in->rd_regtype=REG_FP;
  in->rs2=Extract(undec,18,14);
  if (Extract(undec,13,13))
    {
      /* immediate */
      in->aux1 = 1;
      in->imm = SE(Extract(undec,12,0),13);
    }
  else
    {
      in->aux1=0;
      in->rscc =Extract(undec,4,0);
    }

  return 1;
}

int amem_op2fs(instr *in, unsigned undec)
{
  amem_op2f(in,undec);
  in->rd_regtype=REG_FPHALF;
  return 1;
}

int cas(instr *in, unsigned undec)
{
  in->rd=Extract(undec,29,25);
  in->rs1=Extract(undec,4,0);
  in->rs2=Extract(undec,18,14);

  in->rscc = in->rd;

  // NOTE: we currently do not support ASI's.
  int t = Extract(undec,12,5);

  return 1;
}

int swap(instr *in, unsigned undec)
{
  in->rd=Extract(undec,29,25);
  in->rs1=Extract(undec,29,25);
  in->rs2=Extract(undec,18,14);
  if (Extract(undec,13,13))
    {
      in->aux1 = 1;
      in->imm = SE(Extract(undec,12,0),13);
    }
  else
    {
      in->aux1=0;
      in->rscc =Extract(undec,4,0);
    }

  return 1;
}

int aswap(instr *in, unsigned undec)
{
  in->rd=Extract(undec,29,25);
  in->rs1=Extract(undec,29,25);
  in->rs2=Extract(undec,18,14);
  if (Extract(undec,13,13))
    {
      in->aux1 = 1;
      in->imm = SE(Extract(undec,12,0),13);
    }
  else
    {
      in->aux1=0;
      in->imm = Extract(undec,12,5);
      in->rscc =Extract(undec,4,0);
    }

  return 1;
}

int smem_op2(instr *in, unsigned undec)
{
  in->rs1=Extract(undec,29,25);
  in->rs2=Extract(undec,18,14);
  if (Extract(undec,13,13))
    {
      /* immediate */
      in->aux1 = 1;
      in->imm = SE(Extract(undec,12,0),13);
    }
  else
    {
      in->aux1=0;
      in->rscc =Extract(undec,4,0);
    }

  return 1;
}

int sdmem_op2(instr *in, unsigned undec)
{
  in->rs1=Extract(undec,29,25);
  in->rs1_regtype = REG_INTPAIR;
  in->rs2=Extract(undec,18,14);
  if (Extract(undec,13,13))
    {
      /* immediate */
      in->aux1 = 1;
      in->imm = SE(Extract(undec,12,0),13);
    }
  else
    {
      in->aux1=0;
      in->rscc =Extract(undec,4,0);
    }

  return 1;
}

int sdamem_op2(instr *in, unsigned undec)
{
  in->rs1=Extract(undec,29,25);
  in->rs1_regtype = REG_INTPAIR;
  in->rs2=Extract(undec,18,14);
  if (Extract(undec,13,13))
    {
      /* immediate */
      in->aux1 = 1;
      in->imm = SE(Extract(undec,12,0),13);
    }
  else
    {
      in->aux1=0;
      in->rscc =Extract(undec,4,0);
      switch(Extract(undec,12,5))
	{
	default:
	  // AN ERROR, if this is actually in the code (likely just a case statement)
	  break;
	} 
    }

  return 1;
}

int smem_op2f(instr *in, unsigned undec)
{
  in->rs1=Extract(undec,29,25);
  in->rs1_regtype=REG_FP;
  in->rs2=Extract(undec,18,14);
  if (Extract(undec,13,13))
    {
      /* immediate */
      in->aux1 = 1;
      in->imm = SE(Extract(undec,12,0),13);
    }
  else
    {
      in->aux1=0;
      in->rscc =Extract(undec,4,0);
    }

  return 1;
}

int smem_op2fsr(instr *in, unsigned undec)
{
  in->rs1 = 0;
  int fsr;
  fsr=Extract(undec,29,25); /* if 0: FSR -- if 1: XFSR */
  if (fsr == 0) // FSR
    {
      in->rs1_regtype=REG_INT;
    }
  else if (fsr == 1)
    {
      in->rs1_regtype = REG_INT64;
      in->instruction = iSTXFSR; /* override default */
    }
  else
    {
      /* neither an FSR or XFSR ... just let it be */
    }
  in->rs2=Extract(undec,18,14);
  if (Extract(undec,13,13))
    {
      /* immediate */
      in->aux1 = 1;
      in->imm = SE(Extract(undec,12,0),13);
    }
  else
    {
      in->aux1=0;
      in->rscc =Extract(undec,4,0);
    }

  return 1;
}

int smem_op2fs(instr *in, unsigned undec)
{
  smem_op2f(in,undec);
  in->rs1_regtype=REG_FPHALF;
  return 1;
}

int samem_op2(instr *in, unsigned undec)
{
  in->rs1=Extract(undec,29,25);
  in->rs2=Extract(undec,18,14);
  if (Extract(undec,13,13))
    {
      /* immediate */
      in->aux1 = 1;
      in->imm = SE(Extract(undec,12,0),13);
    }
  else
    {
      in->aux1=0;
      in->rscc =Extract(undec,4,0);
      switch(Extract(undec,12,5))
	{
	  case ASI_RW_SETDEST:
	    // remote write set processor destination (WriteSend)
		in->instruction = iRWSD;
	    break;
	  case ASI_RW_WT_I:
	    // remote write WriteThrough integer
		in->instruction = iRWWT_I;
	    break;
	  case ASI_RW_WTI_I:
	    // remote write WriteThroughInv integer
		in->instruction = iRWWTI_I;
	    break;
	  case ASI_RW_WS_I:
	    // remote write WriteSend    integer
		in->instruction = iRWWS_I;
	    break;
	  case ASI_RW_WSI_I:
	    // remote write WriteSendInv integer
		in->instruction = iRWWT_I;
	    break;
	default:
	  // AN ERROR, if this is actually in the code (likely just a case statement)
	  break;
	}

    }

  return 1;
}

int samem_op2f(instr *in, unsigned undec)
{
  in->rs1=Extract(undec,29,25);
  in->rs1_regtype=REG_FP;
  in->rs2=Extract(undec,18,14);
  if (Extract(undec,13,13))
    {
      /* immediate */
      in->aux1 = 1;
      in->imm = SE(Extract(undec,12,0),13);
    }
  else
    {
      in->aux1=0;
      in->rscc =Extract(undec,4,0);
      switch(Extract(undec,12,5))
	{
	  case ASI_RW_WT_F:
	    // remote write WriteThrough double 
		in->instruction = iRWWT_F;
	    break;
	  case ASI_RW_WTI_F:
	    // remote write WriteThroughInv double
		in->instruction = iRWWTI_F;
	    break;
	  case ASI_RW_WS_F:
	    // remote write WriteSend double
		in->instruction = iRWWS_F;
	    break;
	  case ASI_RW_WSI_F:
	    // remote write WriteSendInv double
		in->instruction = iRWWT_F;
	    break;
	default:
	  // AN ERROR, if this is actually in the code (likely just a case statement)
	  break;
	}

    }

  return 1;
}

int samem_op2fs(instr *in, unsigned undec)
{
  samem_op2f(in,undec);
  in->rs1_regtype=REG_FPHALF;
  return 1;
}

int mem_res(instr *in, unsigned undec)
{
  /* fprintf(stderr,"Reserved memory instruction encountered\n");  */ /* may appear in case statements, etc. */

  return 1;
}

/* FOR GENERALITY OF IMPLEMENTATION, TREAT CONDITION CODES
   LIKE ANY OTHER LOGICAL REGISTERS */

int movcc(instr *in, unsigned undec)
{
  in->rs1=in->rd=Extract(undec,29,25);
  if ((in->aux1 = Extract(undec,13,13)))
    {
      in->imm=SE(Extract(undec,10,0),11);
    }
  else
    {
      in->rs2=Extract(undec,4,0);
    }
  in->aux2=Extract(undec,17,14);

  /* here rscc represents the condition code used */
  in->rscc=COND_REGISTERS+ 4*Extract(undec,18,18) + Extract(undec,12,11);
  return 1;
}

int fmovcc(instr *in, unsigned undec)
{
  in->rs1=in->rd=Extract(undec,29,25);
  in->rs2=Extract(undec,4,0);
  in->aux1=Extract(undec,17,14);

  /* here rscc represents the condition code used */
  in->rscc=COND_REGISTERS+ Extract(undec,13,11);
  in->rd_regtype =REG_FP;
  in->rs2_regtype =REG_FP;
  in->rs1_regtype =REG_FP;

  return 1;
}

int fmovccs(instr *in, unsigned undec)
{
  in->rs1=in->rd=Extract(undec,29,25);
  in->rs2=Extract(undec,4,0);
  in->aux1=Extract(undec,17,14);

  /* here rscc represents the condition code used */
  in->rscc=COND_REGISTERS+ Extract(undec,13,11);
  in->rd_regtype =REG_FPHALF;
  in->rs2_regtype =REG_FPHALF;
  in->rs1_regtype =REG_FPHALF;

  return 1;
}



int tcc(instr *in, unsigned undec)
{
  in->rs1=Extract(undec,18,14);
  if ((in->aux1 = Extract(undec,13,13)))
    {
      in->imm=Extract(undec,6,0);
    }
  else
    {
      in->rs2=Extract(undec,4,0);
    }
  in->aux2=Extract(undec,28,25);

  return 1;
}

int movr(instr *in, unsigned undec)
{
  in->rd=Extract(undec,29,25);
  in->rs1=Extract(undec,29,25);
  if ((in->aux1 = Extract(undec,13,13)))
    {
      in->imm=SE(Extract(undec,9,0),10);
    }
  else
    {
      in->rs2=Extract(undec,4,0);
    }
  in->aux2=Extract(undec,12,10);

  /* here rs1 represents the condition code used */
  in->rscc=Extract(undec,18,14);
  return 1;
}

int popc(instr *in, unsigned undec)
{
  in->rd=Extract(undec,29,25);
  if ((in->aux1=Extract(undec,13,13)))
    {
      in->imm=SE(Extract(undec,12,0),13);
    }
  else
    {
      in->rs2=Extract(undec,4,0);
    }

  return 1;
}

int savrestd(instr *in, unsigned undec)
{
  in->aux1 = Extract(undec,29,25);

  return 1;
}

int impdep1(instr *in, unsigned undec)
{
  return 1;
}

int impdep2(instr *in, unsigned undec)
{
  /*   fprintf(stderr,"Imp-dep1 encountered\n"); */
  return 1;
}

static IMF starters[4]={branch_instr,call_instr,arith_instr,mem_instr};

int start_decode(instr *in, unsigned undec)
{
  return (*(starters[Extract(undec,31,30)]))(in,undec);
}

