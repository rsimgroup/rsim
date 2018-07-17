/*
  exec.cc

  This file simulates the functional unit operation. This is where the
  execution actually takes place. The latencies for the various functional
  units are also defined here.

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
#include "Processor/FastNews.h"
#include "Processor/memory.h"
#include "Processor/exec.h"
#include "Processor/decode.h"
#include "Processor/mainsim.h"
#include "Processor/processor_dbg.h"
#include "Processor/simio.h"
#include <stdlib.h> // for getsubopt

int latencies[numUTYPES];
int repeat[numUTYPES];

typedef int (*latfp)(instance *,state *); // non-deterministic latency function
latfp latfuncs[numUTYPES];

typedef int (*repfp)(instance *,state *); // non-deterministic repeat rate function
repfp repfuncs[numUTYPES];

EFP instr_func[numINSTRS];

int LAT_ALU_MUL=3, LAT_ALU_DIV=9, LAT_ALU_SHIFT=1, LAT_ALU_OTHER=1;
int REP_ALU_MUL=1, REP_ALU_DIV=1, REP_ALU_SHIFT=1, REP_ALU_OTHER=1;
int LAT_FPU_MOV=1, LAT_FPU_CONV=4, LAT_FPU_COMMON=3, LAT_FPU_DIV=10, LAT_FPU_SQRT=10;
int REP_FPU_MOV=1, REP_FPU_CONV=2, REP_FPU_COMMON=1, REP_FPU_DIV=6, REP_FPU_SQRT=6;

int alu_latency(instance *inst, state *proc)
{
  int lat;
  switch (inst->code->instruction)
    {
    case iMULX:
    case iUMUL:
    case iSMUL:
    case iUMULcc:
    case iSMULcc:
      lat=LAT_ALU_MUL; 
      break;
    case iUDIVX:
    case iUDIV:
    case iSDIV:
    case iUDIVcc:
    case iSDIVcc:
      lat=LAT_ALU_DIV;
      break;
    case iSLL:
    case iSRL:
    case iSRA:
      lat=LAT_ALU_SHIFT;
      break;
    default:
      lat = LAT_ALU_OTHER;
      break;
    }
  proc->Running.insert(proc->curr_cycle + lat,
		       inst,inst->tag);
  return lat;
}

int alu_rep(instance *inst, state *proc)
{
  int rep;
  switch (inst->code->instruction)
    {
    case iMULX:
    case iUMUL:
    case iSMUL:
    case iUMULcc:
    case iSMULcc:
      rep=REP_ALU_MUL;
      break;
    case iUDIVX:
    case iUDIV:
    case iSDIV:
    case iUDIVcc:
    case iSDIVcc:
      rep=REP_ALU_DIV;
      break;
    case iSLL:
    case iSRL:
    case iSRA:
      rep=REP_ALU_SHIFT;
      break;
    default:
      rep = REP_ALU_OTHER;
      break;
    }
  proc->FreeingUnits.insert(proc->curr_cycle +rep,
			    uALU);
  return rep;
}

int fpu_latency(instance *inst, state *proc)
{
  int lat;
  switch (inst->code->instruction)
    {
    case iFADDs:
    case iFADDd:
    case iFADDq:
    case iFSUBs:
    case iFSUBd:
    case iFSUBq:
    case iFMULs:
    case iFMULd:
    case iFMULq:
    case iFsMULd:
    case iFdMULq:
    case iFCMPs:
    case iFCMPd:
    case iFCMPq:
    case iFCMPEs:
    case iFCMPEd:
    case iFCMPEq:
      lat = LAT_FPU_COMMON;
      break;
    case iFSQRTs:
    case iFSQRTd:
    case iFSQRTq:
      lat = LAT_FPU_SQRT;
      break;
    case iFDIVs:
    case iFDIVd:
    case iFDIVq:
      lat = LAT_FPU_DIV;
      break;
    case iFsTOx:
    case iFdTOx:
    case iFqTOx:
    case iFxTOs:
    case iFxTOd:
    case iFxToq:
    case iFiTOs:
    case iFiTOd:
    case iFiTOq:
    case iFsTOi:
    case iFdTOi:
    case iFqTOi: // various i->f conversions
      lat=LAT_FPU_CONV;
      break;
    default:
      lat = LAT_FPU_MOV; // move, neg, abs, movcc
      break;
    }
  proc->Running.insert(proc->curr_cycle + lat,
		       inst,inst->tag);
  return lat;
}

int fpu_rep(instance *inst, state *proc)
{
  int rep;
  switch (inst->code->instruction)
    {
    case iFADDs:
    case iFADDd:
    case iFADDq:
    case iFSUBs:
    case iFSUBd:
    case iFSUBq:
    case iFMULs:
    case iFMULd:
    case iFMULq:
    case iFsMULd:
    case iFdMULq:
    case iFCMPs:
    case iFCMPd:
    case iFCMPq:
    case iFCMPEs:
    case iFCMPEd:
    case iFCMPEq:
      rep = REP_FPU_COMMON;
      break;
    case iFSQRTs:
    case iFSQRTd:
    case iFSQRTq:
      rep = REP_FPU_SQRT;
      break;
    case iFDIVs:
    case iFDIVd:
    case iFDIVq:
      rep = REP_FPU_DIV;
      break;
    case iFsTOx:
    case iFdTOx:
    case iFqTOx:
    case iFxTOs:
    case iFxTOd:
    case iFxToq:
    case iFiTOs:
    case iFiTOd:
    case iFiTOq:
    case iFsTOi:
    case iFdTOi:
    case iFqTOi: // various i->f conversions
      rep=REP_FPU_CONV;
      break;
    default:
      rep = REP_FPU_MOV; // move, neg, abs, movcc
      break;
    }
  proc->FreeingUnits.insert(proc->curr_cycle +rep,
			    uFP);
  return rep;
}

/*************************************************************************/
/* UnitSetup : Set up tables that determine the latency of various       */
/*           : functional units + other intialization                    */
/*************************************************************************/

void UnitSetup(state *proc, int except)
{

  if (!FAST_UNITS)
    {
      /* note: The value 0 in the latencies or repeat array indicates that
	 some function must be called in order to calculate the actual
	 latency or repeat rate for the given instruction. */
      latencies[uALU]=0;
      latfuncs[uALU]=alu_latency;
      latencies[uFP]=0;
      latfuncs[uFP]=fpu_latency;
      latencies[uMEM]=0; 
      latfuncs[uMEM]=memory_latency;
      latencies[uADDR]=1;


      if (REP_ALU_MUL == REP_ALU_DIV && REP_ALU_MUL == REP_ALU_SHIFT && REP_ALU_MUL == REP_ALU_OTHER)
	{
	  repeat[uALU]=REP_ALU_MUL;
	}
      else
	{
	  repeat[uALU]=0;
	  repfuncs[uALU]=alu_rep;
	}
      
      repeat[uFP]=0;
      repfuncs[uFP]=fpu_rep;
      repeat[uMEM]=0;
      repfuncs[uMEM]=memory_rep;
      repeat[uADDR]=1;
    }
  else
    {
      // if FAST_UNITS set, do 1 cycle ALU and FPU
      latencies[uALU]=1;
      latencies[uFP]=1;
      latencies[uMEM]=0; 
      latfuncs[uMEM]=memory_latency;
      latencies[uADDR]=1;
      
      repeat[uALU]=1;
      repeat[uFP]=1;
      repeat[uMEM]=0;
      repeat[uADDR]=1;
      repfuncs[uMEM]=memory_rep;
    }
      
  proc->UnitsFree[uALU]=proc->MaxUnits[uALU]=ALU_UNITS;
  proc->UnitsFree[uFP]=proc->MaxUnits[uFP]=FPU_UNITS;
  proc->MaxUnits[uMEM]=MEM_UNITS;

  if (!except)
    {
      proc->UnitsFree[uMEM]=MEM_UNITS; /* we don't reset this on an exception since we
					  don't flush cache ports on exception */
    }
  proc->UnitsFree[uADDR]=proc->MaxUnits[uADDR]=ADDR_UNITS;

  /* also, go through and empty all heaps */
  int Ujunk;
  UTYPE uj;
  tagged_inst ijunktag;
  while (proc->FreeingUnits.GetMin(uj) != -1)
    {
      if (uj == uMEM)
	proc->UnitsFree[uMEM]++;
    }

  for (Ujunk=0; Ujunk<int(numUTYPES); Ujunk++)
    {
      circq<tagged_inst>& q = proc->ReadyQueues[Ujunk];
      while (q.Delete(ijunktag)) ijunktag.inst->tag = -1; // depctr = -2;
    }
}

/*************************************************************************/
/* LetOneUnitStalledGuyGo : free up one instance of a functional unit    */
/*                        : and wake up someone who is waiting for it    */
/*************************************************************************/


static void LetOneUnitStalledGuyGo(state *proc,UTYPE func_unit)
{
  proc->UnitsFree[func_unit]++; /* free up the unit */
  if (func_unit != uMEM)
    {
      instance *i;
      i = proc->UnitQ[func_unit].GetNext(proc);
      if (i != NULL)
	{
	  i->stallqs--;
	  issue(i,proc);
	}
    }
}

#ifdef COREFILE

/*************************************************************************/
/* InstCompletionMessage : print out debug information at complete time  */
/*************************************************************************/

static void InstCompletionMessage(instance *inst, state *proc)
{
  if(proc->curr_cycle > DEBUG_TIME)
    {
      fprintf(corefile, "Completed executing tag = %d, %s, ",
	      inst->tag,inames[inst->code->instruction]);
      switch (inst->code->rd_regtype)
	{
	case REG_INT:
	  fprintf(corefile, "rdi = %d, ",inst->rdvali);
	  break;
	case REG_FP:
	  fprintf(corefile, "rdf = %f, ",inst->rdvalf);
	  break;
	case REG_FPHALF:
	  fprintf(corefile, "rdfh = %f, ",double(inst->rdvalfh));
	  break;
	case REG_INTPAIR:
	  fprintf(corefile, "rdp = %d/%d, ",inst->rdvalipair.a,inst->rdvalipair.b);
	  break;
	case REG_INT64:
	  fprintf(corefile, "rdll = %lld, ",inst->rdvalll);
	  break;
	default: 
	  fprintf(corefile, "rdX = XXX, ");
	  break;
	}
  
      fprintf(corefile, "rccvali = %d \n", inst->rccvali);
    }
  
}
#endif

/*************************************************************************/
/* IssueQueues : Simulate the time spent in execution;move instructions  */
/*             : from the ReadyQueues to the Running queues, and         */
/*             : similarly, move functional units off  FreeingUnits       */
/*************************************************************************/

void IssueQueues(state *proc)
{
  tagged_inst insttagged;
  instance *inst;
#ifdef COREFILE
  if(proc->curr_cycle > DEBUG_TIME)
    fprintf(corefile, "Issuing cycle %d \n",proc->curr_cycle);
#endif
  UTYPE unit_type;
  for (unit_type=UTYPE(0);unit_type<numUTYPES; unit_type=UTYPE(int(unit_type)+1))
    {
      if (unit_type != uMEM)
	{
	  circq<tagged_inst> *q = &(proc->ReadyQueues[unit_type]);
	  while (q->NumInQueue()) /* we have already checked proc->UnitsFree[unit_type] */
	    {
	      q->Delete(insttagged);
	      
	      if (!insttagged.ok())
		{
#ifdef COREFILE
		  if (proc->curr_cycle > DEBUG_TIME)
		    fprintf(corefile,"Nonmatching entry in ReadyQueue -- was %d, now %d\n",insttagged.inst_tag,insttagged.inst->tag);
#endif
		  LetOneUnitStalledGuyGo(proc,unit_type);
		  continue;
		}

	      inst = insttagged.inst;
#ifdef COREFILE
	      if (unit_type != uADDR)
		{
		  
		  if(proc->curr_cycle > DEBUG_TIME)
		    {
		      fprintf(corefile, "Issue tag = %d, %s, ",
			      inst->tag,inames[inst->code->instruction]);
		      
		      switch (inst->code->rs1_regtype)
			{
			case REG_INT:
			  fprintf(corefile, "rs1i = %d, ",inst->rs1vali);
			  break;
			case REG_FP:
			  fprintf(corefile, "rs1f = %f, ",inst->rs1valf);
			  break;
			case REG_FPHALF:
			  fprintf(corefile, "rs1fh = %f, ",double(inst->rs1valfh));
			  break;
			case REG_INTPAIR:
			  fprintf(corefile, "rs1p = %d/%d, ",inst->rs1valipair.a,inst->rs1valipair.b);
			  break;
			case REG_INT64:
			  fprintf(corefile, "rs1ll = %lld, ",inst->rs1valll);
			  break;
			default: 
			  fprintf(corefile, "rs1X = XXX, ");
			  break;
			}
		      
		      switch (inst->code->rs2_regtype)
			{
			case REG_INT:
			  fprintf(corefile, "rs2i = %d, ",inst->rs2vali);
			  break;
			case REG_FP:
			  fprintf(corefile, "rs2f = %f, ",inst->rs2valf);
			  break;
			case REG_FPHALF:
			  fprintf(corefile, "rs2fh = %f, ",double(inst->rs2valfh));
			  break;
			case REG_INTPAIR:
			  fprintf(corefile, "rs2pair unsupported");
			  break;
			case REG_INT64:
			  fprintf(corefile, "rs2ll = %lld, ",inst->rs2valll);
			  break;
			default: 
			  fprintf(corefile, "rs2X = XXX, ");
			  break;
			}
		      
		      fprintf(corefile, "rscc = %d, imm = %d\n",
			      inst->rsccvali, inst->code->imm);
		    }
		}
	      else
		{
		  if(proc->curr_cycle > DEBUG_TIME)
		    fprintf(corefile,"Issue address generation for %d\n",inst->tag);
		}
#endif
	      
	      if (repeat[unit_type])
		proc->FreeingUnits.insert(proc->curr_cycle +repeat[unit_type],unit_type);
	      else
		{
		  (*(repfuncs[unit_type]))(inst,proc);
		}
	      
	      if (latencies[unit_type]) /* deterministic latency */
		proc->Running.insert(proc->curr_cycle + latencies[unit_type],inst,inst->tag);
	      else
		{
		  (*(latfuncs[unit_type]))(inst,proc);
		}
	    }
	}
      else
	{
	  IssueMem(proc);
	}
    }
}

/*************************************************************************/
/* CompleteQueues : Move things from Running queues to Done heap, also   */
/*                : issue instructions for units that are freed up       */
/*************************************************************************/

void CompleteQueues(state *proc)
{
  /* Take off the heap of running instructions that
     completed in time for the next cycle
     Also free up the appropriate units */
  
  int cycle = proc->curr_cycle;
  UTYPE func_unit;
  instance *inst;
  int inst_tag;
  while (proc->Running.num() != 0 && proc->Running.PeekMin() <= cycle)
    {
      proc->Running.GetMin(inst,inst_tag);

      // first check to see if it's been flushed
      if (inst_tag != inst->tag){
	// it's been flushed
#ifdef COREFILE
	if (proc->curr_cycle > DEBUG_TIME)
	  fprintf(corefile,"Got nonmatching tag %d off Running\n",inst_tag);
#endif
	continue;
      }

      if (inst->unit_type != uMEM)
	{
	  (*(instr_func[inst->code->instruction]))(inst,proc);
#ifdef COREFILE
	  if (inst->unit_type != uMEM || !IsStore(inst) || IsRMW(inst) )
	    {
	      if(proc->curr_cycle > DEBUG_TIME)
		InstCompletionMessage(inst,proc);
	    }
#endif      
	  proc->DoneHeap.insert(cycle,inst,inst->tag);
	  
	}
      else
	{
	  // if it's a uMEM, it must just be an address generation thing
#ifdef COREFILE
	  if (proc->curr_cycle > DEBUG_TIME)
	    fprintf(corefile,"Address generated for tag %d\n",inst->tag);
#endif
	  Disambiguate(inst,proc);
	}
    }
  
  while (proc->FreeingUnits.num() != 0 && proc->FreeingUnits.PeekMin() <= cycle)
    {
      proc->FreeingUnits.GetMin(func_unit);
      LetOneUnitStalledGuyGo(proc,func_unit);
    }
}







