/*

  traptable.cc

  Implements the traps that are actually simulated (rather than merely
  emulated)

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


#include "Processor/traps.h"
#include "Processor/instance.h"
#include "Processor/state.h"
#include "Processor/mainsim.h" // for num_instrs
#include "Processor/simio.h"
#include "Processor/processor_dbg.h"
#include <stdlib.h>



int TRAPTABLE_SIZE;

void TrapTableInit()
{
  int cnt=0;

  /* First, the save overflow trap */
  /* order is instruction, rd, rcc, rs1,rs2, rscc,aux1,aux2,imm,
              rd_regtype,rs1_regtype,rs2_regtype,
              taken,annul,cond_branch,uncond_branch,wpchange */

#define TRAPTABLE_SAVE 0
  trapstable[cnt++] = instr(iSAVE,0,0,0,0,0,0,0,0,REG_INT,REG_INT,REG_INT,0,0,0,0,WPC_SAVE);
  trapstable[cnt++] = instr(iSAVE,0,0,0,0,0,0,0,0,REG_INT,REG_INT,REG_INT,0,0,0,0,WPC_SAVE);
  trapstable[cnt++] = instr(iSTW,0,0,16,14,0,1,0, 0 ,REG_INT,REG_INT,REG_INT,0,0,0,0,WPC_NONE);
  trapstable[cnt++] = instr(iSTW,0,0,17,14,0,1,0, 4 ,REG_INT,REG_INT,REG_INT,0,0,0,0,WPC_NONE);
  trapstable[cnt++] = instr(iSTW,0,0,18,14,0,1,0, 8 ,REG_INT,REG_INT,REG_INT,0,0,0,0,WPC_NONE);
  trapstable[cnt++] = instr(iSTW,0,0,19,14,0,1,0,12 ,REG_INT,REG_INT,REG_INT,0,0,0,0,WPC_NONE);
  trapstable[cnt++] = instr(iSTW,0,0,20,14,0,1,0,16 ,REG_INT,REG_INT,REG_INT,0,0,0,0,WPC_NONE);
  trapstable[cnt++] = instr(iSTW,0,0,21,14,0,1,0,20 ,REG_INT,REG_INT,REG_INT,0,0,0,0,WPC_NONE);
  trapstable[cnt++] = instr(iSTW,0,0,22,14,0,1,0,24 ,REG_INT,REG_INT,REG_INT,0,0,0,0,WPC_NONE);
  trapstable[cnt++] = instr(iSTW,0,0,23,14,0,1,0,28 ,REG_INT,REG_INT,REG_INT,0,0,0,0,WPC_NONE);
  trapstable[cnt++] = instr(iSTW,0,0,24,14,0,1,0,32 ,REG_INT,REG_INT,REG_INT,0,0,0,0,WPC_NONE);
  trapstable[cnt++] = instr(iSTW,0,0,25,14,0,1,0,36 ,REG_INT,REG_INT,REG_INT,0,0,0,0,WPC_NONE);
  trapstable[cnt++] = instr(iSTW,0,0,26,14,0,1,0,40 ,REG_INT,REG_INT,REG_INT,0,0,0,0,WPC_NONE);
  trapstable[cnt++] = instr(iSTW,0,0,27,14,0,1,0,44 ,REG_INT,REG_INT,REG_INT,0,0,0,0,WPC_NONE);
  trapstable[cnt++] = instr(iSTW,0,0,28,14,0,1,0,48 ,REG_INT,REG_INT,REG_INT,0,0,0,0,WPC_NONE);
  trapstable[cnt++] = instr(iSTW,0,0,29,14,0,1,0,52 ,REG_INT,REG_INT,REG_INT,0,0,0,0,WPC_NONE);
  trapstable[cnt++] = instr(iSTW,0,0,30,14,0,1,0,56 ,REG_INT,REG_INT,REG_INT,0,0,0,0,WPC_NONE);
  trapstable[cnt++] = instr(iSTW,0,0,31,14,0,1,0,60 ,REG_INT,REG_INT,REG_INT,0,0,0,0,WPC_NONE);
  trapstable[cnt++] = instr(iRESTORE,0,0,0,0,0,0,0,0,REG_INT,REG_INT,REG_INT,0,0,0,0,WPC_RESTORE);
  trapstable[cnt++] = instr(iRESTORE,0,0,0,0,0,0,0,0,REG_INT,REG_INT,REG_INT,0,0,0,0,WPC_RESTORE);
  trapstable[cnt++] = instr(iSAVRESTD,0,0,0,0,0,0,0,0,REG_INT,REG_INT,REG_INT,0,0,0,0,WPC_NONE);
  trapstable[cnt++] = instr(iDONERETRY,0,0,0,0,0,1,0,0,REG_INT,REG_INT,REG_INT,0,0,0,0,WPC_NONE);
#define TRAPTABLE_RESTORE 22
  trapstable[cnt++] = instr(iRESTORE,0,0,0,0,0,0,0,0,REG_INT,REG_INT,REG_INT,0,0,0,0,WPC_RESTORE);
  trapstable[cnt++] = instr(iLDUW,16,0,0,14,0,1,0, 0 ,REG_INT,REG_INT,REG_INT,0,0,0,0,WPC_NONE);
  trapstable[cnt++] = instr(iLDUW,17,0,0,14,0,1,0, 4 ,REG_INT,REG_INT,REG_INT,0,0,0,0,WPC_NONE);
  trapstable[cnt++] = instr(iLDUW,18,0,0,14,0,1,0, 8 ,REG_INT,REG_INT,REG_INT,0,0,0,0,WPC_NONE);
  trapstable[cnt++] = instr(iLDUW,19,0,0,14,0,1,0,12 ,REG_INT,REG_INT,REG_INT,0,0,0,0,WPC_NONE);
  trapstable[cnt++] = instr(iLDUW,20,0,0,14,0,1,0,16 ,REG_INT,REG_INT,REG_INT,0,0,0,0,WPC_NONE);
  trapstable[cnt++] = instr(iLDUW,21,0,0,14,0,1,0,20 ,REG_INT,REG_INT,REG_INT,0,0,0,0,WPC_NONE);
  trapstable[cnt++] = instr(iLDUW,22,0,0,14,0,1,0,24 ,REG_INT,REG_INT,REG_INT,0,0,0,0,WPC_NONE);
  trapstable[cnt++] = instr(iLDUW,23,0,0,14,0,1,0,28 ,REG_INT,REG_INT,REG_INT,0,0,0,0,WPC_NONE);
  trapstable[cnt++] = instr(iLDUW,24,0,0,14,0,1,0,32 ,REG_INT,REG_INT,REG_INT,0,0,0,0,WPC_NONE);
  trapstable[cnt++] = instr(iLDUW,25,0,0,14,0,1,0,36 ,REG_INT,REG_INT,REG_INT,0,0,0,0,WPC_NONE);
  trapstable[cnt++] = instr(iLDUW,26,0,0,14,0,1,0,40 ,REG_INT,REG_INT,REG_INT,0,0,0,0,WPC_NONE);
  trapstable[cnt++] = instr(iLDUW,27,0,0,14,0,1,0,44 ,REG_INT,REG_INT,REG_INT,0,0,0,0,WPC_NONE);
  trapstable[cnt++] = instr(iLDUW,28,0,0,14,0,1,0,48 ,REG_INT,REG_INT,REG_INT,0,0,0,0,WPC_NONE);
  trapstable[cnt++] = instr(iLDUW,29,0,0,14,0,1,0,52 ,REG_INT,REG_INT,REG_INT,0,0,0,0,WPC_NONE);
  trapstable[cnt++] = instr(iLDUW,30,0,0,14,0,1,0,56 ,REG_INT,REG_INT,REG_INT,0,0,0,0,WPC_NONE);
  trapstable[cnt++] = instr(iLDUW,31,0,0,14,0,1,0,60 ,REG_INT,REG_INT,REG_INT,0,0,0,0,WPC_NONE);
  trapstable[cnt++] = instr(iSAVE,0,0,0,0,0,0,0,0,REG_INT,REG_INT,REG_INT,0,0,0,0,WPC_SAVE);
  trapstable[cnt++] = instr(iSAVRESTD,0,0,0,0,0,1,0,0,REG_INT,REG_INT,REG_INT,0,0,0,0,WPC_NONE);
  trapstable[cnt++] = instr(iDONERETRY,0,0,0,0,0,1,0,0,REG_INT,REG_INT,REG_INT,0,0,0,0,WPC_NONE);
#define TRAPTABLE_STFSR 42 /* assume in this case that the FSR value is stored into system %l1 and address in %l2 before start */
  trapstable[cnt++] = instr(iSAVE,0,0,0,0,0,0,0,0,REG_INT,REG_INT,REG_INT,0,0,0,0,WPC_SAVE);
  trapstable[cnt++] = instr(iSTW,0,0,17,18,0,1,0, 0 ,REG_INT,REG_INT,REG_INT,0,0,0,0,WPC_NONE);
  trapstable[cnt++] = instr(iRESTORE,0,0,0,0,0,0,0,0,REG_INT,REG_INT,REG_INT,0,0,0,0,WPC_RESTORE);
  trapstable[cnt++] = instr(iDONERETRY,0,0,0,0,0,0,0,0,REG_INT,REG_INT,REG_INT,0,0,0,0,WPC_NONE);
#define TRAPTABLE_STXFSR 46 /* assume in this case that the FSR value is stored into system %l0 & %l1 and address in %l2 before start */
  trapstable[cnt++] = instr(iSAVE,0,0,0,0,0,0,0,0,REG_INT,REG_INT,REG_INT,0,0,0,0,WPC_SAVE);
  trapstable[cnt++] = instr(iSTW,0,0,16,18,0,1,0, 0 ,REG_INT,REG_INT,REG_INT,0,0,0,0,WPC_NONE);
  trapstable[cnt++] = instr(iSTW,0,0,17,18,0,1,0, 4 ,REG_INT,REG_INT,REG_INT,0,0,0,0,WPC_NONE);
  trapstable[cnt++] = instr(iRESTORE,0,0,0,0,0,0,0,0,REG_INT,REG_INT,REG_INT,0,0,0,0,WPC_RESTORE);
  trapstable[cnt++] = instr(iDONERETRY,0,0,0,0,0,0,0,0,REG_INT,REG_INT,REG_INT,0,0,0,0,WPC_NONE);
#define TRAPTABLE_SZ 51
  
  TRAPTABLE_SIZE = cnt; /* should also be same as TRAPTABLE_SZ */
  
}

struct instr trapstable[TRAPTABLE_SZ];


/****************************************************************************/
/* TrapTableHandle: Enter into privileged state and save aside the old PC   */
/*                  and NPC. Then jump into the appropriate trap handler.   */
/****************************************************************************/

void TrapTableHandle(instance *inst, state *proc, TrapTableCode ttc)
{
  if (proc->privstate)
    {
      fprintf(simerr,"Trapped while in privileged state! Must be a serious unrecoverable error!\n");
      exit(-1);
    }
  proc->privstate = 1;
  proc->trappc = inst->pc;
  proc->trapnpc = inst->npc;
  switch (ttc)
    {
    case TRAP_OVERFLOW:
#ifdef COREFILE
      if (YS__Simtime > DEBUG_TIME)
	fprintf(corefile,"Entering trap table: overflow handler\n");
#endif
      proc->window_overflows++;
      proc->pc = TRAPTABLE_BASE + TRAPTABLE_SAVE;
      proc->npc = proc->pc+1;
      break;
    case TRAP_UNDERFLOW:
#ifdef COREFILE
      if (YS__Simtime > DEBUG_TIME)
	fprintf(corefile,"Entering trap table: underflow handler\n");
#endif
      proc->window_underflows++;
      proc->pc = TRAPTABLE_BASE + TRAPTABLE_RESTORE;
      proc->npc = proc->pc+1;
      break;
    case TRAP_STFSR:
      if (inst->code->instruction == iSTFSR)
	{
#ifdef COREFILE
	  if (YS__Simtime > DEBUG_TIME)
	    fprintf(corefile,"Entering trap table: STFSR handler\n");
#endif
	  proc->pc = TRAPTABLE_BASE + TRAPTABLE_STFSR;
	  proc->npc = proc->pc+1;
	}
      else // STXFSR
	{
#ifdef COREFILE
	  if (YS__Simtime > DEBUG_TIME)
	    fprintf(corefile,"Entering trap table: STXFSR handler\n");
#endif
	  proc->pc = TRAPTABLE_BASE + TRAPTABLE_STXFSR;
	  proc->npc = proc->pc+1;
	}
      break;
    default:
      fprintf(simerr,"Unrecognized trap table code\n");
      exit(-1);
      break;
    }
}


