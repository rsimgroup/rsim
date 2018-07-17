/*
  predecode.cc

  Main loop for predecoder; steps through ELF segments looking for
  text segment. Passes in text segment instructions to predecode
  functions

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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <libelf.h>

extern void TableSetup();
extern int start_decode(instr *, unsigned);

typedef int SPARCinstr;

void instr::print()
{
  printf("%-12.11s%d\t%d\t%d\t%d\t%d\t%d\t%d\n",inames[instruction],rd,rcc,rs1,rs2,aux1,aux2,imm);
}

main(int argc, char **argv)
{
  Elf32_Shdr *shdr;
  Elf32_Ehdr *ehdr;
  Elf32_Phdr *phdr;
  Elf *elf;
  Elf_Scn *scn;
  Elf_Data *data1, *data2;
  unsigned int   cnt, datacnt, pcnt;
  
  int i,j;
  FILE *fp, *fpout;
  int page[2048]; /* a page is this many instructions */
  char filename[80];

  if (argc == 2)
    {
      strcpy(filename,argv[1]);
      strcat(filename,".out");
    }
  else
    {
      strcpy(filename,"a.out");
    }
    
  
  TableSetup();

  int fildes = open(filename,O_RDONLY);
  
  if (fildes == -1)
    {
      fprintf(stderr,"Failure opening file %s\n",filename);
      exit(-1);
    }

  strcat(filename,".dec");
  
  if (elf_version(EV_CURRENT) == EV_NONE)
    {
      fprintf(stderr,"ELF out of date\n");
      exit(-1);
      /* library out of date */
      /* recover from error */
    }

  fpout=fopen(filename,"w");
  
  i=0;
  
  int ehdr_ctr = 0;
  if ((elf = elf_begin(fildes, ELF_C_READ, NULL)) != 0)
    {
      if ((ehdr = elf32_getehdr(elf)) != 0)
	{
	  printf("Elf header #%d: ",ehdr_ctr++);
	  for (int id=0; id<EI_NIDENT; id++)
	    printf("%d-",int(ehdr->e_ident[id]));

	  printf("\nType: %d\tMachine: %d\tVersion: %d\n",
		 ehdr->e_type,ehdr->e_machine,ehdr->e_version);
	  printf("Entry: 0x%x\tPH Offset: 0x%x\tSH Offset: 0x%x\n",
		 ehdr->e_entry,ehdr->e_phoff,ehdr->e_shoff);
	  printf("Elf flags: 0x%x\n",ehdr->e_flags);
	  printf("Elf header size: %d\n",ehdr->e_ehsize);
	  printf("Prog header entry size: %d\tProg header num: %d\n",ehdr->e_phentsize,ehdr->e_phnum);
	  printf("Sec header entry size: %d\tSec header num: %d\tSec header strndx: %d\n\n",ehdr->e_shentsize,ehdr->e_shnum,ehdr->e_shstrndx);

	  /* First get/read the program header */
	  if ((phdr = elf32_getphdr(elf)) == NULL)
	    {
	      fprintf(stderr,"ELF prob with phdr\n");
	      exit(-1);
	    }

	  pcnt = 0;
	  //	  for (pcnt=0; pcnt < ehdr->e_phnum; pcnt++, phdr++)
	  //  {
	      printf("Phdr %d\n",pcnt);
	      printf("Type: %d\tOffset: 0x%x\tVaddr: 0x%x\tPaddr: 0x%x\n",phdr->p_type,phdr->p_offset,phdr->p_vaddr,phdr->p_paddr);
	      printf("Filesz: %d\tMemsz: %d\tFlags: 0x%x\tAlign: %d\n",phdr->p_filesz,phdr->p_memsz,phdr->p_flags,phdr->p_align);
	      // }
	  
	  /* Obtain the .shstrtab data buffer before doing sections */
          if (((scn = elf_getscn(elf, ehdr->e_shstrndx)) == NULL) ||
              ((data2 = elf_getdata(scn, NULL)) == NULL))
	    {
	      fprintf(stderr,"ELF prob with sections\n");
	      exit(-1);
	    }
	  
	  /* Traverse input file, printing each section */
          for (cnt = 1, scn = NULL; scn = elf_nextscn(elf, scn); cnt++)
	    {
	      if ((shdr = elf32_getshdr(scn)) == NULL)
		{
		  fprintf(stderr,"ELF prob with sections\n");
		  exit(-1);
		}
	      
	      printf("Section %d [index %d]: %s\n", cnt,elf_ndxscn(scn),(char *)data2->d_buf + shdr->sh_name);
	      printf("Shdr Type: %d\tFlags: 0x%x\n",shdr->sh_type,shdr->sh_flags);
	      printf("Addr: 0x%x\tOffset: 0x%x\tSize: %d\n",shdr->sh_addr,shdr->sh_offset,shdr->sh_size);
	      printf("Link: 0x%x\tInfo: 0x%x\tAddr align: %d\tEntry Size: %d\n",shdr->sh_link,shdr->sh_info,shdr->sh_addralign,shdr->sh_entsize);

	      if (shdr->sh_flags & SHF_EXECINSTR)
		{
		  printf("Executable section!\n");
		  if (shdr->sh_flags & SHF_WRITE)
		    printf("Executable and writable!\n");
		  lseek(fildes,shdr->sh_offset,SEEK_SET);
		  i=0;
		  while (i < shdr->sh_size/sizeof(SPARCinstr)) // number of instructions
		    {
		      int num, inew;
		      num = read(fildes,page,2048*sizeof(SPARCinstr)); /* get the next page full...*/
		      inew = i+num/sizeof(SPARCinstr);
		      if (inew > shdr->sh_size/sizeof(SPARCinstr))
			inew = shdr->sh_size/sizeof(SPARCinstr);
		      for (j=0; j<inew-i; j++)
			{
			  instr inst;  
			  if (start_decode(&inst,page[j]))
			    {
			      inst.print();
			      inst.output(fpout);
			    } /* otherwise it was just a meta-instruction */
			}
		      i = inew;
		    }
		}

	      if ((data1 = elf_getdata(scn, NULL)) == NULL)
		{
		  fprintf(stderr,"ELF prob with section data\n");
		  exit(-1);
		}
	      for (datacnt = 0, data1 = NULL; (data1 = elf_getdata(scn,data1)) != NULL; datacnt++)
		{
		  printf("Section %d data %d\t\n",cnt,datacnt);
		  printf("Buf: 0x%x\tType: %d\tSize: %d\n",
			 data1->d_buf,data1->d_type,data1->d_size);
		  printf("Offset: 0x%x\tAlign: %d\tVersion: %d\n",
			 data1->d_off,data1->d_align,data1->d_version);
		}
	    }

	  
	}
      elf_end(elf);
    }
  fclose(fpout);
  close(fildes);
}

