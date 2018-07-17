/*
  unelf.cc

  Expands Solaris executable file into non-ELF format. Needed before
  simulating on a non-ELF platform
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
#include "Processor/unelf.h"
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <malloc.h>
#include <libelf.h>
#include <stdlib.h>

int main(int , char **argv)
{
  Elf *elf;
  Elf32_Ehdr *ehdr;
  Elf32_Phdr *phdr;
  Elf32_Shdr *shdr;
  Elf_Scn *scn;
  Elf_Data *data;
  UnElfedHeader myhdr;
  char *fname = argv[1];
  FILE *outfile;

  char fnamebuf[1000];
  strcpy(fnamebuf,fname);
  strcat(fnamebuf,"_unelf");
  
  int fd, cnt, datacnt;
  
  if ((fd = open(fname, O_RDONLY)) < 0)
    {
      fprintf(stderr,"Error in unelf: couldn't open file %s\n",fname);
      exit(-1);
      return -1;
    }
  
  if (elf_version(EV_CURRENT) == EV_NONE)
    {
      fprintf(stderr,"ELF out of date\n");
      exit(-1);
      /* library out of date */
      /* recover from error */
    }

  if (((elf = elf_begin(fd, ELF_C_READ, NULL)) == NULL) ||
      ((ehdr = elf32_getehdr(elf)) == NULL) ||
      ((phdr = elf32_getphdr(elf)) == NULL))
    {
      // failure to read elf info
      fprintf(stderr,"Error reading elf headers from %s\n",fname);
      exit(-1);
    }
  
  myhdr.entry = ehdr->e_entry; /* We currently require first text block to be at entry point */
  
  myhdr.data_start = DOWN_TO_PAGE(phdr->p_vaddr-phdr->p_offset); 
  unsigned alloc_size = ALLOC_SIZE; /* start it out small and let it build up with reallocs, etc */


  char *datachunk = (char *)malloc(alloc_size);
  /* NOTE: data includes the text part also... */
  
  for (cnt = 1, scn = NULL; (scn = elf_nextscn(elf, scn)) != NULL; cnt++)
    {
      if ((shdr = elf32_getshdr(scn)) == NULL)
	{
	  fprintf(stderr,"ELF prob reading section %d of file %s\n",cnt,fname);
	  exit(-1);
	}
      if ((void *)shdr->sh_addr != NULL) /* in other words, it's real stuff, and not file-only stuff */
	{
	  for (datacnt = 0, data = NULL; (data = elf_getdata(scn,data)) != NULL; datacnt++)
	    {
	      if (shdr->sh_addr+data->d_off-myhdr.data_start + data->d_size > alloc_size)
		{
		  alloc_size = UP_TO_PAGE(shdr->sh_addr+data->d_off-myhdr.data_start+data->d_size);
		  datachunk = (char *)realloc((void *)datachunk,alloc_size);
		}
	      if (data->d_buf)
		{
		  fprintf(stdout,"Unelf: Copying %d bytes from elf section %d, data chunk %d\n",data->d_size,cnt,datacnt);
		  memcpy(datachunk+shdr->sh_addr+data->d_off-myhdr.data_start,data->d_buf,data->d_size);
		}
	      else
		{
		  fprintf(stdout,"Unelf: Zeroing %d bytes of elf section %d, data chunk %d\n",data->d_size,cnt,datacnt);
		  memset(datachunk+shdr->sh_addr+data->d_off-myhdr.data_start,0,data->d_size);
		}
	    }
	}
    }
  elf_end(elf);
  close(fd);			/* we've read it all now */

  if ((outfile = fopen(fnamebuf, "w")) == NULL)
    {
      fprintf(stderr,"Error in unelf: couldn't open file %s\n",fnamebuf);
      exit(-1);
      return -1;
    }
  myhdr.size = alloc_size;
  fwrite(&myhdr,sizeof(UnElfedHeader),1,outfile);
  fwrite(datachunk,alloc_size,1,outfile);
  fclose(outfile);
  free(datachunk);
  return 0;
}
