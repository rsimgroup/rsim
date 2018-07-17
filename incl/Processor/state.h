/*****************************************************************************/
/*                                                                           */
/*   state.h :   State class definition and other related definitions        */
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


#ifndef _state_h_
#define _state_h_ 1

#include "instruction.h"
#include "instance.h"
#include "heap.h"
#include "instheap.h"
#include "alloc.h"
#include "allocator.h"
#include "memq.h"
#include "units.h"
#include "stallq.h"
#include "tagcvt.h"
#include "circq.h"
#include "active.h"
#include "branchq.h"
#include "archregnums.h"
#include <stdio.h>
extern "C"
{
#include "MemSys/typedefs.h"
#include "MemSys/req.h"         /* that file contains all the types we need! */
}

extern int entry_pt;          /* entry point for code */
extern int pcbase;            /* first pc represented in code --
				 currently must be same as entry_pt*/
extern unsigned lowshared;    /* data space goes from
				 0 through END_OF_HEAP : program
				 BOTTOM_OF_STACK through lowshared-1: stack
				 lowshared through end of addr space: shared */
extern unsigned highsharedused;  /* high shared address */


extern unsigned MAXSTACKSIZE; // 1 Meg by default

/* Maximum number of elements in the active list */
extern int MAX_ACTIVE_NUMBER;
extern int MAX_ACTIVE_INSTS;
#define MAX_MAX_ACTIVE_NUMBER 4096 

extern int NO_OF_DECODES_PER_CYCLE; // 4
extern int NO_OF_GRADUATES_PER_CYCLE; // 4

/* Maximum number of speculations allowed */
extern int MAX_SPEC; // 8

#define ALLOC_SIZE 4096    /* this is the page size */

extern int SZ_BUF, RAS_STKSZ;  /* size of Branch Pred buffer & return address stack */

#define DEFMAX_MEM_OPS 32 /* this number is chosen because it is twice the
			     number of the MIPS R10K */

extern int STALL_ON_FULL; /* does the system model separate issue queue? */
extern int MAX_MEM_OPS, MAX_ALUFPU_OPS; /* 1) max mem ops in mem queue
					   2) if STALL_ON_FULL, max unissued
					      CPU ops in proc at a time */

/* number of functional units of each type */
extern int ALU_UNITS, FPU_UNITS, MEM_UNITS, ADDR_UNITS;
extern int NUM_WINS; /* number of register windows: must be a power of 2. */
#define MAX_NUM_WINS 32
#define MIN_NUM_WINS 4

#define NO_OF_LOGICAL_INT_REGISTERS  (MAX_NUM_WINS*16 + WINSTART)
#define NO_OF_LOGICAL_FP_REGISTERS 32 
#define NO_OF_PHYSICAL_INT_REGISTERS (NO_OF_LOGICAL_INT_REGISTERS + MAX_ACTIVE_NUMBER)
#define NO_OF_PHYSICAL_FP_REGISTERS (NO_OF_LOGICAL_FP_REGISTERS + MAX_ACTIVE_NUMBER)

#include "hash.h"


/*****************************************************************************/
/*  convert architectural register to logical register (SPARC_specific)      */
/*****************************************************************************/

inline int convert_to_logical(int cwp,int iarch)
{
  if (iarch < 8)
    return iarch;

  if (iarch == COND_XCC)
    iarch = COND_ICC; /* map/rename these two as a single reg. in this fashion */

  if (iarch >=32)
    return iarch-24;

  /* WINDOW STRUCTURE IS AS FOLLOWS (i:input, l:local, o:output)
     (this shows a 4 window system)
     
     i     o
WP3  l
     o i
WP2    l
       o i
WP1      l
	 o i
WP0	   l */

  if (iarch >= 16)
    return WINSTART+16*(cwp&(NUM_WINS-1))+(iarch-16); /* i7-i0,l7-l0 */
  else /* It's an "o" register */
    return WINSTART+16*((cwp-1)&(NUM_WINS-1))+(iarch); /* o7-o0 */
}

#define SIZE_OF_SPARC_INSTRUCTION 4

/*****************************************************************************/
/* InstrNumToAddr: convert given pc from instr num to the equivalent address */
/*****************************************************************************/

inline int InstrNumToAddr(int pc)
{
  return (pc * SIZE_OF_SPARC_INSTRUCTION) + pcbase; /* entry point for code */
}

/*****************************************************************************/
/* InstrAddrToNum: convert given pc from hex addr to equivalent instr num    */
/*****************************************************************************/
inline int InstrAddrToNum(int addr, int &badpc)
{
  badpc = 0;
  if (addr & (SIZE_OF_SPARC_INSTRUCTION - 1))
    badpc = 1;
  return (addr - pcbase) / SIZE_OF_SPARC_INSTRUCTION; /* converts to pc */
}

/******************************************************************/
/****************** MembarInfo structure definition ***************/
/******************************************************************/
struct MembarInfo
{
  int tag;                /* instruction tag     */
  int SS:1;               /* store store membar? */
  int LS:1;               /* load store membar?  */
  int SL:1;               /* store load membar?  */
  int LL:1;               /* load load membar?   */
  int MEMISSUE:1;         /* blocks all memory issue */
  
  int operator == (struct MembarInfo x) {return tag==x.tag;}
  int operator <= (struct MembarInfo x) {return tag<=x.tag;}
  int operator >= (struct MembarInfo x) {return tag>=x.tag;}
  int operator < (struct MembarInfo x) {return tag<x.tag;}
  int operator > (struct MembarInfo x) {return tag>x.tag;}
  int operator != (struct MembarInfo x) {return tag!=x.tag;}
};

/******************************************************************/
/****************** tagged_inst structure definition **************/
/******************************************************************/
/* Used for detecting changes to instance tag */

struct tagged_inst
{
  instance *inst;
  int inst_tag;
  tagged_inst() {}
  tagged_inst(instance *i):inst(i),inst_tag(i->tag) {}
  int ok() const {return inst_tag==inst->tag;}
};
/* Statistics: Efficiency characterization. For more details, refer to
   BennetFlynn1995  TR */
enum eff_loss_stall{
  eNOEFF_LOSS,          /* no efficiency loss                      */
  eBR,                  /* branch-related efficiency losses        */
  eBADBR,               /* branch-related efficiency losses        */
  eSHADOW,              /* loss due to shadow mappers full         */
  eRENAME,              /* loss due to inadequate rename           */
  eMEMQFULL,            /* loss due to memory queue full           */
  eISSUEQFULL,          /* loss due to issue queue full            */
  eNUM_EFF_STALLS       /* number of classes of efficiency loss    */
};


/********************************************************************/
/*********** Branch predictor enumerated type definition ************/
/********************************************************************/

enum bptype {
  TWOBIT,            /* Two-bit hardware bimodal prediction scheme */
  TWOBITAGREE,         /* Two-bit hardware agree prediction scheme */
  STATIC                               /* static prediction scheme */
};
extern bptype BPB_TYPE;

/********************************************************************/
/*********************** state class  definition ********************/
/********************************************************************/

struct state
{
  /* The state class represents the state of an individual processor,
     and is separate for the different processors in the MP case   */

  int proc_id;				/* processor id            */
  static int numprocs;			/* number of processors    */
  static circq<state *> *AllProcessors;	/* pointers to state of all
						  processors       */
  int decode_rate;			/* decode rate of proc     */
  int graduate_rate;			/* graduate rate of proc   */
  int max_active_list_size;		/* instruction window size */
  
  int cwp; 				/* current window pointer  */
  int CANSAVE;                          /* # of reg. wins. that can be
					   saved before trapping */
  int CANRESTORE;                       /* # of reg. wins. that can be
					   restored before trapping*/
  
  int pc; 				/* the current pc            */
  int npc; 				/* the next pc to fetch from */
  int copymappernext;     /* copy shadow mapper on next instrn (delay slot) */
  int unpredbranch;                     /* Does the processor have an
					   unpredicted branch outstanding? */

  int privstate;                        /* Processor is in privileged mode */
  int trappc, trapnpc;			/* PC and NPC saved at time of trap
					   -- return here after trap */
  
  int exit; 				/* set by exit trap handler */
  
  circq<tagged_inst> ReadyQueues[numUTYPES]; /* data structure which keeps
						  track of units that are
						  ready to issue    */

  Heap<UTYPE> FreeingUnits;		/* data structure which keeps track of
					   when units get freed      */
  int UnitsFree[numUTYPES];             /* number of units free of each type */
  MiniStallQ UnitQ[numUTYPES];		/* data structure which keeps track of
					  instructions stalled at each unit */
  MiniStallQ BranchDepQ;		/* data structure which keeps track of
					  stalled branch instructions       */
  int MaxUnits[numUTYPES];		/* maximum number of FU's per type  */
  InstHeap Running;                     /* data structure which keeps track of
					   when instructions complete */

  unsigned highheap; 			/* The extent of the processor heap */
  unsigned lowstack; 			/* The low address of the stack */
  HashTable<unsigned, unsigned> PageTable; /* (mem_map1,mem_map2) are the
					    hashing functions */
        
  class stallqueue *stallq;             /* Holds stalled instruction if
					   processor runs out of renaming
					   regs, active list size, etc. */
  
  /* Free register lists for integer and FP */
  class freelist *free_fp_list;
  class freelist *free_int_list;
  
  /* Active_list */
  class activelist *active_list;
    
  /* Branch Queue class definition */
  MemQ<class BranchQElement *> branchq;
    
  /* DoneHeap definition*/
  InstHeap DoneHeap;			/* keeps track of instructions that
					  are done                         */
  InstHeap MemDoneHeap;			/* keeps track of memory instructions
					  that are done                    */
  
  /* Mappers from logical to physical for int and FP*/
  int *fpmapper;			/* fp logical-to-physical mapper   */
  int *intmapper;			/* int logical-to-physical mapper  */
  MapTable *activemaptable; 		/* current mappers,
					to distinguish from shadow mappers */
    
  /* Busy physical registers indicators: 1 if busy and 0 if not busy       */
  int *fpregbusy;			/* busy table for fp registers     */
  int *intregbusy;			/* busy table for int registers    */

  circq<TagtoInst *> *tag_cvt;		/* tagcvt queue definition         */
  int instruction_count;		/* total number of instructions    */
  int graduation_count;			/* number of graduated instrns     */
  int curr_cycle;			/* current simulated cycle         */

  int stall_the_rest;			/* flag indicating processor stall */
  eff_loss_stall type_of_stall_rest;	/* classification of stall         */
  int stalledeff;			/* efficiency loss due to stall    */

  Allocator<instance> *instances; 	/* pool of instances               */
  Allocator<BranchQElement> *bqes;	/* pool of branch queue elements   */
  Allocator<MapTable> *mappers;		/* pool of shadow mappers          */
  Allocator<stallqueueelement> *stallqs;/* pool of stall queues            */
  Allocator<MiniStallQElt> *ministallqs;/* pool of mini stall queues       */
  Allocator<activelistelement> *actives;/* pool of active list elements    */
  Allocator<TagtoInst> *tagcvts;	/* pool of tagcvt elements         */

#ifndef STORE_ORDERING
  MemQ<instance *> LoadQueue;		/* load queue                       */
  MemQ<instance *> StoreQueue;		/* store queue                      */
  int StoresToMem;			/* keep track of stores outstanding */

  MemQ<int> st_tags;			/* list of store tags               */
  MemQ<int> rmw_tags;			/* list of rmw tags                 */
  MemQ<MembarInfo> membar_tags;		/* list of membar tags              */
  
  int SStag,LStag,SLtag,LLtag,MEMISSUEtag;/* identify membar tags           */
  int minload, minstore;		/* last load and store instr. tags  */
#else
  MemQ<instance *> MemQueue;		/* unified memory queue             */
#endif
  int ReadyUnissuedStores;		/* ready but unissued stores        */
  int unissued; 			/* counts number of ALU & FPU ops that
					haven't issued yet...               */
  
  MemQ<int> ambig_st_tags;		/* ambiguous store tags */
  
  /* Register files for integer and floating point */
  int logical_int_reg_file[NO_OF_LOGICAL_INT_REGISTERS];
  double logical_fp_reg_file[NO_OF_LOGICAL_FP_REGISTERS];
  int physical_int_reg_file[NO_OF_LOGICAL_INT_REGISTERS+MAX_MAX_ACTIVE_NUMBER];
  double physical_fp_reg_file[NO_OF_LOGICAL_FP_REGISTERS+MAX_MAX_ACTIVE_NUMBER];
  MiniStallQ dist_stallq_int[NO_OF_LOGICAL_INT_REGISTERS+MAX_MAX_ACTIVE_NUMBER];
  MiniStallQ dist_stallq_fp[NO_OF_LOGICAL_FP_REGISTERS+MAX_MAX_ACTIVE_NUMBER];

  int *BranchPred;			/* 1st bit of 2-bit branch predictor */
  int *PrevPred;                        /* 2nd bit of 2-bit branch predictor */
  int *ReturnAddressStack;              /* return address predictor */
  int rasptr;				/* return address stack pointer  */

  int last_graduated;			/* last graduated instruction */
  int last_counted;			/* last instruction */
  
  /*************************** Statistics *****************************/

  int start_time;			/* start time for stats collection */
  int start_icount;			/* start instruction count */
  
  int graduates;			/* number of graduates */
  
  int bpb_good_predicts;		/* number of correct predictions   */
  int ras_good_predicts;		/* number of correct returns       */
  int bpb_bad_predicts;			/* number of wrong predictions     */
  int ras_bad_predicts;			/* number of bad returns           */
  STATREC *bad_pred_flushes;		/* impact of mispredictions        */

  int exceptions;			/* number of exceptions            */
  int soft_exceptions;			/* number of soft exceptions       */
  int sl_soft_exceptions;		/* soft excepts due to spec loads
					   killed by coherence             */
  int sl_repl_soft_exceptions;		/* soft excepts due to spec loads
					   killed by replacement from L2   */
  int footnote5; 			/* ones that get reissued by spec loads
					-- footnote 5 of Gharachorloo et al
					                 ICPP 1991         */

  STATREC *except_flushed;		/* impact of exceptions            */
  int window_overflows, window_underflows;  /* number of window traps for
					       overflows/underflows        */
  STATREC *SPECS; 			/*  time at each spec level        */
  STATREC *ACTIVELIST; 			/* size of active list                 */
  STATREC *FUUsage[numUTYPES];          /* utilization of functional units     */
#ifndef STORE_ORDERING
  STATREC *VSB; 			/* average virtual store buffer size */
  STATREC *LoadQueueSize;		/* load queue size                   */
#else
  STATREC *MemQueueSize;		/* memory queue size                 */
#endif

  STATREC *lat_contrs[lNUM_LAT_TYPES];	/* execution time components         */
  STATREC *partial_otime;		/* partial overlap times             */
  int agg_lat_type; 			/* means all lat_contrs count for
					this one type; -1 means off          */
  
  int stats_phase;			/* stats collection phase            */

  STATREC *in_except;                   /* time spent waiting to trap */
  
  /* classify read, write, and rmw times based on different metrics */
  STATREC *readacc, *writeacc, *rmwacc;
  STATREC *readiss, *writeiss, *rmwiss;
  STATREC *readact, *writeact, *rmwact;

  STATREC *demand_read[reqNUM_REQ_STAT_TYPE];
  STATREC *demand_write[reqNUM_REQ_STAT_TYPE];
  STATREC *demand_rmw[reqNUM_REQ_STAT_TYPE];

  STATREC *demand_read_iss[reqNUM_REQ_STAT_TYPE];
  STATREC *demand_write_iss[reqNUM_REQ_STAT_TYPE];
  STATREC *demand_rmw_iss[reqNUM_REQ_STAT_TYPE];

  STATREC *demand_read_act[reqNUM_REQ_STAT_TYPE];
  STATREC *demand_write_act[reqNUM_REQ_STAT_TYPE];
  STATREC *demand_rmw_act[reqNUM_REQ_STAT_TYPE];

  /* prefetch stats */
  STATREC *pref_sh[reqNUM_REQ_STAT_TYPE];
  STATREC *pref_excl[reqNUM_REQ_STAT_TYPE];

  /* classification of loads */
  int ldissues, ldspecs, limbos, unlimbos, redos, kills;

  /* forwarding stats */
  int vsbfwds, fwds, partial_overlaps;

  /* availability, efficiency and utility (BennetFlynn1995 TR)  */
  int avail_fetch_slots; 	/* while executing, just add into these */
  int avail_active_full_losses[lNUM_LAT_TYPES]; /* number of available slots
   						   lost to each cause */
  int eff_losses[eNUM_EFF_STALLS]; /* efficiency losses from each cause */
  
/**************** end of stats block ********************/

  volatile int time_to_dump; 	/* period of ALRM messages and
				   partial statistics                */
  
  instance *in_exception;	/* instance causing exception        */
  int time_pre_exception;	/* time before exception             */

  int MEMSYS;			/* is MemSys mode on?                */
  
  FILE *corefile;		/* corefile file pointer             */
  
  state();			/* constructor                       */
  ~state()			/* destructor                        */
	{if (corefile) fclose(corefile);}

  void copy(const state *); 	/* copy state into another proc      */
  state *fork() const; 
  
  int SPARCtoLog(int iarch)	/* convert SPARC int register to logical reg */
	{return convert_to_logical(cwp,iarch);}
  /* turns cwp, iarch pair into il */


  /* branch prediction functions */
  void BPBSetup(); /* set up branch prediction table */
  int  BPBPredict(int bpc, int statpred); /* returns predicted pc    */
  void BPBComplete(int bpc, int taken, int statpred); /* resolves specultns */
  void RASSetup(); /* set up return-address predictor */
  void RASInsert(int newpc); /* insert on a CALL                     */
  int RASPredict(); /* this is a destructive prediction              */

  /* Stats */
  void report_stats();
  void reset_stats();
  void report_phase();
  void report_phase_fast();
  void report_phase_in(char *);
  void report_partial();
  void endphase();
  void newphase(int);

  int curr_limbos;		/* number of loads past unambiguated stores */

  int DELAY;                    /* Is the processor stalling for any reason */

  /* prefetch stats */
  int prefs;
  int max_prefs;
  instance **prefrdy; // prefetch slots

  /* Memory hierarchy variables */
  ARG *l1_argptr;
  ARG *wb_argptr;
  ARG *l2_argptr;
};

/* Other useful function definitions (see .c files for descriptions) */
extern HashTable<unsigned, unsigned> *SharedPageTable; // (mem_map1,mem_map2);

extern int DEBUG_TIME;		/* time to enable debugging on */

extern void init_decode(state *);
extern int reset_lists(state *);
extern int ExceptionHandler(int, state *);
extern int PreExceptionHandler(instance *, state *);

extern void ComputeAvail(state *);

#define unstall_the_rest(proc) {if (proc->stall_the_rest) {proc->eff_losses[proc->type_of_stall_rest] += proc->stalledeff; proc->stall_the_rest=0; proc->type_of_stall_rest=eNOEFF_LOSS; proc->stalledeff = 0;}}

inline unsigned DOWN_TO_PAGE(unsigned i) { return i-i%ALLOC_SIZE;}
inline unsigned UP_TO_PAGE(unsigned i) {unsigned j=i+ALLOC_SIZE-1; return j-j%ALLOC_SIZE;}

extern "C" void RSIM_EVENT();
extern int startup(char**,state *); // load executable, set up stack, data, some registers, etc.

#endif
