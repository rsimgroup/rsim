/*
  config.cc

  Parses RSIM configuration file

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
#include <stdlib.h>

#include "Processor/state.h"
#include "Processor/simio.h"

extern "C"
{
#include "MemSys/module.h"
#include "MemSys/cache.h"
#include "MemSys/arch.h"
#include "MemSys/directory.h"
#include "MemSys/bus.h"
#include "MemSys/req.h"
#include "MemSys/net.h"
}

static void ConfigureInt(void *,char *);
static void ConfigureIntKB(void *,char *);
static void ConfigureDoubleInt(void *,char *);
static void ConfigureL1Ports(void *,char *);
static void ConfigureBusWidth(void *,char *);
static void ConfigureProt(void *,char *);
static void ConfigureCacheType(void *,char *);
static void ConfigureBPBType(void *,char *);

int ALU_UNITS=2;
int FPU_UNITS=2;
int ADDR_UNITS=2;
int MEM_UNITS=L1_DEFAULT_NUM_PORTS;

/************************************************************************/
/* ParseConfigFile: parse the input file passed in for each of the      */
/* parameter types and call the function specified to determine how     */
/* the variable in question should be set                               */
/************************************************************************/
void ParseConfigFile()
{
  /* Set these parameters 
     int ARCH_numnodes;
     int ARCH_l1type;
     int ARCH_cacsz1, ARCH_setsz1, ARCH_cacsz2, ARCH_setsz2, ARCH_linesz;
     int ARCH_wbufsz, ARCH_dirbufsz;
     int ARCH_nettype, ARCH_flitd, ARCH_NetBufsz, ARCH_NetPortsz, ARCH_flitsz;
     and others as listed below in configparams
     
     */
  static struct
  {
    char *key;      /* the name of the parameter, as it appears in the file */
    void *dataptr;  /* a pointer to the "actual" variable */
    void (*func)(void *, char *); /* a function to convert a configuration
				     file parameter to an actual value.
				     The most common is ConfigureInt, which
				     merely calls "atoi" on the configuration
				     file entry.
				     */
  }
  configparams[] =
  {
    /* NOTE: If user adds any more triplets here, it is _VERY_ important
       that the NUM_CONFIG_ENTRIES macro defined at the end of this array
       is updated accordingly. Order in this array does not matter --
       the new entry can be put anywhere . */
    {"numnodes",&ARCH_numnodes,ConfigureInt},
    {"l1type",&ARCH_l1type,ConfigureCacheType}, /* reads a string */
    {"linesize",&ARCH_linesz,ConfigureInt},
    {"wbufsize",&ARCH_wbufsz,ConfigureInt},
    {"dirbufsize",&ARCH_dirbufsz,ConfigureInt},
    {"l1size",&ARCH_cacsz1,ConfigureInt},
    {"l1assoc",&ARCH_setsz1,ConfigureInt},
    {"l1ports",&L1_NUM_PORTS,ConfigureL1Ports}, /* separate since multiple variables must be set */
    {"l1taglatency",&L1TAG_DELAY,ConfigureInt},
    {"l2size",&ARCH_cacsz2,ConfigureInt},
    {"l2assoc",&ARCH_setsz2,ConfigureInt},
    {"l2taglatency",&L2TAG_DELAY,ConfigureInt},
    {"l2datalatency",&L2DATA_DELAY,ConfigureInt},
    {"wrbbufextra",&wrb_buf_extra,ConfigureInt},
    {"flitdelay",&ARCH_flitd,ConfigureInt},
    {"arbdelay",&ARCH_arbdelay,ConfigureInt},
    {"pipelinedsw",&ARCH_pipelinedsw,ConfigureInt},
    {"flitsize",&ARCH_flitsz,ConfigureInt},
    {"netbufsize",&ARCH_NetBufsz,ConfigureInt},
    {"netportsize",&ARCH_NetPortsz,ConfigureInt},
    {"ccprot",&CCProtocol,ConfigureProt}, /* reads a string */
    {"meminterleaving",&INTERLEAVING_FACTOR,ConfigureInt},
    {"dircycle",&DIRCYCLE,ConfigureDoubleInt}, /* sets a double */
    {"memorylatency",&MEMORY_LATENCY,ConfigureInt},
    {"dirpacketcreate",&DIR_PKTCREATE_TIME,ConfigureInt},
    {"dirpacketcreateaddtl",&DIR_PKTCREATE_TIME_ADDTL,ConfigureInt},
    {"buswidth",&BUSWIDTH,ConfigureBusWidth}, /* sets multiple variables */
    {"buscycle",&BUSCYCLE,ConfigureDoubleInt}, /* sets a double */
    {"busarbdelay",&ARBDELAY,ConfigureDoubleInt}, /* sets a double */
    {"mshrcoal",&MAX_COALS,ConfigureInt},
    {"reqsz",&REQ_SZ,ConfigureInt},
    {"maxstack",&MAXSTACKSIZE,ConfigureIntKB}, /* reads an int in KB */
    {"numalus",&ALU_UNITS,ConfigureInt},
    {"numfpus",&FPU_UNITS,ConfigureInt},
    {"numaddrs",&ADDR_UNITS,ConfigureInt},
    {"regwindows",&NUM_WINS,ConfigureInt},
    {"bpbtype",&BPB_TYPE,ConfigureBPBType}, /* reads a string */
    {"bpbsize",&SZ_BUF,ConfigureInt},
    {"rassize",&RAS_STKSZ,ConfigureInt},
    {"shadowmappers",&MAX_SPEC,ConfigureInt},
    {"latint",&LAT_ALU_OTHER,ConfigureInt},
    {"latmul",&LAT_ALU_MUL,ConfigureInt},
    {"latdiv",&LAT_ALU_DIV,ConfigureInt},
    {"latshift",&LAT_ALU_SHIFT,ConfigureInt},
    {"repint",&REP_ALU_OTHER,ConfigureInt},
    {"repmul",&REP_ALU_MUL,ConfigureInt},
    {"repdiv",&REP_ALU_DIV,ConfigureInt},
    {"repshift",&REP_ALU_SHIFT,ConfigureInt},
    {"latflt",&LAT_FPU_COMMON,ConfigureInt},
    {"latfmov",&LAT_FPU_MOV,ConfigureInt},
    {"latfconv",&LAT_FPU_CONV,ConfigureInt},
    {"latfdiv",&LAT_FPU_DIV,ConfigureInt},
    {"latfsqrt",&LAT_FPU_SQRT,ConfigureInt},
    {"repflt",&REP_FPU_COMMON,ConfigureInt},
    {"repfmov",&REP_FPU_MOV,ConfigureInt},
    {"repfconv",&REP_FPU_CONV,ConfigureInt},
    {"repfdiv",&REP_FPU_DIV,ConfigureInt},
    {"repfsqrt",&REP_FPU_SQRT,ConfigureInt},
    {"portszl1wbreq",&portszl1wbreq,ConfigureInt},
    {"portszwbl1rep",&portszwbl1rep,ConfigureInt},
    {"portszwbl2req",&portszwbl2req,ConfigureInt},
    {"portszl2wbrep",&portszl2wbrep,ConfigureInt},
    {"portszl1l2req",&portszl1l2req,ConfigureInt},
    {"portszl2l1rep",&portszl2l1rep,ConfigureInt},
    {"portszl2l1cohe",&portszl2l1cohe,ConfigureInt},
    {"portszl1l2cr",&portszl1l2cr,ConfigureInt},
    {"portszl2busreq",&portszl2busreq,ConfigureInt},
    {"portszbusl2rep",&portszbusl2rep,ConfigureInt},
    {"portszbusl2cohe",&portszbusl2cohe,ConfigureInt},
    {"portszl2buscr",&portszl2buscr,ConfigureInt},
    {"portszbusother",&portszbusother,ConfigureInt},
    {"portszdir",&portszdir,ConfigureInt},
#define NUM_CONFIG_ENTRIES 72 /* This parameter must be set correctly */
  };

  char buf1[1000], buf2[1000];
  int i;
  fscanf(simin,"%s %s",buf1,buf2);
  while (!feof(simin))
    {
      if (strcasecmp("STOPCONFIG",buf1) == 0) /* the end of the configuration parameters */
	break;
      for (i=0; i<NUM_CONFIG_ENTRIES; i++)
	{
	  if (strcasecmp(configparams[i].key,buf1) == 0)
	    {
	      (*(configparams[i].func))(configparams[i].dataptr,buf2);
	      break;
	    }
	}
      if (i==NUM_CONFIG_ENTRIES)
	{
	  fprintf(simerr,"Unknown configuration option %s\n",buf1);
	  exit(1);
	}
      fscanf(simin,"%s %s",buf1,buf2);
    }
}

static void ConfigureInt(void *dp, char *s)
{
  *((int *)dp) = atoi(s);
}

static void ConfigureIntKB(void *dp, char *s)
{
  *((int *)dp) = atoi(s) * 1024;
}

static void ConfigureDoubleInt(void *dp, char *s)
{
  int tmp;
  ConfigureInt(&tmp,s);
  *((double *)dp) = tmp;
}

static void ConfigureL1Ports(void *dp, char *s)
{
  ConfigureInt(dp,s);
  int np = *((int *)dp);
  L1TAG_PORTS[CACHE_PORTSENTRY] = np;
  MEM_UNITS = np;
}

static void ConfigureBusWidth(void *dp, char *s)
{
  ConfigureInt(dp,s);
  int np = *((int *)dp);
  INTERCONNECTION_WIDTH_BYTES_DIR = np;
}

static void ConfigureProt(void *dp, char *s)
{
  if (strcasecmp(s,"MESI") == 0)
    *((enum CacheCoherenceProtocolType *)dp) = MESI;
  else if (strcasecmp(s,"MSI") == 0)
    *((enum CacheCoherenceProtocolType *)dp) = MSI;
  else
    {
      fprintf(simerr,"Unknown protocol type %s\n",s);
      exit(1);
    }
}


static void ConfigureCacheType(void *dp, char *s)
{
  if (strcasecmp(s,"WB") == 0)
    *((int *)dp) = PR_WB; 
  else if (strcasecmp(s,"WT") == 0)
    *((int *)dp) = PR_WT;
  else
    {
      fprintf(simerr,"Unknown L1 cache type %s\n",s);
      exit(1);
    }
}

static void ConfigureBPBType(void *dp, char *s)
{
  if (strcasecmp(s,"2bit") == 0)
    *((bptype *)dp) = TWOBIT; 
  else if (strcasecmp(s,"2bitagree") == 0)
    *((bptype *)dp) = TWOBITAGREE; 
  else  if (strcasecmp(s,"static") == 0)
    *((bptype *)dp) = STATIC; 
  else
    {
      fprintf(simerr,"Unknown BPB type %s\n",s);
      exit(1);
    }
}

