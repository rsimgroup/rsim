/*****************************************************************************

  instance.h

  Contains definition of instance class and related definitions

  ****************************************************************************/
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


#ifndef _instance_h_
#define _instance_h_ 1

#include "units.h"
#include "instruction.h"
#include "MemSys/miss_type.h"

enum except {		/* exception modes supported */
  OK,			/* no exception */
  DIV0,			/* Division by zero */
  FPERR,		/* Floating point exception */
  SEGV,			/* Segmentation fault */
  BUSERR,		/* bus error, misaligned access */
  SYSTRAP,		/* used for emulating operating system traps */
  WINTRAP,		/* register window overflow/underflow */
  SOFT_LIMBO,		/* memory disambiguation violation exception */
  SOFT_SL_COHE,		/* speculative loads -- coherence violation */
  SOFT_SL_REPL,		/* speculative loads -- replacement exception  */
  SERIALIZE,		/* instruction that requires serialization */
  PRIVILEGED,		/* privileged instruction, requires supervisor mode  */
  ILLEGAL,		/* illegal instruction   */
  BADPC};		/* bad PC address */

class state;
class instr;

extern int newinst, new2ndinst, killinst;

#define BUSY_SETRS1 1
#define BUSY_SETRS2 2
#define BUSY_SETRSCC 4
#define BUSY_SETRSD 8 /* This is used when the output register is an FPHALF,
			 so writing the destination is effectively an RMW... */
#define BUSY_SETRS1P 16

#define BUSY_CLEARRS1 (~1)
#define BUSY_CLEARRS2 (~2)
#define BUSY_CLEARRSCC (~4)
#define BUSY_CLEARRSD (~8)
#define BUSY_CLEARRS1P (~16)

#define BUSY_ALLCLEAR 0
#define BUSY_ALLSET (BUSY_SETRS1 | BUSY_SETRS2 | BUSY_SETRSCC | BUSY_SETRSD | BUSY_SETRS1P)

struct instance                          /* this is a _dynamic_ instruction */
{
  int pc;  /* NOTE: we store the PC and NPC not in imitation of an
	      actual system, but rather to "simulate" system behavior
	      in a simple way */
  int npc; 				/* next pc value */
  instr *code;                         /* the static instruction referenced */
    
  int depctr;                          /* number of dependences */
  int busybits;				/* indicate "busy'ness" of rs1, rs2 and rd */
  int stallqs;                         /* counts # of mini-stallqs */
    
  unsigned addr;                         /* the address of the memory instruction */
  int addr_ready;                        /* has address generation happened yet? */
  
  double time_active_list;               /* time from active list */
  double time_addr_ready;                /* time from address ready */
  double time_issued;                    /* time from issue */
  
  unsigned finish_addr;                  /* the "end address" of the memory instruction */
    
  /* More dependency information */
  int truedep ;				/* true dependence */
  int addrdep;				/* address dependence */
  int strucdep;				/* structural dependence */
  int branchdep;			/* control dependence */
    
  int win_num;                         /* Window number */
    
  /* These define the logical register numbers
     after taking into effect the window pointer,
     etc. */
  int lrs1, lrs2, lrscc;
  int lrd, lrcc; 
    
  int prd, prcc;                    /* physical registers to complete */
  int prs1, prs2, prscc;   /* physical registers that we operate on/with */
  int lrsd,prsd; /* this is used for instructions where dest. reg is
		    an FPHALF.  Since FPs are mapped/renamed by doubles,
		    these are effectively RMWs */
  int prs1p, prdp; /* for INT_PAIR ops */

  /* unions for the various register values */
  union{				/* destination */
    int rdvali;
    double rdvalf;
    float rdvalfh;
    struct {int a, b;} rdvalipair;
    long long rdvalll;
  };
    
  union{				/* register source 1 */
    int rs1vali;
    double rs1valf;
    float rs1valfh;
    struct {int a, b;} rs1valipair;
    long long rs1valll;
  };
    
  union{				/* register source 2 */
    int rs2vali;
    double rs2valf;
    float rs2valfh;
    /* rs2 can't have an int reg pair with this ISA */
    long long rs2valll;
  };

  int rsccvali;                    /* value of source condition code */
    
  int rccvali;                     /* Only integer values */
  double rsdvalf; /* the value of prsd */
    
  int completion;                  /* time for completion */
  except exception_code;           /* 0 for no exception? */
  
  int branch_pred;                 /* pc predicted, filled in at decode */
  int newpc;                       /* 0 for default, not filled in until instr
				      complete  */ 
  int mispredicted;                /* Result of prediction, filled in at
				      completion  */
  int annulled;                    /* whether we annulled it or not */
  int taken;                       /* the return value of StartCtlXfer */

  int memprogress; /* lists current state of memory instruction */
  
  int issuetime;                   /* cycle # when it was issued; should be
				      initialized to MAX_INT */
  int addrissuetime;               /* cycle # when sent to address generation
				      unit; used for static scheduling only */

				     
  int tag;                         /* Unique tag identifier */
  UTYPE unit_type;                 /* Unit type associated with instruction */

  int newst;                    /* store that hasn't been marked ready yet */
  int st_ready;			/* store marked ready to issue? */
  int inuse;			/* indicates use of instance */
  int global_perform:1;		/* has mem operation been globally performed? */
  int limbo;			/* flag "limbo" ambiguous memory ops */
  int kill;			/* kill instruction flag */
  unsigned prefetched:1;	/* flag a prefetch instruction */
  unsigned in_memunit:1;	/* flag a memory operation in memory unit */
  unsigned partial_overlap:1;	/* indicate presense of partial overlaps
				   between memory operations */
  int vsbfwd;			/* forwards from virtual store buffer */
  MISS_TYPE miss;		/* mark type of cache miss */
  unsigned latepf:1;		/* flag a late prefetch */

  public:
    
  /*Constructor to initialize  instance */
  decode_instruction(instr *instrn,state *proc);
  instance(instr *instrn, state *proc)
  {decode_instruction(instrn,proc);}
  instance(instance *inst) {*this=*inst;new2ndinst++;}
  ~instance() {tag=-1;killinst++;} /* reassign tag */
};

#endif
