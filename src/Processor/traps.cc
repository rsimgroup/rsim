/*

  Processor/traps.cc

  This file emulates the operation of certain operating system traps,
  as well as some special traps provided by RSIM

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
#include "Processor/alloc.h"
#include "Processor/exec.h"
#include "Processor/memprocess.h"
#include "Processor/processor_dbg.h"
#include "Processor/simio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <unistd.h>
#include <fcntl.h>

extern "C"
{
#include "MemSys/module.h"
#include "MemSys/simsys.h"
#include "MemSys/associate.h"
#include "MemSys/cohe_types.h"
}

static void Sym_Bzero(instance *,state *);
static void TimeHandler(instance *,state *);
static void TimesHandler(instance *,state *);
static void ReadHandler(instance *,state *);
static void WriteHandler(instance *,state *);
static void SeekHandler(instance *,state *);
static void CloseHandler(instance *,state *);
static void DupHandler(instance *,state *);
static void Dup2Handler(instance *,state *);
static void OpenHandler(instance *,state *);
static void AssociateHandler(instance *,state *);

/**************************************************************************/
/* SysTrapHandle : Handle special operating system and RSIM traps         */
/**************************************************************************/

void SysTrapHandle(instance *inst,state *proc)
{
  int aux2;

  aux2=inst->code->aux2;
  
#ifdef COREFILE
  if (YS__Simtime > DEBUG_TIME)
    fprintf(corefile,"traps.cc: In SysTrapHandler with code %d\n",inst->code->aux2);
#endif

  switch (aux2)
    {
    case 0: // exit
      fprintf(simerr,"Processor %d exiting with code %d\n",proc->proc_id,inst->rs1vali);
      proc->exit = (inst->rs1vali) ? (inst->rs1vali - 128) : 1;
      break;
    case 5: // SH_MALLOC
      proc->physical_int_reg_file[inst->lrd] =
      proc->logical_int_reg_file[inst->lrd] =
	(int) our_sh_malloc(inst->rs1vali);
#ifdef COREFILE
      if(proc->curr_cycle > DEBUG_TIME)
	fprintf(corefile,"Sh_malloc: Returned address of %d, size of %d\n",proc->physical_int_reg_file[inst->lrd],inst->rs1vali);
#endif
      break;
    case 11: // get cycle number
      proc->physical_int_reg_file[inst->lrd] =
      proc->logical_int_reg_file[inst->lrd] = proc->curr_cycle;
      break;
    case 13: // FORK!
      {
	proc->physical_int_reg_file[inst->lrd] = proc->logical_int_reg_file[inst->lrd] = 0;
	state *np = proc->fork();
	proc->physical_int_reg_file[inst->lrd] = proc->logical_int_reg_file[inst->lrd] = np->proc_id;
      }
      break;
    case 15: // getpid
      {
	proc->physical_int_reg_file[inst->lrd] = proc->logical_int_reg_file[inst->lrd] = proc->proc_id;
      }
      break;
    case 32:
      Sym_Bzero(inst,proc);
      break;
    case 33:      /* MEMSYS Off */
      proc->MEMSYS=0;
      break;
    case 34:      /* MEMSYS On */
      proc->MEMSYS=1;
      break;
    case 35: /* newphase */
      if (proc->agg_lat_type != -1)
	{
	  StatrecUpdate(proc->lat_contrs[proc->agg_lat_type],
			proc->curr_cycle-proc->last_counted,1.0);
	  proc->last_graduated=proc->last_counted=proc->curr_cycle;
	}
      
      proc->newphase(inst->rs1vali);
      break;
    case 36: /* endphase */
      if (proc->agg_lat_type != -1)
	{
	  StatrecUpdate(proc->lat_contrs[proc->agg_lat_type],
			proc->curr_cycle-proc->last_counted,1.0);
	  proc->last_graduated=proc->last_counted=proc->curr_cycle;
	}
      proc->endphase();
      break;
    case 47:
    case 48:
      AssociateHandler(inst,proc);
      break;
    case 49: // fullstop -- force everyone to exit!
      {
	fprintf(simerr,"Processor %d forcing grinding halt with code %d\n",proc->proc_id,inst->rs1vali);
	exit(1); // abort all processors immediately, without graceful exit
      }
      break;
    case 50: // RSIM OFF
      proc->decode_rate=1;
      proc->graduate_rate=1;
      proc->max_active_list_size=2;
      break;
    case 51: // RSIM ON
      proc->decode_rate=NO_OF_DECODES_PER_CYCLE;
      proc->graduate_rate=NO_OF_GRADUATES_PER_CYCLE;
      proc->max_active_list_size=MAX_ACTIVE_NUMBER;
      break;
    case 52: // get L2 cache line size
      {
	extern int lvl2_linesz;
	proc->physical_int_reg_file[inst->lrd] = proc->logical_int_reg_file[inst->lrd] = lvl2_linesz;
      }      
      break;
    case 80: // clear stats
      StatClearAll();
      break;
    case 81: // report stats
      StatReportAll();
      break;
    case 100: // time system call.
      // this particular function returns the time since the 
      // startup of the program, that is, when YS__Simtime was 0
      TimeHandler(inst,proc);
      break;

    case 101: // times system call.
      TimesHandler(inst,proc);
      break;
    case 102: // sbrk system call.
      // sbrk is of the form void *sbrk(int incr)
      {
	unsigned oldbrk = proc->highheap;
	if (inst->rs1vali > 0) /* need to increase */
	  {
	    unsigned incr = UP_TO_PAGE(inst->rs1vali);
	    unsigned chunkptr = (unsigned) malloc(incr);
#ifdef COREFILE
	    memset((void *)chunkptr,0x5a,incr);
#endif
	    for (unsigned pg = proc->highheap/ALLOC_SIZE;
		 pg < (proc->highheap + incr)/ALLOC_SIZE;
		 pg++, chunkptr += ALLOC_SIZE)
	      {
		proc->PageTable.insert(pg,chunkptr);
	      }
	    proc->highheap += incr;
	  }
	proc->physical_int_reg_file[inst->lrd] =
	  proc->logical_int_reg_file[inst->lrd] =
	  oldbrk;
      }
      break;
      
    case 103: // read system call.
      ReadHandler(inst,proc);
      break;
      
    case 104: // write system call
      WriteHandler(inst,proc);
      break;
    case 105: // seek system call
      SeekHandler(inst,proc);
      break;
    case 106: // close system call
      CloseHandler(inst,proc);
      break;
    case 107: //open system call
      OpenHandler(inst,proc);
      break;
    case 108: // dup system call
      DupHandler(inst,proc);
      break;
    case 109: // dup system call
      Dup2Handler(inst,proc);
      break;
    default:
      fprintf(simerr, "UNKNOWN OPTION IN EXCEPTION CALL\n");
      exit(-1);
      break;
    }
}

void StackTrapHandle(unsigned addr, state *proc)
{
  while (proc->lowstack > addr)
    {
      unsigned chunk = (unsigned) malloc(ALLOC_SIZE);
      if (chunk == 0)
	{
	  fprintf(simerr,"RSIM: Malloc out of space when growing stack!\n");
	  exit(-1);
	}
      else
	{
	  proc->lowstack -= ALLOC_SIZE;
	  proc->PageTable.insert(proc->lowstack / ALLOC_SIZE,chunk);
#ifdef COREFILE
	  if (YS__Simtime > DEBUG_TIME)
	    fprintf(corefile,"Added page to stack\n");
#endif
	}
    }
}

/*************************************************************************/
/* Sym_Bzero : Simulator exception routine that handles bzero            */
/*************************************************************************/

static void Sym_Bzero(instance *inst,state *proc)
{
#ifdef COREFILE
  if (YS__Simtime > DEBUG_TIME)
    fprintf(corefile,"traps.cc: In Sym_Bzero Handler\n");
#endif
  
  int lr,pr,sz;
  /*read the parameters */

  lr = convert_to_logical(proc->cwp,8);
  pr = proc->intmapper[lr];
  inst->addr = proc->physical_int_reg_file[pr];

  lr = convert_to_logical(proc->cwp,9);
  pr = proc->intmapper[lr];
  sz = proc->physical_int_reg_file[pr];
#ifdef COREFILE
  if(proc->curr_cycle > DEBUG_TIME)
    fprintf(corefile,"SYM Bzero, size of %d\n",sz);
#endif
  
  inst->addr=inst->rs1vali;
  int st=inst->addr & (ALLOC_SIZE-1);
  char *pa = (char *)GetMap(inst,proc);
  if (sz > ALLOC_SIZE-st)
    {
#ifdef COREFILE
      if(proc->curr_cycle > DEBUG_TIME)
	fprintf(corefile,"zeroing, size of %d at pa %p\n",ALLOC_SIZE-st,pa);
#endif
      memset(pa,0,ALLOC_SIZE-st);
      inst->addr += ALLOC_SIZE -st;
      sz -= ALLOC_SIZE-st;
      pa=(char *)GetMap(inst,proc);
      while (sz >=ALLOC_SIZE)
	{
#ifdef COREFILE
	  if(proc->curr_cycle > DEBUG_TIME)
	    fprintf(corefile,"zeroing, size of %d at pa %p\n",ALLOC_SIZE,pa);
#endif
	  memset(pa,0,ALLOC_SIZE);
	  inst->addr += ALLOC_SIZE;
	  sz -= ALLOC_SIZE;
	  pa=(char *)GetMap(inst,proc);
	}
    }
#ifdef COREFILE
  if(proc->curr_cycle > DEBUG_TIME)
    fprintf(corefile,"zeroing, size of %d at pa %p\n",sz,pa);
#endif
  memset(pa,0,sz);
  return;
}

/*************************************************************************/
/* TimeHandler : Simulator exception routine that handles time           */
/*************************************************************************/


static void TimeHandler(instance *inst,state *proc)
{
#ifdef COREFILE
  if (YS__Simtime > DEBUG_TIME)
    fprintf(corefile,"traps.cc: In Time Handler\n");
#endif

  int lr,pr;
  int *pa;

  lr = convert_to_logical(proc->cwp,8);
  pr = proc->intmapper[lr];
  inst->addr = proc->physical_int_reg_file[pr];

  if ((void *)inst->addr != NULL)
    {
      pa = (int *)GetMap(inst,proc);
      *pa = int(YS__Simtime/(300*1000000)); // time for user program in clock seconds
    }
  
  proc->physical_int_reg_file[pr] = int(YS__Simtime/(300*1000000));
}


/*************************************************************************/
/* TimesHandler : Simulator exception routine that handles times         */
/*************************************************************************/

static void TimesHandler(instance *inst,state *proc)
{
#ifdef COREFILE
  if (YS__Simtime > DEBUG_TIME)
    fprintf(corefile,"traps.cc: In Times Handler\n");
#endif

  int lr,pr;
  int *pa;

  lr = convert_to_logical(proc->cwp,8);
  pr = proc->intmapper[lr];
  inst->addr = proc->physical_int_reg_file[pr];

  pa = (int *)GetMap(inst,proc);
  *pa = int(YS__Simtime/(3*1000000)); // time for user program in clock ticks
    // a clock tick defined in limits.h is 1/100 of a sec
    for(int i=0;i<3;i++)
      { 
	inst->addr+=4; //go to next field
	pa = (int *)GetMap(inst,proc);
	*pa = 0;
      }
  
  proc->physical_int_reg_file[pr] = int(YS__Simtime/(3*1000000));
}

/*************************************************************************/
/* ReadHandler : Simulator exception routine that handles read           */
/*************************************************************************/

static void ReadHandler(instance *inst,state *proc)
{
#ifdef COREFILE
  if (YS__Simtime > DEBUG_TIME)
    fprintf(corefile,"traps.cc: In Read Handler\n");
#endif
  
  int fd,number_of_items;
  int buffer;
  int lr,pr;
  int count=0;
  int return_register;
  /*read the parameters */

  lr = convert_to_logical(proc->cwp,8);
  return_register = pr = proc->intmapper[lr];
  fd = proc->physical_int_reg_file[pr];
  lr = convert_to_logical(proc->cwp,9);
  pr = proc->intmapper[lr];
  inst->addr = proc->physical_int_reg_file[pr];
  lr = convert_to_logical(proc->cwp,10);
  pr = proc->intmapper[lr];
  number_of_items = proc->physical_int_reg_file[pr];

  /* do one character at a time - can be optimized */

  int st = inst->addr & (ALLOC_SIZE-1);

  if(number_of_items > ALLOC_SIZE-st) {
    buffer = GetMap(inst,proc);
    if(read(fd,(char*)buffer,ALLOC_SIZE-st)!=-1)
      count+=ALLOC_SIZE-st;
    inst->addr+=ALLOC_SIZE-st;
    number_of_items-=ALLOC_SIZE-st;

    while(number_of_items>=ALLOC_SIZE) {
      buffer = GetMap(inst,proc);
      if(read(fd,(char*)buffer,ALLOC_SIZE)!=-1)
	count+=ALLOC_SIZE;
      else 
	break;
      number_of_items-=ALLOC_SIZE;
      inst->addr+=ALLOC_SIZE;
    }
  }
      
    buffer = GetMap(inst,proc);

    if(read(fd,(char*)buffer,number_of_items)!=-1)
      count+=number_of_items;

  proc->physical_int_reg_file[return_register] = count;
}

/*************************************************************************/
/* WriteHandler : Simulator exception routine that handles write         */
/*************************************************************************/

static void WriteHandler(instance *inst,state *proc)
{
#ifdef COREFILE
  if (YS__Simtime > DEBUG_TIME)
    fprintf(corefile,"traps.cc: In Write Handler\n");
#endif
  
  int fd,number_of_items;
  int buffer;
  int lr,pr;
  int count = 0; /* return value */
  int return_register;
  /*read the parameters */

  lr = convert_to_logical(proc->cwp,8);
  return_register = pr = proc->intmapper[lr];
  fd = proc->physical_int_reg_file[pr];
  lr = convert_to_logical(proc->cwp,9);
  pr = proc->intmapper[lr];
  inst->addr = proc->physical_int_reg_file[pr];
  lr = convert_to_logical(proc->cwp,10);
  pr = proc->intmapper[lr];
  number_of_items = proc->physical_int_reg_file[pr];


  int st = inst->addr & (ALLOC_SIZE-1);

  if(number_of_items > ALLOC_SIZE-st) {
    buffer = GetMap(inst,proc);
    if(write(fd,(char*)buffer,ALLOC_SIZE-st)!=-1)
      count+=ALLOC_SIZE-st;
    inst->addr+=ALLOC_SIZE-st;
    number_of_items-=ALLOC_SIZE-st;

    while(number_of_items>=ALLOC_SIZE) {
      buffer = GetMap(inst,proc);
      if(write(fd,(char*)buffer,ALLOC_SIZE)!=-1)
	count+=ALLOC_SIZE;
      else 
	break;
      number_of_items-=ALLOC_SIZE;
      inst->addr+=ALLOC_SIZE;
    }
  }
      
    buffer = GetMap(inst,proc);

    if(write(fd,(char*)buffer,number_of_items)!=-1)
      count+=number_of_items;

  proc->physical_int_reg_file[return_register] = count;
}


/*************************************************************************/
/* SeekHandler : Simulator exception routine that handles lseek          */
/*************************************************************************/

static void SeekHandler(instance *,state *proc)
{
#ifdef COREFILE
  if (YS__Simtime > DEBUG_TIME)
    fprintf(corefile,"traps.cc: In Seek Handler\n");
#endif
  
  int fd,position,whence;
  int return_register;
  int return_value;
  int lr,pr;
  /* read parameters */
  lr = convert_to_logical(proc->cwp,8);
  return_register = pr = proc->intmapper[lr];
  fd = proc->physical_int_reg_file[pr];
  lr = convert_to_logical(proc->cwp,9);
  pr = proc->intmapper[lr];
  position = proc->physical_int_reg_file[pr];
  lr = convert_to_logical(proc->cwp,10);
  pr = proc->intmapper[lr];
  whence = proc->physical_int_reg_file[pr];
  
  return_value = lseek(fd,position,whence);
  proc->physical_int_reg_file[return_register] = return_value;
}


/*************************************************************************/
/* CloseHandler : Simulator exception routine that handles close         */
/*************************************************************************/

static void CloseHandler(instance *,state *proc)
{
#ifdef COREFILE
  if (YS__Simtime > DEBUG_TIME)
    fprintf(corefile,"traps.cc: In Close Handler\n");
#endif
  
  int fd;
  int lr,pr;
  int return_value;

  lr = convert_to_logical(proc->cwp,8);
  pr = proc->intmapper[lr];
  fd = proc->physical_int_reg_file[pr];
  return_value = close(fd);

  proc->physical_int_reg_file[pr] = return_value;

}


/*************************************************************************/
/* DupHandler : Simulator exception routine that handles duplication of  */
/*              file handles                                             */
/*************************************************************************/

static void DupHandler(instance *,state *proc)
{
#ifdef COREFILE
  if (YS__Simtime > DEBUG_TIME)
    fprintf(corefile,"traps.cc: In Dup Handler\n");
#endif
  
  int fd;
  int lr,pr;
  int return_value;

  lr = convert_to_logical(proc->cwp,8);
  pr = proc->intmapper[lr];
  fd = proc->physical_int_reg_file[pr];
  return_value = dup(fd);

  proc->physical_int_reg_file[pr] = return_value;
}


/*************************************************************************/
/* Dup2Handler : Simulator exception routine that handles dup2 call      */
/*************************************************************************/

static void Dup2Handler(instance *,state *proc)
{
#ifdef COREFILE
  if (YS__Simtime > DEBUG_TIME)
    fprintf(corefile,"traps.cc: In Dup2 Handler\n");
#endif

  int fd,fd2;
  int return_register;
  int return_value;
  int lr,pr;
  /* read parameters */
  lr = convert_to_logical(proc->cwp,8);
  return_register = pr = proc->intmapper[lr];
  fd = proc->physical_int_reg_file[pr];
  lr = convert_to_logical(proc->cwp,9);
  pr = proc->intmapper[lr];
  fd2 = proc->physical_int_reg_file[pr];

  return_value = dup2(fd,fd2);
  proc->physical_int_reg_file[return_register] = return_value;
}


#define OH_PATHMAX 1000 // path of the file is assumed to be MAX 1000 characters for now...
static void OpenHandler(instance *inst,state *proc)
{
#ifdef COREFILE
  if (YS__Simtime > DEBUG_TIME)
    fprintf(corefile,"traps.cc: In Open Handler\n");
#endif

  int lr,pr;
  char buffer[OH_PATHMAX]; 
  int oflag;
  int mode;
  int return_value;
  int return_register;
  char *pa;

  lr = convert_to_logical(proc->cwp,8);
  return_register = pr = proc->intmapper[lr];
  inst->addr = proc->physical_int_reg_file[pr];
  for(int i=0;i<OH_PATHMAX-1;i++)
    {
      pa = (char *)GetMap(inst,proc);
      buffer[i] = *pa;
      if(*pa == '\0')
	break;
      inst->addr++;
    }
  buffer[OH_PATHMAX-1] = '\0';

  lr = convert_to_logical(proc->cwp,9);
  pr = proc->intmapper[lr];
  oflag = proc->physical_int_reg_file[pr];
  lr = convert_to_logical(proc->cwp,10);
  pr = proc->intmapper[lr];
  mode = proc->physical_int_reg_file[pr];
 
  return_value = open(buffer,oflag,mode);
  proc->physical_int_reg_file[return_register] = return_value;
    
}

#define ASSOCNAME_MAX 1000
static void AssociateHandler(instance *inst, state *proc)
{
  /* void AssociateAddrNode(unsigned start,unsigned end, int node, char *) */
  /* void AssociateAddrCohe(unsigned start,unsigned end, int node, char *) */
#ifdef COREFILE
  if (YS__Simtime > DEBUG_TIME)
    fprintf(corefile,"traps.cc: In AssociateNode Handler\n");
#endif

  unsigned start, end;
  int node, cohe;
  char AssocName[ASSOCNAME_MAX], *pa;
  int lr,pr;
  
  /*read the parameters */
  
  lr = convert_to_logical(proc->cwp,8);
  pr = proc->intmapper[lr];
  start = proc->physical_int_reg_file[pr];
  lr = convert_to_logical(proc->cwp,9);
  pr = proc->intmapper[lr];
  end = proc->physical_int_reg_file[pr];
  lr = convert_to_logical(proc->cwp,10);
  pr = proc->intmapper[lr];
  cohe = node = proc->physical_int_reg_file[pr];
  lr = convert_to_logical(proc->cwp,11);
  pr = proc->intmapper[lr];
  inst->addr = proc->physical_int_reg_file[pr];

  /* do one character at a time - can be optimized */

  for(int i=0;i<ASSOCNAME_MAX-1;i++)
    {
      pa = (char *)GetMap(inst,proc);
      AssocName[i] = *pa;
      if(*pa == '\0')
	break;
      inst->addr++;
    }
  AssocName[ASSOCNAME_MAX-1] = '\0';

  if (inst->exception_code == 47) // COHE
    {
      AssociateAddrCohe(start-PROC_TO_MEMSYS,end-PROC_TO_MEMSYS,cohe,AssocName);
      fprintf(simout,"Associate Addr Cohe: start %d end %d cohe %s\n",start,end,Cohe[cohe]);
    }
  else // NODE
    {
      AssociateAddrNode(start-PROC_TO_MEMSYS,end-PROC_TO_MEMSYS,node,AssocName);
      fprintf(simout,"Associate Addr Node: start %d end %d node %d\n",start,end,node);
    }
}



