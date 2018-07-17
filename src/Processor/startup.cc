/*

  Processor/startup.cc

  This file reads the Solaris executable of the application being simulated
  and sets up the simulated stack and heap appropriately.

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
#include "Processor/processor_dbg.h"
#include "Processor/simio.h"
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <malloc.h>
#include <stdlib.h>

#ifndef UNELF
#include <libelf.h>
#else // UNELF case
#include "Processor/unelf.h"
#endif


#define MINIMUM_STACK_FRAME_SIZE 0x60
/* In the SPARC, the min stack frame is 16*4 for spills (will this become
   16*8 with 64-bit int regs?) plus 1*4 for aggregate return pointer plus
   6*4 for callee save space plus 1*4 for an additional save pointer */

int entry_pt;
int pcbase;

/***********************************************************************/
/* startup : startup procedure which reads the application binary and  */
/*         : suitably initializes the stack,heap and data segments     */
/*         : machine-format specific                                   */
/***********************************************************************/

int startup(char **args, state *proc)
{
  int fd;
  // struct exec exec;
  register char *cp;
  
  register int i;
  int argcount;
  int size;
  register char **cpp;
  int cnt;
  int badpc;

  char *datachunk;
  unsigned data_start;
  unsigned alloc_size;
  
#ifndef UNELF
  int datacnt;
  Elf *elf;
  Elf32_Ehdr *ehdr;
  Elf32_Phdr *phdr;
  Elf32_Shdr *shdr;
  Elf_Scn *scn;
  Elf_Data *data;

  
  if ((fd = open(args[0], O_RDONLY)) < 0)
    {
      fprintf(simerr,"Error in startup: couldn't open file %s\n",args[0]);
      exit(-1);
      return -1;
    }
  
  if (elf_version(EV_CURRENT) == EV_NONE)
    {
      fprintf(simerr,"ELF out of date\n");
      exit(-1);
      /* library out of date */
    }

  if (((elf = elf_begin(fd, ELF_C_READ, NULL)) == NULL) ||
      ((ehdr = elf32_getehdr(elf)) == NULL) ||
      ((phdr = elf32_getphdr(elf)) == NULL))
    {
      // failure to read elf info
      fprintf(simerr,"Error reading elf headers from %s\n",args[0]);
      exit(-1);
    }
  
  pcbase = ehdr->e_entry; /* We currently require first text block to be at entry point */
  proc->pc = InstrAddrToNum(ehdr->e_entry,badpc); /* start out at the entry point */
  proc->npc = proc->pc +1;
  
  data_start = DOWN_TO_PAGE(phdr->p_vaddr-phdr->p_offset); /* we want to find out where the elf header will be located */
  alloc_size = ALLOC_SIZE; /* start it out small and let it build up with reallocs, etc */

  // UP_TO_PAGE(shdr->sh_addr+shdr->sh_size) - data_start;

  datachunk = (char *)malloc(alloc_size);
  /* NOTE: data includes the text part also... */
  
  for (cnt = 1, scn = NULL; (scn = elf_nextscn(elf, scn)) != NULL; cnt++)
    {
      if ((shdr = elf32_getshdr(scn)) == NULL)
	{
	  fprintf(simerr,"ELF prob reading section %d of file %s\n",cnt,args[0]);
	  exit(-1);
	}
      if ((void *)shdr->sh_addr != NULL) /* in other words, it's real data, and not file-only data */
	{
	  for (datacnt = 0, data = NULL; (data = elf_getdata(scn,data)) != NULL; datacnt++)
	    {
	      if (shdr->sh_addr+data->d_off-data_start + data->d_size > alloc_size)
		{
		  alloc_size = UP_TO_PAGE(shdr->sh_addr+data->d_off-data_start+data->d_size);
		  datachunk = (char *)realloc((void *)datachunk,alloc_size);
		}
	      if (data->d_buf)
		{
#ifdef COREFILE
		  fprintf(corefile,"Startup: Copying %d bytes from elf section %d, data chunk %d\n",data->d_size,cnt,datacnt);
#endif
		  memcpy(datachunk+shdr->sh_addr+data->d_off-data_start,data->d_buf,data->d_size);
		}
	      else
		{
#ifdef COREFILE
		  fprintf(corefile,"Startup: Zeroing %d bytes of elf section %d, data chunk %d\n",data->d_size,cnt,datacnt);
#endif
		  memset(datachunk+shdr->sh_addr+data->d_off-data_start,0,data->d_size);
		}
	    }
	}
    }
  elf_end(elf);
  close(fd);			/* we've read it all now */

#else // UNELF version -- HP, SunOS
  char fnamebuf[1000];
  strcpy(fnamebuf,args[0]);
  strcat(fnamebuf,"_unelf");

  if ((fd = open(fnamebuf, O_RDONLY)) < 0)
    {
      fprintf(simerr,"Error in startup: couldn't open file %s\n",fnamebuf);
      exit(-1);
      return -1;
    }

  UnElfedHeader myhdr;
  if (read(fd,&myhdr,sizeof(UnElfedHeader)) != sizeof(UnElfedHeader))
    {
      fprintf(simerr,"Error in startup: couldn't read from file %s\n",args[0]);
      exit(-1);
      return -1;
    }

  data_start = myhdr.data_start;
  alloc_size = myhdr.size;
  pcbase = entry_pt = myhdr.entry; // for now, pcbase and entry_pt must be the same
  
  proc->pc = InstrAddrToNum(myhdr.entry,badpc); /* start out at the entry point */
  proc->npc = proc->pc +1;
  datachunk = (char *)malloc(alloc_size);
  if (read(fd,datachunk,alloc_size) != alloc_size)
    {
      fprintf(simerr,"Error in startup: couldn't read from file %s\n",args[0]);
      exit(-1);
      return -1;
    }
  close(fd);
#endif
  
  /****** Insert the data into the processor hash table */
  unsigned data_chunk = (unsigned) datachunk;
  unsigned chunkptr = data_chunk;
  for (unsigned pg=data_start/ALLOC_SIZE;
       pg< (data_start + alloc_size)/ALLOC_SIZE;
       pg++, chunkptr += ALLOC_SIZE)
    {
      proc->PageTable.insert(pg,chunkptr);
    }
  
  unsigned stackchunk = (unsigned) malloc(ALLOC_SIZE);
  proc->PageTable.insert(lowshared / ALLOC_SIZE -1, stackchunk); /* 1 chunk for the initial stack -- it will expand as needed */
  
  unsigned proglim=data_start+alloc_size;
  
  unsigned stackbase=lowshared-ALLOC_SIZE;

  proc->highheap = proglim; 
  proc->lowstack = stackbase;

  
    /*
     *  Figure out how many bytes are needed to hold the arguments on
     *  the new stack that we are building.  Also count the number of
     *  arguments, to become the argc that the new "main" gets called with.
     */
  size = 0;

  fprintf(simerr,"Startup command line: ");

  for (i = 0; args[i] != NULL; i++)
    {
      size += strlen(args[i]) + 1;
      fprintf(simerr,"%s ",args[i]);
    }
  argcount = i;
  fprintf(simerr,"\n\n");
  
  /*
   *  The arguments will get copied starting at "cp", and the argv
   *  pointers to the arguments (and the argc value) will get built
   *  starting at "cpp".  The value for "cpp" is computed by subtracting
   *  off space for the number of arguments (plus 2, for the terminating
   *  NULL and the argc value) times the size of each (4 bytes), and
   *  then rounding the value *down* to a double-word boundary.
   */
  cp = (char *)lowshared - size;
  cpp = (char **)(((int)cp - ((argcount + 2) * 4)) & ~7);

  /* here's the part for setting the stack pointer, which is register %o6, or 14 */
  int prsp = proc->intmapper[proc->SPARCtoLog(14)]; /* the stack pointer *** */
  proc->physical_int_reg_file[prsp] = int(cpp) - MINIMUM_STACK_FRAME_SIZE;
  proc->physical_int_reg_file[proc->intmapper[proc->SPARCtoLog(8)]] = argcount; // argc
  proc->physical_int_reg_file[proc->intmapper[proc->SPARCtoLog(9)]] = int(cpp+1); // argv
  
  proc->logical_int_reg_file[proc->SPARCtoLog(14)] = int(cpp) - MINIMUM_STACK_FRAME_SIZE;
  proc->logical_int_reg_file[proc->SPARCtoLog(8)] = argcount; // argc
  proc->logical_int_reg_file[proc->SPARCtoLog(9)] = int(cpp+1); // argv
  
  cp = (char *)(stackchunk+ALLOC_SIZE - size);
  cpp = (char **)(((int)cp - ((argcount + 2) * 4)) & ~7);
  
  *cpp++ = (char *)argcount;		/* the first value at cpp is argc */

  for (i = 0; i < argcount; i++)	/* copy each argument and set argv's */
    {
      *cpp++ = cp+lowshared-(stackchunk+ALLOC_SIZE);
      strcpy(cp, args[i]);
      cp += strlen(cp) + 1;
    }
  *cpp = NULL;			/* the last argv is a NULL pointer */

  return 0;
}

