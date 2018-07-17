/*

  Processor/state.cc

  This file contains procedures relating to the "state" class. Most of
  the functions in this file are concerned with initialization or statistics
  reporting. "RSIM_EVENT" is activated every cycle to 
  invoke the processor and cache system.

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
#include "Processor/decode.h"
#include "Processor/active.h"
#include "Processor/exec.h"
#include "Processor/memprocess.h"
#include "Processor/mainsim.h"
#include "Processor/freelist.h"
#include "Processor/branchq.h"
#include "Processor/memory.h"
#include "Processor/FastNews.h"
#include "Processor/simio.h"
#include "Processor/units.h"

extern "C"
{
#include "MemSys/simsys.h"
#include "MemSys/req.h"
#include "MemSys/cache.h"
#include "MemSys/arch.h"
#include "MemSys/misc.h"
}

#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <values.h>
#include <time.h>

/* set the names for the latency types -- described in state.h */

static char *lattype_names[lNUM_LAT_TYPES] =
{
  "ALU time", "User 1 time", "User 2 time", "User 3 time",
  "User 4 time", "User 5 time", "User 6 time", "User 7 time", "User 8 time", "User 9 time",
  "Barrier time", "Spin time",
  "Acquire time", "Release time",
  "RMW time", "Write time", "Read time", "Branch time", "FPU time",
  "Except time","MEMBAR time", "BUSY TIME", "Read miss time", "Write miss time", "RMW miss time",
  "Read L1 time", "Read L2 time", "Read localmem time", "Read remotemem time",
  "Write L1 time", "Write L2 time", "Write localmem time", "Write remotemem time",
  "RMW L1 time", "RMW L2 time", "RMW localmem time", "RMW remotemem time",
  "RMW Late PF time", "Write Late PF time", "Read Late PF time"
};

/* set the names for factors leading to efficiency loss (see state.h) */

static char *eff_loss_names[eNUM_EFF_STALLS] =
{
  "OK", "Branch in cycle", "Unpredicted branch",
  "Shadow mappers full", "Rename registers full",
  "Memory queue full", "Issue queue full"
};

static char *fuusage_names[numUTYPES] =
{
  "ALU utilization", "FPU utilization", "Cache ports utilization", "Addr. gen. utilization"
};

/***********************************************************************/
/* mem_map1    : defines a hashing function used in PageTable          */
/***********************************************************************/

static unsigned mem_map1(unsigned k, unsigned sz)
{
  return k&(sz-1);
}

/***********************************************************************/
/* mem_map2    : defines a hashing function used in PageTable          */
/*             : can be machine specific                               */
/***********************************************************************/

static unsigned mem_map2(unsigned k,unsigned sz) // a ROL of 13 or some such
{
  return (((k<< 7) | (k >> (BITS(unsigned)-7))) & (sz-1))*2+1;
}



unsigned lowshared=0x80000000; /* data space goes from 0 through END_OF_HEAP : program
                              BOTTOM_OF_STACK through lowshared-1 : stack
                           lowshared through end of address space : shared */
unsigned highsharedused=0x80000000;

int state::numprocs = 0;
state *AllProcs[MAX_MEMSYS_PROCS];
ARG *AllL1Caches[MAX_MEMSYS_PROCS];
ARG *AllWbuffers[MAX_MEMSYS_PROCS];
ARG *AllL2Caches[MAX_MEMSYS_PROCS];

static int aliveprocs = 0;
static int np = 0;

circq<state *> *state::AllProcessors;
unsigned MAXSTACKSIZE = (1<<20); // 1 Meg by default

/***********************************************************************/
/* ResetInst   : resets an instance                                    */
/***********************************************************************/

static void ResetInst(instance *inst)
{
  inst->tag=-1;inst->inuse=0;
}

/***********************************************************************/
/* ******************** state class constructor ********************** */
/***********************************************************************/


state::state():
  FreeingUnits(ALU_UNITS+FPU_UNITS+ADDR_UNITS+MEM_UNITS) /* each FU can be freed at most once per cycle */
     ,PageTable(mem_map1,mem_map2)
#ifndef STORE_ORDERING
     ,StoresToMem(0)
#else
#endif
     ,ReadyUnissuedStores(0), unissued(0)
{
  AllProcs[numprocs] = this;
  l1_argptr = AllL1Caches[numprocs];
  l2_argptr = AllL2Caches[numprocs];
  wb_argptr = AllWbuffers[numprocs];
  np++;
  aliveprocs++;
  proc_id = numprocs++;
  MemPProcs[proc_id] = this;
  AllProcessors->Insert(this);
  int i,j;
  pc=0;

  copymappernext=0;
  unpredbranch=0;
  
  exit = 0; /* don't exit yet */
  stallq = new stallqueue;
  
  /* Free register lists for integer and FP */

  // ALL THOSE LISTS ARE TAKEN CARE OF IN INIT_DECODE, NOT HERE
  int *origfpfree = new int[NO_OF_PHYSICAL_FP_REGISTERS];
  for (i = 0,j=NO_OF_LOGICAL_FP_REGISTERS;j<NO_OF_PHYSICAL_FP_REGISTERS;i++,j++)
    origfpfree[i]=j;
  free_fp_list = new freelist(NO_OF_PHYSICAL_FP_REGISTERS,origfpfree,
			      NO_OF_PHYSICAL_FP_REGISTERS-NO_OF_LOGICAL_FP_REGISTERS);

  int *origintfree = new int[NO_OF_PHYSICAL_INT_REGISTERS];
  for (i = 0,j=NO_OF_LOGICAL_INT_REGISTERS;j<NO_OF_PHYSICAL_INT_REGISTERS;i++,j++)
    origintfree[i]=j;
  free_int_list = new freelist(NO_OF_PHYSICAL_INT_REGISTERS,origintfree,
			       NO_OF_PHYSICAL_INT_REGISTERS-NO_OF_LOGICAL_INT_REGISTERS);
  
  /* Active_list */
  active_list = new activelist(MAX_ACTIVE_NUMBER);
  decode_rate=NO_OF_DECODES_PER_CYCLE;
  graduate_rate=NO_OF_GRADUATES_PER_CYCLE;
  max_active_list_size = MAX_ACTIVE_NUMBER; //2;

  /* Initialize Ready Queues */
  for (i=0; i<numUTYPES; i++)
    ReadyQueues[i].start(MAX_ACTIVE_INSTS);
  
  /*   instances = new Allocator<instance>(MAX_ACTIVE_NUMBER/2 + 3,ResetInst);  */
  
  instances = new Allocator<instance>(MAX_ACTIVE_INSTS+1, ResetInst);
  /* Note: instances are freed up not only on retirement, but also on
     mispredictions or exceptions, etc. */
  
  bqes = new Allocator<BranchQElement>(MAX_SPEC+2);
  stallqs = new Allocator<stallqueueelement>(MAX_ACTIVE_INSTS + 3);
  ministallqs = new Allocator<MiniStallQElt>((MAX_ACTIVE_INSTS+1)*7); /* The most ministallqelt's we can ever need is for rs1, rs2, rscc, rsd, rs1p, unitQ, and branchdepQ */
  actives = new Allocator<activelistelement>(MAX_ACTIVE_NUMBER + 3);
  tagcvts = new Allocator<TagtoInst>(MAX_ACTIVE_INSTS + 3);
  mappers = new Allocator<MapTable>(MAX_SPEC+1); /* Note: this zeroes out everything; does nto call constructor */
  
  graduation_count=instruction_count=0;
  last_graduated=0;
  cwp=NUM_WINS-1; /* start at the topmost window */
  CANSAVE=NUM_WINS-2; /* subtract 1 for the current window and 1 for the system/trap/overflow window */
  CANRESTORE=0;
  privstate=0;
  curr_cycle=0;
  stall_the_rest=0;
  type_of_stall_rest = eNOEFF_LOSS;
  stalledeff=0;

  BPBSetup(); // setup branch prediction buffer
  RASSetup(); // setup RAS
  
  start_time=start_icount=graduates=0;
  bpb_good_predicts=bpb_bad_predicts=0;
  ras_good_predicts=ras_bad_predicts=0;
  bad_pred_flushes = NewStatrec("Bad prediction flushes",POINT,MEANS,
				NOHIST,8,0.0,double(MAX_ACTIVE_INSTS));
  // total number of instructions flushed on bad predicts

  exceptions=soft_exceptions=sl_soft_exceptions=sl_repl_soft_exceptions=0;
  footnote5=0;
  ldissues=ldspecs=limbos=unlimbos=redos=kills=vsbfwds=fwds=partial_overlaps=0;
  avail_fetch_slots=0;
  
  except_flushed = NewStatrec("Exception flushes",POINT,MEANS,
			      NOHIST,8,0.0,double(MAX_ACTIVE_INSTS));
  // total number of instructions flushed on exceptions
  window_overflows=window_underflows=0;
  SPECS = NewStatrec("Speculation level",POINT,MEANS,NOHIST,MAX_SPEC,0.0,double(MAX_SPEC));

  FUUsage[int(uALU)] = NewStatrec(fuusage_names[uALU], POINT, MEANS, HIST,ALU_UNITS,0.0,double(ALU_UNITS));
  FUUsage[int(uFP)] = NewStatrec(fuusage_names[uFP], POINT, MEANS, HIST,FPU_UNITS,0.0,double(FPU_UNITS));
  FUUsage[int(uMEM)] = NewStatrec(fuusage_names[uMEM], POINT, MEANS, HIST,MEM_UNITS,0.0,double(MEM_UNITS));
  FUUsage[int(uADDR)] = NewStatrec(fuusage_names[uADDR], POINT, MEANS, HIST,ADDR_UNITS,0.0,double(ADDR_UNITS));

#ifndef STORE_ORDERING
  VSB = NewStatrec("Virtual Store Buffer size",POINT,MEANS,NOHIST,10,0.0,100.0);
  LoadQueueSize = NewStatrec("Load queue size", POINT,MEANS,HIST,8,0.0,double(MAX_MEM_OPS));
#else
  MemQueueSize = NewStatrec("Mem queue size", POINT, MEANS, HIST, MAX_MEM_OPS/8,0.0,double(MAX_MEM_OPS));
#endif

  // how long we are at each spec level
  ACTIVELIST = NewStatrec("Active list size",POINT,MEANS,
			  HIST,8,0.0,double(MAX_ACTIVE_INSTS));      
  // size of active list
  agg_lat_type=-1;
  stats_phase=0; // start out in a convenient phase #0
  readacc = NewStatrec("Read accesses",POINT,MEANS,NOHIST,5,0.0,10.0);
  writeacc = NewStatrec("Write accesses",POINT,MEANS,NOHIST,5,0.0,10.0);
  rmwacc = NewStatrec("RMW accesses",POINT,MEANS,NOHIST,5,0.0,10.0);
  
  readiss = NewStatrec("Read accesses (from issue)",POINT,MEANS,NOHIST,5,0.0,10.0);
  writeiss = NewStatrec("Write accesses (from issue)",POINT,MEANS,NOHIST,5,0.0,10.0);
  rmwiss = NewStatrec("RMW accesses (from issue)",POINT,MEANS,NOHIST,5,0.0,10.0);
  
  readact = NewStatrec("Read active",POINT,MEANS,NOHIST,5,0.0,10.0);
  writeact = NewStatrec("Write active",POINT,MEANS,NOHIST,5,0.0,10.0);
  rmwact = NewStatrec("RMW active",POINT,MEANS,NOHIST,5,0.0,10.0);

  for (i=0; i<reqNUM_REQ_STAT_TYPE; i++)
    {
      demand_read[i]=NewStatrec("Demand read",POINT,MEANS,NOHIST,0,0.0,0.0);
      demand_write[i]=NewStatrec("Demand write",POINT,MEANS,NOHIST,0,0.0,0.0);
      demand_rmw[i]=NewStatrec("Demand rmw",POINT,MEANS,NOHIST,0,0.0,0.0);
      
      demand_read_iss[i]=NewStatrec("Demand read (issued)",POINT,MEANS,NOHIST,0,0.0,0.0);
      demand_write_iss[i]=NewStatrec("Demand write (issued)",POINT,MEANS,NOHIST,0,0.0,0.0);
      demand_rmw_iss[i]=NewStatrec("Demand rmw (issued)",POINT,MEANS,NOHIST,0,0.0,0.0);
      
      demand_read_act[i]=NewStatrec("Demand read (active)",POINT,MEANS,NOHIST,0,0.0,0.0);
      demand_write_act[i]=NewStatrec("Demand write (active)",POINT,MEANS,NOHIST,0,0.0,0.0);
      demand_rmw_act[i]=NewStatrec("Demand rmw (active)",POINT,MEANS,NOHIST,0,0.0,0.0);
      
      pref_sh[i]=NewStatrec("pref clean",POINT,MEANS,NOHIST,0,0.0,0.0);
      pref_excl[i]=NewStatrec("pref excl",POINT,MEANS,NOHIST,0,0.0,0.0);
    }
  
  
  in_except = NewStatrec("Waiting for exceptions",POINT,MEANS,NOHIST,5,0.0,50.0);

  for (int lat_ctr=0; lat_ctr < int(lNUM_LAT_TYPES); lat_ctr++)
    {
      lat_contrs[lat_ctr] = NewStatrec(lattype_names[lat_ctr],POINT,MEANS,NOHIST,0,0.0,0.0);
    }

  partial_otime = NewStatrec("Partial Overlap time",POINT,MEANS,NOHIST,0,0.0,0.0);
  avail_fetch_slots=0;
  for (i=0; i<int(lNUM_LAT_TYPES); i++)
    {
      avail_active_full_losses[i]=0;
    }
  for (i=0; i<int(eNUM_EFF_STALLS); i++)
    {
      eff_losses[i]=0;
    }
  
  init_decode(this);
  UnitSetup(this,0);

#ifndef STORE_ORDERING
  SStag=LStag=SLtag=LLtag= -1; /* indicates that anything can pass! */
  MEMISSUEtag=-1;
  // sl_acq = 0;
#endif
 
#ifdef COREFILE
  char proc_file_name[80];
  sprintf(proc_file_name,"corefile.%d",proc_id);
  corefile=fopen(proc_file_name,"w");

#endif
  
  in_exception = NULL;
  MEMSYS=1; /* this is on, unless you do MEMSYS off... */
  curr_limbos = 0;

  time_to_dump=0;

  DELAY=1;

  if (Prefetch)
    {
      typedef instance *instp;
      max_prefs = MEM_UNITS;
      prefrdy = new instp[MEM_UNITS];
    }
}

/***********************************************************************/
/* state::fork() : Duplicate the processor state; used in the process  */
/*               : fork command                                        */
/***********************************************************************/

state *state::fork() const
{
  state *newproc = new state;
  newproc->copy(this);
  if(aliveprocs == 2) /* first parallel proc */
    parelapsedtime = (double) time(0);
  return newproc;
}


/***********************************************************************/
/* state::report_phase  : report statistics at the end of a phase      */
/***********************************************************************/

void state::report_phase() // called when you want to end a phase
{
  fprintf(simout,"PROCESSOR %d Phase %d STATISTICS: \n",proc_id,stats_phase);
  fprintf(simout,"Start cycle: %d\t\ticount: %d\n",start_time,start_icount);
  fprintf(simout,"End cycle: %d\t\ticount: %d\n",curr_cycle,instruction_count);

  report_stats();
  report_phase_fast(); // this gives the critical stats to simerr
  fflush(simout);
}

/*************************************************************************/
/* state::report_partial : report statistics at the end of a timeperiod  */
/*************************************************************************/

void state::report_partial() // called when dumping because of ALRM
{
  report_phase_in("InPhase"); // this gives the critical since last grad
                              // info to simerr
  fflush(simerr);
}

/*************************************************************************/
/* state::report_stats : report statistics at the end of a timeperiod    */
/*************************************************************************/

void state::report_stats()
{
  int i;
  
  StatrecReport(ACTIVELIST);

  StatrecReport(SPECS);
#ifndef STORE_ORDERING
  StatrecReport(VSB);
  StatrecReport(LoadQueueSize);
#else
  StatrecReport(MemQueueSize);
#endif

  fprintf(simout,"BPB Good predictions: %d, BPB Bad predictions: %d, BPB Prediction rate: %f\n",bpb_good_predicts,bpb_bad_predicts,double(bpb_good_predicts)/double(bpb_good_predicts+bpb_bad_predicts));
  fprintf(simout,"RAS Good predictions: %d, RAS Bad predictions: %d, RAS Prediction rate: %f\n",ras_good_predicts,ras_bad_predicts,double(ras_good_predicts)/double(ras_good_predicts+ras_bad_predicts));
  fprintf(simout,"Loads issued: %d, speced: %d, limbos: %d, unlimbos: %d, redos: %d, kills: %d\n",ldissues,ldspecs,limbos,unlimbos,redos,kills);
  fprintf(simout,"Memory unit fwds: %d, Virtual store buffer fwds: %d Partial overlaps: %d\n",fwds,vsbfwds, partial_overlaps);

  StatrecReport(bad_pred_flushes);

  
  fprintf(simout,"Exceptions: %d\n",exceptions);
  fprintf(simout,"Soft Exceptions: %d\n",soft_exceptions);
  fprintf(simout,"SL Soft Exceptions: %d\n",sl_soft_exceptions);
  fprintf(simout,"SL Soft Exceptions (replacements): %d\n",sl_repl_soft_exceptions);
  fprintf(simout,"SL Footnote 5 occurences: %d\n",footnote5);
  fprintf(simout,"Window overflows: %d underflows: %d\n",window_overflows,window_underflows);

  fprintf(simout,"Cycles since last graduation: %d\n",curr_cycle-last_graduated);
  StatrecReport(except_flushed);

  for (i=0; i<numUTYPES; i++)
    {
      if (i != uMEM) /* cache port utilization not really meaningul like others..  */
	{
	  fprintf(simout,"%s: %.1f%%\n",fuusage_names[i],StatrecMean(FUUsage[i]) / double(MaxUnits[i]) * 100.0);
	}
    }
  
#ifdef DETAILED_STATS_LAT_CONTR
  for (i=0; i<numUTYPES; i++)
    StatrecReport(FUUsage[i]);

  for (i=0; i<lNUM_LAT_TYPES;i++)
    StatrecReport(lat_contrs[i]);

  StatrecReport(partial_otime);
  StatrecReport(readacc);
  StatrecReport(writeacc);
  StatrecReport(rmwacc);
  StatrecReport(readiss);
  StatrecReport(writeiss);
  StatrecReport(rmwiss);
  StatrecReport(readact);
  StatrecReport(writeact);
  StatrecReport(rmwact);

#endif
  
  StatrecReport(in_except);

  double drd = StatrecSamples(readacc); 
  double dwt = StatrecSamples(writeacc); 
  double drmw = StatrecSamples(rmwacc); 
  double dmds = drd + dwt + drmw;
  
  for (i=0; i<(int)reqNUM_REQ_STAT_TYPE; i++)
    {
      int d = StatrecSamples(demand_read[i]);
      fprintf(simout,"Demand read %s -- Num %d(%.3f,%.3f) Mean %.3f/%.3f/%.3f Stddev %.3f/%.3f/%.3f\n", Req_stat_type[i], d, d/drd, d/dmds,
	      StatrecMean(demand_read_act[i]),StatrecMean(demand_read[i]),StatrecMean(demand_read_iss[i]),
	      StatrecSdv(demand_read_act[i]),StatrecSdv(demand_read[i]),StatrecSdv(demand_read_iss[i]));
    }
  for (i=0; i<(int)reqNUM_REQ_STAT_TYPE; i++)
    {
      int d = StatrecSamples(demand_write[i]);
      fprintf(simout,"Demand write %s -- Num %d(%.3f,%.3f) Mean %.3f/%.3f/%.3f Stddev %.3f/%.3f/%.3f\n", Req_stat_type[i], d, d/dwt, d/dmds,
	      StatrecMean(demand_write_act[i]),StatrecMean(demand_write[i]),StatrecMean(demand_write_iss[i]),
	      StatrecSdv(demand_write_act[i]),StatrecSdv(demand_write[i]),StatrecSdv(demand_write_iss[i]));
    }
  for (i=0; i<(int)reqNUM_REQ_STAT_TYPE; i++)
    {
      int d = StatrecSamples(demand_rmw[i]);
      fprintf(simout,"Demand rmw %s -- Num %d(%.3f,%.3f) Mean %.3f/%.3f/%.3f Stddev %.3f/%.3f/%.3f\n", Req_stat_type[i], d,d/drmw,d/dmds,
	      StatrecMean(demand_rmw_act[i]),StatrecMean(demand_rmw[i]),StatrecMean(demand_rmw_iss[i]),
	      StatrecSdv(demand_rmw_act[i]),StatrecSdv(demand_rmw[i]),StatrecSdv(demand_rmw_iss[i]));
    }
  for (i=0; i<(int)reqNUM_REQ_STAT_TYPE; i++)
    {
      int d = StatrecSamples(pref_sh[i]);
      fprintf(simout,"Pref sh %s -- Num %d Mean %.3f Stddev %.3f\n", Req_stat_type[i], d, StatrecMean(pref_sh[i]),StatrecSdv(pref_sh[i]));
    }
  for (i=0; i<(int)reqNUM_REQ_STAT_TYPE; i++)
    {
      int d = StatrecSamples(pref_excl[i]);
      fprintf(simout,"Pref excl %s -- Num %d Mean %.3f Stddev %.3f\n", Req_stat_type[i],d,StatrecMean(pref_excl[i]),StatrecSdv(pref_excl[i]));
    }

  fprintf(simout,"\n");

  for (i=0; i <lNUM_LAT_TYPES; i++)
    {      
      fprintf(simout,"Avail loss from %s: %.3f\n",lattype_names[i],
	      double(avail_active_full_losses[i])/
	      (double(decode_rate)*double(curr_cycle-start_time)));
    }
  
  fprintf(simout,"\n");

  for (i=0; i <eNUM_EFF_STALLS; i++) // don't count 0 since this is the OK case
    {      
      fprintf(simout,"Efficiency loss from %s: %.3f\n",eff_loss_names[i],
	      double(eff_losses[i])/double(avail_fetch_slots));
    }

  double ifetch = instruction_count-start_icount;
  
  fprintf(simout,"\n");

  fprintf(simout, "Utility losses from misspecs: %.3f excepts: %.3f\n",
	  StatrecSum(bad_pred_flushes)/ifetch,
	  StatrecSum(except_flushed)/ifetch);

  fprintf(simout,"\n\n\n");  
}

void state::report_phase_in(char *term)
{
  fprintf(simerr,"\nSTAT Proc: %d %s:%d\tTime of last grad: %d\tSince last grad: %d\n",
	  proc_id,term,stats_phase,last_graduated,curr_cycle-last_graduated);
  fprintf(simerr,"STAT Execution time: %d Start time: %d \n",curr_cycle-start_time,start_time);
}

void state::report_phase_fast()
{
  
  fprintf(simerr,"STAT Processor: %d EndPhase: %d Issued: %d Graduated: %d\n",
	  proc_id,stats_phase,instruction_count-start_icount,graduates);
  fprintf(simerr,"STAT Execution time: %d Start time: %d Since last grad: %d\n",
	  curr_cycle-start_time,start_time,curr_cycle-last_graduated);
  for (int i=0; i<lNUM_LAT_TYPES;i++)
    fprintf(simerr,"STAT %s: Grads %d Ratio %.4f\n",lattype_names[i],
	    StatrecSamples(lat_contrs[i]),
	    double(StatrecSamples(lat_contrs[i]))*StatrecMean(lat_contrs[i])/
	    double(curr_cycle-start_time));

  fprintf(simerr,"STAT Window overflows: %d underflows: %d\n",window_overflows,window_underflows);
  fprintf(simerr,"STAT Branch prediction rate: %.4f\n",double(bpb_good_predicts)/double(bpb_good_predicts+bpb_bad_predicts));
  fprintf(simerr,"STAT Return prediction rate: %.4f\n",double(ras_good_predicts)/double(ras_good_predicts+ras_bad_predicts));
  fprintf(simerr,"STAT Reads Mean (ACT): %.3f Stddev: %.3f\n",StatrecMean(readact),StatrecSdv(readact));
  fprintf(simerr,"STAT Writes Mean (ACT): %.3f Stddev: %.3f\n",StatrecMean(writeact),StatrecSdv(writeact));
  fprintf(simerr,"STAT RMW Mean (ACT): %.3f Stddev: %.3f\n",StatrecMean(rmwact),StatrecSdv(rmwact));
  fprintf(simerr,"STAT Reads Mean (EA): %.3f Stddev: %.3f\n",StatrecMean(readacc),StatrecSdv(readacc));
  fprintf(simerr,"STAT Writes Mean (EA): %.3f Stddev: %.3f\n",StatrecMean(writeacc),StatrecSdv(writeacc));
  fprintf(simerr,"STAT RMW Mean (EA): %.3f Stddev: %.3f\n",StatrecMean(rmwacc),StatrecSdv(rmwacc));
  fprintf(simerr,"STAT Reads Mean (ISS): %.3f Stddev: %.3f\n",StatrecMean(readiss),StatrecSdv(readiss));
  fprintf(simerr,"STAT Writes Mean (ISS): %.3f Stddev: %.3f\n",StatrecMean(writeiss),StatrecSdv(writeiss));
  fprintf(simerr,"STAT RMW Mean (ISS): %.3f Stddev: %.3f\n",StatrecMean(rmwiss),StatrecSdv(rmwiss));
  fprintf(simerr,"STAT ExceptionWait Mean: %.3f Stddev: %.3f\n",StatrecMean(in_except),StatrecSdv(in_except));

  double avail = double(avail_fetch_slots)/(double(decode_rate)*double(curr_cycle-start_time));
  double eff = double(instruction_count-start_icount)/double(avail_fetch_slots);
  double util = double(graduates)/double(instruction_count-start_icount);
  
  fprintf(simerr,"STAT Availability: %.3f Efficiency: %.3f Utility: %.3f\n",avail,eff,util);
  
  fprintf(simerr,"\n");
  fflush(simerr);
}

/*************************************************************************/
/* state::end_phase : Full statistics dump at the end of a phase         */
/*************************************************************************/

void state::endphase()
{
  report_phase(); // this gives the full stats dump
  reset_stats();
}

/*************************************************************************/
/* state::newphase: Reset phase collection statistics at new phase       */
/*************************************************************************/

void state::newphase(int phase)
{
  reset_stats(); // to kill the statistics collected in the interphases
  stats_phase=phase;
}

/*************************************************************************/
/* state::reset_stats : Reset phase collection statistics                */
/*************************************************************************/

void state::reset_stats()
{
  int i;
  start_time=curr_cycle;
  start_icount=instruction_count;
  graduates=0;
  bpb_good_predicts=bpb_bad_predicts=0;
  ras_good_predicts=ras_bad_predicts=0;
  StatrecReset(bad_pred_flushes);
  // total number of instructions flushed on bad predicts

  exceptions=soft_exceptions=sl_soft_exceptions=sl_repl_soft_exceptions=0;
  footnote5=0;
  ldissues=ldspecs=limbos=unlimbos=redos=kills=vsbfwds=fwds=partial_overlaps=0;
  avail_fetch_slots=0;

  StatrecReset(except_flushed);
  // total number of instructions flushed on exceptions
  window_overflows=window_underflows=0;
  StatrecReset(SPECS);
  
  for (i=0; i<numUTYPES; i++)
    StatrecReset(FUUsage[i]);
#ifndef STORE_ORDERING
  StatrecReset(VSB);
  StatrecReset(LoadQueueSize);
#else
  StatrecReset(MemQueueSize);
#endif

  // how long we are at each spec level
  StatrecReset(ACTIVELIST);
  // size of active list
  agg_lat_type=-1;
  stats_phase=-1;
  StatrecReset(readacc);
  StatrecReset(writeacc);
  StatrecReset(rmwacc);
  StatrecReset(readact);
  StatrecReset(writeact);
  StatrecReset(rmwact);
  StatrecReset(readiss);
  StatrecReset(writeiss);
  StatrecReset(rmwiss);
  StatrecReset(in_except);
  for (i=0; i<reqNUM_REQ_STAT_TYPE; i++)
    {
      StatrecReset(demand_read[i]);
      StatrecReset(demand_write[i]);
      StatrecReset(demand_rmw[i]);

      StatrecReset(demand_read_act[i]);
      StatrecReset(demand_write_act[i]);
      StatrecReset(demand_rmw_act[i]);

      StatrecReset(demand_read_iss[i]);
      StatrecReset(demand_write_iss[i]);
      StatrecReset(demand_rmw_iss[i]);

      StatrecReset(pref_sh[i]);
      StatrecReset(pref_excl[i]);
    }


  for (int lat_ctr=0; lat_ctr < int(lNUM_LAT_TYPES); lat_ctr++)
    {
      StatrecReset(lat_contrs[lat_ctr]);
    }

  StatrecReset(partial_otime);
  avail_fetch_slots=0;
  for (i=0; i<int(lNUM_LAT_TYPES); i++)
    {
      avail_active_full_losses[i]=0;
    }
  for (i=0; i<int(eNUM_EFF_STALLS); i++)
    {
      eff_losses[i]=0;
    }
}

/*************************************************************************/
/* state::copy   : copy state from one data structure to the other       */
/*************************************************************************/

void state::copy(const state *proc)
{
  // To fork processor proc, do the following
  // newproc = new state;
  // newproc->copy(proc);
  cwp = proc->cwp;
  CANSAVE = proc->CANSAVE;
  CANRESTORE = proc->CANRESTORE;
  pc = proc->pc;
  npc = proc->npc;
  exit = proc->exit;
  MEMSYS = proc->MEMSYS;
  highheap = proc->highheap;
  lowstack = proc->lowstack;

  last_graduated = curr_cycle = proc->curr_cycle;
  start_time=proc->curr_cycle;
  agg_lat_type=proc->agg_lat_type;
  stats_phase=proc->stats_phase;
  
  memcpy(logical_int_reg_file,proc->logical_int_reg_file,sizeof(logical_int_reg_file));
  memcpy(logical_fp_reg_file,proc->logical_fp_reg_file,sizeof(logical_fp_reg_file));
  memcpy(physical_int_reg_file,proc->physical_int_reg_file,sizeof(physical_int_reg_file));
  memcpy(physical_fp_reg_file,proc->physical_fp_reg_file,sizeof(physical_fp_reg_file));
  // the above copies straight over since the pages themselves are going to be all forked

  unsigned pg;
  for (pg=0;pg<highheap/ALLOC_SIZE; pg++)
    {
      unsigned oldaddr;
      if (proc->PageTable.lookup(pg,oldaddr))
	{
	  char *newpg = (char *)malloc(ALLOC_SIZE);
	  
	  memcpy(newpg,(char *)oldaddr,ALLOC_SIZE);
	  PageTable.insert(pg,(unsigned) newpg);
	}
    }
  for (pg=lowshared/ALLOC_SIZE - 1; pg >= lowstack/ALLOC_SIZE; pg--)
    {
      unsigned oldaddr;
      if (proc->PageTable.lookup(pg,oldaddr)) /* in the case of the stack, they should all be present */
	{
	  char *newpg = (char *)malloc(ALLOC_SIZE);
	  
	  memcpy(newpg,(char *)oldaddr,ALLOC_SIZE);
	  PageTable.insert(pg,(unsigned) newpg);
	}
    }

#ifndef STORE_ORDERING
  SStag=LStag=SLtag=LLtag= -1; /* indicates that anything can pass! */
  MEMISSUEtag=-1;
#endif
}

/*************************************************************************/
/* RSIM_EVENT  : The main process event; gets called every cycle         */
/*             : performs the main processor functions                   */
/*             : The main loop calls RSIM_EVENT for each processor every */
/*             : cycle (cycle-by-cycle processor simulation stage)       */
/*************************************************************************/

extern "C" void RSIM_EVENT()
{
  int curtime = (int) YS__Simtime;
  int nondelayed;
  int runL1,runL2,runproc;

  /* ********************* We have some delay when we wait for the cache clock
     and the processor clock to synchronize. *********** */
  
  runL1 = (FASTER_PROC_L1 == 1) || (curtime % FASTER_PROC_L1 == 0);
  runL2 = (FASTER_PROC == 1) ||  (curtime % FASTER_PROC == 0) ;
  runproc = (FASTER_NET == 1) || (curtime % FASTER_NET == 0);

  /* Loop through each processor and advance simulation by a cycle */
  for (int i=0; i<np; i++)
    {
      state *proc = AllProcs[i];
      proc->curr_cycle = curtime;
#ifdef COREFILE
      corefile=proc->corefile;
#endif
      nondelayed = runproc && (--proc->DELAY <= 0);
      // if runproc is 0, make nondelayed == 0 regardless
      
      if (runL1 &&                                   /* this is the the time to run the L1 */
	  (!proc->l1_argptr->mptr->pipe_empty ||     /* There is something to do at L1 */
	   (proc->wb_argptr &&                       /* Or there is somthing at the WB */
	    !(proc->wb_argptr->mptr->pipe_empty && proc->wb_argptr->mptr->inq_empty) ) ) )
	{
	  L1CacheOutSim(proc);          /*************************
					  Handle requests in the
					  pipelines of the L1 cache
					  *************************/
        
	}
	   
      if(runL2 &&                                    /* time to run the L2 */
	 !(proc->l2_argptr->mptr->pipe_empty))       /* and there is something to do */
	{
	  L2CacheOutSim(proc);  /*************************
				  Handle requests in the
				  pipelines of the L2 cache
				  *************************/
	}


      if (nondelayed && !proc->exit) /* no delay present, try to fetch,etc. */
	{
	  /* now, note availability */

	  if (proc->in_exception != NULL && !(proc->exit))
	    {
	      ComputeAvail(proc);
	      PreExceptionHandler(proc->in_exception,proc);
	    }

#ifdef COREFILE
	  if(proc->curr_cycle > DEBUG_TIME)
	    fprintf(corefile,"Completion cycle %d \n",proc->curr_cycle);
#endif
	  
	  CompleteMemQueue(proc);
	  CompleteQueues(proc);    /*************************
				     Completion stage of the
				     pipeline
				     **************************/
	  
	  if (proc->in_exception == NULL && !(proc->exit))
	    {
	      maindecode(proc);    /*************************
				     Main processor pipeline
				     *************************/
	    }

	  if (proc->exit)
	    {
	      aliveprocs--;
	      if (aliveprocs == 1) /* only uniprocessor left */
          	parelapsedtime = (double)time(0) - parelapsedtime;
	      if (aliveprocs == 0)
		return;
	      /* otherwise, just keep running, since caches might
		 still need to service INVL requests, etc. */
	    }
	  
	    if (!proc->DELAY)
	      {
		IssueQueues(proc);   /*********************
				       Issue to queues
				       ********************/

		proc->DELAY=1;
	      }
	    
	    StatrecUpdate(proc->SPECS,double(proc->branchq.NumItems()),1.0);

	    for (int ctrfu=0; ctrfu<numUTYPES; ctrfu++)
	      {
		StatrecUpdate(proc->FUUsage[ctrfu],
			      double(proc->MaxUnits[ctrfu]-proc->UnitsFree[ctrfu]),
			      1.0);
	      }

#ifndef STORE_ORDERING
	    StatrecUpdate(proc->VSB,double(proc->StoresToMem),1.0);
	    StatrecUpdate(proc->LoadQueueSize,double(proc->LoadQueue.NumItems()),1.0);
#else
	    StatrecUpdate(proc->MemQueueSize,double(proc->MemQueue.NumItems()),1.0);
#endif
	    StatrecUpdate(proc->ACTIVELIST,double(proc->active_list->NumElements()),1.0);
	}
      if(runL1 &&                           /* If we need to run L1 and inq is not empty */
         !(proc->l1_argptr->mptr->inq_empty))
	{
	  L1CacheInSim(proc); /****************************
				Handle requests coming into
				L1 cache
				***************************/
	}
      
      if(runL2 &&
         !(proc->l2_argptr->mptr->inq_empty))
	{
	  L2CacheInSim(proc);            /****************************
					   Handle requests coming into
					   L2 cache
					 ****************************/

	}
    }

  /* Schedule the main processorloop for next cycle */
  ActivitySchedTime(ME,1.0,INDEPENDENT);
}



/*************************************************************************/
/* init_decode  : Initializes everything that is needed for the decode   */
/*              : stage                                                  */
/*************************************************************************/

void init_decode(state *proc)
{
  /* initialize the free lists and busy lists*/
  proc->intregbusy = new int[NO_OF_PHYSICAL_INT_REGISTERS];
  proc->fpregbusy = new int[NO_OF_PHYSICAL_FP_REGISTERS];

  proc->activemaptable = NewMapTable(proc);
  if (proc->activemaptable == NULL)
    {
      fprintf(simerr,"Got a NULL map table entry!!\n");
      exit(-1);
    }
  proc->fpmapper = proc->activemaptable->fmap;
  proc->intmapper = proc->activemaptable->imap;

  reset_lists(proc);
  
  proc->instruction_count = 0;
  proc->curr_cycle = 0;
  proc->cwp = NUM_WINS-1;
  proc->CANSAVE = NUM_WINS-2;
  proc->CANRESTORE = 0;
  proc->privstate=0;

  /* intialize the branch queue */

  /* Initialize the tag to instance converter */
  proc->tag_cvt = new circq<TagtoInst *>(MAX_ACTIVE_INSTS+3);
}

/*************************************************************************/
/* reset_lists  : Initialize the register files and the mappers          */
/*************************************************************************/

int reset_lists(state *proc)
{
  /* Set busy register lists and free register lists */
  
  proc->instances->reset();

  proc->copymappernext=0;
  proc->unpredbranch=0;
  unstall_the_rest(proc);
  
  proc->intregbusy[ZEROREG] = 0;
  int i;
  memset((char *)proc->fpregbusy,0,NO_OF_PHYSICAL_FP_REGISTERS * sizeof(int));
  memset((char *)proc->intregbusy,0,NO_OF_PHYSICAL_INT_REGISTERS * sizeof(int));
  
  
  proc->free_int_list->reset();
  proc->free_fp_list->reset();
  
  /* Initialize the mappers */
  for(i=0;i<NO_OF_LOGICAL_FP_REGISTERS;i++)
    {
      proc->fpmapper[i] = i;
    }
  
  for(i=0;i<NO_OF_LOGICAL_INT_REGISTERS;i++)
    {
      proc->intmapper[i] = i;
    }

  memcpy(proc->physical_fp_reg_file,proc->logical_fp_reg_file,
	 NO_OF_LOGICAL_FP_REGISTERS*sizeof(double));
  memcpy(proc->physical_int_reg_file,proc->logical_int_reg_file,
	 NO_OF_LOGICAL_INT_REGISTERS*sizeof(int));
  
  /* Note :The CURRENT WINDOW POINTER REMAINS UNCHANGED */
  
  return 0;
}

/*************************************************************************/
/* ComputeAvail  : Compute the availability performance metric. For more */
/*               : details on the definition see BennetFlynn1995         */
/*************************************************************************/

void ComputeAvail(state *proc)
{
  int avails = proc->active_list->NumAvail();
  if (avails < proc->decode_rate)
    {
      if (proc->in_exception != NULL)
	{
	  // credit all the losses to exception
	  proc->avail_active_full_losses[lEXCEPT]+= proc->decode_rate-avails;
	}
      else
	{
	  // find out why
	  instance *avloss = GetHeadInst(proc);
	  proc->avail_active_full_losses[lattype[avloss->code->instruction]]+= proc->decode_rate-avails;
	}
    }
  else
    {
      avails = proc->decode_rate;
    }
  
  proc->stalledeff += avails;
  proc->avail_fetch_slots += avails;
}








