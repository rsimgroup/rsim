/*
  except.cc

  This file maintains proper processor state after an exception and handles
  each type of exception appropriately. 

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

  
#include "Processor/state.h"
#include "Processor/instance.h"
#include "Processor/active.h"
#include "Processor/tagcvt.h"
#include "Processor/stallq.h"
#include "Processor/memory.h"
#include "Processor/branchq.h"
#include "Processor/traps.h"
#include "Processor/exec.h"
#include "Processor/mainsim.h"
#include "Processor/simio.h"

#include <stdlib.h>

#ifndef NO_IEEE_FP
#include <ieeefp.h>
#endif

static int ProcessSerializedInstruction(instance *, state *);
static void FatalException(instance *, state *);

extern "C"
{
#include "MemSys/simsys.h"
}

/*************************************************************************/
/* IsSoftException : returns true if exception code flags soft exception */
/*************************************************************************/

inline int IsSoftException(except code)
{
  return (code == SOFT_LIMBO) || (code==SOFT_SL_COHE) || (code==SOFT_SL_REPL);
}

/*************************************************************************/
/* PreExceptionHandler : Ensures precise exceptions on "hard" exceptions */
/*************************************************************************/

int PreExceptionHandler(instance *inst, state *proc)
{  
  proc->in_exception=inst;

  /* "hard" exceptions -- that is, exceptions which trap to kernel
     code (and which can thus possibly alter the TLB) have to at least
     wait for all prior stores to at least _issue_. In addition, they have
     to wait for any possible S-L constraint, since loads can go speculatively
     and can cause exceptions detected later. Fortunately, the latter only
     affects SC. "soft" exceptions don't have to wait for anything. */

  if (IsSoftException(inst->exception_code) || proc->ReadyUnissuedStores == 0)
    {
      proc->in_exception=NULL;
      return ExceptionHandler(inst->tag,proc); // now we're for real!
    }
  else
    {
#ifdef COREFILE
      if(proc->curr_cycle > DEBUG_TIME)
	fprintf(corefile,"Processor %d in PreExceptionHandler; waiting for %d stores to issue\n",proc->proc_id,proc->ReadyUnissuedStores);
#endif
      return -1;
    }
}

/*************************************************************************/
/* ExceptionHandler : Ensures preciseness of exceptions                  */
/*************************************************************************/

int ExceptionHandler(int tag, state *proc)
{
  proc->exceptions++;
  /* We have got an exception at the tag value */
  instance *inst = TagCvtHead(tag, proc);
  instance icopy = *inst;

  if (!IsSoftException(inst->exception_code))
    StatrecUpdate(proc->in_except,double(proc->curr_cycle-proc->time_pre_exception),1.0);

  
  /* At this point, all  instructions before this instruction
     have completed and all instructions after this have not
     written back -- precise interrupts */

  /* There are certain things we do irrespective of the exception,
     they are ... */

  FlushBranchQ(tag, proc);
  tag = tag-1; // because we should also kill the excepting instruction

  FlushMems(tag, proc);
    
  FlushStallQ(tag, proc);

  /* This will delete all entries int he DoneHeap */

  UnitSetup(proc,1);

  /* WE HAVE TO FLUSH STALL BEFORE ACTIVE */
  
  int pre = proc->active_list->NumElements();
  FlushActiveList(tag, proc);
  int post = proc->active_list->NumElements();
  StatrecUpdate(proc->except_flushed,double(pre-post),1.0);
  int except_rate = NO_OF_EXCEPT_FLUSHES_PER_CYCLE;

#ifdef COREFILE
  if(proc->curr_cycle > DEBUG_TIME)
    fprintf(corefile,"Tag %d caused exception %d at time %d\n",icopy.tag,icopy.exception_code,proc->curr_cycle);

#endif

#ifdef COREFILE
  /* check to make sure that all busy bits are off */
  int bctr;
  for (bctr=0; bctr< NO_OF_PHYSICAL_INT_REGISTERS; bctr++)
    {
      if (proc->intregbusy[bctr])
	{
	  fprintf(simerr,"Busy bit #%d set at exception!\n",bctr);
	  if(proc->curr_cycle > DEBUG_TIME)
	    fprintf(corefile,"Busy bit #%d set at exception!\n",bctr);
	}
    }
  for (bctr=0; bctr< NO_OF_PHYSICAL_FP_REGISTERS; bctr++)
    {
      if (proc->fpregbusy[bctr])
	{
	  fprintf(simerr,"FPBusy bit #%d set at exception!\n",bctr);
	  if(proc->curr_cycle > DEBUG_TIME)
	    fprintf(corefile,"FPBusy bit #%d set at exception!\n",bctr);
	}
    }
#endif
  
  /* Let us look at the type of exception first */
  switch(icopy.exception_code){
  case OK:
    /* No exception, we should not have come here */
    fprintf(simerr, "ERROR -- P%d(%d) @ %d, exception flagged when none!\n",proc->proc_id,icopy.tag,proc->curr_cycle);
#ifdef COREFILE
    fprintf(corefile, "ERROR, exception flagged when none!\n");
#endif
    exit(-1);
    return -1;
  case SEGV:
    /* handle seg faults, page faults, etc. */
    /* we need to determine just what type of thing this is
       1) TLB miss/page fault -- not currently supported
       2) stack needs to be grown
       3) regular seg fault
       Let real errors fall through to the next case without a break, since
       the next case handles non-returnable fatal errors*/
    {
      unsigned addr = icopy.addr;
      if (addr < lowshared)
	{
	  if (addr < proc->highheap)
	    {
#ifdef COREFILE
	      if (YS__Simtime > DEBUG_TIME)
		fprintf(corefile,"Heap overrun with exception on %d\n",icopy.tag);
#endif
	      /* fall through since this is a regular seg fault */
	    }
	  else if (lowshared-addr > MAXSTACKSIZE) /* note: lowshared can be replaced by "highstack" if they are ever made distinct */
	    {
#ifdef COREFILE
	      if (YS__Simtime > DEBUG_TIME)
		fprintf(corefile,"Stack request exceeds MAXSTACKSIZE for exception on %d\n",icopy.tag);
#endif
	      /* fall through since this is a regular seg fault */
	    }
	  else /* grow the stack */
	    {
#ifdef COREFILE
	      if (YS__Simtime > DEBUG_TIME)
		fprintf(corefile,"Growing stack for exception on %d\n",icopy.tag);
#endif
	      if (except_rate != 0)
		proc->DELAY = pre / except_rate;
	      StackTrapHandle(addr,proc);
	      /* now we can restart from this instruction itself */
	      reset_lists(proc);
	      proc->pc = icopy.pc; // we need to restart the instruction
	      proc->npc = proc->pc+1;
	      break;
	    }
	}
      else
	{
	  /* in this case, we're in the shared segment, so this is
	   definitely a regular seg fault*/
#ifdef COREFILE
	  if (YS__Simtime > DEBUG_TIME)
	    fprintf(corefile,"SEGV in shared region tag %d\n",icopy.tag);
#endif
	  /* fall through */
	}
    }
    /* don't have a break here -- allow the cases where we get a
       non-returnable SEGV error above to fall through naturally */
  case DIV0:
  case FPERR:
  case ILLEGAL:
  case PRIVILEGED:
  case BADPC:
    /* Non returnable error */
    if (except_rate != 0)
      proc->DELAY = pre / except_rate;
    FatalException(&icopy,proc);
    return -1;
    break;
  case BUSERR:
    // this is an alignment failure or some such. Usually it is a
    // non-returnable error. However, this can also be generated if
    // an LDDF or an LDQF is only word-aligned, rather than length-aligned.
    // The latter case is expected to be rare, but must be supported.
    if (except_rate != 0)
      proc->DELAY = pre / except_rate;
    if (((icopy.addr & (mem_length[icopy.code->instruction]-1) != 0)) &&
	((icopy.addr & (mem_align[icopy.code->instruction]-1) == 0)))
      {
	/* Properly handling such unaligned accesses would need more
	   complicated memory simulation, may cause multiple cache
	   lines, multiple page faults, etc. For now, we'll skip that
	   since those are likely to be rare and may be a little
	   difficult to simulate... */

	fprintf(simerr,"Misaligned instruction trap\n");
	instr_func[icopy.code->instruction](&icopy,proc); /* just do the data update and run...*/
	
	if (icopy.code->instruction == iLDDF)
	  proc->physical_fp_reg_file[icopy.lrd] =
	    proc->logical_fp_reg_file[icopy.lrd] = icopy.rs1valf;
	else if (icopy.code->instruction == iLDQF) /* fix later */
	  proc->physical_fp_reg_file[icopy.lrd] =
	    proc->logical_fp_reg_file[icopy.lrd] = icopy.rs1valf;

	/* STDF etc don't actually change the reg. file, so nothing
	   to do there... */
	
	proc->pc = icopy.npc; // go on to next instruction
	proc->npc = proc->pc+1;
	break;
      }
    else
      {
	FatalException(&icopy,proc);
	return -1;
      }
    break;
  case SYSTRAP:  
    /* emulate the system trap */
    if (except_rate != 0)
      proc->DELAY = pre / except_rate;
    reset_lists(proc);
    proc->pc = icopy.npc; // in this case, we don't restart this instruction
    proc->npc = proc->pc+1;
    SysTrapHandle(&icopy,proc);
    break;
  case SOFT_SL_REPL:
    proc->sl_repl_soft_exceptions++;
    // break statement left out intentionally, since the following is
    // for all SL SOFTS, not just from COHE
  case SOFT_SL_COHE:
    proc->sl_soft_exceptions++;
    // break statement left out intentionally, since the following is
    // for all SOFTS, not just SOFT_LIMBO
  case SOFT_LIMBO:
    /* this is a soft exception -- an inplace flush (used for speculative
     load exceptions, for example */
    proc->soft_exceptions++;

    if (soft_rate != 0)
      proc->DELAY = pre / soft_rate;
    reset_lists(proc);
    proc->pc = icopy.pc; // we need to restart the instruction
    proc->npc = icopy.npc; 
    break;
  case SERIALIZE:
    if (except_rate != 0)
      proc->DELAY = pre / except_rate;
    reset_lists(proc);
    if (ProcessSerializedInstruction(&icopy,proc))
      /* if it returns non-zero,then it has set the PC as it desires */
      {
      }
    else
      {
	proc->pc = icopy.npc; // don't restart the instruction separately
	proc->npc = proc->pc + 1;
      }
    break;
  case WINTRAP:
    if (except_rate != 0)
      proc->DELAY = pre / except_rate;
    reset_lists(proc);
    if (icopy.code->wpchange < 0) // save
      TrapTableHandle(&icopy,proc, TRAP_OVERFLOW);
    else // restore
      TrapTableHandle(&icopy,proc, TRAP_UNDERFLOW);
      
     // TTH will set pc, save aside the old pc, etc.
    break;
  default:

    fprintf(simerr," DEfault case in exception.c\n");
    exit(-1);
  }

  return 0;
}

static void FatalException(instance *inst, state *proc)
{
#ifdef COREFILE
  if(proc->curr_cycle > DEBUG_TIME)
    fprintf(corefile, "Non returnable FATAL error!\n"); 
#endif

  fprintf(simerr,"Processor %d dying with FATAL error %d. %g\n",proc->proc_id,inst->exception_code,
	  YS__Simtime);
    
  fprintf(simerr, "pc = %d, tag = %d, %s \n", inst->pc,inst->tag,inames[inst->code->instruction]);
  fprintf(simerr, "lrs1 = %d, prs1 = %d \n",inst->lrs1,inst->prs1);
  fprintf(simerr, "lrs2 = %d, prs2 = %d \n",inst->lrs2,inst->prs2);
  fprintf(simerr, "lrd = %d, prd = %d \n",inst->lrd,inst->prd);

  switch (inst->code->rs1_regtype)
    {
    case REG_INT:
      fprintf(simerr, "rs1i = %d, ",inst->rs1vali);
      break;
    case REG_FP:
      fprintf(simerr, "rs1f = %f, ",inst->rs1valf);
      break;
    case REG_FPHALF:
      fprintf(simerr, "rs1fh = %f, ",double(inst->rs1valfh));
      break;
    case REG_INTPAIR:
      fprintf(simerr, "rs1p = %d/%d, ",inst->rs1valipair.a,inst->rs1valipair.b);
      break;
    case REG_INT64:
      fprintf(simerr, "rs1ll = %lld, ",inst->rs1valll);
      break;
    default: 
      fprintf(simerr, "rs1X = XXX, ");
      break;
    }
  
  switch (inst->code->rs2_regtype)
    {
    case REG_INT:
      fprintf(simerr, "rs2i = %d, ",inst->rs2vali);
      break;
    case REG_FP:
      fprintf(simerr, "rs2f = %f, ",inst->rs2valf);
      break;
    case REG_FPHALF:
      fprintf(simerr, "rs2fh = %f, ",double(inst->rs2valfh));
      break;
    case REG_INTPAIR:
      fprintf(simerr, "rs2pair unsupported");
      break;
    case REG_INT64:
      fprintf(simerr, "rs2ll = %lld, ",inst->rs2valll);
      break;
    default: 
      fprintf(simerr, "rs2X = XXX, ");
      break;
    }
  
  fprintf(simerr, "rsccvali = %d \n", inst->rsccvali);
  
  proc->exit = -1;
  fprintf(simerr,"Processor %d forcing grinding halt with code %d\n",
	  proc->proc_id,inst->rs1vali); 
  exit(1); // no graceful end -- abort all processors immediately
}

static int ProcessSerializedInstruction(instance *inst, state *proc)
{
  switch (inst->code->instruction)
    {
    case iarithSPECIAL2:
      proc->physical_int_reg_file[inst->lrd] =
	proc->logical_int_reg_file[inst->lrd] = inst->rs1vali;
      return 0;
      break;
    case iMULScc:
      fnMULScc_serialized(inst,proc);
      return 0;
      break;
    case iSMULcc:
      fnSMULcc_serialized(inst,proc);
      return 0;
      break;
    case iUMULcc:
      fnUMULcc_serialized(inst,proc);
      return 0;
      break;
    case iLDFSR:
    case iLDXFSR:
      {
	/* we need to load rounding direction and fcc's from the word */
	unsigned RD = (unsigned(inst->rdvali) >> 30) & 3; /* just bits 31-30 */
#ifndef NO_IEEE_FP
	fpsetround(fp_rnd(RD));
#endif
	if (inst->code->instruction == iLDFSR) // FSR
	  {
	    proc->logical_int_reg_file[convert_to_logical(proc->cwp,COND_FCC(0))] =
	      proc->physical_int_reg_file[convert_to_logical(proc->cwp,COND_FCC(0))] =
	      (unsigned(inst->rdvali) >> 10) & 3;
	  }
	else // XFSR
	  {
	    proc->logical_int_reg_file[convert_to_logical(proc->cwp,COND_FCC(0))] =
	      proc->physical_int_reg_file[convert_to_logical(proc->cwp,COND_FCC(0))] =
	      ((unsigned long long)(inst->rdvali) >> 10) & 3;
	    proc->logical_int_reg_file[convert_to_logical(proc->cwp,COND_FCC(1))] =
	      proc->physical_int_reg_file[convert_to_logical(proc->cwp,COND_FCC(1))] =
	      ((unsigned long long)(inst->rdvali) >> 32) & 3;
	    proc->logical_int_reg_file[convert_to_logical(proc->cwp,COND_FCC(2))] =
	      proc->physical_int_reg_file[convert_to_logical(proc->cwp,COND_FCC(2))] =
	      ((unsigned long long)(inst->rdvali) >> 34) & 3;
	    proc->logical_int_reg_file[convert_to_logical(proc->cwp,COND_FCC(3))] =
	      proc->physical_int_reg_file[convert_to_logical(proc->cwp,COND_FCC(3))] =
	      ((unsigned long long)(inst->rdvali) >> 36) & 3;
	  }
      }
      return 0;
      break;
    case iSTFSR:
    case iSTXFSR:
      {
#ifndef NO_IEEE_FP
	unsigned RD = ((unsigned)fpgetround())&3; /* for now, RSIM and host have same rounding direction */
#else
	unsigned RD = 0;
#endif
	unsigned TEM = 31; /* all FP exceptions enabled */
	unsigned NS = 0; /* non-standard FP impl. */
	unsigned VER = 0; /* FP version number -- 7 means no-FPU */
	unsigned FTT = 0; /* trap # */
	unsigned QNE = 0; /*deferred trap queue */
	unsigned fcc0 = proc->logical_int_reg_file[convert_to_logical(proc->cwp,COND_FCC(0))]; /* 2 bits */
	unsigned AEXC = 0; /* accumulated exception, 5 bits */
	unsigned CEXC = 0; /* current exception, 5 bits */
	unsigned fsr = (RD<<30) | (TEM<<23) | (NS<<22) | (VER<<17) |
	  (FTT<<14) | (QNE<<13) | (fcc0<<10) | (AEXC<<5) | (CEXC<<0);
	proc->logical_int_reg_file[convert_to_logical(unsigned(proc->cwp-1) & (NUM_WINS-1),18) /* %l2 */] =
	proc->physical_int_reg_file[convert_to_logical(unsigned(proc->cwp-1) & (NUM_WINS-1),18) /* %l2 */] = inst->addr;
	proc->logical_int_reg_file[convert_to_logical(unsigned(proc->cwp-1) & (NUM_WINS-1),17) /* %l1 */] =
	proc->physical_int_reg_file[convert_to_logical(unsigned(proc->cwp-1) & (NUM_WINS-1),17) /* %l1 */] = fsr;
	if (inst->code->instruction == iSTXFSR)
	  {
	    proc->logical_int_reg_file[convert_to_logical(unsigned(proc->cwp-1) & (NUM_WINS-1),16) /* %l1 */] =
	      proc->physical_int_reg_file[convert_to_logical(unsigned(proc->cwp-1) & (NUM_WINS-1),16) /* %l1 */] = (((proc->logical_int_reg_file[convert_to_logical(proc->cwp,COND_FCC(3))]) << 4) | ((proc->logical_int_reg_file[convert_to_logical(proc->cwp,COND_FCC(2))]) << 2) | ((proc->logical_int_reg_file[convert_to_logical(proc->cwp,COND_FCC(1))]) << 4));
	  }
	/* now trap to the trap table */
	TrapTableHandle(inst,proc,TRAP_STFSR);
	return 1; /* trap table will take care of PC */
      }
      break;
    case iSAVRESTD:
      {
	if (inst->code->aux1 == 0) // SAVE
	  {
	    if (proc->CANSAVE != 0)
	      {
		fprintf(simerr,"Why did we have a SAVED with CANSAVE != 0?");
	      }
	    proc->CANSAVE++;
	    proc->CANRESTORE--;
#ifdef COREFILE
	    if (YS__Simtime > DEBUG_TIME)
	      fprintf(corefile,"SAVED makes CANSAVE %d, CANRESTORE %d\n",proc->CANSAVE,proc->CANRESTORE);
#endif
	  }
	else // RESTORE
	  {
	    if (proc->CANRESTORE != 0)
	      {
		fprintf(simerr,"Why did we have a RESTORED with CANRESTORE != 0?");
	      }
	    proc->CANRESTORE++;
	    proc->CANSAVE--;
#ifdef COREFILE
	    if (YS__Simtime > DEBUG_TIME)
	      fprintf(corefile,"RESTORED makes CANSAVE %d, CANRESTORE %d\n",proc->CANSAVE,proc->CANRESTORE);
#endif
	  }
      }
      break;
    case iDONERETRY:
      {
	proc->privstate=0;
	/* return from trap state */
	if (inst->code->aux1 == 0) // DONE
	  {
	    /* be very careful about if and where this ever gets used */
	    proc->pc = proc->trapnpc;
	    proc->npc = proc->trapnpc+1;
	  }
	else // RETRY
	  {
	    proc->pc = proc->trappc;
	    proc->npc = proc->trapnpc;
	  }
	return 1;
      }
      break;
    default:
      fprintf(simerr,"Unexpected serialized instruction.\n");
      exit(-1);
      break;
    }

  return 0;
}
