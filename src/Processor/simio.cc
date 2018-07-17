/*

  simio.cc

  Allows the simulator input and output to be redirected
  separately from the application input and output

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


#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <stdlib.h>
#include "Processor/simio.h"

FILE *simin = stdin;
FILE *simout = stdout;
FILE *simerr = stderr;

#define SIMIN_FD_OFFSET (-1)
#define SIMOUT_FD_OFFSET (-2)
#define SIMERR_FD_OFFSET (-3)
#define PRACTICAL_MAX_FD 200
int maxfd;

/*************************************************************************/
/* SimIoInit: make sure that simulator input/output have different file  */
/* descriptors than application input/output, since application should   */
/* not just "accidentally" close the simulator's files, etc.             */
/*************************************************************************/
void SimIOInit()
{
  struct rlimit resource;
  if (getrlimit(RLIMIT_NOFILE,&resource) != 0)
    {
      fprintf(simerr,"Couldn't get resource limits. Exiting\n");
      exit(-1);
    }

  /* Take the 3 highest file descriptors so as to minimize interference
     with the application file descriptors */
  
  if ((maxfd = resource.rlim_cur) > PRACTICAL_MAX_FD)
    maxfd = PRACTICAL_MAX_FD;
  if (dup2(0,maxfd+SIMIN_FD_OFFSET) < 0 ||
      dup2(1,maxfd+SIMOUT_FD_OFFSET) < 0 ||
      dup2(2,maxfd+SIMERR_FD_OFFSET) < 0)
    {
      fprintf(simerr,"Problem setting up simin/out/err\n");
      exit(-1);
    }
  if (((simin = fdopen(maxfd+SIMIN_FD_OFFSET,"r")) == NULL) ||
      ((simout = fdopen(maxfd+SIMOUT_FD_OFFSET,"w")) == NULL) ||
      ((simerr = fdopen(maxfd+SIMERR_FD_OFFSET,"w")) == NULL))
    {
      fprintf(simerr,"Problem setting up simin/out/err\n");
      exit(-1);
    }
}

/***************************************************************************/
/* RedirectSimIO: this is used to redirect simulator input/output to       */
/* different files from application input/output so that the files do not  */
/* interfere at all.                                                       */
/***************************************************************************/


void RedirectSimIO(int currfd, const char *file)
{
  
  int fd;
  if (currfd == 0) /* simulator input */
    {
      fd = open(file,O_RDONLY);
      if (dup2(fd,maxfd+SIMIN_FD_OFFSET) < 0)
	{
	  fprintf(simerr,"Problem setting up simin\n");
	  exit(-1);
	}
      close(fd);
      simin = fdopen(maxfd+SIMIN_FD_OFFSET,"r");
    }
  else /* simulator output */
    {
      fd = open(file,O_WRONLY|O_CREAT|O_TRUNC,S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
      if (currfd == 1) // simout
	{
	  if (dup2(fd,maxfd+SIMOUT_FD_OFFSET) < 0)
	    {
	      fprintf(simerr,"Problem setting up simout\n");
	      exit(-1);
	    }
	  close(fd);
	  simout = fdopen(maxfd+SIMOUT_FD_OFFSET,"w");
	}
      else // simerr
	{
	  if (dup2(fd,maxfd+SIMERR_FD_OFFSET) < 0)
	    {
	      fprintf(simerr,"Problem setting up simerr\n");
	      exit(-1);
	    }
	  close(fd);
	  simerr = fdopen(maxfd+SIMERR_FD_OFFSET,"w");
	}
    }
}

