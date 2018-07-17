/*
  branchpred.cc

  Code for the branch prediction hardware

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


#include <stdio.h>
#include <stdlib.h>
#include "Processor/instance.h"
#include "Processor/state.h"
#include "Processor/simio.h"

bptype BPB_TYPE = TWOBIT;
int SZ_BUF = 512;
int RAS_STKSZ = 4;

/*************************************************************************/
/* StartCtlXfer :  predict the direction of control transfer for branch  */
/*************************************************************************/

int StartCtlXfer(instance *inst, state *proc)
     /* the return value is whether or not the branch is taken
	1 = taken , 0 = not taken
	2 = don't know yet, stall future instructions until this one completes
	-1 = taken without speculation (ie unconditional + know addr., not speculative) */
{
  /* this function is called when we first see that an instruction
     is a transfer of control */

  int result = 0;

  if (inst->code->uncond_branch == 2 || inst->code->uncond_branch == 3)
    {
      /* in this case, we know the address we want to go to. There is no
	 question of speculating */
      
      inst->branch_pred=inst->newpc = proc->pc + inst->code->imm;
      inst->taken = -1;

      if (inst->code->uncond_branch == 3)
	proc->RASInsert(inst->pc+2);
      
      return -1;
    }

  /* now we are in the case where we have to speculate
     Either we don't know the address we want to go to
     or we don't know whether or not we want to go there at all */
  
  /* in this case we should start speculating */
  int takeit; /* should we take it ? */

  if (inst->code->uncond_branch == 4) /* it's a return */
    {
      /* predict based on the return address stack */
      result = 1; /* taken speculatively */
      inst->branch_pred = proc->RASPredict();
    }
  else if (inst->code->uncond_branch) /* it's a random jump */
    {
      /* CURRENTLY, WE DON'T PREDICT THIS SORT OF ACCESS */
      result = 2;
      inst->branch_pred = -1;
    }
  else /* a conditional branch */
    {
      if (inst->code->cond_branch && inst->code->aux1 == 0) // it's a "bn" -- "instruction prefetch" so intentionally mispredict
	{
	  takeit = 1;
	}
      else
	takeit = proc->BPBPredict(inst->pc,inst->code->taken);

      result = takeit;
      if (takeit)
	{
	  inst->branch_pred = inst->pc + inst->code->imm;
	}
      else
	{
	  inst->branch_pred = inst->pc + 2;
	}
    }
  
  inst->taken = result;
  return result;
}

/****************************************************************************/
/* decode_branch_instruction : called the first time a branch instruction   */
/*                           : is encountered, sets all the relevant fields */
/****************************************************************************/

int decode_branch_instruction(instr *instrn, instance *inst, state *proc)
{
  
  StartCtlXfer(inst,proc); /* *****************************************
			       Translate this branch instruction,
			       sets inst->branch_pred, and inst->taken
			       *****************************************/ 
      
  /* Let us see what kind of branch this is */
  if( instrn->uncond_branch !=0 )
    {
      /* This is an unconditional branch */
      /* For unconditional branches
	 if Annul bit is 0 Instruction in delay slot is executed always
	 if Annul bit is 1 Instruction in delay slot not executed */
      
      if(instrn->annul == 0)
	{
	  /* Non anulled unconditional branches */
	  
	  switch(inst->taken)
	    {
	    case 0:
	      /* this means speculatively not taken. This can't happen */
	      fprintf(simerr,"decode_branch_instr: case 0 for uncond branch P%d, tag %d, time %d\n",proc->proc_id,inst->tag,proc->curr_cycle);
	      exit(-1);
	      break;
	      
	    case 1:
	      /* speculatively taken */
	      proc->copymappernext = 1;
	      /* NO break here */
	      
	    case -1:
	      /* non-speculatively taken; no need to copy mappers */
	      proc->pc = proc->npc;
	      proc->npc = inst->branch_pred;
	      break;
	      
	    case 2:
	      /* non-predictable, so not speculated */
	      proc->pc = proc->npc;
	      proc->npc= inst->branch_pred; /* will be -1 */
	      inst->branchdep = 1;
	      proc->unpredbranch = 1;
	      return 1;
	    default:
	      fprintf(simerr,"decode_branch_instr: bad case for uncond branch P%d, tag %d, time %d\n",proc->proc_id,inst->tag,proc->curr_cycle);
	      exit(-1);
	      break;
	    }
	}
      else
	{
	  /* Anulled unconditional branches */
	  switch(inst->taken)
	    {
	    case 0:
	      /* speculatively not taken */
	      fprintf(simerr,"decode_branch_instr: case 0 for uncond branch P%d, tag %d, time %d\n",proc->proc_id,inst->tag,proc->curr_cycle);
	      exit(-1);
	      break;
	    case 1:
	      /* speculatively taken */
	      inst->branchdep = 2; /* shadow mapping again */
	      /* Note no break */
	    case -1:
	      /* non-speculatively taken */
	      proc->pc = inst->branch_pred;
	      proc->npc = proc->pc +1;
	      break;
	    case 2:
	      /* I do not know what to do */
	      proc->pc = inst->branch_pred; /* it will be -1 */
	      inst->branchdep = 1;
	      proc->unpredbranch = 1;
	      break;
	    default:
	      fprintf(simerr,"decode_branch_instr: bad case for uncond branch P%d, tag %d, time %d\n",proc->proc_id,inst->tag,proc->curr_cycle);
	      exit(-1);
	      break;
	    }
	}
    }
  else
    {
      /* This is a conditional branch */
      
      /* If annul bit is 0, instruction in delay slot executed always
	 if annul bit is 1 then delay slot not executed unless branch
	 is taken. */
      if (instrn->annul == 0)
	{
	  /* Non annulled conditional branches */
	  
	  /* The shadow mapping will have to be done in the next instruction
	     ie, at the delay slot..lets make a note of it. */
	  proc->copymappernext = 1;
	  
	  proc->pc = proc->npc; /* Since all of them are going to do it
				   anyway. */

	  switch(inst->taken)
	    {
	    case 0:
	      /* speculatively not-taken */
	      proc->npc = proc->npc+1;
	      break;
	    case 1:
	      /* speculatively taken */
	      proc->npc = inst->branch_pred;
	      break;
	    case -1:
	      /* This should not occur here. This means taken for sure */
	      fprintf(simerr,"decode_branch_instr: case -1 for cond branch P%d, tag %d, time %d\n",proc->proc_id,inst->tag,proc->curr_cycle);
	      exit(-1);
	      break;
	    case 2:
	      /* This should not occur here. This means unpredicted */
	      fprintf(simerr,"decode_branch_instr: case 2 for cond branch P%d, tag %d, time %d\n",proc->proc_id,inst->tag,proc->curr_cycle);
	      exit(-1);
	      break;
	    default:
	      fprintf(simerr,"decode_branch_instr: bad case for cond branch P%d, tag %d, time %d\n",proc->proc_id,inst->tag,proc->curr_cycle);
	      exit(-1);
	      break;
	    }
	}
      else
	{
	  /* Anulled conditional branches */
	  switch(inst->taken)
	    {
	    case 0:
	      /* Speculatively not taken */
	      proc->pc = proc->npc +1; /* delay slot not taken */
	      proc->npc = proc->pc +1;
	      inst->branchdep = 2;
	      break;
	    case 1:
	      /* Speculatively taken */
	      proc->pc = proc->npc;
	      proc->npc = inst->branch_pred;
	      inst->branchdep = 2; /* We copy the shadow map here because
				      of the possibility of the delay slot
				      getting squashed */
	      break;
	    case -1:
	      /* This should not occur here. This means taken for sure */
	      fprintf(simerr,"decode_branch_instr: case -1 for cond branch P%d, tag %d, time %d\n",proc->proc_id,inst->tag,proc->curr_cycle);
	      exit(-1);
	      break;
	    case 2:
	      /* This should not occur here. This means unpredicted */
	      fprintf(simerr,"decode_branch_instr: case 2 for cond branch P%d, tag %d, time %d\n",proc->proc_id,inst->tag,proc->curr_cycle);
	      exit(-1);
	      break;
	    default:
	      fprintf(simerr,"decode_branch_instr: bad case for cond branch P%d, tag %d, time %d\n",proc->proc_id,inst->tag,proc->curr_cycle);
	      exit(-1);
	      break;
	    } /* End of switch */
	} /* End of is annulled or not */
    } /* End of is unconditional or not? */
  
  return 0;
}

/*************************************************************************/
/* BPBSetup : Intialize the branch predictor                             */
/*************************************************************************/
		
void state::BPBSetup() 
{
  if (SZ_BUF > 0)
    {
      if (BPB_TYPE == TWOBIT || BPB_TYPE == TWOBITAGREE)
	{
	  BranchPred = new int[SZ_BUF];
	  PrevPred = new int[SZ_BUF];
	  for (int i=0; i< SZ_BUF; i++)
	    {
	      BranchPred[i]=1; /* initialize it to metastable and taken/agree */
	      PrevPred[i]=0;
	    }
	}
      else // static
	{
	}
    }
  else // SZ_BUF <= 0
    {
      if (BPB_TYPE != STATIC)
	{
	  fprintf(simerr,"Cannot simulate dynamic branch prediction with buffer size %d. Reverting to static.\n",SZ_BUF);
	  BPB_TYPE = STATIC;
	}
    }
}

/* Prediction uses the 2 bit history scheme

          nt
      <--------    
   10           11
   |  -------->  ^ 
   |       t     |
nt |             | t
   |             |
   |      nt     |
   v  <--------  |
   00           01
      -------->    
           t

Above, the first bit is the prediction bit

In our system the first bit is represented by BranchPred, and the
second by PrevPred

We support either regular 2-bit prediction or 2-bit agree prediction
(for agree, replace T/NT with A/NA)
*/

/*************************************************************************/
/* BPBPredict : Based on current PC (and, possibly static prediction),   */
/*              predict whether branch taken or not                      */
/*************************************************************************/

int state::BPBPredict(int bpc, int statpred) /* returns prediction: 1 for taken, 0 for not taken */
{
  if (BPB_TYPE == TWOBIT)
    return BranchPred[bpc&(SZ_BUF-1)];
  else if (BPB_TYPE == TWOBITAGREE)
    return (BranchPred[bpc&(SZ_BUF-1)] == statpred); /* this is an XNOR of (dynamic) agree prediction & static prediction */
  else /* right now, the only other type is static */
    return statpred;
}

/*************************************************************************/
/* BPBComplete: called when branch speculation resolved. Set actual      */
/*              history here                                             */
/*************************************************************************/
void state::BPBComplete(int bpc, int taken, int statpred) /* complete is to resolve speculations */
{
  int inbit;
  if (BPB_TYPE == TWOBIT)
    inbit = taken;
  else if (BPB_TYPE == TWOBITAGREE)
    inbit = (taken == statpred); /* agree/disagree */
  else /* currently this means static */
    return; /* static has no BPB */
	  
  int posn = bpc & (SZ_BUF-1);

  if (PrevPred[posn] == BranchPred[posn]) /* either 00 or 11 */
    PrevPred[posn] = inbit;
  else /* in this case, either 01 or 10, so set both = inbit */
    BranchPred[posn] = PrevPred[posn] = inbit;
}


/*************************************************************************/
/* Return Address stack functions.                                       */
/* Operations are insert and destructive predict                         */
/*************************************************************************/

void state::RASSetup()
{
  if (RAS_STKSZ > 0)
    {
      ReturnAddressStack = new int[RAS_STKSZ];
      for (int i=0; i<RAS_STKSZ; i++)
	ReturnAddressStack[i]=0; /* we do this to insure that all are identical */
    }
  rasptr = 0;
}

void state::RASInsert(int retpc) /* added on a call */
{
  if (RAS_STKSZ > 0)
    {
      ReturnAddressStack[rasptr++]=retpc;
      rasptr = rasptr & (RAS_STKSZ - 1);
    }
}

int state::RASPredict()
{
  if (RAS_STKSZ > 0)
    {
      --rasptr;
      rasptr = unsigned(rasptr) & (RAS_STKSZ - 1);
      int res = ReturnAddressStack[rasptr];
      return res;
    }
  else
    {
      return 0;
    }
}
