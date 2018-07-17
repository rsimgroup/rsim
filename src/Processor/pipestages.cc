/*
  Processor/pipestages.cc

 This is the main file that guides the processor pipeline.

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


#include "Processor/decode.h"
#include "Processor/branchq.h"
#include "Processor/state.h"
#include "Processor/memory.h"
#include "Processor/mainsim.h"
#include "Processor/memprocess.h"
#include "Processor/freelist.h"
#include <limits.h>
#include <stdlib.h>

#include "Processor/exec.h"
#include "Processor/instance.h"
#include "Processor/FastNews.h"
#include "Processor/traps.h"
#include "Processor/simio.h"
#include "Processor/processor_dbg.h"

extern "C"
{
#include "MemSys/module.h"
#include "MemSys/simsys.h"
}

/* The maindecode function is called every cycle, and is used to call,
   successively, the stages related to pipeline-processing.

   The decode_cycle function brings new instructions into the active
   list and calls other functions to handle them appropriately. The
   function instance::decode_instruction sets up important fields
   related to each new instruction brought in.

   check_dependencies determines if a new instruction depends on the
   values produced by previous instructions, and if so, puts the
   instruction in stall queues associated with the needed registers.

   If there are no dependencies, the SendToFU and issue functions
   process the instruction.

   At the end of each cycle update_cycle marks the registers
   associated with newly computed results as ready and tries to start
   instructions that depend on those registers.

  */


/*************************************************************************/
/* maindecode :           handles the main pipeline stages of the proc   */
/*************************************************************************/


int maindecode(state *proc) 
{
  /* First update data structures on the basis of what happened
     in the previous cycle  */
  update_cycle(proc);
  graduate_cycle(proc);  
  proc->intregbusy[ZEROREG] = 0;
  
  /* perform the decoding associated with this cycle */
  if (proc->exit)   {
    return 1;
  }

  ComputeAvail(proc);
  if (proc->in_exception == NULL) // we won't take new ops if so
    {
      decode_cycle(proc);
    }
  
  /* Go on to the execution unit */
  return (0);
}


/*************************************************************************/
/* decode_cycle :  brings new instructions into active list              */
/*************************************************************************/

/* decode_cycle() --  The decode routine called every cycle 
 * This is organized into two main parts -
 * 1. Check the stall queue for instructions stalled from previous
 *    cycles.
 * 2. Check the instructions for this cycle.
 */


static instr TheBadPC(iSETHI); /* it's a NOP */


int decode_cycle(state *proc)
{
#ifdef COREFILE
  if(proc->curr_cycle > DEBUG_TIME)
    fprintf(corefile, "ENTERING DECODE CYCLE-CYCLE NUMBER %d. UnitsFree: %d %d %d %d\n", proc->curr_cycle,proc->UnitsFree[uALU],proc->UnitsFree[uFP],proc->UnitsFree[uADDR],proc->UnitsFree[uMEM]);
#endif
  instr *instrn;
  instance *inst;
  int brk = 0;
  int badpc = 0;
  stallqueueelement *stallqitem;
  int decoderate = proc->decode_rate;
  
  /* FIRST CHECK FOR PREVIOUS STALLED INSTRUCTIONS  */
  
  stallqitem = proc->stallq->get_next_entry(NULL);
  stallqueueelement *stallqremove;
  
#ifdef COREFILE
  if(proc->curr_cycle > DEBUG_TIME)
    fprintf(corefile, "\tChecking stall queue....\n");
#endif
  int count = 0;

  while(count < decoderate && (stallqitem != NULL) && (brk != 1))
    {
      /* Checking a previous stalled instruction. */
    
      if( check_dependencies(stallqitem->inst, proc) == 0)
	{
	  count++; /* it got added to active list, so it's like counting
		      the fetch now */
	  stallqremove = stallqitem;
	  stallqitem = proc->stallq->get_next_entry(stallqitem);
	  proc->stallq->delete_entry(stallqremove,proc);
	}
      else /* cannot remove it, as it is still stuck for a hazard */
	{
	  /* In RSIM, there should be no next entry in the
	     stall queue. This will return NULL and break out of
	     this loop. */
	  stallqitem = proc->stallq->get_next_entry(stallqitem);
	}
      
      /* In case there is a stalled instruction pending, or
	 a branch that has temporarily set stall_the_rest */
      if(proc->stall_the_rest)
	brk = 1;
    }

  if(proc->stall_the_rest)
    {
      brk = 1;
    }
  
  /* DECODE THIS CYCLE'S INSTRUCTIONS.
     The number of instructions decoded per cycle is
     NO_OF_DECODES_PER_CYCLE (state.h) */
  
  /* Read in more instructions only if there are no branch
     stalls (or if there is a branch stall and the delay slot
     has to be read in) */
#ifdef COREFILE
  if(proc->curr_cycle > DEBUG_TIME)
    fprintf(corefile, "\tStarting this cycle's decoding...\n");
#endif
  
  while(count++ < decoderate && !brk )
    {
      /* read instruction from input file depending on current PC.
	 However, if proc->next_instrn_pc > number of instructions (the
	 maximum user-mode PC, this will not be safe. So, allow such
	 accesses only with privileged mode -- otherwise, generate
	 a single "BadPC" exception instruction */
	 
      if (proc->pc >= num_instructions || proc->pc < 0)
	{
	  if (proc->privstate && proc->pc >= TRAPTABLE_BASE && (proc->pc - TRAPTABLE_BASE < TRAPTABLE_SIZE))
	    {
	      /* privileged state, doing trap stuff... */
	      instrn = trapstable + (proc->pc - TRAPTABLE_BASE);
	    }
	  else
	    {
	      /* generate a single "BAD PC" exception instruction */
	      instrn = &TheBadPC;
	      badpc = 1;
	    }
	}
      else
	instrn = instr_array + proc->pc; // new instr;
      /* allocating memory */
      
#ifdef COREFILE
      instrn->print();
#endif
      inst = NewInstance(instrn,proc);
      if (badpc)
	{
	  inst->exception_code=BADPC;
	  proc->stall_the_rest=inst->tag;
	  proc->type_of_stall_rest=eBADBR;
	}

      /* In SC or PC, MEMBARs should be NOPs. However, it is not
	 possible to convert them to NOPs dynamically because they
	 might be in delay slots and thereby need to hold shadow
	 mappers, etc. Therefore, if the user really wants the "fewest
	 instructions" for SC, she should run separate versions for
	 SC. However, in practice the extra NOP is not likely to make
	 any significant performance impact */
      
      AddtoTagConverter(inst->tag, inst, proc);
      
      /* If there are dependencies, put it in the stall queue.
	 check_dependencies takes care of issuing it if there are no
	 dependencies */
      if( check_dependencies(inst,proc) != 0)
	{
	  stallqitem = Newstallqueueelement(inst,proc);
	  proc->stallq->append(stallqitem);
	}

      
      /* If a pipeline bubble has to be inserted, then... */
      if(proc->stall_the_rest)
	{
	  brk = 1;
	}
      
      if(proc->stall_the_rest == -1)
	{
	  unstall_the_rest(proc); // this is just a xfer, not a real stall
	}
    }
  return 0;
}


/*****************************************************************************/
/* decode_instruction  : reads in an instruction and converts to an instance */
/*****************************************************************************/

/* decode_instruction() - Converts the static instruction to the
   dynamic instance. Initializes the elements of instance and also
   does a preliminary check for hazards */

int newinst=0,new2ndinst=0,killinst=0;

int instance::decode_instruction(instr *instrn, state *proc)
{
  newinst++;
  addr = 0;
  addr_ready = 0;
  newst=0;
  limbo=0;
  kill=0;

  partial_overlap=0;
  in_memunit=0;
  busybits = 0;
  stallqs = 0;
  rs1valf=rs2valf=0;
  rsccvali=0;
  rsdvalf=0;
  rccvali=rdvali=0;
  
  tag = proc->instruction_count; /*Our instruction id henceforth! */
  proc->instruction_count ++; /* Increment Global variable */

  time_active_list = YS__Simtime;

#ifdef COREFILE
  if(proc->curr_cycle > DEBUG_TIME)
    fprintf(corefile,"Creating tag %d, new = %d, new2 = %d, kill = %d, CANSAVE = %d, CANRESTORE = %d\n",tag,newinst,new2ndinst,killinst,proc->CANSAVE,proc->CANRESTORE);
#endif
 
  issuetime = INT_MAX; /* start it out as high as possible */
  addrissuetime = INT_MAX; /* used only in static sched; start out high */

  vsbfwd = 0;
  miss = mtL1HIT;
  latepf = 0;
  
  /* INITIALIZE DATA ELEMENTS OF INST */
  
  win_num = proc->cwp; /* before any change in this instruction. */

  code=instrn;
  
    
  unit_type = unit[instrn->instruction];
  depctr = 0;
  
  /* Set up default dependency values */
  truedep = 1;
  addrdep = 1; /* this is just for rs2 and rscc, which
		  are used in calculating addresses */
  
  strucdep = 0;
  branchdep = 0;
  
  /* Make all other values zero */
  completion = newpc = 0;
  exception_code=OK;
  mispredicted = annulled = taken = 0;
  memprogress = 0;
  proc->stall_the_rest=0;
  proc->type_of_stall_rest=eNOEFF_LOSS;

  /*************************************************************/
  /******* convert logical and physical registers **************/
  /*************************************************************/

  /*  This is the first time the instruction is being processed,
      we will have to convert the window pointer, register number
      combination to a logical number and then allocate physical
      registers, etc.. */
  
  /* ALL THE SOURCE REGISTERS ARE MAPPED TO THEIR CORR. PHY REGS */
  
  /* Source register 1 */

  switch (instrn->rs1_regtype)
    {
    case REG_INT:
    case REG_INT64:
      lrs1 = convert_to_logical(win_num, instrn->rs1);
      prs1 = proc->intmapper[lrs1];
      break;
    case REG_FP:
      lrs1 = instrn->rs1;
      prs1 = proc->fpmapper[lrs1];
      break;
    case REG_FPHALF:
      lrs1 = instrn->rs1;
      prs1 = proc->fpmapper[unsigned(lrs1)&~1U];
      break;
    case REG_INTPAIR:
      if (instrn->rs1 & 1) /* odd source reg. */
	{
	  exception_code = ILLEGAL;
	  lrs1 = prs1 = prs1p = 0;
	}
      else
	{
	  lrs1 = convert_to_logical(win_num, instrn->rs1);
	  prs1 = proc->intmapper[lrs1];
	  prs1p = proc->intmapper[lrs1+1];
	}
      break;
    default:
      break;
    }
  
  switch (instrn->rs2_regtype)
    {
    case REG_INT:
    case REG_INT64:
      lrs2 = convert_to_logical(win_num, instrn->rs2);
      prs2 = proc->intmapper[lrs2];
      break;
    case REG_FP:
      lrs2 = instrn->rs2;
      prs2 = proc->fpmapper[lrs2];
      break;
    case REG_FPHALF:
      lrs2 = instrn->rs2;
      prs2 = proc->fpmapper[unsigned(lrs2)&~1U];
      break;
    default:
      break;
    }
  
  /* Conditon code as a source register rscc */
  lrscc = convert_to_logical(win_num, instrn->rscc);
  prscc = proc->intmapper[lrscc];
  if (instrn->rd_regtype == REG_FPHALF)
    {
      /* Since FPs are mapped/renamed by doubles, we need to make
	 the dest. reg a source also in this case */
      lrsd = instrn->rd;
      prsd = proc->fpmapper[unsigned(lrsd)&~1U];
    }
  
  int dests=1;
  
  /*************************************************************/
  /***************** Register window operations ****************/
  /*************************************************************/
  
  /* Lets us NOW bump up or bring down the cwp if its a save/restore
     instruction. Note that the source registers do not see the
     effect of the change in the cwp while the destination registers do */

  if (instrn->wpchange == WPC_SAVE)
    {
      if (proc->CANSAVE || proc->privstate)
	{
	  win_num = unsigned(win_num-1) & (NUM_WINS-1);
	}
      else
	{
	  // SAVE EXCEPTION. In this case, do not let this instruction act properly.
	  exception_code = WINTRAP;
	  dests = 0;
	  lrd=lrcc=0;
	  strucdep = 2;
	}
    }
  else if (instrn->wpchange == WPC_RESTORE)
    {
      if (proc->CANRESTORE || proc->privstate)
	{
	  win_num = unsigned(win_num+1) & (NUM_WINS-1);
	}
      else 
	{
	  // RESTORE EXCEPTION.
	  exception_code = WINTRAP;
	  dests = 0;
	  lrd=lrcc=0;
	  strucdep = 2;
	}
    }
  
  /***************************************************************/
  /******************* Memory barrier instructions ***************/
  /***************************************************************/
  
  
#ifndef STORE_ORDERING
  if (instrn->instruction == iMEMBAR && instrn->rs1 == STATE_MEMBAR)
    {
      // decode a membar and put it in its queue
      MembarInfo mb;
      mb.tag = tag;
      mb.SS = (instrn->aux2 & MB_StoreStore) != 0;
      mb.LS = (instrn->aux2 & MB_LoadStore) != 0;
      mb.SL = (instrn->aux2 & MB_StoreLoad) != 0;
      mb.LL = (instrn->aux2 & MB_LoadLoad) != 0;
      mb.MEMISSUE = (instrn->aux1 & MB_MEMISSUE) != 0;
      ComputeMembarInfo(proc,mb);
      proc->membar_tags.Insert(mb);
    }
#endif
  
  if (dests) // ALWAYS, except for win exceptions
    {
      /* Convert the destination condition code register */
      lrcc = convert_to_logical(win_num, instrn->rcc);
      
      /* Convert the Destination Register */
      if(instrn->rd_regtype == REG_FP || instrn->rd_regtype == REG_FPHALF)
	{
	  lrd = instrn->rd;
	  strucdep = 1;     /* To go to the right place in check_dependencies */
	}
      else
	{
	  lrd = convert_to_logical(win_num, instrn->rd);
	  strucdep = 2;          /* To go to the right place in check_dependencies */
	}
    }
  
  /* Check if we are a delay slot */
  /* We need to do it, before we enter a possible branch */
  if(proc->copymappernext == 1)
    {
      /* There is a message for me by the previous branch
	 to take care of copying the shadow mapping, Let me
	 store this information. */
      branchdep = 2;   /* this->branchdep */
      proc->copymappernext = 0; /* We don't want the next
				   instruction to do the same thing*/
    }
  pc = proc->pc; /* NOTE: we store PC and NPC not to imitate actual
		    system behavior, but rather to simulate it easily */
  npc = proc->npc;
  int tmppc = proc->pc; /* We need this to check if we are having a
			   jump in the instruction addresing */
  
  /* Check if we are getting into a branch instruction */
  /* At the end of this and the next stage, we would have got
     the pc and npc for the next instruction, and also identified
     if there are any branch dependencies */
  
  if( (instrn->cond_branch != 0) || (instrn->uncond_branch !=0) )
    decode_branch_instruction(instrn, this, proc);
  else 
    {
      /* For all non control transfer instructions */
      proc->pc = proc->npc;
      proc->npc = proc->pc + 1;
    }
  
  if ((proc->pc != tmppc + 1 && proc->pc != tmppc+2) // not taken branch with no DELAYs
      && !proc->stall_the_rest)
    {
      /* We are doing a jump at this stage to a different portion
	 of the address space, next fetch should begin only in the
	 next cycle, so stall the rest of fetches this cycle */
      proc->stall_the_rest = -1;
      proc->type_of_stall_rest = eBR;
    }
  /* Note that if we are starting a new cycle, this will get reset
     anyway, so we neednt bother about this instruction being the
     last one of a cycle! */
  return(0);
}

/*************************************************************************/
/* issue : Handle the issue once _all_ dependences are taken care of     */
/*************************************************************************/

void issue(instance *inst, state *proc) // actually issue the instance
{
  tagged_inst insttagged(inst);
  inst->strucdep = 0;
  if (inst->unit_type != uMEM)
    {
      proc->ReadyQueues[inst->unit_type].Insert(insttagged); /* uMEM doesn't use this ReadyQ */
      (proc->UnitsFree[inst->unit_type])--; /* uMEM doesn't use this here */
      if (STALL_ON_FULL)
	{
	  proc->unissued--;
	  inst->issuetime = proc->curr_cycle;
#ifdef COREFILE
	  if (YS__Simtime > DEBUG_TIME)
	    fprintf(corefile,"unissued now %d\n",proc->unissued);
#endif
	}
    }
  else
    {
      // do it based on uADDR instead
      proc->ReadyQueues[uADDR].Insert(insttagged); /* uMEM doesn't use this ReadyQ */
      (proc->UnitsFree[uADDR])--;
      if (stat_sched) // uADDR is also statically scheduled
	{
	  proc->unissued--;
	  inst->addrissuetime = proc->curr_cycle;
#ifdef COREFILE
	  if (YS__Simtime > DEBUG_TIME)
	    fprintf(corefile,"unissued now %d\n",proc->unissued);
#endif
	}
    }
}

/*************************************************************************/
/* SendToFU : Handle issue once all dependences (other than structural   */
/*          : dependences for functional units) are taken care of        */
/*************************************************************************/

int SendToFU(instance *inst, state *proc)
{
  /* We have crossed all hazards, ISSUE IT!! */
  /* But, first copy the values of the phy regs
     into the instance */
  

#ifdef COREFILE
  if(proc->curr_cycle > DEBUG_TIME){
    fprintf(corefile, "pc = %d, tag = %d, %s \n", inst->pc,inst->tag,inames[inst->code->instruction]);
    fprintf(corefile, "lrs1 = %d, prs1 = %d \n",inst->lrs1,inst->prs1);
    fprintf(corefile, "lrs2 = %d, prs2 = %d \n",inst->lrs2,inst->prs2);
    fprintf(corefile, "lrd = %d, prd = %d \n",inst->lrd,inst->prd);
  }
#endif

  switch (inst->code->rs1_regtype)
    {
    case REG_INT:
      inst->rs1vali = proc->physical_int_reg_file[inst->prs1];
#ifdef COREFILE
      if(proc->curr_cycle > DEBUG_TIME)
	fprintf(corefile, "rs1i = %d, ",inst->rs1vali);
#endif
      break;
    case REG_INT64:
      inst->rs1valll = proc->physical_int_reg_file[inst->prs1];
#ifdef COREFILE
      if(proc->curr_cycle > DEBUG_TIME)
	fprintf(corefile, "rs1i = %lld, ",inst->rs1valll);
#endif
      break;
    case REG_FP:
      inst->rs1valf = proc->physical_fp_reg_file[inst->prs1];
#ifdef COREFILE
      if(proc->curr_cycle > DEBUG_TIME)
	fprintf(corefile, "rs1f = %f, ",inst->rs1valf);
#endif
      break;
    case REG_FPHALF:
      {
	float *address = (float *) (&proc->physical_fp_reg_file[inst->prs1]);
	if (inst->code->rs1 & 1) /* the odd half */
	  {
	    address += 1;
	  }
	inst->rs1valfh = *address;
#ifdef COREFILE
	if(proc->curr_cycle > DEBUG_TIME)
	  fprintf(corefile, "rs1fh = %f, ",inst->rs1valfh);
#endif
      }
      break;
    case REG_INTPAIR:
      inst->rs1valipair.a = proc->physical_int_reg_file[inst->prs1];
      inst->rs1valipair.b = proc->physical_int_reg_file[inst->prs1p];
#ifdef COREFILE
      if(proc->curr_cycle > DEBUG_TIME)
	fprintf(corefile, "rs1i = %d/%d, ",inst->rs1valipair.a,inst->rs1valipair.b);
#endif
      break;
    default:
      fprintf(simerr,"Unexpected regtype\n");
      exit(-1);
    }

  switch (inst->code->rs2_regtype)
    {
    case REG_INT:
      inst->rs2vali = proc->physical_int_reg_file[inst->prs2];
#ifdef COREFILE
      if(proc->curr_cycle > DEBUG_TIME)
	fprintf(corefile, "rs2i = %d, ",inst->rs2vali);
#endif
      break;
    case REG_INT64:
      inst->rs2valll = proc->physical_int_reg_file[inst->prs2];
#ifdef COREFILE
      if(proc->curr_cycle > DEBUG_TIME)
	fprintf(corefile, "rs2i = %lld, ",inst->rs2valll);
#endif
      break;
    case REG_FP:
      inst->rs2valf = proc->physical_fp_reg_file[inst->prs2];
#ifdef COREFILE
      if(proc->curr_cycle > DEBUG_TIME)
	fprintf(corefile, "rs2f = %f, ",inst->rs2valf);
#endif
      break;
    case REG_FPHALF:
      {
	float *address = (float *) (&proc->physical_fp_reg_file[inst->prs2]);
	if (inst->code->rs2 & 1) /* the odd half */
	  {
	    address += 1;
	  }
	inst->rs2valfh = *address;
#ifdef COREFILE
	if(proc->curr_cycle > DEBUG_TIME)
	  fprintf(corefile, "rs2fh = %f, ",inst->rs2valfh);
#endif
      }
      break;
    default:
      fprintf(simerr,"Unexpected regtype\n");
      exit(-1);
    }

  if (inst->code->rd_regtype == REG_FPHALF)
    {
      inst->rsdvalf = proc->physical_fp_reg_file[inst->prsd];
#ifdef COREFILE
      if(proc->curr_cycle > DEBUG_TIME)
	fprintf(corefile, "rsdf = %f, ",inst->rsdvalf);
#endif
    }
  
  inst->rsccvali =  proc->physical_int_reg_file[inst->prscc];
  
#ifdef COREFILE
  if(proc->curr_cycle > DEBUG_TIME)
    fprintf(corefile, "rsccvali = %d \n", inst->rsccvali);
#endif
  
  if (inst->unit_type == uMEM)
    return 0;
  if (proc->UnitsFree[inst->unit_type] == 0)
    {
      inst->stallqs++;
      proc->UnitQ[inst->unit_type].AddElt(inst,proc);
      return 7;
    }
  else
    {
      issue(inst,proc);
      return 0;
    }
}

/*************************************************************************/
/* check_dependencies : implements the dependecy checking logic of the   */
/*                    : processor                                        */
/*************************************************************************/

int check_dependencies(instance *inst, state *proc)
{
  int next_free_phys_reg = 0;
  /* Check for structural stalls first */
  proc->intmapper[ZEROREG] = 0;
  proc->intregbusy[ZEROREG] = 0;

  /* NOTE: INT_PAIR takes the standard INT route except that it maps
     the second dest register where other instructions would map
     "rcc".  This is OK in the SPARC since the only instructions (LDD,
     LDDA) with an INT_PAIR destination do not have a CC
     destination. */
  
  if (STALL_ON_FULL && inst->strucdep &&
      (inst->unit_type != uMEM || stat_sched) && /* in stat_sched, addr. gen is also lumped in with these */
      proc->unissued >= MAX_ALUFPU_OPS)
    {
      // Make sure there's space in the issue queue before
      // even trying to get renaming registers, etc.
#ifdef COREFILE
      if (YS__Simtime > DEBUG_TIME)
	fprintf(corefile,"Stalling for ALU/FPU issue queue space on tag %d\n",inst->tag);
#endif
      proc->stall_the_rest = inst->tag;
      proc->type_of_stall_rest = eISSUEQFULL;
      return 11;
    }

  /*************************************/
  /* Check for structural stalls first */
  /*************************************/


  

  if(inst->strucdep != 0)
    {
      switch(inst->strucdep)
	{
	case 1:
	  
	  /* No free memory for rd fp */
	  next_free_phys_reg = proc->free_fp_list->getfreereg();
	  if(next_free_phys_reg == -1)
	    {
	      proc->stall_the_rest = inst->tag;
	      proc->type_of_stall_rest = eRENAME;
	      inst->strucdep = 1;
	      return (1);
	    }
	  inst->prd = next_free_phys_reg;
	  /* Note no break here, we have to do the next step too */
      
	case 3:
	  /* Copy old mapping to active list */
	  if(proc->active_list->NumEntries() == proc->max_active_list_size)
	    {
	      /* No space in active list for fp */
	      inst->strucdep = 1;
	      proc->free_fp_list->addfreereg(inst->prd);
	      inst->prd = 0;
	      proc->stall_the_rest = inst->tag;
	      proc->type_of_stall_rest = eNOEFF_LOSS;
	      return (3);
	    }

	  /* NOTE: This code is used for both FP and FPHALF */
	  if (inst->code->rd_regtype == REG_FP)
	    proc->active_list->add_to_active_list(inst->tag, inst->lrd, proc->fpmapper[inst->lrd],
						  REG_FP,proc );
	  else /* FPHALF */
	    proc->active_list->add_to_active_list(inst->tag, unsigned(inst->lrd)&~1U, proc->fpmapper[unsigned(inst->lrd)&~1U],
						  REG_FP,proc);
	    
	  /* NOW, change the mapper to point to the new mapping */
	  proc->cwp = inst->win_num;
	  if (inst->code->wpchange && inst->exception_code != WINTRAP && !proc->privstate)
	    {
	      proc->CANSAVE += inst->code->wpchange;
	      proc->CANRESTORE -= inst->code->wpchange;
#ifdef COREFILE
	      if (YS__Simtime > DEBUG_TIME)
		fprintf(corefile,"Changing cwp. Now CANSAVE %d, CANRESTORE %d\n",proc->CANSAVE,proc->CANRESTORE);
#endif
	    }
	  
	  if (inst->code->rd_regtype == REG_FP)
	    proc->fpmapper[inst->lrd] = inst->prd;
	  else // REG_FPHALF
	    proc->fpmapper[unsigned(inst->lrd)&~1U] = inst->prd;
	  
	  /* Update busy register table */
	  proc->fpregbusy[inst->prd] = 1;
	  
	  inst->strucdep = 5;
	  break;
	  
	case 2:
	  if(inst->lrd == ZEROREG)
	    {
	      inst->strucdep = 4;
	      inst->prd = ZEROREG;
	    }
	  else
	    {
	      /* No free memory for rd int */
	      next_free_phys_reg = proc->free_int_list->getfreereg();
	      if(next_free_phys_reg == -1)
		{
		  proc->stall_the_rest = inst->tag;
		  proc->type_of_stall_rest = eRENAME;
		  inst->strucdep = 2;
		  return (2);
		}
	      inst->prd = next_free_phys_reg;
	      /* Note no break */
	    }
	case 4:
      
	  /* Copy old mapping to active list */
	  if(proc->active_list->NumEntries() == proc->max_active_list_size)
	    {
	      if (inst->prd != ZEROREG)
		{
		  inst->strucdep = 2;
		  proc->free_int_list->addfreereg(inst->prd);
		  inst->prd = ZEROREG;
		}
	      proc->stall_the_rest = inst->tag;
	      proc->type_of_stall_rest = eNOEFF_LOSS;
	      return (4);
	    }
	  proc->active_list->add_to_active_list(inst->tag, inst->lrd,proc->intmapper[inst->lrd],
						REG_INT,proc );
	  /* NOW, change the mapper to point to the new mapping */
	  proc->cwp = inst->win_num;
	  if (inst->code->wpchange && inst->exception_code != WINTRAP && !proc->privstate)
	    {
	      proc->CANSAVE += inst->code->wpchange;
	      proc->CANRESTORE -= inst->code->wpchange;
#ifdef COREFILE
	      if (YS__Simtime > DEBUG_TIME)
		fprintf(corefile,"Changing cwp. Now CANSAVE %d, CANRESTORE %d\n",proc->CANSAVE,proc->CANRESTORE);
#endif
	    }
	  proc->intmapper[inst->lrd] = inst->prd;
	  
	  /* Update busy register table */
	  if (inst->prd != ZEROREG)
	    proc->intregbusy[inst->prd] = 1;
	  inst->strucdep = 5;
	  break ;
	  
	default:
	  break;
	}
    
      switch(inst->strucdep)
	{
	case 5:
	  if (inst->code->rd_regtype == REG_INTPAIR)
	    {
	      /* in this case, we need to map the second register
		 of the pair rather than the cc register */
	      inst->prcc = ZEROREG; /* cc not used */
	      if (inst->code->rd & 1) /* odd destination register */
		{
		  inst->exception_code = ILLEGAL;
		  inst->prdp = 0;
		}
	      else
		{
		  next_free_phys_reg = proc->free_int_list->getfreereg();
		  if(next_free_phys_reg == -1)
		    {
		      proc->stall_the_rest = inst->tag;
		      proc->type_of_stall_rest = eRENAME;
		      inst->strucdep = 5;
		      return (5);
		    }
		  inst->prdp = next_free_phys_reg;
		}
	      /* No break here as we have to do the next part anyway */
	    }
	  else
	    {
	      inst->prdp = ZEROREG; /* pair not used */
	      if (inst->lrcc == ZEROREG)
		{
		  inst->strucdep = 6;
		  inst->prcc = ZEROREG;
		}
	      else
		{
		  /* rcc no free list register */
		  next_free_phys_reg = proc->free_int_list->getfreereg();
		  if(next_free_phys_reg == -1)
		    {
		      proc->stall_the_rest = inst->tag;
		      proc->type_of_stall_rest = eRENAME;
		      inst->strucdep = 5;
		      return (5);
		    }
		  inst->prcc = next_free_phys_reg;
		  /* No break here as we have to do the next part anyway */
		}
	    }
	case 6:
	  /* active list full */
	  
	  /* Copy old mapping to active list */
	  if (proc->active_list->NumEntries() == proc->max_active_list_size)
	    {
	      // THIS CASE SHOULD NEVER HAPPEN SINCE ACTIVE LIST IS IN PAIRS
	      fprintf(simerr,"Tag %d: Active list full in case 6 -- that should never happen.",inst->tag);
	      exit(1);
	      
	      inst->strucdep = 6;
	      proc->stall_the_rest = inst->tag; /* The rest of the insructions should not be
						   decoded */
	      proc->type_of_stall_rest = eNOEFF_LOSS;
	      return (6);
	    }
	  if (inst->code->rd_regtype == REG_INTPAIR)
	    {
	      proc->active_list->add_to_active_list(inst->tag, inst->lrd+1,
						    proc->intmapper[inst->lrd+1],REG_INT,proc);
	      /* NOW, change the mapper to point to the new mapping */
	      proc->intmapper[inst->lrd+1] = inst->prdp;
	      
	      /* Update busy register table */
	      proc->intregbusy[inst->prdp] = 1;
	  
	    }
	  else
	    {
	      proc->active_list->add_to_active_list(inst->tag, inst->lrcc,
						    proc->intmapper[inst->lrcc],REG_INT,proc);
	      /* NOW, change the mapper to point to the new mapping */
	      proc->intmapper[inst->lrcc] = inst->prcc;
	      
	      /* Update busy register table */
	      if (inst->prcc != ZEROREG)
		proc->intregbusy[inst->prcc] = 1;
	  
	    }
	  proc->stalledeff--; // it made it into active list, so it's no longer an eff problem!  
	  
#ifdef COREFILE
	  if (proc->stalledeff < 0)
	    {
	      fprintf(simerr,"STALLED EFFICIENCY DROPS BELOW 0\n");
	      exit(-1);
	    }
#endif
	  
	  inst->strucdep = (inst->unit_type == uMEM) ? 10 : 0;
	  break;
	  
	default :
	  /* fprinf(corefile,"<decode.c> Invalid structural dependency\n");*/
	  break;
	}
    }
  /* Check for branch dependencies here */
  /* If branchdep = 1, then that means we do not know what to
     fetch next(maybe other than the delay slot, things will
     stall automatically after the branch as the next pc will
     be -1 till this gets done, so nothing we need to do. */

  if (STALL_ON_FULL && (inst->unit_type != uMEM || stat_sched))
    {
      proc->unissued++; // from this point on, there will be no chance of ever coming this way again, so ok to increment
#ifdef COREFILE
      if (YS__Simtime > DEBUG_TIME)
	fprintf(corefile,"unissued now %d\n",proc->unissued);
#endif
    }
  
  if (proc->stall_the_rest == inst->tag)
    {
      // we don't call unstall the rest, because we don't want
      // stalledeff getting set to 0 for no obvious reason, as we are
      // still fetching instructions
      proc->stall_the_rest=0;
      proc->type_of_stall_rest=eNOEFF_LOSS;
    }

  /* CHECK FOR BRANCH STRUCTURAL DEPENDENCIES HERE */
  if(inst->branchdep == 2)
    {
      /* This means we are in a place where state has to
	 be saved, either at a branch or at a delay slot.
	 (That work has been done by decode_instruction already!)
	 So all we have to do is to save the shadow mapper */
    
      /* Try to copy into mappers */
      if( AddBranchQ(inst->tag, proc) != 0)
	{    
	  /* out of speculations ! */
	  /* stall future instructions until we get
	     some speculations freed */
	  inst->stallqs++;
	  proc->stall_the_rest = inst->tag;
	  proc->type_of_stall_rest = eSHADOW;
#ifdef COREFILE
	  if(proc->curr_cycle > DEBUG_TIME)
	    fprintf(corefile,"Branch %d failed to get shadow mapper\n",inst->tag);
#endif
	  inst->branchdep = 2;
	  proc->BranchDepQ.AddElt(inst,proc);
	  /*  Dont return, go ahead and issue it, we will
	     take care of the shadow mapping in the update cycle. */
	}
      else
	{
	  /* successful!! */
#ifdef COREFILE
	  if(proc->curr_cycle > DEBUG_TIME)
	    fprintf(corefile,"Branch %d got shadow mapper\n",inst->tag);
#endif
	  inst->branchdep = 0;
	}
    }

  if (proc->unpredbranch && ((inst->branchdep != 1) || inst->code->annul))
    {
      // the processor has some branch that could not be predicted, and
      // either we are that annulled branch itself or we are the
      // delay slot of the delayed branch. Now we need to do a stall the
      // rest. BTW, this _really_ will not like a branch in a delay slot.
      // We should probable serialize in such a case.

      proc->stall_the_rest = inst->tag;
      proc->type_of_stall_rest = eBADBR;
#ifdef COREFILE
      if(proc->curr_cycle > DEBUG_TIME)
	fprintf(corefile,"Stalling the rest for unpredicted branch at %d\n",inst->tag);
#endif
    }
  
  proc->intmapper[ZEROREG] = 0;
  proc->intregbusy[ZEROREG] = 0;
  int oldaddrdep = inst->addrdep; /* Used for disambiguate */

  /************************************/
  /* check for data dependencies next */
  /************************************/

  /* check RAW (true) dependencies for rs1 and rs2 */
  if(inst->truedep == 1)
    {
      inst->truedep = 0;
      inst->addrdep = 0;
      
      /* check for rs1 */
      if (inst->code->rs1_regtype == REG_INT || inst->code->rs1_regtype == REG_INT64)
	{
	  if(proc->intregbusy[inst->prs1] == 1)
	    {
	      inst->busybits |= BUSY_SETRS1;
	      proc->dist_stallq_int[inst->prs1].AddElt(inst,proc,BUSY_CLEARRS1);
	      inst->truedep = 1;
	    }
	}
      else if (inst->code->rs1_regtype == REG_FP || inst->code->rs1_regtype == REG_FPHALF)
	{
	  if(proc->fpregbusy[inst->prs1] == 1)
	    {
	      inst->busybits |= BUSY_SETRS1;
	      proc->dist_stallq_fp[inst->prs1].AddElt(inst,proc,BUSY_CLEARRS1);
	      inst->truedep = 1;
	    }
	}
      else if (inst->code->rs1_regtype == REG_INTPAIR)
	{
	  if(proc->intregbusy[inst->prs1] == 1)
	    {
	      inst->busybits |= BUSY_SETRS1;
	      proc->dist_stallq_int[inst->prs1].AddElt(inst,proc,BUSY_CLEARRS1);
	      inst->truedep = 1;
	    }
	  if(proc->intregbusy[inst->prs1p] == 1)
	    {
	      inst->busybits |= BUSY_SETRS1P;
	      proc->dist_stallq_int[inst->prs1p].AddElt(inst,proc,BUSY_CLEARRS1P);
	      inst->truedep = 1;
	    }
	}

      /* check for rs2 */
      if (inst->code->rs2_regtype == REG_INT || inst->code->rs2_regtype == REG_INT64)
	{
	  if(proc->intregbusy[inst->prs2] == 1)
	    {
	      inst->busybits |= BUSY_SETRS2;
	      proc->dist_stallq_int[inst->prs2].AddElt(inst,proc,BUSY_CLEARRS2);
	      inst->truedep = 1;
	      inst->addrdep=1;
	    }
	}
      else if (inst->code->rs2_regtype == REG_FP || inst->code->rs2_regtype == REG_FPHALF)
	{
	  if (proc->fpregbusy[inst->prs2] == 1)
	    {
	      inst->busybits |= BUSY_SETRS2;
	      proc->dist_stallq_fp[inst->prs2].AddElt(inst,proc,BUSY_CLEARRS2);
	      inst->truedep = 1;
	      inst->addrdep=1;
	    }
	}
      
      /* check for rscc*/
      if (proc->intregbusy[inst->prscc] == 1)
	{
	  inst->busybits |= BUSY_SETRSCC;
	  proc->dist_stallq_int[inst->prscc].AddElt(inst,proc,BUSY_CLEARRSCC);
	  inst->truedep = 1;
	  inst->addrdep=1;
	}

      /* If dest. is a FPHALF, then writing dest. register is effectively an
	 RMW. In this case, we need to make sure that register is also available */
      if (inst->code->rd_regtype == REG_FPHALF && proc->fpregbusy[inst->prsd] == 1)
	{
	  inst->busybits |= BUSY_SETRSD;
	  proc->dist_stallq_fp[inst->prsd].AddElt(inst,proc,BUSY_CLEARRSD);
	  inst->truedep = 1;
	}
    }
  
  /**************************************/
  /* check for address dependences next */
  /**************************************/
 
  if (inst->addrdep == 0 && inst->unit_type == uMEM)
    {
      inst->rs2vali = proc->physical_int_reg_file[inst->prs2];
      inst->rsccvali = proc->physical_int_reg_file[inst->prscc];
      if (oldaddrdep && inst->strucdep == 0) // already in memory system, but ambiguous
	CalculateAddress(inst,proc);
    }
  if (inst->strucdep == 10) // not yet in memory system
    {
      if (NumInMemorySystem(proc) < MAX_MEM_OPS)
	{
	  AddToMemorySystem(inst,proc);
	  inst->strucdep = 0;
	}
      else
	{
	  proc->UnitQ[uMEM].AddElt(inst,proc);
	  inst->stallqs++;
	  inst->strucdep = 0;

	  if (STALL_ON_FULL)
	    {
	      // in this type of processor (probably more realistic),
	      // the processor fetch/etc. stalls when the memory queue
	      // is filled up (by default, we'll keep fetching later
	      // instructions and just make this one wait for its resource
	      // (hold it in its active list space)

#ifdef COREFILE
	      if (YS__Simtime > DEBUG_TIME)
		fprintf(corefile,"Stalling for memory queue space on tag %d\n",inst->tag);
#endif
	      proc->stall_the_rest = inst->tag;
	      proc->type_of_stall_rest = eMEMQFULL;
	    }
	}
    }

  if(inst->truedep == 0)
    {
      /* Now we are ready to issue, but do we have the resources? */

      SendToFU(inst,proc);
      return 0;
    }
  else
    return (0);
#ifdef COREFILE
  if(proc->curr_cycle > DEBUG_TIME)
    fprintf(corefile, "<decode.cc> What am i doing here?\n");
#endif
  
} /*** End of check_dependencies ***/


/*************************************************************************/
/* update_cycle : updates the result registers/branch prediction tables  */
/*              : that are changed as a result of the instruction        */
/*************************************************************************/

int update_cycle(state *proc)
{
  instance *inst;
  int inst_tag;

  /* Let us first check for the case when the branch was the last
     instruction of the previous cycle, it may have completed
     by now, so we need to look at this NOW! */
    
  int over = 0;

  /* Get everything corresponding to this cycle */
  while(over != 1)
    {
      if( (proc->DoneHeap.num() == 0) ||
	  (proc->DoneHeap.PeekMin() > proc->curr_cycle) )
	{
	  over = 1;
	  return 0;
	}
      if( proc->DoneHeap.GetMin(inst,inst_tag) != proc->curr_cycle)
	{
	  if (inst->tag == inst_tag) /* don't flag it otherwise, as that would just be a flush on exception or some such.... */
	    {
#ifdef COREFILE
	      if(proc->curr_cycle > DEBUG_TIME)
		fprintf(corefile,"<decode.cc>Something is wrong! We have \
ignored some executions for several cycles -- tag %d at %d!\n",inst->tag, proc->curr_cycle); 
#endif
	      fprintf(simerr," Ignored executions in pipestages.cc\n");
	      exit(-1);
	      continue;
	    }
	}
      
      /* if inst->tag !== inst_tag, then it is an instruction that has
	 already been flushed out and we can ignore it */
      if (inst->tag == inst_tag)
	{
#ifdef COREFILE
	  if(proc->curr_cycle > DEBUG_TIME)
	    {
	      fprintf(corefile, "INST %p - \n", inst);
	      fprintf(corefile,"Marking tag %d as done\n", inst->tag);
	    }
#endif
	  
	  if (proc->stall_the_rest == inst->tag && !inst->branchdep && !proc->unpredbranch)
	    {
	      unstall_the_rest(proc); // the instruction completed
	    }
	  
	  /* Check if it is a branch instruction */
	  if(inst->code->cond_branch != 0 || inst->code->uncond_branch != 0)
	    {
	      if(inst->branchdep == 1) /* used for unpredicted branches */
		{
		  /* This is most probably an unconditional branch */
		  /* This is an instruction because of which the rest of
		     the other instructions are stalled and waiting,
		     lets update the pc */

		  if (inst->code->annul || inst->tag+1 != proc->instruction_count) // no delay slot remaining
		    {
		      proc->pc = inst->newpc; /* This would have been filled
						 at execution time*/
		      proc->npc = proc->pc+1;
		    }
		  else // delay slot still waiting to issue
		    {
		      proc->pc = inst->pc+1; // should have already been set this way
		      proc->npc = inst->newpc;
		    }
		  /* Note: if there was a delay slot, we also need to fill in
		     that one's NPC value... */
		  HandleUnPredicted(inst,proc);
		  proc->unpredbranch = 0; // go back to normal fetching, etc
		  if (proc->stall_the_rest == inst->tag || proc->type_of_stall_rest == eBADBR)
		    {
		      // either on this tag itself or on the delay slot
		      // but for this tag
		      unstall_the_rest(proc);
		    }
		}
	      /* Check if we did any fancy predictions */
	      if(inst->code->uncond_branch != 2 && inst->code->uncond_branch != 3 && inst->branchdep != 1)
		{
		  /* YES */
		  if(inst->mispredicted == 0)
		    {
#ifdef COREFILE
		      if(proc->curr_cycle > DEBUG_TIME)
			fprintf(corefile, "Predicted branch correctly - tag %d\n",inst->tag);
#endif
		      /* No error in our prediction */
		      /* Delete the corresponding mapper table */
		      GoodPrediction(inst, proc);
		      
		      /* Go on and graduate it as usual */
		    }
		  else
		    {
		      /* Error in prediction */
#ifdef COREFILE
		      if(proc->curr_cycle > DEBUG_TIME)
			fprintf(corefile,"Mispredicted %s - tag %d\n", (inst->code->uncond_branch==4)? "return" : "branch", inst->tag);
#endif
		      int origpred = inst->taken;
		      
		      /* Now set pc and npc appropriately */
		      if (proc->copymappernext && inst->tag+1==proc->instruction_count)
			{
			  /* in this case, we have decoded and
			     completed the branch, but still haven't
			     gotten the delay slot instruction. So, we
			     need to set pc for the delay slot and set
			     npc for the branch target */
			  proc->pc = inst->pc+1; /* this should have already been
						    that way, but anyway... */
			  proc->npc = inst->newpc;
			}
		      else if (inst->code->annul && inst->code->cond_branch && !origpred)
			{
			  /* In this case, we had originally predicted
			     an annulled branch to be not-taken, but
			     it ended up being taken.  In this case,
			     the upcoming pc needs to go to the delay
			     slot, while the npc needs to point to the
			     branch target */
			  proc->pc = inst->pc+1;
			  proc->npc = inst->newpc;
			}
		      else
			{
			  /* in any other circumstance, we go to the
			     branch target and continue execution from
			     there */
			  proc->pc = inst->newpc;
			  proc->npc = proc->pc +1;
			}
		      
		      BadPrediction(inst,proc);
		      /* continue;  // return (0); */
		    }
		}/* End of if for predictions */
	      
	    }
	  
	  
	  /* Look at destination registers */
	  /* make them not busy */
	  /* Update physical register file */
	  
	  if (inst->code->rd_regtype == REG_INT || inst->code->rd_regtype == REG_INT64)
	    {
	      if (inst->prd != 0)
		proc->physical_int_reg_file[inst->prd] = inst->rdvali;
	      proc->intregbusy[inst->prd] = 0;
	      proc->dist_stallq_int[inst->prd].ClearAll(proc);
	    }
	  else if (inst->code->rd_regtype == REG_FP)
	    {
	      proc->physical_fp_reg_file[inst->prd] = inst->rdvalf;
	      proc->fpregbusy[inst->prd] = 0;
	      proc->dist_stallq_fp[inst->prd].ClearAll(proc);
	    }
	  else if (inst->code->rd_regtype == REG_FPHALF)
	    {
	      proc->physical_fp_reg_file[inst->prd] = inst->rsdvalf;
	      float *address = (float *) (&proc->physical_fp_reg_file[inst->prd]);
	      if (inst->code->rd & 1) /* the odd half */
		{
		  address += 1;
		}
	      *address = inst->rdvalfh;
	      proc->fpregbusy[inst->prd] = 0;
	      proc->dist_stallq_fp[inst->prd].ClearAll(proc);
	    }
	  else if (inst->code->rd_regtype == REG_INTPAIR)
	    {
	      if (inst->prd != 0)
		proc->physical_int_reg_file[inst->prd] = inst->rdvalipair.a;
	      proc->physical_int_reg_file[inst->prdp] = inst->rdvalipair.b;
	      proc->intregbusy[inst->prd] = 0;
	      proc->intregbusy[inst->prdp] = 0;
	      proc->dist_stallq_int[inst->prd].ClearAll(proc);
	      proc->dist_stallq_int[inst->prdp].ClearAll(proc);
	    }
	  
	  /* Do the same for rcc too. */
	  if (inst->prcc != 0)
	    proc->physical_int_reg_file[inst->prcc] = inst->rccvali;
	  proc->intregbusy[inst->prcc] = 0;
	  proc->dist_stallq_int[inst->prcc].ClearAll(proc);
	  
	  /* Update active list to show done and exception  */
	  proc->active_list->mark_done_in_active_list(inst->tag,
						      inst->exception_code,
						      proc->curr_cycle);
	}
      else // the instruction has already been killed in some way
	{
#ifdef COREFILE
	  if (proc->curr_cycle > DEBUG_TIME)
	    fprintf(corefile,"Got nonmatching tag %d off Doneheap\n",inst_tag);
#endif
	}
    }
  return 0;
}

/*************************************************************************/
/* instr::print(): print member function for the isntruction class       */
/*************************************************************************/


void instr::print()
{
#ifdef COREFILE
  if(GetSimTime() > DEBUG_TIME) 
    fprintf(corefile,"%-12.11s%d\t%d\t%d\t%d\t%d\t%d\t%d\n",inames[instruction],rd,rcc,rs1,rs2,aux1,aux2,imm);
#endif
}







